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
#ifndef _MX_JS_HTTP_SERVER_SESSION_PLUGIN_H
#define _MX_JS_HTTP_SERVER_SESSION_PLUGIN_H

#include "..\JsHttpServer.h"

//-----------------------------------------------------------

namespace MX {

class CJsHttpServerSessionPlugin : public CJsObjectBase
{
  MX_DISABLE_COPY_CONSTRUCTOR(CJsHttpServerSessionPlugin);
public:
  //can be called simultaneously from different threads servicing different requests
  typedef Callback<HRESULT (__in CJsHttpServerSessionPlugin *lpPlugin, __in BOOL bLoading)> OnLoadSaveCallback;

  CJsHttpServerSessionPlugin(__in DukTape::duk_context *lpCtx);
  ~CJsHttpServerSessionPlugin();

  HRESULT Setup(__in CHttpServer::CRequest *lpRequest, __in OnLoadSaveCallback cLoadSaveCallback,
                __in_z_opt LPCWSTR szSessionVarNameW=NULL, __in_z_opt LPCWSTR szDomainW=NULL,
                __in_z_opt LPCWSTR szPathW=NULL, __in_opt int nExpireTimeInSeconds=-1,
                __in_opt BOOL bIsSecure=FALSE, __in_opt BOOL bIsHttpOnly=FALSE);

  LPCSTR GetSessionId() const;
  CJsHttpServer* GetServer() const;
  CHttpServer::CRequest* GetRequest() const;
  CPropertyBag* GetBag() const;

  MX_JS_BEGIN_MAP(CJsHttpServerSessionPlugin, "Session", 0)
    MX_JS_MAP_METHOD("Save", &CJsHttpServerSessionPlugin::Save, 0)
    MX_JS_MAP_METHOD("RegenerateId", &CJsHttpServerSessionPlugin::RegenerateId, 0)
  MX_JS_END_MAP()

private:
  typedef CHAR SESSION_ID[48];

  static BOOL IsValidSessionId(__in_z LPCSTR szSessionIdA);

  HRESULT RegenerateSessionId();
  HRESULT _Save();

  DukTape::duk_ret_t Save();
  DukTape::duk_ret_t RegenerateId();

  int OnProxyHasNamedProperty(__in_z LPCSTR szPropNameA);
  int OnProxyHasIndexedProperty(__in int nIndex);

  int OnProxyGetNamedProperty(__in_z LPCSTR szPropNameA);
  int OnProxyGetIndexedProperty(__in int nIndex);

  int OnProxySetNamedProperty(__in_z LPCSTR szPropNameA, __in DukTape::duk_idx_t nValueIndex);
  int OnProxySetIndexedProperty(__in int nIndex, __in DukTape::duk_idx_t nValueIndex);

  int OnProxyDeleteNamedProperty(__in_z LPCSTR szPropNameA);
  int OnProxyDeleteIndexedProperty(__in int nIndex);

private:
  CPropertyBag cBag;
  SESSION_ID szCurrentIdA;

  CJsHttpServer *lpHttpServer;
  CHttpServer::CRequest *lpRequest;
  OnLoadSaveCallback cLoadSaveCallback;
  MX::CStringA cStrSessionVarNameA, cStrDomainA, cStrPathA;
  LPCSTR szSessionVarNameA;
  int nExpireTimeInSeconds;
  BOOL bIsSecure, bIsHttpOnly;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JS_HTTP_SERVER_SESSION_PLUGIN_H
