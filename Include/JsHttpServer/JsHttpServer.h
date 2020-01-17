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
#ifndef _MX_JS_HTTP_SERVER_H
#define _MX_JS_HTTP_SERVER_H

#include "..\Defines.h"
#include "..\Http\HttpServer.h"
#include "..\JsLib\JavascriptVM.h"

//-----------------------------------------------------------

namespace MX {

class CJsHttpServer : public virtual CBaseMemObj, public CHttpServer
{
public:
  class CClientRequest;
  class CRequireModuleContext;
  class CJsRequestRequireModuleContext;

  typedef Callback<HRESULT (_Out_ CClientRequest **lplpRequest)> OnNewRequestObjectCallback;

  typedef Callback<HRESULT (_In_ CJsHttpServer *lpHttp, _In_ CClientRequest *lpRequest,
                            _Out_ CStringA &cStrCodeA)> OnRequestCallback;

  typedef Callback<HRESULT (_In_ CJsHttpServer *lpHttp, _In_ CClientRequest *lpRequest,
                            _In_ CJavascriptVM &cJvm, _Inout_ CJavascriptVM::CRequireModuleContext *lpReqContext,
                            _Inout_ CStringA &cStrCodeA)> OnRequireJsModuleCallback;

  typedef Callback<VOID (_In_ CJsHttpServer *lpHttp, _In_ CClientRequest *lpRequest,
                         _In_ HRESULT hrErrorCode)> OnErrorCallback;

  typedef Callback<VOID (_In_ CJsHttpServer *lpHttp, _In_ CClientRequest *lpRequest, _In_ CJavascriptVM &cJvm,
                         _In_ HRESULT hRunErrorCode)> OnJavascriptErrorCallback;

  typedef Callback<HRESULT (_In_ CJsHttpServer *lpHttp, _In_ CClientRequest *lpRequest,
                            _Inout_ CHttpServer::WEBSOCKET_REQUEST_CALLBACK_DATA &sData)>
                            OnWebSocketRequestReceivedCallback;

  //--------

public:
  CJsHttpServer(_In_ CSockets &cSocketMgr, _In_ CIoCompletionPortThreadPool &cWorkerPool,
                _In_opt_ CLoggable *lpLogParent = NULL);
  ~CJsHttpServer();

  VOID SetNewRequestObjectCallback(_In_ OnNewRequestObjectCallback cNewRequestObjectCallback);
  VOID SetRequestCallback(_In_ OnRequestCallback cRequestCallback);
  VOID SetRequireJsModuleCallback(_In_ OnRequireJsModuleCallback cRequireJsModuleCallback);
  VOID SetErrorCallback(_In_ OnErrorCallback cErrorCallback);
  VOID SetJavascriptErrorCallback(_In_ OnJavascriptErrorCallback cJavascriptErrorCallback);
  VOID SetWebSocketRequestReceivedCallback(_In_ OnWebSocketRequestReceivedCallback cWebSocketRequestReceivedCallback);

  static CClientRequest* GetServerRequestFromContext(_In_ DukTape::duk_context *lpCtx);

protected:
  virtual HRESULT OnNewRequestObject(_Out_ CHttpServer::CClientRequest **lplpRequest);

private:
  class CJsHttpServerJVM : public CJavascriptVM, public TLnkLstNode<CJsHttpServerJVM>
  {
  };

public:
  class CClientRequest : public CHttpServer::CClientRequest
  {
  protected:
    CClientRequest();
  public:
    ~CClientRequest();

    VOID Detach();

    HRESULT OnSetup();
    BOOL OnCleanup();

    HRESULT AttachJVM();
    VOID DiscardVM();

    CJavascriptVM* GetVM(_Out_opt_ LPBOOL lpbIsNew = NULL) const
      {
      if (lpbIsNew != NULL)
        *lpbIsNew = (sFlags.nIsNew) != 0 ? TRUE : FALSE;
      return lpJVM;
      };

  public:
    TArrayListWithDelete<CStringA*> cOutputBuffersList;

