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
#ifndef _MX_JS_MYSQL_PLUGIN_H
#define _MX_JS_MYSQL_PLUGIN_H

#include "..\JavascriptVM.h"

//-----------------------------------------------------------

namespace MX {

class CJsMySqlPlugin : public CJsObjectBase
{
  MX_DISABLE_COPY_CONSTRUCTOR(CJsMySqlPlugin);
public:
  CJsMySqlPlugin(__in DukTape::duk_context *lpCtx);
  ~CJsMySqlPlugin();

  MX_JS_DECLARE_CREATABLE(CJsMySqlPlugin, "MySQL")

  MX_JS_BEGIN_MAP(CJsMySqlPlugin)
    MX_JS_MAP_METHOD("connect", &CJsMySqlPlugin::Connect, MX_JS_VARARGS) //host,user[,pass[,dbname[,port]]]
    MX_JS_MAP_METHOD("disconnect", &CJsMySqlPlugin::Disconnect, 0)
    MX_JS_MAP_METHOD("selectDatabase", &CJsMySqlPlugin::SelectDatabase, 1)
    MX_JS_MAP_METHOD("query", &CJsMySqlPlugin::Query, 1)
    MX_JS_MAP_METHOD("queryAndFetchRow", &CJsMySqlPlugin::QueryAndFetch, 1)
    MX_JS_MAP_METHOD("queryClose", &CJsMySqlPlugin::QueryClose, 0)
    MX_JS_MAP_METHOD("escapeString", &CJsMySqlPlugin::EscapeString, MX_JS_VARARGS)
    MX_JS_MAP_METHOD("utf8Truncate", &CJsMySqlPlugin::Utf8Truncate, 2)
    MX_JS_MAP_METHOD("fetchRow", &CJsMySqlPlugin::FetchRow, 0)
    MX_JS_MAP_METHOD("beginTransaction", &CJsMySqlPlugin::BeginTransaction, 0)
    MX_JS_MAP_METHOD("commit", &CJsMySqlPlugin::CommitTransaction, 0)
    MX_JS_MAP_METHOD("rollback", &CJsMySqlPlugin::RollbackTransaction, 0)
    MX_JS_MAP_PROPERTY("affectedRows", &CJsMySqlPlugin::getAffectedRows, NULL, FALSE)
    MX_JS_MAP_PROPERTY("insertId", &CJsMySqlPlugin::getInsertId, NULL, FALSE)
    MX_JS_MAP_PROPERTY("fields", &CJsMySqlPlugin::getFields, NULL, FALSE)
    MX_JS_MAP_PROPERTY("fieldsCount", &CJsMySqlPlugin::getFieldsCount, NULL, FALSE)
  MX_JS_END_MAP()

protected:
  static VOID OnRegister(__in DukTape::duk_context *lpCtx);
  static VOID OnUnregister(__in DukTape::duk_context *lpCtx);

private:
  DukTape::duk_ret_t Connect();
  DukTape::duk_ret_t Disconnect();
  DukTape::duk_ret_t SelectDatabase();
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

  HRESULT _TransactionStart();
  HRESULT _TransactionCommit();
  HRESULT _TransactionRollback();

  VOID ThrowDbError(__in HRESULT hRes, __in_opt LPCSTR filename, __in_opt DukTape::duk_int_t line,
                    __in_opt BOOL bOnlyPush=FALSE);
  VOID ThrowDbError(__in_opt LPCSTR filename, __in_opt DukTape::duk_int_t line, __in_opt BOOL bOnlyPush=FALSE);

private:
  LPVOID lpInternal;
  struct {
    long nConnectTimeout;
    long nReadTimeout, nWriteTimeout;
  } sOptions;
};

//-----------------------------------------------------------

class CJsMySqlError : public CJsWindowsError
{
protected:
  CJsMySqlError(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nStackIndex);

public:
  CJsMySqlError(__in const CJsMySqlError &obj);
  CJsMySqlError& operator=(__in const CJsMySqlError &obj);

  ~CJsMySqlError();

  __inline int GetDbError() const
    {
    return nDbError;
    };

  __inline LPCSTR GetSqlState() const
    {
    return (lpStrSqlStateA != NULL) ? (LPCSTR)(*lpStrSqlStateA) : "";
    };

private:
  friend class CJsMySqlPlugin;

  VOID Cleanup();

private:
  int nDbError;
  CRefCountedStringA *lpStrSqlStateA;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JS_MYSQL_PLUGIN_H
