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

  MX_JS_BEGIN_MAP(CJsMySqlPlugin, "MySQL", 0)
    MX_JS_MAP_METHOD("connect", &CJsMySqlPlugin::Connect, MX_JS_VARARGS) //host,user[,pass[,dbname[,port]]]
    MX_JS_MAP_METHOD("disconnect", &CJsMySqlPlugin::Disconnect, 0)
    MX_JS_MAP_METHOD("selectDatabase", &CJsMySqlPlugin::SelectDatabase, 1)
    MX_JS_MAP_METHOD("query", &CJsMySqlPlugin::Query, 1)
    MX_JS_MAP_METHOD("queryAndFetchRow", &CJsMySqlPlugin::QueryAndFetch, 1)
    MX_JS_MAP_METHOD("queryClose", &CJsMySqlPlugin::QueryClose, 0)
    MX_JS_MAP_METHOD("escapeString", &CJsMySqlPlugin::EscapeString, MX_JS_VARARGS)
    MX_JS_MAP_METHOD("fetchRow", &CJsMySqlPlugin::FetchRow, 0)
    MX_JS_MAP_METHOD("beginTransaction", &CJsMySqlPlugin::BeginTransaction, 0)
    MX_JS_MAP_METHOD("commit", &CJsMySqlPlugin::CommitTransaction, 0)
    MX_JS_MAP_METHOD("rollback", &CJsMySqlPlugin::RollbackTransaction, 0)
    MX_JS_MAP_PROPERTY("error", &CJsMySqlPlugin::getLastError, NULL, FALSE)
    MX_JS_MAP_PROPERTY("dbError", &CJsMySqlPlugin::getLastDbError, NULL, FALSE)
    MX_JS_MAP_PROPERTY("affectedRows", &CJsMySqlPlugin::getAffectedRows, NULL, FALSE)
    MX_JS_MAP_PROPERTY("insertId", &CJsMySqlPlugin::getInsertId, NULL, FALSE)
    MX_JS_MAP_PROPERTY("fields", &CJsMySqlPlugin::getFields, NULL, FALSE)
    MX_JS_MAP_PROPERTY("fieldsCount", &CJsMySqlPlugin::getFieldsCount, NULL, FALSE)
  MX_JS_END_MAP()

private:
  DukTape::duk_ret_t Connect(__in DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t Disconnect(__in DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t SelectDatabase(__in DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t Query(__in DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t QueryAndFetch(__in DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t QueryClose(__in DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t EscapeString(__in DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t FetchRow(__in DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t BeginTransaction(__in DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t CommitTransaction(__in DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t RollbackTransaction(__in DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t getLastError(__in DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t getLastDbError(__in DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t getAffectedRows(__in DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t getInsertId(__in DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t getFieldsCount(__in DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t getFields(__in DukTape::duk_context *lpCtx);

  HRESULT _TransactionStart(__out int &nDbErr);
  HRESULT _TransactionCommit(__out int &nDbErr);
  HRESULT _TransactionRollback(__out int &nDbErr);

  DukTape::duk_ret_t ReturnErrorFromHResult(__in DukTape::duk_context *lpCtx, __in HRESULT hRes);
  DukTape::duk_ret_t ReturnErrorFromHResultAndDbErr(__in DukTape::duk_context *lpCtx, __in HRESULT hRes,
                                                    __in int nDbErr);
  DukTape::duk_ret_t ReturnErrorFromLastDbErr(__in DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t ReturnErrorFromDbErr(__in DukTape::duk_context *lpCtx, __in int nDbErr);

private:
  LPVOID lpInternal;
  HRESULT hLastErr;
  int nLastDbErr;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JS_MYSQL_PLUGIN_H
