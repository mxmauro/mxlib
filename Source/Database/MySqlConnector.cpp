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
#include "..\..\Include\Database\MySqlConnector.h"
#include "..\..\Include\Strings\Utf8.h"
#include "..\..\Include\WaitableObjects.h"
#include "..\..\Include\Finalizer.h"
#include "..\..\Include\AutoPtr.h"
#include "..\Internals\SystemDll.h"
#if (!defined(_WS2DEF_)) && (!defined(_WINSOCKAPI_))
  #include <WS2tcpip.h>
#endif //!(_WS2DEF_ || _WINSOCKAPI_)
#include <stdlib.h>
#define HAVE_STRUCT_TIMESPEC
#include <mysql.h>
#include <mysqld_error.h>
#include <errmsg.h>

#define LIBMYSQL_FINALIZER_PRIORITY 10001

//-----------------------------------------------------------

typedef int (__stdcall *lpfn_mysql_server_init)(int argc, char **argv, char **groups);
typedef void (__stdcall *lpfn_mysql_server_end)(void);

typedef MYSQL* (__stdcall *lpfn_mysql_init)(MYSQL *mysql);
typedef int (__stdcall *lpfn_mysql_options)(MYSQL *mysql, enum mysql_option option, const void *arg);
typedef MYSQL* (__stdcall *lpfn_mysql_real_connect)(MYSQL *mysql, const LPCSTR host, const LPCSTR user,
                                                    const LPCSTR passwd, const LPCSTR db, UINT port,
                                                    const LPCSTR unix_socket, ULONG clientflag);
typedef int (__stdcall *lpfn_mysql_select_db)(MYSQL *mysql, const LPCSTR db);
typedef void (__stdcall *lpfn_mysql_close)(MYSQL *mysql);
typedef int (__stdcall *lpfn_mysql_ping)(MYSQL *mysql);
typedef ULONG (__stdcall *lpfn_mysql_real_escape_string)(MYSQL *mysql, LPCSTR to, const LPCSTR from,
                                                         ULONG length);
typedef ULONG (__stdcall *lpfn_mysql_escape_string)(LPCSTR to, const LPCSTR from, ULONG from_length);
typedef int (__stdcall *lpfn_mysql_real_query)(MYSQL *mysql, const LPCSTR q, ULONG length);
typedef MYSQL_RES* (__stdcall *lpfn_mysql_store_result)(MYSQL *mysql);
typedef MYSQL_RES* (__stdcall *lpfn_mysql_use_result)(MYSQL *mysql);
typedef void (__stdcall *lpfn_mysql_free_result)(MYSQL_RES *result);
typedef UINT (__stdcall *lpfn_mysql_field_count)(MYSQL *mysql);
typedef ULONGLONG (__stdcall *lpfn_mysql_affected_rows)(MYSQL *mysql);
typedef ULONGLONG (__stdcall *lpfn_mysql_num_rows)(MYSQL_RES *res);
typedef UINT (__stdcall *lpfn_mysql_num_fields)(MYSQL_RES *res);
typedef MYSQL_ROW (__stdcall *lpfn_mysql_fetch_row)(MYSQL_RES *result);
typedef ULONG* (__stdcall *lpfn_mysql_fetch_lengths)(MYSQL_RES *result);
typedef MYSQL_FIELD* (__stdcall *lpfn_mysql_fetch_fields)(MYSQL_RES *result);
typedef ULONG (__stdcall *lpfn_mysql_thread_id)(MYSQL *mysql);
typedef ULONGLONG (__stdcall *lpfn_mysql_insert_id)(MYSQL *mysql);
typedef int (__stdcall *lpfn_mysql_set_character_set)(MYSQL *mysql, const LPCSTR csname);
typedef UINT (__stdcall *lpfn_mysql_errno)(MYSQL *mysql);
typedef const LPCSTR (__stdcall *lpfn_mysql_error)(MYSQL *mysql);
typedef const LPCSTR (__stdcall *lpfn_mysql_sqlstate)(MYSQL *mysql);
typedef unsigned long (__stdcall *lpfn_mysql_get_client_version)(void);
typedef MYSQL_STMT* (__stdcall *lpfn_mysql_stmt_init)(MYSQL *mysql);
typedef const LPCSTR (__stdcall *lpfn_mysql_stmt_error)(MYSQL_STMT *stmt);
typedef UINT (__stdcall *lpfn_mysql_stmt_errno)(MYSQL_STMT *stmt);
typedef const LPCSTR (__stdcall *lpfn_mysql_stmt_sqlstate)(MYSQL_STMT *stmt);
typedef my_bool (__stdcall *lpfn_mysql_stmt_close)(MYSQL_STMT *stmt);
typedef int (__stdcall *lpfn_mysql_stmt_prepare)(MYSQL_STMT *stmt, const char *stmt_str,
                                                 unsigned long length);
typedef my_bool (__stdcall *lpfn_mysql_stmt_bind_param)(MYSQL_STMT *stmt, MYSQL_BIND *bind);
typedef int (__stdcall *lpfn_mysql_stmt_execute)(MYSQL_STMT *stmt);
typedef ULONGLONG (__stdcall *lpfn_mysql_stmt_affected_rows)(MYSQL_STMT *stmt);
typedef ULONGLONG (__stdcall *lpfn_mysql_stmt_insert_id)(MYSQL_STMT *stmt);
typedef int (__stdcall *lpfn_mysql_stmt_fetch)(MYSQL_STMT *stmt);
typedef int (__stdcall *lpfn_mysql_stmt_fetch_column)(MYSQL_STMT *stmt, MYSQL_BIND *bind_arg,
                                                     unsigned int column, unsigned long offset);
typedef int (__stdcall *lpfn_mysql_stmt_store_result)(MYSQL_STMT *stmt);
typedef my_bool (__stdcall *lpfn_mysql_stmt_bind_result)(MYSQL_STMT *stmt, MYSQL_BIND *bind);
typedef ULONG (__stdcall *lpfn_mysql_stmt_param_count)(MYSQL_STMT *stmt);
typedef MYSQL_RES* (__stdcall *lpfn_mysql_stmt_result_metadata)(MYSQL_STMT *stmt);
typedef my_bool (__stdcall *lpfn_mysql_stmt_attr_set)(MYSQL_STMT *stmt, enum enum_stmt_attr_type attr_type,
                                                      const void *attr);
typedef unsigned long (__stdcall *lpfn_mysql_get_client_version)(void);

//-----------------------------------------------------------

static HINSTANCE hDll = NULL;

#define __DEFINE_API(api) static lpfn_##api fn_##api = NULL

__DEFINE_API(mysql_init);
__DEFINE_API(mysql_options);
__DEFINE_API(mysql_real_connect);
__DEFINE_API(mysql_select_db);
__DEFINE_API(mysql_close);
__DEFINE_API(mysql_ping);
__DEFINE_API(mysql_real_escape_string);
__DEFINE_API(mysql_escape_string);
__DEFINE_API(mysql_real_query);
__DEFINE_API(mysql_store_result);
__DEFINE_API(mysql_use_result);
__DEFINE_API(mysql_free_result);
__DEFINE_API(mysql_field_count);
__DEFINE_API(mysql_affected_rows);
__DEFINE_API(mysql_num_rows);
__DEFINE_API(mysql_num_fields);
__DEFINE_API(mysql_fetch_row);
__DEFINE_API(mysql_fetch_lengths);
__DEFINE_API(mysql_fetch_fields);
__DEFINE_API(mysql_thread_id);
__DEFINE_API(mysql_insert_id);
__DEFINE_API(mysql_set_character_set);
__DEFINE_API(mysql_errno);
__DEFINE_API(mysql_error);
__DEFINE_API(mysql_sqlstate);
__DEFINE_API(mysql_stmt_init);
__DEFINE_API(mysql_stmt_error);
__DEFINE_API(mysql_stmt_errno);
__DEFINE_API(mysql_stmt_sqlstate);
__DEFINE_API(mysql_stmt_close);
__DEFINE_API(mysql_stmt_prepare);
__DEFINE_API(mysql_stmt_bind_param);
__DEFINE_API(mysql_stmt_execute);
__DEFINE_API(mysql_stmt_affected_rows);
__DEFINE_API(mysql_stmt_insert_id);
__DEFINE_API(mysql_stmt_fetch);
__DEFINE_API(mysql_stmt_fetch_column);
__DEFINE_API(mysql_stmt_store_result);
__DEFINE_API(mysql_stmt_bind_result);
__DEFINE_API(mysql_stmt_param_count);
__DEFINE_API(mysql_stmt_result_metadata);
__DEFINE_API(mysql_stmt_attr_set);
__DEFINE_API(mysql_get_client_version);

#undef __DEFINE_API

static MX::Database::CMySqlConnector::CConnectOptions cDefaultConnectOptions;

//-----------------------------------------------------------

static HRESULT InitializeInternals();
static VOID LIBMYSQL_Shutdown();
static VOID GetAnsiAppPath(_Out_ LPSTR szPathA, _In_ SIZE_T nMaxPathLen);
static HRESULT HResultFromMySqlErr(_In_ int err);

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CMySqlConnectorData : public virtual CBaseMemObj
{
public:
  CMySqlConnectorData();

  VOID ClearErrno();
  VOID SetErrno();
  VOID SetCustomErrno(_In_ int err, _In_z_ LPCSTR szDescriptionA, _In_z_ LPCSTR szSqlStateA);

  HRESULT GetHResult() const
    {
    return HResultFromMySqlErr(nLastDbErr);
    };

public:
  class CBuffer : public virtual CBaseMemObj
  {
  public:
    CBuffer();
    ~CBuffer();

    BOOL EnsureSize(_In_ SIZE_T nNewSize);

  public:
    LPBYTE lpBuffer;
    SIZE_T nBufferSize;
  };

public:
  MYSQL *lpDB;
  int nServerUsingNoBackslashEscapes;
  int nLastDbErr;
  CStringA cStrLastDbErrorDescriptionA;
  CHAR szLastDbSqlStateA[8];
  MYSQL_RES *lpResultSet;
  MYSQL_STMT *lpStmt;
  TAutoFreePtr<MYSQL_BIND> cOutputBindings;
  TArrayListWithDelete<CBuffer*> aOutputBuffersList;
};

} //namespace Internals

} //namespace MX

