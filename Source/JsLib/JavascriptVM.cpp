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
  sLastExecError.nLine = 0;
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
    //add "require" callback
    DukTape::duk_push_global_object(lpCtx);
    DukTape::duk_get_prop_string(lpCtx, -1, "Duktape");
    DukTape::duk_remove(lpCtx, -2);
    DukTape::duk_push_c_function(lpCtx, &CJavascriptVM::OnModSearch, 4);
    DukTape::duk_put_prop_string(lpCtx, -2, "modSearch");
    DukTape::duk_pop(lpCtx);
    //setup DukTape's CommonJS module
    DukTape::duk_module_duktape_init(lpCtx);
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
  sLastExecError.cStrMessageA.Empty();
  sLastExecError.cStrFileNameA.Empty();
  sLastExecError.nLine = 0;
  sLastExecError.cStrStackTraceA.Empty();
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
    DukTape::duk_put_prop_string(lpCtx, -2, "id");
    DukTape::duk_pop_2(lpCtx);
    //run code
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTempA, cStrTempA.GetLength());
    DukTape::duk_eval_raw(lpCtx, szCodeA, 0, DUK_COMPILE_STRLEN | DUK_COMPILE_NOSOURCE);
    return;
  });
  //done
  return hRes;
}

HRESULT CJavascriptVM::RunNativeProtected(__in DukTape::duk_idx_t nArgsCount, __in DukTape::duk_idx_t nRetValuesCount,
                                          __in protected_function func)
{
  RUN_NATIVE_PROTECTED_DATA sData;
  DukTape::duk_idx_t nStackTop;
  HRESULT hRes = S_OK;

  nStackTop = DukTape::duk_get_top(lpCtx);
  sData.func = func;
  sData.nRetValuesCount = nRetValuesCount;
  if (nRetValuesCount == 0)
    nRetValuesCount = 1;
  //do safe call
  try
  {
    if (DukTape::duk_safe_call(lpCtx, &CJavascriptVM::_RunNativeProtectedHelper, &sData, nArgsCount,
                               nRetValuesCount) != DUK_EXEC_SUCCESS)
    {
      GetErrorInfoFromException(-1);
      hRes = sLastExecError.hRes;
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
    hRes = _hRes;
  }
  //done
  return hRes;
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

DukTape::duk_ret_t CJavascriptVM::OnModSearch(__in DukTape::duk_context *lpCtx)
{
  CRequireModuleContext cContext;
  CStringW cStrTempW;
  CStringA cStrCodeA;
  LPCSTR szModuleNameA;
  CJavascriptVM *lpVM;
  HRESULT hRes;

  //get virtual machine pointer
  lpVM = MX::CJavascriptVM::FromContext(lpCtx);
  //initialize context
  cContext.lpCtx = lpCtx;
  szModuleNameA = DukTape::duk_require_string(lpCtx, 0);
  hRes = Utf8_Decode(cStrTempW, szModuleNameA);
  if (FAILED(hRes))
    MX_JS_THROW_HRESULT_ERROR(lpCtx, hRes);
  cContext.szIdW = (LPCWSTR)cStrTempW;
  cContext.nRequireModuleIndex = 1;
  cContext.nExportsObjectIndex = 2;
  cContext.nModuleObjectIndex = 3;
  //raise callback
  hRes = (lpVM->cRequireModuleCallback) ? (lpVM->cRequireModuleCallback(lpCtx, &cContext, cStrCodeA)) : E_NOTIMPL;
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

VOID CJavascriptVM::GetErrorInfoFromException(__in DukTape::duk_idx_t nStackIndex)
{
  static const struct {
    DukTape::duk_uint_t code;
    LPCSTR szMsgA;
    HRESULT hRes;
  } sTypedErrorCodes[] = {
    { DUK_ERR_TYPE_ERROR,      DUK_STR_INVALID_CALL_ARGS,    E_INVALIDARG },
    { DUK_ERR_ERROR,           DUK_STR_ALLOC_FAILED,         E_OUTOFMEMORY },
    { DUK_ERR_RANGE_ERROR,     DUK_STR_STRING_TOO_LONG,      E_OUTOFMEMORY },
    { DUK_ERR_RANGE_ERROR,     DUK_STR_BUFFER_TOO_LONG,      E_OUTOFMEMORY },
    { DUK_ERR_ERROR,           DUK_STR_ALLOC_FAILED,         E_OUTOFMEMORY },
    { DUK_ERR_RANGE_ERROR,     DUK_STR_RESULT_TOO_LONG,      E_OUTOFMEMORY },
    { DUK_ERR_RANGE_ERROR,     DUK_STR_NUMBER_OUTSIDE_RANGE, MX_E_ArithmeticOverflow },
  };

  sLastExecError.hRes = E_FAIL;
  sLastExecError.cStrFileNameA.Empty();
  sLastExecError.nLine = 0;
  sLastExecError.cStrMessageA.Empty();
  sLastExecError.cStrStackTraceA.Empty();

  nStackIndex = DukTape::duk_normalize_index(lpCtx, nStackIndex);
  try
  {
    if (DukTape::duk_is_object(lpCtx, nStackIndex) != 0)
    {
      DukTape::duk_errcode_t nErrCode = DukTape::duk_get_error_code(lpCtx, nStackIndex);
      DWORD dwValue;
      LPCSTR sA;
      SIZE_T i;

      //get message
      DukTape::duk_get_prop_string(lpCtx, nStackIndex, "message");
      sA = (DukTape::duk_is_undefined(lpCtx, -1) == false) ? DukTape::duk_safe_to_string(lpCtx, -1) : "Unknown";
      if (sLastExecError.cStrMessageA.Copy(sA) == FALSE)
      {
err_nomem:
        DukTape::duk_pop(lpCtx);
        sLastExecError.hRes = E_OUTOFMEMORY;
        return;
      }
      DukTape::duk_pop(lpCtx);
      //get filename
      DukTape::duk_get_prop_string(lpCtx, nStackIndex, "fileName");
      sA = (DukTape::duk_is_undefined(lpCtx, -1) == false) ? DukTape::duk_safe_to_string(lpCtx, -1) : "";
      if (sLastExecError.cStrFileNameA.Copy(sA) == FALSE)
        goto err_nomem;
      DukTape::duk_pop(lpCtx);
      //get line number
      DukTape::duk_get_prop_string(lpCtx, nStackIndex, "lineNumber");
      sA = (DukTape::duk_is_undefined(lpCtx, -1) == false) ? DukTape::duk_safe_to_string(lpCtx, -1) : "";
      while (*sA >= '0' && *sA <= '9')
      {
        sLastExecError.nLine = sLastExecError.nLine * 10 + ((ULONG)*sA++ - '0');
      }
      DukTape::duk_pop(lpCtx);
      //stack trace
      DukTape::duk_get_prop_string(lpCtx, nStackIndex, "stack");
      sA = (DukTape::duk_is_undefined(lpCtx, -1) == false) ? DukTape::duk_safe_to_string(lpCtx, -1) : "Unknown";
      if (sLastExecError.cStrStackTraceA.Copy(sA) == FALSE)
        goto err_nomem;
      DukTape::duk_pop(lpCtx);

      //check for typed errors
      sA = (LPCSTR)(sLastExecError.cStrMessageA);
      if (nErrCode == DUK_ERR_ERROR && sA[0] == '\xFF' && sA[1] == '\xFF')
      {
        dwValue = 0;
        for (i=0; i<8; i++)
        {
          if (sA[i+2] >= L'0' && sA[i+2] <= L'9')
            dwValue |= (DWORD) (sA[i+2] - L'0')       << ((7-i) << 2);
          else if (sA[i+2] >= L'A' && sA[i+2] <= L'F')
            dwValue |= (DWORD)((sA[i+2] - L'A') + 10) << ((7-i) << 2);
          else if (sA[i+2] >= L'a' && sA[i+2] <= L'f')
            dwValue |= (DWORD)((sA[i+2] - L'a') + 10) << ((7-i) << 2);
          else
            break;
        }
        if (i >= 8)
          sLastExecError.hRes = (HRESULT)dwValue;
      }
      else
      {
        for (i=0; i<MX_ARRAYLEN(sTypedErrorCodes); i++)
        {
          if (nErrCode == sTypedErrorCodes[i].code)
          {
            if (sTypedErrorCodes[i].szMsgA == NULL ||
                MX::StrCompareA(sTypedErrorCodes[i].szMsgA, sA) == 0)
            {
              sLastExecError.hRes = sTypedErrorCodes[i].hRes;
              break;
            }
          }
        }
      }
    }
    else if (DukTape::duk_is_number(lpCtx, nStackIndex) != 0)
    {
      switch (DukTape::duk_require_int(lpCtx, nStackIndex))
      {
        case DUK_ERR_RANGE_ERROR:
          sLastExecError.hRes = MX_E_ArithmeticOverflow;
          break;
      }
    }
  }
  catch (...)
  {
    sLastExecError.hRes = MX_E_UnhandledException;
    sLastExecError.cStrFileNameA.Empty();
    sLastExecError.nLine = 0;
    sLastExecError.cStrMessageA.Empty();
    sLastExecError.cStrStackTraceA.Empty();
  }
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

