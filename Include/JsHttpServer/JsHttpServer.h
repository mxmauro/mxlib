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
  class CRequireModuleContext;
  class CJsRequestRequireModuleContext;

  typedef Callback<HRESULT (__in CJsHttpServer *lpHttp, __in CHttpServer::CRequest *lpRequest,
                            __in CJavascriptVM &cJvm, __inout CStringA &cStrCodeA)> OnRequestCallback;
  typedef Callback<VOID (__in CJsHttpServer *lpHttp, __in CHttpServer::CRequest *lpRequest,
                         __in CJavascriptVM &cJvm)> OnRequestCleanupCallback;

  typedef Callback<HRESULT (__in CJsHttpServer *lpHttp, __in CHttpServer::CRequest *lpRequest,
                            __in CJavascriptVM &cJvm, __inout CJavascriptVM::CRequireModuleContext *lpReqContext,
                            __inout CStringA &cStrCodeA)> OnRequireJsModuleCallback;

  typedef Callback<VOID (__in CJsHttpServer *lpHttp, __in CHttpServer::CRequest *lpRequest,
                         __in HRESULT hErrorCode)> OnErrorCallback;

  typedef Callback<VOID (__in CJsHttpServer *lpHttp, __in CHttpServer::CRequest *lpRequest, __in CJavascriptVM &cJvm,
                         __in HRESULT hRunErrorCode)> OnJavascriptErrorCallback;

  //--------

public:
  CJsHttpServer(__in MX::CSockets &cSocketMgr, __in MX::CPropertyBag &cPropBag);
  ~CJsHttpServer();

  VOID On(__in OnRequestCallback cRequestCallback);
  VOID On(__in OnRequestCleanupCallback cRequestCleanupCallback);
  VOID On(__in OnRequireJsModuleCallback cRequireJsModuleCallback);
  VOID On(__in OnErrorCallback cErrorCallback);
  VOID On(__in OnJavascriptErrorCallback cJavascriptErrorCallback);

  HRESULT StartListening(__in int nPort, __in_opt CIpcSslLayer::eProtocol nProtocol=CIpcSslLayer::ProtocolUnknown,
                         __in_opt CSslCertificate *lpSslCertificate=NULL, __in_opt CCryptoRSA *lpSslKey=NULL);
  HRESULT StartListening(__in_z LPCSTR szBindAddressA, __in int nPort,
                         __in_opt CIpcSslLayer::eProtocol nProtocol=CIpcSslLayer::ProtocolUnknown,
                         __in_opt CSslCertificate *lpSslCertificate=NULL, __in_opt CCryptoRSA *lpSslKey=NULL);
  HRESULT StartListening(__in_z LPCWSTR szBindAddressW, __in int nPort,
                         __in_opt CIpcSslLayer::eProtocol nProtocol=CIpcSslLayer::ProtocolUnknown,
                         __in_opt CSslCertificate *lpSslCertificate=NULL, __in_opt CCryptoRSA *lpSslKey=NULL);
  VOID StopListening();

  static CHttpServer::CRequest* GetServerRequestFromContext(__in DukTape::duk_context *lpCtx);

private:
  HRESULT OnRequestCompleted(__in MX::CHttpServer *lpHttp, __in MX::CHttpServer::CRequest *lpRequest);

  HRESULT OnRequireJsModule(__in DukTape::duk_context *lpCtx, __in CJavascriptVM::CRequireModuleContext *lpReqContext,
                            __inout CStringA &cStrCodeA);

  VOID OnError(__in CHttpServer *lpHttp, __in CHttpServer::CRequest *lpRequest, __in HRESULT hErrorCode);

  HRESULT InitializeJVM(__in CJavascriptVM &cJvm, __in CHttpServer::CRequest *lpRequest);

  HRESULT TransformJavascriptCode(__inout MX::CStringA &cStrCodeA);
  BOOL TransformJavascriptCode_ConvertToPrint(__inout MX::CStringA &cStrCodeA, __inout SIZE_T nNonCodeBlockStart,
                                              __inout SIZE_T &nCurrPos);

  HRESULT InsertPostField(__in CJavascriptVM &cJvm, __in CHttpBodyParserFormBase::CField *lpField,
                          __in LPCSTR szBaseObjectNameA);
  HRESULT InsertPostFileField(__in CJavascriptVM &cJvm, __in CHttpBodyParserFormBase::CFileField *lpFileField,
                              __in LPCSTR szBaseObjectNameA);

private:
  CSockets &cSocketMgr;
  CPropertyBag &cPropBag;
  CHttpServer cHttpServer;
  //----
  BOOL bShowStackTraceOnError;
  OnRequestCallback cRequestCallback;
  OnRequestCleanupCallback cRequestCleanupCallback;
  OnRequireJsModuleCallback cRequireJsModuleCallback;
  OnErrorCallback cErrorCallback;
  OnJavascriptErrorCallback cJavascriptErrorCallback;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JS_HTTP_SERVER_H
