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
#include "..\..\..\..\Include\JsLib\Plugins\JsSQLitePlugin.h"
#include <intsafe.h>

#define SQLITE_ERROR        1
#define SQLITE_NOMEM        7

//-----------------------------------------------------------

namespace MX {

CJsSQLitePlugin::CJsSQLitePlugin() : CJsObjectBase(), CNonCopyableObj()
{
  return;
}

CJsSQLitePlugin::~CJsSQLitePlugin()
{
  return;
}

VOID CJsSQLitePlugin::SetConnector(_In_ Database::CSQLite3Connector *lpConnector)
{
  cConnector = lpConnector;
  return;
}

Database::CSQLite3Connector* CJsSQLitePlugin::DetachConnector()
{
  return cConnector.Detach();
}

Database::CSQLite3Connector* CJsSQLitePlugin::GetConnector()
{
  Database::CSQLite3Connector *lpConnector;

  lpConnector = cConnector.Get();
  if (lpConnector != NULL)
    lpConnector->AddRef();
  return lpConnector;
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

DukTape::duk_ret_t CJsSQLitePlugin::Connect(_In_ DukTape::duk_context *lpCtx)
{
  MX::CStringW cStrFileNameA;
  DukTape::duk_idx_t nParamsCount;
  Database::CSQLite3Connector::CConnectOptions cOptions;
  LPCSTR szDatabaseNameA;
  HRESULT hRes;

  Disconnect(lpCtx);

  //get parameters
  nParamsCount = DukTape::duk_get_top(lpCtx);
  if (nParamsCount < 1 || nParamsCount > 2)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);

  szDatabaseNameA = DukTape::duk_require_string(lpCtx, 0);

  //has more options?
  if (nParamsCount == 2)
  {
    if (duk_is_object(lpCtx, 1) == 1)
    {
      //don't create
      DukTape::duk_get_prop_string(lpCtx, 1, "create");
      if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
      {
        if (DukTape::duk_is_boolean(lpCtx, -1) != 0)
          cOptions.bDontCreateIfNotExists = (DukTape::duk_require_boolean(lpCtx, -1) != 0) ? FALSE : TRUE;
        else
          cOptions.bDontCreateIfNotExists = (DukTape::duk_require_int(lpCtx, -1) != 0) ? FALSE : TRUE;
      }
      DukTape::duk_pop(lpCtx);

      //read-only
      DukTape::duk_get_prop_string(lpCtx, 1, "readOnly");
      if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
      {
        if (DukTape::duk_is_boolean(lpCtx, -1) != 0)
          cOptions.bReadOnly = (DukTape::duk_require_boolean(lpCtx, -1) != 0) ? TRUE : FALSE;
        else
          cOptions.bReadOnly = (DukTape::duk_require_int(lpCtx, -1) != 0) ? TRUE : FALSE;
      }
      DukTape::duk_pop(lpCtx);

      //busy timeout ms
      DukTape::duk_get_prop_string(lpCtx, 1, "busyTimeoutMs");
      if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
      {
        if (DukTape::duk_is_boolean(lpCtx, -1) != 0)
          cOptions.dwBusyTimeoutMs = (DukTape::duk_require_boolean(lpCtx, -1) != 0) ? 1 : 0;
        else
          cOptions.dwBusyTimeoutMs = DukTape::duk_require_uint(lpCtx, -1);
      }
      DukTape::duk_pop(lpCtx);
    }
    else if (duk_is_null_or_undefined(lpCtx, 1) == 0)
    {
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
    }
  }

  //set connector if none
  if (!cConnector)
  {
    cConnector.Attach(MX_DEBUG_NEW Database::CSQLite3Connector());
    if (!cConnector)
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
  }

  //open
  hRes = cConnector->Connect(szDatabaseNameA, &cOptions);
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);

  //done
  return 0;
}

DukTape::duk_ret_t CJsSQLitePlugin::Disconnect(_In_opt_ DukTape::duk_context *lpCtx)
{
  if (cConnector)
  {
    cConnector->Disconnect();
  }

  //done
  return 0;
}

