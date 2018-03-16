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

CJsMySqlError::CJsMySqlError(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nStackIndex) :
               CJsWindowsError(lpCtx, nStackIndex)
{
  LPCSTR sA;
  SIZE_T nLen;

  nDbError = 0;
  szSqlStateA[0] = 0;

  DukTape::duk_get_prop_string(lpCtx, nStackIndex, "dbError");
  nDbError = (int)DukTape::duk_get_int(lpCtx, -1);
  DukTape::duk_pop(lpCtx);

  DukTape::duk_get_prop_string(lpCtx, nStackIndex, "sqlState");
  sA = (DukTape::duk_is_undefined(lpCtx, -1) == false) ? DukTape::duk_safe_to_string(lpCtx, -1) : "000000";
  nLen = MX::StrLenA(sA);
  if (nLen >= MX_ARRAYLEN(szSqlStateA))
    nLen = MX_ARRAYLEN(szSqlStateA) - 1;
  MX::MemCopy(szSqlStateA, sA, nLen);
  szSqlStateA[nLen]  = 0;
  DukTape::duk_pop(lpCtx);
  return;
}

CJsMySqlError::CJsMySqlError(_In_ const CJsMySqlError &obj) : CJsWindowsError(obj.GetHResult())
{
  *this = obj;
  return;
}

CJsMySqlError::~CJsMySqlError()
{
  return;
}

CJsMySqlError& CJsMySqlError::operator=(_In_ const CJsMySqlError &obj)
{
  CJsWindowsError::operator=(obj);
  //----
  nDbError = obj.nDbError;
  MX::MemCopy(szSqlStateA, obj.szSqlStateA, sizeof(szSqlStateA));
  //----
  return *this;
}

} //namespace MX
