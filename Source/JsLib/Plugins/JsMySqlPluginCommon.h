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
#ifndef _MX_JS_MYSQL_PLUGIN_COMMON_H
#define _MX_JS_MYSQL_PLUGIN_COMMON_H

#include "..\..\..\Include\JsLib\Plugins\JsMySqlPlugin.h"
#include "..\..\..\Include\ArrayList.h"
#if (!defined(_WS2DEF_)) && (!defined(_WINSOCKAPI_))
  #include <WS2tcpip.h>
#endif
#include "mysql\mysql.h"
#include "mysql\mysqld_error.h"
#include "mysql\errmsg.h"
#include "mysql\my_byteorder.h"

//-----------------------------------------------------------

typedef int              (__stdcall *lpfn_mysql_server_init)(int argc, char **argv, char **groups);
typedef void             (__stdcall *lpfn_mysql_server_end)(void);

typedef MYSQL*           (__stdcall *lpfn_mysql_init)(MYSQL *mysql);
typedef int              (__stdcall *lpfn_mysql_options)(MYSQL *mysql, enum mysql_option option, const LPCSTR arg);
typedef MYSQL*           (__stdcall *lpfn_mysql_real_connect)(MYSQL *mysql, const LPCSTR host, const LPCSTR user,
                                                              const LPCSTR passwd, const LPCSTR db, UINT port,
                                                              const LPCSTR unix_socket, ULONG clientflag);
typedef int              (__stdcall *lpfn_mysql_select_db)(MYSQL *mysql, const LPCSTR db);
typedef void             (__stdcall *lpfn_mysql_close)(MYSQL *sock);
typedef ULONG            (__stdcall *lpfn_mysql_real_escape_string)(MYSQL *mysql, LPCSTR to,const LPCSTR from,
                                                                    ULONG length);
