/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2015. All rights reserved.
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
#include "JavascriptVMCommon.h"

//-----------------------------------------------------------

namespace MX {

CJavascriptVM::CRequireModuleContext::CRequireModuleContext()
{
  lpCtx = NULL;
  szIdW = NULL;
  nRequireModuleIndex = nExportsObjectIndex = nModuleObjectIndex = 0;
  return;
}

HRESULT CJavascriptVM::CRequireModuleContext::RequireModule(__in_z LPCWSTR szModuleIdW)
{
  CStringA cStrTempA;

  if (szModuleIdW == NULL)
    return E_POINTER;
  while (*szModuleIdW == L'/')
    szModuleIdW++;
  if (*szModuleIdW == 0)
    return E_INVALIDARG;
  if (cStrTempA.Copy(szModuleIdW) == FALSE)
    return E_OUTOFMEMORY;
  try
  {
    DukTape::duk_dup(lpCtx, nRequireModuleIndex);
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTempA, cStrTempA.GetLength());
    DukTape::duk_call(lpCtx, 1);
  }
  catch (CJavascriptVM::CException& ex)
  {
    return ex.GetErrorCode();
  }
  //done
  return S_OK;
}

HRESULT CJavascriptVM::CRequireModuleContext::AddNativeFunction(__in_z LPCSTR szFuncNameA,
                                                                __in OnNativeFunctionCallback cNativeFunctionCallback,
                                                                __in int nArgsCount)
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

HRESULT CJavascriptVM::CRequireModuleContext::AddProperty(__in_z LPCSTR szPropertyNameA,
                                                          __in_opt BOOL bInitialValueOnStack,
                                                          __in_opt int nFlags)
{
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, nExportsObjectIndex, szPropertyNameA, bInitialValueOnStack,
                                             nFlags, NullCallback(), NullCallback());
}

HRESULT CJavascriptVM::CRequireModuleContext::AddStringProperty(__in_z LPCSTR szPropertyNameA, __in_z LPCSTR szValueA,
                                                                __in_opt int nFlags)
{
  try
  {
    if (szValueA != NULL)
      DukTape::duk_push_string(lpCtx, szValueA);
    else
      DukTape::duk_push_null(lpCtx);
  }
  catch (CJavascriptVM::CException& ex)
  {
    return ex.GetErrorCode();
  }
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, nExportsObjectIndex, szPropertyNameA, TRUE, nFlags,
                                             NullCallback(), NullCallback());
}

HRESULT CJavascriptVM::CRequireModuleContext::AddBooleanProperty(__in_z LPCSTR szPropertyNameA, __in BOOL bValue,
                                                                 __in_opt int nFlags)
{
  try
  {
    DukTape::duk_push_boolean(lpCtx, (bValue != FALSE) ? true : false);
  }
  catch (CJavascriptVM::CException& ex)
  {
    return ex.GetErrorCode();
  }
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, nExportsObjectIndex, szPropertyNameA, TRUE, nFlags,
                                             NullCallback(), NullCallback());
}

HRESULT CJavascriptVM::CRequireModuleContext::AddNumericProperty(__in_z LPCSTR szPropertyNameA, __in double nValue,
                                                                 __in_opt int nFlags)
{
  try
  {
    DukTape::duk_push_number(lpCtx, nValue);
  }
  catch (CJavascriptVM::CException& ex)
  {
    return ex.GetErrorCode();
  }
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, nExportsObjectIndex, szPropertyNameA, TRUE, nFlags,
                                             NullCallback(), NullCallback());
}

HRESULT CJavascriptVM::CRequireModuleContext::AddNullProperty(__in_z LPCSTR szPropertyNameA, __in_opt int nFlags)
{
  try
  {
    DukTape::duk_push_null(lpCtx);
  }
  catch (CJavascriptVM::CException& ex)
  {
    return ex.GetErrorCode();
  }
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, nExportsObjectIndex, szPropertyNameA, TRUE, nFlags,
                                             NullCallback(), NullCallback());
}

HRESULT CJavascriptVM::CRequireModuleContext::AddJsObjectProperty(__in_z LPCSTR szPropertyNameA,
                                                                  __in CJsObjectBase *lpObject, __in_opt int nFlags)
{
  DukTape::duk_idx_t nStackTop;
  HRESULT hRes;

  nStackTop = DukTape::duk_get_top(lpCtx);
  try
  {
    lpObject->PushThis();
  }
  catch (CJavascriptVM::CException& ex)
  {
    DukTape::duk_set_top(lpCtx, nStackTop);
    return ex.GetErrorCode();
  }
  hRes = Internals::JsLib::AddPropertyCommon(lpCtx, NULL, nExportsObjectIndex, szPropertyNameA, TRUE, nFlags,
                                             NullCallback(), NullCallback());
  if (FAILED(hRes))
    lpObject->Release();
  return hRes;
}

HRESULT CJavascriptVM::CRequireModuleContext::AddPropertyWithCallback(__in_z LPCSTR szPropertyNameA,
                                                                      __in OnGetPropertyCallback cGetValueCallback,
                                                                      __in_opt OnSetPropertyCallback cSetValueCallback,
                                                                      __in_opt int nFlags)
{
  if (!cGetValueCallback)
    return E_POINTER;
  nFlags &= ~PropertyFlagWritable;
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, nExportsObjectIndex, szPropertyNameA, FALSE, nFlags,
                                             cGetValueCallback, cSetValueCallback);
}

HRESULT CJavascriptVM::CRequireModuleContext::ReplaceModuleExports(__in DukTape::duk_idx_t nObjIndex,
                                                                   __in_opt BOOL bRemoveFromStack)
{
  try
  {
    if (nObjIndex < 0)
      nObjIndex = DukTape::duk_normalize_index(lpCtx, nObjIndex);
    DukTape::duk_dup(lpCtx, nObjIndex);
    DukTape::duk_put_prop_string(lpCtx, nModuleObjectIndex, "exports");
    if (bRemoveFromStack != FALSE)
      DukTape::duk_remove(lpCtx, nObjIndex);
  }
  catch (CJavascriptVM::CException& ex)
  {
    return ex.GetErrorCode();
  }
  //done
  return S_OK;
}

HRESULT CJavascriptVM::CRequireModuleContext::ReplaceModuleExportsWithObject(__in CJsObjectBase *lpObject)
{
  HRESULT hRes;

  if (lpObject == NULL)
    return E_POINTER;
  if (lpObject->PushThis() != 1)
    return E_FAIL;
  hRes = ReplaceModuleExports(-1, TRUE);
  if (FAILED(hRes))
    DukTape::duk_pop(lpCtx); //release reference added by PushThis
  return hRes;
}

} //namespace MX
