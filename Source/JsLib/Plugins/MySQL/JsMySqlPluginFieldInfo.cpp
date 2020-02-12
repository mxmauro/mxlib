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
#include "JsMySqlPluginCommon.h"

//-----------------------------------------------------------

namespace MX {

namespace Internals {

CJsMySqlFieldInfo::CJsMySqlFieldInfo() : CJsObjectBase(), CNonCopyableObj()
{
  nDecimalsCount = nCharSet = nFlags = 0;
  nType = MYSQL_TYPE_NULL;
  return;
}

CJsMySqlFieldInfo::~CJsMySqlFieldInfo()
{
  return;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getName(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrNameA, cStrNameA.GetLength());
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getOriginalName(_In_ DukTape::duk_context *lpCtx)
{
  if (cStrOriginalNameA.IsEmpty() == FALSE)
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrOriginalNameA, cStrOriginalNameA.GetLength());
  else
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrNameA, cStrNameA.GetLength());
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getTable(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTableA, cStrTableA.GetLength());
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getOriginalTable(_In_ DukTape::duk_context *lpCtx)
{
  if (cStrOriginalTableA.IsEmpty() == FALSE)
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrOriginalTableA, cStrOriginalTableA.GetLength());
  else
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTableA, cStrTableA.GetLength());
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getDatabase(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrDatabaseA, cStrDatabaseA.GetLength());
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getDecimalsCount(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_uint(lpCtx, nDecimalsCount);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getCharSet(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_uint(lpCtx, nCharSet);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getCanBeNull(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_boolean(lpCtx, (nFlags & NOT_NULL_FLAG) ? 0 : 1);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsPrimaryKey(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_boolean(lpCtx, (nFlags & PRI_KEY_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsUniqueKey(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_boolean(lpCtx, (nFlags & UNIQUE_KEY_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsKey(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_boolean(lpCtx, (nFlags & (PRI_KEY_FLAG | UNIQUE_KEY_FLAG | MULTIPLE_KEY_FLAG)) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsUnsigned(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_boolean(lpCtx, (nFlags & UNSIGNED_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsZeroFill(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_boolean(lpCtx, (nFlags & ZEROFILL_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsBinary(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_boolean(lpCtx, (nFlags & BINARY_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsAutoIncrement(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_boolean(lpCtx, (nFlags & AUTO_INCREMENT_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsEnum(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_boolean(lpCtx, (nFlags & ENUM_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsSet(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_boolean(lpCtx, (nFlags & SET_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getHasDefault(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_boolean(lpCtx, (nFlags & NO_DEFAULT_VALUE_FLAG) ? 0 : 1);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsNumeric(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_boolean(lpCtx, (nFlags & NUM_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getType(_In_ DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)nType);
  return 1;
}

} //namespace Internals

} //namespace MX
