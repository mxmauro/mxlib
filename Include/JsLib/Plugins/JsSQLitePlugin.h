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

//-----------------------------------------------------------

namespace MX {

class CJsSQLitePlugin : public CJsObjectBase, public CNonCopyableObj
{
public:
  CJsSQLitePlugin(_In_ DukTape::duk_context *lpCtx);
  ~CJsSQLitePlugin();

  MX_JS_DECLARE_CREATABLE(CJsSQLitePlugin, "SQLite")

  MX_JS_BEGIN_MAP(CJsSQLitePlugin)
    MX_JS_MAP_METHOD("connect", &CJsSQLitePlugin::Connect, MX_JS_VARARGS) //filename[,options]
    MX_JS_MAP_METHOD("disconnect", &CJsSQLitePlugin::Disconnect, 0)
    MX_JS_MAP_METHOD("query", &CJsSQLitePlugin::Query, MX_JS_VARARGS)
    MX_JS_MAP_METHOD("queryAndFetchRow", &CJsSQLitePlugin::QueryAndFetch, MX_JS_VARARGS)
    MX_JS_MAP_METHOD("queryClose", &CJsSQLitePlugin::QueryClose, 0)
    MX_JS_MAP_METHOD("escapeString", &CJsSQLitePlugin::EscapeString, MX_JS_VARARGS)
    MX_JS_MAP_METHOD("utf8Truncate", &CJsSQLitePlugin::Utf8Truncate, 2)
    MX_JS_MAP_METHOD("fetchRow", &CJsSQLitePlugin::FetchRow, 0)
    MX_JS_MAP_METHOD("beginTransaction", &CJsSQLitePlugin::BeginTransaction, MX_JS_VARARGS)
    MX_JS_MAP_METHOD("commit", &CJsSQLitePlugin::CommitTransaction, 0)
    MX_JS_MAP_METHOD("rollback", &CJsSQLitePlugin::RollbackTransaction, 0)
    MX_JS_MAP_PROPERTY("affectedRows", &CJsSQLitePlugin::getAffectedRows, NULL, FALSE)
    MX_JS_MAP_PROPERTY("insertId", &CJsSQLitePlugin::getInsertId, NULL, FALSE)
    MX_JS_MAP_PROPERTY("fields", &CJsSQLitePlugin::getFields, NULL, FALSE)
    MX_JS_MAP_PROPERTY("fieldsCount", &CJsSQLitePlugin::getFieldsCount, NULL, FALSE)
  MX_JS_END_MAP()

protected:
  static VOID OnRegister(_In_ DukTape::duk_context *lpCtx);
  static VOID OnUnregister(_In_ DukTape::duk_context *lpCtx);

private:
  DukTape::duk_ret_t Connect();
  DukTape::duk_ret_t Disconnect();
  DukTape::duk_ret_t Query();
  DukTape::duk_ret_t QueryAndFetch();
  DukTape::duk_ret_t QueryClose();
  DukTape::duk_ret_t EscapeString();
  DukTape::duk_ret_t Utf8Truncate();
  DukTape::duk_ret_t FetchRow();
  DukTape::duk_ret_t BeginTransaction();
  DukTape::duk_ret_t CommitTransaction();
  DukTape::duk_ret_t RollbackTransaction();
  DukTape::duk_ret_t getAffectedRows();
  DukTape::duk_ret_t getInsertId();
  DukTape::duk_ret_t getFieldsCount();
  DukTape::duk_ret_t getFields();

  VOID ThrowDbError(_In_opt_ LPCSTR filename, _In_opt_ DukTape::duk_int_t line);
  VOID ThrowDbError(_In_opt_ LPCSTR filename, _In_opt_ DukTape::duk_int_t line, _In_ int err,
                    _In_opt_ HRESULT hRes = S_OK);

  HRESULT HResultFromSQLiteErr(_In_ int nError);

private:
  LPVOID lpInternal;
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

  __inline LPCSTR GetSqlState() const
    {
    return szSqlStateA;
    };

private:
  friend class CJsSQLitePlugin;

  int nDbError;
  CHAR szSqlStateA[8];
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JS_SQLITE_PLUGIN_H
