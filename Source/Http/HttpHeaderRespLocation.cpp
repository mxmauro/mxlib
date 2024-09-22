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
#include "..\..\Include\Http\HttpHeaderRespLocation.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderRespLocation::CHttpHeaderRespLocation() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderRespLocation::~CHttpHeaderRespLocation()
{
  return;
}

HRESULT CHttpHeaderRespLocation::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  LPCSTR szValueEndA, szStartA;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  szValueEndA = szValueA + nValueLen;

  //skip spaces
  szValueA = SkipSpaces(szValueA, szValueEndA);

  //get location
  szValueA = SkipUntil(szStartA = szValueA, szValueEndA, " \t");
  if (szValueA == szStartA)
    return MX_E_InvalidData;

  //set location
  hRes = SetLocation(szStartA, (SIZE_T)(szValueA - szStartA));
  if (FAILED(hRes))
    return hRes;

  //skip spaces and check for end
  if (SkipSpaces(szValueA, szValueEndA) != szValueEndA)
    return MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpHeaderRespLocation::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  return (cStrDestA.Copy((LPCSTR)cStrLocationA) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

HRESULT CHttpHeaderRespLocation::SetLocation(_In_z_ LPCSTR szLocationA, _In_ SIZE_T nLocationLen)
{
  CUrl cUrl;
  HRESULT hRes;

  if (nLocationLen == (SIZE_T)-1)
    nLocationLen = StrLenA(szLocationA);
  if (nLocationLen == 0)
    return MX_E_InvalidData;
  if (szLocationA == NULL)
    return E_POINTER;

  //some checks
  hRes = cUrl.ParseFromString(szLocationA, nLocationLen);
  if (FAILED(hRes))
    return hRes;

  //set new value
  if (cStrLocationA.CopyN(szLocationA, nLocationLen) == FALSE)
    return E_OUTOFMEMORY;

  //done
  return S_OK;
}

LPCSTR CHttpHeaderRespLocation::GetLocation() const
{
  return (LPCSTR)cStrLocationA;
}

} //namespace MX
