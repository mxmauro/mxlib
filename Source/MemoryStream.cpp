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
#include "..\Include\MemoryStream.h"

//-----------------------------------------------------------

namespace MX {

CMemoryStream::CMemoryStream(_In_opt_ SIZE_T nAllocationGranularity) : CStream()
{
  lpData = NULL;
  nCurrPos = nSize = nAllocated = 0;
  if (nAllocationGranularity < 0x0000100)
  {
    nGranularity = 0x0000100;
  }
  else if (nAllocationGranularity > 0x1000000)
  {
    nGranularity = 0x1000000;
  }
  else
  {
    nAllocationGranularity--;
    nAllocationGranularity |= nAllocationGranularity >> 1;
    nAllocationGranularity |= nAllocationGranularity >> 2;
    nAllocationGranularity |= nAllocationGranularity >> 4;
    nAllocationGranularity |= nAllocationGranularity >> 8;
    nAllocationGranularity |= nAllocationGranularity >> 16;
    nGranularity = nAllocationGranularity + 1;
  }
  bCanGrow = FALSE;
  return;
}

CMemoryStream::~CMemoryStream()
{
  Close();
  return;
}

HRESULT CMemoryStream::Create(_In_opt_ SIZE_T nInitialSize, _In_opt_ BOOL bGrowable)
{
  Close();
  if (nInitialSize > 0)
  {
    if (EnsureSize(nInitialSize) == FALSE)
      return E_OUTOFMEMORY;
    nSize = nInitialSize;
  }
  bCanGrow = bGrowable;
  //done
  return S_OK;
}

VOID CMemoryStream::Close()
{
  MX_FREE(lpData);
  nCurrPos = nSize = nAllocated = 0;
  return;
}

HRESULT CMemoryStream::Read(_Out_ LPVOID lpDest, _In_ SIZE_T nBytes, _Out_ SIZE_T &nBytesRead,
                            _In_opt_ ULONGLONG nStartOffset)
{
  SIZE_T nReadPos;

  nBytesRead = 0;
  if (lpDest == NULL)
    return E_POINTER;
  if (nStartOffset == ULONGLONG_MAX)
  {
    if (nCurrPos >= nSize)
      return MX_E_EndOfFileReached;
    nReadPos = nCurrPos;
  }
  else
  {
    if (nStartOffset >= (ULONGLONG)nSize)
      return MX_E_EndOfFileReached;
    nReadPos = (SIZE_T)nStartOffset;
  }
  if (nBytes > nSize - nReadPos)
    nBytes = nSize - nReadPos;
  MxMemCopy(lpDest, lpData + nReadPos, nBytes);
  if (nStartOffset == ULONGLONG_MAX)
    nCurrPos += nBytes;
  //done
  nBytesRead = nBytes;
  return S_OK;
}

HRESULT CMemoryStream::Write(_In_ LPCVOID lpSrc, _In_ SIZE_T nBytes, _Out_ SIZE_T &nBytesWritten,
                             _In_opt_ ULONGLONG nStartOffset)
{
  SIZE_T nWritePos;

  nBytesWritten = 0;
  if (nBytes == 0)
    return S_OK;
  if (lpSrc == NULL)
    return E_POINTER;
  if (nStartOffset == ULONGLONG_MAX)
  {
    nWritePos = nCurrPos;
  }
  else
  {
    if (nStartOffset > (ULONGLONG)SIZE_T_MAX)
      return MX_E_Unsupported;
    nWritePos = (SIZE_T)nStartOffset;
  }
  if (nBytes > nSize - nWritePos)
  {
    if (bCanGrow != FALSE)
    {
      if (EnsureSize(nWritePos + nBytes) == FALSE)
        return E_OUTOFMEMORY;
    }
    else
    {
      nBytes = nSize - nWritePos;
    }
  }
  MxMemCopy(lpData + nWritePos, lpSrc, nBytes);
  if (nStartOffset == ULONGLONG_MAX)
    nCurrPos += nBytes;
  if (nSize < nWritePos + nBytes)
    nSize = nWritePos + nBytes;
  nBytesWritten = nBytes;
  //done
  return S_OK;
}

HRESULT CMemoryStream::Seek(_In_ ULONGLONG nPosition, _In_opt_ eSeekMethod nMethod)
{
  switch (nMethod)
  {
    case SeekStart:
      if (nPosition > (ULONGLONG)nSize)
        nPosition = (ULONGLONG)nSize;
      break;

    case SeekCurrent:
      if ((LONGLONG)nPosition >= 0)
      {
        if (nPosition > (ULONGLONG)(nSize - nCurrPos))
          nPosition = (ULONGLONG)(nSize - nCurrPos);
        nPosition += (ULONGLONG)nCurrPos;
      }
      else
      {
        nPosition = (~nPosition) + 1;
        if (nPosition > (ULONGLONG)nCurrPos)
          return E_FAIL;
        nPosition = (ULONGLONG)nCurrPos - nPosition;
      }
      break;

    case SeekEnd:
      if (nPosition > (ULONGLONG)nSize)
        nPosition = (ULONGLONG)nSize;
      nPosition = (ULONGLONG)nSize - nPosition;
      break;

    default:
      return E_INVALIDARG;
  }
  nCurrPos = (SIZE_T)nPosition;
  //done
  return S_OK;
}

ULONGLONG CMemoryStream::GetLength() const
{
  return (ULONGLONG)nSize;
}

BOOL CMemoryStream::CanGrow() const
{
  return bCanGrow;
}

CMemoryStream* CMemoryStream::Clone()
{
  return NULL;
}

BOOL CMemoryStream::EnsureSize(_In_ SIZE_T nRequiredSize)
{
  if (nRequiredSize > nAllocated)
  {
    LPBYTE lpNewData;

    if (nRequiredSize < SIZE_T_MAX - nGranularity)
      nRequiredSize = (nRequiredSize + nGranularity - 1) & (~(nGranularity-1));
    if (nAllocated == 0)
      lpNewData = (LPBYTE)MX_MALLOC(nRequiredSize);
    else
      lpNewData = (LPBYTE)MX_REALLOC(lpData, nRequiredSize);
    if (lpNewData == NULL)
      return FALSE;
    lpData = lpNewData;
    nAllocated = nRequiredSize;
  }
  return TRUE;
}

} //namespace MX
