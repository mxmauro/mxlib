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
#include <stdlib.h>
#include "..\..\..\Include\Strings\Utf8.h"

//-----------------------------------------------------------

#define _INTERNAL()        ((Internals::CJsMySqlPluginHelpers*)lpInternal)
#define _DB()              ((Internals::CJsMySqlPluginHelpers*)lpInternal)->lpDB
#define _RS()              ((Internals::CJsMySqlPluginHelpers*)lpInternal)->sQuery.lpResultSet
#define _CALLAPI(apiname)  Internals::API::fn_##apiname

#define longlong  LONGLONG
#define ulonglong ULONGLONG

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
  hRes = Internals::API::Initialize(sOptions.bUseDebugDll);
#else //_DEBUG
  hRes = Internals::API::Initialize();
#endif //_DEBUG
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  //create internal object
  lpInternal = MX_DEBUG_NEW Internals::CJsMySqlPluginHelpers();
  if (lpInternal == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
  //----
  _DB() = _CALLAPI(mysql_init)(NULL);
  if (_DB() == NULL)
  {
    delete _INTERNAL();
    lpInternal = NULL;
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
  }
  if (sOptions.nConnectTimeout > 0)
  {
    nTemp = sOptions.nConnectTimeout;
    _CALLAPI(mysql_options)(_DB(), MYSQL_OPT_CONNECT_TIMEOUT, &nTemp);
  }
  _CALLAPI(mysql_options)(_DB(), MYSQL_OPT_GUESS_CONNECTION, L"");
  if (sOptions.nReadTimeout > 0)
  {
    nTemp = sOptions.nReadTimeout;
    _CALLAPI(mysql_options)(_DB(), MYSQL_OPT_READ_TIMEOUT, &nTemp);
  }
  if (sOptions.nWriteTimeout > 0)
  {
    nTemp = sOptions.nWriteTimeout;
    _CALLAPI(mysql_options)(_DB(), MYSQL_OPT_WRITE_TIMEOUT, &nTemp);
  }
  nTemp = 1;
  _CALLAPI(mysql_options)(_DB(), MYSQL_REPORT_DATA_TRUNCATION, &nTemp);
  nTemp = 0;
  _CALLAPI(mysql_options)(_DB(), MYSQL_OPT_RECONNECT, &nTemp);
  nTemp = 1;
  _CALLAPI(mysql_options)(_DB(), MYSQL_SECURE_AUTH, &nTemp);
  //do connection
  if (_CALLAPI(mysql_real_connect)(_DB(), szHostA, (szUserNameA != NULL) ? szUserNameA : "",
                                   (szPasswordA != NULL) ? szPasswordA : "", NULL, (UINT)nPort, NULL,
                                   CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_COMPRESS | CLIENT_LOCAL_FILES |
                                   CLIENT_PROTOCOL_41 | CLIENT_TRANSACTIONS | CLIENT_FOUND_ROWS) == NULL)
  {
    ThrowDbError(__FILE__, __LINE__, TRUE);
    delete _INTERNAL();
    lpInternal = NULL;
    DukTape::duk_throw_raw(lpCtx);
    return 0;
  }
  if (_CALLAPI(mysql_set_character_set)(_DB(), "utf8") != 0)
  {
    ThrowDbError(__FILE__, __LINE__, TRUE);
    delete _INTERNAL();
    DukTape::duk_throw_raw(lpCtx);
    return 0;
  }
  //set database if any specified
  if (szDbNameA != NULL && *szDbNameA != 0)
  {
    if (_CALLAPI(mysql_select_db)(_DB(), szDbNameA) != 0)
    {
      ThrowDbError(__FILE__, __LINE__, TRUE);
      delete _INTERNAL();
      lpInternal = NULL;
      DukTape::duk_throw_raw(lpCtx);
      return 0;
    }
  }
  //done
  return 0;
}

