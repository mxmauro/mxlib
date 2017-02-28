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

CJavascriptVM::CRequireModuleContext::CRequireModuleContext(__in DukTape::duk_context *_lpCtx, __in_z LPCWSTR _szIdW,
               __in DukTape::duk_idx_t _nModuleObjectIndex, __in DukTape::duk_idx_t _nExportsObjectIndex)
{
  lpCtx = _lpCtx;
  szIdW = _szIdW;
  nModuleObjectIndex = _nModuleObjectIndex;
  nExportsObjectIndex = _nExportsObjectIndex;
  return;
}

HRESULT CJavascriptVM::CRequireModuleContext::RequireModule(__in_z LPCWSTR szModuleIdW)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);

  if (szModuleIdW == NULL)
    return E_POINTER;
  while (*szModuleIdW == L'/')
    szModuleIdW++;
  if (*szModuleIdW == 0)
    return E_INVALIDARG;
  try
  {
    lpJVM->RunNativeProtected(0, 1, [szModuleIdW, this](__in DukTape::duk_context *lpCtx) -> VOID
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
  }
  catch (CJsWindowsError &e)
  {
    return e.GetHResult();
  }
  catch (CJsError &)
  {
    return E_FAIL;
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
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);

  try
  {
    lpJVM->RunNativeProtected(0, 1, [szValueA](__in DukTape::duk_context *lpCtx) -> VOID
    {
      if (szValueA != NULL)
        DukTape::duk_push_string(lpCtx, szValueA);
      else
        DukTape::duk_push_null(lpCtx);
      return;
    });
  }
  catch (CJsWindowsError &e)
  {
    return e.GetHResult();
  }
  catch (CJsError &)
  {
    return E_FAIL;
  }
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, nExportsObjectIndex, szPropertyNameA, TRUE, nFlags,
                                             NullCallback(), NullCallback());
}

HRESULT CJavascriptVM::CRequireModuleContext::AddBooleanProperty(__in_z LPCSTR szPropertyNameA, __in BOOL bValue,
                                                                 __in_opt int nFlags)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);

  try
  {
    lpJVM->RunNativeProtected(0, 1, [bValue](__in DukTape::duk_context *lpCtx) -> VOID
    {
      DukTape::duk_push_boolean(lpCtx, (bValue != FALSE) ? true : false);
      return;
    });
  }
  catch (CJsWindowsError &e)
  {
    return e.GetHResult();
  }
  catch (CJsError &)
  {
    return E_FAIL;
  }
  //done
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, nExportsObjectIndex, szPropertyNameA, TRUE, nFlags,
                                             NullCallback(), NullCallback());
}

HRESULT CJavascriptVM::CRequireModuleContext::AddNumericProperty(__in_z LPCSTR szPropertyNameA, __in double nValue,
                                                                 __in_opt int nFlags)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);

  try
  {
    lpJVM->RunNativeProtected(0, 1, [nValue](__in DukTape::duk_context *lpCtx) -> VOID
    {
      DukTape::duk_push_number(lpCtx, nValue);
      return;
    });
  }
  catch (CJsWindowsError &e)
  {
    return e.GetHResult();
  }
  catch (CJsError &)
  {
    return E_FAIL;
  }
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, nExportsObjectIndex, szPropertyNameA, TRUE, nFlags,
                                             NullCallback(), NullCallback());
}

HRESULT CJavascriptVM::CRequireModuleContext::AddNullProperty(__in_z LPCSTR szPropertyNameA, __in_opt int nFlags)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);

  try
  {
    lpJVM->RunNativeProtected(0, 1, [](__in DukTape::duk_context *lpCtx) -> VOID
    {
      DukTape::duk_push_null(lpCtx);
      return;
    });
  }
  catch (CJsWindowsError &e)
  {
    return e.GetHResult();
  }
  catch (CJsError &)
  {
    return E_FAIL;
  }
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, nExportsObjectIndex, szPropertyNameA, TRUE, nFlags,
                                             NullCallback(), NullCallback());
}

HRESULT CJavascriptVM::CRequireModuleContext::AddJsObjectProperty(__in_z LPCSTR szPropertyNameA,
                                                                  __in CJsObjectBase *lpObject, __in_opt int nFlags)
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

HRESULT CJavascriptVM::CRequireModuleContext::ReplaceModuleExports()
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);

  try
  {
    lpJVM->RunNativeProtected(1, 0, [this](__in DukTape::duk_context *lpCtx) -> VOID
    {
      DukTape::duk_put_prop_string(lpCtx, nModuleObjectIndex, "exports");
      return;
    });
  }
  catch (CJsWindowsError &e)
  {
    return e.GetHResult();
  }
  catch (CJsError &)
  {
    return E_FAIL;
  }
  return S_OK;
}

HRESULT CJavascriptVM::CRequireModuleContext::ReplaceModuleExportsWithObject(__in CJsObjectBase *lpObject)
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
