/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the LICENSE file distributed with
 * this work for additional information regarding copyright ownership.
 *
 * Also, if exists, check the Licenses directory for information about
 * third-party modules.
 *
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "..\..\Include\Http\HttpBodyParserDefault.h"
#include "..\..\Include\FnvHash.h"
#include <intsafe.h>

//-----------------------------------------------------------

namespace MX {

CHttpBodyParserDefault::CHttpBodyParserDefault(_In_ OnDownloadStartedCallback _cDownloadStartedCallback,
                                               _In_opt_ LPVOID _lpUserParam, _In_ DWORD _dwMaxBodySizeInMemory,
                                               _In_ ULONGLONG _ullMaxBodySize) :
                                               CHttpBodyParserBase(), CNonCopyableObj()
{
  FastLock_Initialize(&nMutex);
  cDownloadStartedCallback = _cDownloadStartedCallback;
  lpUserParam = _lpUserParam;
  dwMaxBodySizeInMemory = _dwMaxBodySizeInMemory;
  ullMaxBodySize = _ullMaxBodySize;
  nSize = 0ui64;
  nMemBufferSize = 0;
  sParser.nState = eState::Reading;
  return;
}

CHttpBodyParserDefault::~CHttpBodyParserDefault()
{
  cFileH.Close();
  cMemBuffer.Reset();
  return;
}

HRESULT CHttpBodyParserDefault::Read(_Out_writes_to_(nToRead, *lpnReaded) LPVOID lpDest, _In_ ULONGLONG nOffset,
                                     _In_ SIZE_T nToRead, _Out_opt_ SIZE_T *lpnReaded)
{
  CFastLock cLock(&nMutex);

  if (lpnReaded != NULL)
    *lpnReaded = 0;
  if (lpDest == NULL)
    return E_POINTER;
  if (nToRead == 0)
    return S_OK;
  if (nOffset >= nSize)
    return MX_E_EndOfFileReached;
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
    OVERLAPPED sOvr;
    DWORD dwToRead, dwReaded;

    nOfs = nOffset;
    while (nToRead > 0)
    {
      ::MxMemSet(&sOvr, 0, sizeof(sOvr));
      sOvr.Offset = dwOfs[0];
      sOvr.OffsetHigh = dwOfs[1];
      dwToRead = (nToRead > 65536) ? 65536 : (DWORD)nToRead;
      if (::ReadFile(cFileH, lpDest, dwToRead, &dwReaded, &sOvr) == FALSE)
        return MX_HRESULT_FROM_LASTERROR();
      if (dwToRead != dwReaded)
        return MX_E_ReadFault;
      lpDest = (LPBYTE)lpDest + (SIZE_T)dwReaded;
      nOfs += (SIZE_T)dwReaded;
      nToRead -= (SIZE_T)dwReaded;
      if (lpnReaded != NULL)
        *lpnReaded += (SIZE_T)dwReaded;
    }
  }
  else
  {
    MxMemCopy(lpDest, cMemBuffer.Get() + (SIZE_T)nOffset, nToRead);
    if (lpnReaded != NULL)
      *lpnReaded = nToRead;
  }
  return S_OK;
}

HRESULT CHttpBodyParserDefault::ToString(_Inout_ CStringA &cStrDestA)
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

VOID CHttpBodyParserDefault::KeepFile()
{
  if (cFileH.Get() != NULL)
  {
    MX_IO_STATUS_BLOCK sIoStatusBlock;
    UCHAR _DeleteFile;

    _DeleteFile = 0;
    ::MxNtSetInformationFile(cFileH, &sIoStatusBlock, &_DeleteFile, (ULONG)sizeof(_DeleteFile),
                             MxFileDispositionInformation);
  }
  return;
}

HRESULT CHttpBodyParserDefault::Initialize(_In_ Internals::CHttpParser &cHttpParser)
{
  //done
  return S_OK;
}

HRESULT CHttpBodyParserDefault::Parse(_In_opt_ LPCVOID lpData, _In_opt_ SIZE_T nDataSize)
{
  CStringW cStrTempW;
  SIZE_T k, nNewSize;
  union {
    LPBYTE s;
    LPWSTR sW;
  };
  LPBYTE lpPtr;
  DWORD dwToWrite, dwWritten;
  HRESULT hRes;

  if (lpData == NULL && nDataSize > 0)
    return E_POINTER;
  if (sParser.nState == eState::Done)
    return S_OK;
  if (sParser.nState == eState::Error)
    return MX_E_InvalidData;

  //end of parsing?
  if (lpData == NULL)
  {
    //a zero-length body was sent but all the content should go to disk so create the file
    if ((!cFileH) && dwMaxBodySizeInMemory == 0)
    {
      //switch to file container
      if (cDownloadStartedCallback)
      {
        hRes = cDownloadStartedCallback(&cFileH, L"", lpUserParam);
        if (SUCCEEDED(hRes) && (!cFileH))
          hRes = MX_HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
      }
      else
      {
        hRes = MX_HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
      }
      if (FAILED(hRes))
        goto done;
    }
    sParser.nState = eState::Done;
    return S_OK;
  }

  //check if size is greater than max size or overflow
  if (nSize + (ULONGLONG)nDataSize >= ullMaxBodySize || nSize + (ULONGLONG)nDataSize < nSize)
    return MX_E_BadLength;

  //begin
  hRes = S_OK;
  //if we are writing to memory, check if buffer threshold passed
  if ((!cFileH) && nSize + (ULONGLONG)nDataSize >= (ULONGLONG)dwMaxBodySizeInMemory)
  {
    //switch to file container
    if (cDownloadStartedCallback)
    {
      hRes = cDownloadStartedCallback(&cFileH, L"", lpUserParam);
      if (SUCCEEDED(hRes) && (!cFileH))
        hRes = MX_HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
    }
    else
    {
      hRes = MX_HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
    }
    if (FAILED(hRes))
      goto done;
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
    {
err_nomem:
      hRes = E_OUTOFMEMORY;
      goto done;
    }
    if (nDataSize + (SIZE_T)nSize > nMemBufferSize)
    {
      k = (nDataSize + (SIZE_T)nSize + 4095) & (~4095);
      nNewSize = nMemBufferSize << 1;
      if (nNewSize < k)
        nNewSize = k;
      lpPtr = (LPBYTE)MX_MALLOC(nNewSize);
      if (lpPtr == NULL)
        goto err_nomem;
      MxMemCopy(lpPtr, cMemBuffer.Get(), (SIZE_T)nSize);
      cMemBuffer.Attach(lpPtr);
      nMemBufferSize = nNewSize;
    }
    //add data to buffer
    MxMemCopy(cMemBuffer.Get() + (SIZE_T)nSize, lpData, nDataSize);
    nSize += (ULONGLONG)nDataSize;
    nDataSize = 0;
  }

done:
  if (FAILED(hRes))
    sParser.nState = eState::Error;
  return hRes;
}

} //namespace MX
