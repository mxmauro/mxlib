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
#include "..\Include\Streams.h"
#include "..\Include\Strings\Strings.h"

//-----------------------------------------------------------

namespace MX {

HRESULT CStream::WriteString(__in LPCSTR szFormatA, ...)
{
  va_list args;
  HRESULT hRes;

  va_start(args, szFormatA);
  hRes = WriteStringV(szFormatA, args);
  va_end(args);
  return hRes;
}

HRESULT CStream::WriteStringV(__in LPCSTR szFormatA, __in va_list argptr)
{
  CStringA cStrTempA;
  SIZE_T nWritten;
  HRESULT hRes;

  if (cStrTempA.FormatV(szFormatA, argptr) == FALSE)
    return E_OUTOFMEMORY;
  hRes = Write((LPSTR)cStrTempA, cStrTempA.GetLength(), nWritten);
  if (SUCCEEDED(hRes) && nWritten != cStrTempA.GetLength())
    hRes = MX_E_WriteFault;
  return hRes;
}

HRESULT CStream::WriteString(__in LPCWSTR szFormatA, ...)
{
  va_list args;
  HRESULT hRes;

  va_start(args, szFormatA);
  hRes = WriteStringV(szFormatA, args);
  va_end(args);
  return hRes;
}

HRESULT CStream::WriteStringV(__in LPCWSTR szFormatA, __in va_list argptr)
{
  CStringW cStrTempW;
  SIZE_T nWritten;
  HRESULT hRes;

  if (cStrTempW.FormatV(szFormatA, argptr) == FALSE)
    return E_OUTOFMEMORY;
  hRes = Write((LPWSTR)cStrTempW, cStrTempW.GetLength(), nWritten);
  if (SUCCEEDED(hRes) && nWritten != cStrTempW.GetLength())
    hRes = MX_E_WriteFault;
  return hRes;
}

//-----------------------------------------------------------

CStreamFragment::CStreamFragment() : CStream()
{
  nOffset = nLength = nCurrPos = 0ui64;
  return;
}

HRESULT CStreamFragment::Initialize(__in CSeekableStream *lpStream, __in ULONGLONG _nOffset, __in ULONGLONG _nLength)
{
  ULONGLONG nMaxLength;
  HRESULT hRes;

  cStream.Release();
  nOffset = nLength = nCurrPos = 0ui64;
  if (lpStream != NULL)
  {
    nMaxLength = lpStream->GetLength();
    cStream.Attach(lpStream->Clone());
    if (!cStream)
      return E_OUTOFMEMORY;
    if (_nOffset < nMaxLength)
    {
      nOffset = _nOffset;
      if (_nLength > nMaxLength-nOffset)
        _nLength = nMaxLength - nOffset;
      nLength = _nLength;
    }
    hRes = cStream->Seek(nOffset, CSeekableStream::SeekStart);
    if (FAILED(hRes))
    {
      cStream.Release();
      nOffset = nLength = 0ui64;
      return hRes;
    }
  }
  //done
  return S_OK;
}

HRESULT CStreamFragment::Read(__out LPVOID lpDest, __in SIZE_T nBytes, __out SIZE_T &nReaded)
{
  HRESULT hRes;

  if (!cStream)
  {
    nReaded = 0;
    return MX_E_NotReady;
  }
  if (nLength - nCurrPos > (ULONGLONG)nBytes)
    nBytes = (SIZE_T)(nLength - nCurrPos);
  hRes = cStream->Read(lpDest, nBytes, nReaded);
  nCurrPos += (ULONGLONG)nReaded;
  return hRes;
}

HRESULT CStreamFragment::Write(__in LPCVOID lpSrc, __in SIZE_T nBytes, __out SIZE_T &nWritten)
{
  nWritten = 0;
  return (!cStream) ? MX_E_NotReady : E_NOTIMPL;
}

ULONGLONG CStreamFragment::GetLength() const
{
  return nLength;
}

HRESULT CStreamFragment::Seek(__in ULONGLONG nPosition, __in_opt CSeekableStream::eSeekMethod nMethod)
{
  HRESULT hRes;

  if (!cStream)
    return MX_E_NotReady;
  switch (nMethod)
  {
    case CSeekableStream::SeekStart:
      if (nPosition > nLength)
        nPosition = nLength;
      break;
    case CSeekableStream::SeekCurrent:
      if ((LONGLONG)nPosition > 0)
      {
        if (nPosition > nLength - nCurrPos)
          nPosition = nLength - nCurrPos;
        nPosition += nCurrPos;
      }
      else if ((LONGLONG)nPosition < 0)
      {
        nPosition = (ULONGLONG)(-((LONGLONG)nPosition));
        if (nPosition > nCurrPos)
          nPosition = 0;
        else
          nPosition = nCurrPos - nPosition;
      }
      else
      {
        nPosition = nCurrPos;
      }
      break;
    case CSeekableStream::SeekEnd:
      if (nPosition > nLength)
        nPosition = nLength;
      nPosition = nLength - nPosition;
      break;
    default:
      return E_INVALIDARG;
  }
  hRes = cStream->Seek(nOffset+nPosition);
  if (SUCCEEDED(hRes))
    nCurrPos = nPosition;
  return hRes;
}

CStreamFragment* CStreamFragment::Clone()
{
  TAutoRefCounted<CStreamFragment> cNewStream;

  cNewStream.Attach(MX_DEBUG_NEW CStreamFragment());
  if (!cNewStream)
    return NULL;
  if (FAILED(cNewStream->Initialize(cStream, nOffset, nLength)))
    return NULL;
  if (FAILED(cNewStream->Seek(nCurrPos)))
    return NULL;
  return cNewStream.Detach();
}

} //namespace MX
