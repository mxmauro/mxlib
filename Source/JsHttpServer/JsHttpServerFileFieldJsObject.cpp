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

CFileFieldJsObject::CFileFieldJsObject(_In_ DukTape::duk_context *lpCtx) : CJsObjectBase(lpCtx)
{
  lpFileField = NULL;
  nFilePos = 0ui64;
  return;
}

VOID CFileFieldJsObject::Initialize(_In_ CHttpBodyParserFormBase::CFileField *_lpFileField)
{
  lpFileField = _lpFileField;
  return;
}

DukTape::duk_ret_t CFileFieldJsObject::getType()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_string(lpCtx, lpFileField->GetType());
  return 1;
}

DukTape::duk_ret_t CFileFieldJsObject::getFileName()
{
  DukTape::duk_context *lpCtx = GetContext();
  CStringA cStrTempA;
  HRESULT hRes;

  hRes = Utf8_Encode(cStrTempA, lpFileField->GetFileName());
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  DukTape::duk_push_string(lpCtx, (LPCSTR)cStrTempA);
  return 1;
}

DukTape::duk_ret_t CFileFieldJsObject::getFileSize()
{
  DukTape::duk_context *lpCtx = GetContext();

  DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)(lpFileField->GetSize()));
  return 1;
}

DukTape::duk_ret_t CFileFieldJsObject::SeekFile()
{
  DukTape::duk_context *lpCtx = GetContext();
  DukTape::duk_double_t nPos;

  nPos = DukTape::duk_require_number(lpCtx, 0);
  if (nPos > 0.00001)
    nFilePos = (ULONGLONG)nPos;
  else
    nFilePos = 0ui64;
  return 0;
}

DukTape::duk_ret_t CFileFieldJsObject::ReadFile()
{
  DukTape::duk_context *lpCtx = GetContext();
  DukTape::duk_uint_t nToRead;
  DukTape::duk_idx_t nParamsCount;
  DukTape::duk_bool_t bAsBuffer;
  LPVOID lpBuf;
  SIZE_T nReaded;
  HRESULT hRes;

  nParamsCount = DukTape::duk_get_top(lpCtx);
  if (nParamsCount < 1 || nParamsCount > 2)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  //parse parameters
  nToRead = DukTape::duk_require_uint(lpCtx, 0);
  bAsBuffer = false;
  if (nParamsCount > 1)
  {
    if (DukTape::duk_is_boolean(lpCtx, 1) != 0)
      bAsBuffer = DukTape::duk_require_boolean(lpCtx, 1);
    else
      bAsBuffer = ((int)(DukTape::duk_require_number(lpCtx, 1)) != 0) ? true : false;
  }
  //do file read
  if (nToRead > 0)
  {
    lpBuf = DukTape::duk_push_dynamic_buffer(lpCtx, nToRead);
    hRes = lpFileField->Read(lpBuf, nFilePos, nToRead, &nReaded);
    if (FAILED(hRes))
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
    if (nReaded > 0)
    {
      DukTape::duk_resize_buffer(lpCtx, -1, (DukTape::duk_size_t)nReaded);
      if (bAsBuffer == false)
      {
        DukTape::duk_push_buffer_object(lpCtx, -1, 0, (DukTape::duk_size_t)nReaded, DUK_BUFOBJ_UINT8ARRAY);
        DukTape::duk_remove(lpCtx, -2);
      }
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
