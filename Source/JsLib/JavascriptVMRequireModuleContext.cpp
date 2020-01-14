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
#include "JavascriptVMCommon.h"

//-----------------------------------------------------------

namespace MX {

CJavascriptVM::CRequireModuleContext::CRequireModuleContext(_In_ DukTape::duk_context *_lpCtx, _In_z_ LPCWSTR _szIdW,
                                                            _In_ DukTape::duk_idx_t _nModuleObjectIndex,
                                                            _In_ DukTape::duk_idx_t _nExportsObjectIndex) :
                                                            CBaseMemObj()
{
  lpCtx = _lpCtx;
  szIdW = _szIdW;
  nModuleObjectIndex = _nModuleObjectIndex;
  nExportsObjectIndex = _nExportsObjectIndex;
  return;
}

HRESULT CJavascriptVM::CRequireModuleContext::RequireModule(_In_z_ LPCWSTR szModuleIdW)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  HRESULT hRes;

  if (szModuleIdW == NULL)
    return E_POINTER;
  while (*szModuleIdW == L'/')
    szModuleIdW++;
  if (*szModuleIdW == 0)
    return E_INVALIDARG;
  if (lpCtx == NULL)
    return E_FAIL;

  hRes = lpJVM->RunNativeProtectedAndGetError(0, 1, [szModuleIdW, this](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
    CStringA cStrTempA;

    lpJVM->PushString(szModuleIdW);
    DukTape::duk_push_global_object(lpCtx);
    DukTape::duk_get_prop_string(lpCtx, -1, "require");
    DukTape::duk_remove(lpCtx, -2);
    DukTape::duk_call(lpCtx, 1);
    return;
  });
  //done
  return hRes;
}

HRESULT CJavascriptVM::CRequireModuleContext::AddNativeFunction(_In_z_ LPCSTR szFuncNameA,
                                                                _In_ OnNativeFunctionCallback cNativeFunctionCallback,
                                                                _In_ int nArgsCount)
{
  if (szFuncNameA == NULL || !cNativeFunctionCallback)
    return E_POINTER;
  if (*szFuncNameA == 0 || nArgsCount < -1)
    return E_INVALIDARG;
  if (lpCtx == NULL)
    return E_FAIL;
  return Internals::JsLib::AddNativeFunctionCommon(lpCtx, NULL, nExportsObjectIndex, szFuncNameA,
                                                   cNativeFunctionCallback, nArgsCount);
}

HRESULT CJavascriptVM::CRequireModuleContext::AddProperty(_In_z_ LPCSTR szPropertyNameA,
                                                          _In_opt_ BOOL bInitialValueOnStack,
                                                          _In_opt_ int nFlags)
{
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, nExportsObjectIndex, szPropertyNameA, bInitialValueOnStack,
                                             nFlags, NullCallback(), NullCallback());
}

HRESULT CJavascriptVM::CRequireModuleContext::AddStringProperty(_In_z_ LPCSTR szPropertyNameA, _In_z_ LPCSTR szValueA,
                                                                _In_opt_ int nFlags)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  hRes = lpJVM->RunNativeProtectedAndGetError(0, 1, [szValueA](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    if (szValueA != NULL)
      DukTape::duk_push_string(lpCtx, szValueA);
    else
      DukTape::duk_push_null(lpCtx);
    return;
  });
  if (FAILED(hRes))
    return hRes;
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, nExportsObjectIndex, szPropertyNameA, TRUE, nFlags,
                                             NullCallback(), NullCallback());
}

HRESULT CJavascriptVM::CRequireModuleContext::AddBooleanProperty(_In_z_ LPCSTR szPropertyNameA, _In_ BOOL bValue,
                                                                 _In_opt_ int nFlags)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  hRes = lpJVM->RunNativeProtectedAndGetError(0, 1, [bValue](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_boolean(lpCtx, (bValue != FALSE) ? true : false);
    return;
  });
  if (FAILED(hRes))
    return hRes;
  //done
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, nExportsObjectIndex, szPropertyNameA, TRUE, nFlags,
                                             NullCallback(), NullCallback());
}

HRESULT CJavascriptVM::CRequireModuleContext::AddNumericProperty(_In_z_ LPCSTR szPropertyNameA, _In_ double nValue,
                                                                 _In_opt_ int nFlags)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  hRes = lpJVM->RunNativeProtectedAndGetError(0, 1, [nValue](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_number(lpCtx, nValue);
    return;
  });
  if (FAILED(hRes))
    return hRes;
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, nExportsObjectIndex, szPropertyNameA, TRUE, nFlags,
                                             NullCallback(), NullCallback());
}

HRESULT CJavascriptVM::CRequireModuleContext::AddNullProperty(_In_z_ LPCSTR szPropertyNameA, _In_opt_ int nFlags)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  hRes = lpJVM->RunNativeProtectedAndGetError(0, 1, [](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_null(lpCtx);
    return;
  });
  if (FAILED(hRes))
    return hRes;
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, nExportsObjectIndex, szPropertyNameA, TRUE, nFlags,
                                             NullCallback(), NullCallback());
}

HRESULT CJavascriptVM::CRequireModuleContext::AddJsObjectProperty(_In_z_ LPCSTR szPropertyNameA,
                                                                  _In_ CJsObjectBase *lpObject, _In_opt_ int nFlags)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  HRESULT hRes;

  hRes = lpObject->PushThis();
  if (SUCCEEDED(hRes))
  {
    hRes = Internals::JsLib::AddPropertyCommon(lpCtx, NULL, nExportsObjectIndex, szPropertyNameA, TRUE, nFlags,
                                               NullCallback(), NullCallback());
    if (FAILED(hRes))
      lpObject->Release();
  }
  return hRes;
}

HRESULT CJavascriptVM::CRequireModuleContext::AddPropertyWithCallback(_In_z_ LPCSTR szPropertyNameA,
                                                                      _In_ OnGetPropertyCallback cGetValueCallback,
                                                                      _In_opt_ OnSetPropertyCallback cSetValueCallback,
                                                                      _In_opt_ int nFlags)
{
  if (!cGetValueCallback)
    return E_POINTER;
  nFlags &= ~PropertyFlagWritable;
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, nExportsObjectIndex, szPropertyNameA, FALSE, nFlags,
                                             cGetValueCallback, cSetValueCallback);
}

HRESULT CJavascriptVM::CRequireModuleContext::ReplaceModuleExports()
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  hRes = lpJVM->RunNativeProtectedAndGetError(1, 0, [this](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_put_prop_string(lpCtx, nModuleObjectIndex, "exports");
    return;
  });
  //done
  return hRes;
}

HRESULT CJavascriptVM::CRequireModuleContext::ReplaceModuleExportsWithObject(_In_ CJsObjectBase *lpObject)
{
  HRESULT hRes;

  if (lpObject == NULL)
    return E_POINTER;
  hRes = lpObject->PushThis();
  if (SUCCEEDED(hRes))
    hRes = ReplaceModuleExports();
  return hRes;
}

} //namespace MX
