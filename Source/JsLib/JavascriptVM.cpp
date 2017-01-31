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
#define _MX_JAVASCRIPT_VM_CPP
#include "JavascriptVMCommon.h"

//-----------------------------------------------------------

typedef struct {
  MX::CJavascriptVM::protected_function func;
  DukTape::duk_idx_t nRetValuesCount;
} RUN_NATIVE_PROTECTED_DATA;

//-----------------------------------------------------------

static DukTape::duk_ret_t OnJsOutputDebugString(__in DukTape::duk_context *lpCtx);

static void* my_duk_alloc_function(void *udata, DukTape::duk_size_t size);
static void* my_duk_realloc_function(void *udata, void *ptr, DukTape::duk_size_t size);
static void my_duk_free_function(void *udata, void *ptr);

static void my_duk_fatal_function(void *udata, const char *msg);

//-----------------------------------------------------------

namespace MX {

CJavascriptVM::CJavascriptVM() : CBaseMemObj()
{
  lpCtx = NULL;
  cRequireModuleCallback = NullCallback();
  return;
}

CJavascriptVM::~CJavascriptVM()
{
  Finalize();
  return;
}

VOID CJavascriptVM::On(__in OnRequireModuleCallback _cRequireModuleCallback)
{
  cRequireModuleCallback = _cRequireModuleCallback;
  return;
}

HRESULT CJavascriptVM::Initialize()
{
  HRESULT hRes = S_OK;

  Finalize();
  //----
  lpCtx = DukTape::duk_create_heap(&my_duk_alloc_function, &my_duk_realloc_function, &my_duk_free_function,
                                   NULL, &my_duk_fatal_function);
  if (!lpCtx)
    return E_OUTOFMEMORY;
  hRes = RunNativeProtected(0, 0, [this](__in DukTape::duk_context *lpCtx) -> VOID
  {
    //setup "jsVM" accessible only from native for callbacks
    DukTape::duk_push_global_object(lpCtx);
    DukTape::duk_push_pointer(lpCtx, this);
    DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""jsVM");
    DukTape::duk_pop(lpCtx);
    //add debug output
    DukTape::duk_push_global_object(lpCtx);
    DukTape::duk_push_c_function(lpCtx, &OnJsOutputDebugString, 1);
    DukTape::duk_put_prop_string(lpCtx, -2, "DebugPrint");
    DukTape::duk_pop(lpCtx);
    //setup DukTape's Node.js-like module loading framework
    DukTape::duk_push_object(lpCtx);
    DukTape::duk_push_c_function(lpCtx, &CJavascriptVM::OnNodeJsResolveModule, MX_JS_VARARGS);
    DukTape::duk_put_prop_string(lpCtx, -2, "resolve");
    DukTape::duk_push_c_function(lpCtx, &CJavascriptVM::OnNodeJsLoadModule, MX_JS_VARARGS);
    DukTape::duk_put_prop_string(lpCtx, -2, "load");
    DukTape::duk_module_node_init(lpCtx);
    return;
  });
  if (FAILED(hRes))
  {
    DukTape::duk_destroy_heap(lpCtx);
    lpCtx = NULL;
  }
  //done
  return hRes;
}

VOID CJavascriptVM::Finalize()
{
  if (lpCtx != NULL)
  {
    DukTape::duk_destroy_heap(lpCtx);
    lpCtx = NULL;
  }
  return;
}

CJavascriptVM* CJavascriptVM::FromContext(__in DukTape::duk_context *lpCtx)
{
  CJavascriptVM *lpVM;

  DukTape::duk_push_global_object(lpCtx);
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""jsVM");
  lpVM = reinterpret_cast<MX::CJavascriptVM*>(DukTape::duk_to_pointer(lpCtx, -1));
  DukTape::duk_pop_2(lpCtx);
  return lpVM;
}

HRESULT CJavascriptVM::Run(__in_z LPCSTR szCodeA, __in_z_opt LPCWSTR szFileNameW, __in_opt BOOL bIgnoreResult)
{
  HRESULT hRes;

  if (szCodeA == NULL)
    return E_OUTOFMEMORY;
  if (*szCodeA == 0)
    return E_INVALIDARG;
  if (lpCtx == NULL)
    return E_FAIL;
  if (szFileNameW != NULL)
  {
    while (*szFileNameW == L'/')
      szFileNameW++;
  }
  if (szFileNameW == NULL || *szFileNameW == 0)
    szFileNameW = L"main.jss";
  //initialize last execution error
  hRes = RunNativeProtected(0, (bIgnoreResult != FALSE) ? 0 : 1, [szFileNameW, szCodeA, this]
                            (__in DukTape::duk_context *lpCtx) -> VOID
  {
    CStringA cStrTempA;
    HRESULT hRes;

    hRes = Utf8_Encode(cStrTempA, szFileNameW);
    if (FAILED(hRes))
      MX_JS_THROW_HRESULT_ERROR(lpCtx, hRes);
    //modify require.id to match our passed filename so we have a correct path resolution in 'modSearch'
    DukTape::duk_push_global_object(lpCtx);
    DukTape::duk_get_prop_string(lpCtx, -1, "require");
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTempA, cStrTempA.GetLength());
    DukTape::duk_put_prop_string(lpCtx, -2, "\xff" "moduleId");
    DukTape::duk_pop_2(lpCtx);
    //run code
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTempA, cStrTempA.GetLength());
    DukTape::duk_eval_raw(lpCtx, szCodeA, 0, DUK_COMPILE_STRLEN | DUK_COMPILE_NOSOURCE);
    return;
  });
  //done
  return hRes;
}

HRESULT CJavascriptVM::RunNativeProtected(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nArgsCount,
                                          __in DukTape::duk_idx_t nRetValuesCount, __in protected_function func,
                                          __out_opt CErrorInfo *lpErrorInfo)
{
  RUN_NATIVE_PROTECTED_DATA sData;
  DukTape::duk_idx_t nBaseIdx, nStackTop;
  HRESULT hRes = S_OK;

  if (lpErrorInfo != NULL)
    lpErrorInfo->Clear();
  nStackTop = DukTape::duk_get_top(lpCtx);
  sData.func = func;
  sData.nRetValuesCount = nRetValuesCount;
  if (nRetValuesCount == 0)
    nRetValuesCount = 1;
  //do safe call
  try
  {
    nBaseIdx = nStackTop - nArgsCount;
    if (DukTape::duk_safe_call(lpCtx, &CJavascriptVM::_RunNativeProtectedHelper, &sData, nArgsCount,
                               nRetValuesCount) != DUK_EXEC_SUCCESS)
    {
      hRes = GetErrorInfoFromException(lpCtx, nBaseIdx, lpErrorInfo);
      DukTape::duk_pop(lpCtx);
    }
    else if (sData.nRetValuesCount == 0)
    {
      //because of a bug in 'duk_safe_call', we enforce returning at least one result so we can see the error code
      DukTape::duk_pop(lpCtx);
    }
  }
  catch (HRESULT _hRes)
  {
    DukTape::duk_set_top(lpCtx, nStackTop);
    if (lpErrorInfo != NULL)
    {
      lpErrorInfo->Clear();
      lpErrorInfo->hRes = _hRes;
    }
    hRes = _hRes;
  }
  catch (...)
  {
    DukTape::duk_set_top(lpCtx, nStackTop);
    if (lpErrorInfo != NULL)
    {
      lpErrorInfo->Clear();
      lpErrorInfo->hRes = MX_E_UnhandledException;
    }
    hRes = MX_E_UnhandledException;
  }
  //done
  return hRes;
}

