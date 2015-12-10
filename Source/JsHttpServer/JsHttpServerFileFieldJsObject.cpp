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

CFileFieldJsObject::CFileFieldJsObject(__in DukTape::duk_context *lpCtx) : CJsObjectBase(lpCtx)
{
  lpFileField = NULL;
  nFileSize = 0;
  return;
}

VOID CFileFieldJsObject::Initialize(__in CHttpBodyParserFormBase::CFileField *_lpFileField)
{
  HANDLE hFile;
  DWORD dw, dwHigh;

  lpFileField = _lpFileField;
  hFile = lpFileField->GetFileHandle();
  dw = ::GetFileSize(hFile, &dwHigh);
  if (dwHigh <= 0x1FFFFF)
    nFileSize = ((ULONGLONG)dwHigh << 32) | (ULONGLONG)dw;
  else
    nFileSize = 0x1FFFFFFFFFFFFF;
  return;
}

DukTape::duk_ret_t CFileFieldJsObject::getType(__in DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_string(lpCtx, lpFileField->GetType());
  return 1;
}

DukTape::duk_ret_t CFileFieldJsObject::getFileName(__in DukTape::duk_context *lpCtx)
{
  CStringA cStrTempA;

  if (FAILED(Utf8_Encode(cStrTempA, lpFileField->GetFileName())))
    cStrTempA.Empty();
  DukTape::duk_push_string(lpCtx, (LPCSTR)cStrTempA);
  return 1;
}

DukTape::duk_ret_t CFileFieldJsObject::getFileSize(__in DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)nFileSize);
  return 1;
}

DukTape::duk_ret_t CFileFieldJsObject::SeekFile(__in DukTape::duk_context *lpCtx)
{
  DukTape::duk_double_t nPos;
  union {
    ULONGLONG nFilePos;
    DWORD dw[2];
  };

  nPos = DukTape::duk_require_number(lpCtx, 0);
  if (nPos > 0.00001)
  {
    nFilePos = (ULONGLONG)nPos;
    if (nFilePos > nFileSize)
      nFilePos = nFileSize;
  }
  else
  {
    nFilePos = 0;
  }
  ::SetFilePointer(lpFileField->GetFileHandle(), (LONG)dw[0], (PLONG)&dw[1], SEEK_SET);
  return 0;
}

DukTape::duk_ret_t CFileFieldJsObject::ReadFile(__in DukTape::duk_context *lpCtx)
{
  DukTape::duk_uint_t nToRead;
  LPVOID lpBuf;
  DWORD dwReaded;

  nToRead = DukTape::duk_require_uint(lpCtx, 0);
  if (nToRead > 0)
  {
    lpBuf = DukTape::duk_push_dynamic_buffer(lpCtx, nToRead);
    if (::ReadFile(lpFileField->GetFileHandle(), lpBuf, (DWORD)nToRead, &dwReaded, NULL) != FALSE && dwReaded > 0)
    {
      DukTape::duk_resize_buffer(lpCtx, -1, (DukTape::duk_size_t)dwReaded);
      DukTape::duk_push_buffer_object(lpCtx, -1, 0, (DukTape::duk_size_t)dwReaded, DUK_BUFOBJ_UINT8ARRAY);
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
