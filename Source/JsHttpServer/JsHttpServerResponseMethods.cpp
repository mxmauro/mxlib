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
#include "JsHttpServerCommon.h"

//-----------------------------------------------------------

static DukTape::duk_ret_t OnResetOutput(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                        _In_z_ LPCSTR szFunctionNameA);
static DukTape::duk_ret_t OnEcho(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                 _In_z_ LPCSTR szFunctionNameA);
static DukTape::duk_ret_t OnSetStatus(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                      _In_z_ LPCSTR szFunctionNameA);
static DukTape::duk_ret_t OnSetCookie(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                      _In_z_ LPCSTR szFunctionNameA);
static DukTape::duk_ret_t OnSetHeader(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                      _In_z_ LPCSTR szFunctionNameA);
static DukTape::duk_ret_t OnRemoveHeader(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                         _In_z_ LPCSTR szFunctionNameA);
static DukTape::duk_ret_t OnObStart(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                    _In_z_ LPCSTR szFunctionNameA);
static DukTape::duk_ret_t OnObEnd(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                  _In_z_ LPCSTR szFunctionNameA);
static DukTape::duk_ret_t OnObGetContents(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                          _In_z_ LPCSTR szFunctionNameA);

//-----------------------------------------------------------

namespace MX {

namespace Internals {

namespace JsHttpServer {

HRESULT AddResponseMethods(_In_ CJavascriptVM &cJvm)
{
  HRESULT hRes;

  hRes = cJvm.AddNativeFunction("echo", MX_BIND_CALLBACK(&OnEcho), 1);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddNativeFunction("setStatus", MX_BIND_CALLBACK(&OnSetStatus), MX_JS_VARARGS);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddNativeFunction("setCookie", MX_BIND_CALLBACK(&OnSetCookie), MX_JS_VARARGS);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddNativeFunction("setHeader", MX_BIND_CALLBACK(&OnSetHeader), MX_JS_VARARGS);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddNativeFunction("removeHeader", MX_BIND_CALLBACK(&OnRemoveHeader), 1);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddNativeFunction("resetOutput", MX_BIND_CALLBACK(&OnResetOutput), 0);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddNativeFunction("obStart", MX_BIND_CALLBACK(&OnObStart), 0);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddNativeFunction("obEnd", MX_BIND_CALLBACK(&OnObEnd), MX_JS_VARARGS);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddNativeFunction("obGetContents", MX_BIND_CALLBACK(&OnObGetContents), 0);
  __EXIT_ON_ERROR(hRes);
  //done
  return S_OK;
}

} //namespace JsHttpServer

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

static DukTape::duk_ret_t OnResetOutput(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                        _In_z_ LPCSTR szFunctionNameA)
{
  MX::CJsHttpServer::CClientRequest *lpRequest = MX::CJsHttpServer::GetServerRequestFromContext(lpCtx);

  lpRequest->ResetResponse();
  return 0;
}

static DukTape::duk_ret_t OnEcho(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                 _In_z_ LPCSTR szFunctionNameA)
{
  MX::CJsHttpServer::CClientRequest *lpRequest = MX::CJsHttpServer::GetServerRequestFromContext(lpCtx);
  LPCSTR szBufA;
  MX::CStringA *lpStrA;
  SIZE_T nIdx;
  HRESULT hRes;

  szBufA = DukTape::duk_to_string(lpCtx, 0); //convert to string if other type
  if (*szBufA != 0)
  {
    nIdx = lpRequest->cOutputBuffersList.GetCount();
    if (nIdx > 0)
    {
      lpStrA = lpRequest->cOutputBuffersList.GetElementAt(nIdx - 1);
      hRes = (lpStrA->ConcatN(szBufA, MX::StrLenA(szBufA)) != FALSE) ? S_OK : E_OUTOFMEMORY;
    }
    else
    {
      hRes = lpRequest->SendResponse(szBufA, MX::StrLenA(szBufA));
    }
    if (FAILED(hRes))
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  }
  return 0;
}

static DukTape::duk_ret_t OnSetStatus(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                      _In_z_ LPCSTR szFunctionNameA)
{
  MX::CJsHttpServer::CClientRequest *lpRequest = MX::CJsHttpServer::GetServerRequestFromContext(lpCtx);
  DukTape::duk_idx_t nParamsCount;
  LONG nStatus;
  LPCSTR szReasonA;
  HRESULT hRes;

  //get parameters
  nParamsCount = DukTape::duk_get_top(lpCtx);
  if (nParamsCount < 1 || nParamsCount > 2)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  nStatus = DukTape::duk_require_int(lpCtx, 0);
  szReasonA = NULL;
  if (nParamsCount > 1)
    szReasonA = DukTape::duk_require_string(lpCtx, 1);
  //set status
  hRes = lpRequest->SetResponseStatus(nStatus, szReasonA);
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  return 0;
}

static DukTape::duk_ret_t OnSetCookie(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                      _In_z_ LPCSTR szFunctionNameA)
{
  MX::CJsHttpServer::CClientRequest *lpRequest = MX::CJsHttpServer::GetServerRequestFromContext(lpCtx);
  MX::TAutoRefCounted<MX::CHttpCookie> cCookie;
  MX::CStringW cStrTempW;
  DukTape::duk_idx_t nParamsCount;
  LPCSTR szNameA, szValueA, szPathA, szDomainA;
  DukTape::duk_int_t nExpire;
  BOOL bHasExpireDt, bIsSecure, bIsHttpOnly;
  HRESULT hRes;

  nParamsCount = DukTape::duk_get_top(lpCtx);
  if (nParamsCount == 0 || nParamsCount > 7)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  //get parameters
  szValueA = szPathA = szDomainA = NULL;
  nExpire = 0;
  bHasExpireDt = bIsSecure = bIsHttpOnly = FALSE;
  szNameA = DukTape::duk_require_string(lpCtx, 0);
  if (nParamsCount > 1 && duk_is_null_or_undefined(lpCtx, 1) == 0)
    szValueA = DukTape::duk_require_string(lpCtx, 1);
  if (nParamsCount > 2 && duk_is_null_or_undefined(lpCtx, 2) == 0)
  {
    nExpire = DukTape::duk_require_int(lpCtx, 2);
    bHasExpireDt = TRUE;
  }
  if (nParamsCount > 3 && duk_is_null_or_undefined(lpCtx, 3) == 0)
    szPathA = DukTape::duk_require_string(lpCtx, 3);
  if (nParamsCount > 4 && duk_is_null_or_undefined(lpCtx, 4) == 0)
    szDomainA = DukTape::duk_require_string(lpCtx, 4);
  if (nParamsCount > 5 && duk_is_null_or_undefined(lpCtx, 5) == 0)
    bIsSecure = (MX::CJavascriptVM::GetInt(lpCtx, 5) != 0) ? TRUE : FALSE;
  if (nParamsCount > 6 && duk_is_null_or_undefined(lpCtx, 6) == 0)
    bIsHttpOnly = (MX::CJavascriptVM::GetInt(lpCtx, 6) != 0) ? TRUE : FALSE;

  //setup cookie
  cCookie.Attach(MX_DEBUG_NEW MX::CHttpCookie());
  if (!cCookie)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
  hRes = cCookie->SetName(szNameA);
  if (SUCCEEDED(hRes) && szValueA != NULL)
  {
    hRes = cCookie->SetValue(szValueA);
  }
  if (SUCCEEDED(hRes) && szDomainA != NULL)
  {
    hRes = MX::Utf8_Decode(cStrTempW, szDomainA);
    if (SUCCEEDED(hRes))
      hRes = cCookie->SetDomain((LPCWSTR)cStrTempW);
  }
  if (SUCCEEDED(hRes) && szPathA != NULL)
  {
    hRes = cCookie->SetPath(szPathA);
  }
  if (SUCCEEDED(hRes) && bHasExpireDt != FALSE)
  {
    MX::CDateTime cExpireDt;

    hRes = cExpireDt.SetFromNow(FALSE);
    if (SUCCEEDED(hRes))
    {
      hRes = cExpireDt.Add((int)nExpire, MX::CDateTime::eUnits::Seconds);
      if (SUCCEEDED(hRes))
        cCookie->SetExpireDate(&cExpireDt);
    }
  }

  if (SUCCEEDED(hRes))
  {
    cCookie->SetSecureFlag(bIsSecure);
    cCookie->SetHttpOnlyFlag(bIsHttpOnly);
    hRes = lpRequest->AddResponseCookie(cCookie);
  }
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);