HRESULT CJavascriptVM::RunNativeProtected(__in DukTape::duk_idx_t nArgsCount, __in DukTape::duk_idx_t nRetValuesCount,
                                          __in protected_function func)
{
  return RunNativeProtected(lpCtx, nArgsCount, nRetValuesCount, func, &cLastErrorInfo);
}

HRESULT CJavascriptVM::AddNativeFunction(__in_z LPCSTR szFuncNameA, __in OnNativeFunctionCallback cCallback,
                                         __in int nArgsCount)
{
  if (szFuncNameA == NULL || !cCallback)
    return E_POINTER;
  if (*szFuncNameA == 0 || nArgsCount < -1)
    return E_INVALIDARG;
  if (lpCtx == NULL)
    return E_FAIL;
  return Internals::JsLib::AddNativeFunctionCommon(lpCtx, NULL, -1, szFuncNameA, cCallback, nArgsCount);
}

HRESULT CJavascriptVM::AddProperty(__in_z LPCSTR szPropertyNameA, __in_opt BOOL bInitialValueOnStack,
                                   __in_opt int nFlags)
{
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, -1, szPropertyNameA, bInitialValueOnStack, nFlags,
                                             NullCallback(), NullCallback());
}

HRESULT CJavascriptVM::AddStringProperty(__in_z LPCSTR szPropertyNameA, __in_z LPCSTR szValueA, __in_opt int nFlags)
{
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtected(0, 1, [szValueA](__in DukTape::duk_context *lpCtx) -> VOID
  {
    if (szValueA != NULL)
      DukTape::duk_push_string(lpCtx, szValueA);
    else
      DukTape::duk_push_null(lpCtx);
    return;
  });
  if (SUCCEEDED(hRes))
  {
    hRes = Internals::JsLib::AddPropertyCommon(lpCtx, NULL, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                               NullCallback());
  }
  //done
  return hRes;
}

HRESULT CJavascriptVM::AddStringProperty(__in_z LPCSTR szPropertyNameA, __in_z LPCWSTR szValueW,
                                         __in_opt int nFlags)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  if (szValueW == NULL)
    return AddStringProperty(szPropertyNameA, (LPCSTR)NULL, nFlags);
  hRes = Utf8_Encode(cStrTempA, szValueW);
  if (SUCCEEDED(hRes))
    hRes = AddStringProperty(szPropertyNameA, (LPCSTR)cStrTempA, nFlags);
  //done
  return hRes;
}

HRESULT CJavascriptVM::AddBooleanProperty(__in_z LPCSTR szPropertyNameA, __in BOOL bValue, __in_opt int nFlags)
{
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtected(0, 1, [bValue](__in DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_boolean(lpCtx, (bValue != FALSE) ? true : false);
    return;
  });
  if (SUCCEEDED(hRes))
  {
    hRes = Internals::JsLib::AddPropertyCommon(lpCtx, NULL, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                               NullCallback());
  }
  //done
  return hRes;
}

HRESULT CJavascriptVM::AddIntegerProperty(__in_z LPCSTR szPropertyNameA, __in int nValue, __in_opt int nFlags)
{
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtected(0, 1, [nValue](__in DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)nValue);
    return;
  });
  if (SUCCEEDED(hRes))
  {
    hRes = Internals::JsLib::AddPropertyCommon(lpCtx, NULL, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                               NullCallback());
  }
  //done
  return hRes;
}

HRESULT CJavascriptVM::AddNumericProperty(__in_z LPCSTR szPropertyNameA, __in double nValue, __in_opt int nFlags)
{
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtected(0, 1, [nValue](__in DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_number(lpCtx, nValue);
    return;
  });
  if (SUCCEEDED(hRes))
  {
    hRes = Internals::JsLib::AddPropertyCommon(lpCtx, NULL, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                               NullCallback());
  }
  //done
  return hRes;
}

HRESULT CJavascriptVM::AddNullProperty(__in_z LPCSTR szPropertyNameA, __in_opt int nFlags)
{
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtected(0, 1, [](__in DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_null(lpCtx);
    return;
  });
  if (SUCCEEDED(hRes))
  {
    hRes = Internals::JsLib::AddPropertyCommon(lpCtx, NULL, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                               NullCallback());
  }
  //done
  return hRes;
}

HRESULT CJavascriptVM::AddJsObjectProperty(__in_z LPCSTR szPropertyNameA, __in CJsObjectBase *lpObject,
                                           __in_opt int nFlags)
{
  HRESULT hRes;

  if (lpCtx == NULL || lpCtx != lpObject->GetContext())
    return E_FAIL;
  hRes = lpObject->PushThis();
  if (SUCCEEDED(hRes))
  {
    hRes = Internals::JsLib::AddPropertyCommon(lpCtx, NULL, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                               NullCallback());
  }
  //done
  return hRes;
}

HRESULT CJavascriptVM::AddPropertyWithCallback(__in_z LPCSTR szPropertyNameA,
                                               __in OnGetPropertyCallback cGetValueCallback,
                                               __in_opt OnSetPropertyCallback cSetValueCallback, __in_opt int nFlags)
{
  if (!cGetValueCallback)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  nFlags &= ~PropertyFlagWritable;
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, -1, szPropertyNameA, FALSE, nFlags, cGetValueCallback,
                                             cSetValueCallback);
}

HRESULT CJavascriptVM::RemoveProperty(__in_z LPCSTR szPropertyNameA)
{
  if (lpCtx == NULL)
    return E_FAIL;
  return Internals::JsLib::RemovePropertyCommon(lpCtx, NULL, szPropertyNameA);
}

HRESULT CJavascriptVM::HasProperty(__in_z LPCSTR szPropertyNameA)
{
  if (lpCtx == NULL)
    return E_FAIL;
  return Internals::JsLib::HasPropertyCommon(lpCtx, NULL, szPropertyNameA);
}

HRESULT CJavascriptVM::PushProperty(__in_z LPCSTR szPropertyNameA)
{
  if (lpCtx == NULL)
    return E_FAIL;
  return Internals::JsLib::PushPropertyCommon(lpCtx, NULL, szPropertyNameA);
}