DukTape::duk_ret_t CJsSQLitePlugin::Query(_In_ DukTape::duk_context *lpCtx)
{
  Database::CFieldList cFieldsList;
  LPCSTR szQueryA;
  DukTape::duk_idx_t nParamIdx, nParamsCount;
  DukTape::duk_size_t nQueryLen;
  BOOL bParamsIsArray;
  HRESULT hRes;

  if (!cConnector)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);

  //get parameters
  nParamsCount = DukTape::duk_get_top(lpCtx);
  if (nParamsCount < 1)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  bParamsIsArray = FALSE;
  if (nParamsCount == 2 && DukTape::duk_is_array(lpCtx, 1) != 0)
  {
    bParamsIsArray = TRUE;
    nParamsCount = (DukTape::duk_idx_t)DukTape::duk_get_length(lpCtx, 1);
  }
  else
  {
    nParamsCount--;
  }

  //get query
  szQueryA = DukTape::duk_require_lstring(lpCtx, 0, &nQueryLen);
  if (nQueryLen == 0)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);

  //process input fields
  for (nParamIdx = 0; nParamIdx < nParamsCount; nParamIdx++)
  {
    TAutoRefCounted<Database::CField> cField;
    DukTape::duk_size_t len;
    DukTape::duk_idx_t ndx;

    if (bParamsIsArray != FALSE)
    {
      DukTape::duk_get_prop_index(lpCtx, 1, (DukTape::duk_uarridx_t)nParamIdx);
      ndx = -1;
    }
    else
    {
      ndx = (DukTape::duk_idx_t)(nParamIdx + 1);
    }

    cField.Attach(MX_DEBUG_NEW Database::CField());
    if (!cField)
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
    if (cFieldsList.AddElement(cField.Get()) == FALSE)
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
    cField->AddRef();

    switch (DukTape::duk_get_type(lpCtx, ndx))
    {
      case DUK_TYPE_NONE:
      case DUK_TYPE_UNDEFINED:
      case DUK_TYPE_NULL:
        cField->SetNull();
        break;

      case DUK_TYPE_BOOLEAN:
        cField->SetBoolean(DukTape::duk_get_boolean(lpCtx, ndx) ? TRUE : FALSE);
        break;

      case DUK_TYPE_NUMBER:
        {
          double dbl;
          LONGLONG ll;

          dbl = (double)DukTape::duk_get_number(lpCtx, ndx);
          ll = (LONGLONG)dbl;
          if ((double)ll != dbl || dbl < (double)LONGLONG_MIN || dbl >(double)LONGLONG_MAX)
          {
            cField->SetDouble(dbl);
          }
          else if (ll < (LONGLONG)LONG_MIN || ll >(LONGLONG)LONG_MAX)
          {
            cField->SetInt64(ll);
          }
          else
          {
            cField->SetInt32((LONG)ll);
          }
        }
        break;

      case DUK_TYPE_STRING:
        {
          LPCSTR sA = DukTape::duk_get_lstring(lpCtx, ndx, &len);
          hRes = cField->SetString(sA, len);
          if (FAILED(hRes))
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
        }
        break;

      case DUK_TYPE_OBJECT:
        if (DukTape::duk_is_buffer_data(lpCtx, ndx) != 0)
        {
          LPVOID p = DukTape::duk_get_buffer_data(lpCtx, ndx, &len);
          hRes = cField->SetBlob(p, len);
          if (FAILED(hRes))
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
        }
        else
        {
          CStringA cStrTypeA;

          CJavascriptVM::GetObjectType(lpCtx, ndx, cStrTypeA);
          if (StrCompareA((LPCSTR)cStrTypeA, "Date") == 0)
          {
            CDateTime cDt;

            CJavascriptVM::GetDate(lpCtx, ndx, cDt);
            hRes = cField->SetDateTime(cDt);
            if (FAILED(hRes))
              MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
          }
          else
          {
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
          }
        }
        break;

      case DUK_TYPE_BUFFER:
        {
          LPVOID p = DukTape::duk_get_buffer_data(lpCtx, ndx, &len);
          hRes = cField->SetBlob(p, len);
          if (FAILED(hRes))
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
        }
        break;

      default:
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
    }
    if (bParamsIsArray != FALSE)
    {
      DukTape::duk_pop(lpCtx);
    }
  }

  hRes = cConnector->QueryExecute(szQueryA, nQueryLen, &cFieldsList);
  if (FAILED(hRes))
    ThrowDbError(lpCtx, hRes, __FILE__, __LINE__);

  //done
  return 0;
}

DukTape::duk_ret_t CJsSQLitePlugin::QueryAndFetchRow(_In_ DukTape::duk_context *lpCtx)
{
  Query(lpCtx);
  return FetchRow(lpCtx);
}

