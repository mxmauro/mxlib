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
#include "..\..\..\..\Include\JsLib\Plugins\JsonWebTokenPlugin.h"
#include "..\..\..\..\Include\Strings\Utf8.h"
#include "..\..\..\..\Include\DateTime\DateTime.h"
#include "..\..\..\..\Include\Crypto\Base64.h"
#include "..\..\..\..\Include\Crypto\DigestAlgorithmSHAx.h"

//-----------------------------------------------------------

static VOID GetOptionalString(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nIdx,
                              _Out_ MX::CStringA &cStrDestA);
static VOID GetOptionalTimestamp(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nIdx,
                                 _Out_ MX::CDateTime &cDt, _In_opt_ MX::CDateTime *lpBaseDt);
static HRESULT EncodeAndConcatBuffer(_Inout_ MX::CStringA &cStrEncodedDataA, _In_ LPVOID lpBuffer,
                                     _In_ SIZE_T nBufferLen);
static HRESULT DecodeBase64Buffer(_In_ MX::CBase64Decoder &cBase64Dec, _In_ LPCSTR szEncodedA, _In_ SIZE_T nEncodedLen);
static MX::CDigestAlgorithmSecureHash::eAlgorithm GetHsAlgorithm(_In_z_ LPCSTR szAlgorithmA);

//-----------------------------------------------------------

namespace MX {

CJsonWebTokenPlugin::CJsonWebTokenPlugin(_In_ DukTape::duk_context *lpCtx) : CJsObjectBase(lpCtx)
{
  return;
}

CJsonWebTokenPlugin::~CJsonWebTokenPlugin()
{
  return;
}

DukTape::duk_ret_t CJsonWebTokenPlugin::Create(_In_ DukTape::duk_context *lpCtx)
{
  CDigestAlgorithmSecureHash::eAlgorithm nHsAlgorithm;
  CDateTime cDtNotBefore, cDtExpiresAfter, cDtIssuedAt;
  CStringA cStrAudienceA, cStrSubjectA, cStrIssuerA, cStrJwtIdA, cStrEncodedDataA;
  CSecureStringA cStrKeyA;
  LPCSTR sA;
  LONGLONG nTemp;
  CDigestAlgorithmSecureHash cHmacHash;
  HRESULT hRes;

  //check if the payload is an object
  if (DukTape::duk_check_type_mask(lpCtx, 0, DUK_TYPE_MASK_OBJECT) == 0)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);