  //done
  return 0;
}

static DukTape::duk_ret_t OnSetHeader(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                      _In_z_ LPCSTR szFunctionNameA)
{
  MX::CJsHttpServer::CClientRequest *lpRequest = MX::CJsHttpServer::GetServerRequestFromContext(lpCtx);
  DukTape::duk_idx_t nParamsCount;
  LPCSTR szNameA, szValueA;
  BOOL bReplaceExisting;
  HRESULT hRes;

  nParamsCount = DukTape::duk_get_top(lpCtx);
  if (nParamsCount < 2 || nParamsCount > 3)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  //get parameters
  bReplaceExisting = TRUE;
  szNameA = DukTape::duk_require_string(lpCtx, 0);
  szValueA = DukTape::duk_require_string(lpCtx, 1);
  if (nParamsCount > 2 && duk_is_null_or_undefined(lpCtx, 2) == 0)
    bReplaceExisting = (MX::CJavascriptVM::GetInt(lpCtx, 2) != 0) ? TRUE : FALSE;
  hRes = lpRequest->AddResponseHeader(szNameA, szValueA, MX::StrLenA(szValueA), NULL, bReplaceExisting);
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  return 0;
}

static DukTape::duk_ret_t OnRemoveHeader(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                         _In_z_ LPCSTR szFunctionNameA)
{
  MX::CJsHttpServer::CClientRequest *lpRequest = MX::CJsHttpServer::GetServerRequestFromContext(lpCtx);
  LPCSTR szNameA;

  szNameA = DukTape::duk_require_string(lpCtx, 0);
  lpRequest->RemoveResponseHeader(szNameA);
  return 0;
}

static DukTape::duk_ret_t OnObStart(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                    _In_z_ LPCSTR szFunctionNameA)
{
  MX::CJsHttpServer::CClientRequest *lpRequest = MX::CJsHttpServer::GetServerRequestFromContext(lpCtx);
  MX::CStringA *lpStrA;

  lpStrA = MX_DEBUG_NEW MX::CStringA();
  if (lpStrA == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
  if (lpRequest->cOutputBuffersList.AddElement(lpStrA) == FALSE)
  {
    delete lpStrA;
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
  }
  return 0;
}

static DukTape::duk_ret_t OnObEnd(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                  _In_z_ LPCSTR szFunctionNameA)
{
  MX::CJsHttpServer::CClientRequest *lpRequest = MX::CJsHttpServer::GetServerRequestFromContext(lpCtx);
  DukTape::duk_idx_t nParamsCount;
  MX::CStringA *lpStrA;
  MX::CJavascriptVM *lpJVM;
  BOOL bDiscard;
  SIZE_T nIdx;
  HRESULT hRes;

  bDiscard = FALSE;
  //get parameters
  nParamsCount = DukTape::duk_get_top(lpCtx);
  if (nParamsCount < 0 || nParamsCount > 1)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  if (nParamsCount > 1)
  {
    lpJVM = MX::CJavascriptVM::FromContext(lpCtx);
    if (lpJVM->GetInt(0) != 0)
      bDiscard = TRUE;
  }
  //process output buffer
  nIdx = lpRequest->cOutputBuffersList.GetCount();
  if (nIdx > 0)
  {
    if (bDiscard == FALSE)
    {
      lpStrA = lpRequest->cOutputBuffersList.GetElementAt(nIdx - 1);
      if (lpStrA->IsEmpty() == FALSE)
      {
        hRes = lpRequest->SendResponse((LPCSTR)(*lpStrA), lpStrA->GetLength());
        if (FAILED(hRes))
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
      }
    }
    lpRequest->cOutputBuffersList.RemoveElementAt(nIdx - 1);
  }
  return 0;
}

static DukTape::duk_ret_t OnObGetContents(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                          _In_z_ LPCSTR szFunctionNameA)
{
  MX::CJsHttpServer::CClientRequest *lpRequest = MX::CJsHttpServer::GetServerRequestFromContext(lpCtx);
  MX::CStringA *lpStrA;
  SIZE_T nIdx;

  //process output buffer
  nIdx = lpRequest->cOutputBuffersList.GetCount();
  if (nIdx > 0)
  {
    lpStrA = lpRequest->cOutputBuffersList.GetElementAt(nIdx - 1);
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)(*lpStrA), lpStrA->GetLength());
  }
  else
  {
    DukTape::duk_push_string(lpCtx, "");
  }
  return 1;
}
