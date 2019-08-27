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
#include "..\..\include\Http\HtmlEntities.h"

//-----------------------------------------------------------

namespace MX {

CJsHttpServer::CJsHttpServer(_In_ CSockets &cSocketMgr, _In_ CIoCompletionPortThreadPool &cWorkerPool,
                             _In_opt_ CLoggable *lpLogParent) : CBaseMemObj(),
                                                                CHttpServer(cSocketMgr, cWorkerPool, lpLogParent)
{
  cNewRequestObjectCallback = NullCallback();
  cRequestCallback = NullCallback();
  cRequireJsModuleCallback = NullCallback();
  cErrorCallback = NullCallback();
  cJavascriptErrorCallback = NullCallback();
  cWebSocketRequestReceivedCallback = NullCallback();
  //----
  bShowStackTraceOnError = TRUE;
  //----
  _InterlockedExchange(&(sVMs.nMutex), 0);
  //----
  CHttpServer::SetNewRequestObjectCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnNewRequestObject, this));
  CHttpServer::SetRequestCompletedCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnRequestCompleted, this));
  CHttpServer::SetErrorCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnError, this));
  CHttpServer::SetWebSocketRequestReceivedCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnWebSocketRequestReceived,
                                                   this));
  return;
}

CJsHttpServer::~CJsHttpServer()
{
  StopListening();

  //free cached VMs
  {
    CFastLock cLock(&(sVMs.nMutex));
    CJsHttpServerJVM *lpJVM;

    while ((lpJVM = sVMs.aJvmList.PopHead()) != NULL)
    {
      delete lpJVM;
    }
  }
  return;
}

VOID CJsHttpServer::SetNewRequestObjectCallback(_In_ OnNewRequestObjectCallback _cNewRequestObjectCallback)
{
  cNewRequestObjectCallback = _cNewRequestObjectCallback;
  return;
}

VOID CJsHttpServer::SetRequestCallback(_In_ OnRequestCallback _cRequestCallback)
{
  cRequestCallback = _cRequestCallback;
  return;
}

VOID CJsHttpServer::SetRequireJsModuleCallback(_In_ OnRequireJsModuleCallback _cRequireJsModuleCallback)
{
  cRequireJsModuleCallback = _cRequireJsModuleCallback;
  return;
}

VOID CJsHttpServer::SetErrorCallback(_In_ OnErrorCallback _cErrorCallback)
{
  cErrorCallback = _cErrorCallback;
  return;
}

VOID CJsHttpServer::SetJavascriptErrorCallback(_In_ OnJavascriptErrorCallback _cJavascriptErrorCallback)
{
  cJavascriptErrorCallback = _cJavascriptErrorCallback;
  return;
}

VOID CJsHttpServer::SetWebSocketRequestReceivedCallback(_In_ OnWebSocketRequestReceivedCallback
                                                        _cWebSocketRequestReceivedCallback)
{
  cWebSocketRequestReceivedCallback = _cWebSocketRequestReceivedCallback;
  return;
}