  //check if the secret/private key is a string or an object
  if (DukTape::duk_is_string(lpCtx, 1) != 0)
  {
    sA = DukTape::duk_get_string(lpCtx, 1);
    if (sA == NULL || *sA == 0)
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
    if (cStrKeyA.Copy(sA) == FALSE)
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
  }
  else if (DukTape::duk_is_buffer_data(lpCtx, 1) != 0)
  {
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_Unsupported);
  }
  else
  {
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  }

  //get options
  if (DukTape::duk_check_type_mask(lpCtx, 2, DUK_TYPE_MASK_OBJECT) == 0)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);

  //algorithm
  DukTape::duk_get_prop_string(lpCtx, 2, "algorithm");
  sA = DukTape::duk_get_string(lpCtx, -1);
  if (sA == NULL || *sA == 0)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  nHsAlgorithm = GetHsAlgorithm(sA);
  if (nHsAlgorithm == (CDigestAlgorithmSecureHash::eAlgorithm)-1)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_Unsupported);
  DukTape::duk_pop(lpCtx);

  //issued at
  DukTape::duk_get_prop_string(lpCtx, 2, "issuedAt");
  GetOptionalTimestamp(lpCtx, -1, cDtIssuedAt, NULL);
  DukTape::duk_pop(lpCtx);
  if (cDtIssuedAt.GetTicks() == 0)
  {
    //set default issued date
    hRes = cDtIssuedAt.SetFromNow(FALSE);
    if (FAILED(hRes))
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  }

  //notBefore
  DukTape::duk_get_prop_string(lpCtx, 2, "notBefore");
  GetOptionalTimestamp(lpCtx, -1, cDtNotBefore, &cDtIssuedAt);
  DukTape::duk_pop(lpCtx);

  //expiresAfter
  DukTape::duk_get_prop_string(lpCtx, 2, "expiresAfter");
  GetOptionalTimestamp(lpCtx, -1, cDtExpiresAfter, &cDtIssuedAt);
  DukTape::duk_pop(lpCtx);

  //audience
  DukTape::duk_get_prop_string(lpCtx, 2, "audience");
  GetOptionalString(lpCtx, -1, cStrAudienceA);
  DukTape::duk_pop(lpCtx);

  //subject
  DukTape::duk_get_prop_string(lpCtx, 2, "subject");
  GetOptionalString(lpCtx, -1, cStrSubjectA);
  DukTape::duk_pop(lpCtx);

  //issuer
  DukTape::duk_get_prop_string(lpCtx, 2, "issuer");
  GetOptionalString(lpCtx, -1, cStrIssuerA);
  DukTape::duk_pop(lpCtx);

  //jwtid
  DukTape::duk_get_prop_string(lpCtx, 2, "jwtid");
  GetOptionalString(lpCtx, -1, cStrJwtIdA);
  DukTape::duk_pop(lpCtx);

  //create header
  DukTape::duk_push_object(lpCtx);

  //add algorithm
  switch (nHsAlgorithm)
  {
    case CDigestAlgorithmSecureHash::AlgorithmSHA256:
      DukTape::duk_push_sprintf(lpCtx, "HS256");
      break;
    case CDigestAlgorithmSecureHash::AlgorithmSHA384:
      DukTape::duk_push_sprintf(lpCtx, "HS384");
      break;
    case CDigestAlgorithmSecureHash::AlgorithmSHA512:
      DukTape::duk_push_sprintf(lpCtx, "HS512");
      break;
  }
  DukTape::duk_put_prop_string(lpCtx, -2, "alg");

  //add type
  DukTape::duk_push_string(lpCtx, "JWT");
  DukTape::duk_put_prop_string(lpCtx, -2, "typ");

  //convert to JSON
  DukTape::duk_json_encode(lpCtx, -1);

  //create payload
  DukTape::duk_push_object(lpCtx);

  //copy object
  DukTape::duk_enum(lpCtx, 0, 0);
  while (DukTape::duk_next(lpCtx, -1, 1) != 0)
  {
    sA = DukTape::duk_get_string(lpCtx, -2);
    DukTape::duk_put_prop_string(lpCtx, -4, sA); //[ payload-obj enumerator key value ]
    DukTape::duk_pop(lpCtx); //the value was removed so just pop the key
  }
  DukTape::duk_pop(lpCtx); //pop enumerator

  //add custom fields if present
  if (cStrAudienceA.IsEmpty() == FALSE)
  {
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrAudienceA, cStrAudienceA.GetLength());
    DukTape::duk_put_prop_string(lpCtx, -2, "aud");
  }
  if (cStrSubjectA.IsEmpty() == FALSE)
  {
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrSubjectA, cStrSubjectA.GetLength());
    DukTape::duk_put_prop_string(lpCtx, -2, "sub");
  }
  if (cStrIssuerA.IsEmpty() == FALSE)
  {
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrIssuerA, cStrIssuerA.GetLength());
    DukTape::duk_put_prop_string(lpCtx, -2, "iss");
  }
  if (cStrJwtIdA.IsEmpty() == FALSE)
  {
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrJwtIdA, cStrJwtIdA.GetLength());
    DukTape::duk_put_prop_string(lpCtx, -2, "jwtid");
  }
  if (cDtNotBefore.GetTicks() != 0)
  {
    hRes = cDtNotBefore.GetUnixTime(&nTemp);
    if (FAILED(hRes))
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
    DukTape::duk_push_number(lpCtx, (double)nTemp);
    DukTape::duk_put_prop_string(lpCtx, -2, "nbf");
  }
  if (cDtExpiresAfter.GetTicks() != 0)
  {
    hRes = cDtExpiresAfter.GetUnixTime(&nTemp);
    if (FAILED(hRes))
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
    DukTape::duk_push_number(lpCtx, (double)nTemp);
    DukTape::duk_put_prop_string(lpCtx, -2, "exp");
  }
  hRes = cDtIssuedAt.GetUnixTime(&nTemp);
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  DukTape::duk_push_number(lpCtx, (double)nTemp);
  DukTape::duk_put_prop_string(lpCtx, -2, "iat");

  //convert to JSON
  DukTape::duk_json_encode(lpCtx, -1);

  //encode header
  sA = DukTape::duk_require_string(lpCtx, -2);
  hRes = EncodeAndConcatBuffer(cStrEncodedDataA, (LPVOID)sA, StrLenA(sA));
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);

  //add separator
  if (cStrEncodedDataA.ConcatN(".", 1) == FALSE)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);

  //encode payload
  sA = DukTape::duk_require_string(lpCtx, -1);
  hRes = EncodeAndConcatBuffer(cStrEncodedDataA, (LPVOID)sA, StrLenA(sA));
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);

  //calculate the hash of the encoded data
  hRes = cHmacHash.BeginDigest(nHsAlgorithm, (LPCSTR)cStrKeyA, cStrKeyA.GetLength());
  if (SUCCEEDED(hRes))
    hRes = cHmacHash.DigestStream((LPCSTR)cStrEncodedDataA, cStrEncodedDataA.GetLength());
  if (SUCCEEDED(hRes))
    hRes = cHmacHash.EndDigest();
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);

  //add separator
  if (cStrEncodedDataA.ConcatN(".", 1) == FALSE)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);

  hRes = EncodeAndConcatBuffer(cStrEncodedDataA, cHmacHash.GetResult(), cHmacHash.GetResultSize());
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);

  //done
  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrEncodedDataA, cStrEncodedDataA.GetLength());
  return 1;
}

