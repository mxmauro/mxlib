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
#include "..\..\Include\Http\HttpHeaderReqSecWebSocketVersion.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqSecWebSocketVersion::CHttpHeaderReqSecWebSocketVersion() : CHttpHeaderBase()
{
  nVersion = -1;
  return;
}

CHttpHeaderReqSecWebSocketVersion::~CHttpHeaderReqSecWebSocketVersion()
{
  return;
}

HRESULT CHttpHeaderReqSecWebSocketVersion::Parse(_In_z_ LPCSTR szValueA)
{
  int _nVersion;

  if (szValueA == NULL)
    return E_POINTER;

  //skip spaces
  szValueA = SkipSpaces(szValueA);
  if (*szValueA == 0)
    return MX_E_InvalidData;

  //get value
  _nVersion = 0;
  while (*szValueA >= '0' && *szValueA <= '9')
  {
    _nVersion = _nVersion * 10 + (int)((*szValueA) - '0');
    if (_nVersion > 255)
      return MX_E_InvalidData;
    szValueA++;
  }

  //check for end
  if (*SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;

  //done
  nVersion = _nVersion;
  return S_OK;
}

HRESULT CHttpHeaderReqSecWebSocketVersion::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  if (nVersion < 0)
  {
    cStrDestA.Empty();
    return MX_E_NotReady;
  }
  //done
  return (cStrDestA.Format("%ld", nVersion) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

HRESULT CHttpHeaderReqSecWebSocketVersion::SetVersion(_In_ int _nVersion)
{
  if (_nVersion < 0 || _nVersion > 255)
    return E_INVALIDARG;
  nVersion = _nVersion;
  //done
  return S_OK;
}

int CHttpHeaderReqSecWebSocketVersion::GetVersion() const
{
  return nVersion;
}

} //namespace MX
