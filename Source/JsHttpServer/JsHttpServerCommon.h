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
#ifndef _MX_JS_HTTP_SERVER_COMMON_H
#define _MX_JS_HTTP_SERVER_COMMON_H

#include "..\..\Include\JsHttpServer\JsHttpServer.h"
#include "..\..\Include\Strings\Strings.h"
#include "..\..\Include\Strings\Utf8.h"

//-----------------------------------------------------------

#define __EXIT_ON_ERROR(_hres)  if (FAILED(_hres)) return _hres

//-----------------------------------------------------------

#define INTERNAL_REQUEST_PROPERTY "\xff""\xff""jsHttpServerRequest"

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CFileFieldJsObject : public CJsObjectBase
{
  MX_DISABLE_COPY_CONSTRUCTOR(CFileFieldJsObject);
public:
  CFileFieldJsObject(__in DukTape::duk_context *lpCtx);

  VOID Initialize(__in CHttpBodyParserFormBase::CFileField *lpFileField);

  MX_JS_BEGIN_MAP(CFileFieldJsObject, "FormFileField", 0)
    MX_JS_MAP_PROPERTY("type", &CFileFieldJsObject::getType, NULL, TRUE)
    MX_JS_MAP_PROPERTY("filename", &CFileFieldJsObject::getFileName, NULL, TRUE)
    MX_JS_MAP_PROPERTY("filesize", &CFileFieldJsObject::getFileSize, NULL, TRUE)
    MX_JS_MAP_METHOD("Seek", &CFileFieldJsObject::SeekFile, 1)
    MX_JS_MAP_METHOD("Read", &CFileFieldJsObject::ReadFile, MX_JS_VARARGS)
  MX_JS_END_MAP()

private:
  DukTape::duk_ret_t getType();
  DukTape::duk_ret_t getFileName();
  DukTape::duk_ret_t getFileSize();
  DukTape::duk_ret_t SeekFile();
  DukTape::duk_ret_t ReadFile();

private:
  CHttpBodyParserFormBase::CFileField *lpFileField;
  ULONGLONG nFileSize;
};

//-----------------------------------------------------------

class CRawBodyJsObject : public CJsObjectBase
{
  MX_DISABLE_COPY_CONSTRUCTOR(CRawBodyJsObject);
public:
  CRawBodyJsObject(__in DukTape::duk_context *lpCtx);

  VOID Initialize(__in CHttpBodyParserDefault *lpBody);

  MX_JS_BEGIN_MAP(CRawBodyJsObject, "RawBody", 0)
    MX_JS_MAP_PROPERTY("size", &CRawBodyJsObject::getSize, NULL, TRUE)
    MX_JS_MAP_METHOD("toString", &CRawBodyJsObject::ToString, 0)
    MX_JS_MAP_METHOD("Read", &CRawBodyJsObject::Read, 2)
  MX_JS_END_MAP()

private:
  DukTape::duk_ret_t getSize();
  DukTape::duk_ret_t ToString();
  DukTape::duk_ret_t Read();

private:
  CHttpBodyParserDefault *lpBody;
};

//-----------------------------------------------------------

namespace JsHttpServer {

HRESULT AddResponseMethods(__in CJavascriptVM &cJvm, __in MX::CHttpServer::CRequest *lpRequest);
HRESULT AddHelpersMethods(__in CJavascriptVM &cJvm, __in MX::CHttpServer::CRequest *lpRequest);

} //namespace JsHttpServer

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JS_HTTP_SERVER_COMMON_H
