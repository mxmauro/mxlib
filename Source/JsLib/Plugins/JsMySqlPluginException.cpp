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

CJsMySqlError::CJsMySqlError(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nStackIndex) :
               nDbError(0), lpStrSqlStateA(NULL), CJsWindowsError(lpCtx, nStackIndex)
{
  return;
}

CJsMySqlError::CJsMySqlError(__in const CJsMySqlError &obj) : CJsWindowsError(NULL, 0)
{
  *this = obj;
  return;
}

CJsMySqlError::~CJsMySqlError()
{
  Cleanup();
  return;
}

CJsMySqlError& CJsMySqlError::operator=(__in const CJsMySqlError &obj)
{
  Cleanup();
  CJsWindowsError::operator=(obj);
  //----
  lpStrSqlStateA = obj.lpStrSqlStateA;
  if (obj.lpStrSqlStateA != NULL)
    obj.lpStrSqlStateA->AddRef();
  //----
  return *this;
}

VOID CJsMySqlError::Cleanup()
{
  if (lpStrSqlStateA != NULL)
  {
    lpStrSqlStateA->Release();
    lpStrSqlStateA = NULL;
  }
  return;
}

} //namespace MX
