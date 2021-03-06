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
#include "..\..\Include\Http\HttpHeaderRespSecWebSocketAccept.h"
#include "..\..\Include\Crypto\Base64.h"
#include "..\..\Include\Crypto\MessageDigest.h"

//-----------------------------------------------------------

static const LPCSTR szGuidA = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

//-----------------------------------------------------------

static HRESULT CalculateHash(_In_ LPVOID lpKey, _In_ SIZE_T nKeyLen, _Out_ BYTE aSHA1[20]);

//-----------------------------------------------------------

namespace MX {

CHttpHeaderRespSecWebSocketAccept::CHttpHeaderRespSecWebSocketAccept() : CHttpHeaderBase()
{
  ::MxMemSet(aSHA1, 0, sizeof(aSHA1));
  return;
}

CHttpHeaderRespSecWebSocketAccept::~CHttpHeaderRespSecWebSocketAccept()
{
  ::MxMemSet(aSHA1, 0, sizeof(aSHA1));
  return;
}

HRESULT CHttpHeaderRespSecWebSocketAccept::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
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
  {
    hRes = cDecoder.Process(szStartA, (SIZE_T)(szValueA - szStartA));
    if (SUCCEEDED(hRes))
      hRes = cDecoder.End();
  }
  //and set key
  if (SUCCEEDED(hRes))
  {
    if (cDecoder.GetOutputLength() == 20)
      ::MxMemCopy(aSHA1, cDecoder.GetBuffer(), 20);
    else
      hRes = MX_E_InvalidData;
  }
  if (FAILED(hRes))
    return hRes;

  //check after data
  if (SkipSpaces(szValueA, szValueEndA) != szValueEndA)
    return MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpHeaderRespSecWebSocketAccept::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  CBase64Encoder cEncoder;
  HRESULT hRes;

  //start encoding
  hRes = cEncoder.Begin(cEncoder.GetRequiredSpace(sizeof(aSHA1)));
  if (SUCCEEDED(hRes))
  {
    hRes = cEncoder.Process(aSHA1, sizeof(aSHA1));
    if (SUCCEEDED(hRes))
      hRes = cEncoder.End();
  }
  if (FAILED(hRes))
    return hRes;

  //build output
  return (cStrDestA.CopyN(cEncoder.GetBuffer(), cEncoder.GetOutputLength()) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

HRESULT CHttpHeaderRespSecWebSocketAccept::SetKey(_In_ LPVOID lpKey, _In_ SIZE_T nKeyLen)
{
  return CalculateHash(lpKey, nKeyLen, aSHA1);
}

LPBYTE CHttpHeaderRespSecWebSocketAccept::GetSHA1() const
{
  return const_cast<LPBYTE>(aSHA1);
}

HRESULT CHttpHeaderRespSecWebSocketAccept::VerifyKey(_In_ LPVOID lpKey, _In_ SIZE_T nKeyLen)
{
  BYTE aVerifierSHA1[20];
  HRESULT hRes;

  hRes = CalculateHash(lpKey, nKeyLen, aVerifierSHA1);
  if (SUCCEEDED(hRes))
  {
    hRes = (::MxMemCompare(aSHA1, aVerifierSHA1, sizeof(aSHA1)) == 0) ? S_OK : S_FALSE;
  }
  return hRes;
}

} //namespace MX

//-----------------------------------------------------------

static HRESULT CalculateHash(_In_ LPVOID lpKey, _In_ SIZE_T nKeyLen, _Out_ BYTE aSHA1[20])
{
  MX::CBase64Encoder cEncoder;
  MX::CMessageDigest cDigest;
  HRESULT hRes;

  if (lpKey == NULL && nKeyLen > 0)
    return E_POINTER;

  //convert key to base64
  hRes = cEncoder.Begin(cEncoder.GetRequiredSpace(nKeyLen));
  if (SUCCEEDED(hRes))
  {
    hRes = cEncoder.Process(lpKey, nKeyLen);
    if (SUCCEEDED(hRes))
      hRes = cEncoder.End();
  }
  if (FAILED(hRes))
    return hRes;

  //hash the base64 output plus the guid
  hRes = cDigest.BeginDigest(MX::CMessageDigest::AlgorithmSHA1);
  if (SUCCEEDED(hRes))
  {
    hRes = cDigest.DigestStream(cEncoder.GetBuffer(), cEncoder.GetOutputLength());
    if (SUCCEEDED(hRes))
    {
      hRes = cDigest.DigestStream(szGuidA, MX::StrLenA(szGuidA));
      if (SUCCEEDED(hRes))
        hRes = cDigest.EndDigest();
    }
  }
  if (FAILED(hRes))
    return hRes;

  //store the hash
  ::MxMemCopy(aSHA1, cDigest.GetResult(), 20);

  //done
  return S_OK;
}