DukTape::duk_ret_t CJsSQLitePlugin::QueryClose(_In_opt_ DukTape::duk_context *lpCtx)
{
  UNREFERENCED_PARAMETER(lpCtx);

  if (cConnector)
  {
    cConnector->QueryClose();
  }

  //done
  return 0;
}

DukTape::duk_ret_t CJsSQLitePlugin::EscapeString(_In_ DukTape::duk_context *lpCtx)
{
  CStringA cStrResultA;
  DukTape::duk_idx_t nParamsCount;
  DukTape::duk_size_t nStrLen;
  BOOL bIsLikeStatement;
  LPCSTR szStrA;
  HRESULT hRes;

  if (!cConnector)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);

  //get parameters
  nParamsCount = DukTape::duk_get_top(lpCtx);
  if (nParamsCount < 1 || nParamsCount > 2)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  szStrA = DukTape::duk_require_lstring(lpCtx, 0, &nStrLen);

  bIsLikeStatement = FALSE;
  if (nParamsCount > 1)
  {
    if (DukTape::duk_is_boolean(lpCtx, 1) != 0)
      bIsLikeStatement = (DukTape::duk_require_boolean(lpCtx, 1) != 0) ? TRUE : FALSE;
    else
      bIsLikeStatement = (DukTape::duk_require_int(lpCtx, 1) != 0) ? TRUE : FALSE;
  }

  //detect if server is using no backslashes escape
  hRes = cConnector->EscapeString(cStrResultA, szStrA, nStrLen, bIsLikeStatement);
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);

  //done
  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrResultA, cStrResultA.GetLength());
  return 1;
}

DukTape::duk_ret_t CJsSQLitePlugin::FetchRow(_In_ DukTape::duk_context *lpCtx)
{
  SIZE_T nFieldsCount;
  HRESULT hRes;

  if (!cConnector)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);

  hRes = cConnector->FetchRow();
  if (hRes == S_FALSE)
  {
    DukTape::duk_push_null(lpCtx);
    return 1;
  }

  if (FAILED(hRes))
    ThrowDbError(lpCtx, hRes, __FILE__, __LINE__);

  //create an object for each result
  DukTape::duk_push_object(lpCtx);

  nFieldsCount = cConnector->GetFieldsCount();
  for (SIZE_T i = 0; i < nFieldsCount; i++)
  {
    TAutoRefCounted<Database::CField> cField;
    LPCSTR sA;
    LPBYTE p;

    cField.Attach(cConnector->GetField(i));
    switch (cField->GetType())
    {
      case Database::eFieldType::Null:
        DukTape::duk_push_null(lpCtx);
        break;

      case Database::eFieldType::String:
        DukTape::duk_push_lstring(lpCtx, cField->GetString(), cField->GetLength());
        break;

      case Database::eFieldType::Boolean:
        DukTape::duk_push_boolean(lpCtx, (cField->GetBoolean() != FALSE) ? 1 : 0);
        break;

      case Database::eFieldType::UInt32:
        DukTape::duk_push_uint(lpCtx, cField->GetUInt32());
        break;

      case Database::eFieldType::Int32:
        DukTape::duk_push_int(lpCtx, cField->GetInt32());
        break;

      case Database::eFieldType::UInt64:
        DukTape::duk_push_number(lpCtx, (double)(cField->GetUInt64()));
        break;

      case Database::eFieldType::Int64:
        DukTape::duk_push_number(lpCtx, (double)(cField->GetInt64()));
        break;

      case Database::eFieldType::Double:
        DukTape::duk_push_number(lpCtx, cField->GetDouble());
        break;

      case Database::eFieldType::DateTime:
        CJavascriptVM::PushDate(lpCtx, *(cField->GetDateTime()), TRUE);
        break;

      case Database::eFieldType::Blob:
        p = (LPBYTE)(DukTape::duk_push_fixed_buffer(lpCtx, cField->GetLength()));
        ::MxMemCopy(p, cField->GetBlob(), cField->GetLength());
        DukTape::duk_push_buffer_object(lpCtx, -1, 0, cField->GetLength(), DUK_BUFOBJ_UINT8ARRAY);
        DukTape::duk_remove(lpCtx, -2);
        break;

      default:
        DukTape::duk_push_undefined(lpCtx);
        break;
    }

    //add field data
    sA = cConnector->GetFieldName(i);

    if (*sA == 0)
    {
      DukTape::duk_put_prop_index(lpCtx, -2, (DukTape::duk_uarridx_t)i);
    }
    else
    {
      DukTape::duk_dup(lpCtx, -1);
      DukTape::duk_put_prop_index(lpCtx, -3, (DukTape::duk_uarridx_t)i);
      DukTape::duk_put_prop_string(lpCtx, -2, sA);
    }
  }

  //done
  return 1;
}

