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
#include "..\..\..\..\Include\JsLib\Plugins\JsMySqlPlugin.h"

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
  ::MxMemCopy(szSqlStateA, sA, nLen);
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
  ::MxMemCopy(szSqlStateA, obj.szSqlStateA, sizeof(szSqlStateA));
  //----
  return *this;
}

} //namespace MX