HRESULT CJavascriptVM::CreateObject(__in_z LPCSTR szObjectNameA, __in_opt CProxyCallbacks *lpCallbacks)
{
  HRESULT hRes;

  if (szObjectNameA == NULL)
    return E_POINTER;
  if (*szObjectNameA == 0)
    return E_INVALIDARG;
  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtected(0, 0, [szObjectNameA, lpCallbacks](__in DukTape::duk_context *lpCtx) -> VOID
  {
    LPCSTR sA, szObjectNameA_2;
    HRESULT hRes;

    hRes = Internals::JsLib::FindObject(lpCtx, szObjectNameA, TRUE, FALSE); //NOTE: Should the parent be the proxy???
    if (FAILED(hRes))
      MX_JS_THROW_HRESULT_ERROR(lpCtx, hRes);
    //if FindObject returned OK, then the object we are trying to create, already exists
    if (hRes == S_OK)
      MX_JS_THROW_HRESULT_ERROR(lpCtx, MX_E_AlreadyExists);
    MX_ASSERT(hRes == S_FALSE);
    //the stack contains the parent object, create new one
    DukTape::duk_push_object(lpCtx);
    //advance 'szObjectNameA' to point to the last portion of the name
    sA = StrChrA(szObjectNameA_2 = szObjectNameA, '.', TRUE);
    if (sA != NULL)
      szObjectNameA_2 = sA + 1;
    //store name in object's internal property
    DukTape::duk_push_string(lpCtx, szObjectNameA_2);
    DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""name");
    //setup callbacks (if any) by creating and returning a Proxy object
    if (lpCallbacks != NULL)
    {
      LPVOID p;

      //create handler object
      DukTape::duk_push_object(lpCtx);
      //create a buffer to store the serialized callback data
      p = DukTape::duk_push_fixed_buffer(lpCtx, CProxyCallbacks::serialization_buffer_size());
      lpCallbacks->serialize(p);
      DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""callbacks");
      //save object's name
      DukTape::duk_push_string(lpCtx, szObjectNameA_2);
      DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""name");
      //add handler functions
      DukTape::duk_push_c_function(lpCtx, &CJavascriptVM::_ProxyHasPropHelper, 2);
      DukTape::duk_put_prop_string(lpCtx, -2, "has");
      DukTape::duk_push_c_function(lpCtx, &CJavascriptVM::_ProxyGetPropHelper, 3);
      DukTape::duk_put_prop_string(lpCtx, -2, "get");
      DukTape::duk_push_c_function(lpCtx, &CJavascriptVM::_ProxySetPropHelper, 4);
      DukTape::duk_put_prop_string(lpCtx, -2, "set");
      DukTape::duk_push_c_function(lpCtx, &CJavascriptVM::_ProxyDeletePropHelper, 2);
      DukTape::duk_put_prop_string(lpCtx, -2, "deleteProperty");
      //duplicate target object reference for later use
      DukTape::duk_dup(lpCtx, -2);
      //push Proxy object's constructor
      DukTape::duk_get_global_string(lpCtx, "Proxy");
      //reorder stack [target...handler...target... Proxy] -> [target... Proxy...target...handler]
      DukTape::duk_swap(lpCtx, -3, -1);
      //create proxy object
      DukTape::duk_new(lpCtx, 2);
      //save target object in proxy using previous reference
      DukTape::duk_swap(lpCtx, -2, -1);
      DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""proxyTarget");
    }
    //store object in its parent
    DukTape::duk_put_prop_string(lpCtx, -2, szObjectNameA_2);
    //pop parent object
    DukTape::duk_pop(lpCtx);
    return;
  });
  //done
  return hRes;
}

HRESULT CJavascriptVM::AddObjectNativeFunction(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szFuncNameA,
                                               __in OnNativeFunctionCallback cCallback,
                                               __in int nArgsCount)
{
  if (szObjectNameA == NULL || szFuncNameA == NULL || !cCallback)
    return E_POINTER;
  if (*szObjectNameA == 0 || *szFuncNameA == 0 || nArgsCount < -1)
    return E_INVALIDARG;
  if (lpCtx == NULL)
    return E_FAIL;
  return Internals::JsLib::AddNativeFunctionCommon(lpCtx, szObjectNameA, -1, szFuncNameA, cCallback, nArgsCount);
}

HRESULT CJavascriptVM::AddObjectProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA,
                                         __in_opt BOOL bInitialValueOnStack, __in_opt int nFlags)
{
  if (szObjectNameA == NULL)
    return E_POINTER;
  return Internals::JsLib::AddPropertyCommon(lpCtx, szObjectNameA, -1, szPropertyNameA, bInitialValueOnStack, nFlags,
                                             NullCallback(), NullCallback());
}

HRESULT CJavascriptVM::AddObjectStringProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA,
                                               __in_z LPCSTR szValueA, __in_opt int nFlags)
{
  HRESULT hRes;

  if (szObjectNameA == NULL)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtected(0, 1, [szValueA](__in DukTape::duk_context *lpCtx) -> VOID
  {
    if (szValueA != NULL)
      DukTape::duk_push_string(lpCtx, szValueA);
    else
      DukTape::duk_push_null(lpCtx);
    return;
  });
  if (SUCCEEDED(hRes))
  {
    hRes = Internals::JsLib::AddPropertyCommon(lpCtx, szObjectNameA, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                               NullCallback());
  }
  //done
  return hRes;
}

HRESULT CJavascriptVM::AddObjectStringProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA,
                                               __in_z LPCWSTR szValueW, __in_opt int nFlags)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  if (szValueW == NULL)
    return AddObjectStringProperty(szObjectNameA, szPropertyNameA, (LPCSTR)NULL, nFlags);
  hRes = Utf8_Encode(cStrTempA, szValueW);
  if (SUCCEEDED(hRes))
    hRes = AddObjectStringProperty(szObjectNameA, szPropertyNameA, (LPCSTR)cStrTempA, nFlags);
  //done
  return hRes;
}

HRESULT CJavascriptVM::AddObjectBooleanProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA,
                                                __in BOOL bValue, __in_opt int nFlags)
{
  HRESULT hRes;

  if (szObjectNameA == NULL)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtected(0, 1, [bValue](__in DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_boolean(lpCtx, (bValue != FALSE) ? true : false);
    return;
  });
  if (SUCCEEDED(hRes))
  {
    hRes = Internals::JsLib::AddPropertyCommon(lpCtx, szObjectNameA, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                               NullCallback());
  }
  //done
  return hRes;
}

HRESULT CJavascriptVM::AddObjectIntegerProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA,
                                                __in int nValue, __in_opt int nFlags)
{
  HRESULT hRes;

  if (szObjectNameA == NULL)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtected(0, 1, [nValue](__in DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)nValue);
    return;
  });
  if (SUCCEEDED(hRes))
  {
    hRes = Internals::JsLib::AddPropertyCommon(lpCtx, szObjectNameA, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                               NullCallback());
  }
  //done
  return hRes;
}

HRESULT CJavascriptVM::AddObjectNumericProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA,
                                                __in double nValue, __in_opt int nFlags)
{
  HRESULT hRes;

  if (szObjectNameA == NULL)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtected(0, 1, [nValue](__in DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_number(lpCtx, nValue);
    return;
  });
  if (SUCCEEDED(hRes))
  {
    hRes = Internals::JsLib::AddPropertyCommon(lpCtx, szObjectNameA, -1, szPropertyNameA, TRUE, nFlags,
                                               NullCallback(), NullCallback());
  }
  //done
  return hRes;
}

HRESULT CJavascriptVM::AddObjectNullProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA,
                                             __in_opt int nFlags)
{
  HRESULT hRes;

  if (szObjectNameA == NULL)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtected(0, 1, [](__in DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_null(lpCtx);
    return;
  });
  if (SUCCEEDED(hRes))
  {
    hRes = Internals::JsLib::AddPropertyCommon(lpCtx, szObjectNameA, -1, szPropertyNameA, TRUE, nFlags,
                                               NullCallback(), NullCallback());
  }
  //done
  return hRes;
}

HRESULT CJavascriptVM::AddObjectJsObjectProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA,
                                                 __in CJsObjectBase *lpObject, __in_opt int nFlags)
{
  HRESULT hRes;

  if (szObjectNameA == NULL)
    return E_POINTER;
  if (lpCtx == NULL || lpCtx != lpObject->GetContext())
    return E_FAIL;
  hRes = lpObject->PushThis();
  if (SUCCEEDED(hRes))
  {
    hRes = Internals::JsLib::AddPropertyCommon(lpCtx, szObjectNameA, -1, szPropertyNameA, TRUE, nFlags,
                                               NullCallback(), NullCallback());
  }
  //done
  return hRes;
}

