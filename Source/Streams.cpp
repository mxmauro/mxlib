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
#include "..\Include\AutoPtr.h"

//-----------------------------------------------------------

namespace MX {

HRESULT CStream::WriteString(_In_ LPCSTR szFormatA, ...)
{
  va_list args;
  HRESULT hRes;

  va_start(args, szFormatA);
  hRes = WriteStringV(szFormatA, args);
  va_end(args);
  return hRes;
}

HRESULT CStream::WriteStringV(_In_ LPCSTR szFormatA, _In_ va_list argptr)
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

HRESULT CStream::WriteString(_In_ LPCWSTR szFormatA, ...)
{
  va_list args;
  HRESULT hRes;

  va_start(args, szFormatA);
  hRes = WriteStringV(szFormatA, args);
  va_end(args);
  return hRes;
}

HRESULT CStream::WriteStringV(_In_ LPCWSTR szFormatA, _In_ va_list argptr)
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

HRESULT CStream::Seek(_In_ ULONGLONG nPosition, _In_opt_ eSeekMethod nMethod)
{
  return E_NOTIMPL;
}
HRESULT CStream::CopyTo(_In_ CStream *lpTarget, _In_ SIZE_T nBytes, _Out_ SIZE_T &nBytesWritten,
                        _In_opt_ ULONGLONG nStartOffset)
{
  MX::TAutoFreePtr<BYTE> aBuffer;
  BYTE aMemSafeBuf[1024], *lpTempBuf;
  SIZE_T nBufferSize, nThisBytesRead, nThisBytesWritten;
  HRESULT hRes;

  nBytesWritten = 0;
  if (lpTarget == NULL)
    return E_POINTER;
  nBufferSize = 65536;
  aBuffer.Attach((LPBYTE)MX_MALLOC(nBufferSize));
  if (aBuffer)
  {
    lpTempBuf = aBuffer.Get();
  }
  else
  {
    nBufferSize = sizeof(aMemSafeBuf);
    lpTempBuf = aMemSafeBuf;
  }
  hRes = S_OK;
  while (nBytes > 0)
  {
    hRes = Read(lpTempBuf, (nBytes > nBufferSize) ? nBufferSize : nBytes, nThisBytesRead, nStartOffset);
    if (FAILED(hRes))
      break;
    if (nThisBytesRead == 0)
      break;
    nBytes -= nThisBytesRead;
    if (nStartOffset != ULONGLONG_MAX)
      nStartOffset += (ULONGLONG)nThisBytesRead;

    hRes = lpTarget->Write(lpTempBuf, nThisBytesRead, nThisBytesWritten);
    if (FAILED(hRes))
      break;
    nBytesWritten += nThisBytesWritten;
    if (nThisBytesWritten != nThisBytesRead)
    {
      hRes = MX_E_WriteFault;
      break;
    }
  }
  //done
  return (hRes == MX_E_EndOfFileReached) ? S_OK : hRes;
}

} //namespace MX
