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
#include "JsMySqlPluginCommon.h"
#include "..\..\..\..\Include\AutoPtr.h"
#include <stdlib.h>
#include "..\..\..\..\Include\Strings\Utf8.h"

//-----------------------------------------------------------

#define jsmysql_data       ((Internals::CJsMySqlPluginData*)lpInternal)
#define _CALLAPI(apiname)  Internals::API::fn_##apiname

#define longlong  LONGLONG
#define ulonglong ULONGLONG

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CJsMySqlPluginData : public CBaseMemObj
{
public:
  CJsMySqlPluginData() : CBaseMemObj()
    {
    lpDB = NULL;
    nServerUsingNoBackslashEscapes = -1;
    lpResultSet = NULL;
    lpStmt = NULL;
    nAffectedRows = 0ui64;
    nLastInsertId = 0ui64;
    nFieldsCount = 0;
    return;
    };

public:
  class COutputBuffer : public CBaseMemObj
  {
  public:
    COutputBuffer() : CBaseMemObj()
      {
      nSize = 0;
      return;
      };

    BOOL EnsureSize(_In_ SIZE_T nNewSize)
      {
      if (nNewSize > nSize)
      {
        nSize = 0;
        cData.Reset();
        cData.Attach((LPBYTE)MX_MALLOC(nNewSize));
        if (!cData)
          return FALSE;
        nSize = nNewSize;
      }
      return TRUE;
      };
  public:
    TAutoFreePtr<BYTE> cData;
    SIZE_T nSize;
  };

public:
  MYSQL *lpDB;
  int nServerUsingNoBackslashEscapes;
  MYSQL_RES *lpResultSet;
  MYSQL_STMT *lpStmt;
  ULONGLONG nAffectedRows;
  ULONGLONG nLastInsertId;
  SIZE_T nFieldsCount;
  TArrayListWithRelease<CJsMySqlFieldInfo*> aFieldsList;
  TAutoFreePtr<MYSQL_BIND> cOutputBindings;
  TArrayListWithDelete<COutputBuffer*> aOutputBuffersList;
};

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

namespace MX {

CJsMySqlPlugin::CJsMySqlPlugin(_In_ DukTape::duk_context *lpCtx) : CJsObjectBase(lpCtx)
{
  lpInternal = NULL;
  //default options
  sOptions.nConnectTimeout = 30;
  sOptions.nReadTimeout = sOptions.nWriteTimeout = 45;
#ifdef _DEBUG
  sOptions.bUseDebugDll = FALSE;
#endif //_DEBUG

  //has options in constructor?
  if (DukTape::duk_get_top(lpCtx) > 0)
  {
    if (duk_is_object(lpCtx, 0) == 1)
    {
      //connect timeout option
      DukTape::duk_get_prop_string(lpCtx, 0, "connectTimeout");
      if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
      {
        if (DukTape::duk_is_boolean(lpCtx, -1) != 0)
          sOptions.nConnectTimeout = (DukTape::duk_require_boolean(lpCtx, -1) != 0) ? 1 : 0;
        else
          sOptions.nConnectTimeout = (long)DukTape::duk_require_int(lpCtx, -1);
      }
      DukTape::duk_pop(lpCtx);
      //read timeout option
      DukTape::duk_get_prop_string(lpCtx, 0, "readTimeout");
      if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
      {
        if (DukTape::duk_is_boolean(lpCtx, -1) != 0)
          sOptions.nReadTimeout = (DukTape::duk_require_boolean(lpCtx, -1) != 0) ? 1 : 0;
        else
          sOptions.nReadTimeout = (long)DukTape::duk_require_int(lpCtx, -1);
      }
      DukTape::duk_pop(lpCtx);
      //write timeout option
      DukTape::duk_get_prop_string(lpCtx, 0, "writeTimeout");
      if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
      {
        if (DukTape::duk_is_boolean(lpCtx, -1) != 0)
          sOptions.nWriteTimeout = (DukTape::duk_require_boolean(lpCtx, -1) != 0) ? 1 : 0;
        else
          sOptions.nWriteTimeout = (long)DukTape::duk_require_int(lpCtx, -1);
      }
      DukTape::duk_pop(lpCtx);
    }
    else if (duk_is_null_or_undefined(lpCtx, 0) == 0)
    {
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
    }
  }
  return;
}

CJsMySqlPlugin::~CJsMySqlPlugin()
{
  Disconnect();
  return;
}

VOID CJsMySqlPlugin::OnRegister(_In_ DukTape::duk_context *lpCtx)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  HRESULT hRes;

  DukTape::duk_eval_raw(lpCtx, "function MySqlError(_hr, _dbError, _dbErrorMsg, _sqlState) {\r\n"
                                 "WindowsError.call(this, _hr);\r\n"
                                 "if (_dbErrorMsg.length > 0)\r\n"
                                 "    this.message = _dbErrorMsg;\r\n"
                                 "else\r\n"
                                 "    this.message = \"General failure\";\r\n"
                                 "this.name = \"MySqlError\";\r\n"
                                 "this.dbError = _dbError;\r\n"
                                 "this.sqlState = _sqlState;\r\n"
                                 "return this; }\r\n"
                               "MySqlError.prototype = Object.create(WindowsError.prototype);\r\n"
                               "MySqlError.prototype.constructor=MySqlError;\r\n", 0,
                        DUK_COMPILE_EVAL | DUK_COMPILE_NOSOURCE | DUK_COMPILE_STRLEN | DUK_COMPILE_NOFILENAME);

  hRes = lpJVM->RegisterException("MySqlError", [](_In_ DukTape::duk_context *lpCtx,
                                                   _In_ DukTape::duk_idx_t nExceptionObjectIndex) -> VOID
  {
    throw CJsMySqlError(lpCtx, nExceptionObjectIndex);
    return;
  });
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  return;
}

VOID CJsMySqlPlugin::OnUnregister(_In_ DukTape::duk_context *lpCtx)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);

  lpJVM->UnregisterException("MySqlError");

  DukTape::duk_push_global_object(lpCtx);
  DukTape::duk_del_prop_string(lpCtx, -1, "MySqlError");
  DukTape::duk_pop(lpCtx);
  return;
}

