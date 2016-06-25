/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2006-2015. All rights reserved.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 *
 * 2. This notice may not be removed or altered from any source distribution.
 *
 * 3. YOU MAY NOT:
 *
 *    a. Modify, translate, adapt, alter, or create derivative works from
 *       this software.
 *    b. Copy (other than one back-up copy), distribute, publicly display,
 *       transmit, sell, rent, lease or otherwise exploit this software.
 *    c. Distribute, sub-license, rent, lease, loan [or grant any third party
 *       access to or use of the software to any third party.
 **/
#include "JsHttpServerCommon.h"

//-----------------------------------------------------------

static DukTape::duk_ret_t OnEcho(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                 __in_z LPCSTR szFunctionNameA);
static DukTape::duk_ret_t OnSetStatus(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                      __in_z LPCSTR szFunctionNameA);
static DukTape::duk_ret_t OnSetCookie(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                      __in_z LPCSTR szFunctionNameA);
static DukTape::duk_ret_t OnSetHeader(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                      __in_z LPCSTR szFunctionNameA);

//-----------------------------------------------------------

namespace MX {

namespace Internals {

namespace JsHttpServer {

HRESULT AddResponseMethods(__in CJavascriptVM &cJvm, __in MX::CHttpServer::CRequest *lpRequest)
{
  HRESULT hRes;

  hRes = cJvm.AddNativeFunction("echo", MX_BIND_CALLBACK(&OnEcho), 1);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddNativeFunction("setStatus", MX_BIND_CALLBACK(&OnSetStatus), MX_JS_VARARGS);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddNativeFunction("setCookie", MX_BIND_CALLBACK(&OnSetCookie), MX_JS_VARARGS);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddNativeFunction("setHeader", MX_BIND_CALLBACK(&OnSetHeader), 2);
  __EXIT_ON_ERROR(hRes);
  //done
  return S_OK;
}

} //namespace JsHttpServer

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

static DukTape::duk_ret_t OnEcho(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                 __in_z LPCSTR szFunctionNameA)
{
  MX::CHttpServer::CRequest *lpRequest = MX::CJsHttpServer::GetServerRequestFromContext(lpCtx);
  LPCSTR szBufA;
  HRESULT hRes;

  szBufA = DukTape::duk_to_string(lpCtx, 0); //convert to string if other type
  if (*szBufA != 0)
  {
    hRes = lpRequest->SendResponse(szBufA, MX::StrLenA(szBufA));
    if (FAILED(hRes))
      MX_JS_THROW_HRESULT_ERROR(lpCtx, hRes);
  }
  return 0;
}

static DukTape::duk_ret_t OnSetStatus(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                      __in_z LPCSTR szFunctionNameA)
{
  MX::CHttpServer::CRequest *lpRequest = MX::CJsHttpServer::GetServerRequestFromContext(lpCtx);
  DukTape::duk_idx_t nParamsCount;
  LONG nStatus;
  LPCSTR szReasonA;
  HRESULT hRes;

  //get parameters
  nParamsCount = DukTape::duk_get_top(lpCtx);
  if (nParamsCount < 1 || nParamsCount > 2)
    MX_JS_THROW_HRESULT_ERROR(lpCtx, E_INVALIDARG);
  nStatus = DukTape::duk_require_int(lpCtx, 0);
  szReasonA = NULL;
  if (nParamsCount > 1)
    szReasonA = DukTape::duk_require_string(lpCtx, 1);
  //set status
  hRes = lpRequest->SetResponseStatus(nStatus, szReasonA);
  if (FAILED(hRes))
    MX_JS_THROW_HRESULT_ERROR(lpCtx, hRes);
  return 0;
}

static DukTape::duk_ret_t OnSetCookie(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                      __in_z LPCSTR szFunctionNameA)
{
  MX::CHttpServer::CRequest *lpRequest = MX::CJsHttpServer::GetServerRequestFromContext(lpCtx);
  MX::CHttpCookie cCookie;
  MX::CStringW cStrTempW;
  DukTape::duk_idx_t nParamsCount;
  LPCSTR szNameA, szValueA, szPathA, szDomainA;
  DukTape::duk_int_t nExpire;
  BOOL bHasExpireDt, bIsSecure, bIsHttpOnly;
  HRESULT hRes;

  nParamsCount = DukTape::duk_get_top(lpCtx);
  if (nParamsCount == 0 || nParamsCount > 7)
    MX_JS_THROW_HRESULT_ERROR(lpCtx, E_INVALIDARG);
  //get parameters
  szValueA = szPathA = szDomainA = NULL;
  nExpire = 0;
  bHasExpireDt = bIsSecure = bIsHttpOnly = FALSE;
  szNameA = DukTape::duk_require_string(lpCtx, 0);
  if (nParamsCount > 1 && DukTape::duk_is_null_or_undefined(lpCtx, 1) == 0)
    szValueA = DukTape::duk_require_string(lpCtx, 1);
  if (nParamsCount > 2 && DukTape::duk_is_null_or_undefined(lpCtx, 2) == 0)
  {
    nExpire = DukTape::duk_require_int(lpCtx, 2);
    bHasExpireDt = TRUE;
  }
  if (nParamsCount > 3 && DukTape::duk_is_null_or_undefined(lpCtx, 3) == 0)
    szPathA = DukTape::duk_require_string(lpCtx, 3);
  if (nParamsCount > 4 && DukTape::duk_is_null_or_undefined(lpCtx, 4) == 0)
    szDomainA = DukTape::duk_require_string(lpCtx, 4);
  if (nParamsCount > 5 && DukTape::duk_is_null_or_undefined(lpCtx, 5) == 0)
    bIsSecure = DukTape::duk_require_boolean(lpCtx, 5) ? TRUE : FALSE;
  if (nParamsCount > 6 && DukTape::duk_is_null_or_undefined(lpCtx, 6) == 0)
    bIsHttpOnly = DukTape::duk_require_boolean(lpCtx, 6) ? TRUE : FALSE;
  //setup cookie
  hRes = cCookie.SetName(szNameA);
  if (SUCCEEDED(hRes) && szValueA != NULL)
    hRes = cCookie.SetValue(szValueA);
  if (SUCCEEDED(hRes) && szDomainA != NULL)
  {
    hRes = MX::Utf8_Decode(cStrTempW, szDomainA);
    if (SUCCEEDED(hRes))
      hRes = cCookie.SetDomain((LPCWSTR)cStrTempW);
  }
  if (SUCCEEDED(hRes) && szPathA != NULL)
    hRes = cCookie.SetPath(szPathA);
  if (SUCCEEDED(hRes) && bHasExpireDt != FALSE)
  {
    MX::CDateTime cExpireDt;

    hRes = cExpireDt.SetFromNow(FALSE);
    if (SUCCEEDED(hRes))
      hRes = cExpireDt.Add((int)nExpire, MX::CDateTime::UnitsSeconds);
    if (SUCCEEDED(hRes))
      cCookie.SetExpireDate(&cExpireDt);
  }
  if (SUCCEEDED(hRes))
  {
    cCookie.SetSecureFlag(bIsSecure);
    cCookie.SetHttpOnlyFlag(bIsHttpOnly);
    hRes = lpRequest->AddResponseCookie(cCookie);
  }
  if (FAILED(hRes))
    MX_JS_THROW_HRESULT_ERROR(lpCtx, hRes);
  return 0;
}

static DukTape::duk_ret_t OnSetHeader(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                      __in_z LPCSTR szFunctionNameA)
{
  MX::CHttpServer::CRequest *lpRequest = MX::CJsHttpServer::GetServerRequestFromContext(lpCtx);
  LPCSTR szNameA, szValueA;
  HRESULT hRes;

  szNameA = DukTape::duk_require_string(lpCtx, 0);
  szValueA = DukTape::duk_require_string(lpCtx, 1);
  hRes = lpRequest->AddResponseHeader(szNameA, szValueA);
  if (FAILED(hRes))
    MX_JS_THROW_HRESULT_ERROR(lpCtx, hRes);
  return 0;
}
