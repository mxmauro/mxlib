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

//NOTE: UNDOCUMENTED!!!
extern "C" {
  extern _locale_tstruct __initiallocalestructinfo;
};

#define _INTERNAL()        ((Internals::CJsMySqlPluginHelpers*)lpInternal)
#define _DB()              ((Internals::CJsMySqlPluginHelpers*)lpInternal)->lpDB
#define _RS()              ((Internals::CJsMySqlPluginHelpers*)lpInternal)->sQuery.lpResultSet
#define _CALLAPI(apiname)  Internals::API::fn_##apiname

#define longlong  LONGLONG
#define ulonglong ULONGLONG

//-----------------------------------------------------------

namespace MX {

CJsMySqlPlugin::CJsMySqlPlugin(__in DukTape::duk_context *lpCtx) : CJsObjectBase(lpCtx)
{
  lpInternal = NULL;
  hLastErr = S_OK;
  nLastDbErr = 0;
  szLastDbErrA[0] = szLastSqlStateA[0] = 0;
  return;
}

CJsMySqlPlugin::~CJsMySqlPlugin()
{
  Disconnect(NULL);
  return;
}

DukTape::duk_ret_t CJsMySqlPlugin::Connect(__in DukTape::duk_context *lpCtx)
{
  LPCSTR szHostA, szUserNameA, szPasswordA, szDbNameA;
  DukTape::duk_idx_t nParamsCount;
  DukTape::duk_ret_t nRet;
  int nPort;
  long nTemp;
  HRESULT hRes;

  Disconnect(lpCtx);
  //get parameters
  nParamsCount = DukTape::duk_get_top(lpCtx);
  if (nParamsCount < 1 || nParamsCount > 5)
    return ReturnErrorFromHResult(lpCtx, E_INVALIDARG);
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
      return ReturnErrorFromHResult(lpCtx, E_INVALIDARG);
  }
  //initialize APIs
  hRes = Internals::API::Initialize();
  if (FAILED(hRes))
    return ReturnErrorFromHResult(lpCtx, hRes);
  //create internal object
  lpInternal = MX_DEBUG_NEW Internals::CJsMySqlPluginHelpers();
  if (lpInternal == NULL)
    return ReturnErrorFromHResult(lpCtx, E_OUTOFMEMORY);
  //----
  _DB() = _CALLAPI(mysql_init)(NULL);
  if (_DB() == NULL)
  {
    delete _INTERNAL();
    lpInternal = NULL;
    return ReturnErrorFromHResult(lpCtx, E_OUTOFMEMORY);
  }
  nTemp = 10;
  _CALLAPI(mysql_options)(_DB(), MYSQL_OPT_CONNECT_TIMEOUT, (const LPCSTR)&nTemp);
  _CALLAPI(mysql_options)(_DB(), MYSQL_OPT_GUESS_CONNECTION, (const LPCSTR)L"");
  nTemp = 15;
  _CALLAPI(mysql_options)(_DB(), MYSQL_OPT_READ_TIMEOUT, (const LPCSTR)&nTemp);
  nTemp = 15;
  _CALLAPI(mysql_options)(_DB(), MYSQL_OPT_WRITE_TIMEOUT, (const LPCSTR)&nTemp);
  nTemp = 1;
  _CALLAPI(mysql_options)(_DB(), MYSQL_REPORT_DATA_TRUNCATION, (const LPCSTR)&nTemp);
  nTemp = 0;
  _CALLAPI(mysql_options)(_DB(), MYSQL_OPT_RECONNECT, (const LPCSTR)&nTemp);
  nTemp = 1;
  _CALLAPI(mysql_options)(_DB(), MYSQL_SECURE_AUTH, (const LPCSTR)&nTemp);
  //do connection
  if (_CALLAPI(mysql_real_connect)(_DB(), szHostA, (szUserNameA != NULL) ? szUserNameA : "",
                                   (szPasswordA != NULL) ? szPasswordA : "", NULL, (UINT)nPort, NULL,
                                   CLIENT_LONG_PASSWORD|CLIENT_LONG_FLAG|CLIENT_COMPRESS|CLIENT_LOCAL_FILES|
                                   CLIENT_PROTOCOL_41|CLIENT_TRANSACTIONS|CLIENT_SECURE_CONNECTION) == NULL)
  {
    nRet = ReturnErrorFromHResultAndDbErr(lpCtx, E_FAIL);
    delete _INTERNAL();
    lpInternal = NULL;
    return nRet;
  }
  if (_CALLAPI(mysql_set_character_set)(_DB(), "utf8") != 0)
  {
    nRet = ReturnErrorFromLastDbErr(lpCtx);
    delete _INTERNAL();
    lpInternal = NULL;
    return nRet;
  }
  //set database if any specified
  if (szDbNameA != NULL && *szDbNameA != 0)
  {
    if (_CALLAPI(mysql_select_db)(_DB(), szDbNameA) != 0)
    {
      nRet = ReturnErrorFromLastDbErr(lpCtx);
      delete _INTERNAL();
      lpInternal = NULL;
      return nRet;
    }
  }
  //done
  return ReturnErrorFromHResult(lpCtx, S_OK);
}

