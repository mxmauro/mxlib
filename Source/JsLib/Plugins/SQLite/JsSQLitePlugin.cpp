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
#include "JsSQLitePluginCommon.h"
#include "..\..\..\..\Include\AutoPtr.h"
#include "..\..\..\..\Include\Strings\Utf8.h"
#include "..\..\..\..\Include\DateTime\DateTime.h"
#include <stdio.h>
#include <stdlib.h>

//-----------------------------------------------------------

#define jssqlite_data       ((Internals::CJsSQLitePluginData*)lpInternal)
#define _CALLAPI(apiname)  Internals::API::fn_##apiname

#define longlong  LONGLONG
#define ulonglong ULONGLONG

#define QUERY_FLAGS_HAS_RESULTS                       0x0001
#define QUERY_FLAGS_ON_FIRST_ROW                      0x0002
#define QUERY_FLAGS_EOF_REACHED                       0x0004

#define _FIELD_TYPE_Null                                   0
#define _FIELD_TYPE_Number                                 1
#define _FIELD_TYPE_Blob                                   2
#define _FIELD_TYPE_Text                                   3
#define _FIELD_TYPE_DateTime                               4

#define _DEFAULT_BUSY_TIMEOUT_MS                       30000
#define _DEFAULT_DB_CLOSE_TIMEOUT_MS                   10000

#define DATETIME_QUICK_FLAGS_Dot                        0x01
#define DATETIME_QUICK_FLAGS_Dash                       0x02
#define DATETIME_QUICK_FLAGS_Slash                      0x04
#define DATETIME_QUICK_FLAGS_Colon                      0x08

//-----------------------------------------------------------

static BOOL GetTypeFromName(_In_z_ LPCSTR szNameA, _Out_ int *lpnType, _Out_ int *lpnRealType);
static BOOL MustRetry(_In_ int err, _In_ DWORD dwInitialBusyTimeoutMs, _Inout_ DWORD &dwBusyTimeoutMs);

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CJsSQLitePluginData : public CBaseMemObj
{
public:
  CJsSQLitePluginData() : CBaseMemObj()
    {
    lpDB = NULL;
    lpStmt = NULL;
    nAffectedRows = 0ui64;
    nLastInsertId = 0ui64;
    nFieldsCount = 0;
    dwFlags = 0;
    return;
    };

  ~CJsSQLitePluginData()
    {
    if (lpDB != NULL)
      JsSQLiteDbManager::Close(lpDB);
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
  sqlite3 *lpDB;
  sqlite3_stmt *lpStmt;
  ULONGLONG nAffectedRows;
  ULONGLONG nLastInsertId;
  SIZE_T nFieldsCount;
  DWORD dwFlags;
  TArrayListWithRelease<CJsSQLiteFieldInfo*> aFieldsList;
};

//-----------------------------------------------------------

class CAutoLockDB : public MX::CBaseMemObj
{
public:
  CAutoLockDB(_In_opt_ CJsSQLitePluginData *lpData) : MX::CBaseMemObj()
    {
    mtx = (lpData != NULL) ? sqlite3_db_mutex(lpData->lpDB) : NULL;
    if (mtx != NULL)
      sqlite3_mutex_enter(mtx);
    return;
    };

  ~CAutoLockDB()
    {
    if (mtx != NULL)
      sqlite3_mutex_leave(mtx);
    return;
    };

private:
  sqlite3_mutex *mtx;
};


} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

namespace MX {

CJsSQLitePlugin::CJsSQLitePlugin(_In_ DukTape::duk_context *lpCtx) : CJsObjectBase(lpCtx)
{
  lpInternal = NULL;
  sOptions.bDontCreate = FALSE;
  sOptions.bReadOnly = FALSE;
  sOptions.dwBusyTimeoutMs = _DEFAULT_BUSY_TIMEOUT_MS;
  sOptions.dwCloseTimeoutMs = _DEFAULT_DB_CLOSE_TIMEOUT_MS;

  //has options in constructor?
  if (DukTape::duk_get_top(lpCtx) > 0)
  {
    if (duk_is_object(lpCtx, 0) == 1)
    {
      int timeout;

      //don't create
      DukTape::duk_get_prop_string(lpCtx, 0, "create");
      if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
      {
        if (DukTape::duk_is_boolean(lpCtx, -1) != 0)
          sOptions.bDontCreate = (DukTape::duk_require_boolean(lpCtx, -1) != 0) ? FALSE : TRUE;
        else
          sOptions.bDontCreate = (DukTape::duk_require_int(lpCtx, -1) != 0) ? FALSE : TRUE;
      }
      DukTape::duk_pop(lpCtx);
      //read-only
      DukTape::duk_get_prop_string(lpCtx, 0, "readOnly");
      if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
      {
        if (DukTape::duk_is_boolean(lpCtx, -1) != 0)
          sOptions.bReadOnly = (DukTape::duk_require_boolean(lpCtx, -1) != 0) ? TRUE : FALSE;
        else
          sOptions.bReadOnly = (DukTape::duk_require_int(lpCtx, -1) != 0) ? TRUE : FALSE;
      }
      DukTape::duk_pop(lpCtx);
      //busy timeout ms
      DukTape::duk_get_prop_string(lpCtx, 0, "busyTimeoutMs");
      if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
      {
        if (DukTape::duk_is_boolean(lpCtx, -1) != 0)
          timeout = (DukTape::duk_require_boolean(lpCtx, -1) != 0) ? 1 : 0;
        else
          timeout = (int)DukTape::duk_require_int(lpCtx, -1);
        if (timeout < 0)
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
        sOptions.dwBusyTimeoutMs = (DWORD)timeout;
      }
      //close timeout ms
      DukTape::duk_get_prop_string(lpCtx, 0, "closeTimeoutMs");
      if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
      {
        if (DukTape::duk_is_boolean(lpCtx, -1) != 0)
          timeout = (DukTape::duk_require_boolean(lpCtx, -1) != 0) ? 1 : 0;
        else
          timeout = (int)DukTape::duk_require_int(lpCtx, -1);
        if (timeout < 0)
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
        sOptions.dwCloseTimeoutMs = (DWORD)timeout;
      }
      //----
      DukTape::duk_pop(lpCtx);
    }
    else if (duk_is_null_or_undefined(lpCtx, 0) == 0)
    {
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
    }
  }
  return;
}

CJsSQLitePlugin::~CJsSQLitePlugin()
{
  Disconnect();
  return;
}

VOID CJsSQLitePlugin::OnRegister(_In_ DukTape::duk_context *lpCtx)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  HRESULT hRes;

