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
#include "..\Http\HttpBodyParserFormBase.h"
#include "..\JsLib\JavascriptVM.h"

//-----------------------------------------------------------

namespace MX {

class CJsHttpServer : public virtual CBaseMemObj, public CHttpServer
{
public:
  class CClientRequest;
  class CRequireModuleContext;
  class CJsRequestRequireModuleContext;

public:
  typedef Callback<HRESULT (_In_ CJsHttpServer *lpHttp, _Out_ CClientRequest **lplpRequest)> OnNewRequestObjectCallback;

  typedef Callback<HRESULT (_In_ CJsHttpServer *lpHttp, _In_ CClientRequest *lpRequest,
                            _Outptr_ _Maybenull_ CHttpBodyParserBase **lplpBodyParser)>
                            OnRequestHeadersReceivedCallback;

  typedef Callback<VOID (_In_ CJsHttpServer *lpHttp, _In_ CClientRequest *lpRequest)> OnRequestCompletedCallback;

  typedef Callback<HRESULT (_In_ CJsHttpServer *lpHttp, _In_ CClientRequest *lpRequest,
                            _In_ CJavascriptVM &cJvm, _Inout_ CJavascriptVM::CRequireModuleContext *lpReqContext,
                            _Inout_ CStringA &cStrCodeA)> OnRequireJsModuleCallback;

  typedef Callback<HRESULT (_In_ CJsHttpServer *lpHttp, _In_ CClientRequest *lpRequest, _In_ int nVersion,
                            _In_opt_ LPCSTR *szProtocolsA, _In_ SIZE_T nProtocolsCount, _Out_ int &nSelectedProtocol,
                            _In_ TArrayList<int> &aSupportedVersions,
                            _Out_ _Maybenull_ CWebSocket **lplpWebSocket)> OnWebSocketRequestReceivedCallback;

  //--------

public:
  CJsHttpServer(_In_ CSockets &cSocketMgr, _In_opt_ CLoggable *lpLogParent = NULL);
  ~CJsHttpServer();

  VOID SetNewRequestObjectCallback(_In_ OnNewRequestObjectCallback cNewRequestObjectCallback);
  VOID SetRequestHeadersReceivedCallback(_In_ OnRequestHeadersReceivedCallback cRequestHeadersReceivedCallback);
  VOID SetRequestCompletedCallback(_In_ OnRequestCompletedCallback cRequestCompletedCallback);
  VOID SetRequireJsModuleCallback(_In_ OnRequireJsModuleCallback cRequireJsModuleCallback);
  VOID SetWebSocketRequestReceivedCallback(_In_ OnWebSocketRequestReceivedCallback cWebSocketRequestReceivedCallback);

  static CClientRequest* GetServerRequestFromContext(_In_ DukTape::duk_context *lpCtx);

  //remove some inherited public methods
  VOID SetNewRequestObjectCallback(_In_ CHttpServer::OnNewRequestObjectCallback cNewRequestObjectCallback) = delete;
  VOID SetRequestHeadersReceivedCallback(_In_ CHttpServer::OnRequestHeadersReceivedCallback
                                         cRequestHeadersReceivedCallback) = delete;
  VOID SetRequestCompletedCallback(_In_ CHttpServer::OnRequestCompletedCallback cRequestCompletedCallback) = delete;
  VOID SetWebSocketRequestReceivedCallback(_In_ CHttpServer::OnWebSocketRequestReceivedCallback
                                           cWebSocketRequestReceivedCallback) = delete;

protected:
  virtual HRESULT OnNewRequestObject(_In_ CHttpServer *lpHttp, _Out_ CHttpServer::CClientRequest **lplpRequest);

private:
  class CJvm : public CJavascriptVM
  {
  public:
    CLnkLstNode cListNode;
  };

private:
  class CJvmManager : public virtual TRefCounted<CBaseMemObj>
  {
  public:
    CJvmManager();
    ~CJvmManager();

