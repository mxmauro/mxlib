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
#include "..\..\Include\Http\HttpHeaderRespAge.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderRespAge::CHttpHeaderRespAge() : CHttpHeaderBase()
{
  nAge = 0ui64;
  return;
}

CHttpHeaderRespAge::~CHttpHeaderRespAge()
{
  return;
}

HRESULT CHttpHeaderRespAge::Parse(_In_z_ LPCSTR szValueA)
{
  ULONGLONG _nAge, nTemp;

  if (szValueA == NULL)
    return E_POINTER;
  _nAge = 0ui64;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //get first byte
  if (*szValueA < '0' || *szValueA > '9')
    return MX_E_InvalidData;
  while (*szValueA == '0')
    szValueA++;
  while (*szValueA >= '0' && *szValueA <= '9')
  {
    nTemp = _nAge * 10ui64;
    if (nTemp < _nAge)
      return MX_E_ArithmeticOverflow;
    _nAge = nTemp + (ULONGLONG)(*szValueA) - (ULONGLONG)'0';
    if (_nAge < nTemp)
      return MX_E_ArithmeticOverflow;
    szValueA++;
  }
  //skip spaces and check for end
  if (*SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;
  //set new value
  nAge = _nAge;
  //done
  return S_OK;
}

HRESULT CHttpHeaderRespAge::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  if (cStrDestA.Format("%I64u", nAge) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

HRESULT CHttpHeaderRespAge::SetAge(_In_ ULONGLONG _nAge)
{
  nAge = _nAge;
  return S_OK;
}

ULONGLONG CHttpHeaderRespAge::GetAge() const
{
  return nAge;
}

} //namespace MX