DukTape::duk_ret_t CJsonWebTokenPlugin::Verify(_In_ DukTape::duk_context *lpCtx)
{
  CDigestAlgorithmSecureHash::eAlgorithm nHsAlgorithm;
  CDateTime cDtNow;
  ULONG nThreshold;
  BOOL bIgnoreTimestamp;
  LONGLONG nNowTimestamp;
  CStringA cStrAudienceA, cStrSubjectA, cStrIssuerA, cStrJwtIdA;
  CSecureStringA cStrKeyA;
  LPCSTR sA;
  SIZE_T nDotOffsets[2];
  CDigestAlgorithmSecureHash cHmacHash;
  CBase64Decoder cBase64Dec;
  HRESULT hRes;

  //check if the payload is a string
  sA = DukTape::duk_get_string(lpCtx, 0);
  if (sA == NULL || *sA == 0)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);

  nDotOffsets[0] = 0;
  while (sA[nDotOffsets[0]] != 0 && sA[nDotOffsets[0]] != '.')
    nDotOffsets[0]++;
  if (nDotOffsets[0] == 0 || sA[nDotOffsets[0]] != '.')
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG); //first dot not found or at the beginning

  nDotOffsets[1] = nDotOffsets[0] + 1;
  while (sA[nDotOffsets[1]] != 0 && sA[nDotOffsets[1]] != '.')
    nDotOffsets[1]++;
  if (nDotOffsets[1] == nDotOffsets[0] + 1 || sA[nDotOffsets[1]] != '.')
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG); //second dot not found or next to the first

  sA += nDotOffsets[1] + 1;
  if (*sA == 0)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG); //second dot at the end
  while (*sA != 0)
  {
    if (*sA == '.')
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG); //a third dot was found
    sA++;
  }

  //check if the secret/private key is a string or an object
  if (DukTape::duk_is_string(lpCtx, 1) != 0)
  {
    sA = DukTape::duk_get_string(lpCtx, 1);
    if (sA == NULL || *sA == 0)
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
    if (cStrKeyA.Copy(sA) == FALSE)
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
  }
  else if (DukTape::duk_is_buffer_data(lpCtx, 1) != 0)
  {
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_Unsupported);
  }
  else
  {
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  }

  //get options
  if (DukTape::duk_check_type_mask(lpCtx, 2, DUK_TYPE_MASK_OBJECT) == 0)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);

  //algorithm
  DukTape::duk_get_prop_string(lpCtx, 2, "algorithm");
  sA = DukTape::duk_get_string(lpCtx, -1);
  if (sA == NULL || *sA == 0)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  nHsAlgorithm = GetHsAlgorithm(sA);
  if (nHsAlgorithm == (CDigestAlgorithmSecureHash::eAlgorithm)-1)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_Unsupported);
  DukTape::duk_pop(lpCtx);

  //timestamp
  DukTape::duk_get_prop_string(lpCtx, 2, "timestamp");
  GetOptionalTimestamp(lpCtx, -1, cDtNow, NULL);
  DukTape::duk_pop(lpCtx);
  if (cDtNow.GetTicks() == 0)
  {
    //set default timestamp
    hRes = cDtNow.SetFromNow(FALSE);
    if (FAILED(hRes))
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  }
  hRes = cDtNow.GetUnixTime(&nNowTimestamp);
  if (FAILED(hRes))
    return hRes;

  //threshold
  nThreshold = 0;
  DukTape::duk_get_prop_string(lpCtx, 2, "threshold");
  if (DukTape::duk_is_undefined(lpCtx, -1) == 0 && DukTape::duk_is_null(lpCtx, 1) == 0)
  {
    nThreshold = (ULONG)DukTape::duk_require_uint(lpCtx, -1);
  }
  DukTape::duk_pop(lpCtx);

  //ignore timestamp
  bIgnoreTimestamp = FALSE;
  DukTape::duk_get_prop_string(lpCtx, 2, "ignoreTimestamp");
  if (DukTape::duk_is_undefined(lpCtx, -1) == 0 && DukTape::duk_is_null(lpCtx, 1) == 0)
  {
    if (CJavascriptVM::GetInt(lpCtx, -1) != 0)
      bIgnoreTimestamp = TRUE;
  }
  DukTape::duk_pop(lpCtx);

  //audience
  DukTape::duk_get_prop_string(lpCtx, 2, "audience");
  GetOptionalString(lpCtx, -1, cStrAudienceA);
  DukTape::duk_pop(lpCtx);

  //subject
  DukTape::duk_get_prop_string(lpCtx, 2, "subject");
  GetOptionalString(lpCtx, -1, cStrSubjectA);
  DukTape::duk_pop(lpCtx);

  //issuer
  DukTape::duk_get_prop_string(lpCtx, 2, "issuer");
  GetOptionalString(lpCtx, -1, cStrIssuerA);
  DukTape::duk_pop(lpCtx);

  //jwtid
  DukTape::duk_get_prop_string(lpCtx, 2, "jwtid");
  GetOptionalString(lpCtx, -1, cStrJwtIdA);
  DukTape::duk_pop(lpCtx);

  //verify the payload and signature
  sA = DukTape::duk_get_string(lpCtx, 0);
  hRes = cHmacHash.BeginDigest(nHsAlgorithm, (LPCSTR)cStrKeyA, cStrKeyA.GetLength());
  if (SUCCEEDED(hRes))
    hRes = cHmacHash.DigestStream(sA, nDotOffsets[1]);
  if (SUCCEEDED(hRes))
    hRes = cHmacHash.EndDigest();
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);

  //decode signature
  hRes = DecodeBase64Buffer(cBase64Dec, sA + nDotOffsets[1] + 1, StrLenA(sA) - (nDotOffsets[1] + 1));
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);

  //and verify
  if (cBase64Dec.GetOutputLength() != cHmacHash.GetResultSize() ||
      MemCompare(cBase64Dec.GetBuffer(), cHmacHash.GetResult(), cHmacHash.GetResultSize()) != 0)
  {
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, TRUST_E_BAD_DIGEST);
  }

  //decode header
  hRes = DecodeBase64Buffer(cBase64Dec, sA, nDotOffsets[0]);
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);

  DukTape::duk_push_lstring(lpCtx, (LPCSTR)(cBase64Dec.GetBuffer()), cBase64Dec.GetOutputLength());
  DukTape::duk_json_decode(lpCtx, -1);
  DukTape::duk_check_type_mask(lpCtx, -1, DUK_TYPE_MASK_OBJECT | DUK_TYPE_MASK_THROW);

  //validate type
  DukTape::duk_get_prop_string(lpCtx, -1, "typ");
  sA = DukTape::duk_require_string(lpCtx, -1);
  if (StrCompareA(sA, "JWT", FALSE) != 0)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  DukTape::duk_pop(lpCtx);

  //validate algorithm
  DukTape::duk_get_prop_string(lpCtx, -1, "alg");
  sA = DukTape::duk_require_string(lpCtx, -1);
  switch (nHsAlgorithm)
  {
    case CDigestAlgorithmSecureHash::AlgorithmSHA256:
      if (StrCompareA(sA, "HS256", FALSE) != 0)
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
      break;
    case CDigestAlgorithmSecureHash::AlgorithmSHA384:
      if (StrCompareA(sA, "HS384", FALSE) != 0)
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
      break;
    case CDigestAlgorithmSecureHash::AlgorithmSHA512:
      if (StrCompareA(sA, "HS512", FALSE) != 0)
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
      break;
  }
  DukTape::duk_pop_2(lpCtx);

  //decode payload
  sA = DukTape::duk_get_string(lpCtx, 0);
  hRes = DecodeBase64Buffer(cBase64Dec, sA + nDotOffsets[0] + 1, nDotOffsets[1] - (nDotOffsets[0] + 1));
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);

  DukTape::duk_push_lstring(lpCtx, (LPCSTR)(cBase64Dec.GetBuffer()), cBase64Dec.GetOutputLength());
  DukTape::duk_json_decode(lpCtx, -1);
  DukTape::duk_check_type_mask(lpCtx, -1, DUK_TYPE_MASK_OBJECT | DUK_TYPE_MASK_THROW);

  //validate audience
  if (cStrAudienceA.IsEmpty() == FALSE)
  {
    DukTape::duk_get_prop_string(lpCtx, -1, "audience");
    sA = duk_get_string(lpCtx, -1);
    if (sA == NULL || StrCompareA(sA, (LPCSTR)cStrAudienceA) != 0)
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, TRUST_E_FAIL);
    DukTape::duk_pop(lpCtx);
  }

  //validate subject
  if (cStrSubjectA.IsEmpty() == FALSE)
  {
    DukTape::duk_get_prop_string(lpCtx, -1, "subject");
    sA = duk_get_string(lpCtx, -1);
    if (sA == NULL || StrCompareA(sA, (LPCSTR)cStrSubjectA) != 0)
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, TRUST_E_FAIL);
    DukTape::duk_pop(lpCtx);
  }

  //validate issuer
  if (cStrIssuerA.IsEmpty() == FALSE)
  {
    DukTape::duk_get_prop_string(lpCtx, -1, "issuer");
    sA = duk_get_string(lpCtx, -1);
    if (sA == NULL || StrCompareA(sA, (LPCSTR)cStrIssuerA) != 0)
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, TRUST_E_FAIL);
    DukTape::duk_pop(lpCtx);
  }

  //validate jwtid
  if (cStrJwtIdA.IsEmpty() == FALSE)
  {
    DukTape::duk_get_prop_string(lpCtx, -1, "jwtid");
    sA = duk_get_string(lpCtx, -1);
    if (sA == NULL || StrCompareA(sA, (LPCSTR)cStrJwtIdA) != 0)
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, TRUST_E_FAIL);
    DukTape::duk_pop(lpCtx);
  }

  if (bIgnoreTimestamp == FALSE)
  {
    LONGLONG nTimestamp;

    //validate not before
    DukTape::duk_get_prop_string(lpCtx, -1, "nbf");
    if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
    {
      nTimestamp = (LONGLONG)DukTape::duk_require_number(lpCtx, -1);
      if (nNowTimestamp < nTimestamp - (LONGLONG)(ULONGLONG)nThreshold)
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, TRUST_E_FAIL);
    }
    DukTape::duk_pop(lpCtx);

    //validate expire after
    DukTape::duk_get_prop_string(lpCtx, -1, "exp");
    if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
    {
      nTimestamp = (LONGLONG)DukTape::duk_require_number(lpCtx, -1);
      if (nNowTimestamp >= nTimestamp + (LONGLONG)(ULONGLONG)nThreshold)
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, TRUST_E_FAIL);
    }
    DukTape::duk_pop(lpCtx);
  }

  //done
  return 1;
}

} //namespace MX

