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

#include "..\..\..\..\Include\JsLib\Plugins\JsMySqlPlugin.h"
#include "..\..\..\..\Include\ArrayList.h"
#if (!defined(_WS2DEF_)) && (!defined(_WINSOCKAPI_))
  #include <WS2tcpip.h>
#endif
#define HAVE_STRUCT_TIMESPEC
#include <mysql.h>
#include <mysqld_error.h>
#include <errmsg.h>

//-----------------------------------------------------------

typedef int              (__stdcall *lpfn_mysql_server_init)(int argc, char **argv, char **groups);
typedef void             (__stdcall *lpfn_mysql_server_end)(void);

typedef MYSQL*           (__stdcall *lpfn_mysql_init)(MYSQL *mysql);
typedef int              (__stdcall *lpfn_mysql_options)(MYSQL *mysql, enum mysql_option option, const void *arg);
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
typedef unsigned long    (__stdcall *lpfn_mysql_get_client_version)(void);
typedef MYSQL_STMT*      (__stdcall *lpfn_mysql_stmt_init)(MYSQL *mysql);
typedef const LPCSTR     (__stdcall *lpfn_mysql_stmt_error)(MYSQL_STMT *stmt);
typedef UINT             (__stdcall *lpfn_mysql_stmt_errno)(MYSQL_STMT *stmt);
typedef const LPCSTR     (__stdcall *lpfn_mysql_stmt_sqlstate)(MYSQL_STMT *stmt);
typedef my_bool          (__stdcall *lpfn_mysql_stmt_close)(MYSQL_STMT *stmt);
typedef int              (__stdcall *lpfn_mysql_stmt_prepare)(MYSQL_STMT *stmt, const char *stmt_str,
                                                              unsigned long length);
typedef my_bool          (__stdcall *lpfn_mysql_stmt_bind_param)(MYSQL_STMT *stmt, MYSQL_BIND *bind);
typedef int              (__stdcall *lpfn_mysql_stmt_execute)(MYSQL_STMT *stmt);
typedef ULONGLONG        (__stdcall *lpfn_mysql_stmt_affected_rows)(MYSQL_STMT *stmt);
typedef ULONGLONG        (__stdcall *lpfn_mysql_stmt_insert_id)(MYSQL_STMT *stmt);
typedef int              (__stdcall *lpfn_mysql_stmt_fetch)(MYSQL_STMT *stmt);
typedef int              (__stdcall *lpfn_mysql_stmt_fetch_column)(MYSQL_STMT *stmt, MYSQL_BIND *bind_arg,
                                                                   unsigned int column, unsigned long offset);