DukTape::duk_ret_t CJsSQLitePlugin::BeginTransaction(_In_ DukTape::duk_context *lpCtx)
{
  HRESULT hRes;

  if (!cConnector)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);

  switch (DukTape::duk_get_top(lpCtx))
  {
    case 0:
      hRes = cConnector->TransactionStart();
      if (FAILED(hRes))
        ThrowDbError(lpCtx, hRes, __FILE__, __LINE__);
      break;

    case 1:
      {
      BOOL bIsImmediate = FALSE;
      BOOL bIsExclusive = FALSE;
      Database::CSQLite3Connector::eTxType nType;

      if (DukTape::duk_is_object(lpCtx, 0) == 0)
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);

      DukTape::duk_get_prop_string(lpCtx, 0, "immediate");
      if (duk_is_null_or_undefined(lpCtx, -1) == 0)
        bIsImmediate = (MX::CJavascriptVM::GetInt(lpCtx, -1) != 0) ? TRUE : FALSE;
      DukTape::duk_pop(lpCtx);
      //----
      DukTape::duk_get_prop_string(lpCtx, 0, "exclusive");
      if (duk_is_null_or_undefined(lpCtx, -1) == 0)
        bIsExclusive = (MX::CJavascriptVM::GetInt(lpCtx, -1) != 0) ? TRUE : FALSE;
      DukTape::duk_pop(lpCtx);

      if (bIsExclusive != FALSE)
        nType = Database::CSQLite3Connector::eTxType::Exclusive;
      else if (bIsImmediate != FALSE)
        nType = Database::CSQLite3Connector::eTxType::Immediate;
      else
        nType = Database::CSQLite3Connector::eTxType::Standard;

      hRes = cConnector->TransactionStart(nType);
      if (FAILED(hRes))
        ThrowDbError(lpCtx, hRes, __FILE__, __LINE__);
      }
      break;

    default:
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  }

  //done
  return 0;
}

DukTape::duk_ret_t CJsSQLitePlugin::CommitTransaction(_In_ DukTape::duk_context *lpCtx)
{
  HRESULT hRes;

  if (!cConnector)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);

  hRes = cConnector->TransactionCommit();
  if (FAILED(hRes))
    ThrowDbError(lpCtx, hRes, __FILE__, __LINE__);

  //done
  return 0;
}

DukTape::duk_ret_t CJsSQLitePlugin::RollbackTransaction(_In_ DukTape::duk_context *lpCtx)
{
  HRESULT hRes;

  if (!cConnector)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);

  hRes = cConnector->TransactionRollback();
  if (FAILED(hRes))
    ThrowDbError(lpCtx, hRes, __FILE__, __LINE__);

  //done
  return 0;
}

DukTape::duk_ret_t CJsSQLitePlugin::isConnected(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_boolean(lpCtx, (cConnector && cConnector->IsConnected() != FALSE) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsSQLitePlugin::getAffectedRows(_In_ DukTape::duk_context *lpCtx)
{
  if (cConnector)
    DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)(cConnector->GetAffectedRows()));
  else
    DukTape::duk_push_number(lpCtx, 0.0);
  return 1;
}

DukTape::duk_ret_t CJsSQLitePlugin::getInsertId(_In_ DukTape::duk_context *lpCtx)
{
  if (cConnector)
    DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)(cConnector->GetLastInsertId()));
  else
    DukTape::duk_push_number(lpCtx, 0.0);
  return 1;
}

VOID CJsSQLitePlugin::ThrowDbError(_In_ DukTape::duk_context *lpCtx, _In_ HRESULT hRes, _In_opt_ LPCSTR filename,
                                  _In_opt_ DukTape::duk_int_t line)
{
  LPCSTR sA;
  int err;

  if (hRes == E_INVALIDARG || hRes == E_OUTOFMEMORY)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  err = (cConnector) ? cConnector->GetErrorCode() : SQLITE_ERROR;
  if ((err & 0xFF) == SQLITE_NOMEM)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);

  DukTape::duk_get_global_string(lpCtx, "SQLiteError");

  DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)hRes);

  DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)err);

  sA = (cConnector) ? cConnector->GetErrorDescription() : "";
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

} //namespace MX