#define mysql_data ((Internals::CMySqlConnectorData*)lpInternalData)

//-----------------------------------------------------------

namespace MX {

namespace Database {

CMySqlConnector::CMySqlConnector() : CBaseConnector()
{
  lpInternalData = NULL;
  return;
}

CMySqlConnector::~CMySqlConnector()
{
  Disconnect();
  return;
}

HRESULT CMySqlConnector::Connect(_In_z_ LPCSTR szServerHostA, _In_z_ LPCSTR szUserNameA,
                                 _In_opt_z_ LPCSTR szUserPasswordA, _In_opt_z_ LPCSTR szDatabaseNameA,
                                 _In_opt_ USHORT wServerPort, _In_opt_ CMySqlConnector::CConnectOptions *lpOptions)
{
  long nTemp;
  CHAR d[4];
  HRESULT hRes;

  if (szServerHostA == NULL || szUserNameA == NULL)
    return E_POINTER;
  if (*szServerHostA == 0 || *szUserNameA == 0)
    return E_INVALIDARG;

  if (lpOptions == NULL)
  {
    lpOptions = &cDefaultConnectOptions;
  }
  Disconnect();

  //initialize APIs
  hRes = InitializeInternals();
  if (FAILED(hRes))
    return hRes;

  //create internal object
  lpInternalData = MX_DEBUG_NEW Internals::CMySqlConnectorData();
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;

  //initialize mysql
  mysql_data->lpDB = fn_mysql_init(NULL);
  if (mysql_data->lpDB == NULL)
  {
    delete mysql_data;
    lpInternalData = NULL;
    return E_OUTOFMEMORY;
  }

  nTemp = (long)(lpOptions->dwConnectTimeoutMs / 1000);
  fn_mysql_options(mysql_data->lpDB, MYSQL_OPT_CONNECT_TIMEOUT, &nTemp);

  nTemp = (long)(lpOptions->dwReadTimeoutMs / 1000);
  fn_mysql_options(mysql_data->lpDB, MYSQL_OPT_READ_TIMEOUT, &nTemp);

  nTemp = (long)(lpOptions->dwWriteTimeoutMs / 1000);
  fn_mysql_options(mysql_data->lpDB, MYSQL_OPT_WRITE_TIMEOUT, &nTemp);

  fn_mysql_options(mysql_data->lpDB, MYSQL_OPT_GUESS_CONNECTION, "");

  nTemp = 0; //<<---Seeing LibMariaDB code, this fix the issue
  fn_mysql_options(mysql_data->lpDB, MYSQL_REPORT_DATA_TRUNCATION, &nTemp);

  nTemp = 0;
  fn_mysql_options(mysql_data->lpDB, MYSQL_OPT_RECONNECT, &nTemp);

  nTemp = 1;
  fn_mysql_options(mysql_data->lpDB, MYSQL_SECURE_AUTH, &nTemp);

  if (StrCompareA(szServerHostA, "shared-mem") == 0)
  {
    CHAR szPathA[4096];

    GetAnsiAppPath(szPathA, MX_ARRAYLEN(szPathA));
    fn_mysql_options(mysql_data->lpDB, MYSQL_PLUGIN_DIR, szPathA);

    fn_mysql_options(mysql_data->lpDB, MYSQL_SHARED_MEMORY_BASE_NAME, "MYSQL");

    nTemp = MYSQL_PROTOCOL_MEMORY;
    fn_mysql_options(mysql_data->lpDB, MYSQL_OPT_PROTOCOL, &nTemp);

    szServerHostA = "localhost";
    wServerPort = 0;
  }
  else if (StrCompareA(szServerHostA, "named-pipes") == 0)
  {
    CHAR szPathA[4096];

    GetAnsiAppPath(szPathA, MX_ARRAYLEN(szPathA));
    fn_mysql_options(mysql_data->lpDB, MYSQL_PLUGIN_DIR, szPathA);

    nTemp = MYSQL_PROTOCOL_PIPE;
    fn_mysql_options(mysql_data->lpDB, MYSQL_OPT_PROTOCOL, &nTemp);

    szServerHostA = ".";
    wServerPort = 0;
  }

  //do connection
  if (fn_mysql_real_connect(mysql_data->lpDB, szServerHostA, (szUserNameA != NULL) ? szUserNameA : "",
                                   (szUserPasswordA != NULL) ? szUserPasswordA : "", NULL, (UINT)wServerPort, NULL,
                                   CLIENT_LONG_FLAG | CLIENT_COMPRESS | CLIENT_LOCAL_FILES |
                                   CLIENT_PROTOCOL_41 | CLIENT_TRANSACTIONS | CLIENT_FOUND_ROWS) == NULL)
  {
on_error:
    mysql_data->SetErrno();
    hRes = mysql_data->GetHResult();

    //cleanup
    delete mysql_data;
    lpInternalData = NULL;

    //done
    return hRes; 
  }

  if (fn_mysql_set_character_set(mysql_data->lpDB, "utf8") != 0)
    goto on_error;

  //set database if any specified
  if (szDatabaseNameA != NULL && *szDatabaseNameA != 0)
  {
    if (fn_mysql_select_db(mysql_data->lpDB, szDatabaseNameA) != 0)
      goto on_error;
  }

  //at last, detect if server is using no backslashes escape

  //since we cannot check "mysql->server_status" SERVER_STATUS_NO_BACKSLASH_ESCAPES's flag
  //directly... make a real escape of an apostrophe
  nTemp = (int)fn_mysql_real_escape_string(mysql_data->lpDB, d, "\'", 1);
  mysql_data->nServerUsingNoBackslashEscapes = (nTemp < 2 || *d == '\\') ? 0 : 1;

  //done
  mysql_data->ClearErrno();
  return S_OK;
}

HRESULT CMySqlConnector::Connect(_In_z_ LPCWSTR szServerHostW, _In_z_ LPCWSTR szUserNameW,
                                 _In_opt_z_ LPCWSTR szUserPasswordW, _In_opt_z_ LPCWSTR szDatabaseNameW,
                                 _In_opt_ USHORT wServerPort, _In_opt_ CConnectOptions *lpOptions)
{
  CStringW cStrServerHostW, cStrUserNameW, cStrDatabaseNameW;
  CSecureStringW cStrUserPasswordW;

  if (szServerHostW == NULL || szUserNameW == NULL)
    return E_POINTER;
  if (*szServerHostW == 0 || *szUserNameW == 0)
    return E_INVALIDARG;

  if (cStrServerHostW.Copy(szServerHostW) == FALSE)
    return E_OUTOFMEMORY;
  if (cStrUserNameW.Copy(szUserNameW) == FALSE)
    return E_OUTOFMEMORY;
  if (szDatabaseNameW != NULL && *szDatabaseNameW != 0)
  {
    if (cStrDatabaseNameW.Copy(szDatabaseNameW) == FALSE)
      return E_OUTOFMEMORY;
  }
  if (szUserPasswordW != NULL && *szUserPasswordW != 0)
  {
    if (cStrUserPasswordW.Copy(szUserPasswordW) == FALSE)
      return E_OUTOFMEMORY;
  }
  return Connect((LPCWSTR)cStrServerHostW, (LPCWSTR)cStrUserNameW, (LPCWSTR)cStrUserPasswordW,
                 (LPCWSTR)cStrDatabaseNameW, wServerPort, lpOptions);
}

VOID CMySqlConnector::Disconnect()
{
  if (lpInternalData != NULL)
  {
    if (mysql_data->lpDB != NULL)
    {
      QueryClose();
      fn_mysql_close(mysql_data->lpDB);
    }

    delete mysql_data;
    lpInternalData = NULL;
  }

  //done
  return;
}

BOOL CMySqlConnector::IsConnected() const
{
  if (lpInternalData != NULL)
  {
    int err;

    if (fn_mysql_ping(mysql_data->lpDB) == 0)
      return TRUE;

    err = fn_mysql_errno(mysql_data->lpDB);
    const_cast<CMySqlConnector*>(this)->Disconnect();
  }
  return FALSE;
}

HRESULT CMySqlConnector::SelectDatabase(_In_ LPCSTR szDatabaseNameA)
{
  if (szDatabaseNameA == NULL)
    return E_POINTER;
  if (*szDatabaseNameA == 0)
    return E_INVALIDARG;

  if (lpInternalData == NULL)
    return MX_E_NotReady;
  QueryClose();

  //select database
  if (fn_mysql_select_db(mysql_data->lpDB, szDatabaseNameA) != 0)
  {
    mysql_data->SetErrno();

    if (mysql_data->nLastDbErr < ER_ERROR_FIRST || mysql_data->nLastDbErr > ER_ERROR_LAST)
    {
      Disconnect();
    }

    return mysql_data->GetHResult();
  }

  //done
  mysql_data->ClearErrno();
  return S_OK;
}

HRESULT CMySqlConnector::SelectDatabase(_In_ LPCWSTR szDatabaseNameW)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (szDatabaseNameW == NULL)
    return E_POINTER;
  if (*szDatabaseNameW == 0)
    return E_INVALIDARG;

