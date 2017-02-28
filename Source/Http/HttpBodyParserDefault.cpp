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
#include "..\..\Include\Http\HttpBodyParserDefault.h"
#include "..\..\Include\FnvHash.h"
#include <intsafe.h>

//-----------------------------------------------------------

#ifdef _DEBUG
  #define _SHARING_MODE FILE_SHARE_READ
  #define _ATTRIBUTES  (FILE_ATTRIBUTE_NORMAL)
#else //_DEBUG
  #define _SHARING_MODE 0
  #define _ATTRIBUTES  (FILE_ATTRIBUTE_NORMAL|FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE)
#endif //_DEBUG

//-----------------------------------------------------------

namespace MX {

CHttpBodyParserDefault::CHttpBodyParserDefault() : CHttpBodyParserBase()
{
  _InterlockedExchange(&nMutex, 0);
  nMaxBodySizeInMemory = 32768;
  nMaxBodySize = ULONGLONG_MAX;
  nSize = 0ui64;
  nMemBufferSize = 0;
  sParser.nState = StateReading;
  return;
}

CHttpBodyParserDefault::~CHttpBodyParserDefault()
{
  cFileH.Close();
  cMemBuffer.Reset();
  return;
}

HRESULT CHttpBodyParserDefault::Read(__out LPVOID lpDest, __in ULONGLONG nOffset, __in SIZE_T nToRead,
                                     __out_opt SIZE_T *lpnReaded)
{
  CFastLock cLock(&nMutex);

  if (lpnReaded != NULL)
    *lpnReaded = 0;
  if (lpDest == NULL)
    return E_POINTER;
  if (nToRead == 0 || nOffset >= nSize)
    return S_OK;
  //validate to-read size
  if ((ULONGLONG)nToRead > nSize - nOffset)
    nToRead = (SIZE_T)(nSize - nOffset);
  //----
  if (cFileH != NULL)
  {
    union {
      ULONGLONG nOfs;
      DWORD dwOfs[2];
    };
    DWORD dwToRead, dwReaded;

    nOfs = nOffset;
    if (::SetFilePointer(cFileH, (LONG)dwOfs[0], (PLONG)&dwOfs[1], FILE_BEGIN) ==  INVALID_SET_FILE_POINTER)
      return MX_HRESULT_FROM_LASTERROR();
    while (nToRead > 0)
    {
      dwToRead = (nToRead > 65536) ? 65536 : (DWORD)nToRead;
      if (::ReadFile(cFileH, lpDest, dwToRead, &dwReaded, NULL) == FALSE)
        return MX_HRESULT_FROM_LASTERROR();
      if (dwToRead != dwReaded)
        return MX_E_ReadFault;
      lpDest = (LPBYTE)lpDest + (SIZE_T)dwReaded;
      nToRead -= (SIZE_T)dwReaded;
      if (lpnReaded != NULL)
        *lpnReaded += (SIZE_T)dwReaded;
    }
  }
  else
  {
    MemCopy(lpDest, cMemBuffer.Get() + (SIZE_T)nOffset, nToRead);
    if (lpnReaded != NULL)
      *lpnReaded = nToRead;
  }
  return S_OK;
}

HRESULT CHttpBodyParserDefault::ToString(__inout CStringA &cStrDestA)
{
  SIZE_T nReaded;
  ULONGLONG nSize;
  HRESULT hRes;

  cStrDestA.Empty();
  nSize = GetSize();
  if (nSize >= (ULONGLONG)SIZE_T_MAX)
    return E_OUTOFMEMORY;
  if (cStrDestA.EnsureBuffer((SIZE_T)nSize+1) == FALSE)
    return E_OUTOFMEMORY;
  hRes = Read((LPSTR)cStrDestA, 0ui64, (SIZE_T)nSize, &nReaded);
  if (SUCCEEDED(hRes))
  {
    ((LPSTR)cStrDestA)[nReaded] = 0;
    cStrDestA.Refresh();
  }
  else
  {
    cStrDestA.Empty();
  }
  return hRes;
}

HRESULT CHttpBodyParserDefault::Initialize(__in CPropertyBag &cPropBag, __in CHttpCommon &cHttpCmn)
{
  WCHAR szTempW[MAX_PATH];
  SIZE_T nLen;
  DWORD dw;
  ULONGLONG ull;
  LPCWSTR szValueW;
  HRESULT hRes;

  hRes = cPropBag.GetDWord(MX_HTTP_BODYPARSER_PROPERTY_MaxBodySizeInMemory, dw,
                           MX_HTTP_BODYPARSER_PROPERTY_MaxBodySizeInMemory_DEFVAL);
  if (FAILED(hRes) && hRes != MX_E_NotFound)
    return hRes;
  nMaxBodySizeInMemory = ((int)dw >= 0) ? (int)dw : -1;
  //----
  hRes = cPropBag.GetQWord(MX_HTTP_BODYPARSER_PROPERTY_MaxNonFormBodySize, ull,
                           MX_HTTP_BODYPARSER_PROPERTY_MaxNonFormBodySize_DEFVAL);
  if (FAILED(hRes) && hRes != MX_E_NotFound)
    return hRes;
  nMaxBodySize = (ull > 32768ui64) ? ull : 0ui64;
  //----
  hRes = cPropBag.GetString(MX_HTTP_BODYPARSER_PROPERTY_TempFolder, szValueW,
                            MX_HTTP_BODYPARSER_PROPERTY_TempFolder_DEFVAL);
  if (FAILED(hRes) && hRes != MX_E_NotFound)
    return hRes;
  if (szValueW == NULL || *szValueW == 0)
  {
    nLen = (SIZE_T)::GetTempPathW(MAX_PATH, szTempW);
    if (cStrTempFolderW.Copy(szTempW) == FALSE)
      return E_OUTOFMEMORY;
  }
  else
  {
    if (cStrTempFolderW.Copy(szValueW) == FALSE)
      return E_OUTOFMEMORY;
  }
  nLen = cStrTempFolderW.GetLength();
  if (nLen > 0 && ((LPWSTR)cStrTempFolderW)[nLen-1] != L'\\')
  {
    if (cStrTempFolderW.Concat(L"\\") == FALSE)
      cStrTempFolderW.Empty();
  }
  //done
  return S_OK;
}

HRESULT CHttpBodyParserDefault::Parse(__in LPCVOID lpData, __in SIZE_T nDataSize)
{
  CStringW cStrTempW;
  SIZE_T k, nNewSize;
  union {
    LPBYTE s;
    LPWSTR sW;
  };
  LPBYTE lpPtr;
  Fnv64_t nHash;
  DWORD dw, dwToWrite, dwWritten;
  HRESULT hRes;

  if (lpData == NULL && nDataSize > 0)
    return E_POINTER;
  if (sParser.nState == StateDone)
    return S_OK;
  if (sParser.nState == StateError)
    return MX_E_InvalidData;

  //end of parsing?
  if (lpData == NULL)
  {
    sParser.nState = StateDone;
    return S_OK;
  }

  //begin
  hRes = S_OK;
  //check if size is greater than max size or overflow
  if (nSize + (ULONGLONG)nDataSize >= nMaxBodySize || nSize + (ULONGLONG)nDataSize < nSize)
  {
    MarkEntityAsTooLarge();
    return S_OK; //error 413 will be sent after boody is parsed
  }

  if (IsEntityTooLarge() != FALSE)
    return S_OK; //error 413 will be sent after boody is parsed

  //if we are writing to memory, check if buffer threshold passed
  if ((!cFileH) && nMaxBodySizeInMemory >= 0 && nSize + (ULONGLONG)nDataSize >= (ULONGLONG)(ULONG)nMaxBodySizeInMemory)
  {
    //switch to file container
    dw = ::GetCurrentProcessId();
    nHash = fnv_64a_buf(&dw, sizeof(dw), FNV1A_64_INIT);
    dw = ::GetTickCount();
    nHash = fnv_64a_buf(&dw, sizeof(dw), nHash);
    k = (SIZE_T)this;
    nHash = fnv_64a_buf(&k, sizeof(k), nHash);
    if (cStrTempW.Format(L"%stmp%016I64x", (LPWSTR)cStrTempFolderW, (ULONGLONG)nHash) == FALSE)
    {
err_nomem:
      hRes = E_OUTOFMEMORY;
      goto done;
    }
    cFileH.Attach(::CreateFileW((LPWSTR)cStrTempW, GENERIC_READ | GENERIC_WRITE, _SHARING_MODE, NULL, CREATE_ALWAYS,
                                _ATTRIBUTES, NULL));
    if (!cFileH)
    {
      hRes = MX_HRESULT_FROM_LASTERROR();
      goto done;
    }
    //move already existing in-memory data
    s = cMemBuffer.Get();
    k = (SIZE_T)nSize;
    while (k > 0)
    {
      dwToWrite = (nDataSize > 65536) ? 65536 : (DWORD)k;
      if (::WriteFile(cFileH, s, dwToWrite, &dwWritten, NULL) == FALSE)
      {
        hRes = MX_HRESULT_FROM_LASTERROR();
        goto done;
      }
      if (dwToWrite != dwWritten)
      {
        hRes = MX_E_WriteFault;
        goto done;
      }
      s += (SIZE_T)dwWritten;
      k -= (SIZE_T)dwWritten;
    }
    //free in-memory buffer
    cMemBuffer.Reset();
    nMemBufferSize = 0;
  }

  //if we are writing to a container, continue
  if (cFileH.Get() != NULL)
  {
    while (nDataSize > 0)
    {
      dwToWrite = (nDataSize > 65536) ? 65536 : (DWORD)nDataSize;
      if (::WriteFile(cFileH, lpData, dwToWrite, &dwWritten, NULL) == FALSE)
      {
        hRes = MX_HRESULT_FROM_LASTERROR();
        goto done;
      }
      if (dwToWrite != dwWritten)
      {
        hRes = MX_E_WriteFault;
        goto done;
      }
      lpData = (LPBYTE)lpData + (SIZE_T)dwWritten;
      nDataSize -= (SIZE_T)dwWritten;
      nSize += (SIZE_T)dwWritten;
    }
  }
  else
  {
    //grow buffer?
    if ((ULONGLONG)nDataSize + nSize > SIZE_T_MAX)
      goto err_nomem;
    if (nDataSize + (SIZE_T)nSize > nMemBufferSize)
    {
      k = (nDataSize + (SIZE_T)nSize + 4095) & (~4095);
      nNewSize = nMemBufferSize << 1;
      if (nNewSize < k)
        nNewSize = k;
      lpPtr = (LPBYTE)MX_MALLOC(nNewSize);
      if (lpPtr == NULL)
        goto err_nomem;
      MemCopy(lpPtr, cMemBuffer.Get(), (SIZE_T)nSize);
      cMemBuffer.Attach(lpPtr);
      nMemBufferSize = nNewSize;
    }
    //add data to buffer
    MemCopy(cMemBuffer.Get() + (SIZE_T)nSize, lpData, nDataSize);
    nSize += (ULONGLONG)nDataSize;
    nDataSize = 0;
  }

done:
  if (FAILED(hRes))
    sParser.nState = StateError;
  return hRes;
}

} //namespace MX
