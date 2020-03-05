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
        _nSeconds = nTemp + (ULONGLONG)(*szValueA) - (ULONGLONG)'0';
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