DukTape::duk_ret_t CJsMySqlPlugin::Disconnect()
{
  if (lpInternal != NULL)
  {
    delete _INTERNAL();
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
  _INTERNAL()->QueryClose();
  if (_CALLAPI(mysql_select_db)(_DB(), szDbNameA) != 0)
    ThrowDbError(__FILE__, __LINE__);
  //done
  return 0;
}

DukTape::duk_ret_t CJsMySqlPlugin::Query()
{
  DukTape::duk_context *lpCtx = GetContext();
  LPCSTR szQueryA;
  HRESULT hRes;

  if (lpInternal == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);
  //get parameters
  szQueryA = DukTape::duk_require_string(lpCtx, 0);
  if (*szQueryA == 0)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  //execute query
  hRes = _INTERNAL()->ExecuteQuery(szQueryA, (SIZE_T)-1, &(_RS()));
  if (FAILED(hRes))
    ThrowDbError(hRes, __FILE__, __LINE__);
  //get some data
  _INTERNAL()->sQuery.nAffectedRows = _CALLAPI(mysql_affected_rows)(_DB());
  _INTERNAL()->sQuery.nLastInsertId = _CALLAPI(mysql_insert_id)(_DB());
  _INTERNAL()->sQuery.nFieldsCount = (_RS() != NULL) ? (SIZE_T)(_CALLAPI(mysql_num_fields)(_RS())) : 0;
  if (_INTERNAL()->BuildFieldInfoArray(lpCtx) == FALSE)
  {
    _INTERNAL()->QueryClose();
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
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
    _INTERNAL()->QueryClose();
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
  if (_INTERNAL()->nServerUsingNoBackslashEscapes == -1)
  {
    //since we cannot check "mysql->server_status" SERVER_STATUS_NO_BACKSLASH_ESCAPES's flag
    //directly... make a real escape of an apostrophe
    CHAR d[4];
    ULONG ret;

    ret = _CALLAPI(mysql_real_escape_string)(_DB(), d, "\'", 1);
    if (ret < 2)
    {
      _INTERNAL()->nServerUsingNoBackslashEscapes = 0; //(?) should not happen
    }
    else if (d[0] == '\\')
    {
      _INTERNAL()->nServerUsingNoBackslashEscapes = 0;
    }
    else
    {
      _INTERNAL()->nServerUsingNoBackslashEscapes = 1;
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
        if (_INTERNAL()->nServerUsingNoBackslashEscapes != 0)
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
        if (_INTERNAL()->nServerUsingNoBackslashEscapes != 0)
          goto doDefault;
        if (cStrResultA.ConcatN("\\r", 2) == FALSE)
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
        break;

      case '\n':
        if (_INTERNAL()->nServerUsingNoBackslashEscapes != 0)
          goto doDefault;
        if (cStrResultA.ConcatN("\\n", 2) == FALSE)
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
        break;

      case '\032':
        if (_INTERNAL()->nServerUsingNoBackslashEscapes != 0)
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
  static const DWORD dwMilliMult[3] = { 100, 10, 1 };
  DukTape::duk_context *lpCtx = GetContext();
  Internals::CJsMySqlPluginHelpers::CFieldInfo *lpFieldInfo;
  MYSQL_ROW lpRow;
  ULONG k, *lpnRowLengths;
  SIZE_T i, nCount;
  BYTE aTempBuf[8], *p;
  char *szEndPosA;
  union {
    ULONGLONG ull;
    LONGLONG ll;
    double nDbl;
    DWORD dw;
  } u;
  DWORD dwMillis;
  DukTape::duk_idx_t nObjIdx;
  LPCSTR sA, szFieldNameA;
  unsigned int nErr;

  if (lpInternal == NULL || _RS() == NULL)
  {
    DukTape::duk_push_null(lpCtx);
    return 1;
  }
  //fetch next row
  lpRow = _CALLAPI(mysql_fetch_row)(_RS());
  if (lpRow == NULL)
  {
    nErr = _CALLAPI(mysql_errno)(_DB());
    if (nErr != 0)
      ThrowDbError(__FILE__, __LINE__);
    DukTape::duk_push_null(lpCtx);
    return 1;
  }
  lpnRowLengths = _CALLAPI(mysql_fetch_lengths)(_RS());
  if (lpnRowLengths == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
  //process row
  nCount = _INTERNAL()->sQuery.nFieldsCount;
  if (nCount == 0)
  {
    DukTape::duk_push_null(lpCtx);
    return 1;
  }
  //create an object
  DukTape::duk_push_object(lpCtx);
  for (i=0; i<nCount; i++)
  {
    lpFieldInfo = _INTERNAL()->sQuery.aFieldsList.GetElementAt(i);
    if (lpRow[i] != NULL)
    {
      sA = lpRow[i];
      switch (lpFieldInfo->GetType())
      {
        case MYSQL_TYPE_BIT:
          u.ull = 0;
          if (lpnRowLengths[i] >= 1 && lpnRowLengths[i] <= 8)
          {
            MemSet(aTempBuf, 0, 8);
            MemCopy(aTempBuf, lpRow[i], lpnRowLengths[i]);
            if ((lpFieldInfo->GetFlags() & UNSIGNED_FLAG) != 0)
            {
              u.ull = (ULONGLONG)uint8korr(aTempBuf);
            }
            else
            {
              if ((aTempBuf[lpnRowLengths[i]-1] & 0x80) != 0)
                MemSet(aTempBuf+lpnRowLengths[i], 0xFF, 8-lpnRowLengths[i]);
              u.ll = (LONGLONG)sint8korr(aTempBuf);
            }
          }
          if ((lpFieldInfo->GetFlags() & UNSIGNED_FLAG) != 0)
            DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)u.ull);
          else
            DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)u.ll);
          break;

        case MYSQL_TYPE_TINY:
        case MYSQL_TYPE_SHORT:
        case MYSQL_TYPE_LONG:
        case MYSQL_TYPE_LONGLONG:
        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_YEAR:
          if ((lpFieldInfo->GetFlags() & UNSIGNED_FLAG) != 0)
          {
            u.ull = _strtoui64(sA, &szEndPosA, 10);
            if (u.ull <= 0xFFFFFFFFui64)
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.ull);
            else
              DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)u.ull);
          }
          else
          {
            u.ll = _strtoi64(sA, &szEndPosA, 10);
            if (u.ll >= -2147483648i64 && u.ull < 2147483647i64)
              DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)u.ll);
            else
              DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)u.ll);
          }
          break;

        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_FLOAT:
        case MYSQL_TYPE_DOUBLE:
        case MYSQL_TYPE_NEWDECIMAL:
          u.nDbl = atof(sA);
          DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)u.nDbl);
          break;

        case MYSQL_TYPE_NULL:
          DukTape::duk_push_null(lpCtx);
          break;

        case MYSQL_TYPE_DATE:
        case MYSQL_TYPE_NEWDATE:
          //"YYYY-MM-DD"
          if (lpnRowLengths[i] == 10 &&
              sA[0] >= '0' && sA[0] <= '9' &&
              sA[1] >= '0' && sA[1] <= '9' &&
              sA[2] >= '0' && sA[2] <= '9' &&
              sA[3] >= '0' && sA[3] <= '9' &&
              sA[4] == '-' &&
              sA[5] >= '0' && sA[5] <= '9' &&
              sA[6] >= '0' && sA[6] <= '9' &&
              sA[7] == '-' &&
              sA[8] >= '0' && sA[8] <= '9' &&
              sA[9] >= '0' && sA[9] <= '9')
          {
            if (MX::StrNCompareA(sA, "0000-00-00", 10) != 0)
            {
              DukTape::duk_get_global_string(lpCtx, "Date");
              DukTape::duk_new(lpCtx, 0);
              nObjIdx = DukTape::duk_normalize_index(lpCtx, -1);
              //----
              DukTape::duk_get_prop_string(lpCtx, nObjIdx, "setUTCFullYear");
              DukTape::duk_dup(lpCtx, nObjIdx);
              u.dw = (DWORD)(sA[0] - '0') * 1000 + (DWORD)(sA[1] - '0') * 100 +
                     (DWORD)(sA[2] - '0') * 10   + (DWORD)(sA[3] - '0');
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
              u.dw = (DWORD)(sA[5] - '0') * 10 + (DWORD)(sA[6] - '0');
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw - 1);
              u.dw = (DWORD)(sA[8] - '0') * 10 + (DWORD)(sA[9] - '0');
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
              DukTape::duk_call_method(lpCtx, 3);
              DukTape::duk_pop(lpCtx); //pop void return
              //----
              DukTape::duk_get_prop_string(lpCtx, nObjIdx, "setUTCHours");
              DukTape::duk_dup(lpCtx, nObjIdx);
              DukTape::duk_push_uint(lpCtx, 0);
              DukTape::duk_push_uint(lpCtx, 0);
              DukTape::duk_push_uint(lpCtx, 0);
              DukTape::duk_push_uint(lpCtx, 0);
              DukTape::duk_call_method(lpCtx, 4);
              DukTape::duk_pop(lpCtx); //pop void return
            }
            else
            {
              DukTape::duk_push_undefined(lpCtx);
            }
          }
          else
          {
            DukTape::duk_push_undefined(lpCtx);
          }
          break;

        case MYSQL_TYPE_TIME:
          //"HH:MM:SS[.ffff]"
          if (lpnRowLengths[i] >= 8 &&
              sA[0] >= '0' && sA[0] <= '9' &&
              sA[1] >= '0' && sA[1] <= '9' &&
              sA[2] == ':' &&
              sA[3] >= '0' && sA[3] <= '9' &&
              sA[4] >= '0' && sA[4] <= '9' &&
              sA[5] == ':' &&
              sA[6] >= '0' && sA[6] <= '9' &&
              sA[7] >= '0' && sA[7] <= '9')
          {
            dwMillis = 0;
            if (lpnRowLengths[i] >= 9)
            {
              if (sA[8] == '.')
              {
                for (k=9; k<12 && k<lpnRowLengths[i]; k++)
                {
                  if (sA[k] >= '0' && sA[k] <= '9')
                  {
                    dwMillis += (DWORD)(sA[k] - '0') * dwMilliMult[k-9];
                  }
                  else
                  {
                    dwMillis = 0xFFFFFFFFUL;
                    break;
                  }
                }
              }
              else
              {
                dwMillis = 0xFFFFFFFFUL;
              }
            }
            if (dwMillis != 0xFFFFFFFFUL)
            {
              DukTape::duk_get_global_string(lpCtx, "Date");
              DukTape::duk_new(lpCtx, 0);
              nObjIdx = DukTape::duk_normalize_index(lpCtx, -1);
              //----
              DukTape::duk_get_prop_string(lpCtx, nObjIdx, "setUTCHours");
              DukTape::duk_dup(lpCtx, nObjIdx);
              u.dw = (DWORD)(sA[0] - '0') * 10 + (DWORD)(sA[1] - '0');
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
              u.dw = (DWORD)(sA[3] - '0') * 10 + (DWORD)(sA[4] - '0');
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
              u.dw = (DWORD)(sA[6] - '0') * 10 + (DWORD)(sA[7] - '0');
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)dwMillis);
              DukTape::duk_call_method(lpCtx, 4);
              DukTape::duk_pop(lpCtx); //pop void return
            }
            else
            {
              DukTape::duk_push_undefined(lpCtx);
            }
          }
          else
          {
            DukTape::duk_push_undefined(lpCtx);
          }
          break;

        case MYSQL_TYPE_TIMESTAMP:
        case MYSQL_TYPE_DATETIME:
          //"YYYY-MM-DD HH:MM:SS[.ffff]"
          if (lpnRowLengths[i] >= 19 &&
              sA[0] >= '0' && sA[0] <= '9' &&
              sA[1] >= '0' && sA[1] <= '9' &&
              sA[2] >= '0' && sA[2] <= '9' &&
              sA[3] >= '0' && sA[3] <= '9' &&
              sA[4] == '-' &&
              sA[5] >= '0' && sA[5] <= '9' &&
              sA[6] >= '0' && sA[6] <= '9' &&
              sA[7] == '-' &&
              sA[8] >= '0' && sA[8] <= '9' &&
              sA[9] >= '0' && sA[9] <= '9' &&
              sA[10] == ' ' &&
              sA[11] >= '0' && sA[11] <= '9' &&
              sA[12] >= '0' && sA[12] <= '9' &&
              sA[13] == ':' &&
              sA[14] >= '0' && sA[14] <= '9' &&
              sA[15] >= '0' && sA[15] <= '9' &&
              sA[16] == ':' &&
              sA[17] >= '0' && sA[17] <= '9' &&
              sA[18] >= '0' && sA[18] <= '9')
          {
            dwMillis = 0;
            if (lpnRowLengths[i] >= 20)
            {
              if (sA[19] == '.')
              {
                for (k=20; k<23 && k<lpnRowLengths[i]; k++)
                {
                  if (sA[k] >= '0' && sA[k] <= '9')
                  {
                    dwMillis += (DWORD)(sA[k] - '0') * dwMilliMult[k-20];
                  }
                  else
                  {
                    dwMillis = 0xFFFFFFFFUL;
                    break;
                  }
                }
              }
              else
              {
                dwMillis = 0xFFFFFFFFUL;
              }
            }
            if (dwMillis != 0xFFFFFFFFUL)
            {
              DukTape::duk_get_global_string(lpCtx, "Date");
              DukTape::duk_new(lpCtx, 0);
              nObjIdx = DukTape::duk_normalize_index(lpCtx, -1);
              //set date if not all zeroes
              if (MX::StrNCompareA(sA, "0000-00-00", 10) != 0)
              {
                DukTape::duk_get_prop_string(lpCtx, nObjIdx, "setUTCFullYear");
                DukTape::duk_dup(lpCtx, nObjIdx);
                u.dw = (DWORD)(sA[0] - '0') * 1000 + (DWORD)(sA[1] - '0') * 100 +
                       (DWORD)(sA[2] - '0') * 10   + (DWORD)(sA[3] - '0');
                DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
                u.dw = (DWORD)(sA[5] - '0') * 10 + (DWORD)(sA[6] - '0');
                DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw - 1);
                u.dw = (DWORD)(sA[8] - '0') * 10 + (DWORD)(sA[9] - '0');
                DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
                DukTape::duk_call_method(lpCtx, 3);
                DukTape::duk_pop(lpCtx); //pop void return
              }
              //set time
              DukTape::duk_get_prop_string(lpCtx, nObjIdx, "setUTCHours");
              DukTape::duk_dup(lpCtx, nObjIdx);
              u.dw = (DWORD)(sA[11] - '0') * 10 + (DWORD)(sA[12] - '0');
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
              u.dw = (DWORD)(sA[14] - '0') * 10 + (DWORD)(sA[15] - '0');
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
              u.dw = (DWORD)(sA[17] - '0') * 10 + (DWORD)(sA[18] - '0');
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)dwMillis);
              DukTape::duk_call_method(lpCtx, 4);
              DukTape::duk_pop(lpCtx); //pop void return
            }
            else
            {
              DukTape::duk_push_undefined(lpCtx);
            }
          }
          else
          {
            DukTape::duk_push_undefined(lpCtx);
          }
          break;

        case MYSQL_TYPE_TINY_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_LONG_BLOB:
        case MYSQL_TYPE_BLOB:
          if ((lpFieldInfo->GetFlags() & BINARY_FLAG) != 0)
          {
            p = (LPBYTE)(DukTape::duk_push_fixed_buffer(lpCtx, (DukTape::duk_size_t)(lpnRowLengths[i])));
            MemCopy(p, lpRow[i], (SIZE_T)(lpnRowLengths[i]));
            DukTape::duk_push_buffer_object(lpCtx, -1, 0, (DukTape::duk_size_t)(lpnRowLengths[i]),
                                            DUK_BUFOBJ_UINT8ARRAY);
            DukTape::duk_remove(lpCtx, -2);
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
          DukTape::duk_push_lstring(lpCtx, lpRow[i], (DukTape::duk_size_t)lpnRowLengths[i]);
          break;
      }
    }
    else
    {
      DukTape::duk_push_null(lpCtx);
    }
    szFieldNameA = lpFieldInfo->GetName();
    if (*szFieldNameA == 0)
    {
      DukTape::duk_put_prop_index(lpCtx, -2, (DukTape::duk_uarridx_t)i);
    }
    else
    {
      DukTape::duk_dup(lpCtx, -1);
      DukTape::duk_put_prop_index(lpCtx, -3, (DukTape::duk_uarridx_t)i);
      DukTape::duk_put_prop_string(lpCtx, -2, szFieldNameA);
    }
  }
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::BeginTransaction()
{
  HRESULT hRes;

  hRes = (lpInternal != NULL) ? _TransactionStart() : MX_E_NotReady;
  if (FAILED(hRes))
    ThrowDbError(hRes, __FILE__, __LINE__);
  return 0;
}

