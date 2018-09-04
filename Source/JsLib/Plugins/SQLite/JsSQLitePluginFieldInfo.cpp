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