HRESULT CJavascriptVM::AddObjectPropertyWithCallback(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA,
                                                     __in OnGetPropertyCallback cGetValueCallback,
                                                     __in_opt OnSetPropertyCallback cSetValueCallback,
                                                     __in_opt int nFlags)
{
  if (szObjectNameA == NULL)
    return E_POINTER;
  if (!cGetValueCallback)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  nFlags &= ~PropertyFlagWritable;
  return Internals::JsLib::AddPropertyCommon(lpCtx, szObjectNameA, -1, szPropertyNameA, FALSE, nFlags,
                                             cGetValueCallback, cSetValueCallback);
}

HRESULT CJavascriptVM::RemoveObjectProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA)
{
  if (szObjectNameA == NULL)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  return Internals::JsLib::RemovePropertyCommon(lpCtx, szObjectNameA, szPropertyNameA);
}

HRESULT CJavascriptVM::HasObjectProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA)
{
  if (szObjectNameA == NULL)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  return Internals::JsLib::HasPropertyCommon(lpCtx, szObjectNameA, szPropertyNameA);
}

HRESULT CJavascriptVM::PushObjectProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA)
{
  if (szObjectNameA == NULL)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  return Internals::JsLib::PushPropertyCommon(lpCtx, szObjectNameA, szPropertyNameA);
}

VOID CJavascriptVM::PushString(__in DukTape::duk_context *lpCtx, __in_z LPCWSTR szStrW, __in_opt SIZE_T nStrLen)
{
  MX::CStringA cStrTempA;
  HRESULT hRes;

  hRes = Utf8_Encode(cStrTempA, szStrW, nStrLen);
  if (FAILED(hRes))
    MX_JS_THROW_HRESULT_ERROR(lpCtx, hRes);
  DukTape::duk_push_string(lpCtx, cStrTempA);
  return;
}

VOID CJavascriptVM::PushString(__in_z LPCWSTR szStrW, __in_opt SIZE_T nStrLen)
{
  PushString(lpCtx, szStrW, nStrLen);
  return;
}

VOID CJavascriptVM::GetString(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nStackIndex,
                              __inout CStringW &cStrW, __in_opt BOOL bAppend)
{
  LPCSTR sA;
  HRESULT hRes;

  if (bAppend == FALSE)
    cStrW.Empty();
  sA = DukTape::duk_require_string(lpCtx, nStackIndex);
  hRes = Utf8_Decode(cStrW, sA, (SIZE_T)-1, bAppend);
  if (FAILED(hRes))
    MX_JS_THROW_HRESULT_ERROR(lpCtx, hRes);
  return;
}

VOID CJavascriptVM::GetString(__in DukTape::duk_idx_t nStackIndex, __inout CStringW &cStrW, __in_opt BOOL bAppend)
{
  GetString(lpCtx, nStackIndex, cStrW, bAppend);
  return;
}

VOID CJavascriptVM::PushDate(__in DukTape::duk_context *lpCtx, __in LPSYSTEMTIME lpSt, __in_opt BOOL bAsUtc)
{
  DukTape::duk_idx_t nObjIdx;

  if (lpSt == NULL)
    MX_JS_THROW_HRESULT_ERROR(lpCtx, E_POINTER);
  DukTape::duk_get_global_string(lpCtx, "Date");
  DukTape::duk_new(lpCtx, 0);
  nObjIdx = DukTape::duk_normalize_index(lpCtx, -1);
  //----
  DukTape::duk_get_prop_string(lpCtx, nObjIdx, (bAsUtc != FALSE) ? "setUTCFullYear" : "setFullYear");
  DukTape::duk_dup(lpCtx, nObjIdx);
  DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)(lpSt->wYear));
  DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)(lpSt->wMonth - 1));
  DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)(lpSt->wDay));
  DukTape::duk_call_method(lpCtx, 3);
  DukTape::duk_pop(lpCtx); //pop void return
  //----
  DukTape::duk_get_prop_string(lpCtx, nObjIdx, (bAsUtc != FALSE) ? "setUTCHours" : "setHours");
  DukTape::duk_dup(lpCtx, nObjIdx);
  DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)(lpSt->wHour));
  DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)(lpSt->wMinute));
  DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)(lpSt->wSecond));
  DukTape::duk_push_uint(lpCtx, (DukTape::duk_uint_t)(lpSt->wMilliseconds));
  DukTape::duk_call_method(lpCtx, 4);
  DukTape::duk_pop(lpCtx); //pop void return
  return;
}

VOID CJavascriptVM::PushDate(__in LPSYSTEMTIME lpSt, __in_opt BOOL bAsUtc)
{
  PushDate(lpCtx, lpSt, bAsUtc);
  return;
}

