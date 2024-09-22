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
#include "..\..\Include\Http\HttpHeaderReqHost.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqHost::CHttpHeaderReqHost() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderReqHost::~CHttpHeaderReqHost()
{
  return;
}

HRESULT CHttpHeaderReqHost::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  LPCSTR szValueEndA, szHostStartA, szHostEndA;
  int nPort;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  szValueEndA = szValueA + nValueLen;

  //skip spaces
  szValueA = SkipSpaces(szValueA, szValueEndA);

  //get host
  szValueA = SkipUntil(szHostStartA = szValueA, szValueEndA, ": \t");
  if (szValueA == szHostStartA)
    return MX_E_InvalidData;
  szHostEndA = szValueA;

  //parse port if any
  nPort = -1;
  if (szValueA < szValueEndA && *szValueA == ':')
  {
    szValueA++;
    while (szValueA < szValueEndA && *szValueA == '0')
      szValueA++;
    nPort = 0;
    while (szValueA < szValueEndA && *szValueA >= '0' && *szValueA <= '9')
    {
      nPort = nPort * 10 + (int)(*szValueA++ - '0');
      if (nPort > 65535)
        return MX_E_InvalidData;
    }
    if (nPort == 0)
      return MX_E_InvalidData;
  }

  //skip spaces and check for end
  if (SkipSpaces(szValueA, szValueEndA) != szValueEndA)
    return MX_E_InvalidData;

  //set new value
  hRes = cUrl.SetHost(szHostStartA, (SIZE_T)(szHostEndA - szHostStartA));
  if (SUCCEEDED(hRes) && nPort > 0)
    hRes = cUrl.SetPort(nPort);
  if (FAILED(hRes))
    return (hRes == E_INVALIDARG) ? MX_E_InvalidData : hRes;

  //done
  return S_OK;
}

HRESULT CHttpHeaderReqHost::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  if (cStrDestA.Copy(cUrl.GetHost()) == FALSE)
    return E_OUTOFMEMORY;

  if (cStrDestA.IsEmpty() == FALSE && cUrl.GetPort() > 0)
  {
    if (cStrDestA.AppendFormat(":%d", cUrl.GetPort()) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderReqHost::SetHost(_In_z_ LPCWSTR szHostW, _In_opt_ SIZE_T nHostLen)
{
  HRESULT hRes;

  if (nHostLen == (SIZE_T)-1)
    nHostLen = StrLenW(szHostW);
  if (nHostLen == 0)
    return MX_E_InvalidData;
  if (szHostW == NULL)
    return E_POINTER;

  //some checks
  hRes = cUrl.SetHost(szHostW);
  if (FAILED(hRes))
    return (hRes == E_INVALIDARG) ? MX_E_InvalidData : hRes;

  //done
  return S_OK;
}

LPCWSTR CHttpHeaderReqHost::GetHost() const
{
  return cUrl.GetHost();
}

HRESULT CHttpHeaderReqHost::SetPort(_In_ int nPort)
{
  return cUrl.SetPort(nPort);
}

int CHttpHeaderReqHost::GetPort() const
{
  return cUrl.GetPort();
}

} //namespace MX