//-----------------------------------------------------------

static VOID GetOptionalString(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nIdx,
                              _Out_ MX::CStringA &cStrDestA)
{
  cStrDestA.Empty();
  if (DukTape::duk_is_undefined(lpCtx, nIdx) == 0 && DukTape::duk_is_null(lpCtx, nIdx) == 0)
  {
    LPCSTR sA = DukTape::duk_get_string(lpCtx, nIdx);
    if (sA != NULL)
    {
      if (cStrDestA.Copy(sA) == FALSE)
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
    }
  }
  return;
}

static VOID GetOptionalTimestamp(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nIdx,
                                 _Out_ MX::CDateTime &cDt, _In_opt_ MX::CDateTime *lpBaseDt)
{
  cDt.Clear();
  if (DukTape::duk_is_undefined(lpCtx, nIdx) == 0 && DukTape::duk_is_null(lpCtx, nIdx) == 0)
  {
    MX::CStringA cStrTypeA;
    HRESULT hRes;

    MX::CJavascriptVM::GetObjectType(lpCtx, nIdx, cStrTypeA);
    if (MX::StrCompareA((LPCSTR)cStrTypeA, "Date") == 0)
    {
      SYSTEMTIME sSt;

      MX::CJavascriptVM::GetDate(lpCtx, nIdx, &sSt);
      hRes = cDt.SetFromSystemTime(sSt);
      if (FAILED(hRes))
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
    }
    else if (DukTape::duk_is_string(lpCtx, nIdx) != 0)
    {
      LPCSTR sA = DukTape::duk_require_string(lpCtx, nIdx);

      if (sA != NULL)
      {
        ULONG nOffset = 0;
        BOOL bNegative = FALSE;
        MX::CDateTime::eUnits nUnits = MX::CDateTime::UnitsMilliseconds;

        //get sign
        if (*sA == '-')
        {
          sA++;
          bNegative = TRUE;
        }
        //get offset
        if (*sA < '0' || *sA > '9')
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
        while (*sA >= '0' && *sA <= '9')
        {
          nOffset = nOffset * 10 + (LONG)(*sA - '0');
          if (nOffset > 0x7FFFFFFFUL)
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_ArithmeticOverflow);
          sA++;
        }
        //skip spaces
        while (*sA == ' ')
          sA++;
        //units
        if (*sA != 0)
        {
          if (sA[0] != 0 && sA[1] == 0)
          {
            switch (sA[0])
            {
              case 'y':
              case 'Y':
                nUnits = MX::CDateTime::UnitsYear;
                break;
              case 'w':
              case 'W':
                nUnits = MX::CDateTime::UnitsDay;
                if ((ULONG)nOffset * 7 < (ULONG)nOffset)
                  MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_ArithmeticOverflow);
                nOffset *= 7;
                if (nOffset > 0x7FFFFFFFUL)
                  MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_ArithmeticOverflow);
                break;
              case 'd':
              case 'D':
                nUnits = MX::CDateTime::UnitsDay;
                break;
              case 'h':
              case 'H':
                nUnits = MX::CDateTime::UnitsHours;
                break;
              case 'm':
              case 'M':
                nUnits = MX::CDateTime::UnitsMinutes;
                break;
              case 's':
              case 'S':
                nUnits = MX::CDateTime::UnitsSeconds;
                break;
              default:
                MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
            }
          }
          else
          {
            if (MX::StrNCompareA(sA, "yr", 2, TRUE) == 0 &&
                (sA[2] == 0 || ((sA[2] == 's' || sA[2] == 'S') && sA[3] == 0)))
            {
              nUnits = MX::CDateTime::UnitsYear;
            }
            else if (MX::StrNCompareA(sA, "year", 4, TRUE) == 0 &&
                     (sA[4] == 0 || ((sA[4] == 's' || sA[4] == 'S') && sA[5] == 0)))
            {
              nUnits = MX::CDateTime::UnitsYear;
            }
            else if (MX::StrNCompareA(sA, "week", 4, TRUE) == 0 &&
                     (sA[4] == 0 || ((sA[4] == 's' || sA[4] == 'S') && sA[5] == 0)))
            {
              nUnits = MX::CDateTime::UnitsDay;
              if ((ULONG)nOffset * 7 < (ULONG)nOffset)
                MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_ArithmeticOverflow);
              nOffset *= 7;
              if (nOffset > 0x7FFFFFFFUL)
                MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_ArithmeticOverflow);
            }
            else if (MX::StrNCompareA(sA, "day", 3, TRUE) == 0 &&
                     (sA[3] == 0 || ((sA[3] == 's' || sA[3] == 'S') && sA[4] == 0)))
            {
              nUnits = MX::CDateTime::UnitsDay;
            }
            else if (MX::StrNCompareA(sA, "hr", 2, TRUE) == 0 &&
                     (sA[2] == 0 || ((sA[2] == 's' || sA[2] == 'S') && sA[3] == 0)))
            {
              nUnits = MX::CDateTime::UnitsHours;
            }
            else if (MX::StrNCompareA(sA, "hour", 4, TRUE) == 0 &&
                     (sA[4] == 0 || ((sA[4] == 's' || sA[4] == 'S') && sA[5] == 0)))
            {
              nUnits = MX::CDateTime::UnitsHours;
            }
            else if (MX::StrNCompareA(sA, "min", 3, TRUE) == 0 &&
                     (sA[3] == 0 || ((sA[3] == 's' || sA[3] == 'S') && sA[4] == 0)))
            {
              nUnits = MX::CDateTime::UnitsMinutes;
            }
            else if (MX::StrNCompareA(sA, "minute", 6, TRUE) == 0 &&
                     (sA[6] == 0 || ((sA[6] == 's' || sA[6] == 'S') && sA[7] == 0)))
            {
              nUnits = MX::CDateTime::UnitsMinutes;
            }
            else if (MX::StrNCompareA(sA, "sec", 3, TRUE) == 0 &&
                     (sA[3] == 0 || ((sA[3] == 's' || sA[3] == 'S') && sA[4] == 0)))
            {
              nUnits = MX::CDateTime::UnitsSeconds;
            }
            else if (MX::StrNCompareA(sA, "second", 6, TRUE) == 0 &&
                     (sA[6] == 0 || ((sA[6] == 's' || sA[6] == 'S') && sA[7] == 0)))
            {
              nUnits = MX::CDateTime::UnitsSeconds;
            }
            else if (MX::StrNCompareA(sA, "msec", 4, TRUE) == 0 &&
                     (sA[4] == 0 || ((sA[4] == 's' || sA[4] == 'S') && sA[5] == 0)))
            {
              nUnits = MX::CDateTime::UnitsMilliseconds;
            }
            else if (MX::StrNCompareA(sA, "millisecond", 11, TRUE) == 0 &&
                     (sA[11] == 0 || ((sA[11] == 's' || sA[11] == 'S') && sA[12] == 0)))
            {
              nUnits = MX::CDateTime::UnitsMilliseconds;
            }
            else if (MX::StrCompareA(sA, "ms", TRUE) == 0)
            {
              nUnits = MX::CDateTime::UnitsMilliseconds;
            }
            else
            {
              MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
            }
          }
        }

        if (lpBaseDt != NULL)
        {
          cDt = *lpBaseDt;
        }
        else
        {
          hRes = cDt.SetFromNow(FALSE);
          if (FAILED(hRes))
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
        }
        hRes = (bNegative == FALSE) ? cDt.Add((LONGLONG)nOffset, nUnits)
                                    : cDt.Sub(-((LONGLONG)nOffset), nUnits);
        if (FAILED(hRes))
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
      }
    }
    else if (DukTape::duk_is_number(lpCtx, nIdx) != 0)
    {
      DukTape::duk_int_t nOffset;

      nOffset = DukTape::duk_require_int(lpCtx, nIdx);

      if (lpBaseDt != NULL)
      {
        cDt = *lpBaseDt;
      }
      else
      {
        hRes = cDt.SetFromNow(FALSE);
        if (FAILED(hRes))
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
      }
      hRes = cDt.Add((LONGLONG)nOffset, MX::CDateTime::UnitsSeconds);
      if (FAILED(hRes))
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
    }
    else
    {
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_Unsupported);
    }
  }
  return;
}

