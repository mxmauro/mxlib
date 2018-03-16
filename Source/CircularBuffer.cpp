/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2015. All rights reserved.
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
#include "..\Include\CircularBuffer.h"

//-----------------------------------------------------------

namespace MX {

CCircularBuffer::CCircularBuffer() : CBaseMemObj()
{
  lpData = NULL;
  nSize = nStart = nLen = 0;
  return;
}

CCircularBuffer::~CCircularBuffer()
{
  SetBufferSize(0);
  return;
}

SIZE_T CCircularBuffer::Find(_In_ BYTE nToScan, _In_ SIZE_T nStartPos)
{
  LPBYTE lpPtr1, lpPtr2;
  SIZE_T i, nSize1,  nSize2;

  GetReadPtr(&lpPtr1, &nSize1, &lpPtr2, &nSize2);
  if (nStartPos >= nSize1+nSize2)
    return (SIZE_T)-1;
  if (nStartPos < nSize1)
  {
    for (i=nStartPos; i<nSize1; i++)
    {
      if (lpPtr1[i] == nToScan)
        return i;
    }
    i = 0;
  }
  else
  {
    i = nStartPos - nSize1;
  }
  for (; i<nSize2; i++)
  {
    if (lpPtr2[i] == nToScan)
      return i+nSize1;
  }
  return (SIZE_T)-1;
}

VOID CCircularBuffer::GetReadPtr(_Out_opt_ LPBYTE *lplpPtr1, _Out_opt_ SIZE_T *lpnSize1, _Out_opt_ LPBYTE *lplpPtr2,
                                 _Out_opt_ SIZE_T *lpnSize2)
{
  if (lplpPtr1 != NULL)
    *lplpPtr1 = NULL;
  if (lpnSize1 != NULL)
    *lpnSize1 = 0;
  if (lplpPtr2 != NULL)
    *lplpPtr2 = NULL;
  if (lpnSize2 != NULL)
    *lpnSize2 = 0;
  if (lpData == NULL)
    return;
  if (lplpPtr1 != NULL)
    *lplpPtr1 = lpData + nStart;
  if (nStart <= nSize-nLen)
  {
    if (lpnSize1 != NULL)
      *lpnSize1 = nLen;
  }
  else
  {
    if (lpnSize1 != NULL)
      *lpnSize1 = nSize-nStart;
    if (lplpPtr2 != NULL)
      *lplpPtr2 = lpData;
    if (lpnSize2 != NULL)
      *lpnSize2 = nLen - (nSize-nStart);
  }
  return;
}

HRESULT CCircularBuffer::AdvanceReadPtr(_In_ SIZE_T nCount)
{
  if (lpData == NULL)
    return E_FAIL;
  if (nCount > nLen)
    return E_INVALIDARG;
  MX_ASSERT(nStart+nCount >= nStart);
  if (nStart < nSize-nCount)
    nStart += nCount;
  else
    nStart -= nSize-nCount;
  nLen -= nCount;
  return S_OK;
}

VOID CCircularBuffer::GetWritePtr(_Out_opt_ LPBYTE *lplpPtr1, _Out_opt_ SIZE_T *lpnSize1, _Out_opt_ LPBYTE *lplpPtr2,
                                  _Out_opt_ SIZE_T *lpnSize2)
{
  SIZE_T nEnd;

  if (lplpPtr1 != NULL)
    *lplpPtr1 = NULL;
  if (lpnSize1 != NULL)
    *lpnSize1 = 0;
  if (lplpPtr2 != NULL)
    *lplpPtr2 = NULL;
  if (lpnSize2 != NULL)
    *lpnSize2 = 0;
  if (lpData == NULL || nSize == nLen)
    return;
  if (nStart < nSize-nLen)
  {
    nEnd = nStart+nLen;
    if (lplpPtr1 != NULL)
      *lplpPtr1 = lpData + nEnd;
    if (lpnSize1 != NULL)
      *lpnSize1 = nSize - nEnd;
    if (nStart > 0)
    {
      if (lplpPtr2 != NULL)
        *lplpPtr2 = lpData;
      if (lpnSize2 != NULL)
        *lpnSize2 = nStart;
    }
  }
  else
  {
    if (lplpPtr1 != NULL)
      *lplpPtr1 = lpData + (nStart - (nSize-nLen));
    if (lpnSize1 != NULL)
      *lpnSize1 = nSize - nLen;
  }
  return;
}

HRESULT CCircularBuffer::AdvanceWritePtr(_In_ SIZE_T nCount)
{
  if (lpData == NULL)
    return E_FAIL;
  if (nCount > nSize-nLen)
    return E_INVALIDARG;
  nLen += nCount;
  return S_OK;
}

SIZE_T CCircularBuffer::GetAvailableForRead() const
{
  SIZE_T nSize1, nSize2;

  const_cast<CCircularBuffer*>(this)->GetReadPtr(NULL, &nSize1, NULL, &nSize2);
  return nSize1+nSize2;
}

SIZE_T CCircularBuffer::Peek(_Out_writes_(nToRead) LPVOID lpDest, _In_ SIZE_T nToRead)
{
  LPBYTE lpPtr1, lpPtr2;
  SIZE_T nReaded, nSize1, nSize2;

  if (lpDest == NULL || nToRead == 0)
    return 0;
  GetReadPtr(&lpPtr1, &nSize1, &lpPtr2, &nSize2);
  nReaded = (nToRead < nSize1) ? nToRead : nSize1;
  MemCopy(lpDest, lpPtr1, nReaded);
  if (nToRead > nSize1)
  {
    nToRead -= nSize1;
    if (nToRead > nSize2)
      nToRead = nSize2;
    MemCopy((LPBYTE)lpDest+nReaded, lpPtr2, nToRead);
    nReaded += nToRead;
  }
  return nReaded;
}

SIZE_T CCircularBuffer::Read(_Out_writes_(nToRead) LPVOID lpDest, _In_ SIZE_T nToRead)
{
  SIZE_T nReaded;

  nReaded = Peek(lpDest, nToRead);
  if (nReaded > 0)
    AdvanceReadPtr(nReaded);
  return nReaded;
}

HRESULT CCircularBuffer::Write(_In_ LPCVOID lpSrc, _In_ SIZE_T nSrcLength, _In_ BOOL bExpandIfNeeded)
{
  LPBYTE lpPtr1, lpPtr2;
  SIZE_T _nCap, nSize1, nSize2;
  HRESULT hRes;

  if (lpSrc == NULL)
    return E_POINTER;
  if (nSrcLength == 0)
    return E_INVALIDARG;
  if (nLen+nSrcLength < nLen)
    return E_OUTOFMEMORY;
  if (nLen+nSrcLength > nSize)
  {
    if (bExpandIfNeeded == FALSE)
      return MX_E_BufferOverflow;
    _nCap = nSize << 1;
    if (_nCap < nSize || _nCap < nLen+nSrcLength)
      _nCap = nLen+nSrcLength;
    hRes = SetBufferSize(_nCap);
    if (FAILED(hRes))
      return hRes;
  }
  GetWritePtr(&lpPtr1, &nSize1, &lpPtr2, &nSize2);
  if (nSrcLength <= nSize1)
  {
    MemCopy(lpPtr1, lpSrc, nSrcLength);
  }
  else
  {
    MX_ASSERT(nSrcLength <= nSize1+nSize2);
    MemCopy(lpPtr1, lpSrc, nSize1);
    MemCopy(lpPtr2, (LPBYTE)lpSrc+nSize1, nSrcLength-nSize1);
  }
  return AdvanceWritePtr(nSrcLength);
}

HRESULT CCircularBuffer::SetBufferSize(_In_ SIZE_T _nSize)
{
  LPBYTE lpNewData;
  SIZE_T nTemp;

  if (_nSize == 0)
  {
    MX_FREE(lpData);
    nSize = nStart = nLen = 0;
  }
  else if (lpData == NULL)
  {
    _nSize = (_nSize + (size_t)4095) & (size_t)(~0x0FFF);
    lpData = (LPBYTE)MX_MALLOC(_nSize);
    if (lpData == NULL)
      return E_OUTOFMEMORY;
    nSize = _nSize;
    MX_ASSERT(nStart == 0);
    MX_ASSERT(nLen == 0);
  }
  else
  {
    if (nLen > _nSize)
      return E_OUTOFMEMORY;
    _nSize = (_nSize + (SIZE_T)4095) & (SIZE_T)(~0x0FFF);
    lpNewData = (LPBYTE)MX_MALLOC(_nSize);
    if (lpNewData == NULL)
      return E_OUTOFMEMORY;
    if (nStart+nLen <= nSize)
    {
      MemCopy(lpNewData, lpData+nStart, nLen);
    }
    else
    {
      nTemp = nSize-nStart;
      MemCopy(lpNewData, lpData+nStart, nTemp);
      MemCopy(lpNewData+nTemp, lpData, nLen-nTemp);
    }
    MX_FREE(lpData);
    lpData = lpNewData;
    nSize = _nSize;
    nStart = 0;
  }
  return S_OK;
}

VOID CCircularBuffer::ReArrangeBuffer()
{
  SIZE_T nStartAreaLen, nEndAreaLen, nToMove;
  BYTE aTempBuffer[4096];

  if (lpData != NULL && nLen > 0 && nStart > 0)
  {
    while (nStart != nSize)
    {
      //calculate block to move
      nEndAreaLen = nSize-nStart;
      nStartAreaLen = nLen - nEndAreaLen;
      if ((nToMove = nEndAreaLen) > 4096)
        nToMove = 4096;
      //move from the buffer ending area to the temp buffer
      MemCopy(aTempBuffer, lpData+(nSize-nToMove), nToMove);
      //move from the pre buffer ending area 'nToMove' bytes ahead
      if (nToMove < nEndAreaLen)
        MemMove(lpData+nStart+nToMove, lpData+nStart, nEndAreaLen-nToMove);
      //move from the buffer starting area 'nToMove' bytes ahead
      MemMove(lpData+nStart+nToMove, lpData, nStartAreaLen);
      //move from the temp buffer to the buffer starting area
      MemCopy(lpData, aTempBuffer, nToMove);
      //advance start position
      nStart += nToMove;
    }
    nStart = 0;
  }
  return;
}

} //namespace MX