DukTape::duk_ret_t CJsMySqlPlugin::Connect()
{
  DukTape::duk_context *lpCtx = GetContext();
  LPCSTR szHostA, szUserNameA, szPasswordA, szDbNameA;
  DukTape::duk_idx_t nParamsCount;
  int nPort;
  long nTemp;
  HRESULT hRes;

  Disconnect();
  //get parameters
  nParamsCount = DukTape::duk_get_top(lpCtx);
  if (nParamsCount < 1 || nParamsCount > 5)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  szHostA = DukTape::duk_require_string(lpCtx, 0);
  szUserNameA = NULL;
  if (nParamsCount > 1)
    szUserNameA = DukTape::duk_require_string(lpCtx, 1);
  szPasswordA = NULL;
  if (nParamsCount > 2)
    szPasswordA = DukTape::duk_require_string(lpCtx, 2);
  szDbNameA = NULL;
  if (nParamsCount > 3)
    szDbNameA = DukTape::duk_require_string(lpCtx, 3);
  nPort = 3306;
  if (nParamsCount > 4)
  {
    nPort = DukTape::duk_require_int(lpCtx, 4);
    if (nPort < 1 || nPort > 65535)
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  }
  //initialize APIs
#ifdef _DEBUG
  hRes = Internals::API::MySqlInitialize(sOptions.bUseDebugDll);
#else //_DEBUG
  hRes = Internals::API::MySqlInitialize();
#endif //_DEBUG
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  //create internal object
  lpInternal = MX_DEBUG_NEW Internals::CJsMySqlPluginData();
  if (lpInternal == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
  //----
  jsmysql_data->lpDB = _CALLAPI(mysql_init)(NULL);
  if (jsmysql_data->lpDB == NULL)
  {
    delete jsmysql_data;
    lpInternal = NULL;
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
  }
  if (sOptions.nConnectTimeout > 0)
  {
    nTemp = sOptions.nConnectTimeout;
    _CALLAPI(mysql_options)(jsmysql_data->lpDB, MYSQL_OPT_CONNECT_TIMEOUT, &nTemp);
  }
  _CALLAPI(mysql_options)(jsmysql_data->lpDB, MYSQL_OPT_GUESS_CONNECTION, L"");
  if (sOptions.nReadTimeout > 0)
  {
    nTemp = sOptions.nReadTimeout;
    _CALLAPI(mysql_options)(jsmysql_data->lpDB, MYSQL_OPT_READ_TIMEOUT, &nTemp);
  }
  if (sOptions.nWriteTimeout > 0)
  {
    nTemp = sOptions.nWriteTimeout;
    _CALLAPI(mysql_options)(jsmysql_data->lpDB, MYSQL_OPT_WRITE_TIMEOUT, &nTemp);
  }
  nTemp = 1;
  _CALLAPI(mysql_options)(jsmysql_data->lpDB, MYSQL_REPORT_DATA_TRUNCATION, &nTemp);
  nTemp = 0;
  _CALLAPI(mysql_options)(jsmysql_data->lpDB, MYSQL_OPT_RECONNECT, &nTemp);
  nTemp = 1;
  _CALLAPI(mysql_options)(jsmysql_data->lpDB, MYSQL_SECURE_AUTH, &nTemp);
  //do connection
  if (_CALLAPI(mysql_real_connect)(jsmysql_data->lpDB, szHostA, (szUserNameA != NULL) ? szUserNameA : "",
                                   (szPasswordA != NULL) ? szPasswordA : "", NULL, (UINT)nPort, NULL,
                                   CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_COMPRESS | CLIENT_LOCAL_FILES |
                                   CLIENT_PROTOCOL_41 | CLIENT_TRANSACTIONS | CLIENT_FOUND_ROWS) == NULL)
  {
raise_error:
    try
    {
      ThrowDbError(__FILE__, __LINE__, TRUE);
    }
    catch (...)
    {
      delete jsmysql_data;
      lpInternal = NULL;
      throw;
    }
    delete jsmysql_data;
    lpInternal = NULL;
    DukTape::duk_throw_raw(lpCtx);
    return 0;
  }
  if (_CALLAPI(mysql_set_character_set)(jsmysql_data->lpDB, "utf8") != 0)
    goto raise_error;
  //set database if any specified
  if (szDbNameA != NULL && *szDbNameA != 0)
  {
    if (_CALLAPI(mysql_select_db)(jsmysql_data->lpDB, szDbNameA) != 0)
      goto raise_error;
  }
  //done
  return 0;
}

DukTape::duk_ret_t CJsMySqlPlugin::Disconnect()
{
  if (lpInternal != NULL)
  {
    if (jsmysql_data->lpDB != NULL)
    {
      QueryClose();
      _CALLAPI(mysql_close)(jsmysql_data->lpDB);
    }
    delete jsmysql_data;
    lpInternal = NULL;
  }
  //done
  return 0;
}

DukTape::duk_ret_t CJsMySqlPlugin::SelectDatabase()
{
  DukTape::duk_context *lpCtx = GetContext();
  LPCSTR szDbNameA;

  if (lpInternal == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);
  //get parameters
  szDbNameA = DukTape::duk_require_string(lpCtx, 0);
  if (*szDbNameA == 0)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  //select database
  QueryClose();
  if (_CALLAPI(mysql_select_db)(jsmysql_data->lpDB, szDbNameA) != 0)
    ThrowDbError(__FILE__, __LINE__);
  //done
  return 0;
}

DukTape::duk_ret_t CJsMySqlPlugin::Query()
{
  DukTape::duk_context *lpCtx = GetContext();
  LPCSTR szQueryA;
  int nRetryCount;
  SIZE_T nQueryLegth;
  DukTape::duk_idx_t nParamsCount;
  HRESULT hRes;

  if (lpInternal == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);
  //get parameters
  nParamsCount = DukTape::duk_get_top(lpCtx);
  if (nParamsCount < 1)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  szQueryA = DukTape::duk_require_string(lpCtx, 0);
  if (*szQueryA == 0)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  //close previous query if any
  QueryClose();
  //validate arguments
  nQueryLegth = StrLenA(szQueryA);
  if (nQueryLegth > 0xFFFFFFFF)
    nQueryLegth = 0xFFFFFFFF;
  while (nQueryLegth > 0 && szQueryA[nQueryLegth - 1] == ';')
    nQueryLegth--;
  //execute query
  nRetryCount = 5;
  while (nRetryCount > 0)
  {
    TAutoFreePtr<MYSQL_BIND> cInputBindings;
    ULONG nInputParams;
    DukTape::duk_idx_t idx;
    int err;

    QueryClose();

    try
    {
      jsmysql_data->lpStmt = _CALLAPI(mysql_stmt_init)(jsmysql_data->lpDB);
      if (jsmysql_data->lpStmt == NULL)
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);

      if (_CALLAPI(mysql_stmt_prepare)(jsmysql_data->lpStmt, szQueryA, (ULONG)nQueryLegth) != 0)
      {
        err = _CALLAPI(mysql_stmt_errno)(jsmysql_data->lpStmt);
        if (err == CR_OUT_OF_MEMORY)
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
        if (nRetryCount > 1 && err == ER_CANT_LOCK)
          throw (int)ER_CANT_LOCK;
        ThrowDbError(__FILE__, __LINE__);
      }

      nInputParams = _CALLAPI(mysql_stmt_param_count)(jsmysql_data->lpStmt);
      if (nParamsCount != (DukTape::duk_idx_t)nInputParams + 1)
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);

      if (nInputParams > 0)
      {
        //calculate data size
        SIZE_T nSize = (SIZE_T)nInputParams * sizeof(MYSQL_BIND);
        LPBYTE lpPtr;

        for (idx = 1; idx < nParamsCount; idx++)
        {
          DukTape::duk_size_t nLen;
          MX::CStringA cStrTypeA;

          switch (DukTape::duk_get_type(lpCtx, idx))
          {
            case DUK_TYPE_NONE:
            case DUK_TYPE_UNDEFINED:
            case DUK_TYPE_NULL:
              nSize += sizeof(my_bool);
              break;

            case DUK_TYPE_BOOLEAN:
              nSize += sizeof(LONG);
              break;

            case DUK_TYPE_NUMBER:
              nSize += sizeof(double);
              break;

            case DUK_TYPE_STRING:
              DukTape::duk_get_lstring(lpCtx, idx, &nLen);
              nSize += nLen;
              break;

            case DUK_TYPE_OBJECT:
              CJavascriptVM::GetObjectType(lpCtx, idx, cStrTypeA);
              if (StrCompareA((LPCSTR)cStrTypeA, "Date") == 0)
              {
                nSize += sizeof(MYSQL_TIME);
              }
              else
              {
                MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
              }
              break;

            case DUK_TYPE_BUFFER:
              DukTape::duk_get_buffer(lpCtx, idx, &nLen);
              nSize += nLen;
              break;

            default:
              MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
          }
        }

        cInputBindings.Attach((MYSQL_BIND*)MX_MALLOC((SIZE_T)nInputParams * sizeof(MYSQL_BIND) + nSize));
        if (!cInputBindings)
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
        MemSet(cInputBindings.Get(), 0, (SIZE_T)nInputParams * sizeof(MYSQL_BIND) + nSize);

        lpPtr = (LPBYTE)cInputBindings.Get() + (SIZE_T)nInputParams * sizeof(MYSQL_BIND);
        for (idx = 1; idx < nParamsCount; idx++)
        {
          MX::CStringA cStrTypeA;
          MYSQL_BIND *lpBind = cInputBindings.Get() + (SIZE_T)(idx - 1);
          DukTape::duk_size_t nLen;
          LPVOID s;

          switch (DukTape::duk_get_type(lpCtx, idx))
          {
            case DUK_TYPE_NONE:
            case DUK_TYPE_UNDEFINED:
            case DUK_TYPE_NULL:
              lpBind->buffer_type = MYSQL_TYPE_NULL;
              lpBind->is_null = (my_bool*)lpPtr;
              *(lpBind->is_null) = 1;

              lpPtr += sizeof(my_bool);
              break;

            case DUK_TYPE_BOOLEAN:
              lpBind->buffer_type = MYSQL_TYPE_LONG;
              lpBind->buffer = lpPtr;
              lpBind->buffer_length = (ULONG)sizeof(LONG);
              *((LONG*)(lpBind->buffer)) = DukTape::duk_get_boolean(lpCtx, idx) ? 1 : 0;

              lpPtr += sizeof(LONG);
              break;

            case DUK_TYPE_NUMBER:
              {
              double dbl;
              LONGLONG ll;

              lpBind->buffer = lpPtr;

              dbl = (double)DukTape::duk_get_number(lpCtx, idx);
              ll = (LONGLONG)dbl;
              if ((double)ll != dbl || dbl < (double)LONG_MIN)
              {
                lpBind->buffer_type = MYSQL_TYPE_DOUBLE;
                lpBind->buffer_length = (ULONG)sizeof(double);
                *((double*)(lpBind->buffer)) = dbl;
              }
              else if (ll < (LONGLONG)LONG_MIN)
              {
                lpBind->buffer_type = MYSQL_TYPE_LONGLONG;
                lpBind->buffer_length = (ULONG)sizeof(LONGLONG);
                *((LONGLONG*)(lpBind->buffer)) = ll;
              }
              else if (ll <= (LONGLONG)LONG_MAX)
              {
                lpBind->buffer_type = MYSQL_TYPE_LONG;
                lpBind->buffer_length = (ULONG)sizeof(LONG);
                *((LONG*)(lpBind->buffer)) = (LONG)ll;
              }
              else if ((ULONG)(ULONGLONG)ll <= (ULONG)LONG_MAX)
              {
                lpBind->buffer_type = MYSQL_TYPE_LONG;
                lpBind->buffer_length = (ULONG)sizeof(LONG);
                *((ULONG*)(lpBind->buffer)) = (ULONG)(ULONGLONG)ll;
                lpBind->is_unsigned = 1;
              }
              else
              {
                lpBind->buffer_type = MYSQL_TYPE_DOUBLE;
                lpBind->buffer_length = (ULONG)sizeof(double);
                *((double*)(lpBind->buffer)) = dbl;
              }

              lpPtr += sizeof(double);
              }
              break;

            case DUK_TYPE_STRING:
              s = (LPVOID)DukTape::duk_get_lstring(lpCtx, idx, &nLen);

              lpBind->buffer_type = MYSQL_TYPE_VAR_STRING;
              lpBind->buffer = lpPtr;
              lpBind->buffer_length = (ULONG)nLen;
              MemCopy(lpBind->buffer, s, nLen);

              lpPtr += nLen;
              break;

            case DUK_TYPE_OBJECT:
              CJavascriptVM::GetObjectType(lpCtx, idx, cStrTypeA);
              if (StrCompareA((LPCSTR)cStrTypeA, "Date") == 0)
              {
                SYSTEMTIME sSt;

                CJavascriptVM::GetDate(lpCtx, idx, &sSt);

                lpBind->buffer_type = MYSQL_TYPE_DATETIME;
                lpBind->buffer = lpPtr;
                lpBind->buffer_length = (ULONG)sizeof(MYSQL_TIME);

                ((MYSQL_TIME*)lpPtr)->day = (UINT)(sSt.wDay);
                ((MYSQL_TIME*)lpPtr)->month = (UINT)(sSt.wMonth);
                ((MYSQL_TIME*)lpPtr)->year = (UINT)(sSt.wYear);
                ((MYSQL_TIME*)lpPtr)->hour = (UINT)(sSt.wHour);
                ((MYSQL_TIME*)lpPtr)->minute = (UINT)(sSt.wMinute);
                ((MYSQL_TIME*)lpPtr)->second = (UINT)(sSt.wSecond);
                ((MYSQL_TIME*)lpPtr)->second_part = (ULONG)(sSt.wMilliseconds);
                ((MYSQL_TIME*)lpPtr)->neg = 0;
                ((MYSQL_TIME*)lpPtr)->time_type = MYSQL_TIMESTAMP_DATETIME;

                lpPtr += sizeof(MYSQL_TIME);
              }
              break;

            case DUK_TYPE_BUFFER:
              s = DukTape::duk_get_buffer(lpCtx, idx, &nLen);

              lpBind->buffer_type = MYSQL_TYPE_LONG_BLOB;
              lpBind->buffer = lpPtr;
              lpBind->buffer_length = (ULONG)nLen;
              MemCopy(lpBind->buffer, s, nLen);

              lpPtr += nLen;
              break;
          }
        }

        if (_CALLAPI(mysql_stmt_bind_param)(jsmysql_data->lpStmt, cInputBindings.Get()) != 0)
        {
          err = _CALLAPI(mysql_stmt_errno)(jsmysql_data->lpStmt);
          if (err == CR_OUT_OF_MEMORY)
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          if (nRetryCount > 1 && err == ER_CANT_LOCK)
            throw (int)ER_CANT_LOCK;
          ThrowDbError(__FILE__, __LINE__);
        }
      }

      if (_CALLAPI(mysql_stmt_execute)(jsmysql_data->lpStmt) != 0)
      {
        err = _CALLAPI(mysql_stmt_errno)(jsmysql_data->lpStmt);
        if (err == CR_OUT_OF_MEMORY)
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
        if (nRetryCount > 1 && err == ER_CANT_LOCK)
          throw (int)ER_CANT_LOCK;
        ThrowDbError(__FILE__, __LINE__);
      }

      jsmysql_data->lpResultSet = _CALLAPI(mysql_stmt_result_metadata)(jsmysql_data->lpStmt);
      if (jsmysql_data->lpResultSet == NULL)
      {
        err = _CALLAPI(mysql_stmt_errno)(jsmysql_data->lpStmt);
        if (err != 0)
        {
          if (err == CR_OUT_OF_MEMORY)
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          if (nRetryCount > 1 && err == ER_CANT_LOCK)
            throw (int)ER_CANT_LOCK;
          ThrowDbError(__FILE__, __LINE__);
        }
      }

      hRes = S_OK;
    }
    catch (int err)
    {
      UNREFERENCED_PARAMETER(err);
      QueryClose();
      hRes = E_FAIL;
    }
    catch (...)
    {
      //javascript or other error, just re-throw
      QueryClose();
      throw;
    }
    if (SUCCEEDED(hRes))
      break;
    ::Sleep(50);
    nRetryCount--;
  }

  //get some data
  jsmysql_data->nAffectedRows = _CALLAPI(mysql_stmt_affected_rows)(jsmysql_data->lpStmt);
  jsmysql_data->nLastInsertId = _CALLAPI(mysql_stmt_insert_id)(jsmysql_data->lpStmt);
  jsmysql_data->nFieldsCount = 0;

  //get last insert id and initialize result-set fields
  if (jsmysql_data->lpResultSet != NULL)
  {
    /*
    my_bool attr_val;

    attr_val = 1;
    _CALLAPI(mysql_stmt_attr_set)(jsmysql_data->lpStmt, STMT_ATTR_UPDATE_MAX_LENGTH, &attr_val);

    attr_val = CURSOR_TYPE_READ_ONLY;
    _CALLAPI(mysql_stmt_attr_set)(jsmysql_data->lpStmt, STMT_ATTR_CURSOR_TYPE, &attr_val);
    */

    jsmysql_data->nFieldsCount = (SIZE_T)(_CALLAPI(mysql_num_fields)(jsmysql_data->lpResultSet));
    if (jsmysql_data->nFieldsCount > 0)
    {
      try
      {
        TAutoRefCounted<Internals::CJsMySqlFieldInfo> cFieldInfo;
        SIZE_T i;
        MYSQL_FIELD *lpFields;
        MYSQL_BIND *lpBind;
        ULONG *lpLengthPtr;
        my_bool *lpNulPtr;
        Internals::CJsMySqlPluginData::COutputBuffer *lpOutputBuffer;

        //allocate for result bindings
        jsmysql_data->cOutputBindings.Attach((MYSQL_BIND*)MX_MALLOC(jsmysql_data->nFieldsCount * (sizeof(MYSQL_BIND) +
                                                                    sizeof(ULONG) + sizeof(my_bool))));
        if (!(jsmysql_data->cOutputBindings))
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
        MemSet(jsmysql_data->cOutputBindings.Get(), 0, jsmysql_data->nFieldsCount *
                                                       (sizeof(MYSQL_BIND) + sizeof(ULONG) + sizeof(my_bool)));

        lpBind = jsmysql_data->cOutputBindings.Get();
        lpLengthPtr = (ULONG*)(lpBind + jsmysql_data->nFieldsCount);
        lpNulPtr = (my_bool*)(lpLengthPtr + jsmysql_data->nFieldsCount);

        lpFields = _CALLAPI(mysql_fetch_fields)(jsmysql_data->lpResultSet);
        //build fields
        for (i = 0; i < jsmysql_data->nFieldsCount; i++)
        {
          cFieldInfo.Attach(MX_DEBUG_NEW Internals::CJsMySqlFieldInfo(lpCtx));
          if (!cFieldInfo)
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          //name
          if (cFieldInfo->cStrNameA.Copy(lpFields[i].name) == FALSE)
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          //original name
          if (lpFields[i].org_name != NULL)
          {
            if (cFieldInfo->cStrOriginalNameA.Copy(lpFields[i].org_name) == FALSE)
              MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          }
          //table
          if (lpFields[i].table != NULL)
          {
            if (cFieldInfo->cStrTableA.Copy(lpFields[i].table) == FALSE)
              MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          }
          //original table
          if (lpFields[i].table != NULL)
          {
            if (cFieldInfo->cStrOriginalTableA.Copy(lpFields[i].org_table) == FALSE)
              MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          }
          //database
          if (lpFields[i].db != NULL)
          {
            if (cFieldInfo->cStrDatabaseA.Copy(lpFields[i].db) == FALSE)
              MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          }
          //length, max length, charset, decimals count
          cFieldInfo->nDecimalsCount = lpFields[i].decimals;
          cFieldInfo->nCharSet = lpFields[i].charsetnr;
          cFieldInfo->nFlags = lpFields[i].flags;
          cFieldInfo->nType = lpFields[i].type;

          lpBind[i].buffer_type = cFieldInfo->nType;
          lpBind[i].length = lpLengthPtr + i;
          lpBind[i].is_null = lpNulPtr + i;
          lpBind[i].is_unsigned = ((lpFields[i].flags & UNSIGNED_FLAG) != 0) ? 1 : 0;

          if ((lpFields[i].flags & (BLOB_FLAG | BINARY_FLAG)) == (BLOB_FLAG | BINARY_FLAG))
          {
            lpBind[i].buffer_type = MYSQL_TYPE_BLOB;
          }
          else switch (lpFields[i].type) {
            case MYSQL_TYPE_TINY_BLOB:
            case MYSQL_TYPE_MEDIUM_BLOB:
            case MYSQL_TYPE_LONG_BLOB:
            case MYSQL_TYPE_BLOB:
              break;

            case MYSQL_TYPE_TINY:
            case MYSQL_TYPE_SHORT:
            case MYSQL_TYPE_LONG:
              lpBind[i].buffer_type = MYSQL_TYPE_LONG;
              lpBind[i].buffer_length = sizeof(LONG);
              break;

            case MYSQL_TYPE_LONGLONG:
              lpBind[i].buffer_length = sizeof(LONGLONG);
              break;

            case MYSQL_TYPE_INT24:
            case MYSQL_TYPE_YEAR:
            case MYSQL_TYPE_BIT:
              lpBind[i].buffer_type = MYSQL_TYPE_LONGLONG;
              lpBind[i].buffer_length = sizeof(LONGLONG);
              break;

            case MYSQL_TYPE_FLOAT:
              lpBind[i].buffer_length = sizeof(float);
              break;

            case MYSQL_TYPE_DECIMAL:
            case MYSQL_TYPE_NEWDECIMAL:
            case MYSQL_TYPE_DOUBLE:
              lpBind[i].buffer_type = MYSQL_TYPE_DOUBLE;
              lpBind[i].buffer_length = sizeof(double);
              break;

            case MYSQL_TYPE_TIMESTAMP:
            case MYSQL_TYPE_DATE:
            case MYSQL_TYPE_TIME:
            case MYSQL_TYPE_DATETIME:
            case MYSQL_TYPE_NEWDATE:
            //case MYSQL_TYPE_TIMESTAMP2: //only server-side
            //case MYSQL_TYPE_DATETIME2:  //only server-side
            //case MYSQL_TYPE_TIME2:      //only server-side
              lpBind[i].buffer_type = MYSQL_TYPE_DATETIME;
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

          //add to list
          if (jsmysql_data->aFieldsList.AddElement(cFieldInfo.Get()) == FALSE)
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          cFieldInfo.Detach();

          lpOutputBuffer = MX_DEBUG_NEW Internals::CJsMySqlPluginData::COutputBuffer();
          if (lpOutputBuffer != NULL)
          {
            if (jsmysql_data->aOutputBuffersList.AddElement(lpOutputBuffer) == FALSE)
            {
              delete lpOutputBuffer;
              MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
            }
            if (lpBind[i].buffer_length > 0)
            {
              if (lpOutputBuffer->EnsureSize((SIZE_T)(lpBind[i].buffer_length)) == FALSE)
                MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
              lpBind[i].buffer = lpOutputBuffer->cData.Get();
            }
          }
          else
          {
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          }
        }

        if (_CALLAPI(mysql_stmt_bind_result)(jsmysql_data->lpStmt, jsmysql_data->cOutputBindings.Get()) != 0)
        {
          int err = _CALLAPI(mysql_stmt_errno)(jsmysql_data->lpStmt);
          if (err == CR_OUT_OF_MEMORY)
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          ThrowDbError(__FILE__, __LINE__);
        }
      }
      catch (...)
      {
        //javascript or other error, just re-throw
        QueryClose();
        throw;
      }
    }
  }

  //done
  return 0;
}