VOID CJsHttpServer::OnRequestCompleted(_In_ MX::CHttpServer *lpHttp, _In_ CHttpServer::CClientRequest *_lpRequest,
                                       _In_ HANDLE hShutdownEv)
{
  CClientRequest *lpRequest = static_cast<CClientRequest*>(_lpRequest);
  int nOrigThreadPriority = -1000000;
  CStringA cStrCodeA;
  CUrl *lpUrl;
  BOOL bDetached = FALSE;
  HRESULT hRes;

  if (!cRequestCallback)
  {
    hRes = E_FAIL;
on_init_error:
    //restore thread's priority
    if (nOrigThreadPriority > -1000000)
      ::SetThreadPriority(::GetCurrentThread(), nOrigThreadPriority);

    ResetAndDisableClientCache(lpRequest);
    hRes = lpRequest->SendErrorPage(500, hRes);
    lpRequest->End(hRes);
    return;
  }
  //build path
  lpUrl = lpRequest->GetUrl();
  if (lpUrl == NULL) //only accept absolute paths
  {
    hRes = E_NOTIMPL;
    goto on_init_error;
  }
  if ((lpUrl->GetPath())[0] != L'/') //only accept absolute paths
  {
    ResetAndDisableClientCache(lpRequest);
    hRes = lpRequest->SendErrorPage(403, E_INVALIDARG);
    lpRequest->End(hRes);
    return;
  }

  //lower thread's priority
  nOrigThreadPriority = ::GetThreadPriority(::GetCurrentThread());
  ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_LOWEST);

  hRes = cRequestCallback(this, lpRequest, cStrCodeA);
  if (hRes == S_FALSE)
  {
    //restore thread's priority
    ::SetThreadPriority(::GetCurrentThread(), nOrigThreadPriority);

    //the callback owns the request now
    return;
  }

  if (FAILED(hRes))
  {
    lpRequest->DiscardVM();
    goto on_init_error;
  }

  //transform provided code
  if (cStrCodeA.IsEmpty() == FALSE)
  {
    hRes = (lpRequest->lpJVM != NULL) ? TransformJavascriptCode(cStrCodeA) : MX_E_NotReady;
    if (FAILED(hRes))
      goto on_init_error;
    if (cStrCodeA.InsertN("var _code = (function() {  'use strict';  ", 0, 42) == FALSE ||
        cStrCodeA.ConcatN("\r\n});\r\n_code();\r\ndelete _code;\r\n", 32) == FALSE)
    {
      hRes = E_OUTOFMEMORY;
      goto on_init_error;
    }

    //execute JS code
    if (cStrCodeA.IsEmpty() == FALSE)
    {
      try
      {
        lpRequest->lpJVM->Run(cStrCodeA, lpUrl->GetPath());
        bDetached = (lpRequest->sFlags.nDetached != 0) ? TRUE : FALSE;
      }
      catch (CJsHttpServerSystemExit &e)
      {
        if (*(e.GetDescription()) != 0)
        {
          CStringA cStrBodyA;

          hRes = ResetAndDisableClientCache(lpRequest);
          if (SUCCEEDED(hRes))
          {
            if (cStrBodyA.Format("Error: %s", e.GetDescription()) != FALSE)
            {
              hRes = HtmlEntities::ConvertTo(cStrBodyA);
              if (SUCCEEDED(hRes))
              {
                if (cStrBodyA.InsertN("<html><body><pre>", 0, 6 + 6 + 5) != FALSE &&
                    cStrBodyA.ConcatN("</pre></body></html>", 6 + 7 + 7) != FALSE)
                {
                  hRes = lpRequest->SendResponse((LPCSTR)cStrBodyA, cStrBodyA.GetLength());
                }
                else
                {
                  hRes = E_OUTOFMEMORY;
                }
              }
            }
            else
            {
              hRes = E_OUTOFMEMORY;
            }
          }
        }
      }
      catch (CJsWindowsError &e)
      {
        hRes = ResetAndDisableClientCache(lpRequest);
        if (SUCCEEDED(hRes))
        {
          hRes = BuildErrorPage(lpRequest, e.GetHResult(), e.GetDescription(), e.GetFileName(), e.GetLineNumber(),
                                e.GetStackTrace());
        }
      }
      catch (CJsError &e)
      {
        hRes = ResetAndDisableClientCache(lpRequest);
        if (SUCCEEDED(hRes))
        {
          hRes = BuildErrorPage(lpRequest, E_FAIL, e.GetDescription(), e.GetFileName(), e.GetLineNumber(),
                                e.GetStackTrace());
        }
      }
      catch (...)
      {
        hRes = E_FAIL;
      }
    }
    if (FAILED(hRes))
      goto on_init_error;
  }

  //restore thread's priority
  ::SetThreadPriority(::GetCurrentThread(), nOrigThreadPriority);

  //done
  if (bDetached == FALSE)
  {
    lpRequest->End(S_OK);
  }
  return;
}

HRESULT CJsHttpServer::OnRequireJsModule(_In_ DukTape::duk_context *lpCtx,
                                         _In_ CJavascriptVM::CRequireModuleContext *lpReqContext,
                                         _Inout_ CStringA &cStrCodeA)
{
  CClientRequest *lpRequest;
  HRESULT hRes;

  if (!cRequireJsModuleCallback)
    return E_NOTIMPL;
  lpRequest = GetServerRequestFromContext(lpCtx);
  if (lpRequest == NULL)
    return E_FAIL;
  hRes = cRequireJsModuleCallback(this, lpRequest, *CJavascriptVM::FromContext(lpCtx), lpReqContext, cStrCodeA);
  if (SUCCEEDED(hRes))
    hRes = TransformJavascriptCode(cStrCodeA);
  //done
  return hRes;
}

