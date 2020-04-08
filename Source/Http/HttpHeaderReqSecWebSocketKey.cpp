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
#include "..\..\Include\Http\HttpHeaderReqSecWebSocketKey.h"
#include "..\..\Include\Crypto\Base64.h"
#include "..\..\Include\Crypto\SecureRandom.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqSecWebSocketKey::CHttpHeaderReqSecWebSocketKey() : CHttpHeaderBase()
{
  lpKey = NULL;
  nKeyLength = 0;
  return;
}

CHttpHeaderReqSecWebSocketKey::~CHttpHeaderReqSecWebSocketKey()
{
  SecureFreeKey();
  return;
}

HRESULT CHttpHeaderReqSecWebSocketKey::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  CBase64Decoder cDecoder;
  LPCSTR szValueEndA, szStartA;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  szValueEndA = szValueA + nValueLen;

  //skip spaces
  szStartA = SkipSpaces(szValueA, szValueEndA);

  //reach the end
  for (szValueA = szStartA; szValueA < szValueEndA && *szValueA > ' '; szValueA++);
  if (szValueA == szStartA)
    return MX_E_InvalidData;

  //start decoding
  hRes = cDecoder.Begin(cDecoder.GetRequiredSpace((SIZE_T)(szValueA - szStartA)));
  if (SUCCEEDED(hRes))
    hRes = cDecoder.Process(szStartA, (SIZE_T)(szValueA - szStartA));
  if (SUCCEEDED(hRes))
    hRes = cDecoder.End();

  //and set key
  if (SUCCEEDED(hRes))
  {
    hRes = InternalSetKey(cDecoder.GetBuffer(), cDecoder.GetOutputLength());
    ::MxMemSet(cDecoder.GetBuffer(), 0, cDecoder.GetOutputLength());
  }
  if (FAILED(hRes))
    return hRes;

  //check after data
  if (SkipSpaces(szValueA, szValueEndA) != szValueEndA)
    return MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpHeaderReqSecWebSocketKey::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  CBase64Encoder cEncoder;
  HRESULT hRes;

  cStrDestA.Empty();

  if (lpKey == NULL)
    return MX_E_NotReady;

  //start encoding
  hRes = cEncoder.Begin(cEncoder.GetRequiredSpace(nKeyLength));
  if (SUCCEEDED(hRes))
    hRes = cEncoder.Process(lpKey, nKeyLength);
  if (SUCCEEDED(hRes))
    hRes = cEncoder.End();
  if (FAILED(hRes))
    return hRes;

  //build output
  return (cStrDestA.CopyN(cEncoder.GetBuffer(), cEncoder.GetOutputLength()) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

HRESULT CHttpHeaderReqSecWebSocketKey::GenerateKey(_In_ SIZE_T nKeyLen)
{
  LPBYTE lpNewKey;

  if (nKeyLen == 0)
    return E_INVALIDARG;

  lpNewKey = (LPBYTE)MX_MALLOC(nKeyLen);
  if (lpNewKey == NULL)
    return E_POINTER;
  SecureRandom::Generate(lpNewKey, nKeyLen);

  SecureFreeKey();

  lpKey = lpNewKey;
  nKeyLength = nKeyLen;

  //done
  return S_OK;
}

HRESULT CHttpHeaderReqSecWebSocketKey::SetKey(_In_ LPVOID lpKey, _In_ SIZE_T nKeyLen)
{
  if (lpKey == NULL && nKeyLen > 0)
    return E_POINTER;
  return InternalSetKey(lpKey, nKeyLen);
}

HRESULT CHttpHeaderReqSecWebSocketKey::InternalSetKey(_In_ LPVOID _lpKey, _In_ SIZE_T nKeyLen)
{
  if (nKeyLen > 0)
  {
    LPBYTE lpNewKey;

    lpNewKey = (LPBYTE)MX_MALLOC(nKeyLen);
    if (lpNewKey == NULL)
      return E_POINTER;
    ::MxMemCopy(lpNewKey, _lpKey, nKeyLen);

    SecureFreeKey();

    lpKey = lpNewKey;
    nKeyLength = nKeyLen;
  }
  else
  {
    SecureFreeKey();
  }
  //done
  return S_OK;
}

VOID CHttpHeaderReqSecWebSocketKey::SecureFreeKey()
{
  if (lpKey != NULL)
  {
    ::MxMemSet(lpKey, 0, nKeyLength);
    MX_FREE(lpKey);
  }
  lpKey = NULL;
  nKeyLength = 0;
  return;
}

} //namespace MX