DukTape::duk_ret_t CJsMySqlPlugin::Disconnect(__in DukTape::duk_context *lpCtx)
{
  if (lpInternal != NULL)
  {
    if (_DB() != NULL)
    {
      /*
      int nRetryCount;

      _INTERNAL()->QueryClose(FALSE);
      nRetryCount = 20;
      while (nRetryCount > 0 && _INTERNAL()->nTransactionCounter > 0)
      {
        if (FAILED(TransactionRollback(err)))
          nRetryCount--;
      }
      */
      _CALLAPI(mysql_close)(_DB());
    }
    //----
    delete _INTERNAL();
    lpInternal = NULL;
  }
  hLastErr = S_OK;
  nLastDbErr = 0;
  szLastDbErrA[0] = szLastSqlStateA[0] = 0;
  //done
  return 0;
}

DukTape::duk_ret_t CJsMySqlPlugin::SelectDatabase(__in DukTape::duk_context *lpCtx)
{
  LPCSTR szDbNameA;

  if (lpInternal == NULL)
    return ReturnErrorFromHResult(lpCtx, MX_E_NotReady);
  //get parameters
  szDbNameA = DukTape::duk_require_string(lpCtx, 0);
  if (*szDbNameA == 0)
    return ReturnErrorFromHResult(lpCtx, E_INVALIDARG);
  //select database
  _INTERNAL()->QueryClose();
  if (_CALLAPI(mysql_select_db)(_DB(), szDbNameA) != 0)
    return ReturnErrorFromLastDbErr(lpCtx);
  //done
  return ReturnErrorFromHResult(lpCtx, S_OK);
}

DukTape::duk_ret_t CJsMySqlPlugin::Query(__in DukTape::duk_context *lpCtx)
{
  LPCSTR szQueryA;
  HRESULT hRes;

  if (lpInternal == NULL)
    return ReturnErrorFromHResult(lpCtx, MX_E_NotReady);
  //get parameters
  szQueryA = DukTape::duk_require_string(lpCtx, 0);
  if (*szQueryA == 0)
    return ReturnErrorFromHResult(lpCtx, E_INVALIDARG);
  //execute query
  hRes = _INTERNAL()->ExecuteQuery(szQueryA, (SIZE_T)-1, &(_RS()));
  if (FAILED(hRes))
    return ReturnErrorFromHResultAndDbErr(lpCtx, hRes);
  //get some data
  _INTERNAL()->sQuery.nAffectedRows = _CALLAPI(mysql_affected_rows)(_DB());
  _INTERNAL()->sQuery.nLastInsertId = _CALLAPI(mysql_insert_id)(_DB());
  _INTERNAL()->sQuery.nFieldsCount = (_RS() != NULL) ? (SIZE_T)(_CALLAPI(mysql_num_fields)(_RS())) : 0;
  if (_INTERNAL()->BuildFieldInfoArray() == FALSE)
  {
    _INTERNAL()->QueryClose();
    return ReturnErrorFromHResult(lpCtx, E_OUTOFMEMORY);
  }
  //done
  return ReturnErrorFromHResult(lpCtx, S_OK);
}