typedef ULONG            (__stdcall *lpfn_mysql_escape_string)(LPCSTR to,const LPCSTR from, ULONG from_length);
typedef int              (__stdcall *lpfn_mysql_real_query)(MYSQL *mysql, const LPCSTR q, ULONG length);
typedef MYSQL_RES*       (__stdcall *lpfn_mysql_store_result)(MYSQL *mysql);
typedef MYSQL_RES*       (__stdcall *lpfn_mysql_use_result)(MYSQL *mysql);
typedef void             (__stdcall *lpfn_mysql_free_result)(MYSQL_RES *result);
typedef UINT             (__stdcall *lpfn_mysql_field_count)(MYSQL *mysql);
typedef ULONGLONG        (__stdcall *lpfn_mysql_affected_rows)(MYSQL *mysql);
typedef ULONGLONG        (__stdcall *lpfn_mysql_num_rows)(MYSQL_RES *res);
typedef UINT             (__stdcall *lpfn_mysql_num_fields)(MYSQL_RES *res);
typedef MYSQL_ROW        (__stdcall *lpfn_mysql_fetch_row)(MYSQL_RES *result);
typedef ULONG*           (__stdcall *lpfn_mysql_fetch_lengths)(MYSQL_RES *result);
typedef MYSQL_FIELD*     (__stdcall *lpfn_mysql_fetch_fields)(MYSQL_RES *result);
typedef ULONG            (__stdcall *lpfn_mysql_thread_id)(MYSQL *mysql);
typedef ULONGLONG        (__stdcall *lpfn_mysql_insert_id)(MYSQL *mysql);
typedef int              (__stdcall *lpfn_mysql_set_character_set)(MYSQL *mysql, const LPCSTR csname);
typedef UINT             (__stdcall *lpfn_mysql_errno)(MYSQL *mysql);
typedef const LPCSTR     (__stdcall *lpfn_mysql_error)(MYSQL *mysql);
typedef const LPCSTR     (__stdcall *lpfn_mysql_sqlstate)(MYSQL *mysql);

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CJsMySqlPluginHelpers : public virtual CBaseMemObj
{
public:
  CJsMySqlPluginHelpers();
  ~CJsMySqlPluginHelpers();

  HRESULT ExecuteQuery(__in LPCSTR szQueryA, __in SIZE_T nQueryLegth, __out_opt MYSQL_RES **lplpResult=NULL);
  VOID QueryClose(__in_opt BOOL bFlushData=TRUE);
  VOID CloseResultSet(__in MYSQL_RES *lpResult, __in_opt BOOL bFlushData=TRUE);

  BOOL BuildFieldInfoArray();

  static BOOL IsConnectionLostError(__in int nError);

  static HRESULT HResultFromMySqlErr(__in int nError);

public:
  class CFieldInfo : public CJsObjectBase
  {
  public:
    CFieldInfo(__in DukTape::duk_context *lpCtx);
    ~CFieldInfo();

    LPCSTR GetName() const
      {
      return (LPCSTR)cStrNameA;
      };

    enum enum_field_types GetType() const
      {
      return nType;
      };

    ULONG GetFlags() const
      {
      return nFlags;
      };

    MX_JS_BEGIN_MAP(CFieldInfo, "MySQLFieldInfo", 0)
      MX_JS_MAP_PROPERTY("name", &CFieldInfo::getName, NULL, TRUE)
      MX_JS_MAP_PROPERTY("originalName", &CFieldInfo::getOriginalName, NULL, TRUE)
      MX_JS_MAP_PROPERTY("table", &CFieldInfo::getTable, NULL, TRUE)
      MX_JS_MAP_PROPERTY("originalTable", &CFieldInfo::getOriginalTable, NULL, TRUE)
      MX_JS_MAP_PROPERTY("database", &CFieldInfo::getDatabase, NULL, TRUE)
      MX_JS_MAP_PROPERTY("length", &CFieldInfo::getLength, NULL, TRUE)
      MX_JS_MAP_PROPERTY("maxLength", &CFieldInfo::getMaxLength, NULL, TRUE)
      MX_JS_MAP_PROPERTY("decimals", &CFieldInfo::getDecimalsCount, NULL, TRUE)
      MX_JS_MAP_PROPERTY("charset", &CFieldInfo::getCharSet, NULL, TRUE)
      MX_JS_MAP_PROPERTY("canNull", &CFieldInfo::getCanBeNull, NULL, TRUE)
      MX_JS_MAP_PROPERTY("isPrimaryKey", &CFieldInfo::getIsPrimaryKey, NULL, TRUE)
      MX_JS_MAP_PROPERTY("isUniqueKey", &CFieldInfo::getIsUniqueKey, NULL, TRUE)
      MX_JS_MAP_PROPERTY("isKey", &CFieldInfo::getIsKey, NULL, TRUE)
      MX_JS_MAP_PROPERTY("isUnsigned", &CFieldInfo::getIsUnsigned, NULL, TRUE)
      MX_JS_MAP_PROPERTY("isZeroFill", &CFieldInfo::getIsZeroFill, NULL, TRUE)
      MX_JS_MAP_PROPERTY("isBinary", &CFieldInfo::getIsBinary, NULL, TRUE)
      MX_JS_MAP_PROPERTY("isAutoIncrement", &CFieldInfo::getIsAutoIncrement, NULL, TRUE)
      MX_JS_MAP_PROPERTY("isEnum", &CFieldInfo::getIsEnum, NULL, TRUE)
      MX_JS_MAP_PROPERTY("isSet", &CFieldInfo::getIsSet, NULL, TRUE)
      MX_JS_MAP_PROPERTY("hasDefault", &CFieldInfo::getHasDefault, NULL, TRUE)
      MX_JS_MAP_PROPERTY("isNumeric", &CFieldInfo::getIsNumeric, NULL, TRUE)
      MX_JS_MAP_PROPERTY("type", &CFieldInfo::getType, NULL, TRUE)
    MX_JS_END_MAP()

  private:
    DukTape::duk_ret_t getName(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getOriginalName(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getTable(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getOriginalTable(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getDatabase(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getLength(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getMaxLength(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getDecimalsCount(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getCharSet(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getCanBeNull(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getIsPrimaryKey(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getIsUniqueKey(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getIsKey(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getIsUnsigned(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getIsZeroFill(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getIsBinary(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getIsAutoIncrement(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getIsEnum(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getIsSet(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getHasDefault(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getIsNumeric(__in DukTape::duk_context *lpCtx);
    DukTape::duk_ret_t getType(__in DukTape::duk_context *lpCtx);

  private:
    friend class CJsMySqlPluginHelpers;

    CStringA cStrNameA, cStrOriginalNameA;
    CStringA cStrTableA, cStrOriginalTableA;
    CStringA cStrDatabaseA;
    ULONG nLength, nMaxLength;
    UINT nDecimalsCount, nCharSet, nFlags;
    enum enum_field_types nType;
  };

public:
  MYSQL *lpDB;
  int nServerUsingNoBackslashEscapes;
  struct {
    MYSQL_RES *lpResultSet;
    ULONGLONG nAffectedRows;
    ULONGLONG nLastInsertId;
    SIZE_T nFieldsCount;
    TArrayListWithRelease<CFieldInfo*> aFieldsList;
  } sQuery;
  SIZE_T nTransactionCounter;
};

//-----------------------------------------------------------

namespace API {

HRESULT Initialize();

extern lpfn_mysql_init fn_mysql_init;
extern lpfn_mysql_options fn_mysql_options;
extern lpfn_mysql_real_connect fn_mysql_real_connect;
extern lpfn_mysql_select_db fn_mysql_select_db;
extern lpfn_mysql_close fn_mysql_close;
extern lpfn_mysql_real_escape_string fn_mysql_real_escape_string;
extern lpfn_mysql_escape_string fn_mysql_escape_string;
extern lpfn_mysql_real_query fn_mysql_real_query;
extern lpfn_mysql_store_result fn_mysql_store_result;
extern lpfn_mysql_use_result fn_mysql_use_result;
extern lpfn_mysql_free_result fn_mysql_free_result;
extern lpfn_mysql_field_count fn_mysql_field_count;
extern lpfn_mysql_affected_rows fn_mysql_affected_rows;
extern lpfn_mysql_num_rows fn_mysql_num_rows;
extern lpfn_mysql_num_fields fn_mysql_num_fields;
extern lpfn_mysql_fetch_row fn_mysql_fetch_row;
extern lpfn_mysql_fetch_lengths fn_mysql_fetch_lengths;
extern lpfn_mysql_fetch_fields fn_mysql_fetch_fields;
extern lpfn_mysql_thread_id fn_mysql_thread_id;
extern lpfn_mysql_insert_id fn_mysql_insert_id;
extern lpfn_mysql_set_character_set fn_mysql_set_character_set;
extern lpfn_mysql_errno fn_mysql_errno;
extern lpfn_mysql_error fn_mysql_error;
extern lpfn_mysql_sqlstate fn_mysql_sqlstate;
} //namespace API

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JS_MYSQL_PLUGIN_COMMON_H