  DukTape::duk_eval_raw(lpCtx, "function SQLiteError(_hr, _dbError, _dbErrorMsg) {\r\n"
                                 "WindowsError.call(this, _hr);\r\n"
                                 "if (_dbErrorMsg.length > 0)\r\n"
                                 "    this.message = _dbErrorMsg;\r\n"
                                 "else\r\n"
                                 "    this.message = \"General failure\";\r\n"
                                 "this.name = \"SQLiteError\";\r\n"
                                 "this.dbError = _dbError;\r\n"
                                 "return this; }\r\n"
                               "SQLiteError.prototype = Object.create(WindowsError.prototype);\r\n"
                               "SQLiteError.prototype.constructor=SQLiteError;\r\n", 0,
                        DUK_COMPILE_EVAL | DUK_COMPILE_NOSOURCE | DUK_COMPILE_STRLEN | DUK_COMPILE_NOFILENAME);

  hRes = lpJVM->RegisterException("SQLiteError", [](_In_ DukTape::duk_context *lpCtx,
                                                    _In_ DukTape::duk_idx_t nExceptionObjectIndex) -> VOID
  {
    throw CJsSQLiteError(lpCtx, nExceptionObjectIndex);
    return;
  });
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  return;
}

VOID CJsSQLitePlugin::OnUnregister(_In_ DukTape::duk_context *lpCtx)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);

  lpJVM->UnregisterException("SQLiteError");

  DukTape::duk_push_global_object(lpCtx);
  DukTape::duk_del_prop_string(lpCtx, -1, "SQLiteError");
  DukTape::duk_pop(lpCtx);
  return;
}

