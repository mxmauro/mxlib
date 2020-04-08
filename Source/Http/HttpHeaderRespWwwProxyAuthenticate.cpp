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
#include "..\..\Include\Http\HttpHeaderRespWwwProxyAuthenticate.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderRespWwwProxyAuthenticateCommon::CHttpHeaderRespWwwProxyAuthenticateCommon() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderRespWwwProxyAuthenticateCommon::~CHttpHeaderRespWwwProxyAuthenticateCommon()
{
  return;
}

HRESULT CHttpHeaderRespWwwProxyAuthenticateCommon::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  LPCSTR szValueEndA;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  szValueEndA = szValueA + nValueLen;

  //skip spaces
  szValueA = SkipSpaces(szValueA, szValueEndA);

  //check type
  if ((SIZE_T)(szValueEndA - szValueA) >= 5 && StrNCompareA(szValueA, "basic", 5, TRUE) == 0)
  {
    szValueA += 5;
    cHttpAuth.Attach(MX_DEBUG_NEW CHttpAuthBasic());
    if (!cHttpAuth)
      return E_OUTOFMEMORY;
  }
  else if ((SIZE_T)(szValueEndA - szValueA) >= 6 && StrNCompareA(szValueA, "digest", 6, TRUE) == 0)
  {
    szValueA += 6;
    cHttpAuth.Attach(MX_DEBUG_NEW CHttpAuthDigest());
    if (!cHttpAuth)
      return E_OUTOFMEMORY;
  }
  else
  {
    return MX_E_Unsupported;
  }
  if (szValueA < szValueEndA && *szValueA != ' ' && *szValueA != '\t')
  {
    return ((*szValueA >= 'A' && *szValueA <= 'Z') ||
            (*szValueA >= 'a' && *szValueA <= 'z') ||
            (*szValueA >= '0' && *szValueA <= '9')) ? MX_E_Unsupported : MX_E_InvalidData;
  }

  //parse header
  hRes = cHttpAuth->Parse(szValueA, (SIZE_T)(szValueEndA - szValueA));

  //done
  if (FAILED(hRes))
    cHttpAuth.Release();
  return hRes;
}

HRESULT CHttpHeaderRespWwwProxyAuthenticateCommon::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  cStrDestA.Empty();
  return MX_E_Unsupported;
}

CHttpAuthBase* CHttpHeaderRespWwwProxyAuthenticateCommon::GetHttpAuth()
{
  return cHttpAuth.Get();
}

//--------

CHttpHeaderRespWwwAuthenticate::CHttpHeaderRespWwwAuthenticate() : CHttpHeaderRespWwwProxyAuthenticateCommon()
{
  return;
}

CHttpHeaderRespWwwAuthenticate::~CHttpHeaderRespWwwAuthenticate()
{
  return;
}

//--------

CHttpHeaderRespProxyAuthenticate::CHttpHeaderRespProxyAuthenticate() : CHttpHeaderRespWwwProxyAuthenticateCommon()
{
  return;
}

CHttpHeaderRespProxyAuthenticate::~CHttpHeaderRespProxyAuthenticate()
{
  return;
}

} //namespace MX
