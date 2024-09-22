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
#include "..\..\Include\Http\HttpHeaderRespSecWebSocketProtocol.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderRespSecWebSocketProtocol::CHttpHeaderRespSecWebSocketProtocol() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderRespSecWebSocketProtocol::~CHttpHeaderRespSecWebSocketProtocol()
{
  return;
}

HRESULT CHttpHeaderRespSecWebSocketProtocol::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
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
  if (szValueA >= szValueEndA)
    return MX_E_InvalidData;

  //protocol
  szValueA = SkipUntil(szStartA = szValueA, szValueEndA, ";, \t");
  if (szValueA == szStartA)
    return MX_E_InvalidData;

  hRes = SetProtocol(szStartA, (SIZE_T)(szValueA - szStartA));
  if (FAILED(hRes))
    return hRes;

  //check for end
  if (SkipSpaces(szValueA, szValueEndA) != szValueEndA)
    return MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpHeaderRespSecWebSocketProtocol::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  if (cStrProtocolA.IsEmpty() != FALSE)
  {
    cStrDestA.Empty();
    return MX_E_NotReady;
  }
  //done
  return (cStrDestA.CopyN((LPCSTR)cStrProtocolA, cStrProtocolA.GetLength()) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

HRESULT CHttpHeaderRespSecWebSocketProtocol::SetProtocol(_In_z_ LPCSTR szProtocolA, _In_opt_ SIZE_T nProtocolLen)
{
  LPCSTR szStartA;

  if (nProtocolLen == (SIZE_T)-1)
    nProtocolLen = StrLenA(szProtocolA);
  if (nProtocolLen == 0)
    return MX_E_InvalidData;
  if (szProtocolA == NULL)
    return E_POINTER;

  szProtocolA = GetToken(szStartA = szProtocolA, szProtocolA + nProtocolLen);
  if (szProtocolA != szStartA + nProtocolLen)
    return MX_E_InvalidData;

  //set protocol
  if (cStrProtocolA.CopyN(szStartA, nProtocolLen) == FALSE)
    return E_OUTOFMEMORY;

  //done
  return S_OK;
}

LPCSTR CHttpHeaderRespSecWebSocketProtocol::GetProtocol() const
{
  return (LPCSTR)cStrProtocolA;
}

} //namespace MX