  hRes = Utf8_Encode(cStrTempA, szDatabaseNameW);
  if (SUCCEEDED(hRes))
    hRes = SelectDatabase((LPCSTR)cStrTempA);
  return hRes;
}

int CMySqlConnector::GetErrorCode() const
{
  return (lpInternalData != NULL) ? mysql_data->nLastDbErr : 0;
}

LPCSTR CMySqlConnector::GetErrorDescription() const
{
  return (lpInternalData != NULL) ? (LPCSTR)(mysql_data->cStrLastDbErrorDescriptionA) : "";
}

LPCSTR CMySqlConnector::GetSqlState() const
{
  return (lpInternalData != NULL) ? mysql_data->szLastDbSqlStateA : "00000";
}

HRESULT CMySqlConnector::QueryExecute(_In_ LPCSTR szQueryA, _In_ SIZE_T nQueryLen, _In_ CFieldList *lpInputFieldsList)
{
  SIZE_T nParamsCount, nRetryCount;

  //validate arguments
  if (szQueryA == NULL)
    return E_POINTER;
  if (nQueryLen == (SIZE_T)-1)
    nQueryLen = StrLenA(szQueryA);
  while (nQueryLen > 0 && szQueryA[nQueryLen - 1] == ';')
    nQueryLen--;
  if (nQueryLen == 0)
    return E_INVALIDARG;

  if (lpInternalData == NULL)
    return MX_E_NotReady;

  nParamsCount = (lpInputFieldsList != NULL) ? lpInputFieldsList->GetCount() : 0;

  //execute query
  nRetryCount = 5;

try_again:
  QueryClose();

  mysql_data->lpStmt = fn_mysql_stmt_init(mysql_data->lpDB);
  if (mysql_data->lpStmt == NULL)
  {
err_nomem:
    QueryClose();
    mysql_data->SetCustomErrno(ER_OUTOFMEMORY, "Out of memory", "HY001");
    return E_OUTOFMEMORY;
  }

  if (fn_mysql_stmt_prepare(mysql_data->lpStmt, szQueryA, (ULONG)nQueryLen) != 0)
  {
    mysql_data->SetErrno();
    if (mysql_data->nLastDbErr == ER_UNSUPPORTED_PS)
    {
      //query does not support the prepared statement protocol so switch to a standard query
      QueryClose();

      //but the standard query does not support input bindings
      if (nParamsCount > 0)
      {
err_invalidarg:
        QueryClose();
        mysql_data->ClearErrno();
        return E_INVALIDARG;
      }

      if (fn_mysql_real_query(mysql_data->lpDB, szQueryA, (ULONG)nQueryLen) == 0)
      {
        mysql_data->lpResultSet = fn_mysql_store_result(mysql_data->lpDB);
        if (mysql_data->lpResultSet == NULL)
        {
          if (fn_mysql_errno(mysql_data->lpDB) != 0)
          {
            mysql_data->SetErrno();
            goto on_error;
          }
        }

        goto after_query;
      }

      mysql_data->SetErrno();
    }
    if (mysql_data->nLastDbErr != 0)
    {
on_error:
      if (nRetryCount > 1 && mysql_data->nLastDbErr == ER_CANT_LOCK)
      {
        nRetryCount--;
        ::Sleep(20);
        goto try_again;
      }

      QueryClose();

      if (mysql_data->nLastDbErr < ER_ERROR_FIRST || mysql_data->nLastDbErr > ER_ERROR_LAST)
      {
        Disconnect();
      }

      return mysql_data->GetHResult();
    }
  }

  //if it is a prepared statement...
  if (mysql_data->lpStmt != NULL)
  {
    //...process input fields
    if (nParamsCount != (SIZE_T)fn_mysql_stmt_param_count(mysql_data->lpStmt))
      goto err_invalidarg;

    if (nParamsCount > 0)
    {
      TAutoFreePtr<MYSQL_BIND> cInputBindings;
      CField *lpField;
      MYSQL_BIND *lpBind;
      SIZE_T nParamIdx, nSize;
      LPBYTE lpPtr;

      nSize = (SIZE_T)nParamsCount * sizeof(MYSQL_BIND);

      //calculate data size
      for (nParamIdx = 0; nParamIdx < nParamsCount; nParamIdx++)
      {
        lpField = lpInputFieldsList->GetElementAt(nParamIdx);
        switch (lpField->GetType())
        {
          case FieldTypeNull:
            nSize += sizeof(my_bool);
            break;

          case FieldTypeBoolean:
            nSize += sizeof(LONG);
            break;

          case FieldTypeUInt32:
          case FieldTypeInt32:
            nSize += sizeof(ULONG);
            break;

          case FieldTypeUInt64:
          case FieldTypeInt64:
            nSize += sizeof(ULONGLONG);
            break;

          case FieldTypeDouble:
            nSize += sizeof(double);
            break;

          case FieldTypeString:
          case FieldTypeBlob:
            break;

          case FieldTypeDateTime:
            nSize += sizeof(MYSQL_TIME);
            break;

          default:
            QueryClose();
            mysql_data->ClearErrno();
            return E_INVALIDARG;
        }
      }

      cInputBindings.Attach((MYSQL_BIND*)MX_MALLOC(nParamsCount * sizeof(MYSQL_BIND) + nSize));
      if (!cInputBindings)
        goto err_nomem;
      ::MxMemSet(cInputBindings.Get(), 0, nParamsCount * sizeof(MYSQL_BIND) + nSize);

      lpPtr = (LPBYTE)cInputBindings.Get() + nParamsCount * sizeof(MYSQL_BIND);
      lpBind = cInputBindings.Get();
      for (nParamIdx = 0; nParamIdx < nParamsCount; nParamIdx++, lpBind++)
      {
        lpField = lpInputFieldsList->GetElementAt(nParamIdx);
        lpBind->buffer = lpPtr;
        switch (lpField->GetType())
        {
          case FieldTypeNull:
            lpBind->buffer_type = MYSQL_TYPE_NULL;
            lpBind->is_null = (my_bool *)lpPtr;
            *(lpBind->is_null) = 1;

            lpPtr += sizeof(my_bool);
            break;

          case FieldTypeBoolean:
            lpBind->buffer_type = MYSQL_TYPE_LONG;
            lpBind->buffer_length = (ULONG)sizeof(LONG);
            *((LONG*)(lpBind->buffer)) = (lpField->GetBoolean() != FALSE) ? 1 : 0;

            lpPtr += sizeof(LONG);
            break;

          case FieldTypeUInt32:
          case FieldTypeInt32:
            lpBind->buffer_type = MYSQL_TYPE_LONG;
            lpBind->buffer_length = (ULONG)sizeof(LONG);
            if (lpField->GetType() == FieldTypeUInt32)
            {
              lpBind->is_unsigned = 1;
              *((ULONG*)(lpBind->buffer)) = lpField->GetUInt32();
            }
            else
            {
              *((LONG*)(lpBind->buffer)) = lpField->GetInt32();
            }

            lpPtr += sizeof(LONG);
            break;

          case FieldTypeUInt64:
          case FieldTypeInt64:
            lpBind->buffer_type = MYSQL_TYPE_LONGLONG;
            lpBind->buffer_length = (ULONG)sizeof(ULONGLONG);
            if (lpField->GetType() == FieldTypeUInt64)
            {
              lpBind->is_unsigned = 1;
              *((ULONGLONG*)(lpBind->buffer)) = lpField->GetUInt64();
            }
            else
            {
              *((LONGLONG*)(lpBind->buffer)) = lpField->GetInt64();
            }

            lpPtr += sizeof(LONGLONG);
            break;

          case FieldTypeDouble:
            lpBind->buffer_type = MYSQL_TYPE_DOUBLE;
            lpBind->buffer_length = (ULONG)sizeof(double);
            *((double*)(lpBind->buffer)) = lpField->GetDouble();

            lpPtr += sizeof(double);
            break;

          case FieldTypeString:
          case FieldTypeBlob:
            if (lpField->GetType() == FieldTypeString)
            {
              lpBind->buffer_type = MYSQL_TYPE_VAR_STRING;
              lpBind->buffer = (LPVOID)(lpField->GetString());
            }
            else
            {
              lpBind->buffer_type = MYSQL_TYPE_BLOB;
              lpBind->buffer = (LPVOID)(lpField->GetBlob());
            }
            lpBind->buffer_length = (ULONG)(lpField->GetLength());
            break;

          case FieldTypeDateTime:
            {
            int nYear, nMonth, nDay, nHours, nMinutes, nSeconds, nMilliSeconds;

            lpField->GetDateTime()->GetDateTime(&nYear, &nMonth, &nDay, &nHours, &nMinutes, &nSeconds,
                                                &nMilliSeconds);

            lpBind->buffer_type = MYSQL_TYPE_DATETIME;
            lpBind->buffer_length = (ULONG)sizeof(MYSQL_TIME);

            ((MYSQL_TIME*)lpPtr)->day = (UINT)nDay;
            ((MYSQL_TIME*)lpPtr)->month = (UINT)nMonth;
            ((MYSQL_TIME*)lpPtr)->year = (UINT)nYear;
            ((MYSQL_TIME*)lpPtr)->hour = (UINT)nHours;
            ((MYSQL_TIME*)lpPtr)->minute = (UINT)nMinutes;
            ((MYSQL_TIME*)lpPtr)->second = (UINT)nSeconds;
            ((MYSQL_TIME*)lpPtr)->second_part = (ULONG)nMilliSeconds;
            ((MYSQL_TIME*)lpPtr)->neg = 0;
            ((MYSQL_TIME*)lpPtr)->time_type = MYSQL_TIMESTAMP_DATETIME;

            lpPtr += sizeof(MYSQL_TIME);
            }
            break;
        }
      }

      if (fn_mysql_stmt_bind_param(mysql_data->lpStmt, cInputBindings.Get()) != 0)
      {
        mysql_data->SetErrno();
        goto on_error;
      }
    }

    //...execute statement
    if (fn_mysql_stmt_execute(mysql_data->lpStmt) != 0)
    {
      mysql_data->SetErrno();
      goto on_error;
    }

    //analyze result set (if any)
    mysql_data->lpResultSet = fn_mysql_stmt_result_metadata(mysql_data->lpStmt);
    if (mysql_data->lpResultSet == NULL)
    {
      if (fn_mysql_stmt_errno(mysql_data->lpStmt) != 0)
      {
        mysql_data->SetErrno();
        goto on_error;
      }
    }
  }

after_query:

  //get some data
  if (mysql_data->lpStmt != NULL)
  {
    ullAffectedRows = fn_mysql_stmt_affected_rows(mysql_data->lpStmt);
    ullLastInsertId = fn_mysql_stmt_insert_id(mysql_data->lpStmt);
  }
  else
  {
    ullAffectedRows = fn_mysql_affected_rows(mysql_data->lpDB);
    ullLastInsertId = fn_mysql_insert_id(mysql_data->lpDB);
  }

  //get last insert id and initialize result-set fields
  if (mysql_data->lpResultSet != NULL)
  {
    SIZE_T nFieldsCount;

    nFieldsCount = (SIZE_T)fn_mysql_num_fields(mysql_data->lpResultSet);
    if (nFieldsCount > 0)
    {
      MYSQL_FIELD *lpFields;
      MYSQL_BIND *lpBind = NULL;
      ULONG *lpLengthPtr = NULL;
      my_bool *lpErrorPtr = NULL;
      my_bool *lpNulPtr = NULL;

      //allocate buffers for output bindings on prepared statements
      if (mysql_data->lpStmt != NULL)
      {
        //allocate for result bindings
        mysql_data->cOutputBindings.Attach((MYSQL_BIND*)MX_MALLOC(nFieldsCount * (sizeof(MYSQL_BIND) + sizeof(ULONG) +
                                                                                  sizeof(my_bool) + sizeof(my_bool))));
        if (!(mysql_data->cOutputBindings))
          goto err_nomem;
        ::MxMemSet(mysql_data->cOutputBindings.Get(), 0, nFieldsCount * (sizeof(MYSQL_BIND) + sizeof(ULONG) +
                                                                         sizeof(my_bool) + sizeof(my_bool)));

        lpBind = mysql_data->cOutputBindings.Get();
        lpLengthPtr = (ULONG*)(lpBind + nFieldsCount);
        lpErrorPtr = (my_bool*)(lpLengthPtr + nFieldsCount);
        lpNulPtr = (my_bool*)(lpErrorPtr + nFieldsCount);
      }

      //build fields
      lpFields = fn_mysql_fetch_fields(mysql_data->lpResultSet);
      for (SIZE_T i = 0; i < nFieldsCount; i++)
      {
        TAutoDeletePtr<CMySqlColumn> cColumn;

        cColumn.Attach(MX_DEBUG_NEW CMySqlColumn());
        if (!(cColumn && cColumn->cField))
          goto err_nomem;

        //name
        if (cColumn->cStrNameA.Copy(lpFields[i].name) == FALSE)
          goto err_nomem;

        //table
        if (lpFields[i].table != NULL)
        {
          if (cColumn->cStrTableA.Copy(lpFields[i].table) == FALSE)
            goto err_nomem;
        }

        //flags
        cColumn->nFlags = lpFields[i].flags;

        //type
        cColumn->nType = lpFields[i].type;

        //setup output bindings for prepared statements
        if (mysql_data->lpStmt != NULL)
        {
          lpBind[i].buffer_type = lpFields[i].type;
          lpBind[i].length = lpLengthPtr + i;
          lpBind[i].error = lpErrorPtr + i;
          lpBind[i].is_null = lpNulPtr + i;
          lpBind[i].is_unsigned = ((lpFields[i].flags & UNSIGNED_FLAG) != 0) ? 1 : 0;

          if ((lpFields[i].flags & (BLOB_FLAG | BINARY_FLAG)) == (BLOB_FLAG | BINARY_FLAG))
          {
            lpBind[i].buffer_type = MYSQL_TYPE_BLOB;
          }
          else switch (lpFields[i].type)
          {
            case MYSQL_TYPE_TINY_BLOB:
            case MYSQL_TYPE_MEDIUM_BLOB:
            case MYSQL_TYPE_LONG_BLOB:
            case MYSQL_TYPE_BLOB:
              break;

            case MYSQL_TYPE_TINY:
            case MYSQL_TYPE_SHORT:
            case MYSQL_TYPE_LONG:
            case MYSQL_TYPE_INT24:
            case MYSQL_TYPE_YEAR:
              lpBind[i].buffer_type = MYSQL_TYPE_LONG;
              lpBind[i].buffer_length = sizeof(LONG);
              break;

            case MYSQL_TYPE_LONGLONG:
              lpBind[i].buffer_length = sizeof(LONGLONG);
              break;

            case MYSQL_TYPE_BIT:
              lpBind[i].buffer_type = MYSQL_TYPE_LONGLONG;
              lpBind[i].buffer_length = sizeof(LONGLONG);
              break;

            case MYSQL_TYPE_FLOAT:
              lpBind[i].buffer_length = sizeof(float);
              break;

            case MYSQL_TYPE_DOUBLE:
            case MYSQL_TYPE_DECIMAL:
            case MYSQL_TYPE_NEWDECIMAL:
              lpBind[i].buffer_type = MYSQL_TYPE_DOUBLE;
              lpBind[i].buffer_length = sizeof(double);
              break;

            case MYSQL_TYPE_TIMESTAMP:
            case MYSQL_TYPE_DATETIME:
              lpBind[i].buffer_type = MYSQL_TYPE_DATETIME;
              lpBind[i].buffer_length = sizeof(MYSQL_TIME);
              break;

            case MYSQL_TYPE_DATE:
            case MYSQL_TYPE_TIME:
              lpBind[i].buffer_length = sizeof(MYSQL_TIME);
              break;

            case MYSQL_TYPE_NEWDATE:
              lpBind[i].buffer_type = MYSQL_TYPE_DATE;
              lpBind[i].buffer_length = sizeof(MYSQL_TIME);
              break;

            case MYSQL_TYPE_ENUM:
            case MYSQL_TYPE_VARCHAR:
            case MYSQL_TYPE_VAR_STRING:
            case MYSQL_TYPE_STRING:
            case MYSQL_TYPE_SET:
            case MYSQL_TYPE_JSON:
              lpBind[i].buffer_type = MYSQL_TYPE_VAR_STRING;
              break;

            case MYSQL_TYPE_NULL:
              break;

            default:
              lpBind[i].buffer_type = MYSQL_TYPE_NULL;
              break;
          }
        }

        //add to columns list
        if (aColumnsList.AddElement(cColumn.Get()) == FALSE)
          goto err_nomem;
        cColumn.Detach();

        //pre-allocate output buffer
        if (mysql_data->lpStmt != NULL)
        {
          Internals::CMySqlConnectorData::CBuffer *lpBuffer;

          lpBuffer = MX_DEBUG_NEW Internals::CMySqlConnectorData::CBuffer();
          if (lpBuffer == NULL)
            goto err_nomem;
          if (mysql_data->aOutputBuffersList.AddElement(lpBuffer) == FALSE)
          {
            delete lpBuffer;
            goto err_nomem;
          }
          if (lpBind[i].buffer_length > 0)
          {
            if (lpBuffer->EnsureSize((SIZE_T)(lpBind[i].buffer_length)) == FALSE)
              goto err_nomem;
            lpBind[i].buffer = lpBuffer->lpBuffer;
          }
        }
      }

      //bind output
      if (mysql_data->lpStmt != NULL)
      {
        if (fn_mysql_stmt_bind_result(mysql_data->lpStmt, mysql_data->cOutputBindings.Get()) != 0)
        {
          mysql_data->SetErrno();
          goto on_error;
        }
      }
    }
  }

  //done
  return S_OK;
}

HRESULT CMySqlConnector::FetchRow()
{
  CMySqlColumn *lpColumn;
  SIZE_T i, nColumnsCount;
  int err;
  BYTE aTempBuf[8];
  LPCSTR sA;
  LPSTR szEndPosA;
  union {
    ULONG ul;
    LONG l;
    ULONGLONG ull;
    LONGLONG ll;
    double dbl;
  } u;
  CDateTime cDt;
  HRESULT hRes;

  if (lpInternalData == NULL)
    return MX_E_NotReady;

  if (mysql_data->lpResultSet == NULL)
  {
    mysql_data->ClearErrno();
    QueryClose();
    return S_FALSE;
  }

  nColumnsCount = aColumnsList.GetCount();
  if (nColumnsCount == 0)
  {
    mysql_data->ClearErrno();
    QueryClose();
    return S_FALSE;
  }

  if (mysql_data->lpStmt == NULL)
  {
    MYSQL_ROW lpRow;
    ULONG *lpnRowLengths;

    lpRow = fn_mysql_fetch_row(mysql_data->lpResultSet);
    if (lpRow == NULL)
    {
      err = fn_mysql_errno(mysql_data->lpDB);
      if (err != 0)
      {
        mysql_data->SetErrno();
        QueryClose();

        if (mysql_data->nLastDbErr < ER_ERROR_FIRST || mysql_data->nLastDbErr > ER_ERROR_LAST)
        {
          Disconnect();
        }

        return mysql_data->GetHResult();
      }

      //done
      mysql_data->ClearErrno();
      QueryClose();
      return S_FALSE;
    }

    lpnRowLengths = fn_mysql_fetch_lengths(mysql_data->lpResultSet);
    if (lpnRowLengths == NULL)
    {
err_nomem:
      mysql_data->SetCustomErrno(ER_OUTOFMEMORY, "Out of memory", "HY001");
      QueryClose();
      return E_OUTOFMEMORY;
    }

    for (i = 0; i < nColumnsCount; i++)
    {
      lpColumn = (CMySqlColumn*)(aColumnsList.GetElementAt(i));

      if (lpRow[i] != NULL)
      {
        sA = lpRow[i];
        switch (lpColumn->nType)
        {
          case MYSQL_TYPE_BIT:
            u.ull = 0;
            if (lpnRowLengths[i] >= 1 && lpnRowLengths[i] <= 8)
            {
              ::MxMemSet(aTempBuf, 0, 8);
              ::MxMemCopy(aTempBuf, lpRow[i], lpnRowLengths[i]);
              if ((lpColumn->nFlags & UNSIGNED_FLAG) != 0)
              {
                u.ull = *((ULONGLONG*)aTempBuf); //SAFE because MySQL is little endian
              }
              else
              {
                if ((aTempBuf[lpnRowLengths[i] - 1] & 0x80) != 0)
                  ::MxMemSet(aTempBuf + lpnRowLengths[i], 0xFF, 8 - lpnRowLengths[i]);
                u.ll = *((LONGLONG*)aTempBuf); //SAFE because MySQL is little endian
              }
            }
            if ((lpColumn->nFlags & UNSIGNED_FLAG) != 0)
              lpColumn->cField->SetUInt64(u.ull);
            else
              lpColumn->cField->SetInt64(u.ll);
            break;

          case MYSQL_TYPE_TINY:
          case MYSQL_TYPE_SHORT:
          case MYSQL_TYPE_LONG:
          case MYSQL_TYPE_INT24:
          case MYSQL_TYPE_YEAR:
            if ((lpColumn->nFlags & UNSIGNED_FLAG) != 0)
            {
              u.ul = strtoul(sA, &szEndPosA, 10);
              lpColumn->cField->SetUInt32(u.ul);
            }
            else
            {
              u.l = strtol(sA, &szEndPosA, 10);
              lpColumn->cField->SetInt32(u.l);
            }
            break;

          case MYSQL_TYPE_LONGLONG:
            if ((lpColumn->nFlags & UNSIGNED_FLAG) != 0)
            {
              u.ull = _strtoui64(sA, &szEndPosA, 10);
              lpColumn->cField->SetUInt64(u.ull);
            }
            else
            {
              u.ll = _strtoi64(sA, &szEndPosA, 10);
              lpColumn->cField->SetInt64(u.ll);
            }
            break;

          case MYSQL_TYPE_FLOAT:
          case MYSQL_TYPE_DECIMAL:
          case MYSQL_TYPE_DOUBLE:
          case MYSQL_TYPE_NEWDECIMAL:
            hRes = StrToDoubleA(sA, (SIZE_T)-1, &(u.dbl));
            if (FAILED(hRes))
            {
on_error:     if (hRes == E_OUTOFMEMORY)
                goto err_nomem;
              mysql_data->ClearErrno();
              QueryClose();
              return hRes;
            }
            lpColumn->cField->SetDouble(u.dbl);
            break;

          case MYSQL_TYPE_NULL:
            lpColumn->cField->SetNull();
            break;

          case MYSQL_TYPE_DATE:
          case MYSQL_TYPE_NEWDATE:
            //"YYYY-MM-DD"
            if (lpnRowLengths[i] != 10)
            {
err_invdata:  mysql_data->ClearErrno();
              QueryClose();
              return MX_E_InvalidData;
            }

            if (MX::StrNCompareA(sA, "0000-00-00", 10) == 0)
            {
              lpColumn->cField->SetNull();
              break;
            }

            if (sA[0] < '0' || sA[0] > '9' || sA[1] < '0' || sA[1] > '9' ||
                sA[2] < '0' || sA[2] > '9' || sA[3] < '0' || sA[3] > '9' ||
                sA[4] != '-' ||
                sA[5] < '0' || sA[5] > '9' || sA[6] < '0' || sA[6] > '9' ||
                sA[7] != '-' ||
                sA[8] < '0' || sA[8] > '9' || sA[9] < '0' || sA[9] > '9')
            {
              goto err_invdata;
            }

            hRes = cDt.SetDateTime((int)(sA[0] - '0') * 1000 + (int)(sA[1] - '0') * 100 + (int)(sA[2] - '0') * 10 +
                                   (int)(sA[3] - '0'),
                                   (int)(sA[5] - '0') * 10 + (int)(sA[6] - '0'),
                                   (int)(sA[8] - '0') * 10 + (int)(sA[9] - '0'),
                                   0, 0, 0, 0);
            if (SUCCEEDED(hRes))
              hRes = lpColumn->cField->SetDateTime(cDt);
            if (FAILED(hRes))
              goto on_error;
            break;

          case MYSQL_TYPE_TIME:
            //"HH:MM:SS[.ffff]"
            if (lpnRowLengths[i] < 8)
              goto err_invdata;

            if (sA[0] < '0' || sA[0] > '9' || sA[1] < '0' || sA[1] > '9' ||
                sA[2] != ':' ||
                sA[3] < '0' || sA[3] > '9' || sA[4] < '0' || sA[4] > '9' ||
                sA[5] != ':' ||
                sA[6] < '0' || sA[6] > '9' || sA[7] < '0' || sA[7] > '9')
            {
              goto err_invdata;
            }

            u.l = 0;
            if (lpnRowLengths[i] >= 9)
            {
              if (sA[8] != '.')
                goto err_invdata;

              for (SIZE_T k = 9; k < 12 && k < lpnRowLengths[i]; k++)
              {
                if (sA[k] < '0' || sA[k] > '9')
                  break;
                u.l += (int)(sA[k] - '0') * ((k == 9) ? 100 : ((k == 10) ? 10 : 1));
              }
            }

            hRes = cDt.SetFromNow(FALSE);
            if (SUCCEEDED(hRes))
            {
              hRes = cDt.SetTime((int)(sA[0] - '0') * 10 + (int)(sA[1] - '0'),
                                 (int)(sA[3] - '0') * 10 + (int)(sA[4] - '0'),
                                 (int)(sA[6] - '0') * 10 + (int)(sA[7] - '0'),
                                 (int)(u.l));
            }
            if (SUCCEEDED(hRes))
              hRes = lpColumn->cField->SetDateTime(cDt);
            if (FAILED(hRes))
              goto on_error;
            break;

          case MYSQL_TYPE_TIMESTAMP:
          case MYSQL_TYPE_DATETIME:
            //"YYYY-MM-DD HH:MM:SS[.ffff]"
            if (lpnRowLengths[i] < 19)
              goto err_invdata;

            if (sA[0] < '0' || sA[0] > '9' || sA[1] < '0' || sA[1] > '9' ||
                sA[2] < '0' || sA[2] > '9' || sA[3] < '0' || sA[3] > '9' ||
                sA[4] != '-' ||
                sA[5] < '0' || sA[5] > '9' || sA[6] < '0' || sA[6] > '9' ||
                sA[7] != '-' ||
                sA[8] < '0' || sA[8] > '9' || sA[9] < '0' || sA[9] > '9' ||
                sA[10] != ' ' ||
                sA[11] < '0' || sA[11] > '9' || sA[12] < '0' || sA[12] > '9' ||
                sA[13] != ':' ||
                sA[14] < '0' || sA[14] > '9' || sA[15] < '0' || sA[15] > '9' ||
                sA[16] != ':' ||
                sA[17] < '0' || sA[17] > '9' || sA[18] < '0' || sA[18] > '9')
            {
              goto err_invdata;
            }

            u.l = 0;
            if (lpnRowLengths[i] >= 20)
            {
              if (sA[19] == '.')
                goto err_invdata;
              for (SIZE_T k = 20; k < 23 && k < lpnRowLengths[i]; k++)
              {
                if (sA[k] < '0' || sA[k] > '9')
                  break;
                u.l += (int)(sA[k] - '0') * ((k == 20) ? 100 : ((k == 21) ? 10 : 1));
              }
            }

            hRes = cDt.SetDateTime((int)(sA[0] - '0') * 1000 + (int)(sA[1] - '0') * 100 + (int)(sA[2] - '0') * 10 +
                                   (int)(sA[3] - '0'),
                                   (int)(sA[5] - '0') * 10 + (int)(sA[6] - '0'),
                                   (int)(sA[8] - '0') * 10 + (int)(sA[9] - '0'),
                                   (int)(sA[11] - '0') * 10 + (int)(sA[12] - '0'),
                                   (int)(sA[14] - '0') * 10 + (int)(sA[15] - '0'),
                                   (int)(sA[17] - '0') * 10 + (int)(sA[18] - '0'),
                                   (int)(u.l));
            if (SUCCEEDED(hRes))
              hRes = lpColumn->cField->SetDateTime(cDt);
            if (FAILED(hRes))
              goto on_error;
            break;

          case MYSQL_TYPE_TINY_BLOB:
          case MYSQL_TYPE_MEDIUM_BLOB:
          case MYSQL_TYPE_LONG_BLOB:
          case MYSQL_TYPE_BLOB:
            if ((lpColumn->nFlags & BINARY_FLAG) != 0)
            {
              hRes = lpColumn->cField->SetBlob(lpRow[i], (SIZE_T)(lpnRowLengths[i]));
              if (FAILED(hRes))
                goto on_error;
              break;
            }
            //fall into default string handling if not a binary blob (a.k.a. TEXT)

          //case MYSQL_TYPE_ENUM:
          //case MYSQL_TYPE_SET:
          //case MYSQL_TYPE_VARCHAR:
          //case MYSQL_TYPE_VAR_STRING:
          //case MYSQL_TYPE_STRING:
          //case MYSQL_TYPE_GEOMETRY:
          default:
            hRes = lpColumn->cField->SetString((LPCSTR)lpRow[i], (SIZE_T)lpnRowLengths[i]);
            if (FAILED(hRes))
              goto on_error;
            break;
        }
      }
      else
      {
        lpColumn->cField->SetNull();
      }
    }
  }
  else
  {
    MYSQL_BIND *lpBind;
    ULONG *lpLengthPtr;
    my_bool *lpErrorPtr, *lpNulPtr;
    Internals::CMySqlConnectorData::CBuffer *lpBuffer;

    //set result bind buffers length to 0 before fetching
    lpBind = mysql_data->cOutputBindings.Get();
    lpLengthPtr = (ULONG*)(lpBind + nColumnsCount);
    lpErrorPtr = (my_bool*)(lpLengthPtr + nColumnsCount);
    lpNulPtr = (my_bool*)(lpErrorPtr + nColumnsCount);

    //fetch next row
    err = fn_mysql_stmt_fetch(mysql_data->lpStmt);
    if (err == MYSQL_NO_DATA)
    {
      mysql_data->ClearErrno();
      QueryClose();
      return S_FALSE;
    }

    if (err != 0 && err != MYSQL_DATA_TRUNCATED)
    {
      mysql_data->SetErrno();
      QueryClose();

      if (mysql_data->nLastDbErr < ER_ERROR_FIRST || mysql_data->nLastDbErr > ER_ERROR_LAST)
      {
        Disconnect();
      }

      return mysql_data->GetHResult();
    }

    //ensure enough buffer size in the field and fetch truncated columns
    for (i = 0; i < nColumnsCount; i++)
    {
      if (lpErrorPtr[i] != 0 || lpLengthPtr[i] > lpBind[i].buffer_length)
      {
        lpBuffer = mysql_data->aOutputBuffersList.GetElementAt(i);
        if (lpBuffer->EnsureSize((SIZE_T)lpLengthPtr[i]) == FALSE)
          goto err_nomem;
        lpBind[i].buffer = lpBuffer->lpBuffer;

        lpBind[i].buffer_length = lpLengthPtr[i];

        err = fn_mysql_stmt_fetch_column(mysql_data->lpStmt, &lpBind[i], (unsigned int)i, 0);
        if (err != 0)
        {
          mysql_data->SetErrno();
          QueryClose();

          if (mysql_data->nLastDbErr < ER_ERROR_FIRST || mysql_data->nLastDbErr > ER_ERROR_LAST)
          {
            Disconnect();
          }

          return mysql_data->GetHResult();
        }
      }
    }

    for (i = 0; i < nColumnsCount; i++)
    {
      lpColumn = (CMySqlColumn*)(aColumnsList.GetElementAt(i));

      if (lpNulPtr[i] == 0)
      {
        switch (lpBind[i].buffer_type)
        {
          case MYSQL_TYPE_BIT:
            u.ull = 0;
            if (lpLengthPtr[i] >= 1 && lpLengthPtr[i] <= 8)
            {
              ::MxMemSet(aTempBuf, 0, 8);
              ::MxMemCopy(aTempBuf, lpBind[i].buffer, lpLengthPtr[i]);
              if ((lpColumn->nFlags & UNSIGNED_FLAG) != 0)
              {
                u.ull = *((ULONGLONG*)aTempBuf); //SAFE because MySQL is little endian
              }
              else
              {
                if ((aTempBuf[lpLengthPtr[i] - 1] & 0x80) != 0)
                  ::MxMemSet(aTempBuf + lpLengthPtr[i], 0xFF, 8 - lpLengthPtr[i]);
                u.ll = *((LONGLONG*)aTempBuf); //SAFE because MySQL is little endian
              }
            }
            if ((lpColumn->nFlags & UNSIGNED_FLAG) != 0)
              lpColumn->cField->SetUInt64(u.ull);
            else
              lpColumn->cField->SetInt64(u.ll);
            break;

          case MYSQL_TYPE_TINY:
          case MYSQL_TYPE_SHORT:
          case MYSQL_TYPE_LONG:
          case MYSQL_TYPE_INT24:
          case MYSQL_TYPE_YEAR:
            if ((lpColumn->nFlags & UNSIGNED_FLAG) != 0)
            {
              lpColumn->cField->SetUInt32(*((ULONG*)(lpBind[i].buffer)));
            }
            else
            {
              lpColumn->cField->SetInt32(*((LONG*)(lpBind[i].buffer)));
            }
            break;

          case MYSQL_TYPE_LONGLONG:
            if ((lpColumn->nFlags & UNSIGNED_FLAG) != 0)
            {
              lpColumn->cField->SetUInt64(*((ULONGLONG*)(lpBind[i].buffer)));
            }
            else
            {
              lpColumn->cField->SetInt64(*((LONGLONG*)(lpBind[i].buffer)));
            }
            break;

          case MYSQL_TYPE_FLOAT:
            lpColumn->cField->SetDouble(*((float*)(lpBind[i].buffer)));
            break;

          case MYSQL_TYPE_DECIMAL:
          case MYSQL_TYPE_DOUBLE:
          case MYSQL_TYPE_NEWDECIMAL:
            lpColumn->cField->SetDouble(*((double*)(lpBind[i].buffer)));
            break;

          case MYSQL_TYPE_DATE:
          case MYSQL_TYPE_TIME:
          case MYSQL_TYPE_DATETIME:
            {
            MYSQL_TIME *lpMySqlTime = (MYSQL_TIME*)(lpBind[i].buffer);

            if (lpBind[i].buffer_type == MYSQL_TYPE_TIME)
            {
              hRes = cDt.SetFromNow(FALSE);
              if (SUCCEEDED(hRes))
              {
                hRes = cDt.SetTime((int)(lpMySqlTime->hour), (int)(lpMySqlTime->minute),
                                   (int)(lpMySqlTime->second), (int)(lpMySqlTime->second_part));
              }
              if (SUCCEEDED(hRes))
              {
                hRes = lpColumn->cField->SetDateTime(cDt);
              }
              if (FAILED(hRes))
                goto on_error;
            }
            else if (lpMySqlTime->month != 0 && lpMySqlTime->day != 0)
            {
              if (lpBind[i].buffer_type == MYSQL_TYPE_DATETIME)
              {
                hRes = cDt.SetDateTime((int)(lpMySqlTime->year), (int)(lpMySqlTime->month), (int)(lpMySqlTime->day),
                                       (int)(lpMySqlTime->hour), (int)(lpMySqlTime->minute), (int)(lpMySqlTime->second),
                                       (int)(lpMySqlTime->second_part));
              }
              else
              {
                hRes = cDt.SetDateTime((int)(lpMySqlTime->year), (int)(lpMySqlTime->month), (int)(lpMySqlTime->day),
                                       0, 0, 0, 0);
              }
              if (SUCCEEDED(hRes))
              {
                hRes = lpColumn->cField->SetDateTime(cDt);
              }
              if (FAILED(hRes))
                goto on_error;
            }
            else
            {
              lpColumn->cField->SetNull();
            }
            }
            break;

          case MYSQL_TYPE_NULL:
            lpColumn->cField->SetNull();
            break;

          case MYSQL_TYPE_TINY_BLOB:
          case MYSQL_TYPE_MEDIUM_BLOB:
          case MYSQL_TYPE_LONG_BLOB:
          case MYSQL_TYPE_BLOB:
          case MYSQL_TYPE_VAR_STRING:
            if ((lpColumn->nFlags & BINARY_FLAG) != 0)
            {
              hRes = lpColumn->cField->SetBlob(lpBind[i].buffer, (SIZE_T)lpLengthPtr[i]);
              if (FAILED(hRes))
                goto on_error;
            }
            else
            {
              hRes = lpColumn->cField->SetString((LPCSTR)(lpBind[i].buffer), (SIZE_T)lpLengthPtr[i]);
              if (FAILED(hRes))
                goto on_error;
            }
            break;

          default:
            lpColumn->cField->SetNull();
            break;
        }
      }
      else
      {
        lpColumn->cField->SetNull();
      }
    }
  }

  //done
  mysql_data->ClearErrno();
  return S_OK;
}

VOID CMySqlConnector::QueryClose()
{
  if (lpInternalData != NULL)
  {
    if (mysql_data->lpStmt != NULL)
    {
      fn_mysql_stmt_close(mysql_data->lpStmt);
      mysql_data->lpStmt = NULL;
    }
    if (mysql_data->lpResultSet != NULL)
    {
      fn_mysql_free_result(mysql_data->lpResultSet);
      mysql_data->lpResultSet = NULL;
    }
    mysql_data->aOutputBuffersList.RemoveAllElements();
    mysql_data->cOutputBindings.Reset();
  }

  //call super
  __super::QueryClose();

  //done
  return;
}

HRESULT CMySqlConnector::TransactionStart()
{
  return TransactionStart(FALSE);
}

HRESULT CMySqlConnector::TransactionStart(_In_ BOOL bReadOnly)
{
  if (bReadOnly == FALSE)
    return QueryExecute("START TRANSACTION READ WRITE;", 29);
  return QueryExecute("START TRANSACTION READ ONLY;", 28);
}

HRESULT CMySqlConnector::TransactionCommit()
{
  return QueryExecute("COMMIT;", 7);
}

HRESULT CMySqlConnector::TransactionRollback()
{
  return QueryExecute("ROLLBACK;", 9);
}

HRESULT CMySqlConnector::EscapeString(_Out_ CStringA &cStrA, _In_ LPCSTR szStrA, _In_opt_ SIZE_T nStrLen,
                                      _In_opt_ BOOL bIsLike)
{
  CStringA cStrTempA;
  LPCSTR szStrEndA;

  if (nStrLen == (SIZE_T)-1)
    nStrLen = StrLenA(szStrA);
  if (nStrLen == 0)
    return S_OK;

  if (lpInternalData == NULL)
    return MX_E_NotReady;

  //full escaping... since we are converting strings to utf-8, we don't need to
  //check for special multibyte characters because utf-8 multibyte chars are
  //always >= 0x80 and doesn't conflict with the backslash (0x5C)
  szStrEndA = szStrA + nStrLen;
  while (szStrA < szStrEndA)
  {
    LPCSTR szStartA = szStrA;

    while (szStrA < szStrEndA && *szStrA != '\'' && *szStrA != '"' && *szStrA != '\r' && *szStrA != '\n' &&
           *szStrA != '\032' && *szStrA != 0 && *szStrA != '\\')
    {
      szStrA++;
    }
    if (szStrA > szStartA)
    {
      if (cStrTempA.ConcatN(szStartA, (SIZE_T)(szStrA - szStartA)) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (szStrA < szStrEndA)
    {
      switch (*szStrA)
      {
        case '\'':
          if (mysql_data->nServerUsingNoBackslashEscapes != 0)
          {
            if (cStrTempA.ConcatN("''", 2) == FALSE)
              return E_OUTOFMEMORY;
            break;
          }
          //fall into next...

        case '"':
          if (cStrTempA.AppendFormat("\\%c", *szStrA) == FALSE)
            return E_OUTOFMEMORY;
          break;

        case '\r':
          if (mysql_data->nServerUsingNoBackslashEscapes != 0)
          {
copy_char:  if (cStrTempA.ConcatN(szStrA, 1) == FALSE)
              return E_OUTOFMEMORY;
          }
          else
          {
            if (cStrTempA.ConcatN("\\r", 2) == FALSE)
              return E_OUTOFMEMORY;
          }
          break;

        case '\n':
          if (mysql_data->nServerUsingNoBackslashEscapes != 0)
            goto copy_char;
          if (cStrTempA.ConcatN("\\n", 2) == FALSE)
            return E_OUTOFMEMORY;
          break;

        case '\032':
          if (mysql_data->nServerUsingNoBackslashEscapes != 0)
            goto copy_char;
          if (cStrTempA.ConcatN("\\Z", 2) == FALSE)
            return E_OUTOFMEMORY;
          break;

        case 0:
          if (cStrTempA.ConcatN("\\0", 2) == FALSE)
            return E_OUTOFMEMORY;
          break;

        case '\\':
          if (bIsLike != FALSE && szStrA + 1 < szStrEndA && (szStrA[1] == '%' || szStrA[1] == '_'))
          {
            szStrA++;
            if (cStrTempA.AppendFormat("\\%c", *szStrA) == FALSE)
              return E_OUTOFMEMORY;
          }
          else
          {
            if (cStrTempA.ConcatN("\\\\", 2) == FALSE)
              return E_OUTOFMEMORY;
          }
          break;
      }
      szStrA++;
    }
  }

  //done
  cStrA.Attach(cStrTempA.Detach());
  return S_OK;
}

HRESULT CMySqlConnector::EscapeString(_Out_ CStringW &cStrW, _In_ LPCWSTR szStrW, _In_opt_ SIZE_T nStrLen,
                                      _In_opt_ BOOL bIsLike)
{
  CStringW cStrTempW;
  LPCWSTR szStrEndW;

  if (nStrLen == (SIZE_T)-1)
    nStrLen = StrLenW(szStrW);
  if (nStrLen == 0)
    return S_OK;

  if (lpInternalData == NULL)
    return MX_E_NotReady;

  //full escaping... since we are converting strings to utf-8, we don't need to
  //check for special multibyte characters because utf-8 multibyte chars are
  //always >= 0x80 and doesn't conflict with the backslash (0x5C)
  szStrEndW = szStrW + nStrLen;
  while (szStrW < szStrEndW)
  {
    LPCWSTR szStartW = szStrW;

    while (szStrW < szStrEndW && *szStrW != L'\'' && *szStrW != L'"' && *szStrW != L'\r' && *szStrW != L'\n' &&
           *szStrW != L'\032' && *szStrW != 0 && *szStrW != L'\\')
    {
      szStrW++;
    }
    if (szStrW > szStartW)
    {
      if (cStrTempW.ConcatN(szStartW, (SIZE_T)(szStrW - szStartW)) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (szStrW < szStrEndW)
    {
      switch (*szStrW)
      {
        case L'\'':
          if (mysql_data->nServerUsingNoBackslashEscapes != 0)
          {
            if (cStrTempW.ConcatN(L"''", 2) == FALSE)
              return E_OUTOFMEMORY;
            break;
          }
          //fall into next...

        case L'"':
          if (cStrTempW.AppendFormat(L"\\%c", *szStrW) == FALSE)
            return E_OUTOFMEMORY;
          break;

        case L'\r':
          if (mysql_data->nServerUsingNoBackslashEscapes != 0)
          {
copy_char:  if (cStrTempW.ConcatN(szStrW, 1) == FALSE)
              return E_OUTOFMEMORY;
          }
          else
          {
            if (cStrTempW.ConcatN(L"\\r", 2) == FALSE)
              return E_OUTOFMEMORY;
          }
          break;

        case L'\n':
          if (mysql_data->nServerUsingNoBackslashEscapes != 0)
            goto copy_char;
          if (cStrTempW.ConcatN(L"\\n", 2) == FALSE)
            return E_OUTOFMEMORY;
          break;

        case L'\032':
          if (mysql_data->nServerUsingNoBackslashEscapes != 0)
            goto copy_char;
          if (cStrTempW.ConcatN(L"\\Z", 2) == FALSE)
            return E_OUTOFMEMORY;
          break;

        case 0:
          if (cStrTempW.ConcatN(L"\\0", 2) == FALSE)
            return E_OUTOFMEMORY;
          break;

        case L'\\':
          if (bIsLike != FALSE && szStrW + 1 < szStrEndW && (szStrW[1] == L'%' || szStrW[1] == L'_'))
          {
            szStrW++;
            if (cStrTempW.AppendFormat(L"\\%c", *szStrW) == FALSE)
              return E_OUTOFMEMORY;
          }
          else
          {
            if (cStrTempW.ConcatN(L"\\\\", 2) == FALSE)
              return E_OUTOFMEMORY;
          }
          break;
      }
      szStrW++;
    }
  }

  //done
  cStrW.Attach(cStrTempW.Detach());
  return S_OK;
}

//-----------------------------------------------------------

CMySqlConnector::CConnectOptions::CConnectOptions() : CBaseMemObj()
{
  dwConnectTimeoutMs = 30000;
  dwReadTimeoutMs = dwWriteTimeoutMs = 45000;
  return;
}

} //namespace Database

} //namespace MX

//-----------------------------------------------------------

#define CLEAR_API(x) fn_##x = NULL;
#define LOAD_API(x) fn_##x = (lpfn_##x)::GetProcAddress(_hDll, #x); \
      if (fn_##x == NULL)                                           \
        goto on_loaderror;

static HRESULT InitializeInternals()
{
  static LONG volatile nMutex = MX_FASTLOCK_INIT;

  if (hDll == NULL)
  {
    MX::CFastLock cLock(&nMutex);
    HINSTANCE _hDll;

    if (hDll == NULL)
    {
      MYSQL *lpSQLite;
      HRESULT hRes;

      //load library
      hRes = MX::Internals::LoadAppDll(L"libmariadb.dll", &_hDll);
      if (FAILED(hRes))
        return hRes;

      //load apis

      fn_mysql_init = (lpfn_mysql_init)::GetProcAddress(_hDll, "mysql_init");
      if (fn_mysql_init == NULL)
      {
on_loaderror:
        hRes = MX_E_ProcNotFound;

on_error:
        ::FreeLibrary(_hDll);

        CLEAR_API(mysql_init);
        CLEAR_API(mysql_options);
        CLEAR_API(mysql_real_connect);
        CLEAR_API(mysql_select_db);
        CLEAR_API(mysql_close);
        CLEAR_API(mysql_ping);
        CLEAR_API(mysql_real_escape_string);
        CLEAR_API(mysql_escape_string);
        CLEAR_API(mysql_real_query);
        CLEAR_API(mysql_store_result);
        CLEAR_API(mysql_use_result);
        CLEAR_API(mysql_free_result);
        CLEAR_API(mysql_field_count);
        CLEAR_API(mysql_affected_rows);
        CLEAR_API(mysql_num_rows);
        CLEAR_API(mysql_num_fields);
        CLEAR_API(mysql_fetch_row);
        CLEAR_API(mysql_fetch_lengths);
        CLEAR_API(mysql_fetch_fields);
        CLEAR_API(mysql_thread_id);
        CLEAR_API(mysql_insert_id);
        CLEAR_API(mysql_set_character_set);
        CLEAR_API(mysql_errno);
        CLEAR_API(mysql_error);
        CLEAR_API(mysql_sqlstate);
        CLEAR_API(mysql_stmt_init);
        CLEAR_API(mysql_stmt_error);
        CLEAR_API(mysql_stmt_errno);
        CLEAR_API(mysql_stmt_sqlstate);
        CLEAR_API(mysql_stmt_close);
        CLEAR_API(mysql_stmt_prepare);
        CLEAR_API(mysql_stmt_bind_param);
        CLEAR_API(mysql_stmt_execute);
        CLEAR_API(mysql_stmt_affected_rows);
        CLEAR_API(mysql_stmt_insert_id);
        CLEAR_API(mysql_stmt_fetch);
        CLEAR_API(mysql_stmt_fetch_column);
        CLEAR_API(mysql_stmt_store_result);
        CLEAR_API(mysql_stmt_bind_result);
        CLEAR_API(mysql_stmt_param_count);
        CLEAR_API(mysql_stmt_result_metadata);
        CLEAR_API(mysql_stmt_attr_set);
        CLEAR_API(mysql_get_client_version);

        //done
        return hRes;
      }
      LOAD_API(mysql_init);
      LOAD_API(mysql_options);
      LOAD_API(mysql_real_connect);
      LOAD_API(mysql_select_db);
      LOAD_API(mysql_close);
      LOAD_API(mysql_ping);
      LOAD_API(mysql_real_escape_string);
      LOAD_API(mysql_escape_string);
      LOAD_API(mysql_real_query);
      LOAD_API(mysql_store_result);
      LOAD_API(mysql_use_result);
      LOAD_API(mysql_free_result);
      LOAD_API(mysql_field_count);
      LOAD_API(mysql_affected_rows);
      LOAD_API(mysql_num_rows);
      LOAD_API(mysql_num_fields);
      LOAD_API(mysql_fetch_row);
      LOAD_API(mysql_fetch_lengths);
      LOAD_API(mysql_fetch_fields);
      LOAD_API(mysql_thread_id);
      LOAD_API(mysql_insert_id);
      LOAD_API(mysql_set_character_set);
      LOAD_API(mysql_errno);
      LOAD_API(mysql_error);
      LOAD_API(mysql_sqlstate);
      LOAD_API(mysql_stmt_init);
      LOAD_API(mysql_stmt_error);
      LOAD_API(mysql_stmt_errno);
      LOAD_API(mysql_stmt_sqlstate);
      LOAD_API(mysql_stmt_close);
      LOAD_API(mysql_stmt_prepare);
      LOAD_API(mysql_stmt_bind_param);
      LOAD_API(mysql_stmt_execute);
      LOAD_API(mysql_stmt_affected_rows);
      LOAD_API(mysql_stmt_insert_id);
      LOAD_API(mysql_stmt_fetch);
      LOAD_API(mysql_stmt_fetch_column);
      LOAD_API(mysql_stmt_store_result);
      LOAD_API(mysql_stmt_bind_result);
      LOAD_API(mysql_stmt_param_count);
      LOAD_API(mysql_stmt_result_metadata);
      LOAD_API(mysql_stmt_attr_set);
      LOAD_API(mysql_get_client_version);

      if (fn_mysql_get_client_version() != MYSQL_VERSION_ID)
      {
        hRes = MX_E_Unsupported;
        goto on_error;
      }

      //initialize library
      lpSQLite = fn_mysql_init(NULL);
      if (lpSQLite == NULL)
      {
        hRes = E_OUTOFMEMORY;
        goto on_error;
      }
      fn_mysql_close(lpSQLite);

      //register shutdown callback
      hRes = MX::RegisterFinalizer(&LIBMYSQL_Shutdown, LIBMYSQL_FINALIZER_PRIORITY);
      if (FAILED(hRes))
        goto on_error;

      //save dll instance
      hDll = _hDll;
    }
  }

  //done
  return S_OK;
}

static VOID LIBMYSQL_Shutdown()
{
  if (hDll != NULL)
  {
    ::FreeLibrary(hDll);
    hDll = NULL;
  }

  CLEAR_API(mysql_init);
  CLEAR_API(mysql_options);
  CLEAR_API(mysql_real_connect);
  CLEAR_API(mysql_select_db);
  CLEAR_API(mysql_close);
  CLEAR_API(mysql_ping);
  CLEAR_API(mysql_real_escape_string);
  CLEAR_API(mysql_escape_string);
  CLEAR_API(mysql_real_query);
  CLEAR_API(mysql_store_result);
  CLEAR_API(mysql_use_result);
  CLEAR_API(mysql_free_result);
  CLEAR_API(mysql_field_count);
  CLEAR_API(mysql_affected_rows);
  CLEAR_API(mysql_num_rows);
  CLEAR_API(mysql_num_fields);
  CLEAR_API(mysql_fetch_row);
  CLEAR_API(mysql_fetch_lengths);
  CLEAR_API(mysql_fetch_fields);
  CLEAR_API(mysql_thread_id);
  CLEAR_API(mysql_insert_id);
  CLEAR_API(mysql_set_character_set);
  CLEAR_API(mysql_errno);
  CLEAR_API(mysql_error);
  CLEAR_API(mysql_sqlstate);
  CLEAR_API(mysql_stmt_init);
  CLEAR_API(mysql_stmt_error);
  CLEAR_API(mysql_stmt_errno);
  CLEAR_API(mysql_stmt_sqlstate);
  CLEAR_API(mysql_stmt_close);
  CLEAR_API(mysql_stmt_prepare);
  CLEAR_API(mysql_stmt_bind_param);
  CLEAR_API(mysql_stmt_execute);
  CLEAR_API(mysql_stmt_affected_rows);
  CLEAR_API(mysql_stmt_insert_id);
  CLEAR_API(mysql_stmt_fetch);
  CLEAR_API(mysql_stmt_fetch_column);
  CLEAR_API(mysql_stmt_store_result);
  CLEAR_API(mysql_stmt_bind_result);
  CLEAR_API(mysql_stmt_param_count);
  CLEAR_API(mysql_stmt_result_metadata);
  CLEAR_API(mysql_stmt_attr_set);
  CLEAR_API(mysql_get_client_version);
  return;
}

#undef LOAD_API
#undef CLEAR_API

static VOID GetAnsiAppPath(_Out_ LPSTR szPathA, _In_ SIZE_T nMaxPathLen)
{
  WCHAR szPathW[4096], *sW;
  DWORD dwLen;
  int nLen;

  //get application path
  dwLen = ::GetModuleFileNameW(NULL, szPathW, MX_ARRAYLEN(szPathW) - 2);
  if (dwLen == 0)
  {
    strcpy_s(szPathA, nMaxPathLen, ".\\");
    return;
  }
  szPathW[dwLen] = 0;
  sW = (LPWSTR)MX::StrChrW(szPathW, L'\\', TRUE);
  if (sW == NULL)
  {
    strcpy_s(szPathA, nMaxPathLen, ".\\");
    return;
  }
  sW[1] = 0;
  nLen = ::WideCharToMultiByte(CP_ACP, 0, szPathW, (int)MX::StrLenW(szPathW), szPathA, 4096, NULL, NULL);
  if (nLen <= 0)
  {
    strcpy_s(szPathA, nMaxPathLen, ".\\");
    return;
  }
  szPathA[nLen] = 0;
  return;
}

static HRESULT HResultFromMySqlErr(_In_ int err)
{
  if (err == 0)
    return S_OK;
  switch (err)
  {
    case CR_TCP_CONNECTION:
    case CR_SERVER_LOST:
    case CR_SERVER_GONE_ERROR:
    case CR_SERVER_LOST_EXTENDED:
      return MX_E_BrokenPipe;

    case 2029: //CR_NULL_POINTER
      return E_POINTER;

    case CR_OUT_OF_MEMORY:
    case ER_OUTOFMEMORY:
    case ER_OUT_OF_SORTMEMORY:
    case ER_OUT_OF_RESOURCES:
      return E_OUTOFMEMORY;

    case CR_UNSUPPORTED_PARAM_TYPE:
      return MX_E_Unsupported;

    case 2050: //CR_FETCH_CANCELED
      return MX_E_Cancelled;

    case CR_NOT_IMPLEMENTED:
      return E_NOTIMPL;

    case ER_DUP_ENTRY:
    case ER_DUP_KEY:
      return MX_E_DuplicateKey;

    case ER_NO_REFERENCED_ROW:
    case ER_ROW_IS_REFERENCED:
    case ER_ROW_IS_REFERENCED_2:
    case ER_NO_REFERENCED_ROW_2:
      return MX_E_ConstraintsCheckFailed;

    case ER_LOCK_WAIT_TIMEOUT:
    case ER_XA_RBTIMEOUT:
      return MX_E_Timeout;
  }
  return E_FAIL;
}

//-----------------------------------------------------------

namespace MX {

namespace Internals {

CMySqlConnectorData::CMySqlConnectorData() : CBaseMemObj()
{
  lpDB = NULL;
  nServerUsingNoBackslashEscapes = -1;
  nLastDbErr = 0;
  strcpy_s(szLastDbSqlStateA, MX_ARRAYLEN(szLastDbSqlStateA), "00000");
  lpResultSet = NULL;
  lpStmt = NULL;
  return;
}

VOID CMySqlConnectorData::ClearErrno()
{
  nLastDbErr = 0;
  cStrLastDbErrorDescriptionA.Empty();
  strcpy_s(szLastDbSqlStateA, MX_ARRAYLEN(szLastDbSqlStateA), "00000");
  return;
}

VOID CMySqlConnectorData::SetErrno()
{
  LPCSTR sA;

  try
  {
    if (lpStmt != NULL)
      nLastDbErr = (int)fn_mysql_stmt_errno(lpStmt);
    else
      nLastDbErr = (int)fn_mysql_errno(lpDB);
    if (nLastDbErr == 0)
      nLastDbErr = ER_UNKNOWN_ERROR;

    if (lpStmt != NULL)
      sA = fn_mysql_stmt_error(lpStmt);
    else
      sA = fn_mysql_error(lpDB);
    if (sA != NULL && *sA != 0)
      cStrLastDbErrorDescriptionA.Copy(sA);
    else
      cStrLastDbErrorDescriptionA.Empty();

    if (lpStmt != NULL)
      sA = fn_mysql_stmt_sqlstate(lpStmt);
    else
      sA = fn_mysql_sqlstate(lpDB);
    if (sA == NULL || *sA == 0)
      sA = "00000";
    strncpy_s(szLastDbSqlStateA, MX_ARRAYLEN(szLastDbSqlStateA), sA, _TRUNCATE);
  }
  catch (...)
  {
    nLastDbErr = ER_UNKNOWN_ERROR;
    cStrLastDbErrorDescriptionA.Empty();
    strcpy_s(szLastDbSqlStateA, MX_ARRAYLEN(szLastDbSqlStateA), "ERUNK");
  }
  return;
}

VOID CMySqlConnectorData::SetCustomErrno(_In_ int err, _In_z_ LPCSTR szDescriptionA, _In_z_ LPCSTR szSqlStateA)
{
  nLastDbErr = err;
  cStrLastDbErrorDescriptionA.Copy(szDescriptionA);
  strncpy_s(szLastDbSqlStateA, MX_ARRAYLEN(szLastDbSqlStateA), szSqlStateA, _TRUNCATE);
  return;
}

//-----------------------------------------------------------

CMySqlConnectorData::CBuffer::CBuffer() : CBaseMemObj()
{
  lpBuffer = NULL;
  nBufferSize = 0;
  return;
}

CMySqlConnectorData::CBuffer::~CBuffer()
{
  MX_FREE(lpBuffer);
  return;
}

BOOL CMySqlConnectorData::CBuffer::EnsureSize(_In_ SIZE_T nNewSize)
{
  if (nNewSize > nBufferSize)
  {
    LPBYTE lpNewBuffer;

    lpNewBuffer = (LPBYTE)MX_MALLOC(nNewSize);
    if (lpNewBuffer == NULL)
      return FALSE;
    MX_FREE(lpBuffer);
    lpBuffer = lpNewBuffer;
    nBufferSize = nNewSize;
  }
  return TRUE;
}

} //namespace Internals

} //namespace MX
