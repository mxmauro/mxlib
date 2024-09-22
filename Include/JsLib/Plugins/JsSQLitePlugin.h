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
#ifndef _MX_JS_SQLITE_PLUGIN_H
#define _MX_JS_SQLITE_PLUGIN_H

#include "..\JavascriptVM.h"
#include "..\..\Database\Sqlite3Connector.h"

//-----------------------------------------------------------

namespace MX {

class CJsSQLitePlugin : public CJsObjectBase, public CNonCopyableObj
{
public:
  CJsSQLitePlugin();
  ~CJsSQLitePlugin();

  MX_JS_DECLARE_CREATABLE(CJsSQLitePlugin, "SQLite")

  MX_JS_BEGIN_MAP(CJsSQLitePlugin)
    MX_JS_MAP_METHOD("connect", &CJsSQLitePlugin::Connect, MX_JS_VARARGS) //filename[,options]
    MX_JS_MAP_METHOD("disconnect", &CJsSQLitePlugin::Disconnect, 0)
    MX_JS_MAP_METHOD("query", &CJsSQLitePlugin::Query, MX_JS_VARARGS)
    MX_JS_MAP_METHOD("queryAndFetchRow", &CJsSQLitePlugin::QueryAndFetchRow, MX_JS_VARARGS)
    MX_JS_MAP_METHOD("queryClose", &CJsSQLitePlugin::QueryClose, 0)
    MX_JS_MAP_METHOD("escapeString", &CJsSQLitePlugin::EscapeString, MX_JS_VARARGS)
    MX_JS_MAP_METHOD("fetchRow", &CJsSQLitePlugin::FetchRow, 0)
    MX_JS_MAP_METHOD("beginTransaction", &CJsSQLitePlugin::BeginTransaction, MX_JS_VARARGS)
    MX_JS_MAP_METHOD("commit", &CJsSQLitePlugin::CommitTransaction, 0)
    MX_JS_MAP_METHOD("rollback", &CJsSQLitePlugin::RollbackTransaction, 0)
    MX_JS_MAP_PROPERTY("isConnected", &CJsSQLitePlugin::isConnected, NULL, FALSE)
    MX_JS_MAP_PROPERTY("affectedRows", &CJsSQLitePlugin::getAffectedRows, NULL, FALSE)
    MX_JS_MAP_PROPERTY("insertId", &CJsSQLitePlugin::getInsertId, NULL, FALSE)
  MX_JS_END_MAP()

public:
  VOID SetConnector(_In_ Database::CSQLite3Connector *lpConnector);
  Database::CSQLite3Connector* DetachConnector();
  Database::CSQLite3Connector* GetConnector();

protected:
  static VOID OnRegister(_In_ DukTape::duk_context *lpCtx);
  static VOID OnUnregister(_In_ DukTape::duk_context *lpCtx);

private:
  DukTape::duk_ret_t Connect(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t Disconnect(_In_opt_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t Query(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t QueryAndFetchRow(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t QueryClose(_In_opt_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t EscapeString(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t Utf8Truncate(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t FetchRow(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t BeginTransaction(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t CommitTransaction(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t RollbackTransaction(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t isConnected(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t getAffectedRows(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t getInsertId(_In_ DukTape::duk_context *lpCtx);

  VOID ThrowDbError(_In_ DukTape::duk_context *lpCtx, _In_ HRESULT hRes, _In_opt_ LPCSTR filename,
                    _In_opt_ DukTape::duk_int_t line);

private:
  TAutoRefCounted<Database::CSQLite3Connector> cConnector;
};

//-----------------------------------------------------------

class CJsSQLiteError : public CJsWindowsError
{
protected:
  CJsSQLiteError(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nStackIndex);

public:
  CJsSQLiteError(_In_ const CJsSQLiteError &obj);
  CJsSQLiteError& operator=(_In_ const CJsSQLiteError &obj);

  ~CJsSQLiteError();

  __inline int GetDbError() const
    {
    return nDbError;
    };

  __inline LPCSTR GetDbErrorMessage() const
    {
    return (LPCSTR)cStrDbErrorMessageA;
    };

private:
  friend class CJsSQLitePlugin;

  int nDbError;
  CStringA cStrDbErrorMessageA;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JS_SQLITE_PLUGIN_H
