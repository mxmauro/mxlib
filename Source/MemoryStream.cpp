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
#include "..\Include\MemoryStream.h"

//-----------------------------------------------------------

namespace MX {

CMemoryStream::CMemoryStream(__in_opt SIZE_T nAllocationGranularity) : CSeekableStream()
{
  lpData = NULL;
  nOffset = nSize = nAllocated = 0;
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

HRESULT CMemoryStream::Create(__in_opt SIZE_T nInitialSize, __in_opt BOOL bGrowable)
{
  Close();
  if (nInitialSize > 0)
  {
    nAllocated = (nInitialSize+nGranularity-1) & (~nGranularity);
    if (nAllocated < nInitialSize)
      nAllocated = nInitialSize;
    lpData = (LPBYTE)MX_MALLOC(nAllocated);
    if (lpData == NULL)
    {
      nAllocated = 0;
      return E_OUTOFMEMORY;
    }
    nSize = nInitialSize;
  }
  bCanGrow = bGrowable;
  //done
  return S_OK;
}

VOID CMemoryStream::Close()
{
  MX_FREE(lpData);
  nOffset = nSize = nAllocated = 0;
  return;
}

HRESULT CMemoryStream::Read(__out LPVOID lpDest, __in SIZE_T nBytes, __out SIZE_T &nReaded)
{
  nReaded = 0;
  if (lpDest == NULL)
    return E_POINTER;
  if (nOffset >= nSize)
    return MX_E_EndOfFileReached;
  if (nBytes > nSize - nOffset)
    nBytes = nSize - nOffset;
  MemCopy(lpDest, lpData+nOffset, nBytes);
  nOffset += nBytes;
  //done
  nReaded = nBytes;
  return S_OK;
}

HRESULT CMemoryStream::Write(__in LPCVOID lpSrc, __in SIZE_T nBytes, __out SIZE_T &nWritten)
{
  LPBYTE lpNewData;
  SIZE_T nNewAllocated;

  nWritten = 0;
  if (nBytes == 0)
    return S_OK;
  if (lpSrc == NULL)
    return E_POINTER;
  if (nBytes > nSize - nOffset)
  {
    if (bCanGrow != FALSE)
    {
      if (nBytes > nAllocated - nOffset)
      {
        if (nOffset+nBytes < nOffset)
          return E_OUTOFMEMORY;
        nNewAllocated = (nOffset+nBytes+nGranularity-1) & (~nGranularity);
        if (nNewAllocated < nOffset+nBytes)
          nNewAllocated = nOffset+nBytes;
        lpNewData = (LPBYTE)MX_REALLOC(lpData, nNewAllocated);
        if (lpNewData == NULL)
          return E_OUTOFMEMORY;
        lpData = lpNewData;
        nAllocated = nNewAllocated;
      }
      nSize = nOffset + nBytes;
    }
    else
    {
      nBytes = nSize - nOffset;
    }
  }
  MemCopy(lpData+nOffset, lpSrc, nBytes);
  nOffset += nBytes;
  //done
  nWritten = nBytes;
  return S_OK;
}

HRESULT CMemoryStream::Seek(__in ULONGLONG nPosition, __in_opt eSeekMethod nMethod)
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
        if (nPosition > (ULONGLONG)(nSize - nOffset))
          nPosition = (ULONGLONG)(nSize - nOffset);
        nPosition += (ULONGLONG)nOffset;
      }
      else
      {
        nPosition = (~nPosition) + 1;
        if (nPosition > (ULONGLONG)nOffset)
          return E_FAIL;
        nPosition = nOffset - (SIZE_T)nPosition;
      }
      break;

    case SeekEnd:
      if (nPosition >(ULONGLONG)nSize)
        nPosition = (ULONGLONG)nSize;
      nPosition = (ULONGLONG)nSize - nPosition;
      break;

    default:
      return E_INVALIDARG;
  }
  nOffset = (SIZE_T)nPosition;
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

} //namespace MX
