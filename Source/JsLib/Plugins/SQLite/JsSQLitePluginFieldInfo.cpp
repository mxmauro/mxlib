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
#include "JsSQLitePluginCommon.h"

//-----------------------------------------------------------

namespace MX {

namespace Internals {

CJsSQLiteFieldInfo::CJsSQLiteFieldInfo(_In_ DukTape::duk_context *lpCtx) : CJsObjectBase(lpCtx)
{
  nType = 0;
  nRealType = nCurrType = SQLITE_NULL;
  return;
}

CJsSQLiteFieldInfo::~CJsSQLiteFieldInfo()
{
  return;
}

DukTape::duk_ret_t CJsSQLiteFieldInfo::getName()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrNameA, cStrNameA.GetLength());
  return 1;
}

DukTape::duk_ret_t CJsSQLiteFieldInfo::getOriginalName()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (cStrOriginalNameA.IsEmpty() == FALSE)
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrOriginalNameA, cStrOriginalNameA.GetLength());
  else
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrNameA, cStrNameA.GetLength());
  return 1;
}

DukTape::duk_ret_t CJsSQLiteFieldInfo::getTable()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTableA, cStrTableA.GetLength());
  return 1;
}

DukTape::duk_ret_t CJsSQLiteFieldInfo::getOriginalTable()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTableA, cStrTableA.GetLength());
  return 1;
}

DukTape::duk_ret_t CJsSQLiteFieldInfo::getDatabase()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrDatabaseA, cStrDatabaseA.GetLength());
  return 1;
}
/*
DukTape::duk_ret_t CJsSQLiteFieldInfo::getCanBeNull()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & NOT_NULL_FLAG) ? 0 : 1);
  return 1;
}

DukTape::duk_ret_t CJsSQLiteFieldInfo::getIsPrimaryKey()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & PRI_KEY_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsSQLiteFieldInfo::getIsUniqueKey()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & UNIQUE_KEY_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsSQLiteFieldInfo::getIsKey()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & (PRI_KEY_FLAG | UNIQUE_KEY_FLAG | MULTIPLE_KEY_FLAG)) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsSQLiteFieldInfo::getIsUnsigned()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & UNSIGNED_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsSQLiteFieldInfo::getIsZeroFill()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & ZEROFILL_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsSQLiteFieldInfo::getIsBinary()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & BINARY_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsSQLiteFieldInfo::getIsAutoIncrement()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & AUTO_INCREMENT_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsSQLiteFieldInfo::getIsEnum()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & ENUM_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsSQLiteFieldInfo::getIsSet()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & SET_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsSQLiteFieldInfo::getHasDefault()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & NO_DEFAULT_VALUE_FLAG) ? 0 : 1);
  return 1;
}

DukTape::duk_ret_t CJsSQLiteFieldInfo::getIsNumeric()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & NUM_FLAG) ? 1 : 0);
  return 1;
}
*/
DukTape::duk_ret_t CJsSQLiteFieldInfo::getType()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)nRealType);
  return 1;
}

} //namespace Internals

} //namespace MX