DukTape::duk_ret_t CJsSQLitePlugin::Connect()
{
  DukTape::duk_context *lpCtx = GetContext();
  LPCSTR szDatabaseNameA;
  HRESULT hRes;

  Disconnect();
  //get parameters
  if (DukTape::duk_get_top(lpCtx) != 1)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  szDatabaseNameA = DukTape::duk_require_string(lpCtx, 0);
  //initialize APIs
  hRes = Internals::API::SQLiteInitialize();
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  //create internal object
  lpInternal = MX_DEBUG_NEW Internals::CJsSQLitePluginData();
  if (lpInternal == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
  //----
  hRes = Internals::JsSQLiteDbManager::Open(szDatabaseNameA, sOptions.bReadOnly, sOptions.bDontCreate,
                                            sOptions.dwCloseTimeoutMs, &(jssqlite_data->lpDB));
  if (FAILED(hRes))
  {
    delete jssqlite_data;
    lpInternal = NULL;
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
  }

  //done
  return 0;
}

DukTape::duk_ret_t CJsSQLitePlugin::Disconnect()
{
  if (lpInternal != NULL)
  {
    QueryClose();
    delete jssqlite_data;
    lpInternal = NULL;
  }
  //done
  return 0;
}

DukTape::duk_ret_t CJsSQLitePlugin::Query()
{
  Internals::CAutoLockDB(jssqlite_data);
  DukTape::duk_context *lpCtx = GetContext();
  LPCSTR szQueryA;
  SIZE_T nQueryLegth;
  DukTape::duk_idx_t nParamsCount;
  const char *szLeftOverA;
  int err, nInputParams;
  DWORD dwBusyTimeoutMs;

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
  dwBusyTimeoutMs = sOptions.dwBusyTimeoutMs;
  do
  {
    err = sqlite3_prepare_v2(jssqlite_data->lpDB, szQueryA, (int)nQueryLegth, &(jssqlite_data->lpStmt), &szLeftOverA);
  }
  while (MustRetry(err, sOptions.dwBusyTimeoutMs, dwBusyTimeoutMs) != FALSE);
  if (err != SQLITE_OK)
  {
    jssqlite_data->lpStmt = NULL;
    ThrowDbError(__FILE__, __LINE__);
  }

  try
  {
    //parse input parameters
    nInputParams = sqlite3_bind_parameter_count(jssqlite_data->lpStmt);
    if (nInputParams < 0)
      nInputParams = 0;
    if (nInputParams > 0)
    {
      MX::CStringA cStrTypeA;
      DukTape::duk_size_t nLen;
      DukTape::duk_idx_t idx;

      if (nParamsCount != (DukTape::duk_idx_t)nInputParams + 1)
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);

      for (idx = 1; idx < nParamsCount; idx++)
      {
        LPVOID s;

        switch (DukTape::duk_get_type(lpCtx, idx))
        {
          case DUK_TYPE_NONE:
          case DUK_TYPE_UNDEFINED:
          case DUK_TYPE_NULL:
            err = sqlite3_bind_null(jssqlite_data->lpStmt, (int)idx);
            if (err != SQLITE_OK)
              ThrowDbError(__FILE__, __LINE__);
            break;

          case DUK_TYPE_BOOLEAN:
            err = sqlite3_bind_int(jssqlite_data->lpStmt, (int)idx, DukTape::duk_get_boolean(lpCtx, idx) ? 1 : 0);
            if (err != SQLITE_OK)
              ThrowDbError(__FILE__, __LINE__);
            break;

          case DUK_TYPE_NUMBER:
            {
              double dbl;
              LONGLONG ll;

              dbl = (double)DukTape::duk_get_number(lpCtx, idx);
              ll = (LONGLONG)dbl;
              if ((double)ll != dbl || dbl < (double)LONG_MIN)
              {
                err = sqlite3_bind_double(jssqlite_data->lpStmt, (int)idx, dbl);
                if (err != SQLITE_OK)
                  ThrowDbError(__FILE__, __LINE__);
              }
              else if (ll >= (LONGLONG)LONG_MIN || ll <= (LONGLONG)LONG_MAX)
              {
                err = sqlite3_bind_int(jssqlite_data->lpStmt, (int)idx, (LONG)ll);
                if (err != SQLITE_OK)
                  ThrowDbError(__FILE__, __LINE__);
              }
              else
              {
                err = sqlite3_bind_int64(jssqlite_data->lpStmt, (int)idx, ll);
                if (err != SQLITE_OK)
                  ThrowDbError(__FILE__, __LINE__);
              }
            }
            break;

          case DUK_TYPE_STRING:
            s = (LPVOID)DukTape::duk_get_lstring(lpCtx, idx, &nLen);
            err = sqlite3_bind_text(jssqlite_data->lpStmt, (int)idx, (const char*)s, (int)nLen, SQLITE_TRANSIENT);
            if (err != SQLITE_OK)
              ThrowDbError(__FILE__, __LINE__);
            break;

          case DUK_TYPE_OBJECT:
            CJavascriptVM::GetObjectType(lpCtx, idx, cStrTypeA);
            if (StrCompareA((LPCSTR)cStrTypeA, "Date") == 0)
            {
              SYSTEMTIME sSt;
              CHAR szBufA[32];

              CJavascriptVM::GetDate(lpCtx, idx, &sSt);
              _snprintf_s(szBufA, MX_ARRAYLEN(szBufA), _TRUNCATE, "%04u-%02u-%02u %02u:%02u:%02u.%03u",
                          sSt.wYear, sSt.wMonth, sSt.wDay, sSt.wHour, sSt.wMinute, sSt.wSecond, sSt.wMilliseconds);
              err = sqlite3_bind_text(jssqlite_data->lpStmt, (int)idx, szBufA, (int)MX::StrLenA(szBufA),
                                      SQLITE_TRANSIENT);
              if (err != SQLITE_OK)
                ThrowDbError(__FILE__, __LINE__);
            }
            else
            {
              MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
            }
            break;

          case DUK_TYPE_BUFFER:
            s = DukTape::duk_get_buffer(lpCtx, idx, &nLen);
            if (nLen < 0x7FFFFFFFL)
            {
              err = sqlite3_bind_blob(jssqlite_data->lpStmt, (int)idx, s, (int)nLen, SQLITE_TRANSIENT);
              if (err != SQLITE_OK)
                ThrowDbError(__FILE__, __LINE__);
            }
            else
            {
              err = sqlite3_bind_blob64(jssqlite_data->lpStmt, (int)idx, s, (sqlite3_uint64)nLen, SQLITE_TRANSIENT);
              if (err != SQLITE_OK)
                ThrowDbError(__FILE__, __LINE__);
            }
            break;

          default:
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
        }
      }
    }

    //execute query and optionally get first row
    dwBusyTimeoutMs = sOptions.dwBusyTimeoutMs;
    do
    {
      err = sqlite3_step(jssqlite_data->lpStmt);
    }
    while (MustRetry(err, sOptions.dwBusyTimeoutMs, dwBusyTimeoutMs) != FALSE);
    switch (err)
    {
      case SQLITE_ROW:
        jssqlite_data->dwFlags = QUERY_FLAGS_HAS_RESULTS | QUERY_FLAGS_ON_FIRST_ROW;
        break;

      case SQLITE_SCHEMA:
      case SQLITE_DONE:
      case SQLITE_OK:
        break;

      default:
        ThrowDbError(__FILE__, __LINE__);
    }

    //get some data
    jssqlite_data->nLastInsertId = (ULONGLONG)sqlite3_last_insert_rowid(jssqlite_data->lpDB);
    jssqlite_data->nAffectedRows = (ULONGLONG)sqlite3_changes(jssqlite_data->lpDB);

    //get fields count
    if ((jssqlite_data->dwFlags & QUERY_FLAGS_HAS_RESULTS) != 0)
    {
      jssqlite_data->nFieldsCount = (SIZE_T)sqlite3_column_count(jssqlite_data->lpStmt);
      if (jssqlite_data->nFieldsCount > 0)
      {
        TAutoRefCounted<Internals::CJsSQLiteFieldInfo> cFieldInfo;
        SIZE_T i;

        //build fields
        for (i = 0; i < jssqlite_data->nFieldsCount; i++)
        {
          LPCSTR sA;

          cFieldInfo.Attach(MX_DEBUG_NEW Internals::CJsSQLiteFieldInfo(lpCtx));
          if (!cFieldInfo)
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          //name
          sA = sqlite3_column_name(jssqlite_data->lpStmt, (int)i);
          if (cFieldInfo->cStrNameA.Copy((sA != NULL) ? sA : "") == FALSE)
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          //original name
          sA = sqlite3_column_origin_name(jssqlite_data->lpStmt, (int)i);
          if (sA != NULL && *sA != 0)
          {
            if (cFieldInfo->cStrOriginalNameA.Copy(sA) == FALSE)
              MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          }
          //table
          sA = sqlite3_column_table_name(jssqlite_data->lpStmt, (int)i);
          if (sA != NULL && *sA != 0)
          {
            if (cFieldInfo->cStrTableA.Copy(sA) == FALSE)
              MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          }
          //database
          sA = sqlite3_column_database_name(jssqlite_data->lpStmt, (int)i);
          if (sA != NULL && *sA != 0)
          {
            if (cFieldInfo->cStrDatabaseA.Copy(sA) == FALSE)
              MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          }

          sA = sqlite3_column_decltype(jssqlite_data->lpStmt, (int)i);
          if (sA == NULL || *sA == 0 || GetTypeFromName(sA, &(cFieldInfo->nType), &(cFieldInfo->nRealType)) == FALSE)
          {
            switch (sqlite3_column_type(jssqlite_data->lpStmt, (int)i))
            {
              case SQLITE_INTEGER:
                cFieldInfo->nType = _FIELD_TYPE_Number;
                cFieldInfo->nRealType = SQLITE_INTEGER;
                break;

              case SQLITE_FLOAT:
                cFieldInfo->nType = _FIELD_TYPE_Number;
                cFieldInfo->nRealType = SQLITE_FLOAT;
                break;

              case SQLITE_BLOB:
                cFieldInfo->nType = _FIELD_TYPE_Blob;
                cFieldInfo->nRealType = SQLITE_BLOB;
                break;

              case SQLITE_TEXT:
                cFieldInfo->nType = _FIELD_TYPE_Text;
                cFieldInfo->nRealType = SQLITE_TEXT;
                break;

              default:
                cFieldInfo->nType = _FIELD_TYPE_Null;
                cFieldInfo->nRealType = SQLITE_NULL;
                break;
            }
          }

          //get retrieved field row type
          if (cFieldInfo->nRealType != SQLITE_NULL)
            cFieldInfo->nCurrType = sqlite3_column_type(jssqlite_data->lpStmt, (int)i);
          else
            cFieldInfo->nCurrType = SQLITE_NULL;

          //add to list
          if (jssqlite_data->aFieldsList.AddElement(cFieldInfo.Get()) == FALSE)
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          cFieldInfo.Detach();
        }
      }
    }
  }
  catch (...)
  {
    QueryClose();
    throw;
  }

  //done
  return 0;
}

