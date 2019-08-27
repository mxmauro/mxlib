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
#include "..\..\Include\Http\HttpHeaderReqReferer.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqReferer::CHttpHeaderReqReferer() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderReqReferer::~CHttpHeaderReqReferer()
{
  return;
}

HRESULT CHttpHeaderReqReferer::Parse(_In_z_ LPCSTR szValueA)
{
  LPCSTR szStartA;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;

  //skip spaces
  szValueA = SkipSpaces(szValueA);

  //get referer
  szValueA = SkipUntil(szStartA = szValueA, " \t");
  if (szValueA == szStartA)
    return MX_E_InvalidData;

  //set referer
  hRes = SetReferer(szStartA, (SIZE_T)(szValueA - szStartA));
  if (FAILED(hRes))
    return hRes;

  //skip spaces and check for end
  if (*CHttpHeaderBase::SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;
  //done
  return S_OK;
}

HRESULT CHttpHeaderReqReferer::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  return (cStrDestA.Copy((LPCSTR)cStrRefererA) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

HRESULT CHttpHeaderReqReferer::SetReferer(_In_z_ LPCSTR szRefererA, _In_ SIZE_T nRefererLen)
{
  CUrl cUrl;
  HRESULT hRes;

  if (nRefererLen == (SIZE_T)-1)
    nRefererLen = StrLenA(szRefererA);
  if (nRefererLen == 0)
    return MX_E_InvalidData;
  if (szRefererA == NULL)
    return E_POINTER;
  //some checks
  hRes = cUrl.ParseFromString(szRefererA, nRefererLen);
  if (FAILED(hRes))
    return hRes;
  //set new value
  if (cStrRefererA.CopyN(szRefererA, nRefererLen) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCSTR CHttpHeaderReqReferer::GetReferer() const
{
  return (LPSTR)cStrRefererA;
}

} //namespace MX