DukTape::duk_ret_t CJsMySqlPlugin::QueryAndFetch()
{
  Query();
  return FetchRow();
}

DukTape::duk_ret_t CJsMySqlPlugin::QueryClose()
{
  if (lpInternal != NULL)
  {
    if (jsmysql_data->lpStmt != NULL)
    {
      _CALLAPI(mysql_stmt_close)(jsmysql_data->lpStmt);
      jsmysql_data->lpStmt = NULL;
    }
    if (jsmysql_data->lpResultSet != NULL)
    {
      _CALLAPI(mysql_free_result)(jsmysql_data->lpResultSet);
      jsmysql_data->lpResultSet = NULL;
    }
    jsmysql_data->aFieldsList.RemoveAllElements();
    jsmysql_data->aOutputBuffersList.RemoveAllElements();
    jsmysql_data->cOutputBindings.Reset();
    jsmysql_data->nFieldsCount = 0;
  }
  //done
  return 0;
}

DukTape::duk_ret_t CJsMySqlPlugin::EscapeString()
{
  DukTape::duk_context *lpCtx = GetContext();
  CStringA cStrResultA;
  DukTape::duk_idx_t nParamsCount;
  BOOL bIsLikeStatement;
  LPCSTR szStrA;

  if (lpInternal == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);
  //get parameters
  nParamsCount = DukTape::duk_get_top(lpCtx);
  if (nParamsCount < 1 || nParamsCount > 2)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  szStrA = DukTape::duk_require_string(lpCtx, 0);
  bIsLikeStatement = FALSE;
  if (nParamsCount > 1)
  {
    if (DukTape::duk_is_boolean(lpCtx, 1) != 0)
      bIsLikeStatement = (DukTape::duk_require_boolean(lpCtx, 1) != 0) ? TRUE : FALSE;
    else
      bIsLikeStatement = (DukTape::duk_require_int(lpCtx, 1) != 0) ? TRUE : FALSE;
  }
  //detect if server is using no backslashes escape
  if (jsmysql_data->nServerUsingNoBackslashEscapes == -1)
  {
    //since we cannot check "mysql->server_status" SERVER_STATUS_NO_BACKSLASH_ESCAPES's flag
    //directly... make a real escape of an apostrophe
    CHAR d[4];
    ULONG ret;

    ret = _CALLAPI(mysql_real_escape_string)(jsmysql_data->lpDB, d, "\'", 1);
    if (ret < 2)
    {
      jsmysql_data->nServerUsingNoBackslashEscapes = 0; //(?) should not happen
    }
    else if (d[0] == '\\')
    {
      jsmysql_data->nServerUsingNoBackslashEscapes = 0;
    }
    else
    {
      jsmysql_data->nServerUsingNoBackslashEscapes = 1;
    }
  }
  //full escaping... since we are converting strings to utf-8, we don't need to
  //check for special multibyte characters because utf-8 multibyte chars are
  //always >= 0x80 and doesn't conflict with the backslash (0x5C)
  while (*szStrA != 0)
  {
    switch (*szStrA)
    {
      case '\'':
        if (jsmysql_data->nServerUsingNoBackslashEscapes != 0)
        {
          if (cStrResultA.ConcatN("''", 2) == FALSE)
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          break;
        }
        //fall into next...
      case '"':
        if (cStrResultA.AppendFormat("\\%c", *szStrA) == FALSE)
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
        break;

      case '\r':
        if (jsmysql_data->nServerUsingNoBackslashEscapes != 0)
          goto doDefault;
        if (cStrResultA.ConcatN("\\r", 2) == FALSE)
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
        break;

      case '\n':
        if (jsmysql_data->nServerUsingNoBackslashEscapes != 0)
          goto doDefault;
        if (cStrResultA.ConcatN("\\n", 2) == FALSE)
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
        break;

      case '\032':
        if (jsmysql_data->nServerUsingNoBackslashEscapes != 0)
          goto doDefault;
        if (cStrResultA.ConcatN("\\Z", 2) == FALSE)
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
        break;

      case 0:
        if (cStrResultA.ConcatN("\\0", 2) == FALSE)
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
        break;

      case '\\':
        if (bIsLikeStatement != FALSE && (szStrA[1]=='%' || szStrA[1]=='_'))
        {
          szStrA++;
          if (cStrResultA.AppendFormat("\\%c", *szStrA) == FALSE)
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
        }
        else
        {
          if (cStrResultA.ConcatN("\\\\", 2) == FALSE)
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
        }
        break;

      default:
doDefault:
        if (cStrResultA.ConcatN(szStrA, 1) == FALSE)
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
        break;
    }
    szStrA++;
  }
  //done
  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrResultA, cStrResultA.GetLength());
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::Utf8Truncate()
{
  DukTape::duk_context *lpCtx = GetContext();
  CStringA cStrResultA;
  DukTape::duk_size_t nStrLen;
  DukTape::duk_uint_t nMaxLength;
  LPCSTR sA, szStrA, szEndA;
  int nCharLen;

  if (lpInternal == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);
  //get parameters
  szStrA = DukTape::duk_require_lstring(lpCtx, 0, &nStrLen);
  szEndA = szStrA + nStrLen;
  if (DukTape::duk_is_boolean(lpCtx, 1) != 0)
    nMaxLength = (DukTape::duk_require_boolean(lpCtx, 1) != 0) ? 1 : 0;
  else
    nMaxLength = DukTape::duk_require_uint(lpCtx, 1);
  //count characters
  for (sA=szStrA;  sA<szEndA; sA+=(SIZE_T)nCharLen)
  {
    nCharLen = Utf8_DecodeChar(NULL, sA, (SIZE_T)(szEndA-sA));
    if (nCharLen < 1)
      break;
  }
  //done
  DukTape::duk_push_lstring(lpCtx, szStrA, (DukTape::duk_size_t)(sA - szStrA));
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::FetchRow()
{
  DukTape::duk_context *lpCtx = GetContext();
  Internals::CJsMySqlFieldInfo *lpFieldInfo;
  SIZE_T i, nFieldsCount;
  MYSQL_BIND *lpBind;
  Internals::CJsMySqlPluginData::COutputBuffer *lpOutputBuffer;
  ULONG *lpLengthPtr;
  my_bool *lpNulPtr;
  int err;

  if (lpInternal == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);
  if (jsmysql_data->lpResultSet == NULL || (nFieldsCount = jsmysql_data->aFieldsList.GetCount()) == 0)
  {
    DukTape::duk_push_null(lpCtx);
    return 1;
  }
  //set result bind buffers length to 0 before fetching
  lpBind = jsmysql_data->cOutputBindings.Get();
  lpLengthPtr = (ULONG*)(lpBind + jsmysql_data->nFieldsCount);
  lpNulPtr = (my_bool*)(lpLengthPtr + jsmysql_data->nFieldsCount);
  for (i = 0; i < nFieldsCount; i++)
  {
    if (lpBind[i].buffer_type == MYSQL_TYPE_TINY_BLOB || lpBind[i].buffer_type == MYSQL_TYPE_MEDIUM_BLOB ||
        lpBind[i].buffer_type == MYSQL_TYPE_LONG_BLOB || lpBind[i].buffer_type == MYSQL_TYPE_BLOB ||
        lpBind[i].buffer_type == MYSQL_TYPE_VAR_STRING)
    {
      lpBind[i].buffer_length = 0;
    }
  }
  //fetch next row
  err = _CALLAPI(mysql_stmt_fetch)(jsmysql_data->lpStmt);
  if (err == MYSQL_NO_DATA)
  {
    DukTape::duk_push_null(lpCtx);
    return 1;
  }
  if (err != 0 && err != MYSQL_DATA_TRUNCATED)
    ThrowDbError(__FILE__, __LINE__);
  //ensure enough buffer size in the field
  for (i = 0; i < nFieldsCount; i++)
  {
    if (lpBind[i].buffer_type == MYSQL_TYPE_TINY_BLOB || lpBind[i].buffer_type == MYSQL_TYPE_MEDIUM_BLOB ||
        lpBind[i].buffer_type == MYSQL_TYPE_LONG_BLOB || lpBind[i].buffer_type == MYSQL_TYPE_BLOB ||
        lpBind[i].buffer_type == MYSQL_TYPE_VAR_STRING)
    {
      lpOutputBuffer = jsmysql_data->aOutputBuffersList.GetElementAt(i);
      lpBind[i].buffer_length = lpLengthPtr[i];
      if (lpOutputBuffer->EnsureSize((SIZE_T)lpLengthPtr[i]) == FALSE)
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
      lpBind[i].buffer = lpOutputBuffer->cData.Get();
    }
  }
  //fetch each column
  for (i = 0; i < nFieldsCount; i++)
  {
    err = _CALLAPI(mysql_stmt_fetch_column)(jsmysql_data->lpStmt, &lpBind[i], (unsigned int)i, 0);
    if (err != 0)
      ThrowDbError(__FILE__, __LINE__);
  }

  //create an object for each result
  DukTape::duk_push_object(lpCtx);
  for (i = 0; i < nFieldsCount; i++)
  {
    lpFieldInfo = jsmysql_data->aFieldsList.GetElementAt(i);
    if (lpNulPtr[i] == 0)
    {
      switch (lpBind[i].buffer_type)
      {
        case MYSQL_TYPE_TINY_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_LONG_BLOB:
        case MYSQL_TYPE_BLOB:
        case MYSQL_TYPE_VAR_STRING:
          if ((lpFieldInfo->nFlags & BINARY_FLAG) != 0)
          {
            LPBYTE p = (LPBYTE)(DukTape::duk_push_fixed_buffer(lpCtx, (DukTape::duk_size_t)(lpLengthPtr[i])));

            MemCopy(p, lpBind[i].buffer, (SIZE_T)lpLengthPtr[i]);
            DukTape::duk_push_buffer_object(lpCtx, -1, 0, (DukTape::duk_size_t)(lpLengthPtr[i]), DUK_BUFOBJ_UINT8ARRAY);
            DukTape::duk_remove(lpCtx, -2);
          }
          else
          {
            DukTape::duk_push_lstring(lpCtx, (const char*)(lpBind[i].buffer), (DukTape::duk_size_t)(lpLengthPtr[i]));
          }
          break;

        case MYSQL_TYPE_LONG:
          if ((lpFieldInfo->nFlags & UNSIGNED_FLAG) != 0)
            DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)*((ULONG*)(lpBind[i].buffer)));
          else
            DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)*((LONG*)(lpBind[i].buffer)));
          break;

        case MYSQL_TYPE_LONGLONG:
          if ((lpFieldInfo->nFlags & UNSIGNED_FLAG) != 0)
          {
            ULONGLONG ull = *((ULONGLONG*)(lpBind[i].buffer));

            if (ull < (ULONGLONG)ULONG_MAX)
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_int_t)ull);
            else
              DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)ull);
          }
          else
          {
            LONGLONG ll = *((LONGLONG*)(lpBind[i].buffer));

            if (ll >= (LONGLONG)LONG_MIN && ll <= (LONGLONG)LONG_MAX)
              DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)ll);
            else
              DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)ll);
          }
          break;

        case MYSQL_TYPE_FLOAT:
          DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)*((float*)(lpBind[i].buffer)));
          break;

        case MYSQL_TYPE_DOUBLE:
          DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)*((double*)(lpBind[i].buffer)));
          break;

        case MYSQL_TYPE_DATETIME:
          {
          MYSQL_TIME *lpDt = (MYSQL_TIME*)(lpBind[i].buffer);
          SYSTEMTIME sSt;

          if (lpFieldInfo->nType == MYSQL_TYPE_TIME)
          {
            ::GetSystemTime(&sSt);
            sSt.wHour = (WORD)(lpDt->hour);
            sSt.wMinute = (WORD)(lpDt->minute);
            sSt.wSecond = (WORD)(lpDt->second);
            sSt.wMilliseconds = (WORD)(lpDt->second_part);

            CJavascriptVM::PushDate(lpCtx, &sSt, TRUE);
          }
          else if (lpDt->month != 0 && lpDt->day != 0)
          {
            sSt.wYear = (WORD)(lpDt->year);
            sSt.wMonth = (WORD)(lpDt->month);
            sSt.wDay = (WORD)(lpDt->day);
            sSt.wHour = (WORD)(lpDt->hour);
            sSt.wMinute = (WORD)(lpDt->minute);
            sSt.wSecond = (WORD)(lpDt->second);
            sSt.wMilliseconds = (WORD)(lpDt->second_part);

            CJavascriptVM::PushDate(lpCtx, &sSt, TRUE);
          }
          else
          {
            DukTape::duk_push_null(lpCtx);
          }
          }
          break;

        case MYSQL_TYPE_NULL:
          DukTape::duk_push_null(lpCtx);
          break;

        default:
          DukTape::duk_push_undefined(lpCtx);
          break;
      }
    }
    else
    {
      DukTape::duk_push_null(lpCtx);
    }
    if (lpFieldInfo->cStrNameA.IsEmpty() != FALSE)
    {
      DukTape::duk_put_prop_index(lpCtx, -2, (DukTape::duk_uarridx_t)i);
    }
    else
    {
      DukTape::duk_dup(lpCtx, -1);
      DukTape::duk_put_prop_index(lpCtx, -3, (DukTape::duk_uarridx_t)i);
      DukTape::duk_put_prop_string(lpCtx, -2, (LPCSTR)(lpFieldInfo->cStrNameA));
    }
  }
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::BeginTransaction()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (lpInternal == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);
  DukTape::duk_push_lstring(lpCtx, "START TRANSACTION;", 18);
  return Query();
}

