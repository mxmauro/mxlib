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
#include "JsHttpServerCommon.h"

//-----------------------------------------------------------

namespace MX {

namespace Internals {

CRawBodyJsObject::CRawBodyJsObject() : CJsObjectBase(), CNonCopyableObj()
{
  lpBody = NULL;
  return;
}

VOID CRawBodyJsObject::Initialize(_In_ CHttpBodyParserDefault *_lpBody)
{
  lpBody = _lpBody;
  return;
}

DukTape::duk_ret_t CRawBodyJsObject::getSize(_In_ DukTape::duk_context *lpCtx)
{
  ULONGLONG nSize;

  nSize = lpBody->GetSize();
  if (nSize > 0x1FFFFFFFFFFFFFui64)
    nSize = 0x1FFFFFFFFFFFFFui64;
  DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)nSize);
  return 1;
}

DukTape::duk_ret_t CRawBodyJsObject::ToString(_In_ DukTape::duk_context *lpCtx)
{
  CStringA cStrTempA;
  HRESULT hRes;

  hRes = lpBody->ToString(cStrTempA);
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  DukTape::duk_push_string(lpCtx, (LPSTR)cStrTempA);
  return 1;
}

DukTape::duk_ret_t CRawBodyJsObject::Read(_In_ DukTape::duk_context *lpCtx)
{
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