DukTape::duk_ret_t CJsSQLitePlugin::QueryAndFetch()
{
  Query();
  return FetchRow();
}

DukTape::duk_ret_t CJsSQLitePlugin::QueryClose()
{
  if (lpInternal != NULL)
  {
    if (jssqlite_data->lpStmt != NULL)
    {
      sqlite3_finalize(jssqlite_data->lpStmt);
      jssqlite_data->lpStmt = NULL;
    }
    jssqlite_data->aFieldsList.RemoveAllElements();
    jssqlite_data->nFieldsCount = 0;
    jssqlite_data->dwFlags = 0;
  }
  //done
  return 0;
}

DukTape::duk_ret_t CJsSQLitePlugin::EscapeString()
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

  while (*szStrA != 0)
  {
    switch (*szStrA)
    {
      case '\'':
        if (cStrResultA.ConcatN("''", 2) == FALSE)
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
        break;
      case '\\':
        if (bIsLikeStatement != FALSE && (szStrA[1] == '%' || szStrA[1] == '_'))
        {
          szStrA++;
          if (cStrResultA.AppendFormat("\\%c", *szStrA) == FALSE)
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          break;
        }
        //fall into default
      default:
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

DukTape::duk_ret_t CJsSQLitePlugin::Utf8Truncate()
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

DukTape::duk_ret_t CJsSQLitePlugin::FetchRow()
{
  Internals::CAutoLockDB(jssqlite_data);
  DukTape::duk_context *lpCtx = GetContext();
  Internals::CJsSQLiteFieldInfo *lpFieldInfo;
  SIZE_T i, nFieldsCount;
  int err;

  if (lpInternal == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);

  nFieldsCount = jssqlite_data->aFieldsList.GetCount();
  if ((jssqlite_data->dwFlags & QUERY_FLAGS_HAS_RESULTS) == 0 ||
      (jssqlite_data->dwFlags & QUERY_FLAGS_EOF_REACHED) != 0 ||
      nFieldsCount == 0)
  {
    DukTape::duk_push_null(lpCtx);
    return 1;
  }

  if ((jssqlite_data->dwFlags & QUERY_FLAGS_ON_FIRST_ROW) != 0)
  {
    //first row is retrieved when executing the statement
    //so, on the first QueryFetchNextRow do a no-op
    jssqlite_data->dwFlags &= ~QUERY_FLAGS_ON_FIRST_ROW;
    (jssqlite_data->nAffectedRows)++;
  }
  else
  {
    DWORD dwBusyTimeoutMs;

    dwBusyTimeoutMs = sOptions.dwBusyTimeoutMs;
    do
    {
      err = sqlite3_step(jssqlite_data->lpStmt);
    }
    while (MustRetry(err, sOptions.dwBusyTimeoutMs, dwBusyTimeoutMs) != FALSE);
    switch (err)
    {
      case SQLITE_ROW:
        (jssqlite_data->nAffectedRows)++;
        //get retrieved field row type
        for (i = 0; i < nFieldsCount; i++)
        {
          lpFieldInfo = jssqlite_data->aFieldsList.GetElementAt(i);
          if (lpFieldInfo->nRealType != SQLITE_NULL)
            lpFieldInfo->nCurrType = sqlite3_column_type(jssqlite_data->lpStmt, (int)i);
        }
        break;

      case SQLITE_DONE:
      case SQLITE_OK:
        jssqlite_data->dwFlags |= QUERY_FLAGS_EOF_REACHED;
        break;

      default:
        ThrowDbError(__FILE__, __LINE__);
    }
  }

  if ((jssqlite_data->dwFlags & QUERY_FLAGS_EOF_REACHED) != 0)
  {
    DukTape::duk_push_null(lpCtx);
    return 1;
  }

  //create an object for each result
  DukTape::duk_push_object(lpCtx);
  for (i = 0; i < nFieldsCount; i++)
  {
    lpFieldInfo = jssqlite_data->aFieldsList.GetElementAt(i);
    if (lpFieldInfo->nRealType != SQLITE_NULL && lpFieldInfo->nCurrType != SQLITE_NULL)
    {
      union {
        sqlite_int64 i64;
        double dbl;
        char *sA;
        LPBYTE p;
      } val;
      LPBYTE d;
      int val_len;

      switch (lpFieldInfo->nType)
      {
        case _FIELD_TYPE_Number:
          if (lpFieldInfo->nCurrType == SQLITE_INTEGER)
          {
            val.i64 = sqlite3_column_int64(jssqlite_data->lpStmt, (int)i);
            if (val.i64 >= (LONGLONG)LONG_MIN && val.i64 <= (LONGLONG)LONG_MAX)
              DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)(val.i64));
            else
              DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)(val.i64));
          }
          else
          {
            val.dbl = sqlite3_column_double(jssqlite_data->lpStmt, (int)i);

            DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)(val.dbl));
          }
          break;

        case _FIELD_TYPE_Blob:
          val.p = (LPBYTE)sqlite3_column_blob(jssqlite_data->lpStmt, (int)i);
          val_len = sqlite3_column_bytes(jssqlite_data->lpStmt, (int)i);

          d = (LPBYTE)(DukTape::duk_push_fixed_buffer(lpCtx, (DukTape::duk_size_t)val_len));

          MemCopy(d, val.p, (SIZE_T)val_len);
          DukTape::duk_push_buffer_object(lpCtx, -1, 0, (DukTape::duk_size_t)val_len, DUK_BUFOBJ_UINT8ARRAY);
          DukTape::duk_remove(lpCtx, -2);
          break;

        case _FIELD_TYPE_Text:
          val.sA = (char*)sqlite3_column_text(jssqlite_data->lpStmt, (int)i);
          val_len = sqlite3_column_bytes(jssqlite_data->lpStmt, (int)i);

          DukTape::duk_push_lstring(lpCtx, (const char*)(val.sA), (DukTape::duk_size_t)val_len);
          break;

        case _FIELD_TYPE_DateTime:
          {
          CDateTime cDt;
          CStringW cStrTempW;
          SYSTEMTIME sSt;
          int flags;
          HRESULT hRes;

          val.sA = (char*)sqlite3_column_text(jssqlite_data->lpStmt, (int)i);
          val_len = sqlite3_column_bytes(jssqlite_data->lpStmt, (int)i);

          for (int k = flags = 0; k < val_len; k++)
          {
            switch (val.sA[k])
            {
              case '/':
                flags |= DATETIME_QUICK_FLAGS_Slash;
                break;
              case '-':
                flags |= DATETIME_QUICK_FLAGS_Dash;
                break;
              case '.':
                flags |= DATETIME_QUICK_FLAGS_Dot;
                break;
              case ':':
                flags |= DATETIME_QUICK_FLAGS_Colon;
                break;
            }
          }
          hRes = Utf8_Decode(cStrTempW, val.sA, val_len);
          if (FAILED(hRes))
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);

          switch (flags)
          {
            case DATETIME_QUICK_FLAGS_Dash | DATETIME_QUICK_FLAGS_Colon | DATETIME_QUICK_FLAGS_Dot:
              hRes = cDt.SetFromString((LPCWSTR)cStrTempW, L"%Y-%m-%d %H:%M:%S.%f");
              if (FAILED(hRes) && hRes != E_OUTOFMEMORY)
                hRes = cDt.SetFromString((LPCWSTR)cStrTempW, L"%Y-%m-%d %H:%M:%S.");
fetchrow_set_datetime:
              if (FAILED(hRes))
                MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
              cDt.GetSystemTime(sSt);
              MX::CJavascriptVM::PushDate(lpCtx, &sSt, TRUE);
              break;

            case DATETIME_QUICK_FLAGS_Slash | DATETIME_QUICK_FLAGS_Colon | DATETIME_QUICK_FLAGS_Dot:
              hRes = cDt.SetFromString((LPCWSTR)cStrTempW, L"%Y/%m/%d %H:%M:%S.%f");
              if (FAILED(hRes) && hRes != E_OUTOFMEMORY)
                hRes = cDt.SetFromString((LPCWSTR)cStrTempW, L"%Y/%m/%d %H:%M:%S.");
              goto fetchrow_set_datetime;

            case DATETIME_QUICK_FLAGS_Dash | DATETIME_QUICK_FLAGS_Colon:
              hRes = cDt.SetFromString((LPCWSTR)cStrTempW, L"%Y-%m-%d %H:%M:%S");
              goto fetchrow_set_datetime;

            case DATETIME_QUICK_FLAGS_Slash | DATETIME_QUICK_FLAGS_Colon:
              hRes = cDt.SetFromString((LPCWSTR)cStrTempW, L"%Y/%m/%d %H:%M:%S");
              goto fetchrow_set_datetime;

            case DATETIME_QUICK_FLAGS_Dash:
              hRes = cDt.SetFromString((LPCWSTR)cStrTempW, L"%Y-%m-%d");
fetchrow_set_date:
              if (FAILED(hRes))
                MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
              cDt.GetSystemTime(sSt);
              sSt.wHour = sSt.wMinute = sSt.wSecond = sSt.wMilliseconds = 0;
              MX::CJavascriptVM::PushDate(lpCtx, &sSt, TRUE);
              break;

            case DATETIME_QUICK_FLAGS_Slash:
              hRes = cDt.SetFromString((LPCWSTR)cStrTempW, L"%Y/%m/%d");
              goto fetchrow_set_date;

            case DATETIME_QUICK_FLAGS_Colon | DATETIME_QUICK_FLAGS_Dot:
              hRes = cDt.SetFromString((LPCWSTR)cStrTempW, L"%H:%M:%S.%f");
              if (FAILED(hRes) && hRes != E_OUTOFMEMORY)
                hRes = cDt.SetFromString((LPCWSTR)cStrTempW, L"%H:%M:%S.");
fetchrow_set_time:
              if (FAILED(hRes))
                MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
              ::GetSystemTime(&sSt);
              sSt.wHour = (WORD)(cDt.GetHours());
              sSt.wMinute = (WORD)(cDt.GetMinutes());
              sSt.wSecond = (WORD)(cDt.GetSeconds());
              sSt.wMilliseconds = (WORD)(cDt.GetMilliSeconds());
              MX::CJavascriptVM::PushDate(lpCtx, &sSt, TRUE);
              break;

            case DATETIME_QUICK_FLAGS_Colon:
              hRes = cDt.SetFromString((LPCWSTR)cStrTempW, L"%H:%M:%S");
              goto fetchrow_set_time;

            default:
              MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_InvalidData);
              break;
          }
          }
          break;

        default:
          DukTape::duk_push_null(lpCtx);
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