static HRESULT EncodeAndConcatBuffer(_Inout_ MX::CStringA &cStrEncodedDataA, _In_ LPVOID lpBuffer,
                                     _In_ SIZE_T nBufferLen)
{
  MX::CBase64Encoder cBase64Enc;
  LPCSTR sA;
  SIZE_T nLen;
  HRESULT hRes;

  hRes = cBase64Enc.Begin();
  if (SUCCEEDED(hRes))
    hRes = cBase64Enc.Process(lpBuffer, nBufferLen);
  if (SUCCEEDED(hRes))
    hRes = cBase64Enc.End();
  if (FAILED(hRes))
    return hRes;

  for (sA = cBase64Enc.GetBuffer(), nLen = cBase64Enc.GetOutputLength(); nLen > 0; sA++, nLen--)
  {
    switch (*sA)
    {
      case '+':
        if (cStrEncodedDataA.ConcatN("-", 1) == FALSE)
          return E_OUTOFMEMORY;
        break;
      case '/':
        if (cStrEncodedDataA.ConcatN("_", 1) == FALSE)
          return E_OUTOFMEMORY;
        break;
      case '=':
        break;
      default:
        if (cStrEncodedDataA.ConcatN(sA, 1) == FALSE)
          return E_OUTOFMEMORY;
        break;
    }
  }
  //done
  return S_OK;
}