    HRESULT AllocAndInitVM(_Out_ CJvm **lplpJVM, _Out_ BOOL &bIsNew,
                           _In_ OnRequireJsModuleCallback cRequireJsModuleCallback, _In_ CClientRequest *lpRequest);
    VOID FreeVM(_In_ CJvm *lpJVM);

  private:
    HRESULT InsertPostField(_In_ CJavascriptVM &cJvm, _In_ CHttpBodyParserFormBase::CField *lpField,
                            _In_ LPCSTR szBaseObjectNameA);
    HRESULT InsertPostFileField(_In_ CJavascriptVM &cJvm, _In_ CHttpBodyParserFormBase::CFileField *lpFileField,
                                _In_ LPCSTR szBaseObjectNameA);

    VOID ParseJsonBody(_In_ DukTape::duk_context *lpCtx, _In_ LPVOID v, _In_opt_z_ LPCSTR szPropNameA,
                       _In_ DukTape::duk_uarridx_t nArrayIndex) throw();

  private:
    LONG volatile nMutex;
    CLnkLst cJvmList;
  };

public:
  class CClientRequest : public CHttpServer::CClientRequest
  {
  protected:
    CClientRequest();
  public:
    ~CClientRequest();

    HRESULT OnSetup();
    BOOL OnCleanup();

    VOID DisplayDebugInfoOnError(_In_ BOOL bFilename, _In_ BOOL bStackTrace);

    HRESULT AttachJVM();
    VOID DiscardVM();

    CJavascriptVM* GetVM(_Out_opt_ LPBOOL lpbIsNew = NULL) const;

    HRESULT RunScript(_In_ LPCSTR szCodeA, _In_opt_z_ LPCWSTR szFileNameW = NULL);

  public:
    TArrayListWithDelete<CStringA*> cOutputBuffersList;

  private:
    VOID FreeJVM();

    HRESULT BuildErrorPage(_In_ HRESULT hr, _In_opt_z_ LPCSTR szDescriptionA, _In_z_ LPCSTR szFileNameA, _In_ int nLine,
                           _In_z_ LPCSTR szStackTraceA);

    HRESULT OnRequireJsModule(_In_ DukTape::duk_context *lpCtx, _In_ CJavascriptVM::CRequireModuleContext *lpReqContext,
                              _Inout_ CStringA &cStrCodeA);

  private:
    friend class CJsHttpServer;

    CJsHttpServer *lpJsHttpServer;
    TAutoRefCounted<CJvmManager> cJvmManager;
    CJvm *lpJVM;
    OnRequireJsModuleCallback cRequireJsModuleCallback;
    LONG volatile nFlags;
  };

private:
  HRESULT OnRequestHeadersReceived(_In_ CHttpServer *lpHttp, _In_ CHttpServer::CClientRequest *lpRequest,
                                   _Outptr_ _Maybenull_ CHttpBodyParserBase **lplpBodyParser);
  VOID OnRequestCompleted(_In_ CHttpServer *lpHttp, _In_ CHttpServer::CClientRequest *lpRequest);
  HRESULT OnWebSocketRequestReceived(_In_ CHttpServer *lpHttp, _In_ CHttpServer::CClientRequest *lpRequest,
                                     _In_ int nVersion, _In_opt_ LPCSTR *szProtocolsA, _In_ SIZE_T nProtocolsCount,
                                     _Out_ int &nSelectedProtocol, _In_ TArrayList<int> &aSupportedVersions,
                                     _Out_ _Maybenull_ CWebSocket **lplpWebSocket);

private:
  OnNewRequestObjectCallback cNewRequestObjectCallback;
  OnRequestHeadersReceivedCallback cRequestHeadersReceivedCallback;
  OnRequestCompletedCallback cRequestCompletedCallback;
  OnRequireJsModuleCallback cRequireJsModuleCallback;
  OnWebSocketRequestReceivedCallback cWebSocketRequestReceivedCallback;

  TAutoRefCounted<CJvmManager> cJvmManager;
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
