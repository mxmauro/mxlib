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
#ifndef _MX_JS_MYSQL_PLUGIN_H
#define _MX_JS_MYSQL_PLUGIN_H

#include "..\JavascriptVM.h"

//-----------------------------------------------------------

namespace MX {

class CJsMySqlPlugin : public CJsObjectBase, public CNonCopyableObj
{
public:
  CJsMySqlPlugin();
  ~CJsMySqlPlugin();

  MX_JS_DECLARE_CREATABLE(CJsMySqlPlugin, "MySQL")

  MX_JS_BEGIN_MAP(CJsMySqlPlugin)
    MX_JS_MAP_METHOD("connect", &CJsMySqlPlugin::Connect, MX_JS_VARARGS) //host,user[,pass[,dbname[,port]]]
    MX_JS_MAP_METHOD("disconnect", &CJsMySqlPlugin::Disconnect, 0)
    MX_JS_MAP_METHOD("selectDatabase", &CJsMySqlPlugin::SelectDatabase, 1)
    MX_JS_MAP_METHOD("query", &CJsMySqlPlugin::Query, MX_JS_VARARGS)
    MX_JS_MAP_METHOD("queryAndFetchRow", &CJsMySqlPlugin::QueryAndFetch, MX_JS_VARARGS)
    MX_JS_MAP_METHOD("queryClose", &CJsMySqlPlugin::QueryClose, 0)
    MX_JS_MAP_METHOD("escapeString", &CJsMySqlPlugin::EscapeString, MX_JS_VARARGS)
    MX_JS_MAP_METHOD("utf8Truncate", &CJsMySqlPlugin::Utf8Truncate, 2)
    MX_JS_MAP_METHOD("fetchRow", &CJsMySqlPlugin::FetchRow, 0)
    MX_JS_MAP_METHOD("beginTransaction", &CJsMySqlPlugin::BeginTransaction, MX_JS_VARARGS)
    MX_JS_MAP_METHOD("commit", &CJsMySqlPlugin::CommitTransaction, 0)
    MX_JS_MAP_METHOD("rollback", &CJsMySqlPlugin::RollbackTransaction, 0)
    MX_JS_MAP_PROPERTY("isConnected", &CJsMySqlPlugin::isConnected, NULL, FALSE)
    MX_JS_MAP_PROPERTY("affectedRows", &CJsMySqlPlugin::getAffectedRows, NULL, FALSE)
    MX_JS_MAP_PROPERTY("insertId", &CJsMySqlPlugin::getInsertId, NULL, FALSE)
    MX_JS_MAP_PROPERTY("fields", &CJsMySqlPlugin::getFields, NULL, FALSE)
    MX_JS_MAP_PROPERTY("fieldsCount", &CJsMySqlPlugin::getFieldsCount, NULL, FALSE)
  MX_JS_END_MAP()

protected:
  static VOID OnRegister(_In_ DukTape::duk_context *lpCtx);
  static VOID OnUnregister(_In_ DukTape::duk_context *lpCtx);

private:
  DukTape::duk_ret_t Connect(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t Disconnect(_In_opt_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t SelectDatabase(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t Query(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t QueryAndFetch(_In_ DukTape::duk_context *lpCtx);
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
  DukTape::duk_ret_t getFieldsCount(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t getFields(_In_ DukTape::duk_context *lpCtx);

  VOID ThrowDbError(_In_ DukTape::duk_context *lpCtx, _In_opt_ LPCSTR filename, _In_opt_ DukTape::duk_int_t line,
                    _In_opt_ BOOL bOnlyPush = FALSE);

  static HRESULT HResultFromMySqlErr(_In_ int nError);

private:
  LPVOID lpInternal;
};

//-----------------------------------------------------------

class CJsMySqlError : public CJsWindowsError
{
protected:
  CJsMySqlError(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nStackIndex);

public:
  CJsMySqlError(_In_ const CJsMySqlError &obj);
  CJsMySqlError& operator=(_In_ const CJsMySqlError &obj);

  ~CJsMySqlError();

  __inline int GetDbError() const
    {
    return nDbError;
    };

  __inline LPCSTR GetSqlState() const
    {
    return szSqlStateA;
    };

private:
  friend class CJsMySqlPlugin;

  int nDbError;
  CHAR szSqlStateA[8];
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JS_MYSQL_PLUGIN_H
