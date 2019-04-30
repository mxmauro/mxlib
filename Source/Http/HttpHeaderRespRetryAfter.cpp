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
#include "..\..\Include\Http\HttpHeaderRespRetryAfter.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderRespRetryAfter::CHttpHeaderRespRetryAfter() : CHttpHeaderBase()
{
  nSeconds = 0;
  return;
}

CHttpHeaderRespRetryAfter::~CHttpHeaderRespRetryAfter()
{
  return;
}

HRESULT CHttpHeaderRespRetryAfter::Parse(_In_z_ LPCSTR szValueA)
{
  CDateTime cDt;
  ULONGLONG _nSeconds, nTemp;
  LPCSTR sA;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //check if only digits
  sA = szValueA;
  if (*szValueA >= '0' && *szValueA <= '9')
  {
    while (*sA >= '0' && *sA <= '9')
      sA++;
    if (*SkipSpaces(sA) == 0)
    {
      while (*szValueA == '0')
        szValueA++;
      _nSeconds = 0;
      while (*szValueA >= '0' && *szValueA <= '9')
      {
        nTemp = _nSeconds * 10ui64;
        if (nTemp < _nSeconds)
          return MX_E_ArithmeticOverflow;
        _nSeconds = nTemp + (ULONGLONG)(*szValueA - '0');
        if (_nSeconds < nTemp)
          return MX_E_ArithmeticOverflow;
        szValueA++;
      }
      hRes = SetSeconds(_nSeconds);
      goto done;
    }
  }
  //else assume a date
  hRes = CHttpCommon::ParseDate(cDt, szValueA);
  if (SUCCEEDED(hRes))
    hRes = SetDate(cDt);
  //done
done:
  if (FAILED(hRes))
    nSeconds = 0;
  return hRes;
}

HRESULT CHttpHeaderRespRetryAfter::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  return (cStrDestA.Format(cStrDestA, "%I64u", nSeconds) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

HRESULT CHttpHeaderRespRetryAfter::SetSeconds(_In_ ULONGLONG _nSeconds)
{
  nSeconds = _nSeconds;
  return S_OK;
}

HRESULT CHttpHeaderRespRetryAfter::SetDate(_In_ CDateTime &cDt)
{
  CDateTime cDtNow;

  nSeconds = 0;
  if (SUCCEEDED(cDtNow.SetFromNow(FALSE)))
  {
    LONGLONG nDiffSecs = cDt.GetDiff(cDtNow, CDateTime::UnitsSeconds);
    if (nDiffSecs >= 0i64)
      nSeconds = (ULONGLONG)nDiffSecs;
  }
  return S_OK;
}

ULONGLONG CHttpHeaderRespRetryAfter::GetSeconds() const
{
  return nSeconds;
}

} //namespace MX
