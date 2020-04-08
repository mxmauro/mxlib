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
#include "..\..\Include\Http\HttpHeaderReqExpect.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqExpect::CHttpHeaderReqExpect() : CHttpHeaderBase()
{
  nExpectation = ExpectationUnsupported;
  return;
}

CHttpHeaderReqExpect::~CHttpHeaderReqExpect()
{
  return;
}

HRESULT CHttpHeaderReqExpect::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  LPCSTR szValueEndA;
  eExpectation _nExpectation = ExpectationUnsupported;

  if (szValueA == NULL)
    return E_POINTER;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  szValueEndA = szValueA + nValueLen;

  nExpectation = ExpectationUnsupported;
  //skip spaces
  szValueA = SkipSpaces(szValueA, szValueEndA);

  //check expectation
  if ((SIZE_T)(szValueEndA - szValueA) >= 12 && StrNCompareA(szValueA, "100-continue", 12, TRUE) == 0)
  {
    _nExpectation = Expectation100Continue;
    szValueA += 12;
  }
  else
  {
    return MX_E_Unsupported;
  }

  //skip spaces and check for end
  if (SkipSpaces(szValueA, szValueEndA) != szValueEndA)
    return MX_E_InvalidData;

  //done
  nExpectation = _nExpectation;
  return S_OK;
}

HRESULT CHttpHeaderReqExpect::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  switch (nExpectation)
  {
    case Expectation100Continue:
      if (cStrDestA.Copy("100-continue") == FALSE)
        return E_OUTOFMEMORY;
      return S_OK;
  }
  cStrDestA.Empty();
  return MX_E_Unsupported;
}

HRESULT CHttpHeaderReqExpect::SetExpectation(_In_ eExpectation _nExpectation)
{
  if (_nExpectation != Expectation100Continue && _nExpectation != ExpectationUnsupported)
    return E_INVALIDARG;
  nExpectation = _nExpectation;
  return S_OK;
}

CHttpHeaderReqExpect::eExpectation CHttpHeaderReqExpect::GetExpectation() const
{
  return nExpectation;
}

} //namespace MX