DukTape::duk_ret_t CJsMySqlPlugin::QueryAndFetch(__in DukTape::duk_context *lpCtx)
{
  DukTape::duk_ret_t ret;

  ret = Query(lpCtx);
  if (ret == 1 && DukTape::duk_require_boolean(lpCtx, -1) != 0)
  {
    DukTape::duk_pop(lpCtx);
    ret = FetchRow(lpCtx);
  }
  return ret;
}

DukTape::duk_ret_t CJsMySqlPlugin::QueryClose(__in DukTape::duk_context *lpCtx)
{
  if (lpInternal != NULL)
    _INTERNAL()->QueryClose();
  //done
  return 0;
}

DukTape::duk_ret_t CJsMySqlPlugin::EscapeString(__in DukTape::duk_context *lpCtx)
{
  CStringA cStrResultA;
  DukTape::duk_idx_t nParamsCount;
  BOOL bIsLikeStatement;
  LPCSTR szStrA;

  if (lpInternal == NULL)
    return ReturnErrorFromHResult(lpCtx, MX_E_NotReady);
  //get parameters
  nParamsCount = DukTape::duk_get_top(lpCtx);
  if (nParamsCount < 1 || nParamsCount > 2)
    return ReturnErrorFromHResult(lpCtx, E_INVALIDARG);
  szStrA = DukTape::duk_require_string(lpCtx, 0);
  bIsLikeStatement = FALSE;
  if (nParamsCount > 1)
    bIsLikeStatement = (DukTape::duk_require_boolean(lpCtx, 1) != 0) ? TRUE : FALSE;
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
            return ReturnErrorFromHResult(lpCtx, E_OUTOFMEMORY);
          break;
        }
        //fall into next...
      case '"':
        if (cStrResultA.AppendFormat("\\%c", *szStrA) == FALSE)
          return ReturnErrorFromHResult(lpCtx, E_OUTOFMEMORY);
        break;

      case '\r':
        if (_INTERNAL()->nServerUsingNoBackslashEscapes != 0)
          goto doDefault;
        if (cStrResultA.ConcatN("\\r", 2) == FALSE)
          return ReturnErrorFromHResult(lpCtx, E_OUTOFMEMORY);
        break;

      case '\n':
        if (_INTERNAL()->nServerUsingNoBackslashEscapes != 0)
          goto doDefault;
        if (cStrResultA.ConcatN("\\n", 2) == FALSE)
          return ReturnErrorFromHResult(lpCtx, E_OUTOFMEMORY);
        break;

      case '\032':
        if (_INTERNAL()->nServerUsingNoBackslashEscapes != 0)
          goto doDefault;
        if (cStrResultA.ConcatN("\\Z", 2) == FALSE)
          return ReturnErrorFromHResult(lpCtx, E_OUTOFMEMORY);
        break;

      case 0:
        if (cStrResultA.ConcatN("\\0", 2) == FALSE)
          return ReturnErrorFromHResult(lpCtx, E_OUTOFMEMORY);
        break;

      case '\\':
        if (bIsLikeStatement != FALSE && (szStrA[1]=='%' || szStrA[1]=='_'))
        {
          szStrA++;
          if (cStrResultA.AppendFormat("\\%c", *szStrA) == FALSE)
            return ReturnErrorFromHResult(lpCtx, E_OUTOFMEMORY);
        }
        else
        {
          if (cStrResultA.ConcatN("\\\\", 2) == FALSE)
            return ReturnErrorFromHResult(lpCtx, E_OUTOFMEMORY);
        }
        break;

      default:
doDefault:
        if (cStrResultA.ConcatN(szStrA, 1) == FALSE)
          return ReturnErrorFromHResult(lpCtx, E_OUTOFMEMORY);
         break;
    }
    szStrA++;
  }
  //done
  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrResultA, cStrResultA.GetLength());
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::FetchRow(__in DukTape::duk_context *lpCtx)
{
  static const DWORD dwMilliMult[3] ={ 100, 10, 1 };
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
  LPCSTR sA, szFieldNameA;

  if (lpInternal == NULL || _RS() == NULL)
  {
    hLastErr = MX_E_NotReady;
    nLastDbErr = 0;
    szLastDbErrA[0] = szLastSqlStateA[0] = 0;
    DukTape::duk_push_null(lpCtx);
    return 1;
  }
  //fetch next row
  lpRow = _CALLAPI(mysql_fetch_row)(_RS());
  if (lpRow == NULL)
  {
    nLastDbErr = _CALLAPI(mysql_errno)(_DB());
    strncpy_s(szLastDbErrA, _countof(szLastDbErrA), _CALLAPI(mysql_error)(_DB()), _TRUNCATE);
    strncpy_s(szLastSqlStateA, _countof(szLastSqlStateA), _CALLAPI(mysql_sqlstate)(_DB()), _TRUNCATE);
    hLastErr = (nLastDbErr == 0) ? MX_E_EndOfFileReached : E_FAIL;
    DukTape::duk_push_null(lpCtx);
    return 1;
  }
  lpnRowLengths = _CALLAPI(mysql_fetch_lengths)(_RS());
  if (lpnRowLengths == NULL)
  {
    nLastDbErr = 0;
    szLastDbErrA[0] = szLastSqlStateA[0] = 0;
    hLastErr = E_OUTOFMEMORY;
    DukTape::duk_push_null(lpCtx);
    return 1;
  }
  //process row
  nCount = _INTERNAL()->sQuery.nFieldsCount;
  if (nCount == 0)
  {
    nLastDbErr = 0;
    szLastDbErrA[0] = szLastSqlStateA[0] = 0;
    hLastErr = S_OK;
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
            u.ull = _strtoui64_l(sA, &szEndPosA, 10, &__initiallocalestructinfo);
            if (u.ull <= 0xFFFFFFFFui64)
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.ull);
            else
              DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)u.ull);
          }
          else
          {
            u.ll = _strtoi64_l(sA, &szEndPosA, 10, &__initiallocalestructinfo);
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
          u.nDbl = _atof_l(sA, &__initiallocalestructinfo);
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
            DukTape::duk_get_global_string(lpCtx, "Date");
            u.dw = (DWORD)(sA[0] - '0') * 1000 + (DWORD)(sA[1] - '0') * 100 +
                   (DWORD)(sA[2] - '0') * 10   + (DWORD)(sA[3] - '0');
            DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
            u.dw = (DWORD)(sA[5] - '0') * 10 + (DWORD)(sA[6] - '0');
            DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
            u.dw = (DWORD)(sA[8] - '0') * 10 + (DWORD)(sA[9] - '0');
            DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
            DukTape::duk_new(lpCtx, 3);
          }
          else
          {
            DukTape::duk_push_undefined(lpCtx);
          }
          break;

        case MYSQL_TYPE_TIME:
          //"HH:MM:SS"
          if (lpnRowLengths[i] == 8 &&
              sA[0] >= '0' && sA[0] <= '9' &&
              sA[1] >= '0' && sA[1] <= '9' &&
              sA[2] == ':' &&
              sA[3] >= '0' && sA[3] <= '9' &&
              sA[4] >= '0' && sA[4] <= '9' &&
              sA[5] == ':' &&
              sA[6] >= '0' && sA[6] <= '9' &&
              sA[7] >= '0' && sA[7] <= '9')
          {
            DukTape::duk_get_global_string(lpCtx, "Date");
            DukTape::duk_push_number(lpCtx, 1.0);
            DukTape::duk_push_number(lpCtx, 1.0);
            DukTape::duk_push_number(lpCtx, 1.0);
            u.dw = (DWORD)(sA[0] - '0') * 10 + (DWORD)(sA[1] - '0');
            DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
            u.dw = (DWORD)(sA[3] - '0') * 10 + (DWORD)(sA[4] - '0');
            DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
            u.dw = (DWORD)(sA[6] - '0') * 10 + (DWORD)(sA[7] - '0');
            DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
            DukTape::duk_new(lpCtx, 6);
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
              u.dw = (DWORD)(sA[0] - '0') * 1000 + (DWORD)(sA[1] - '0') * 100 +
                     (DWORD)(sA[2] - '0') * 10   + (DWORD)(sA[3] - '0');
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
              u.dw = (DWORD)(sA[5] - '0') * 10 + (DWORD)(sA[6] - '0');
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
              u.dw = (DWORD)(sA[8] - '0') * 10 + (DWORD)(sA[9] - '0');
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
              u.dw = (DWORD)(sA[11] - '0') * 10 + (DWORD)(sA[12] - '0');
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
              u.dw = (DWORD)(sA[14] - '0') * 10 + (DWORD)(sA[15] - '0');
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
              u.dw = (DWORD)(sA[17] - '0') * 10 + (DWORD)(sA[18] - '0');
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)u.dw);
              DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)dwMillis);
              DukTape::duk_new(lpCtx, 7);
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