DukTape::duk_ret_t CJsMySqlPlugin::CommitTransaction()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (lpInternal == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);
  DukTape::duk_push_lstring(lpCtx, "COMMIT;", 7);
  return Query();
}

DukTape::duk_ret_t CJsMySqlPlugin::RollbackTransaction()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (lpInternal == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);
  DukTape::duk_push_lstring(lpCtx, "ROLLBACK;", 9);
  return Query();
}

DukTape::duk_ret_t CJsMySqlPlugin::getAffectedRows()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (lpInternal != NULL)
    DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)(jsmysql_data->nAffectedRows));
  else
    DukTape::duk_push_number(lpCtx, 0.0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::getInsertId()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (lpInternal != NULL)
    DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)(jsmysql_data->nLastInsertId));
  else
    DukTape::duk_push_number(lpCtx, 0.0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::getFieldsCount()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (lpInternal != NULL && jsmysql_data->lpResultSet != NULL)
    DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)(jsmysql_data->nFieldsCount));
  else
    DukTape::duk_push_uint(lpCtx, 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::getFields()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (lpInternal != NULL && jsmysql_data->lpResultSet != NULL)
  {
    Internals::CJsMySqlFieldInfo *lpFieldInfo;
    SIZE_T i, nCount;

    //create an array
    DukTape::duk_push_object(lpCtx);
    nCount = jsmysql_data->aFieldsList.GetCount();
    //add each field as the array index
    for (i = 0; i < nCount; i++)
    {
      lpFieldInfo = jsmysql_data->aFieldsList.GetElementAt(i);
      lpFieldInfo->PushThis();
      DukTape::duk_put_prop_index(lpCtx, -2, (DukTape::duk_uarridx_t)i);
    }
  }
  else
  {
    DukTape::duk_push_null(lpCtx);
  }
  return 1;
}

