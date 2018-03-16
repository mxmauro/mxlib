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

} //namespace MX
