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
#ifndef _MX_JS_HTTP_SERVER_H
#define _MX_JS_HTTP_SERVER_H

#include "..\Defines.h"
#include "..\Http\HttpServer.h"
#include "..\JsLib\JavascriptVM.h"

//-----------------------------------------------------------

namespace MX {

class CJsHttpServer : public CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CJsHttpServer);
public:
  class CJsRequest;
  class CRequireModuleContext;
  class CJsRequestRequireModuleContext;

  typedef Callback<HRESULT (_In_ CPropertyBag &cPropBag, _Out_ CJsRequest **lplpRequest)> OnNewRequestObjectCallback;

  typedef Callback<HRESULT (_In_ CJsHttpServer *lpHttp, _In_ CJsRequest *lpRequest,
                            _In_ CJavascriptVM &cJvm, _Inout_ CStringA &cStrCodeA)> OnRequestCallback;
  typedef Callback<VOID (_In_ CJsHttpServer *lpHttp, _In_ CJsRequest *lpRequest,
                         _In_ CJavascriptVM &cJvm)> OnRequestCleanupCallback;

  typedef Callback<HRESULT (_In_ CJsHttpServer *lpHttp, _In_ CJsRequest *lpRequest,
                            _In_ CJavascriptVM &cJvm, _Inout_ CJavascriptVM::CRequireModuleContext *lpReqContext,
                            _Inout_ CStringA &cStrCodeA)> OnRequireJsModuleCallback;

  typedef Callback<VOID (_In_ CJsHttpServer *lpHttp, _In_ CJsRequest *lpRequest,
                         _In_ HRESULT hErrorCode)> OnErrorCallback;

  typedef Callback<VOID (_In_ CJsHttpServer *lpHttp, _In_ CJsRequest *lpRequest, _In_ CJavascriptVM &cJvm,
                         _In_ HRESULT hRunErrorCode)> OnJavascriptErrorCallback;

  //--------

public:
  CJsHttpServer(_In_ MX::CSockets &cSocketMgr, _In_ MX::CPropertyBag &cPropBag);
  ~CJsHttpServer();

  VOID On(_In_ OnNewRequestObjectCallback cNewRequestObjectCallback);
  VOID On(_In_ OnRequestCallback cRequestCallback);
  VOID On(_In_ OnRequestCleanupCallback cRequestCleanupCallback);
  VOID On(_In_ OnRequireJsModuleCallback cRequireJsModuleCallback);
  VOID On(_In_ OnErrorCallback cErrorCallback);
  VOID On(_In_ OnJavascriptErrorCallback cJavascriptErrorCallback);

  HRESULT StartListening(_In_ int nPort, _In_opt_ CIpcSslLayer::eProtocol nProtocol=CIpcSslLayer::ProtocolUnknown,
                         _In_opt_ CSslCertificate *lpSslCertificate=NULL, _In_opt_ CCryptoRSA *lpSslKey=NULL);
  HRESULT StartListening(_In_z_ LPCSTR szBindAddressA, _In_ int nPort,
                         _In_opt_ CIpcSslLayer::eProtocol nProtocol=CIpcSslLayer::ProtocolUnknown,
                         _In_opt_ CSslCertificate *lpSslCertificate=NULL, _In_opt_ CCryptoRSA *lpSslKey=NULL);
  HRESULT StartListening(_In_z_ LPCWSTR szBindAddressW, _In_ int nPort,
                         _In_opt_ CIpcSslLayer::eProtocol nProtocol=CIpcSslLayer::ProtocolUnknown,
                         _In_opt_ CSslCertificate *lpSslCertificate=NULL, _In_opt_ CCryptoRSA *lpSslKey=NULL);
  VOID StopListening();

  static CJsRequest* GetServerRequestFromContext(_In_ DukTape::duk_context *lpCtx);

protected:
  virtual HRESULT OnNewRequestObject(_In_ CPropertyBag &cPropBag, _Out_ CHttpServer::CRequest **lplpRequest);

public:
  class CJsRequest : public CHttpServer::CRequest
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CJsRequest);
  protected:
    CJsRequest(_In_ CPropertyBag &cPropBag);
  public:
    ~CJsRequest();

  public:
    TArrayListWithDelete<CStringA*> cOutputBuffersList;

  private:
    friend class CJsHttpServer;
  };

private:
  HRESULT OnRequestCompleted(_In_ MX::CHttpServer *lpHttp, _In_ CHttpServer::CRequest *lpRequest);

  HRESULT OnRequireJsModule(_In_ DukTape::duk_context *lpCtx, _In_ CJavascriptVM::CRequireModuleContext *lpReqContext,
                            _Inout_ CStringA &cStrCodeA);

  VOID OnError(_In_ CHttpServer *lpHttp, _In_ CHttpServer::CRequest *lpRequest, _In_ HRESULT hErrorCode);

  HRESULT InitializeJVM(_In_ CJavascriptVM &cJvm, _In_ CJsRequest *lpRequest);

  HRESULT TransformJavascriptCode(_Inout_ MX::CStringA &cStrCodeA);
  BOOL TransformJavascriptCode_ConvertToPrint(_Inout_ MX::CStringA &cStrCodeA, _Inout_ SIZE_T nNonCodeBlockStart,
                                              _Inout_ SIZE_T &nCurrPos);

  HRESULT InsertPostField(_In_ CJavascriptVM &cJvm, _In_ CHttpBodyParserFormBase::CField *lpField,
                          _In_ LPCSTR szBaseObjectNameA);
  HRESULT InsertPostFileField(_In_ CJavascriptVM &cJvm, _In_ CHttpBodyParserFormBase::CFileField *lpFileField,
                              _In_ LPCSTR szBaseObjectNameA);

  HRESULT ResetAndDisableClientCache(_In_ CJsRequest *lpRequest);
  HRESULT BuildErrorPage(_In_ CJsRequest *lpRequest, _In_ HRESULT hr, _In_z_ LPCSTR szDescriptionA,
                         _In_z_ LPCSTR szFileNameA, _In_ int nLine, _In_z_ LPCSTR szStackTraceA);

private:
  CSockets &cSocketMgr;
  CPropertyBag &cPropBag;
  CHttpServer cHttpServer;
  //----
  BOOL bShowStackTraceOnError;
  OnNewRequestObjectCallback cNewRequestObjectCallback;
  OnRequestCallback cRequestCallback;
  OnRequestCleanupCallback cRequestCleanupCallback;
  OnRequireJsModuleCallback cRequireJsModuleCallback;
  OnErrorCallback cErrorCallback;
  OnJavascriptErrorCallback cJavascriptErrorCallback;
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