VOID CJsHttpServer::OnError(_In_ CHttpServer *lpHttp, _In_ CHttpServer::CClientRequest *_lpRequest,
                            _In_ HRESULT hrErrorCode)
{
  if (cErrorCallback)
  {
    CClientRequest *lpRequest = static_cast<CClientRequest*>(_lpRequest);

    cErrorCallback(this, lpRequest, hrErrorCode);
  }
  return;
}

HRESULT CJsHttpServer::OnWebSocketRequestReceived(_In_ CHttpServer *lpHttp,
                                                  _In_ CHttpServer::CClientRequest *_lpRequest, _In_ HANDLE hShutdownEv,
                                                  _Inout_ CHttpServer::WEBSOCKET_REQUEST_CALLBACK_DATA &sData)
{
  if (cWebSocketRequestReceivedCallback)
  {
    CClientRequest *lpRequest = static_cast<CClientRequest*>(_lpRequest);

    return cWebSocketRequestReceivedCallback(this, lpRequest, hShutdownEv, sData);
  }
  return MX_E_Unsupported;
}

HRESULT CJsHttpServer::ResetAndDisableClientCache(_In_ CClientRequest *lpRequest)
{
  HRESULT hRes;

  lpRequest->ResetResponse();
  hRes = lpRequest->AddResponseHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  if (SUCCEEDED(hRes))
    hRes = lpRequest->AddResponseHeader("Pragma", "no-cache");
  if (SUCCEEDED(hRes))
    hRes = lpRequest->AddResponseHeader("Expires", "0");
  return hRes;
}

HRESULT CJsHttpServer::BuildErrorPage(_In_ CClientRequest *lpRequest, _In_ HRESULT hr, _In_z_ LPCSTR szDescriptionA,
                                      _In_z_ LPCSTR szFileNameA, _In_ int nLine, _In_z_ LPCSTR szStackTraceA)
{
  CStringA cStrBodyA;
  HRESULT hRes;

  if (cStrBodyA.Format("Error 0x%08X: %s", hr, szDescriptionA) == FALSE)
    return E_OUTOFMEMORY;
  if (bShowStackTraceOnError != FALSE)
  {
    if (cStrBodyA.AppendFormat(" @ %s(%lu)\r\nStack trace:\r\n%s", szFileNameA, nLine, szStackTraceA) == FALSE)
      return E_OUTOFMEMORY;
  }
  hRes = HtmlEntities::ConvertTo(cStrBodyA);
  if (FAILED(hRes))
    return hRes;
  if (cStrBodyA.InsertN("<pre>", 0, 5) == FALSE || cStrBodyA.ConcatN("</pre>", 6) == FALSE)
    return E_OUTOFMEMORY;
  return lpRequest->SendErrorPage(500, hr, (LPCSTR)cStrBodyA);
}

