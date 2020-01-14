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

class CFileFieldJsObject : public CJsObjectBase, public CNonCopyableObj
{
public:
  CFileFieldJsObject(_In_ DukTape::duk_context *lpCtx);

  VOID Initialize(_In_ CHttpBodyParserFormBase::CFileField *lpFileField);

  MX_JS_DECLARE(CFileFieldJsObject, "FormFileField")

  MX_JS_BEGIN_MAP(CFileFieldJsObject)
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
  ULONGLONG nFilePos;
};

//-----------------------------------------------------------

class CRawBodyJsObject : public CJsObjectBase, public CNonCopyableObj
{
public:
  CRawBodyJsObject(_In_ DukTape::duk_context *lpCtx);

  VOID Initialize(_In_ CHttpBodyParserDefault *lpBody);

  MX_JS_DECLARE(CRawBodyJsObject, "RawBody")

  MX_JS_BEGIN_MAP(CRawBodyJsObject)
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

HRESULT AddResponseMethods(_In_ CJavascriptVM &cJvm);
HRESULT AddHelpersMethods(_In_ CJavascriptVM &cJvm);

} //namespace JsHttpServer

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JS_HTTP_SERVER_COMMON_H
