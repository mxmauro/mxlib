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

//-----------------------------------------------------------

namespace MX {

namespace Internals {

CJsMySqlPluginHelpers::CJsMySqlPluginHelpers() : CBaseMemObj()
{
  lpDB = NULL;
  nServerUsingNoBackslashEscapes = -1;
  sQuery.lpResultSet = NULL;
  sQuery.nAffectedRows = 0ui64;
  sQuery.nLastInsertId = 0ui64;
  sQuery.nFieldsCount = 0;
  return;
}

CJsMySqlPluginHelpers::~CJsMySqlPluginHelpers()
{
  return;
}

HRESULT CJsMySqlPluginHelpers::ExecuteQuery(__in LPCSTR szQueryA, __in SIZE_T nQueryLegth,
                                            __out_opt MYSQL_RES **lplpResult)
{
  int nRetryCount, err;
  MYSQL_RES *lpLocalResult;

  //close previous query if any
  QueryClose();
  //validate arguments
  err = 0;
  if (lplpResult == NULL)
    lplpResult = &lpLocalResult;
  *lplpResult = NULL;
  if (nQueryLegth == (SIZE_T)-1)
    nQueryLegth = StrLenA(szQueryA);
  if (nQueryLegth > 0xFFFFFFFF)
    nQueryLegth = 0xFFFFFFFF;
  while (nQueryLegth > 0 && szQueryA[nQueryLegth-1] == ';')
    nQueryLegth--;
  //execute query
  nRetryCount = 5;
  while (nRetryCount > 0)
  {
    if (API::fn_mysql_real_query(lpDB, szQueryA, (ULONG)nQueryLegth) == 0)
    {
      *lplpResult = API::fn_mysql_use_result(lpDB);
      if (lplpResult == &lpLocalResult)
        CloseResultSet(lpLocalResult);
      return S_OK;
    }
    err = API::fn_mysql_errno(lpDB);
    if (IsConnectionLostError(err) != FALSE)
      return MX_E_BrokenPipe;
    if (err == 0)
      err = ER_UNKNOWN_ERROR; //fake error
    if (nRetryCount == 1 || err != ER_CANT_LOCK)
      break;
    ::Sleep(50);
    nRetryCount--;
  }
  return HResultFromMySqlErr(err);
}

VOID CJsMySqlPluginHelpers::QueryClose(__in_opt BOOL bFlushData)
{
  if (sQuery.lpResultSet != NULL)
  {
    CloseResultSet(sQuery.lpResultSet, bFlushData);
    sQuery.lpResultSet = NULL;
  }
  sQuery.aFieldsList.RemoveAllElements();
  sQuery.nFieldsCount = 0;
  return;
}

VOID CJsMySqlPluginHelpers::CloseResultSet(__in MYSQL_RES *lpResult, __in_opt BOOL bFlushData)
{
  MYSQL_ROW lpRow;

  if (lpResult != NULL)
  {
    do
    {
      lpRow = API::fn_mysql_fetch_row(lpResult);
    }
    while (lpRow != NULL);
    API::fn_mysql_free_result(lpResult);
  }
  return;
}

BOOL CJsMySqlPluginHelpers::BuildFieldInfoArray(__in DukTape::duk_context *lpCtx)
{
  TAutoRefCounted<CFieldInfo> cFieldInfo;
  SIZE_T i;
  MYSQL_FIELD *lpFields;

  sQuery.aFieldsList.RemoveAllElements();
  if (sQuery.lpResultSet == NULL)
    return TRUE;
  lpFields = API::fn_mysql_fetch_fields(sQuery.lpResultSet);
  if (sQuery.nFieldsCount == 0 || lpFields == NULL)
    return TRUE;
  //build fields
  for (i=0; i<sQuery.nFieldsCount; i++)
  {
    cFieldInfo.Attach(MX_DEBUG_NEW CFieldInfo(lpCtx));
    if (!cFieldInfo)
      return FALSE;
    //name
    if (cFieldInfo->cStrNameA.Copy(lpFields[i].name) == FALSE)
      return FALSE;
    //original name
    if (lpFields[i].org_name != NULL)
    {
      if (cFieldInfo->cStrOriginalNameA.Copy(lpFields[i].org_name) == FALSE)
        return FALSE;
    }
    //table
    if (lpFields[i].table != NULL)
    {
      if (cFieldInfo->cStrTableA.Copy(lpFields[i].table) == FALSE)
        return FALSE;
    }
    //original table
    if (lpFields[i].table != NULL)
    {
      if (cFieldInfo->cStrOriginalTableA.Copy(lpFields[i].org_table) == FALSE)
        return FALSE;
    }
    //database
    if (lpFields[i].db != NULL)
    {
      if (cFieldInfo->cStrDatabaseA.Copy(lpFields[i].db) == FALSE)
        return FALSE;
    }
    //length, max length, charset, decimals count
    cFieldInfo->nLength = lpFields[i].length;
    cFieldInfo->nMaxLength = lpFields[i].max_length;
    cFieldInfo->nDecimalsCount = lpFields[i].decimals;
    cFieldInfo->nCharSet = lpFields[i].charsetnr;
    cFieldInfo->nFlags = lpFields[i].flags;
    cFieldInfo->nType =lpFields[i].type;
    //add to list
    if (sQuery.aFieldsList.AddElement(cFieldInfo.Get()) == FALSE)
      return FALSE;
    cFieldInfo.Detach();
  }
  //done
  return TRUE;
}

BOOL CJsMySqlPluginHelpers::IsConnectionLostError(__in int nError)
{
  return (nError == CR_TCP_CONNECTION || nError == CR_SERVER_LOST ||
          nError == CR_SERVER_GONE_ERROR || nError == CR_SERVER_LOST_EXTENDED) ? TRUE : FALSE;
}

HRESULT CJsMySqlPluginHelpers::HResultFromMySqlErr(__in int nError)
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

} //namespace Internals

} //namespace MX