DukTape::duk_ret_t CJsSQLitePlugin::BeginTransaction()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (lpInternal == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);
  DukTape::duk_push_lstring(lpCtx, "BEGIN TRANSACTION;", 18);
  return Query();
}

DukTape::duk_ret_t CJsSQLitePlugin::CommitTransaction()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (lpInternal == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);
  DukTape::duk_push_lstring(lpCtx, "COMMIT TRANSACTION;", 19);
  return Query();
}

DukTape::duk_ret_t CJsSQLitePlugin::RollbackTransaction()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (lpInternal == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);
  DukTape::duk_push_lstring(lpCtx, "ROLLBACK TRANSACTION;", 21);
  return Query();
}

DukTape::duk_ret_t CJsSQLitePlugin::getAffectedRows()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (lpInternal != NULL)
    DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)(jssqlite_data->nAffectedRows));
  else
    DukTape::duk_push_number(lpCtx, 0.0);
  return 1;
}

DukTape::duk_ret_t CJsSQLitePlugin::getInsertId()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (lpInternal != NULL)
    DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)(jssqlite_data->nLastInsertId));
  else
    DukTape::duk_push_number(lpCtx, 0.0);
  return 1;
}

DukTape::duk_ret_t CJsSQLitePlugin::getFieldsCount()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (lpInternal != NULL && (jssqlite_data->dwFlags & QUERY_FLAGS_HAS_RESULTS) != 0)
    DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)(jssqlite_data->nFieldsCount));
  else
    DukTape::duk_push_uint(lpCtx, 0);
  return 1;
}