static HRESULT DecodeBase64Buffer(_In_ MX::CBase64Decoder &cBase64Dec, _In_ LPCSTR szEncodedA, _In_ SIZE_T nEncodedLen)
{
  HRESULT hRes;

  hRes = cBase64Dec.Begin();
  if (SUCCEEDED(hRes))
  {
    SIZE_T nMissingEqualsCount;

    nMissingEqualsCount = ((nEncodedLen & 3) != 0) ? (4 - (nEncodedLen & 3)) : 0;

    while (SUCCEEDED(hRes) && nEncodedLen > 0)
    {
      if (*szEncodedA == '-')
      {
        hRes = cBase64Dec.Process("+", 1);
        szEncodedA++;
        nEncodedLen--;
      }
      else if (*szEncodedA == '_')
      {
        hRes = cBase64Dec.Process("/", 1);
        szEncodedA++;
        nEncodedLen--;
      }
      else
      {
        LPCSTR szStartA = szEncodedA;
        while (nEncodedLen > 0 && *szEncodedA != '-' && *szEncodedA != '_')
        {
          szEncodedA++;
          nEncodedLen--;
        }
        hRes = cBase64Dec.Process(szStartA, (SIZE_T)(szEncodedA - szStartA));
      }
    }
    if (SUCCEEDED(hRes) && nMissingEqualsCount > 0)
    {
      hRes = cBase64Dec.Process("===", nMissingEqualsCount);
    }
  }
  if (SUCCEEDED(hRes))
    hRes = cBase64Dec.End();
  //done
  return hRes;
}

static MX::CDigestAlgorithmSecureHash::eAlgorithm GetHsAlgorithm(_In_z_ LPCSTR szAlgorithmA)
{
  if (MX::StrCompareA(szAlgorithmA, "HS256", TRUE) == 0)
    return MX::CDigestAlgorithmSecureHash::AlgorithmSHA256;
  if (MX::StrCompareA(szAlgorithmA, "HS384", TRUE) == 0)
    return MX::CDigestAlgorithmSecureHash::AlgorithmSHA384;
  if (MX::StrCompareA(szAlgorithmA, "HS512", TRUE) == 0)
    return MX::CDigestAlgorithmSecureHash::AlgorithmSHA512;
  return (MX::CDigestAlgorithmSecureHash::eAlgorithm)-1;
}
