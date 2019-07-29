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
  typedef Callback<HRESULT (_In_ CJsHttpServerSessionPlugin *lpPlugin, _In_ BOOL bLoading)> OnLoadSaveCallback;

  CJsHttpServerSessionPlugin(_In_ DukTape::duk_context *lpCtx);
  ~CJsHttpServerSessionPlugin();

  HRESULT Setup(_In_ CJsHttpServer::CClientRequest *lpRequest, _In_ OnLoadSaveCallback cLoadSaveCallback,
                _In_opt_z_ LPCWSTR szSessionVarNameW = NULL, _In_opt_z_ LPCWSTR szDomainW = NULL,
                _In_opt_z_ LPCWSTR szPathW = NULL, _In_opt_ int nExpireTimeInSeconds = -1,
                _In_opt_ BOOL bIsSecure = FALSE, _In_opt_ BOOL bIsHttpOnly = FALSE);
  HRESULT Save();

  LPCSTR GetSessionId() const
    {
    return szCurrentIdA;
    };

  CJsHttpServer* GetServer() const
    {
    return lpHttpServer;
    };

  CJsHttpServer::CClientRequest* GetRequest() const
    {
    return lpRequest;
    };

  CPropertyBag* GetBag() const;

  MX_JS_DECLARE_WITH_PROXY(CJsHttpServerSessionPlugin, "Session")

  MX_JS_BEGIN_MAP(CJsHttpServerSessionPlugin)
    MX_JS_MAP_METHOD("save", &CJsHttpServerSessionPlugin::_Save, 0)
    MX_JS_MAP_METHOD("regenerateId", &CJsHttpServerSessionPlugin::RegenerateId, 0)
  MX_JS_END_MAP()

private:
  typedef CHAR SESSION_ID[48];

  static BOOL IsValidSessionId(_In_z_ LPCSTR szSessionIdA);

  HRESULT RegenerateSessionId();

  DukTape::duk_ret_t _Save();
  DukTape::duk_ret_t RegenerateId();

  int OnProxyHasNamedProperty(_In_z_ LPCSTR szPropNameA);
  int OnProxyHasIndexedProperty(_In_ int nIndex);

  int OnProxyGetNamedProperty(_In_z_ LPCSTR szPropNameA);
  int OnProxyGetIndexedProperty(_In_ int nIndex);

  int OnProxySetNamedProperty(_In_z_ LPCSTR szPropNameA, _In_ DukTape::duk_idx_t nValueIndex);
  int OnProxySetIndexedProperty(_In_ int nIndex, _In_ DukTape::duk_idx_t nValueIndex);

  int OnProxyDeleteNamedProperty(_In_z_ LPCSTR szPropNameA);
  int OnProxyDeleteIndexedProperty(_In_ int nIndex);

private:
  CPropertyBag cBag;
  SESSION_ID szCurrentIdA;

  CJsHttpServer *lpHttpServer;
  CJsHttpServer::CClientRequest *lpRequest;
  OnLoadSaveCallback cLoadSaveCallback;
  MX::CStringA cStrSessionVarNameA, cStrDomainA, cStrPathA;
  LPCSTR szSessionVarNameA;
  int nExpireTimeInSeconds;
  BOOL bIsSecure, bIsHttpOnly;
  BOOL bDirty;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JS_HTTP_SERVER_SESSION_PLUGIN_H