DukTape::duk_ret_t CJsSQLitePlugin::getFields()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (lpInternal != NULL && (jssqlite_data->dwFlags & QUERY_FLAGS_HAS_RESULTS) != 0)
  {
    Internals::CJsSQLiteFieldInfo *lpFieldInfo;
    SIZE_T i, nCount;

    //create an array
    DukTape::duk_push_object(lpCtx);
    nCount = jssqlite_data->aFieldsList.GetCount();
    //add each field as the array index
    for (i = 0; i < nCount; i++)
    {
      lpFieldInfo = jssqlite_data->aFieldsList.GetElementAt(i);
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

VOID CJsSQLitePlugin::ThrowDbError(_In_opt_ LPCSTR filename, _In_opt_ DukTape::duk_int_t line)
{
  DukTape::duk_context *lpCtx = GetContext();
  LPCSTR sA;
  int err;

  err = sqlite3_extended_errcode(jssqlite_data->lpDB);
  if (err == 0)
    err = SQLITE_ERROR;
  if (err == SQLITE_NOMEM)
    MX::CJavascriptVM::ThrowWindowsError(lpCtx, E_OUTOFMEMORY, filename, line);

  sA = sqlite3_errmsg(jssqlite_data->lpDB);
  if (sA == NULL || *sA == 0)
    sA = sqlite3_errstr(err);

  DukTape::duk_get_global_string(lpCtx, "SQLiteError");
  DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)HResultFromSQLiteErr(err));
  DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)err);
  DukTape::duk_push_string(lpCtx, (sA != NULL) ? sA : "");
  DukTape::duk_new(lpCtx, 3);

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

  DukTape::duk_throw_raw(lpCtx);
  return;
}