  private:
    VOID FreeJVM();

  private:
    friend class CJsHttpServer;

    CJsHttpServer *lpJsHttpServer;
    CJsHttpServerJVM *lpJVM;
    struct {
      int nIsNew : 1;
      int nDetached : 1;
      int nDiscardedVM : 1;
    } sFlags;
  };

private:
  using CHttpServer::SetNewRequestObjectCallback;
  using CHttpServer::SetRequestHeadersReceivedCallback;
  using CHttpServer::SetRequestCompletedCallback;
  using CHttpServer::SetWebSocketRequestReceivedCallback;
  using CHttpServer::SetErrorCallback;

  VOID OnRequestCompleted(_In_ MX::CHttpServer *lpHttp, _In_ CHttpServer::CClientRequest *lpRequest);
  HRESULT OnRequireJsModule(_In_ DukTape::duk_context *lpCtx, _In_ CJavascriptVM::CRequireModuleContext *lpReqContext,
                            _Inout_ CStringA &cStrCodeA);
  VOID OnError(_In_ CHttpServer *lpHttp, _In_ CHttpServer::CClientRequest *lpRequest, _In_ HRESULT hrErrorCode);
  HRESULT OnWebSocketRequestReceived(_In_ CHttpServer *lpHttp, _In_ CHttpServer::CClientRequest *lpRequest,
                                     _Inout_ CHttpServer::WEBSOCKET_REQUEST_CALLBACK_DATA &sData);

  HRESULT TransformJavascriptCode(_Inout_ MX::CStringA &cStrCodeA);
  BOOL TransformJavascriptCode_ConvertToPrint(_Inout_ MX::CStringA &cStrCodeA, _Inout_ SIZE_T nNonCodeBlockStart,
                                              _Inout_ SIZE_T &nCurrPos);

  HRESULT InsertPostField(_In_ CJavascriptVM &cJvm, _In_ CHttpBodyParserFormBase::CField *lpField,
                          _In_ LPCSTR szBaseObjectNameA);
  HRESULT InsertPostFileField(_In_ CJavascriptVM &cJvm, _In_ CHttpBodyParserFormBase::CFileField *lpFileField,
                              _In_ LPCSTR szBaseObjectNameA);

  DukTape::duk_ret_t OnRequestDetach(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                     _In_z_ LPCSTR szFunctionNameA);

  HRESULT ResetAndDisableClientCache(_In_ CClientRequest *lpRequest);
  HRESULT BuildErrorPage(_In_ CClientRequest *lpRequest, _In_ HRESULT hr, _In_z_ LPCSTR szDescriptionA,
                         _In_z_ LPCSTR szFileNameA, _In_ int nLine, _In_z_ LPCSTR szStackTraceA);

  HRESULT AllocAndInitVM(_Out_ CJsHttpServerJVM **lplpJVM, _Out_ BOOL &bIsNew, _In_ CClientRequest *lpRequest);
  VOID FreeVM(_In_ CJsHttpServerJVM *lpJVM);

private:
  BOOL bShowStackTraceOnError;
  OnNewRequestObjectCallback cNewRequestObjectCallback;
  OnRequestCallback cRequestCallback;
  OnRequireJsModuleCallback cRequireJsModuleCallback;
  OnErrorCallback cErrorCallback;
  OnJavascriptErrorCallback cJavascriptErrorCallback;
  OnWebSocketRequestReceivedCallback cWebSocketRequestReceivedCallback;
  struct {
    LONG volatile nMutex;
    TLnkLst<CJsHttpServerJVM> aJvmList;
  } sVMs;
};

//-----------------------------------------------------------

class CJsHttpServerSystemExit : public CJsError
{
protected:
  CJsHttpServerSystemExit(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nStackIndex);

public:
  CJsHttpServerSystemExit(_In_ const CJsHttpServerSystemExit &obj);
  CJsHttpServerSystemExit& operator=(_In_ const CJsHttpServerSystemExit &obj);

private:
  friend class CJsHttpServer;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JS_HTTP_SERVER_H
