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
#ifndef _MX_JS_SQLITE_PLUGIN_H
#define _MX_JS_SQLITE_PLUGIN_H

#include "..\JavascriptVM.h"

//-----------------------------------------------------------

namespace MX {

class CJsSQLitePlugin : public CJsObjectBase
{
  MX_DISABLE_COPY_CONSTRUCTOR(CJsSQLitePlugin);
public:
  CJsSQLitePlugin(_In_ DukTape::duk_context *lpCtx);
  ~CJsSQLitePlugin();

  MX_JS_DECLARE_CREATABLE(CJsSQLitePlugin, "SQLite")

  MX_JS_BEGIN_MAP(CJsSQLitePlugin)
    MX_JS_MAP_METHOD("connect", &CJsSQLitePlugin::Connect, MX_JS_VARARGS) //host,user[,pass[,dbname[,port]]]
    MX_JS_MAP_METHOD("disconnect", &CJsSQLitePlugin::Disconnect, 0)
    MX_JS_MAP_METHOD("query", &CJsSQLitePlugin::Query, MX_JS_VARARGS)
    MX_JS_MAP_METHOD("queryAndFetchRow", &CJsSQLitePlugin::QueryAndFetch, MX_JS_VARARGS)
    MX_JS_MAP_METHOD("queryClose", &CJsSQLitePlugin::QueryClose, 0)
    MX_JS_MAP_METHOD("escapeString", &CJsSQLitePlugin::EscapeString, MX_JS_VARARGS)
    MX_JS_MAP_METHOD("utf8Truncate", &CJsSQLitePlugin::Utf8Truncate, 2)
    MX_JS_MAP_METHOD("fetchRow", &CJsSQLitePlugin::FetchRow, 0)
    MX_JS_MAP_METHOD("beginTransaction", &CJsSQLitePlugin::BeginTransaction, 0)
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

  static HRESULT HResultFromSQLiteErr(_In_ int nError);

private:
  LPVOID lpInternal;
  struct {
    BOOL bDontCreate, bReadOnly;
    DWORD dwCloseTimeoutMs;
    DWORD dwBusyTimeoutMs;
  } sOptions;
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
