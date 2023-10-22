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
#include "..\..\Include\Database\SQLite3Connector.h"
#include "Internals\SQLite3\sqlite_cfg.h"
#include "Internals\SQLite3\sqlite3.h"
#include "..\..\Include\Finalizer.h"
#include "..\..\Include\WaitableObjects.h"
#include "..\..\Include\Strings\Utf8.h"
#include <stdio.h>
#include "..\..\Include\AutoPtr.h"

#define QUERY_FLAGS_HAS_RESULTS                       0x0001
#define QUERY_FLAGS_ON_FIRST_ROW                      0x0002
#define QUERY_FLAGS_EOF_REACHED                       0x0004

#define _FIELD_TYPE_Null                                   0
#define _FIELD_TYPE_Blob                                   1
#define _FIELD_TYPE_Text                                   2
#define _FIELD_TYPE_DateTime                               3
#define _FIELD_TYPE_Long                                   4
#define _FIELD_TYPE_LongLong                               5
#define _FIELD_TYPE_Double                                 6
#define _FIELD_TYPE_Boolean                                7

#define _DEFAULT_BUSY_TIMEOUT_MS                       30000

#define DATETIME_QUICK_FLAGS_Dot                        0x01
#define DATETIME_QUICK_FLAGS_Dash                       0x02
#define DATETIME_QUICK_FLAGS_Slash                      0x04
#define DATETIME_QUICK_FLAGS_Colon                      0x08

#define SQLITE_FINALIZER_PRIORITY 10001

//-----------------------------------------------------------

typedef struct {
  size_t len;
  size_t count;
  char *data;
  char *close;
} MYFUNC_CONCAT_CONTEXT;

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CSQLite3ConnectorData : public CBaseMemObj
{
public:
  CSQLite3ConnectorData();
  ~CSQLite3ConnectorData();

  VOID ClearErrno();
  VOID SetErrno(_In_ int err, _In_opt_ HRESULT hRes = S_OK);
  VOID SetCustomErrno(_In_ int err, _In_z_ LPCSTR szDescriptionA);

private:
  HRESULT GetHResultFromErr(_In_ int err, _In_ BOOL bCheckDB);

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
  DWORD dwBusyTimeoutMs;
  sqlite3 *lpDB;
  int nLastDbErr;
  HRESULT hLastDbRes;
  CStringA cStrLastDbErrorDescriptionA;
  sqlite3_stmt *lpStmt;
  TArrayListWithRelease<Database::CField*> aInputFieldsList;
  DWORD dwFlags;
};

//-----------------------------------------------------------

class CAutoLockSQLite3DB : public virtual CBaseMemObj
{
public:
  CAutoLockSQLite3DB(_In_opt_ CSQLite3ConnectorData *lpData) : CBaseMemObj()
    {
    mtx = (lpData != NULL) ? sqlite3_db_mutex(lpData->lpDB) : NULL;
    if (mtx != NULL)
      sqlite3_mutex_enter(mtx);
    return;
    };

