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

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqSecWebSocketKey::CHttpHeaderReqSecWebSocketKey() : CHttpHeaderBase()
{
  nKeyLength = 0;
  return;
}

CHttpHeaderReqSecWebSocketKey::~CHttpHeaderReqSecWebSocketKey()
{
  return;
}

HRESULT CHttpHeaderReqSecWebSocketKey::Parse(_In_z_ LPCSTR szValueA)
{
  CBase64Decoder cDecoder;
  LPCSTR szStartA;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;

  //skip spaces
  szStartA = SkipSpaces(szValueA);

  //reach the end
  for (szValueA = szStartA; *szValueA > L' '; szValueA++);
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
    hRes = InternalSetKey(cDecoder.GetBuffer(), cDecoder.GetOutputLength());
  if (FAILED(hRes))
    return hRes;

  //check after data
  if (*SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpHeaderReqSecWebSocketKey::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  CBase64Encoder cEncoder;
  HRESULT hRes;

  cStrDestA.Empty();

  if (nKeyLength == 0)
    return MX_E_NotReady;

  //start encoding
  hRes = cEncoder.Begin(cEncoder.GetRequiredSpace(nKeyLength));
  if (SUCCEEDED(hRes))
    hRes = cEncoder.Process(cKey.Get(), nKeyLength);
  if (SUCCEEDED(hRes))
    hRes = cEncoder.End();
  if (FAILED(hRes))
    return hRes;

  //build output
  return (cStrDestA.CopyN(cEncoder.GetBuffer(), cEncoder.GetOutputLength()) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

HRESULT CHttpHeaderReqSecWebSocketKey::SetKey(_In_ LPVOID lpKey, _In_ SIZE_T nKeyLen)
{
  if (lpKey == NULL && nKeyLen > 0)
    return E_POINTER;
  return InternalSetKey(lpKey, nKeyLen);
}

HRESULT CHttpHeaderReqSecWebSocketKey::InternalSetKey(_In_ LPVOID lpKey, _In_ SIZE_T nKeyLen)
{
  if (nKeyLen > 0)
  {
    LPBYTE lpNewKey;

    lpNewKey = (LPBYTE)MX_MALLOC(nKeyLen);
    if (lpNewKey == NULL)
      return E_POINTER;
    cKey.Attach(lpNewKey);
    MxMemCopy(lpNewKey, lpKey, nKeyLen);
  }
  else
  {
    cKey.Reset();
  }
  nKeyLength = nKeyLen;
  //done
  return S_OK;
}

LPBYTE CHttpHeaderReqSecWebSocketKey::GetKey() const
{
  return (const_cast<TAutoFreePtr<BYTE>&>(cKey)).Get();
}

SIZE_T CHttpHeaderReqSecWebSocketKey::GetKeyLength() const
{
  return nKeyLength;
}

} //namespace MX
