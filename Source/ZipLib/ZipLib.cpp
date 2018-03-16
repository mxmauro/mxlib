/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2006-2015. All rights reserved.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 *
 * 2. This notice may not be removed or altered from any source distribution.
 *
 * 3. YOU MAY NOT:
 *
 *    a. Modify, translate, adapt, alter, or create derivative works from
 *       this software.
 *    b. Copy (other than one back-up copy), distribute, publicly display,
 *       transmit, sell, rent, lease or otherwise exploit this software.
 *    c. Distribute, sub-license, rent, lease, loan [or grant any third party
 *       access to or use of the software to any third party.
 **/
#include "..\..\Include\ZipLib\ZipLib.h"
#include "Source\zlib.h"

//-----------------------------------------------------------

//ZLIB Header
//~~~~~~~~~~~~~
//Byte CMF: bits 0 to 3  CM     Compression method (always 8=DEFLATE)
//          bits 4 to 7  CINFO  Compression info (2^CINFO = window size)
//Byte FLG: bits 0 to 4  FCHECK (check bits for CMF and FLG)
//          bit  5       FDICT  (preset dictionary)
//          bits 6 to 7  FLEVEL  (compression level)

//-----------------------------------------------------------

#define INUSE_None                                         0
#define INUSE_Compressing                                  1
#define INUSE_Decompressing                                2

#define __stream                       ((z_streamp)lpStream)

//-----------------------------------------------------------

static voidpf my_alloc_func(voidpf opaque, uInt items, uInt size);
static void my_free_func(voidpf opaque, voidpf address);

//-----------------------------------------------------------

namespace MX {

CZipLib::CZipLib(_In_ BOOL _bUseZipLibHeader)
{
  bUseZipLibHeader = _bUseZipLibHeader;
  nInUse = INUSE_None;
  lpStream = (z_streamp)MX_MALLOC(sizeof(z_stream));
  Cleanup();
  return;
}

CZipLib::~CZipLib()
{
  End();
  MX_FREE(lpStream);
  return;
}

HRESULT CZipLib::BeginCompress(_In_ int nCompressionLevel)
{
  int nErr;

  if (nCompressionLevel<1 || nCompressionLevel > 9)
    return E_INVALIDARG;
  if (nInUse != INUSE_None)
    return MX_E_AlreadyInitialized;
  if (lpStream == NULL)
    return E_OUTOFMEMORY;
  //free processed data
  cProcessed.SetBufferSize(0);
  //initialize compression
  __try
  {
    nErr = deflateInit2(__stream, nCompressionLevel, Z_DEFLATED, (bUseZipLibHeader != FALSE) ? 15 : -15, 8,
                        Z_DEFAULT_STRATEGY);
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    nErr = Z_MEM_ERROR;
  }
  if (nErr != Z_OK)
    return (nErr == Z_MEM_ERROR) ? E_OUTOFMEMORY : E_FAIL;
  nInUse = INUSE_Compressing;
  return S_OK;
}

HRESULT CZipLib::BeginDecompress()
{
  int nErr;

  if (nInUse != INUSE_None)
    return MX_E_AlreadyInitialized;
  if (lpStream == NULL)
    return E_OUTOFMEMORY;
  //free processed data
  cProcessed.SetBufferSize(0);
  //initialize decompression
  nGZipHdrState = 1;
  __try
  {
    nErr = inflateInit2(__stream, (bUseZipLibHeader != FALSE) ? 15 : -15);
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    nErr = Z_MEM_ERROR;
  }
  if (nErr != Z_OK)
    return (nErr == Z_MEM_ERROR) ? E_OUTOFMEMORY : E_FAIL;
  nInUse = INUSE_Decompressing;
  return S_OK;
}

HRESULT CZipLib::CompressStream(_In_ LPCVOID lpSrc, _In_ SIZE_T nSrcLen)
{
  SIZE_T nToProcess, nReaded, nWritten;
  LPBYTE s;
  int nErr;
  HRESULT hRes;

  if (nInUse != INUSE_Compressing)
    return E_FAIL;
  if (lpSrc == NULL && nSrcLen > 0)
    return E_POINTER;
  //compress this block
  s = (LPBYTE)lpSrc;
  while (nSrcLen > 0)
  {
    __stream->next_in = s;
    if ((nToProcess = 131072) > nSrcLen)
      nToProcess = nSrcLen;
    __stream->avail_in = (uInt)nToProcess;
    __stream->next_out = aTempBuf;
    __stream->avail_out = (uInt)sizeof(aTempBuf);
    nWritten = sizeof(aTempBuf);
    __try
    {
      nErr = deflate(__stream, Z_NO_FLUSH);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      nErr = Z_DATA_ERROR;
    }
    if (nErr != Z_OK)
    {
      Cleanup();
      return (nErr == Z_MEM_ERROR) ? E_OUTOFMEMORY : E_FAIL;
    }
    nReaded = nToProcess - (SIZE_T)(__stream->avail_in);
    nWritten -= (SIZE_T)(__stream->avail_out);
    s += nReaded;
    nSrcLen -= nReaded;
    if (nWritten > 0)
    {
      hRes = cProcessed.Write(aTempBuf, nWritten);
      if (FAILED(hRes))
      {
        Cleanup();
        return hRes;
      }
    }
  }
  return S_OK;
}

HRESULT CZipLib::DecompressStream(_In_ LPCVOID lpSrc, _In_ SIZE_T nSrcLen, _Out_opt_ SIZE_T *lpnUnusedBytes)
{
  static const BYTE aFalseGZipHeader = 0x1F;
  SIZE_T nToProcess, nReaded, nWritten;
  LPBYTE s;
  int nErr, nPasses;
  HRESULT hRes;

  if (lpnUnusedBytes != NULL)
    *lpnUnusedBytes = nSrcLen;
  if (nInUse != INUSE_Decompressing)
    return E_FAIL;
  if (lpSrc == NULL && nSrcLen > 0)
    return E_POINTER;
  //decompress this block
  nPasses = 1;
  s = (LPBYTE)lpSrc;
  if (nGZipHdrState == 1)
  {
    if (*s != 0x1F)
    {
      nGZipHdrState = 0;
    }
    else if (nSrcLen >= 2)
    {
      if (s[1] == 0x8B)
      {
        //GZIP header found
        s += 2;
        nSrcLen -= 2;
        if (lpnUnusedBytes != NULL)
          *lpnUnusedBytes -= 2;
        nGZipHdrState = 3;
      }
      else
      {
        nGZipHdrState = 0;
      }
    }
    else
    {
      //not enough data to check for GZIP header
      nGZipHdrState = 2;
      if (lpnUnusedBytes != NULL)
        *lpnUnusedBytes = 0;
      return S_OK;
    }
  }
  else if (nGZipHdrState == 2)
  {
    if (*s == 0x8B)
    {
      //GZIP header found
      s++;
      nSrcLen--;
      if (lpnUnusedBytes != NULL)
        (*lpnUnusedBytes)--;
      nGZipHdrState = 3;
    }
    else
    {
      //no GZIP header
      nGZipHdrState = 0;
      //add the skipped 0x1F byte to the compressed data
      nPasses = 2;
    }
  }
  if (nGZipHdrState >= 3)
  {
    if (CheckAndSkipGZipHeader(s, nSrcLen, lpnUnusedBytes) == FALSE)
    {
      Cleanup();
      return MX_E_InvalidData;
    }
  }
  while (nPasses > 0)
  {
    nPasses--;
    //NOTE: if nPasses==2, nDataSize will be always greather than zero
    while (bEndReached == FALSE && nSrcLen > 0)
    {
      if (nPasses == 0)
      {
        __stream->next_in = s;
        if ((nToProcess = 131072) > nSrcLen)
          nToProcess = nSrcLen;
      }
      else
      {
        __stream->next_in = (Bytef*)&aFalseGZipHeader;
        nToProcess = 1;
      }
      __stream->avail_in = (uInt)nToProcess;
      __stream->next_out = aTempBuf;
      __stream->avail_out = (uInt)sizeof(aTempBuf);
      nWritten = (SIZE_T)(__stream->avail_out);
      __try
      {
        nErr = inflate(__stream, Z_SYNC_FLUSH);
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
        nErr = Z_DATA_ERROR;
      }
      if (nErr != Z_OK && nErr != Z_STREAM_END)
      {
        Cleanup();
        return (nErr == Z_MEM_ERROR) ? E_OUTOFMEMORY : MX_E_InvalidData;
      }
      if (nPasses == 0)
      {
        nReaded = nToProcess - (SIZE_T)(__stream->avail_in);
        s += nReaded;
        nSrcLen -= nReaded;
        if (lpnUnusedBytes != NULL)
          *lpnUnusedBytes -= nReaded;
      }
      else
      {
        nReaded = 1;
      }
      nWritten -= (SIZE_T)(__stream->avail_out);
      if (nErr == Z_STREAM_END)
        bEndReached = TRUE;
      if (nWritten > 0)
      {
        hRes = cProcessed.Write(aTempBuf, nWritten);
        if (FAILED(hRes))
        {
          Cleanup();
          return hRes;
        }
      }
    }
  }
  return S_OK;
}

HRESULT CZipLib::End()
{
  int nErr, nRetry;
  HRESULT hRes;

  switch (nInUse)
  {
    case INUSE_Compressing:
      nErr = Z_OK;
      nRetry = 24;
      while (nErr == Z_OK)
      {
        __stream->next_in = aTempBuf;
        __stream->avail_in = 0;
        __stream->next_out = aTempBuf;
        __stream->avail_out = (uInt)sizeof(aTempBuf);
        __try
        {
          nErr = deflate(__stream, Z_FINISH);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
          nErr = Z_DATA_ERROR;
        }
        if (nErr != Z_STREAM_END && nErr != Z_OK)
        {
          Cleanup();
          return (nErr == Z_MEM_ERROR) ? E_OUTOFMEMORY : E_FAIL;
        }
        if (__stream->avail_out > 0)
        {
          hRes = cProcessed.Write(aTempBuf, (SIZE_T)(__stream->avail_out));
          if (FAILED(hRes))
          {
            Cleanup();
            return hRes;
          }
        }
        else
        {
          if ((--nRetry) == 0)
          {
            Cleanup();
            return E_FAIL;
          }
        }
      }
      Cleanup();
      break;

    case INUSE_Decompressing:
      if (bEndReached == FALSE)
      {
        nErr = Z_OK;
        nRetry = 24;
        while (nErr == Z_OK)
        {
          __stream->next_in = aTempBuf;
          __stream->avail_in = 0;
          __stream->next_out = aTempBuf;
          __stream->avail_out = (uInt)sizeof(aTempBuf);
          __try
          {
            nErr = inflate(__stream, Z_FINISH);
          }
          __except (EXCEPTION_EXECUTE_HANDLER)
          {
            nErr = Z_DATA_ERROR;
          }
          if (nErr != Z_STREAM_END && nErr != Z_OK)
          {
            Cleanup();
            return (nErr == Z_MEM_ERROR) ? E_OUTOFMEMORY : MX_E_InvalidData;
          }
          if (__stream->avail_out > 0)
          {
            hRes = cProcessed.Write(aTempBuf, (SIZE_T)(__stream->avail_out));
            if (FAILED(hRes))
            {
              Cleanup();
              return hRes;
            }
          }
          else
          {
            if ((--nRetry) == 0)
            {
              Cleanup();
              return E_FAIL;
            }
          }
        }
      }
      Cleanup();
      break;

    default:
      return E_FAIL;
  }
  return S_OK;
}

SIZE_T CZipLib::GetAvailableData() const
{
  return cProcessed.GetAvailableForRead();
}

SIZE_T CZipLib::GetData(_Out_ LPVOID lpDest, _In_ SIZE_T nDestSize)
{
  if (lpDest == NULL)
    return 0;
  return cProcessed.Read(lpDest, nDestSize);
}

BOOL CZipLib::HasDecompressEndOfStreamBeenReached()
{
  return (nInUse == INUSE_Decompressing) ? bEndReached : FALSE;
}

VOID CZipLib::Cleanup()
{
  switch (nInUse)
  {
    case INUSE_Compressing:
      __try
      {
        deflateEnd(__stream);
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      { }
      break;

    case INUSE_Decompressing:
      __try
      {
        inflateEnd(__stream);
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      { }
      break;
  }
  nInUse = INUSE_None;
  nLevel = nGZipHdrState = 0;
  if (lpStream != NULL)
  {
    MemSet(lpStream, 0, sizeof(z_stream));
    __stream->zalloc = &my_alloc_func;
    __stream->zfree = &my_free_func;
  }
  bEndReached = FALSE;
  wTemp16 = 0;
  return;
}

BOOL CZipLib::CheckAndSkipGZipHeader(_Inout_ LPBYTE &s, _Inout_ SIZE_T &nSrcLen, _Inout_opt_ SIZE_T *lpnUnusedBytes)
{
  while (nGZipHdrState > 0 && nSrcLen > 0)
  {
    switch (nGZipHdrState)
    {
      case 3: //method
        if (*s != Z_DEFLATED)
          return FALSE;
        nGZipHdrState++;
        break;
      case 4: //flags
        aTempBuf[8] = *s;
        if ((aTempBuf[8] & 0xE0) != 0) //RESERVED
          return FALSE;
        nGZipHdrState++;
        break;

      case 5: //discard time, xflags and OS code
      case 6:
      case 7:
      case 8:
      case 9:
      case 10:
        nGZipHdrState++;
        break;

      case 11: //has EXTRA_FIELD?
        if ((aTempBuf[8] & 0x04) == 0)
          goto casgzh_cont21; //no... skip to next
        wTemp16 = (WORD)(*s);
        nGZipHdrState++;
        break;
      case 12:
        wTemp16 |= ((WORD)(*s)) << 8;
        if (wTemp16 == 0)
          nGZipHdrState = 21;
        else
          nGZipHdrState++;
        break;
      case 14:
        if ((--wTemp16) == 0)
          nGZipHdrState = 21;
        break;

      case 21: //has ORIG_NAME?
casgzh_cont21:
        if ((aTempBuf[8] & 0x08) == 0)
          goto casgzh_cont31; //no... skip to next
        //fall thru
      case 22:
        if (*s == 0)
          nGZipHdrState = 31;
        break;

      case 31: //has COMMENT?
casgzh_cont31:
        if ((aTempBuf[8] & 0x10) == 0)
          goto casgzh_cont41; //no... skip to next
        //fall thru
      case 32:
        if ((*s) == 0)
          nGZipHdrState = 41;
        break;

      case 41: //has HEAD_CRC?
casgzh_cont41:
        if ((aTempBuf[8] & 0x02) == 0)
        {
          //no... mark end
          nGZipHdrState = 0;
          return TRUE;
        }
        nGZipHdrState++;
        break;
      case 42:
        //CRC has 2 bytes so mark end
        nGZipHdrState = 0;
        break;
    }
    s++;
    nSrcLen--;
    if (lpnUnusedBytes != NULL)
      (*lpnUnusedBytes)--;
  }
  return TRUE;
}

} //namespace MX

//-----------------------------------------------------------

static voidpf my_alloc_func(voidpf opaque, uInt items, uInt size)
{
  LPVOID p;

  p = MX_MALLOC((SIZE_T)items * (SIZE_T)size);
  if (p != NULL)
    MX::MemSet(p, 0, size);
  return (voidpf)p;
}

static void my_free_func(voidpf opaque, voidpf address)
{
  MX_FREE(address);
  return;
}
