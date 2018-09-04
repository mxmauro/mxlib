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

CJsMySqlFieldInfo::CJsMySqlFieldInfo(_In_ DukTape::duk_context *lpCtx) : CJsObjectBase(lpCtx)
{
  nDecimalsCount = nCharSet = nFlags = 0;
  nType = MYSQL_TYPE_NULL;
  return;
}

CJsMySqlFieldInfo::~CJsMySqlFieldInfo()
{
  return;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getName()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrNameA, cStrNameA.GetLength());
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getOriginalName()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (cStrOriginalNameA.IsEmpty() == FALSE)
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrOriginalNameA, cStrOriginalNameA.GetLength());
  else
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrNameA, cStrNameA.GetLength());
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getTable()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTableA, cStrTableA.GetLength());
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getOriginalTable()
{
  DukTape::duk_context *lpCtx = GetContext();

  if (cStrOriginalTableA.IsEmpty() == FALSE)
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrOriginalTableA, cStrOriginalTableA.GetLength());
  else
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTableA, cStrTableA.GetLength());
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getDatabase()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrDatabaseA, cStrDatabaseA.GetLength());
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getDecimalsCount()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_uint(lpCtx, nDecimalsCount);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getCharSet()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_uint(lpCtx, nCharSet);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getCanBeNull()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & NOT_NULL_FLAG) ? 0 : 1);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsPrimaryKey()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & PRI_KEY_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsUniqueKey()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & UNIQUE_KEY_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsKey()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & (PRI_KEY_FLAG | UNIQUE_KEY_FLAG | MULTIPLE_KEY_FLAG)) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsUnsigned()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & UNSIGNED_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsZeroFill()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & ZEROFILL_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsBinary()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & BINARY_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsAutoIncrement()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & AUTO_INCREMENT_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsEnum()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & ENUM_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsSet()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & SET_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getHasDefault()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & NO_DEFAULT_VALUE_FLAG) ? 0 : 1);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getIsNumeric()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_boolean(lpCtx, (nFlags & NUM_FLAG) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsMySqlFieldInfo::getType()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)nType);
  return 1;
}

} //namespace Internals

} //namespace MX