typedef int              (__stdcall *lpfn_mysql_stmt_store_result)(MYSQL_STMT *stmt);
typedef my_bool          (__stdcall *lpfn_mysql_stmt_bind_result)(MYSQL_STMT *stmt, MYSQL_BIND *bind);
typedef ULONG            (__stdcall *lpfn_mysql_stmt_param_count)(MYSQL_STMT *stmt);
typedef MYSQL_RES*       (__stdcall *lpfn_mysql_stmt_result_metadata)(MYSQL_STMT *stmt);
typedef my_bool          (__stdcall *lpfn_mysql_stmt_attr_set)(MYSQL_STMT *stmt, enum enum_stmt_attr_type attr_type,
                                                               const void *attr);

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CJsMySqlFieldInfo : public CJsObjectBase
{
public:
  CJsMySqlFieldInfo(_In_ DukTape::duk_context *lpCtx);
  ~CJsMySqlFieldInfo();

  MX_JS_DECLARE(CJsMySqlFieldInfo, "MySqlLFieldInfo")

  MX_JS_BEGIN_MAP(CJsMySqlFieldInfo)
    MX_JS_MAP_PROPERTY("name", &CJsMySqlFieldInfo::getName, NULL, TRUE)
    MX_JS_MAP_PROPERTY("originalName", &CJsMySqlFieldInfo::getOriginalName, NULL, TRUE)
    MX_JS_MAP_PROPERTY("table", &CJsMySqlFieldInfo::getTable, NULL, TRUE)
    MX_JS_MAP_PROPERTY("originalTable", &CJsMySqlFieldInfo::getOriginalTable, NULL, TRUE)
    MX_JS_MAP_PROPERTY("database", &CJsMySqlFieldInfo::getDatabase, NULL, TRUE)
    MX_JS_MAP_PROPERTY("decimals", &CJsMySqlFieldInfo::getDecimalsCount, NULL, TRUE)
    MX_JS_MAP_PROPERTY("charset", &CJsMySqlFieldInfo::getCharSet, NULL, TRUE)
    MX_JS_MAP_PROPERTY("canNull", &CJsMySqlFieldInfo::getCanBeNull, NULL, TRUE)
    MX_JS_MAP_PROPERTY("isPrimaryKey", &CJsMySqlFieldInfo::getIsPrimaryKey, NULL, TRUE)
    MX_JS_MAP_PROPERTY("isUniqueKey", &CJsMySqlFieldInfo::getIsUniqueKey, NULL, TRUE)
    MX_JS_MAP_PROPERTY("isKey", &CJsMySqlFieldInfo::getIsKey, NULL, TRUE)
    MX_JS_MAP_PROPERTY("isUnsigned", &CJsMySqlFieldInfo::getIsUnsigned, NULL, TRUE)
    MX_JS_MAP_PROPERTY("isZeroFill", &CJsMySqlFieldInfo::getIsZeroFill, NULL, TRUE)
    MX_JS_MAP_PROPERTY("isBinary", &CJsMySqlFieldInfo::getIsBinary, NULL, TRUE)
    MX_JS_MAP_PROPERTY("isAutoIncrement", &CJsMySqlFieldInfo::getIsAutoIncrement, NULL, TRUE)
    MX_JS_MAP_PROPERTY("isEnum", &CJsMySqlFieldInfo::getIsEnum, NULL, TRUE)
    MX_JS_MAP_PROPERTY("isSet", &CJsMySqlFieldInfo::getIsSet, NULL, TRUE)
    MX_JS_MAP_PROPERTY("hasDefault", &CJsMySqlFieldInfo::getHasDefault, NULL, TRUE)
    MX_JS_MAP_PROPERTY("isNumeric", &CJsMySqlFieldInfo::getIsNumeric, NULL, TRUE)
    MX_JS_MAP_PROPERTY("type", &CJsMySqlFieldInfo::getType, NULL, TRUE)
  MX_JS_END_MAP()

private:
  DukTape::duk_ret_t getName();
  DukTape::duk_ret_t getOriginalName();
  DukTape::duk_ret_t getTable();
  DukTape::duk_ret_t getOriginalTable();
  DukTape::duk_ret_t getDatabase();
  DukTape::duk_ret_t getDecimalsCount();
  DukTape::duk_ret_t getCharSet();
  DukTape::duk_ret_t getCanBeNull();
  DukTape::duk_ret_t getIsPrimaryKey();
  DukTape::duk_ret_t getIsUniqueKey();
  DukTape::duk_ret_t getIsKey();
  DukTape::duk_ret_t getIsUnsigned();
  DukTape::duk_ret_t getIsZeroFill();
  DukTape::duk_ret_t getIsBinary();
  DukTape::duk_ret_t getIsAutoIncrement();
  DukTape::duk_ret_t getIsEnum();
  DukTape::duk_ret_t getIsSet();
  DukTape::duk_ret_t getHasDefault();
  DukTape::duk_ret_t getIsNumeric();
  DukTape::duk_ret_t getType();

private:
  friend class CJsMySqlPlugin;

  CStringA cStrNameA, cStrOriginalNameA;
  CStringA cStrTableA, cStrOriginalTableA;
  CStringA cStrDatabaseA;
  UINT nDecimalsCount, nCharSet, nFlags;
  enum enum_field_types nType;
};

//-----------------------------------------------------------

namespace API {

HRESULT MySqlInitialize();

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
extern lpfn_mysql_stmt_init fn_mysql_stmt_init;
extern lpfn_mysql_stmt_error fn_mysql_stmt_error;
extern lpfn_mysql_stmt_errno fn_mysql_stmt_errno;
extern lpfn_mysql_stmt_sqlstate fn_mysql_stmt_sqlstate;
extern lpfn_mysql_stmt_close fn_mysql_stmt_close;
extern lpfn_mysql_stmt_prepare fn_mysql_stmt_prepare;
extern lpfn_mysql_stmt_bind_param fn_mysql_stmt_bind_param;
extern lpfn_mysql_stmt_execute fn_mysql_stmt_execute;
extern lpfn_mysql_stmt_affected_rows fn_mysql_stmt_affected_rows;
extern lpfn_mysql_stmt_insert_id fn_mysql_stmt_insert_id;
extern lpfn_mysql_stmt_fetch fn_mysql_stmt_fetch;
extern lpfn_mysql_stmt_fetch_column fn_mysql_stmt_fetch_column;
extern lpfn_mysql_stmt_store_result fn_mysql_stmt_store_result;
extern lpfn_mysql_stmt_bind_result fn_mysql_stmt_bind_result;
extern lpfn_mysql_stmt_param_count fn_mysql_stmt_param_count;
extern lpfn_mysql_stmt_result_metadata fn_mysql_stmt_result_metadata;
extern lpfn_mysql_stmt_attr_set fn_mysql_stmt_attr_set;

} //namespace API

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JS_MYSQL_PLUGIN_COMMON_H