HRESULT CJsHttpServer::AllocAndInitVM(_Out_ CJsHttpServerJVM **lplpJVM, _Out_ BOOL &bIsNew,
                                      _In_ CClientRequest *lpRequest)
{
  TAutoDeletePtr<CJsHttpServerJVM> cJVM;
  CStringA cStrTempA, cStrTempA_2;
  CUrl *lpUrl;
  CHAR szBufA[32];
  TAutoRefCounted<CHttpBodyParserBase> cBodyParser;
  CSockets *lpSckMgr;
  HANDLE hConn;
  SOCKADDR_INET sAddr;
  SIZE_T i, nCount;
  HRESULT hRes;

  bIsNew = FALSE;
  {
    CFastLock cLock(&(sVMs.nMutex));

    cJVM.Attach(sVMs.aJvmList.PopHead());
    if (cJVM)
    {
      if (FAILED(cJVM->Reset()))
      {
        cJVM.Reset();
      }
    }
    if (!cJVM)
    {
      cJVM.Attach(MX_DEBUG_NEW CJsHttpServerJVM());
      if (!cJVM)
        return E_OUTOFMEMORY;
      bIsNew = TRUE;
    }
  }

  //init JVM
  if (bIsNew != FALSE)
  {
    cJVM->SetRequireModuleCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnRequireJsModule, this));
    hRes = cJVM->Initialize();
    __EXIT_ON_ERROR(hRes);

    hRes = cJVM->RegisterException("SystemExit", [](_In_ DukTape::duk_context *lpCtx,
                                   _In_ DukTape::duk_idx_t nExceptionObjectIndex) -> VOID
    {
      throw CJsHttpServerSystemExit(lpCtx, nExceptionObjectIndex);
      return;
    });
    __EXIT_ON_ERROR(hRes);

    //register C++ objects
    hRes = Internals::CFileFieldJsObject::Register(*cJVM.Get());
    __EXIT_ON_ERROR(hRes);
    hRes = Internals::CRawBodyJsObject::Register(*cJVM.Get());
    __EXIT_ON_ERROR(hRes);

    //response functions
    hRes = Internals::JsHttpServer::AddResponseMethods(*cJVM.Get());
    __EXIT_ON_ERROR(hRes);

    //helper functions
    hRes = Internals::JsHttpServer::AddHelpersMethods(*cJVM.Get());
    __EXIT_ON_ERROR(hRes);
  }
  else
  {
    //delete a previously set request object
    hRes = cJVM->RunNativeProtectedAndGetError(0, 0, [](_In_ DukTape::duk_context *lpCtx) -> VOID
    {
      DukTape::duk_push_global_object(lpCtx);
      DukTape::duk_del_prop_string(lpCtx, -1, "request");
      DukTape::duk_pop(lpCtx);
    });
    __EXIT_ON_ERROR(hRes);
  }

  //store request pointer
  hRes = cJVM->RunNativeProtectedAndGetError(0, 0, [lpRequest](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_global_object(lpCtx);
    DukTape::duk_push_pointer(lpCtx, lpRequest);
    DukTape::duk_put_prop_string(lpCtx, -2, INTERNAL_REQUEST_PROPERTY);
    DukTape::duk_pop(lpCtx);
    return;
  });
  __EXIT_ON_ERROR(hRes);

  //create request object
  hRes = cJVM->CreateObject("request");
  __EXIT_ON_ERROR(hRes);
  hRes = cJVM->AddObjectStringProperty("request", "method", lpRequest->GetMethod(),
                                      CJavascriptVM::PropertyFlagEnumerable);
  __EXIT_ON_ERROR(hRes);
  lpUrl = lpRequest->GetUrl();
  //host
  hRes = Utf8_Encode(cStrTempA, lpUrl->GetHost());
  __EXIT_ON_ERROR(hRes);
  hRes = cJVM->AddObjectStringProperty("request", "host", (LPCSTR)cStrTempA, CJavascriptVM::PropertyFlagEnumerable);
  __EXIT_ON_ERROR(hRes);
  //port
  hRes = cJVM->AddObjectNumericProperty("request", "port", (double)(lpUrl->GetPort()),
                                         CJavascriptVM::PropertyFlagEnumerable);
  __EXIT_ON_ERROR(hRes);
  //path
  hRes = Utf8_Encode(cStrTempA, lpUrl->GetPath());
  __EXIT_ON_ERROR(hRes);
  hRes = cJVM->AddObjectStringProperty("request", "path", (LPCSTR)cStrTempA, CJavascriptVM::PropertyFlagEnumerable);
  __EXIT_ON_ERROR(hRes);

  hRes = cJVM->AddObjectNativeFunction("request", "detach", MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnRequestDetach,
                                        this), 0);
  __EXIT_ON_ERROR(hRes);

  //query strings
  hRes = cJVM->CreateObject("request.query");
  __EXIT_ON_ERROR(hRes);
  nCount = lpUrl->GetQueryStringCount();
  for (i = 0; i < nCount; i++)
  {
    if (StrCompareW(lpUrl->GetQueryStringName(i), L"session_id") == 0)
      continue; //skip session id query
    hRes = Utf8_Encode(cStrTempA, lpUrl->GetQueryStringName(i));
    __EXIT_ON_ERROR(hRes);
    hRes = Utf8_Encode(cStrTempA_2, lpUrl->GetQueryStringValue(i));
    __EXIT_ON_ERROR(hRes);
    hRes = cJVM->AddObjectStringProperty("request.query", (LPCSTR)cStrTempA, (LPCSTR)cStrTempA_2,
                                          CJavascriptVM::PropertyFlagEnumerable);
    __EXIT_ON_ERROR(hRes);
  }

  //remote ip and port
  lpSckMgr = lpRequest->GetUnderlyingSocketManager();
  hConn = lpRequest->GetUnderlyingSocketHandle();
  if (hConn == NULL || lpSckMgr == NULL)
    return E_UNEXPECTED;
  hRes = lpSckMgr->GetPeerAddress(hConn, &sAddr);
  __EXIT_ON_ERROR(hRes);
  hRes = cJVM->CreateObject("request.remote");
  __EXIT_ON_ERROR(hRes);
  switch (sAddr.si_family)
  {
    case AF_INET:
      if (cStrTempA.Format("%lu.%lu.%lu.%lu", sAddr.Ipv4.sin_addr.S_un.S_un_b.s_b1,
          sAddr.Ipv4.sin_addr.S_un.S_un_b.s_b2, sAddr.Ipv4.sin_addr.S_un.S_un_b.s_b3,
          sAddr.Ipv4.sin_addr.S_un.S_un_b.s_b4) == FALSE)
      {
        return E_OUTOFMEMORY;
      }
      hRes = cJVM->AddObjectStringProperty("request.remote", "family", "ipv4", CJavascriptVM::PropertyFlagEnumerable);
      __EXIT_ON_ERROR(hRes);
      hRes = cJVM->AddObjectStringProperty("request.remote", "address", (LPCSTR)cStrTempA,
                                            CJavascriptVM::PropertyFlagEnumerable);
      __EXIT_ON_ERROR(hRes);
      hRes = cJVM->AddObjectNumericProperty("request.remote", "port", (double)htons(sAddr.Ipv4.sin_port),
                                             CJavascriptVM::PropertyFlagEnumerable);
      __EXIT_ON_ERROR(hRes);
      break;

    case AF_INET6:
      if (cStrTempA.Format("%4x:%4x:%4x:%4x:%4x:%4x:%4x:%4x", sAddr.Ipv6.sin6_addr.u.Word[0],
          sAddr.Ipv6.sin6_addr.u.Word[1], sAddr.Ipv6.sin6_addr.u.Word[2],
          sAddr.Ipv6.sin6_addr.u.Word[3], sAddr.Ipv6.sin6_addr.u.Word[4],
          sAddr.Ipv6.sin6_addr.u.Word[5], sAddr.Ipv6.sin6_addr.u.Word[6],
          sAddr.Ipv6.sin6_addr.u.Word[7]) == FALSE)
      {
        return E_OUTOFMEMORY;
      }
      for (i = 0; i < 8; i++)
      {
        szBufA[(i << 1)] = '0';
        szBufA[(i << 1) + 1] = ':';
      }
      szBufA[15] = 0; //--> "0:0:0:0:0:0:0:0"
      for (i = 8; i >= 2; i--)
      {
        LPCSTR sA = StrFindA((LPCSTR)cStrTempA, szBufA);
        if (sA != NULL)
        {
          if (i == 8) //special case for all values equal to zero
          {
            if (cStrTempA.Copy("::") == FALSE)
              return E_OUTOFMEMORY;
          }
          else if (sA == (LPCSTR)cStrTempA)
          {
            //the group of zeros are at the beginning
            cStrTempA.Delete(0, (i << 1) - 1);
            if (cStrTempA.InsertN(":", 0, 1) == FALSE)
              return E_OUTOFMEMORY;
          }

          else if (sA[(i << 1) - 1] == 0)
          {
            //the group of zeros are at the end
            cStrTempA.Delete((SIZE_T)(sA - (LPCSTR)cStrTempA), (i << 1) - 1);
            if (cStrTempA.ConcatN(":", 1) == FALSE)
              return E_OUTOFMEMORY;
          }
          else
          {
            //they are in the middle
            cStrTempA.Delete((SIZE_T)(sA - (LPCSTR)cStrTempA), (i << 1) - 1);
          }
          break;
        }
        szBufA[(i << 1) + 1 - 2] = 0;
      }
      hRes = cJVM->AddObjectStringProperty("request.family", "family", "ipv6", CJavascriptVM::PropertyFlagEnumerable);
      __EXIT_ON_ERROR(hRes);
      hRes = cJVM->AddObjectStringProperty("request.remote", "address", (LPCSTR)cStrTempA,
                                            CJavascriptVM::PropertyFlagEnumerable);
      __EXIT_ON_ERROR(hRes);
      hRes = cJVM->AddObjectNumericProperty("request.remote", "port", (double)htons(sAddr.Ipv6.sin6_port),
                                             CJavascriptVM::PropertyFlagEnumerable);
      __EXIT_ON_ERROR(hRes);
      break;

    default:
      hRes = E_NOTIMPL;
      break;
  }

  //add request headers
  hRes = cJVM->CreateObject("request.headers");
  __EXIT_ON_ERROR(hRes);

  nCount = lpRequest->GetRequestHeadersCount();
  for (i = 0; i < nCount; i++)
  {
    CHttpHeaderBase *lpHdr;

    lpHdr = lpRequest->GetRequestHeader(i);
    hRes = lpHdr->Build(cStrTempA, lpRequest->GetBrowser());
    __EXIT_ON_ERROR(hRes);
    hRes = cJVM->AddObjectStringProperty("request.headers", lpHdr->GetHeaderName(), (LPCSTR)cStrTempA,
                                          CJavascriptVM::PropertyFlagEnumerable);
    __EXIT_ON_ERROR(hRes);
  }

  //add cookies
  hRes = cJVM->CreateObject("request.cookies");
  __EXIT_ON_ERROR(hRes);
  nCount = lpRequest->GetRequestCookiesCount();
  for (i = 0; i < nCount; i++)
  {
    CHttpCookie *lpCookie;

    lpCookie = lpRequest->GetRequestCookie(i);
    if (cStrTempA.Copy(lpCookie->GetName()) == FALSE)
      return E_OUTOFMEMORY;
    for (LPSTR sA = (LPSTR)cStrTempA; *sA != 0; sA++)
    {
      if (*sA == '.')
        *sA = '_';
    }
    hRes = cJVM->HasObjectProperty("request.cookies", (LPCSTR)cStrTempA);
    if (hRes == S_FALSE)
    {
      //don't add duplicates
      hRes = cJVM->AddObjectStringProperty("request.cookies", (LPCSTR)cStrTempA, lpCookie->GetValue(),
                                            CJavascriptVM::PropertyFlagEnumerable);
    }
    __EXIT_ON_ERROR(hRes);
  }

  //add body
  hRes = cJVM->CreateObject("request.post");
  __EXIT_ON_ERROR(hRes);
  hRes = cJVM->CreateObject("request.files");
  __EXIT_ON_ERROR(hRes);
  cBodyParser.Attach(lpRequest->GetRequestBodyParser());
  if (cBodyParser)
  {
    if (StrCompareA(cBodyParser->GetType(), "default") == 0)
    {
      CHttpBodyParserDefault *lpParser = (CHttpBodyParserDefault*)(cBodyParser.Get());
      Internals::CRawBodyJsObject *lpJsObj;

      hRes = lpParser->ToString(cStrTempA);
      __EXIT_ON_ERROR(hRes);
      lpJsObj = MX_DEBUG_NEW Internals::CRawBodyJsObject(*cJVM);
      if (!lpJsObj)
        return E_OUTOFMEMORY;
      lpJsObj->Initialize(lpParser);
      hRes = cJVM->AddObjectJsObjectProperty("request", "rawbody", lpJsObj, CJavascriptVM::PropertyFlagEnumerable);
      lpJsObj->Release();
      __EXIT_ON_ERROR(hRes);
    }
    else if (StrCompareA(cBodyParser->GetType(), "form") == 0)
    {
      CHttpBodyParserFormBase *lpParser = (CHttpBodyParserFormBase*)(cBodyParser.Get());

      //add post fields
      nCount = lpParser->GetFieldsCount();
      for (i = 0; i < nCount; i++)
      {
        hRes = InsertPostField(*cJVM, lpParser->GetField(i), "request.post");
        __EXIT_ON_ERROR(hRes);
      }
      //add files fields
      nCount = lpParser->GetFileFieldsCount();
      for (i = 0; i < nCount; i++)
      {
        hRes = InsertPostFileField(*cJVM, lpParser->GetFileField(i), "request.files");
        __EXIT_ON_ERROR(hRes);
      }
    }
  }

  //done
  *lplpJVM = cJVM.Detach();
  return S_OK;
}

VOID CJsHttpServer::FreeVM(_In_ CJsHttpServerJVM *lpJVM)
{
  CFastLock cLock(&(sVMs.nMutex));

  sVMs.aJvmList.PushTail(lpJVM);
  return;
}

} //namespace MX
