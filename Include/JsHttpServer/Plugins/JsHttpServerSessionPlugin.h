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
#ifndef _MX_JS_HTTP_SERVER_SESSION_PLUGIN_H
#define _MX_JS_HTTP_SERVER_SESSION_PLUGIN_H

#include "..\JsHttpServer.h"

//-----------------------------------------------------------

namespace MX {

class CJsHttpServerSessionPlugin : public CJsObjectBase, public CNonCopyableObj
{
public:
  //can be called simultaneously from different threads servicing different requests
  typedef Callback<HRESULT (_In_ CJsHttpServerSessionPlugin *lpPlugin, _In_ BOOL bLoading)> OnLoadSaveCallback;

public:
  CJsHttpServerSessionPlugin();
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

  DukTape::duk_ret_t _Save(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t RegenerateId(_In_ DukTape::duk_context *lpCtx);

  int OnProxyHasNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szPropNameA);
  int OnProxyHasIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_ int nIndex);

  int OnProxyGetNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szPropNameA);
  int OnProxyGetIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_ int nIndex);

  int OnProxySetNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szPropNameA,
                              _In_ DukTape::duk_idx_t nValueIndex);
  int OnProxySetIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_ int nIndex, _In_ DukTape::duk_idx_t nValueIndex);

  int OnProxyDeleteNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szPropNameA);
  int OnProxyDeleteIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_ int nIndex);

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