DukTape::duk_ret_t CJsMySqlPlugin::CommitTransaction()
{
  HRESULT hRes;

  hRes = (lpInternal != NULL) ? _TransactionCommit() : MX_E_NotReady;
  if (FAILED(hRes))
    ThrowDbError(hRes, __FILE__, __LINE__);
  return 0;
}

DukTape::duk_ret_t CJsMySqlPlugin::RollbackTransaction()
{
  HRESULT hRes;

  hRes = (lpInternal != NULL) ? _TransactionRollback() : MX_E_NotReady;
  if (FAILED(hRes))
    ThrowDbError(hRes, __FILE__, __LINE__);
  return 0;
}

DukTape::duk_ret_t CJsMySqlPlugin::getAffectedRows()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (lpInternal != NULL)
    DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)(_INTERNAL()->sQuery.nAffectedRows));
  else
    DukTape::duk_push_number(lpCtx, 0.0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::getInsertId()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (lpInternal != NULL)
    DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)(_INTERNAL()->sQuery.nLastInsertId));
  else
    DukTape::duk_push_number(lpCtx, 0.0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::getFieldsCount()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (lpInternal != NULL && _RS() != NULL)
  {
    UINT nFieldsCount;

    nFieldsCount = _CALLAPI(mysql_num_fields)(_RS());
    DukTape::duk_push_uint(lpCtx, nFieldsCount);
  }
  else
  {
    DukTape::duk_push_uint(lpCtx, 0);
  }
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::getFields()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (lpInternal != NULL && _RS() != NULL)
  {
    Internals::CJsMySqlPluginHelpers::CFieldInfo *lpFieldInfo;
    SIZE_T i, nCount;

    //create an array
    DukTape::duk_push_object(lpCtx);
    nCount = _INTERNAL()->sQuery.aFieldsList.GetCount();
    //add each field as the array index
    for (i=0; i<nCount; i++)
    {
      lpFieldInfo = _INTERNAL()->sQuery.aFieldsList.GetElementAt(i);
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

HRESULT CJsMySqlPlugin::_TransactionStart()
{
  return _INTERNAL()->ExecuteQuery("START TRANSACTION;", 18);
}

HRESULT CJsMySqlPlugin::_TransactionCommit()
{
  return _INTERNAL()->ExecuteQuery("COMMIT;", 7);
}

HRESULT CJsMySqlPlugin::_TransactionRollback()
{
  return _INTERNAL()->ExecuteQuery("ROLLBACK;", 9);
}

VOID CJsMySqlPlugin::ThrowDbError(_In_ HRESULT hRes, _In_opt_ LPCSTR filename, _In_opt_ DukTape::duk_int_t line,
                                  _In_opt_ BOOL bOnlyPush)
{
  DukTape::duk_context *lpCtx = GetContext();
  LPCSTR sA;
  int nDbErr;

  if (hRes == E_INVALIDARG || hRes == E_OUTOFMEMORY)
    MX::CJavascriptVM::ThrowWindowsError(lpCtx, hRes, filename, line);

  DukTape::duk_get_global_string(lpCtx, "MySqlError");
  DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)hRes);

  nDbErr = (int)_CALLAPI(mysql_errno)(_DB());
  if (nDbErr == 0)
    nDbErr = ER_UNKNOWN_ERROR;
  DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)nDbErr);

  sA = _CALLAPI(mysql_error)(_DB());
  DukTape::duk_push_string(lpCtx, (sA != NULL) ? sA : "");

  sA = _CALLAPI(mysql_sqlstate)(_DB());
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

VOID CJsMySqlPlugin::ThrowDbError(_In_opt_ LPCSTR filename, _In_opt_ DukTape::duk_int_t line, _In_opt_ BOOL bOnlyPush)
{
  DukTape::duk_context *lpCtx = GetContext();
  int err;

  err = (int)_CALLAPI(mysql_errno)(_DB());
  if (err == 0)
    err = ER_UNKNOWN_ERROR;
  return ThrowDbError(Internals::CJsMySqlPluginHelpers::HResultFromMySqlErr(err), filename, line, bOnlyPush);
}

} //namespace MX