HRESULT CJsSQLitePlugin::HResultFromSQLiteErr(_In_ int nError)
{
  if (nError == 0)
    return S_OK;
  switch (nError & 0xFF)
  {
    case SQLITE_NOMEM:
      return E_OUTOFMEMORY;

    case SQLITE_PERM:
    case SQLITE_AUTH:
      return E_ACCESSDENIED;

    case SQLITE_ABORT:
      return MX_E_Cancelled;

    case SQLITE_BUSY:
      return MX_E_Busy;

    case SQLITE_NOTFOUND:
      return MX_E_NotFound;

    case SQLITE_CONSTRAINT:
      switch (nError)
      {
        case SQLITE_CONSTRAINT_FOREIGNKEY:
        case SQLITE_CONSTRAINT_PRIMARYKEY:
        case SQLITE_CONSTRAINT_UNIQUE:
        case SQLITE_CONSTRAINT_ROWID:
          return MX_E_DuplicateKey;
      }
      return MX_E_ConstraintsCheckFailed;
  }
  return E_FAIL;
}

} //namespace MX

//-----------------------------------------------------------

static BOOL GetTypeFromName(_In_z_ LPCSTR szNameA, _Out_ int *lpnType, _Out_ int *lpnRealType)
{
  static const struct tagTYPE {
    LPCSTR szName;
    int nType;
    int nRealType;
  } aTypesList[] = {
    { "AUTOINCREMENT",    _FIELD_TYPE_Number,    SQLITE_INTEGER },
    { "BIGINT",           _FIELD_TYPE_Number,    SQLITE_INTEGER },
    { "BINARY",           _FIELD_TYPE_Blob,      SQLITE_BLOB },
    { "BIT",              _FIELD_TYPE_Number,    SQLITE_INTEGER },
    { "BLOB",             _FIELD_TYPE_Blob,      SQLITE_BLOB },
    { "BOOL",             _FIELD_TYPE_Number,    SQLITE_INTEGER },
    { "CHAR",             _FIELD_TYPE_Text,      SQLITE_TEXT },
    { "CHARACTER",        _FIELD_TYPE_Text,      SQLITE_TEXT },
    { "COUNTER",          _FIELD_TYPE_Number,    SQLITE_INTEGER },
    { "CURRENCY",         _FIELD_TYPE_Number,    SQLITE_FLOAT },
    { "DATE",             _FIELD_TYPE_DateTime,  SQLITE_TEXT },
    { "DATETIME",         _FIELD_TYPE_DateTime,  SQLITE_TEXT },
    { "DECIMAL",          _FIELD_TYPE_Number,    SQLITE_FLOAT },
    { "DOUBLE",           _FIELD_TYPE_Number,    SQLITE_FLOAT },
    { "FLOAT",            _FIELD_TYPE_Number,    SQLITE_FLOAT },
    { "FLOAT4",           _FIELD_TYPE_Number,    SQLITE_FLOAT },
    { "FLOAT8",           _FIELD_TYPE_Number,    SQLITE_FLOAT },
    { "GENERAL",          _FIELD_TYPE_Blob,      SQLITE_BLOB },
    { "GUID",             _FIELD_TYPE_Text,      SQLITE_TEXT },
    { "IDENTITY",         _FIELD_TYPE_Number,    SQLITE_INTEGER },
    { "IMAGE",            _FIELD_TYPE_Blob,      SQLITE_BLOB },
    { "INT",              _FIELD_TYPE_Number,    SQLITE_INTEGER },
    { "INT2",             _FIELD_TYPE_Number,    SQLITE_INTEGER },
    { "INT4",             _FIELD_TYPE_Number,    SQLITE_INTEGER },
    { "INT8",             _FIELD_TYPE_Number,    SQLITE_INTEGER },
    { "INTEGER",          _FIELD_TYPE_Number,    SQLITE_INTEGER },
    { "LOGICAL",          _FIELD_TYPE_Number,    SQLITE_INTEGER },
    { "LONG",             _FIELD_TYPE_Number,    SQLITE_INTEGER },
    { "LONGCHAR",         _FIELD_TYPE_Text,      SQLITE_TEXT },
    { "LONGTEXT",         _FIELD_TYPE_Text,      SQLITE_TEXT },
    { "LONGVARCHAR",      _FIELD_TYPE_Text,      SQLITE_TEXT },
    { "MEMO",             _FIELD_TYPE_Text,      SQLITE_TEXT },
    { "MONEY",            _FIELD_TYPE_Number,    SQLITE_FLOAT },
    { "NCHAR",            _FIELD_TYPE_Text,      SQLITE_TEXT },
    { "NOTE",             _FIELD_TYPE_Text,      SQLITE_TEXT },
    { "NTEXT",            _FIELD_TYPE_Text,      SQLITE_TEXT },
    { "NUMERIC",          _FIELD_TYPE_Number,    SQLITE_FLOAT },
    { "NVARCHAR",         _FIELD_TYPE_Text,      SQLITE_TEXT },
    { "OLEOBJECT",        _FIELD_TYPE_Blob,      SQLITE_BLOB },
    { "REAL",             _FIELD_TYPE_Number,    SQLITE_FLOAT },
    { "SERIAL",           _FIELD_TYPE_Blob,      SQLITE_BLOB },
    { "SERIAL4",          _FIELD_TYPE_Blob,      SQLITE_BLOB },
    { "SMALLINT",         _FIELD_TYPE_Number,    SQLITE_INTEGER },
    { "STRING",           _FIELD_TYPE_Text,      SQLITE_TEXT },
    { "TEXT",             _FIELD_TYPE_Text,      SQLITE_TEXT },
    { "TIME",             _FIELD_TYPE_DateTime,  SQLITE_TEXT },
    { "TIMESTAMP",        _FIELD_TYPE_DateTime,  SQLITE_TEXT },
    { "TIMESTAMPTZ",      _FIELD_TYPE_DateTime,  SQLITE_TEXT },
    { "TIMETZ",           _FIELD_TYPE_DateTime,  SQLITE_TEXT },
    { "TINYINT",          _FIELD_TYPE_Number,    SQLITE_INTEGER },
    { "UNIQUEIDENTIFIER", _FIELD_TYPE_Text,      SQLITE_TEXT },
    { "VARBINARY",        _FIELD_TYPE_Blob,      SQLITE_BLOB },
    { "VARCHAR",          _FIELD_TYPE_Text,      SQLITE_TEXT },
    { "YESNO",            _FIELD_TYPE_Number,    SQLITE_INTEGER }
  };
  SIZE_T nHalf, nMin, nMax, nNameLen;

  for (nNameLen = 0; (szNameA[nNameLen] >= 'A' && szNameA[nNameLen] <= 'Z') ||
                     (szNameA[nNameLen] >= 'a' && szNameA[nNameLen] <= 'z'); nNameLen++);
  nMax = MX_ARRAYLEN(aTypesList);
  nMin = 0;
  while (nMin < nMax)
  {
    nHalf = nMin + (nMax-nMin) / 2;
    if (MX::StrNCompareA(aTypesList[nHalf].szName, szNameA, nNameLen, TRUE) < 0)
      nMin = nHalf + 1;
    else
      nMax = nHalf;
  }
  if (nMin == nMax && MX::StrNCompareA(aTypesList[nMin].szName, szNameA, nNameLen, TRUE) == 0)
  {
    *lpnType = aTypesList[nMin].nType;
    *lpnRealType = aTypesList[nMin].nRealType;
    return TRUE;
  }
  return FALSE;
}

static BOOL MustRetry(_In_ int err, _In_ DWORD dwInitialBusyTimeoutMs, _Inout_ DWORD &dwBusyTimeoutMs)
{
  if (err != SQLITE_BUSY || dwInitialBusyTimeoutMs == 0 || dwBusyTimeoutMs == 0)
    return FALSE;
  if (dwBusyTimeoutMs >= 100)
  {
    ::Sleep(100);
    dwBusyTimeoutMs -= 100;
  }
  else
  {
    ::Sleep(dwBusyTimeoutMs);
    dwBusyTimeoutMs = 0;
  }
  return TRUE;
}