VOID CJavascriptVM::GetDate(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nObjIdx, __out LPSYSTEMTIME lpSt)
{
  static WORD wDaysInMonths[12] ={ 31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  static WORD wMonthOffsets[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
  WORD y, w;

  if (lpSt == NULL)
    MX_JS_THROW_HRESULT_ERROR(lpCtx, E_POINTER);
  MX::MemSet(lpSt, 0, sizeof(SYSTEMTIME));
  
  nObjIdx = DukTape::duk_normalize_index(lpCtx, nObjIdx);

  if (DukTape::duk_is_object(lpCtx, nObjIdx) == 0)
    MX_JS_THROW_HRESULT_ERROR(lpCtx, E_FAIL);

  DukTape::duk_get_prop_string(lpCtx, nObjIdx, "getUTCDate");
  DukTape::duk_dup(lpCtx, nObjIdx);
  DukTape::duk_call_method(lpCtx, 0);
  lpSt->wDay = (WORD)(DukTape::duk_require_int(lpCtx, -1));
  DukTape::duk_pop(lpCtx);
  //----
  DukTape::duk_get_prop_string(lpCtx, nObjIdx, "getUTCMonth");
  DukTape::duk_dup(lpCtx, nObjIdx);
  DukTape::duk_call_method(lpCtx, 0);
  lpSt->wMonth = (WORD)(DukTape::duk_require_int(lpCtx, -1)) + 1;
  DukTape::duk_pop(lpCtx);
  //----
  DukTape::duk_get_prop_string(lpCtx, nObjIdx, "getUTCFullYear");
  DukTape::duk_dup(lpCtx, nObjIdx);
  DukTape::duk_call_method(lpCtx, 0);
  lpSt->wYear = (WORD)(DukTape::duk_require_int(lpCtx, -1));
  DukTape::duk_pop(lpCtx);
  //----
  DukTape::duk_get_prop_string(lpCtx, nObjIdx, "getUTCHours");
  DukTape::duk_dup(lpCtx, nObjIdx);
  DukTape::duk_call_method(lpCtx, 0);
  lpSt->wHour = (WORD)(DukTape::duk_require_int(lpCtx, -1));
  DukTape::duk_pop(lpCtx);
  //----
  DukTape::duk_get_prop_string(lpCtx, nObjIdx, "getUTCMinutes");
  DukTape::duk_dup(lpCtx, nObjIdx);
  DukTape::duk_call_method(lpCtx, 0);
  lpSt->wMinute = (WORD)(DukTape::duk_require_int(lpCtx, -1));
  DukTape::duk_pop(lpCtx);
  //----
  DukTape::duk_get_prop_string(lpCtx, nObjIdx, "getUTCSeconds");
  DukTape::duk_dup(lpCtx, nObjIdx);
  DukTape::duk_call_method(lpCtx, 0);
  lpSt->wSecond = (WORD)(DukTape::duk_require_int(lpCtx, -1));
  DukTape::duk_pop(lpCtx);
  //----
  DukTape::duk_get_prop_string(lpCtx, nObjIdx, "getUTCMilliseconds");
  DukTape::duk_dup(lpCtx, nObjIdx);
  DukTape::duk_call_method(lpCtx, 0);
  lpSt->wMilliseconds = (WORD)(DukTape::duk_require_int(lpCtx, -1));
  DukTape::duk_pop(lpCtx);
  //validate
  if (lpSt->wMonth < 1 || lpSt->wMonth > 12 || lpSt->wYear < 1 || lpSt->wDay < 1)
    MX_JS_THROW_HRESULT_ERROR(lpCtx, E_FAIL);
  if (lpSt->wMonth != 2)
  {
    if (lpSt->wDay > wDaysInMonths[lpSt->wMonth - 1])
      MX_JS_THROW_HRESULT_ERROR(lpCtx, E_FAIL);
  }
  else
  {
    w = ((lpSt->wYear % 400) == 0 || ((lpSt->wYear % 4) == 0 && (lpSt->wYear % 100) != 0)) ? 29 : 28;
    if (lpSt->wDay > w)
      MX_JS_THROW_HRESULT_ERROR(lpCtx, E_FAIL);
  }
  //calculate day of week
  y = lpSt->wYear - ((lpSt->wMonth < 3) ? 1 : 0);
  lpSt->wDayOfWeek = (lpSt->wYear + lpSt->wYear / 4 - lpSt->wYear / 100 + lpSt->wYear / 400 +
                      wMonthOffsets[lpSt->wMonth - 1] + lpSt->wDay) % 7;
  return;
}

VOID CJavascriptVM::GetDate(__in DukTape::duk_idx_t nObjIdx, __out LPSYSTEMTIME lpSt)
{
  GetDate(lpCtx, nObjIdx, lpSt);
  return;
}

//NOTE: Setting 'var a=0;' can lead to 'a == FALSE'. No idea if JS or DukTape.
//      These methods checks for boolean values first and for integer values
DukTape::duk_int_t CJavascriptVM::GetInt(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nObjIdx)
{
  if (DukTape::duk_is_boolean(lpCtx, nObjIdx))
    return (DukTape::duk_get_boolean(lpCtx, nObjIdx) != 0) ? 1 : 0;
  return DukTape::duk_require_int(lpCtx, nObjIdx);
}

DukTape::duk_int_t CJavascriptVM::GetInt(__in DukTape::duk_idx_t nObjIdx)
{
  return GetInt(lpCtx, nObjIdx);
}

DukTape::duk_uint_t CJavascriptVM::GetUInt(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nObjIdx)
{
  if (DukTape::duk_is_boolean(lpCtx, nObjIdx))
    return (DukTape::duk_get_boolean(lpCtx, nObjIdx) != 0) ? 1 : 0;
  return DukTape::duk_require_uint(lpCtx, nObjIdx);
}

DukTape::duk_uint_t CJavascriptVM::GetUInt(__in DukTape::duk_idx_t nObjIdx)
{
  return GetUInt(lpCtx, nObjIdx);
}

DukTape::duk_double_t CJavascriptVM::GetDouble(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nObjIdx)
{
  if (DukTape::duk_is_boolean(lpCtx, nObjIdx))
    return (DukTape::duk_get_boolean(lpCtx, nObjIdx) != 0) ? 1.0 : 0.0;
  return DukTape::duk_require_number(lpCtx, nObjIdx);
}

DukTape::duk_double_t CJavascriptVM::GetDouble(__in DukTape::duk_idx_t nObjIdx)
{
  return GetDouble(lpCtx, nObjIdx);
}

HRESULT CJavascriptVM::AddSafeString(__inout CStringA &cStrCodeA, __in_z LPCSTR szStrA, __in_opt SIZE_T nStrLen)
{
  LPCSTR szStartA;
  CHAR chA;

  if (nStrLen == (SIZE_T)-1)
    nStrLen = StrLenA(szStrA);
  if (nStrLen == 0)
    return S_OK;
  if (szStrA == NULL)
    return E_POINTER;
  while (nStrLen > 0)
  {
    szStartA = szStrA;
    while (nStrLen > 0 && StrChrA("'\"\\\n\r\t\b\f", *szStrA) == NULL)
    {
      szStrA++;
      nStrLen--;
    }
    if (szStrA > szStartA)
    {
      if (cStrCodeA.ConcatN(szStartA, (SIZE_T)(szStrA-szStartA)) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (nStrLen > 0)
    {
      nStrLen--;
      switch (chA = *szStrA++)
      {
        case '\n':
          chA = 'n';
          break;
        case '\r':
          chA = 'r';
          break;
        case '\t':
          chA = 't';
          break;
        case '\b':
          chA = 'b';
          break;
        case '\f':
          chA = 'f';
          break;
      }
      if (cStrCodeA.ConcatN("\\", 1) == FALSE || cStrCodeA.ConcatN(&chA, 1) == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  //done
  return S_OK;
}

HRESULT CJavascriptVM::AddSafeString(__inout CStringA &cStrCodeA, __in_z LPCWSTR szStrW, __in_opt SIZE_T nStrLen)
{
  MX::CStringA cStrTempA;
  HRESULT hRes;

  if (nStrLen == (SIZE_T)-1)
    nStrLen = StrLenW(szStrW);
  if (nStrLen == 0)
    return S_OK;
  if (szStrW == NULL)
    return E_POINTER;
  hRes = MX::Utf8_Encode(cStrTempA, szStrW, nStrLen);
  if (SUCCEEDED(hRes))
    hRes = AddSafeString(cStrCodeA, (LPCSTR)cStrTempA, cStrTempA.GetLength());
  //done
  return hRes;
}

DukTape::duk_ret_t CJavascriptVM::OnNodeJsResolveModule(__in DukTape::duk_context *lpCtx)
{
  MX::CStringA cStrTempA;
  LPSTR szRequestedIdA = (LPSTR)DukTape::duk_get_string(lpCtx, 0);
  LPCSTR szParentIdA = DukTape::duk_get_string(lpCtx, 1);
  LPSTR p, q, q_last;

  if (szParentIdA == NULL)
    szParentIdA = "";
  if (szRequestedIdA == NULL)
    szRequestedIdA = "";

  //  A few notes on the algorithm:
  //
  //    - Terms are not allowed to begin with a period unless the term
  //      is either '.' or '..'.  This simplifies implementation (and
  //      is within CommonJS modules specification).
  //
  //    - There are few output bound checks here.  This is on purpose:
  //      the resolution input is length checked and the output is never
  //      longer than the input.  The resolved output is written directly
  //      over the input because it's never longer than the input at any
  //      point in the algorithm.
  //
  //    - Non-ASCII characters are processed as individual bytes and
  //      need no special treatment.  However, U+0000 terminates the
  //      algorithm; this is not an issue because U+0000 is not a
  //      desirable term character anyway.

  //  Set up the resolution input which is the requested ID directly
  //  (if absolute or no current module path) or with current module
  //  ID prepended (if relative and current module path exists).
  //
  //  Suppose current module is 'foo/bar' and relative path is './quux'.
  //  The 'bar' component must be replaced so the initial input here is
  //  'foo/bar/.././quux'.
  if (*szParentIdA != 0 && *szRequestedIdA == '.')
  {
    if (cStrTempA.Format("%s/../%s", szParentIdA, szRequestedIdA) == FALSE)
      MX_JS_THROW_HRESULT_ERROR(lpCtx, E_OUTOFMEMORY);
    szRequestedIdA = (LPSTR)cStrTempA;
  }
  else
  {
    if (cStrTempA.Copy(szRequestedIdA) == FALSE)
      MX_JS_THROW_HRESULT_ERROR(lpCtx, E_OUTOFMEMORY);
  }
  szRequestedIdA = (LPSTR)cStrTempA;

  // Resolution loop.  At the top of the loop we're expecting a valid
  // term: '.', '..', or a non-empty identifier not starting with a period.
  p = szRequestedIdA;
  q = szRequestedIdA;
  for (;;)
  {
    DukTape::duk_uint_fast8_t c;

    // Here 'p' always points to the start of a term.
    //
    // We can also unconditionally reset q_last here: if this is
    // the last (non-empty) term q_last will have the right value
    // on loop exit.
    MX_ASSERT(p >= q);  //output is never longer than input during resolution

    q_last = q;
    c = *p++;
    if (c == 0)
    {
      goto resolve_error;
    }
    if (c == '.')
    {
      c = *p++;
      if (c == '/')
        goto eat_dup_slashes; // Term was '.' and is eaten entirely (including dup slashes)

      if (c == '.' && *p == '/')
      {
        // Term was '..', backtrack resolved name by one component.
        // q[-1] = previous slash (or beyond start of buffer)
        // q[-2] = last char of previous component (or beyond start of buffer)
        p++;  // eat (first) input slash

        MX_ASSERT(q >= szRequestedIdA);
        if (q == szRequestedIdA)
          goto resolve_error;

        MX_ASSERT(*(q - 1) == '/');
        q--;  //Backtrack to last output slash (dups already eliminated).
        for (;;)
        {
          //Backtrack to previous slash or start of buffer.
          MX_ASSERT(q >= szRequestedIdA);
          if (q == szRequestedIdA)
            break;
          if (*(q - 1) == '/')
            break;
          q--;
        }
        goto eat_dup_slashes;
      }
      goto resolve_error;
    }
    if (c == '/')
      goto resolve_error; // e.g. require('/foo'), empty terms not allowed

    for (;;)
    {
      //Copy term name until end or '/'
      *q++ = c;
      c = *p++;
      if (c == 0)
        goto loop_done; // This was the last term, and q_last was updated to match this term at loop top.
      if (c == '/')
      {
        *q++ = '/';
        break;
      }
      // write on next loop
    }
eat_dup_slashes:
    while (*p == '/')
      p++;
  }
loop_done:
  MX_ASSERT(q >= szRequestedIdA);
  DukTape::duk_push_lstring(lpCtx, szRequestedIdA, (size_t)(q - szRequestedIdA));
  return 1;

resolve_error:
  MX_JS_THROW_ERROR(lpCtx, DUK_ERR_TYPE_ERROR, "cannot resolve module id: %s", szRequestedIdA);
  return 0;
}

DukTape::duk_ret_t CJavascriptVM::OnNodeJsLoadModule(__in DukTape::duk_context *lpCtx)
{
  //Entry stack: [ resolved_id exports module ]
  CStringW cStrTempW;
  CStringA cStrCodeA;
  LPCSTR szModuleNameA;
  CJavascriptVM *lpVM;
  HRESULT hRes;

  //get virtual machine pointer
  lpVM = MX::CJavascriptVM::FromContext(lpCtx);
  //initialize context
  szModuleNameA = DukTape::duk_require_string(lpCtx, 0);
  hRes = Utf8_Decode(cStrTempW, szModuleNameA);
  if (SUCCEEDED(hRes))
  {
    CRequireModuleContext cContext(lpCtx, (LPCWSTR)cStrTempW, 2, 1);

    //raise callback
    hRes = (lpVM->cRequireModuleCallback) ? (lpVM->cRequireModuleCallback(lpCtx, &cContext, cStrCodeA)) : E_NOTIMPL;
  }
  if (FAILED(hRes))
    MX_JS_THROW_HRESULT_ERROR(lpCtx, hRes);
  if (cStrCodeA.IsEmpty() == FALSE)
    DukTape::duk_push_string(lpCtx, (LPCSTR)cStrCodeA);
  else
    DukTape::duk_push_undefined(lpCtx);
  return 1;
}

DukTape::duk_ret_t CJavascriptVM::_ProxyHasPropHelper(__in DukTape::duk_context *lpCtx)
{
  CProxyCallbacks cProxyCallbacks;
  LPCSTR szObjNameA;
  DukTape::duk_idx_t nObjNameIndex;
  LPVOID p;
  DukTape::duk_size_t nBufSize;
  DukTape::duk_bool_t retVal;
  int nRet;

  //get callbacks
  DukTape::duk_push_this(lpCtx);
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""callbacks");
  p = DukTape::duk_require_buffer(lpCtx, -1, &nBufSize);
  cProxyCallbacks.deserialize(p);
  DukTape::duk_pop(lpCtx);
  //get object's name
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""name");
  szObjNameA = DukTape::duk_require_string(lpCtx, -1);
  //remove "this"
  DukTape::duk_remove(lpCtx, -2);
  //get index of object's name for later removal
  nObjNameIndex = DukTape::duk_normalize_index(lpCtx, -1);
  //if key is a string or a number, raise event, else continue
  nRet = 0;
  if (DukTape::duk_is_string(lpCtx, 1))
  {
    if (cProxyCallbacks.cProxyHasNamedPropertyCallback)
      nRet = cProxyCallbacks.cProxyHasNamedPropertyCallback(lpCtx, szObjNameA, DukTape::duk_get_string(lpCtx, 1));
  }
  else if ((DukTape::duk_is_number(lpCtx, 1) || DukTape::duk_is_boolean(lpCtx, 1)) && !DukTape::duk_is_nan(lpCtx, 1))
  {
    if (cProxyCallbacks.cProxyHasIndexedPropertyCallback)
      nRet = cProxyCallbacks.cProxyHasIndexedPropertyCallback(lpCtx, szObjNameA, DukTape::duk_get_int(lpCtx, 1));
  }
  //remove retrieved object's name from stack
  DukTape::duk_remove(lpCtx, nObjNameIndex);
  //process returned value
  if (nRet == 0)
  {
    retVal = 0;
  }
  else if (nRet > 0)
  {
    retVal = 1;
  }
  else
  {
    //does have property?
    retVal = DukTape::duk_has_prop(lpCtx, 0);
  }
  //done
  DukTape::duk_push_boolean(lpCtx, retVal);
  return 1;
}

DukTape::duk_ret_t CJavascriptVM::_ProxyGetPropHelper(__in DukTape::duk_context *lpCtx)
{
  CProxyCallbacks cProxyCallbacks;
  LPCSTR szObjNameA;
  DukTape::duk_idx_t nObjNameIndex;
  LPVOID p;
  DukTape::duk_size_t nBufSize;
  int nRet;

  //get callbacks
  DukTape::duk_push_this(lpCtx);
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""callbacks");
  p = DukTape::duk_require_buffer(lpCtx, -1, &nBufSize);
  cProxyCallbacks.deserialize(p);
  DukTape::duk_pop(lpCtx);
  //get object's name
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""name");
  szObjNameA = DukTape::duk_require_string(lpCtx, -1);
  //remove "this"
  DukTape::duk_remove(lpCtx, -2);
  //get index of object's name for later removal
  nObjNameIndex = DukTape::duk_normalize_index(lpCtx, -1);
  //if key is a string or a number, raise event, else continue
  nRet = 0;
  if (DukTape::duk_is_string(lpCtx, 1))
  {
    LPCSTR szPropNameA = DukTape::duk_get_string(lpCtx, 1);
    if (cProxyCallbacks.cProxyGetNamedPropertyCallback)
      nRet = cProxyCallbacks.cProxyGetNamedPropertyCallback(lpCtx, szObjNameA, szPropNameA);
    if (nRet < 0)
    {
      //throw error
      MX_JS_THROW_ERROR(lpCtx, DUK_ERR_TYPE_ERROR, "Cannot retrieve value of property '%s'", szPropNameA);
      return DUK_RET_TYPE_ERROR;
    }
  }
  else if ((DukTape::duk_is_number(lpCtx, 1) || DukTape::duk_is_boolean(lpCtx, 1)) && !DukTape::duk_is_nan(lpCtx, 1))
  {
    int nIndex =  DukTape::duk_get_int(lpCtx, 1);
    if (cProxyCallbacks.cProxyGetIndexedPropertyCallback)
      nRet = cProxyCallbacks.cProxyGetIndexedPropertyCallback(lpCtx, szObjNameA, nIndex);
    if (nRet < 0)
    {
      //throw error
      MX_JS_THROW_ERROR(lpCtx, DUK_ERR_TYPE_ERROR, "Cannot retrieve value of property at index #%d", nIndex);
      return DUK_RET_TYPE_ERROR;
    }
  }
  //remove retrieved object's name from stack
  DukTape::duk_remove(lpCtx, nObjNameIndex);
  //process returned value
  if (nRet == 0)
  {
    //get original property's value
    DukTape::duk_dup(lpCtx, 1);
    DukTape::duk_get_prop(lpCtx, 0);
  } //else a value was pushed
  //done
  return 1;
}

DukTape::duk_ret_t CJavascriptVM::_ProxySetPropHelper(__in DukTape::duk_context *lpCtx)
{
  CProxyCallbacks cProxyCallbacks;
  LPCSTR szObjNameA;
  DukTape::duk_idx_t nObjNameIndex;
  LPVOID p;
  DukTape::duk_size_t nBufSize;
  int nRet;

  //get callbacks
  DukTape::duk_push_this(lpCtx);
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""callbacks");
  p = DukTape::duk_require_buffer(lpCtx, -1, &nBufSize);
  cProxyCallbacks.deserialize(p);
  DukTape::duk_pop(lpCtx);
  //get object's name
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""name");
  szObjNameA = DukTape::duk_require_string(lpCtx, -1);
  //remove "this"
  DukTape::duk_remove(lpCtx, -2);
  //get index of object's name for later removal
  nObjNameIndex = DukTape::duk_normalize_index(lpCtx, -1);
  //if key is a string or a number, raise event, else continue
  nRet = 0;
  if (DukTape::duk_is_string(lpCtx, 1))
  {
    LPCSTR szPropNameA = DukTape::duk_get_string(lpCtx, 1);
    if (cProxyCallbacks.cProxySetNamedPropertyCallback)
      nRet = cProxyCallbacks.cProxySetNamedPropertyCallback(lpCtx, szObjNameA, szPropNameA, 2);
    if (nRet < 0)
    {
      //throw error
      MX_JS_THROW_ERROR(lpCtx, DUK_ERR_TYPE_ERROR, "Cannot set value of property '%s'", szPropNameA);
      return DUK_RET_TYPE_ERROR;
    }
  }
  else if ((DukTape::duk_is_number(lpCtx, 1) || DukTape::duk_is_boolean(lpCtx, 1)) && !DukTape::duk_is_nan(lpCtx, 1))
  {
    int nIndex =  DukTape::duk_get_int(lpCtx, 1);
    if (cProxyCallbacks.cProxySetIndexedPropertyCallback)
      nRet = cProxyCallbacks.cProxySetIndexedPropertyCallback(lpCtx, szObjNameA, nIndex, 2);
    if (nRet < 0)
    {
      //throw error
      MX_JS_THROW_ERROR(lpCtx, DUK_ERR_TYPE_ERROR, "Cannot set value of property at index #%d", nIndex);
      return DUK_RET_TYPE_ERROR;
    }
  }
  //remove retrieved object's name from stack
  DukTape::duk_remove(lpCtx, nObjNameIndex);
  //process returned value
  if (nRet == 0)
  {
    //set passed value
    DukTape::duk_dup(lpCtx, 1);
    DukTape::duk_dup(lpCtx, 2);
    DukTape::duk_put_prop(lpCtx, 0);
  }
  else
  {
    //a value was pushed
    DukTape::duk_dup(lpCtx, 1);
    DukTape::duk_swap(lpCtx, -1, -2);
    DukTape::duk_put_prop(lpCtx, 0);
  }
  //done (return success)
  DukTape::duk_push_boolean(lpCtx, 1);
  return 1;
}

DukTape::duk_ret_t CJavascriptVM::_ProxyDeletePropHelper(__in DukTape::duk_context *lpCtx)
{
  CProxyCallbacks cProxyCallbacks;
  LPCSTR szObjNameA;
  DukTape::duk_idx_t nObjNameIndex;
  LPVOID p;
  DukTape::duk_size_t nBufSize;
  int nRet;

  //get callbacks
  DukTape::duk_push_this(lpCtx);
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""callbacks");
  p = DukTape::duk_require_buffer(lpCtx, -1, &nBufSize);
  cProxyCallbacks.deserialize(p);
  DukTape::duk_pop(lpCtx);
  //get object's name
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""name");
  szObjNameA = DukTape::duk_require_string(lpCtx, -1);
  //remove "this"
  DukTape::duk_remove(lpCtx, -2);
  //get index of object's name for later removal
  nObjNameIndex = DukTape::duk_normalize_index(lpCtx, -1);
  //if key is a string or a number, raise event, else continue
  nRet = 1;
  if (DukTape::duk_is_string(lpCtx, 1))
  {
    LPCSTR szPropNameA = DukTape::duk_get_string(lpCtx, 1);
    if (cProxyCallbacks.cProxyDeleteNamedPropertyCallback)
      nRet = cProxyCallbacks.cProxyDeleteNamedPropertyCallback(lpCtx, szObjNameA, szPropNameA);
    if (nRet < 0)
    {
      //throw error
      MX_JS_THROW_ERROR(lpCtx, DUK_ERR_TYPE_ERROR, "Cannot delete property '%s'", szPropNameA);
      return DUK_RET_TYPE_ERROR;
    }
  }
  else if (DukTape::duk_is_number(lpCtx, 1) || DukTape::duk_is_boolean(lpCtx, 1))
  {
    int nIndex =  DukTape::duk_get_int(lpCtx, 1);
    if (cProxyCallbacks.cProxyDeleteIndexedPropertyCallback)
      nRet = cProxyCallbacks.cProxyDeleteIndexedPropertyCallback(lpCtx, szObjNameA, nIndex);
    if (nRet < 0)
    {
      //throw error
      MX_JS_THROW_ERROR(lpCtx, DUK_ERR_TYPE_ERROR, "Cannot delete property at index #%d", nIndex);
      return DUK_RET_TYPE_ERROR;
    }
  }
  //remove retrieved object's name from stack
  DukTape::duk_remove(lpCtx, nObjNameIndex);
  //process returned value
  if (nRet > 0)
  {
    //delete key
    DukTape::duk_dup(lpCtx, 1);
    DukTape::duk_del_prop(lpCtx, 0);
  }
  //done (return success)
  DukTape::duk_push_boolean(lpCtx, 1);
  return 1;
}

DukTape::duk_ret_t CJavascriptVM::_RunNativeProtectedHelper(__in DukTape::duk_context *lpCtx, __in void *udata)
{
  RUN_NATIVE_PROTECTED_DATA *lpData = (RUN_NATIVE_PROTECTED_DATA*)udata;
  DukTape::duk_idx_t nStackTop;

  lpData->func(lpCtx);
  nStackTop = DukTape::duk_get_top(lpCtx);
  if (nStackTop == 0 && lpData->nRetValuesCount == 0)
  {
    DukTape::duk_push_null(lpCtx);
    return 1;
  }
  return (DukTape::duk_ret_t)(lpData->nRetValuesCount);
}

HRESULT CJavascriptVM::GetErrorInfoFromException(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nStackIndex,
                                                 __out_opt CErrorInfo *lpErrorInfo)
{
  static const struct {
    DukTape::duk_uint_t code;
    LPCSTR szMsgA;
    HRESULT hRes;
  } sTypedErrorCodes[] = {
    { DUK_ERR_TYPE_ERROR,      DUK_STR_INVALID_INDEX,        E_INVALIDARG },
    { DUK_ERR_ERROR,           DUK_STR_ALLOC_FAILED,         E_OUTOFMEMORY },
    { DUK_ERR_RANGE_ERROR,     DUK_STR_STRING_TOO_LONG,      E_OUTOFMEMORY },
    { DUK_ERR_RANGE_ERROR,     DUK_STR_BUFFER_TOO_LONG,      E_OUTOFMEMORY },
    { DUK_ERR_ERROR,           DUK_STR_ALLOC_FAILED,         E_OUTOFMEMORY },
    { DUK_ERR_RANGE_ERROR,     DUK_STR_RESULT_TOO_LONG,      E_OUTOFMEMORY },
    { DUK_ERR_RANGE_ERROR,     DUK_STR_NUMBER_OUTSIDE_RANGE, MX_E_ArithmeticOverflow },
  };
  HRESULT hRes = E_FAIL;

  nStackIndex = DukTape::duk_normalize_index(lpCtx, nStackIndex);
  try
  {
    if (DukTape::duk_is_object(lpCtx, nStackIndex) != 0)
    {
      DukTape::duk_errcode_t nErrCode = DukTape::duk_get_error_code(lpCtx, nStackIndex);
      DWORD dwValue;
      LPCSTR sA;
      SIZE_T i;
      BOOL bMemError = FALSE;

      //get message
      DukTape::duk_get_prop_string(lpCtx, nStackIndex, "message");
      sA = DukTape::duk_get_string(lpCtx, -1);
      if (sA == NULL)
        sA = "Unknown";
      //check for typed errors
      if (nErrCode == DUK_ERR_ERROR && sA[0] == 'H' && sA[1] == 'R' && sA[2] == ':')
      {
        dwValue = 0;
        for (i=0; i<8; i++)
        {
          if (sA[i+3] >= L'0' && sA[i+3] <= L'9')
            dwValue |= (DWORD)(sA[i+3] - L'0')       << ((7-i) << 2);
          else if (sA[i+3] >= L'A' && sA[i+3] <= L'F')
            dwValue |= (DWORD)((sA[i+3] - L'A') + 10) << ((7-i) << 2);
          else if (sA[i+3] >= L'a' && sA[i+3] <= L'f')
            dwValue |= (DWORD)((sA[i+3] - L'a') + 10) << ((7-i) << 2);
          else
            break;
        }
        if (i >= 8 && sA[8+3] == 0)
        {
          hRes = (HRESULT)dwValue;
          goto got_hresult;
        }
      }
      for (i=0; i<MX_ARRAYLEN(sTypedErrorCodes); i++)
      {
        if (nErrCode == sTypedErrorCodes[i].code)
        {
          if (sTypedErrorCodes[i].szMsgA == NULL || MX::StrCompareA(sTypedErrorCodes[i].szMsgA, sA) == 0)
          {
            hRes = sTypedErrorCodes[i].hRes;
            break;
          }
        }
      }
got_hresult:
      //store message
      if (lpErrorInfo != NULL)
      {
        if (lpErrorInfo->cStrMessageA.Copy(sA) == FALSE)
          bMemError = TRUE;
      }
      DukTape::duk_pop(lpCtx);
      //get error details
      if (lpErrorInfo != NULL)
      {
        //filename
        DukTape::duk_get_prop_string(lpCtx, nStackIndex, "fileName");
        sA = (DukTape::duk_is_undefined(lpCtx, -1) == false) ? DukTape::duk_safe_to_string(lpCtx, -1) : "";
        if (lpErrorInfo->cStrFileNameA.Copy(sA) == FALSE)
          bMemError = TRUE;
        DukTape::duk_pop(lpCtx);
        //line number
        DukTape::duk_get_prop_string(lpCtx, nStackIndex, "lineNumber");
        sA = (DukTape::duk_is_undefined(lpCtx, -1) == false) ? DukTape::duk_safe_to_string(lpCtx, -1) : "";
        while (*sA >= '0' && *sA <= '9')
        {
          lpErrorInfo->nLine = lpErrorInfo->nLine * 10 + ((ULONG)*sA++ - '0');
        }
        DukTape::duk_pop(lpCtx);
        //stack trace
        DukTape::duk_get_prop_string(lpCtx, nStackIndex, "stack");
        sA = (DukTape::duk_is_undefined(lpCtx, -1) == false) ? DukTape::duk_safe_to_string(lpCtx, -1) : "N/A";
        if (lpErrorInfo->cStrStackTraceA.Copy(sA) == FALSE)
          bMemError = TRUE;
        DukTape::duk_pop(lpCtx);
      }
      //check for memory error
      if (bMemError != FALSE)
        hRes = E_OUTOFMEMORY;
    }
    else if (DukTape::duk_is_number(lpCtx, nStackIndex) != 0)
    {
      switch (DukTape::duk_require_int(lpCtx, nStackIndex))
      {
        case DUK_ERR_RANGE_ERROR:
          hRes = MX_E_ArithmeticOverflow;
          break;
      }
    }
  }
  catch (...)
  {
    if (lpErrorInfo != NULL)
      lpErrorInfo->Clear();
    hRes = MX_E_UnhandledException;
  }
  //done
  if (lpErrorInfo != NULL)
    lpErrorInfo->hRes = hRes;
  return hRes;
}

//-----------------------------------------------------------

CJavascriptVM::CErrorInfo::CErrorInfo()
{
  hRes = S_OK;
  nLine = 0;
  return;
}

VOID CJavascriptVM::CErrorInfo::Clear()
{
  hRes = S_OK;
  cStrMessageA.Empty();
  cStrFileNameA.Empty();
  nLine = 0;
  cStrStackTraceA.Empty();
  return;
}

} //namespace MX

//-----------------------------------------------------------

static DukTape::duk_ret_t OnJsOutputDebugString(__in DukTape::duk_context *lpCtx)
{
  LPCSTR szBufA;

  szBufA = DukTape::duk_require_string(lpCtx, 0);
#ifdef _DEBUG
  if (szBufA != NULL && szBufA[0] != 0)
    MX::DebugPrint("%s\r\n", szBufA);
#endif //_DEBUG
  //done
  return 0;
}

static void* my_duk_alloc_function(void *udata, DukTape::duk_size_t size)
{
  return MX_MALLOC(size);
}

static void* my_duk_realloc_function(void *udata, void *ptr, DukTape::duk_size_t size)
{
  return MX_REALLOC(ptr, size);
}

static void my_duk_free_function(void *udata, void *ptr)
{
  MX_FREE(ptr);
  return;
}

static void my_duk_fatal_function(void *udata, const char *msg)
{
  throw E_FAIL;
}