DukTape::duk_ret_t CJsMySqlPlugin::BeginTransaction(__in DukTape::duk_context *lpCtx)
{
  if (lpInternal == NULL)
    return ReturnErrorFromHResult(lpCtx, MX_E_NotReady);
  return ReturnErrorFromHResultAndDbErr(lpCtx, _TransactionStart());
}

DukTape::duk_ret_t CJsMySqlPlugin::CommitTransaction(__in DukTape::duk_context *lpCtx)
{
  if (lpInternal == NULL)
    return ReturnErrorFromHResult(lpCtx, MX_E_NotReady);
  return ReturnErrorFromHResultAndDbErr(lpCtx, _TransactionCommit());
}

DukTape::duk_ret_t CJsMySqlPlugin::RollbackTransaction(__in DukTape::duk_context *lpCtx)
{
  if (lpInternal == NULL)
    return ReturnErrorFromHResult(lpCtx, MX_E_NotReady);
  return ReturnErrorFromHResultAndDbErr(lpCtx, _TransactionRollback());
}

DukTape::duk_ret_t CJsMySqlPlugin::getLastError(__in DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_int(lpCtx, (int)hLastErr);
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::getLastDbError(__in DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_int(lpCtx, (int)nLastDbErr);
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::getLastDbErrorMessage(__in DukTape::duk_context *lpCtx)
{
  SIZE_T i;

  for (i=0; szLastDbErrA[i]!=0; i++)
  {
    if ((BYTE)szLastDbErrA[i] >= 128)
      break;
  }
  if (szLastDbErrA[i] == 0)
  {
    DukTape::duk_push_lstring(lpCtx, szLastDbErrA, i);
  }
  else
  {
    CStringA cStrTempA;
    CStringW cStrTempW;

    if (cStrTempW.Copy(szLastDbErrA) != FALSE &&
        SUCCEEDED(MX::Utf8_Encode(cStrTempA, (LPCWSTR)cStrTempW, cStrTempW.GetLength())))
    {
      DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTempA, cStrTempA.GetLength());
    }
    else
    {
      MX_JS_THROW_ERROR(lpCtx, DUK_ERR_ALLOC_ERROR, "**%08X", E_OUTOFMEMORY);
    }
  }
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::getLastSqlState(__in DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_string(lpCtx, szLastSqlStateA);
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::getAffectedRows(__in DukTape::duk_context *lpCtx)
{
  if (lpInternal != NULL)
    DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)(_INTERNAL()->sQuery.nAffectedRows));
  else
    DukTape::duk_push_number(lpCtx, 0.0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::getInsertId(__in DukTape::duk_context *lpCtx)
{
  if (lpInternal != NULL)
    DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)(_INTERNAL()->sQuery.nLastInsertId));
  else
    DukTape::duk_push_number(lpCtx, 0.0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::getFieldsCount(__in DukTape::duk_context *lpCtx)
{
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

DukTape::duk_ret_t CJsMySqlPlugin::getFields(__in DukTape::duk_context *lpCtx)
{
  if (lpInternal != NULL && _RS() != NULL)
  {
    Internals::CJsMySqlPluginHelpers::CFieldInfo *lpFieldInfo;
    DukTape::duk_ret_t nRet;
    SIZE_T i, nCount;

    //create an array
    DukTape::duk_push_object(lpCtx);
    nCount = _INTERNAL()->sQuery.aFieldsList.GetCount();
    //add each field as the array index
    for (i=0; i<nCount; i++)
    {
      lpFieldInfo = _INTERNAL()->sQuery.aFieldsList.GetElementAt(i);
      nRet = lpFieldInfo->PushThis(lpCtx);
      if (nRet < 0)
        return nRet;
      DukTape::duk_put_prop_index(lpCtx, -2, (DukTape::duk_uarridx_t)i);
    }
  }
  else
  {
    DukTape::duk_push_null(lpCtx);
  }
  return 1;
}

HRESULT CJsMySqlPlugin::_TransactionStart()
{
  HRESULT hRes;

  if (_INTERNAL()->nTransactionCounter > 1024)
    return E_FAIL;
  //CLOSE CURRENT QUERY
  hRes = _INTERNAL()->ExecuteQuery("START TRANSACTION;", 18);
  if (SUCCEEDED(hRes))
    (_INTERNAL()->nTransactionCounter)++;
  //done
  return hRes;
}

HRESULT CJsMySqlPlugin::_TransactionCommit()
{
  HRESULT hRes;

  if (_INTERNAL()->nTransactionCounter == 0)
    return E_FAIL;
  //CLOSE CURRENT QUERY
  hRes = _INTERNAL()->ExecuteQuery("COMMIT;", 7);
  if (SUCCEEDED(hRes))
    (_INTERNAL()->nTransactionCounter)--;
  //done
  return hRes;
}

HRESULT CJsMySqlPlugin::_TransactionRollback()
{
  HRESULT hRes;

  if (_INTERNAL()->nTransactionCounter == 0)
    return E_FAIL;
  //CLOSE CURRENT QUERY
  hRes = _INTERNAL()->ExecuteQuery("ROLLBACK;", 9);
  if (SUCCEEDED(hRes))
    (_INTERNAL()->nTransactionCounter)--;
  //done
  return hRes;
}

DukTape::duk_ret_t CJsMySqlPlugin::ReturnErrorFromHResult(__in DukTape::duk_context *lpCtx, __in HRESULT hRes)
{
  if (hRes == E_INVALIDARG)
  {
    MX_JS_THROW_ERROR(lpCtx, DUK_ERR_API_ERROR, "**%08X", E_INVALIDARG);
    return 0;
  }
  if (hRes == E_OUTOFMEMORY)
  {
    MX_JS_THROW_ERROR(lpCtx, DUK_ERR_ALLOC_ERROR, "**%08X", E_OUTOFMEMORY);
    return 0;
  }
  //----
  hLastErr = hRes;
  nLastDbErr = 0;
  szLastDbErrA[0] = szLastSqlStateA[0] = 0;
  DukTape::duk_push_boolean(lpCtx, (SUCCEEDED(hRes)) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::ReturnErrorFromHResultAndDbErr(__in DukTape::duk_context *lpCtx, __in HRESULT hRes)
{
  if (hRes == E_INVALIDARG)
  {
    MX_JS_THROW_ERROR(lpCtx, DUK_ERR_API_ERROR, "**%08X", E_INVALIDARG);
    return 0;
  }
  if (hRes == E_OUTOFMEMORY)
  {
    MX_JS_THROW_ERROR(lpCtx, DUK_ERR_ALLOC_ERROR, "**%08X", E_OUTOFMEMORY);
    return 0;
  }
  //----
  hLastErr = hRes;
  if (SUCCEEDED(hRes))
  {
    nLastDbErr = 0;
    szLastDbErrA[0] = szLastSqlStateA[0] = 0;
  }
  else
  {
    nLastDbErr = (int)_CALLAPI(mysql_errno)(_DB());
    if (nLastDbErr == 0)
      nLastDbErr = ER_UNKNOWN_ERROR;
    strncpy_s(szLastDbErrA, _countof(szLastDbErrA), _CALLAPI(mysql_error)(_DB()), _TRUNCATE);
    strncpy_s(szLastSqlStateA, _countof(szLastSqlStateA), _CALLAPI(mysql_sqlstate)(_DB()), _TRUNCATE);
  }
  DukTape::duk_push_boolean(lpCtx, (SUCCEEDED(hRes)) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlPlugin::ReturnErrorFromLastDbErr(__in DukTape::duk_context *lpCtx)
{
  int err;

  err = (int)_CALLAPI(mysql_errno)(_DB());
  if (err == 0)
    err = ER_UNKNOWN_ERROR;
  return ReturnErrorFromHResultAndDbErr(lpCtx, Internals::CJsMySqlPluginHelpers::HResultFromMySqlErr(err));
}

} //namespace MX