  ~CAutoLockSQLite3DB()
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

#define sqlite3_data ((Internals::CSQLite3ConnectorData*)lpInternalData)

//-----------------------------------------------------------

static BOOL bInitialized = FALSE;
static MX::Database::CSQLite3Connector::CConnectOptions cDefaultConnectOptions;

//-----------------------------------------------------------

static HRESULT InitializeInternals();
static VOID SQLITE_Shutdown();

static void *my_mem_alloc(int nByte);
static void my_mem_free(void *pPrior);
static int my_mem_memsize(void *pPrior);
static void *my_mem_realloc(void *pPrior, int nByte);
static int my_mem_roundup(int n);
static int my_mem_init(void *NotUsed);
static void my_mem_shutdown(void *NotUsed);

static BOOL GetTypeFromName(_In_z_ LPCSTR szNameA, _Out_ PULONG lpnType, _Out_ PULONG lpnRealType);
static BOOL MustRetry(_In_ int err, _In_ DWORD dwInitialBusyTimeoutMs, _Inout_ DWORD &dwBusyTimeoutMs);

static void myFuncUtcNow(sqlite3_context *ctx, int argc, sqlite3_value **argv);
static void myFuncNow(sqlite3_context *ctx, int argc, sqlite3_value **argv);
//static void myFuncRegExp(sqlite3_context *ctx, int argc, sqlite3_value **argv);
static void myFuncConcatStep(sqlite3_context *ctx, int argc, sqlite3_value **argv);
static void myFuncConcatFinal(sqlite3_context *ctx);
static void myFuncFree(_In_ void *ptr);

static __inline BOOL IsSQLiteFatalError(int err)
{
  err &= 0xFF;
  return (err != SQLITE_OK && err != SQLITE_BUSY && err != SQLITE_LOCKED && err != SQLITE_CONSTRAINT &&
          err != SQLITE_MISMATCH && err != SQLITE_FULL && err != SQLITE_MISMATCH) ? TRUE : FALSE;
}

//-----------------------------------------------------------

namespace MX {

namespace Database {

CSQLite3Connector::CSQLite3Connector() : CBaseConnector()
{
  lpInternalData = NULL;
  return;
}

CSQLite3Connector::~CSQLite3Connector()
{
  Disconnect();
  return;
}

HRESULT CSQLite3Connector::Connect(_In_z_ LPCSTR szFileNameA, _In_opt_ CConnectOptions *lpOptions)
{
  CStringW cStrTempW;
  HRESULT hRes;

  if (szFileNameA == NULL)
    return E_POINTER;
  if (*szFileNameA == 0)
    return E_INVALIDARG;

  hRes = Utf8_Decode(cStrTempW, szFileNameA);
  if (SUCCEEDED(hRes))
    hRes = Connect((LPCWSTR)cStrTempW, lpOptions);

  //done
  return hRes;
}

HRESULT CSQLite3Connector::Connect(_In_z_ LPCWSTR szFileNameW, _In_opt_ CConnectOptions *lpOptions)
{
  CStringA cStrFileNameA;
  BOOL bFileExists;
  DWORD dwBusyTimeoutMs;
  int err, flags;
  HRESULT hRes;

  if (szFileNameW == NULL)
    return E_POINTER;
  if (*szFileNameW == 0)
    return E_INVALIDARG;

  if (lpOptions == NULL)
  {
    lpOptions = &cDefaultConnectOptions;
  }
  Disconnect();

  dwBusyTimeoutMs = (lpOptions->dwBusyTimeoutMs != 0) ? lpOptions->dwBusyTimeoutMs : INFINITE;

  //initialize APIs
  hRes = InitializeInternals();
  if (FAILED(hRes))
    return hRes;

  //create internal object
  lpInternalData = MX_DEBUG_NEW Internals::CSQLite3ConnectorData();
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;

  //open database
  flags = SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_SHAREDCACHE;
  if (lpOptions->bReadOnly != FALSE)
    flags |= SQLITE_OPEN_READONLY;
  else if (lpOptions->bDontCreateIfNotExists == FALSE)
    flags |= SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE;
  else
    flags |= SQLITE_OPEN_READWRITE;

  bFileExists = (::GetFileAttributesW(szFileNameW) != INVALID_FILE_ATTRIBUTES);

  hRes = Utf8_Encode(cStrFileNameA, szFileNameW);
  if (FAILED(hRes))
  {
    delete sqlite3_data;
    lpInternalData = NULL;

    return hRes;
  }

  err = sqlite3_open_v2((LPCSTR)cStrFileNameA, &(sqlite3_data->lpDB), flags, NULL);
  if (err != SQLITE_OK)
  {
    sqlite3_data->SetErrno(err);
    hRes = sqlite3_data->hLastDbRes;

    delete sqlite3_data;
    lpInternalData = NULL;

    return hRes;
  }
  sqlite3_extended_result_codes(sqlite3_data->lpDB, 1);

  err = sqlite3_create_function(sqlite3_data->lpDB, "UTC_TIMESTAMP", 0, SQLITE_UTF8, 0, myFuncUtcNow, NULL, NULL);
  if (err == SQLITE_OK)
  {
    err = sqlite3_create_function(sqlite3_data->lpDB, "NOW", 0, SQLITE_UTF8, 0, myFuncNow, NULL, NULL);
  }
  if (err == SQLITE_OK)
  {
    err = sqlite3_create_function(sqlite3_data->lpDB, "CURRENT_TIMESTAMP", 0, SQLITE_UTF8, 0, myFuncNow, NULL, NULL);
  }
  //if (err == SQLITE_OK)
  //{
  //  err = sqlite3_create_function(sqlite3_data->lpDB, "REGEXP", 2, SQLITE_UTF8, 0, myFuncRegExp, 0, 0);
  //}

  if (err == SQLITE_OK)
  {
    err = sqlite3_create_function(sqlite3_data->lpDB, "CONCAT", -1, SQLITE_UTF8, 0, NULL, myFuncConcatStep,
                                  myFuncConcatFinal);
  }
  if (err != SQLITE_OK)
  {
    sqlite3_data->SetErrno(err);
    hRes = sqlite3_data->hLastDbRes;

    delete sqlite3_data;
    lpInternalData = NULL;

    if (bFileExists == FALSE)
    {
      ::DeleteFileW(szFileNameW);
    }

    return hRes;
  }

  sqlite3_data->dwBusyTimeoutMs = dwBusyTimeoutMs;

  //done
  sqlite3_data->ClearErrno();
  return S_OK;
}


VOID CSQLite3Connector::Disconnect()
{
  if (lpInternalData != NULL)
  {
    QueryClose();
    delete sqlite3_data;
    lpInternalData = NULL;
  }

  //done
  return;
}

BOOL CSQLite3Connector::IsConnected() const
{
  return (lpInternalData != NULL) ? TRUE : FALSE;
}

int CSQLite3Connector::GetErrorCode() const
{
  return (lpInternalData != NULL) ? sqlite3_data->nLastDbErr : 0;
}

LPCSTR CSQLite3Connector::GetErrorDescription() const
{
  return (lpInternalData != NULL) ? (LPCSTR)(sqlite3_data->cStrLastDbErrorDescriptionA) : "";
}

HRESULT CSQLite3Connector::QueryExecute(_In_ LPCSTR szQueryA, _In_opt_ SIZE_T nQueryLen, _In_opt_ CFieldList *lpInputFieldsList)
{
  Internals::CAutoLockSQLite3DB cDbLock(sqlite3_data);
  int err, inputParams;
  SIZE_T nParamsCount;
  DWORD dwBusyTimeoutMs;

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

  QueryClose();

  dwBusyTimeoutMs = sqlite3_data->dwBusyTimeoutMs;
  do
  {
    const char *szLeftOverA;

    err = sqlite3_prepare_v2(sqlite3_data->lpDB, szQueryA, (int)nQueryLen, &(sqlite3_data->lpStmt), &szLeftOverA);
  }
  while (MustRetry(err, sqlite3_data->dwBusyTimeoutMs, dwBusyTimeoutMs) != FALSE);
  if (err != SQLITE_OK)
  {
on_error:
    sqlite3_data->SetErrno(err);
    QueryClose();
    if (IsSQLiteFatalError(err) != FALSE)
      Disconnect();

    return sqlite3_data->hLastDbRes;
  }

    //parse input parameters
  inputParams = sqlite3_bind_parameter_count(sqlite3_data->lpStmt);
  if (inputParams < 0)
    inputParams = 0;
  if ((SIZE_T)inputParams != nParamsCount)
  {
err_invalidarg:
    QueryClose();
    sqlite3_data->ClearErrno();
    return E_INVALIDARG;
  }
  if (nParamsCount > 0)
  {
    CField *lpField;
    SIZE_T nParamIdx;

    for (nParamIdx = 0; nParamIdx < nParamsCount; nParamIdx++)
    {
      err = 0;
      lpField = lpInputFieldsList->GetElementAt(nParamIdx);
      switch (lpField->GetType())
      {
        case eFieldType::Null:
          err = sqlite3_bind_null(sqlite3_data->lpStmt, (int)nParamIdx + 1);
          break;

        case eFieldType::Boolean:
          err = sqlite3_bind_int(sqlite3_data->lpStmt, (int)nParamIdx + 1, (lpField->GetBoolean() != FALSE) ? 1 : 0);
          break;

        case eFieldType::UInt32:
          err = sqlite3_bind_int64(sqlite3_data->lpStmt, (int)nParamIdx + 1,
                                   (sqlite3_int64)(ULONGLONG)(lpField->GetUInt32()));
          break;

        case eFieldType::Int32:
          err = sqlite3_bind_int64(sqlite3_data->lpStmt, (int)nParamIdx + 1, (sqlite3_int64)(lpField->GetInt32()));
          break;

        case eFieldType::UInt64:
          err = sqlite3_bind_int64(sqlite3_data->lpStmt, (int)nParamIdx + 1, (sqlite3_int64)(lpField->GetUInt64()));
          break;

        case eFieldType::Int64:
          err = sqlite3_bind_int64(sqlite3_data->lpStmt, (int)nParamIdx + 1, (sqlite3_int64)(lpField->GetInt64()));
          break;

        case eFieldType::Double:
          err = sqlite3_bind_double(sqlite3_data->lpStmt, (int)nParamIdx + 1, lpField->GetDouble());
          break;

        case eFieldType::String:
          //we don't copy the data so we need to keep a reference to the source field
          if (sqlite3_data->aInputFieldsList.AddElement(lpField) == FALSE)
          {
err_nomem:
            QueryClose();
            sqlite3_data->SetCustomErrno(SQLITE_NOMEM, "Out of memory");
            return E_OUTOFMEMORY;
          }
          lpField->AddRef();

          err = sqlite3_bind_text(sqlite3_data->lpStmt, (int)nParamIdx + 1, lpField->GetString(),
                                  (int)(lpField->GetLength()), SQLITE_TRANSIENT);
          break;

        case eFieldType::Blob:
          //we don't copy the data so we need to keep a reference to the source field
          if (sqlite3_data->aInputFieldsList.AddElement(lpField) == FALSE)
            goto err_nomem;
          lpField->AddRef();

          err = sqlite3_bind_blob(sqlite3_data->lpStmt, (int)nParamIdx + 1, lpField->GetBlob(),
                                  (int)(lpField->GetLength()), SQLITE_STATIC);
          break;

        case eFieldType::DateTime:
          {
          CHAR szBufA[32];
          int nYear, nMonth, nDay, nHours, nMinutes, nSeconds, nMilliSeconds;

          lpField->GetDateTime()->GetDateTime(&nYear, &nMonth, &nDay, &nHours, &nMinutes, &nSeconds,
                                              &nMilliSeconds);

          _snprintf_s(szBufA, MX_ARRAYLEN(szBufA), _TRUNCATE, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                      nYear, nMonth, nDay, nHours, nMinutes, nSeconds, nMilliSeconds);
          err = sqlite3_bind_text(sqlite3_data->lpStmt, (int)nParamIdx + 1, szBufA, (int)MX::StrLenA(szBufA),
                                  SQLITE_TRANSIENT);
          }
          break;

        default:
          goto err_invalidarg;
      }
      if (err != 0)
        goto on_error;
    }
  }

  //execute query and optionally get first row
  dwBusyTimeoutMs = sqlite3_data->dwBusyTimeoutMs;
  do
  {
    err = sqlite3_step(sqlite3_data->lpStmt);
  }
  while (MustRetry(err, sqlite3_data->dwBusyTimeoutMs, dwBusyTimeoutMs) != FALSE);

  switch (err)
  {
    case SQLITE_ROW:
      sqlite3_data->dwFlags = QUERY_FLAGS_HAS_RESULTS | QUERY_FLAGS_ON_FIRST_ROW;
      break;

    case SQLITE_SCHEMA:
    case SQLITE_DONE:
    case SQLITE_OK:
      break;

    default:
      goto on_error;
  }

  //get some data
  ullLastInsertId = (ULONGLONG)sqlite3_last_insert_rowid(sqlite3_data->lpDB);
  ullAffectedRows = (ULONGLONG)sqlite3_changes(sqlite3_data->lpDB);

  //get fields count
  if ((sqlite3_data->dwFlags & QUERY_FLAGS_HAS_RESULTS) != 0)
  {
    int nFieldsCount = sqlite3_column_count(sqlite3_data->lpStmt);
    if (nFieldsCount > 0)
    {
      //build fields
      for (int i = 0; i < nFieldsCount; i++)
      {
        TAutoDeletePtr<CSQLite3Column> cColumn;
        LPCSTR sA;

        cColumn.Attach(MX_DEBUG_NEW CSQLite3Column());
        if (!(cColumn && cColumn->cField))
          goto err_nomem;

        //name
        sA = sqlite3_column_name(sqlite3_data->lpStmt, (int)i);
        if (cColumn->cStrNameA.Copy((sA != NULL) ? sA : "") == FALSE)
          goto err_nomem;

        //table
        sA = sqlite3_column_table_name(sqlite3_data->lpStmt, (int)i);
        if (cColumn->cStrTableA.Copy((sA != NULL) ? sA : "") == FALSE)
          goto err_nomem;

        //type

        sA = sqlite3_column_decltype(sqlite3_data->lpStmt, (int)i);
        if (sA == NULL || *sA == 0 || GetTypeFromName(sA, &(cColumn->nType), &(cColumn->nRealType)) == FALSE)
        {
          switch (sqlite3_column_type(sqlite3_data->lpStmt, (int)i))
          {
            case SQLITE_INTEGER:
              cColumn->nType = _FIELD_TYPE_LongLong;
              cColumn->nRealType = SQLITE_INTEGER;
              break;

            case SQLITE_FLOAT:
              cColumn->nType = _FIELD_TYPE_Double;
              cColumn->nRealType = SQLITE_FLOAT;
              break;

            case SQLITE_BLOB:
              cColumn->nType = _FIELD_TYPE_Blob;
              cColumn->nRealType = SQLITE_BLOB;
              break;

            case SQLITE_TEXT:
              cColumn->nType = _FIELD_TYPE_Text;
              cColumn->nRealType = SQLITE_TEXT;
              break;

            default:
              cColumn->nType = _FIELD_TYPE_Null;
              cColumn->nRealType = SQLITE_NULL;
              break;
          }
        }

        //get retrieved field row type
        if (cColumn->nRealType != SQLITE_NULL)
          cColumn->nCurrType = sqlite3_column_type(sqlite3_data->lpStmt, (int)i);
        else
          cColumn->nCurrType = SQLITE_NULL;

        //add to columns list
        if (aColumnsList.AddElement(cColumn.Get()) == FALSE)
          goto err_nomem;
        cColumn.Detach();
      }
    }
  }

  //done
  return 0;
}

HRESULT CSQLite3Connector::FetchRow()
{
  Internals::CAutoLockSQLite3DB cDbLock(sqlite3_data);
  CSQLite3Column *lpColumn;
  SIZE_T i, nColumnsCount;
  int err;

  if (lpInternalData == NULL)
    return MX_E_NotReady;

  nColumnsCount = aColumnsList.GetCount();
  if ((sqlite3_data->dwFlags & QUERY_FLAGS_HAS_RESULTS) == 0 ||
      (sqlite3_data->dwFlags & QUERY_FLAGS_EOF_REACHED) != 0 ||
      nColumnsCount == 0)
  {
    sqlite3_data->ClearErrno();
    QueryClose();
    return S_FALSE;
  }

  if ((sqlite3_data->dwFlags & QUERY_FLAGS_ON_FIRST_ROW) != 0)
  {
    //first row is retrieved when executing the statement
    //so, on the first FetchRow do a no-op
    sqlite3_data->dwFlags &= ~QUERY_FLAGS_ON_FIRST_ROW;
    ullAffectedRows++;
  }
  else
  {
    DWORD dwBusyTimeoutMs;

    dwBusyTimeoutMs = sqlite3_data->dwBusyTimeoutMs;
    do
    {
      err = sqlite3_step(sqlite3_data->lpStmt);
    }
    while (MustRetry(err, sqlite3_data->dwBusyTimeoutMs, dwBusyTimeoutMs) != FALSE);
    switch (err)
    {
      case SQLITE_ROW:
        ullAffectedRows++;
        //get retrieved field row type
        for (i = 0; i < nColumnsCount; i++)
        {
          lpColumn = (CSQLite3Column*)(aColumnsList.GetElementAt(i));
          if (lpColumn->nRealType != SQLITE_NULL)
            lpColumn->nCurrType = sqlite3_column_type(sqlite3_data->lpStmt, (int)i);
        }
        break;

      case SQLITE_DONE:
      case SQLITE_OK:
        sqlite3_data->dwFlags |= QUERY_FLAGS_EOF_REACHED;
        break;

      default:
        sqlite3_data->SetErrno(err);
        QueryClose();
        if (IsSQLiteFatalError(err) != FALSE)
          Disconnect();
        return sqlite3_data->hLastDbRes;

    }
  }

  if ((sqlite3_data->dwFlags & QUERY_FLAGS_EOF_REACHED) != 0)
  {
    sqlite3_data->ClearErrno();
    QueryClose();
    return S_FALSE;
  }

  for (i = 0; i < nColumnsCount; i++)
  {
    lpColumn = (CSQLite3Column*)(aColumnsList.GetElementAt(i));
    if (lpColumn->nRealType != SQLITE_NULL && lpColumn->nCurrType != SQLITE_NULL)
    {
      union {
        sqlite_int64 i64;
        int i;
        double dbl;
        char *sA;
        LPBYTE p;
      } val;
      int val_len;
      HRESULT hRes;

      switch (lpColumn->nType)
      {
        case _FIELD_TYPE_Blob:
          val.p = (LPBYTE)sqlite3_column_blob(sqlite3_data->lpStmt, (int)i);
          val_len = sqlite3_column_bytes(sqlite3_data->lpStmt, (int)i);

          hRes = lpColumn->cField->SetBlob(val.p, (SIZE_T)val_len);
          if (FAILED(hRes))
          {
on_hres_error:
            sqlite3_data->ClearErrno();
            QueryClose();
            return hRes;
          }
          break;

        case _FIELD_TYPE_Text:
          val.sA = (char*)sqlite3_column_text(sqlite3_data->lpStmt, (int)i);
          val_len = sqlite3_column_bytes(sqlite3_data->lpStmt, (int)i);

          hRes = lpColumn->cField->SetString(val.sA, (SIZE_T)val_len);
          if (FAILED(hRes))
            goto on_hres_error;
          break;

        case _FIELD_TYPE_DateTime:
          {
          CDateTime cDt, cDt2;
          CHAR szTempA[32];
          int flags;

          val.sA = (char*)sqlite3_column_text(sqlite3_data->lpStmt, (int)i);
          val_len = sqlite3_column_bytes(sqlite3_data->lpStmt, (int)i);
          if (val_len >= MX_ARRAYLEN(szTempA))
          {
            hRes = MX_E_InvalidData;
            goto on_hres_error;
          }
            
          for (int k = flags = 0; k < val_len; k++)
          {
            switch (szTempA[k] = val.sA[k])
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
          szTempA[val_len] = 0;

          switch (flags)
          {
            case DATETIME_QUICK_FLAGS_Dash | DATETIME_QUICK_FLAGS_Colon | DATETIME_QUICK_FLAGS_Dot:
              hRes = cDt.SetFromString(szTempA, "%Y-%m-%d %H:%M:%S.%f");
              if (FAILED(hRes) && hRes != E_OUTOFMEMORY)
                hRes = cDt.SetFromString(szTempA, "%Y-%m-%d %H:%M:%S.");

fetchrow_set_datetime:
              if (FAILED(hRes))
                goto on_hres_error;

              hRes = lpColumn->cField->SetDateTime(cDt);
              if (FAILED(hRes))
                goto on_hres_error;
              break;

            case DATETIME_QUICK_FLAGS_Slash | DATETIME_QUICK_FLAGS_Colon | DATETIME_QUICK_FLAGS_Dot:
              hRes = cDt.SetFromString(szTempA, "%Y/%m/%d %H:%M:%S.%f");
              if (FAILED(hRes) && hRes != E_OUTOFMEMORY)
                hRes = cDt.SetFromString(szTempA, "%Y/%m/%d %H:%M:%S.");
              goto fetchrow_set_datetime;

            case DATETIME_QUICK_FLAGS_Dash | DATETIME_QUICK_FLAGS_Colon:
              hRes = cDt.SetFromString(szTempA, "%Y-%m-%d %H:%M:%S");
              goto fetchrow_set_datetime;

            case DATETIME_QUICK_FLAGS_Slash | DATETIME_QUICK_FLAGS_Colon:
              hRes = cDt.SetFromString(szTempA, "%Y/%m/%d %H:%M:%S");
              goto fetchrow_set_datetime;

            case DATETIME_QUICK_FLAGS_Dash:
              hRes = cDt.SetFromString(szTempA, "%Y-%m-%d");
fetchrow_set_date:
              if (SUCCEEDED(hRes))
                hRes = cDt.SetTime(0, 0, 0, 0);
              goto fetchrow_set_datetime;

            case DATETIME_QUICK_FLAGS_Slash:
              hRes = cDt.SetFromString(szTempA, "%Y/%m/%d");
              goto fetchrow_set_date;

            case DATETIME_QUICK_FLAGS_Colon | DATETIME_QUICK_FLAGS_Dot:
              hRes = cDt2.SetFromString(szTempA, "%H:%M:%S.%f");
              if (FAILED(hRes) && hRes != E_OUTOFMEMORY)
                hRes = cDt2.SetFromString(szTempA, "%H:%M:%S.");
fetchrow_set_time:
              if (FAILED(hRes))
                goto on_hres_error;

              hRes = cDt.SetFromNow(FALSE);
              if (SUCCEEDED(hRes))
              {
                hRes = cDt.SetTime(cDt2.GetHours(), cDt2.GetMinutes(), cDt2.GetSeconds(), cDt2.GetMilliSeconds());
              }
              goto fetchrow_set_datetime;

            case DATETIME_QUICK_FLAGS_Colon:
              hRes = cDt2.SetFromString(szTempA, "%H:%M:%S");
              goto fetchrow_set_time;

            default:
              hRes = MX_E_InvalidData;
              goto on_hres_error;
          }
          }
          break;

        case _FIELD_TYPE_Boolean:
          val.i64 = sqlite3_column_int64(sqlite3_data->lpStmt, (int)i);
          lpColumn->cField->SetBoolean((val.i64 != 0) ? TRUE : FALSE);
          break;

        case _FIELD_TYPE_Long:
          val.i = sqlite3_column_int(sqlite3_data->lpStmt, (int)i);
          lpColumn->cField->SetInt32(val.i);
          break;

        case _FIELD_TYPE_LongLong:
          val.i64 = sqlite3_column_int64(sqlite3_data->lpStmt, (int)i);
          lpColumn->cField->SetInt64(val.i64);
          break;

        case _FIELD_TYPE_Double:
          val.dbl = sqlite3_column_double(sqlite3_data->lpStmt, (int)i);
          lpColumn->cField->SetDouble(val.dbl);
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

  //done
  sqlite3_data->ClearErrno();
  return S_OK;
}

VOID CSQLite3Connector::QueryClose()
{
  if (lpInternalData != NULL)
  {
    if (sqlite3_data->lpStmt != NULL)
    {
      sqlite3_finalize(sqlite3_data->lpStmt);
      sqlite3_data->lpStmt = NULL;
    }

    sqlite3_data->aInputFieldsList.RemoveAllElements();
    sqlite3_data->dwFlags = 0;
  }

  //call super
  __super::QueryClose();

  //done
  return;
}

HRESULT CSQLite3Connector::TransactionStart()
{
  return TransactionStart(eTxType::Standard);
}

HRESULT CSQLite3Connector::TransactionStart(_In_ eTxType nType)
{
  switch (nType)
  {
    case eTxType::Standard:
      return QueryExecute("BEGIN TRANSACTION;", 18);

    case eTxType::Exclusive:
      return QueryExecute("BEGIN EXCLUSIVE TRANSACTION;", 28);

    case eTxType::Immediate:
      return QueryExecute("BEGIN IMMEDIATE TRANSACTION;", 28);
  }
  return E_INVALIDARG;
}

HRESULT CSQLite3Connector::TransactionCommit()
{
  return QueryExecute("COMMIT TRANSACTION;", 19);
}

HRESULT CSQLite3Connector::TransactionRollback()
{
  return QueryExecute("ROLLBACK TRANSACTION;", 21);
}

HRESULT CSQLite3Connector::EscapeString(_Out_ CStringA &cStrA, _In_ LPCSTR szStrA, _In_opt_ SIZE_T nStrLen,
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

    while (szStrA < szStrEndA && (*szStrA != '\'' && *szStrA != '\\'))
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
          if (cStrTempA.ConcatN("''", 2) == FALSE)
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

HRESULT CSQLite3Connector::EscapeString(_Out_ CStringW &cStrW, _In_ LPCWSTR szStrW, _In_opt_ SIZE_T nStrLen,
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

  szStrEndW = szStrW + nStrLen;
  while (szStrW < szStrEndW)
  {
    LPCWSTR szStartW = szStrW;

    while (szStrW < szStrEndW && (*szStrW != L'\'' && *szStrW != L'\\'))
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
          if (cStrTempW.ConcatN(L"''", 2) == FALSE)
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

CSQLite3Connector::CConnectOptions::CConnectOptions() : CBaseMemObj()
{
  bDontCreateIfNotExists = FALSE;
  bReadOnly = FALSE;
  dwBusyTimeoutMs = 30000;
  return;
}

} //namespace Database

} //namespace MX

//-----------------------------------------------------------

namespace MX {

namespace Internals {

CSQLite3ConnectorData::CSQLite3ConnectorData() : CBaseMemObj()
{
  lpDB = NULL;
  nLastDbErr = 0;
  hLastDbRes = S_OK;
  lpStmt = NULL;
  dwFlags = 0;
  dwBusyTimeoutMs = _DEFAULT_BUSY_TIMEOUT_MS;
  return;
}

CSQLite3ConnectorData::~CSQLite3ConnectorData()
{
  if (lpDB != NULL)
    sqlite3_close(lpDB);
  return;
}

VOID CSQLite3ConnectorData::ClearErrno()
{
  nLastDbErr = 0;
  hLastDbRes = S_OK;
  cStrLastDbErrorDescriptionA.Empty();
  return;
}

VOID CSQLite3ConnectorData::SetErrno(_In_ int err, _In_opt_ HRESULT hRes)
{
  LPCSTR sA;

  nLastDbErr = err;
  if (SUCCEEDED(hRes))
  {
    hRes = MX_HRESULT_FROM_WIN32(sqlite3_system_errno(lpDB));
  }
  if (SUCCEEDED(hRes))
  {
    hRes = GetHResultFromErr(err, TRUE);
  }
  hLastDbRes = hRes;

  sA = sqlite3_errstr(err);
  if (sA != NULL && *sA != 0)
    cStrLastDbErrorDescriptionA.Copy(sA);
  else
    cStrLastDbErrorDescriptionA.Empty();

  return;
}

VOID CSQLite3ConnectorData::SetCustomErrno(_In_ int err, _In_z_ LPCSTR szDescriptionA)
{
  nLastDbErr = err;
  hLastDbRes = GetHResultFromErr(err, FALSE);

  if (szDescriptionA != NULL && *szDescriptionA != 0)
    cStrLastDbErrorDescriptionA.Copy(szDescriptionA);
  else
    cStrLastDbErrorDescriptionA.Empty();

  return;
}

HRESULT CSQLite3ConnectorData::GetHResultFromErr(_In_ int err, _In_ BOOL bCheckDB)
{
  if (err == 0)
    return S_OK;
  switch (err & 0xFF)
  {
    case SQLITE_NOMEM:
      return E_OUTOFMEMORY;

    case SQLITE_PERM:
    case SQLITE_AUTH:
      return E_ACCESSDENIED;

    case SQLITE_ABORT:
      return MX_E_Cancelled;

    case SQLITE_BUSY:
    case SQLITE_LOCKED:
      return MX_E_Busy;

    case SQLITE_CANTOPEN:
    case SQLITE_NOTFOUND:
      return MX_E_NotFound;

    case SQLITE_CONSTRAINT:
      switch (err)
      {
        case SQLITE_CONSTRAINT_FOREIGNKEY:
        case SQLITE_CONSTRAINT_PRIMARYKEY:
        case SQLITE_CONSTRAINT_UNIQUE:
        case SQLITE_CONSTRAINT_ROWID:
          return MX_E_DuplicateKey;
      }
      return MX_E_ConstraintsCheckFailed;
  }
  if (bCheckDB != FALSE && lpDB != NULL)
  {
    HRESULT hRes = MX_HRESULT_FROM_WIN32(sqlite3_system_errno(lpDB));
    if (FAILED(hRes))
      return hRes;
  }
  return E_FAIL;
}

//-----------------------------------------------------------

CSQLite3ConnectorData::CBuffer::CBuffer() : CBaseMemObj()
{
  lpBuffer = NULL;
  nBufferSize = 0;
  return;
}

CSQLite3ConnectorData::CBuffer::~CBuffer()
{
  MX_FREE(lpBuffer);
  return;
}

BOOL CSQLite3ConnectorData::CBuffer::EnsureSize(_In_ SIZE_T nNewSize)
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

//-----------------------------------------------------------

static HRESULT InitializeInternals()
{
  static LONG volatile nMutex = MX_FASTLOCK_INIT;

  if (bInitialized == NULL)
  {
    MX::CFastLock cLock(&nMutex);
    HRESULT hRes;

    if (bInitialized == FALSE)
    {
      static const sqlite3_mem_methods defaultMethods = {
        my_mem_alloc, my_mem_free, my_mem_realloc, my_mem_memsize, my_mem_roundup,
        my_mem_init, my_mem_shutdown, 0
      };
      sqlite3_config(SQLITE_CONFIG_MALLOC, &defaultMethods);

      //register shutdown callback
      hRes = MX::RegisterFinalizer(&SQLITE_Shutdown, SQLITE_FINALIZER_PRIORITY);
      if (FAILED(hRes))
        return hRes;

      //done
      bInitialized = TRUE;
    }
  }

  //done
  return S_OK;
}

static VOID SQLITE_Shutdown()
{
  if (bInitialized != FALSE)
  {
    sqlite3_shutdown();

    bInitialized = FALSE;
  }
  return;
}

static void* my_mem_alloc(int nByte)
{
  void *p = MX_MALLOC((SIZE_T)nByte);
  if (p == NULL)
  {
    sqlite3_log(SQLITE_NOMEM, "failed to allocate %u bytes of memory", nByte);
  }
  return p;
}

static void my_mem_free(void *pPrior)
{
  MX_FREE(pPrior);
  return;
}

static int my_mem_memsize(void *pPrior)
{
  return (int)::MxMemSize(pPrior);
}

static void* my_mem_realloc(void *pPrior, int nByte)
{
  void *p = MX_REALLOC(pPrior, (SIZE_T)nByte);
  if (p == NULL)
  {
    sqlite3_log(SQLITE_NOMEM, "failed memory resize %u to %u bytes", my_mem_memsize(pPrior), nByte);
  }
  return p;
}

static int my_mem_roundup(int n)
{
  return (n + 7) & (~7);
}

static int my_mem_init(void *NotUsed)
{
  return SQLITE_OK;
}

static void my_mem_shutdown(void *NotUsed)
{
  return;
}

//-----------------------------------------------------------

static BOOL GetTypeFromName(_In_z_ LPCSTR szNameA, _Out_ PULONG lpnType, _Out_ PULONG lpnRealType)
{
  static const struct tagTYPE {
    LPCSTR szName;
    int nType;
    int nRealType;
  } aTypesList[] = {
    { "AUTOINCREMENT",    _FIELD_TYPE_LongLong, SQLITE_INTEGER },
    { "BIGINT",           _FIELD_TYPE_LongLong, SQLITE_INTEGER },
    { "BINARY",           _FIELD_TYPE_Blob,     SQLITE_BLOB },
    { "BIT",              _FIELD_TYPE_LongLong, SQLITE_INTEGER },
    { "BLOB",             _FIELD_TYPE_Blob,     SQLITE_BLOB },
    { "BOOL",             _FIELD_TYPE_Boolean,  SQLITE_INTEGER },
    { "CHAR",             _FIELD_TYPE_Text,     SQLITE_TEXT },
    { "CHARACTER",        _FIELD_TYPE_Text,     SQLITE_TEXT },
    { "COUNTER",          _FIELD_TYPE_LongLong, SQLITE_INTEGER },
    { "CURRENCY",         _FIELD_TYPE_Double,   SQLITE_FLOAT },
    { "DATE",             _FIELD_TYPE_DateTime, SQLITE_TEXT },
    { "DATETIME",         _FIELD_TYPE_DateTime, SQLITE_TEXT },
    { "DECIMAL",          _FIELD_TYPE_Double,   SQLITE_FLOAT },
    { "DOUBLE",           _FIELD_TYPE_Double,   SQLITE_FLOAT },
    { "FLOAT",            _FIELD_TYPE_Double,   SQLITE_FLOAT },
    { "FLOAT4",           _FIELD_TYPE_Double,   SQLITE_FLOAT },
    { "FLOAT8",           _FIELD_TYPE_Double,   SQLITE_FLOAT },
    { "GENERAL",          _FIELD_TYPE_Blob,     SQLITE_BLOB },
    { "GUID",             _FIELD_TYPE_Text,     SQLITE_TEXT },
    { "IDENTITY",         _FIELD_TYPE_LongLong, SQLITE_INTEGER },
    { "IMAGE",            _FIELD_TYPE_Blob,     SQLITE_BLOB },
    { "INT",              _FIELD_TYPE_Long,     SQLITE_INTEGER },
    { "INT2",             _FIELD_TYPE_Long,     SQLITE_INTEGER },
    { "INT4",             _FIELD_TYPE_Long,     SQLITE_INTEGER },
    { "INT8",             _FIELD_TYPE_LongLong, SQLITE_INTEGER },
    { "INTEGER",          _FIELD_TYPE_LongLong, SQLITE_INTEGER },
    { "LOGICAL",          _FIELD_TYPE_LongLong, SQLITE_INTEGER },
    { "LONG",             _FIELD_TYPE_Long,     SQLITE_INTEGER },
    { "LONGCHAR",         _FIELD_TYPE_Text,     SQLITE_TEXT },
    { "LONGTEXT",         _FIELD_TYPE_Text,     SQLITE_TEXT },
    { "LONGVARCHAR",      _FIELD_TYPE_Text,     SQLITE_TEXT },
    { "MEMO",             _FIELD_TYPE_Text,     SQLITE_TEXT },
    { "MONEY",            _FIELD_TYPE_Double,   SQLITE_FLOAT },
    { "NCHAR",            _FIELD_TYPE_Text,     SQLITE_TEXT },
    { "NOTE",             _FIELD_TYPE_Text,     SQLITE_TEXT },
    { "NTEXT",            _FIELD_TYPE_Text,     SQLITE_TEXT },
    { "NUMERIC",          _FIELD_TYPE_Double,   SQLITE_FLOAT },
    { "NVARCHAR",         _FIELD_TYPE_Text,     SQLITE_TEXT },
    { "OLEOBJECT",        _FIELD_TYPE_Blob,     SQLITE_BLOB },
    { "REAL",             _FIELD_TYPE_Double,   SQLITE_FLOAT },
    { "SERIAL",           _FIELD_TYPE_Blob,     SQLITE_BLOB },
    { "SERIAL4",          _FIELD_TYPE_Blob,     SQLITE_BLOB },
    { "SMALLINT",         _FIELD_TYPE_Long,     SQLITE_INTEGER },
    { "STRING",           _FIELD_TYPE_Text,     SQLITE_TEXT },
    { "TEXT",             _FIELD_TYPE_Text,     SQLITE_TEXT },
    { "TIME",             _FIELD_TYPE_DateTime, SQLITE_TEXT },
    { "TIMESTAMP",        _FIELD_TYPE_DateTime, SQLITE_TEXT },
    { "TIMESTAMPTZ",      _FIELD_TYPE_DateTime, SQLITE_TEXT },
    { "TIMETZ",           _FIELD_TYPE_DateTime, SQLITE_TEXT },
    { "TINYINT",          _FIELD_TYPE_Long,     SQLITE_INTEGER },
    { "UNIQUEIDENTIFIER", _FIELD_TYPE_Text,     SQLITE_TEXT },
    { "VARBINARY",        _FIELD_TYPE_Blob,     SQLITE_BLOB },
    { "VARCHAR",          _FIELD_TYPE_Text,     SQLITE_TEXT },
    { "YESNO",            _FIELD_TYPE_Boolean,  SQLITE_INTEGER }
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
  err &= 0xFF;
  if ((err != SQLITE_BUSY && err != SQLITE_LOCKED) || dwInitialBusyTimeoutMs == 0 || dwBusyTimeoutMs == 0)
    return FALSE;
  if (dwBusyTimeoutMs != INFINITE)
  {
    if (dwBusyTimeoutMs >= 50)
    {
      ::Sleep(50);
      dwBusyTimeoutMs -= 50;
    }
    else
    {
      ::Sleep(dwBusyTimeoutMs);
      dwBusyTimeoutMs = 0;
    }
  }
  return TRUE;
}

static void myFuncUtcNow(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  SYSTEMTIME sSt;
  CHAR szBufA[32];

  ::GetSystemTime(&sSt);
  _snprintf_s(szBufA, MX_ARRAYLEN(szBufA), _TRUNCATE, "%04lu-%02lu-%02lu %02lu:%02lu:%02lu", (DWORD)(sSt.wYear),
              (DWORD)(sSt.wMonth), (DWORD)(sSt.wDay), (DWORD)(sSt.wHour), (DWORD)(sSt.wMinute), (DWORD)(sSt.wSecond));
  sqlite3_result_text(ctx, szBufA, -1, SQLITE_TRANSIENT);
  return;
}

static void myFuncNow(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  SYSTEMTIME sSt;
  CHAR szBufA[32];

  ::GetLocalTime(&sSt);
  _snprintf_s(szBufA, MX_ARRAYLEN(szBufA), _TRUNCATE, "%04lu-%02lu-%02lu %02lu:%02lu:%02lu", (DWORD)(sSt.wYear),
              (DWORD)(sSt.wMonth), (DWORD)(sSt.wDay), (DWORD)(sSt.wHour), (DWORD)(sSt.wMinute), (DWORD)(sSt.wSecond));
  sqlite3_result_text(ctx, szBufA, -1, SQLITE_TRANSIENT);
  return;
}

//static void myFuncRegExp(sqlite3_context *ctx, int argc, sqlite3_value **argv)
//{
//  if (argc < 2) return;
//  const char *pattern = (const char *)sqlite3_value_text(argv[0]);
//  const char *text = (const char*)sqlite3_value_text(argv[1]);
//  if (pattern && text) sqlite3_result_int(ctx, strRegexp(pattern, text));
//  return;
//}

static BOOL myFuncConcatCommon(_In_ MYFUNC_CONCAT_CONTEXT *p, _In_ const char *data, _In_ size_t len)
{
  if (data != NULL)
  {
    p->data = (char*)MX_REALLOC(p->data, p->len + len + 1);
    if (p->data == NULL)
      return FALSE;
    p->data[p->len] = 0;
    strncat_s(p->data, p->len + len + 1, data, len);
    p->len += len;
  }
  return TRUE;
}

static void myFuncConcatStep(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  MYFUNC_CONCAT_CONTEXT *p;
  const char *txt;
  const char *sep;

  p = (MYFUNC_CONCAT_CONTEXT*)sqlite3_aggregate_context(ctx, sizeof(MYFUNC_CONCAT_CONTEXT));
  if (p == NULL)
  {
err_nomem:
    if (p != NULL)
    {
      MX_FREE(p->data);
      MX_FREE(p->close);
    }
    sqlite3_result_error_nomem(ctx);
    return;
  }
  txt = (const char*)sqlite3_value_text(argv[0]);
  if (txt == NULL)
    return;
  sep = (const char*)sqlite3_value_text(argv[1]);
  if (argc > 3)
  {
    const char *open = (const char*)sqlite3_value_text(argv[2]);
    const char *close = (const char*)sqlite3_value_text(argv[3]);
    if (p->close == NULL && close != NULL )
    {
      size_t len = strlen(close);

      p->close = (char*)MX_MALLOC(len + 1);
      if (p->close == NULL)
        goto err_nomem;
      strncpy_s(p->close, len + 1, close, len);
      p->close[len] = 0;
    }
    if (p->data == NULL && open != NULL)
    {
      if (myFuncConcatCommon(p, open, strlen(open)) == FALSE)
        goto err_nomem;
    }
  }
  if (p->count && sep != NULL)
  {
    if (myFuncConcatCommon(p, sep, strlen(sep)) == FALSE)
      goto err_nomem;
  }
  if (myFuncConcatCommon(p, txt, strlen(txt)) == FALSE)
    goto err_nomem;
  (p->count)++;
  return;
}

static void myFuncConcatFinal(sqlite3_context *ctx)
{
  MYFUNC_CONCAT_CONTEXT *p;

  p = (MYFUNC_CONCAT_CONTEXT*)sqlite3_aggregate_context(ctx, 0);
  if (p != NULL && p->data != NULL)
  {
    if (p->close != NULL)
    {
      if (myFuncConcatCommon(p, p->close, strlen(p->close)) == FALSE)
      {
        MX_FREE(p->data);
        MX_FREE(p->close);
        sqlite3_result_error_nomem(ctx);
        return;
      }
      MX_FREE(p->close);
    }
    sqlite3_result_text(ctx, p->data, (int)(unsigned int)(p->len), myFuncFree);
  }
  else
  {
    if (p != NULL)
    {
      MX_FREE(p->data);
    }
    sqlite3_result_text(ctx, "", 0, SQLITE_STATIC);
  }
  return;
}

static void myFuncFree(_In_ void *ptr)
{
  MX_FREE(ptr);
  return;
}
