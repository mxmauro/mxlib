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
#include "JsHttpServerCommon.h"

//-----------------------------------------------------------

namespace MX {

namespace Internals {

CRawBodyJsObject::CRawBodyJsObject(__in DukTape::duk_context *lpCtx) : CJsObjectBase(lpCtx)
{
  lpBody = NULL;
  return;
}

VOID CRawBodyJsObject::Initialize(__in CHttpBodyParserDefault *_lpBody)
{
  lpBody = _lpBody;
  return;
}

DukTape::duk_ret_t CRawBodyJsObject::getSize()
{
  DukTape::duk_context *lpCtx = GetContext();
  ULONGLONG nSize;

  nSize = lpBody->GetSize();
  if (nSize > 0x1FFFFFFFFFFFFFui64)
    nSize = 0x1FFFFFFFFFFFFFui64;
  DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)nSize);
  return 1;
}

DukTape::duk_ret_t CRawBodyJsObject::ToString()
{
  DukTape::duk_context *lpCtx = GetContext();
  CStringA cStrTempA;
  HRESULT hRes;

  hRes = lpBody->ToString(cStrTempA);
  __THROW_ERROR_ON_FAILED_HRESULT(hRes);
  DukTape::duk_push_string(lpCtx, (LPSTR)cStrTempA);
  return 1;
}

DukTape::duk_ret_t CRawBodyJsObject::Read()
{
  DukTape::duk_context *lpCtx = GetContext();
  DukTape::duk_uint_t nOffset, nToRead;
  LPVOID lpBuf;
  SIZE_T nReaded;
  ULONGLONG nSize;

  nOffset = DukTape::duk_require_uint(lpCtx, 0);
  nToRead = DukTape::duk_require_uint(lpCtx, 1);
  nSize = lpBody->GetSize();
  if ((ULONGLONG)nToRead > nSize)
    nToRead = (DukTape::duk_uint_t)nSize;
  if ((ULONGLONG)nOffset > nSize)
  {
    nOffset = nToRead = 0;
  }
  else if ((ULONGLONG)nOffset > nSize - (ULONGLONG)nToRead)
  {
    nToRead = (DukTape::duk_uint_t)(nSize - (ULONGLONG)nOffset);
  }
  if (nToRead > 0)
  {
    lpBuf = DukTape::duk_push_dynamic_buffer(lpCtx, nToRead);
    if (SUCCEEDED(lpBody->Read(lpBuf, (ULONGLONG)nOffset, (SIZE_T)nToRead, &nReaded)) && nReaded > 0)
    {
      DukTape::duk_resize_buffer(lpCtx, -1, (DukTape::duk_size_t)nReaded);
      DukTape::duk_push_buffer_object(lpCtx, -1, 0, (DukTape::duk_size_t)nReaded, DUK_BUFOBJ_UINT8ARRAY);
      DukTape::duk_remove(lpCtx, -2);
    }
    else
    {
      DukTape::duk_pop(lpCtx);
      DukTape::duk_push_null(lpCtx);
    }
  }
  else
  {
    DukTape::duk_push_null(lpCtx);
  }
  return 1;
}

} //namespace Internals

} //namespace MX