#ifdef _DEBUG
DukTape::duk_ret_t CJsMySqlPlugin::getUseDebugDll()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (sOptions.bUseDebugDll != FALSE) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::setUseDebugDll()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (DukTape::duk_get_top(lpCtx) != 1)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  sOptions.bUseDebugDll = (CJavascriptVM::GetInt(lpCtx, 0) != 0) ? TRUE : FALSE;
  return 0;
}
#endif //_DEBUG

VOID CJsMySqlPlugin::ThrowDbError(_In_opt_ LPCSTR filename, _In_opt_ DukTape::duk_int_t line, _In_opt_ BOOL bOnlyPush)
{
  DukTape::duk_context *lpCtx = GetContext();
  LPCSTR sA;
  int err;

  DukTape::duk_get_global_string(lpCtx, "MySqlError");

  if (jsmysql_data->lpStmt != NULL)
    err = (int)_CALLAPI(mysql_stmt_errno)(jsmysql_data->lpStmt);
  else
    err = (int)_CALLAPI(mysql_errno)(jsmysql_data->lpDB);
  if (err == 0)
    err = ER_UNKNOWN_ERROR;
  DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)HResultFromMySqlErr(err));
  DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)err);

  if (jsmysql_data->lpStmt != NULL)
    sA = _CALLAPI(mysql_stmt_error)(jsmysql_data->lpStmt);
  else
    sA = _CALLAPI(mysql_error)(jsmysql_data->lpDB);
  DukTape::duk_push_string(lpCtx, (sA != NULL) ? sA : "");

  if (jsmysql_data->lpStmt != NULL)
    sA = _CALLAPI(mysql_stmt_sqlstate)(jsmysql_data->lpStmt);
  else
    sA = _CALLAPI(mysql_sqlstate)(jsmysql_data->lpDB);
  DukTape::duk_push_string(lpCtx, (sA != NULL) ? sA : "");

  DukTape::duk_new(lpCtx, 4);

  if (filename != NULL && *filename != 0)
    DukTape::duk_push_string(lpCtx, filename);
  else
    DukTape::duk_push_undefined(lpCtx);
  DukTape::duk_put_prop_string(lpCtx, -2, "fileName");

  if (filename != NULL && *filename != 0 && line != 0)
    DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)line);
  else
    DukTape::duk_push_undefined(lpCtx);
  DukTape::duk_put_prop_string(lpCtx, -2, "lineNumber");

  if (bOnlyPush == FALSE)
    DukTape::duk_throw_raw(lpCtx);
  return;
}

HRESULT CJsMySqlPlugin::HResultFromMySqlErr(_In_ int nError)
{
  if (nError == 0)
    return S_OK;
  switch (nError)
  {
    case CR_TCP_CONNECTION:
    case CR_SERVER_LOST:
    case CR_SERVER_GONE_ERROR:
    case CR_SERVER_LOST_EXTENDED:
      return MX_E_BrokenPipe;

    case CR_NULL_POINTER:
      return E_POINTER;

    case CR_OUT_OF_MEMORY:
    case ER_OUTOFMEMORY:
    case ER_OUT_OF_SORTMEMORY:
    case ER_OUT_OF_RESOURCES:
      return E_OUTOFMEMORY;

    case CR_UNSUPPORTED_PARAM_TYPE:
      return MX_E_Unsupported;

    case CR_FETCH_CANCELED:
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

} //namespace MX
