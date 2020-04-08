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
#define _MX_JAVASCRIPT_VM_CPP
#include "JavascriptVMCommon.h"

#define X_BYTE_ENC(_x,_y)   (((UCHAR)(_x)) ^ ((UCHAR)(_y+0x4A)))

//-----------------------------------------------------------

typedef struct {
  MX::CJavascriptVM::lpfnProtectedFunction fnFunc;
  DukTape::duk_idx_t nRetValuesCount;
} RUN_NATIVE_PROTECTED_DATA;

//-----------------------------------------------------------

namespace DukTape {
#ifdef DUK_USE_DEBUG
LONG nDebugLevel = 0;
#endif //DUK_USE_DEBUG
} //namespace DukTape

//-----------------------------------------------------------

#include <BigInteger.min.js.h>

//-----------------------------------------------------------

static DukTape::duk_ret_t OnDebugString(_In_ DukTape::duk_context *lpCtx);
static DukTape::duk_ret_t OnSetUnhandledExceptionHandler(_In_ DukTape::duk_context *lpCtx);
static DukTape::duk_ret_t OnFormatErrorMessage(_In_ DukTape::duk_context *lpCtx);

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

VOID CJavascriptVM::SetRequireModuleCallback(_In_ OnRequireModuleCallback _cRequireModuleCallback)
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
  hRes = RunNativeProtectedAndGetError(0, 0, [this](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_global_stash(lpCtx);
    //setup the global internal object
    DukTape::duk_push_bare_object(lpCtx);
    DukTape::duk_put_prop_string(lpCtx, -2, "mxjslib");

    //get our internal global object
    DukTape::duk_get_prop_string(lpCtx, -1, "mxjslib");

    //setup "jsVM" accessible only from native for callbacks
    DukTape::duk_push_pointer(lpCtx, this);
    DukTape::duk_put_prop_string(lpCtx, -2, "\xff" "jsVM");

    //setup "unhandledExceptionHandler"
    DukTape::duk_push_null(lpCtx);
    DukTape::duk_put_prop_string(lpCtx, -2, "\xff" "unhandledExceptionHandler");

    //done with internal objects
    DukTape::duk_pop_2(lpCtx);

    //setup global functions and methods
    DukTape::duk_push_global_object(lpCtx);

    //add debug output
    DukTape::duk_push_c_function(lpCtx, &OnDebugString, 1);
    DukTape::duk_put_prop_string(lpCtx, -2, "DebugPrint");

#ifdef _DEBUG
    //add IS_DEBUG_BUILD property
    DukTape::duk_push_boolean(lpCtx, 1);
    DukTape::duk_put_prop_string(lpCtx, -2, "IS_DEBUG_BUILD");
#endif //_DEBUG

    //add unhandled exception handler
    DukTape::duk_push_c_function(lpCtx, &OnSetUnhandledExceptionHandler, 1);
    DukTape::duk_put_prop_string(lpCtx, -2, "SetUnhandledExceptionHandler");

    //add windows error message retrieval
    DukTape::duk_push_c_function(lpCtx, &OnFormatErrorMessage, 1);
    DukTape::duk_put_prop_string(lpCtx, -2, "FormatErrorMessage");

    //setup DukTape's Node.js-like module loading framework
    DukTape::duk_push_object(lpCtx);
    DukTape::duk_push_c_function(lpCtx, &CJavascriptVM::OnNodeJsResolveModule, MX_JS_VARARGS);
    DukTape::duk_put_prop_string(lpCtx, -2, "resolve");
    DukTape::duk_push_c_function(lpCtx, &CJavascriptVM::OnNodeJsLoadModule, MX_JS_VARARGS);
    DukTape::duk_put_prop_string(lpCtx, -2, "load");

    DukTape::duk_module_node_init(lpCtx);

    //done with global object
    DukTape::duk_pop(lpCtx);

    //add WindowsError exception
    DukTape::duk_eval_raw(lpCtx, "function WindowsError(_hr) {\r\n"
                                     "this.hr = _hr;\r\n"
                                     "Error.call(this, \"\");\r\n"
                                     "this.message = FormatErrorMessage(_hr);\r\n"
                                     "this.name = \"WindowsError\";\r\n"
                                     "return this; }\r\n"
                                 "WindowsError.prototype = Object.create(Error.prototype);\r\n"
                                 "WindowsError.prototype.constructor=WindowsError;\r\n", 0,
                          DUK_COMPILE_EVAL | DUK_COMPILE_NOSOURCE | DUK_COMPILE_STRLEN | DUK_COMPILE_NOFILENAME);
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

CJavascriptVM* CJavascriptVM::FromContext(_In_ DukTape::duk_context *lpCtx)
{
  CJavascriptVM *lpVM;

  if (lpCtx == NULL)
    return NULL;

  //push our internal global object
  DukTape::duk_push_global_stash(lpCtx);
  DukTape::duk_get_prop_string(lpCtx, -1, "mxjslib");

  //get property
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff" "jsVM");
  lpVM = reinterpret_cast<MX::CJavascriptVM*>(DukTape::duk_to_pointer(lpCtx, -1));

  //done with our internal global object
  DukTape::duk_pop_3(lpCtx);
  return lpVM;
}

VOID CJavascriptVM::Run(_In_z_ LPCSTR szCodeA, _In_opt_z_ LPCWSTR szFileNameW, _In_opt_ BOOL bIgnoreResult)
{
  if (szCodeA == NULL)
    throw CJsWindowsError(E_OUTOFMEMORY);
  if (*szCodeA == 0)
    throw CJsWindowsError(E_INVALIDARG);
  if (lpCtx == NULL)
    throw CJsWindowsError(E_FAIL);
  if (szFileNameW != NULL)
  {
    while (*szFileNameW == L'/')
      szFileNameW++;
  }
  if (szFileNameW == NULL || *szFileNameW == 0)
    szFileNameW = L"main.jss";
  //initialize last execution error
  RunNativeProtected(0, (bIgnoreResult != FALSE) ? 0 : 1,
                     [szFileNameW, szCodeA, this](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    CStringA cStrTempA;
    HRESULT hRes;

    hRes = Utf8_Encode(cStrTempA, szFileNameW);
    if (FAILED(hRes))
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
    //modify require.id to match our passed filename so we have a correct path resolution in 'modSearch'
    DukTape::duk_push_global_object(lpCtx);
    DukTape::duk_get_prop_string(lpCtx, -1, "require");
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTempA, cStrTempA.GetLength());
    DukTape::duk_put_prop_string(lpCtx, -2, "\xff" "moduleId");
    DukTape::duk_pop_2(lpCtx);
    //run code
    DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTempA, cStrTempA.GetLength());
    DukTape::duk_eval_raw(lpCtx, szCodeA, MX::StrLenA(szCodeA), 1 | DUK_COMPILE_NOSOURCE | DUK_COMPILE_SHEBANG);
    return;
  }, TRUE);
  //done
  return;
}

VOID CJavascriptVM::RunNativeProtected(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nArgsCount,
                                       _In_ DukTape::duk_idx_t nRetValuesCount, _In_ lpfnProtectedFunction fnFunc,
                                       _In_opt_ BOOL bCatchUnhandled)
{
  RUN_NATIVE_PROTECTED_DATA sData;

#ifdef DUK_USE_DEBUG
  DukTape::duk_idx_t nBaseIdx, nStackTop;
#endif //DUK_USE_DEBUG
  HRESULT hRes = S_OK;

#ifdef DUK_USE_DEBUG
  nStackTop = DukTape::duk_get_top(lpCtx);
  nBaseIdx = nStackTop - nArgsCount;
#endif //DUK_USE_DEBUG
  sData.fnFunc = fnFunc;
  sData.nRetValuesCount = nRetValuesCount;
  //do safe call
  try
  {
    if (DukTape::duk_safe_call(lpCtx, &CJavascriptVM::_RunNativeProtectedHelper, &sData, nArgsCount,
                               (nRetValuesCount > 0) ? nRetValuesCount : 1) != DUK_EXEC_SUCCESS)
    {
#ifdef DUK_USE_DEBUG
      if (DukTape::nDebugLevel >= 3)
      {
        DukTape::duk_idx_t k, nNewStackTop = DukTape::duk_get_top(lpCtx);

        DebugPrint("DukTape: Exception detected => %lu / %lu / %lu\n", nStackTop, nBaseIdx, nNewStackTop);
        for (k = 0; k < nNewStackTop; k++)
        {
          if (DukTape::duk_get_error_code(lpCtx, k) != 0)
          {
            DukTape::duk_get_prop_string(lpCtx, k, "stack");
            DebugPrint("DukTape:     %lu) %s\n", k, DukTape::duk_safe_to_string(lpCtx, -1));
            DukTape::duk_pop(lpCtx);
          }
        }
      }
#endif //DUK_USE_DEBUG

      if (HandleException(lpCtx, -1, bCatchUnhandled) != FALSE)
      {
        //the unhandled exception handler handled took control and might returned some value
        //because it didn't throw any new exception, assume success
        if (sData.nRetValuesCount == 0)
        {
          //because of the behavior in 'duk_safe_call', we enforce returning at least one result
          //so we can see the error code
          DukTape::duk_pop(lpCtx);
        }
      }
    }
    else if (sData.nRetValuesCount == 0)
    {
      //because of the behavior in 'duk_safe_call', we enforce returning at least one result
      //so we can see the error code
      DukTape::duk_pop(lpCtx);
    }
  }
  catch (CJsError &)
  {
    //DukTape::duk_set_top(lpCtx, nStackTop);
    throw;
  }
  catch (HRESULT hRes)
  {
    //DukTape::duk_set_top(lpCtx, nStackTop);
    throw CJsWindowsError(hRes);
  }
  catch (...)
  {
    //DukTape::duk_set_top(lpCtx, nStackTop);
    throw CJsWindowsError(MX_E_UnhandledException);
  }
  //done
  return;
}

VOID CJavascriptVM::RunNativeProtected(_In_ DukTape::duk_idx_t nArgsCount, _In_ DukTape::duk_idx_t nRetValuesCount,
                                       _In_ lpfnProtectedFunction fnFunc, _In_opt_ BOOL bCatchUnhandled)
{
  RunNativeProtected(lpCtx, nArgsCount, nRetValuesCount, fnFunc, bCatchUnhandled);
  return;
}

HRESULT CJavascriptVM::RunNativeProtectedAndGetError(_In_ DukTape::duk_context *lpCtx,
                                                     _In_ DukTape::duk_idx_t nArgsCount,
                                                     _In_ DukTape::duk_idx_t nRetValuesCount,
                                                     _In_ lpfnProtectedFunction fnFunc, _In_opt_ BOOL bCatchUnhandled)
{
  try
  {
    RunNativeProtected(lpCtx, nArgsCount, nRetValuesCount, fnFunc, bCatchUnhandled);
  }
  catch (CJsWindowsError &e)
  {
    return e.GetHResult();
  }
#pragma warning(disable : 4101)
  catch (CJsError &e)
  {
    return E_FAIL;
  }
#pragma warning(default : 4101)
  return S_OK;
}

HRESULT CJavascriptVM::RunNativeProtectedAndGetError(_In_ DukTape::duk_idx_t nArgsCount,
                                                     _In_ DukTape::duk_idx_t nRetValuesCount,
                                                     _In_ lpfnProtectedFunction fnFunc, _In_opt_ BOOL bCatchUnhandled)
{
  return RunNativeProtectedAndGetError(lpCtx, nArgsCount, nRetValuesCount, fnFunc, bCatchUnhandled);
}

HRESULT CJavascriptVM::RegisterException(_In_z_ LPCSTR szExceptionNameA,
                                         _In_ lpfnThrowExceptionCallback fnThrowExceptionCallback)
{
  EXCEPTION_ITEM sNewItem;

  if (szExceptionNameA == NULL || fnThrowExceptionCallback == NULL)
    return E_POINTER;
  if (*szExceptionNameA == 0)
    return E_INVALIDARG;
  sNewItem.szExceptionNameA = szExceptionNameA;
  sNewItem.fnThrowExceptionCallback = fnThrowExceptionCallback;
  if (aRegisteredExceptionsList.AddElement(&sNewItem) == FALSE)
    return E_OUTOFMEMORY;
  return S_OK;
}

HRESULT CJavascriptVM::UnregisterException(_In_z_ LPCSTR szExceptionNameA)
{
  SIZE_T i, nCount;

  if (szExceptionNameA == NULL)
    return E_POINTER;
  if (*szExceptionNameA == 0)
    return E_INVALIDARG;
  nCount = aRegisteredExceptionsList.GetCount();
  for (i=0; i<nCount; i++)
  {
    if (MX::StrCompareA(aRegisteredExceptionsList[i].szExceptionNameA, szExceptionNameA, TRUE) == 0)
    {
      aRegisteredExceptionsList.RemoveElementAt(i);
      return S_OK;
    }
  }
  return MX_E_NotFound;
}

HRESULT CJavascriptVM::AddNativeFunction(_In_z_ LPCSTR szFuncNameA, _In_ OnNativeFunctionCallback cCallback,
                                         _In_ int nArgsCount)
{
  if (szFuncNameA == NULL || !cCallback)
    return E_POINTER;
  if (*szFuncNameA == 0 || nArgsCount < -1)
    return E_INVALIDARG;
  if (lpCtx == NULL)
    return E_FAIL;
  return Internals::JsLib::AddNativeFunctionCommon(lpCtx, NULL, -1, szFuncNameA, cCallback, nArgsCount);
}

HRESULT CJavascriptVM::AddProperty(_In_z_ LPCSTR szPropertyNameA, _In_opt_ BOOL bInitialValueOnStack,
                                   _In_opt_ int nFlags)
{
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, -1, szPropertyNameA, bInitialValueOnStack, nFlags,
                                             NullCallback(), NullCallback());
}

HRESULT CJavascriptVM::AddStringProperty(_In_z_ LPCSTR szPropertyNameA, _In_z_ LPCSTR szValueA, _In_opt_ int nFlags)
{
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtectedAndGetError(0, 1, [szValueA](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    if (szValueA != NULL)
      DukTape::duk_push_string(lpCtx, szValueA);
    else
      DukTape::duk_push_null(lpCtx);
    return;
  });
  if (FAILED(hRes))
    return hRes;
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                             NullCallback());
}

HRESULT CJavascriptVM::AddStringProperty(_In_z_ LPCSTR szPropertyNameA, _In_z_ LPCWSTR szValueW,
                                         _In_opt_ int nFlags)
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

HRESULT CJavascriptVM::AddBooleanProperty(_In_z_ LPCSTR szPropertyNameA, _In_ BOOL bValue, _In_opt_ int nFlags)
{
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtectedAndGetError(0, 1, [bValue](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_boolean(lpCtx, (bValue != FALSE) ? true : false);
    return;
  });
  if (FAILED(hRes))
    return hRes;
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                             NullCallback());
}

HRESULT CJavascriptVM::AddIntegerProperty(_In_z_ LPCSTR szPropertyNameA, _In_ int nValue, _In_opt_ int nFlags)
{
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtectedAndGetError(0, 1, [nValue](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)nValue);
    return;
  });
  if (FAILED(hRes))
    return hRes;
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                             NullCallback());
}

HRESULT CJavascriptVM::AddNumericProperty(_In_z_ LPCSTR szPropertyNameA, _In_ double nValue, _In_opt_ int nFlags)
{
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtectedAndGetError(0, 1, [nValue](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_number(lpCtx, nValue);
    return;
  });
  if (FAILED(hRes))
    return hRes;
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                             NullCallback());
}

HRESULT CJavascriptVM::AddNullProperty(_In_z_ LPCSTR szPropertyNameA, _In_opt_ int nFlags)
{
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtectedAndGetError(0, 1, [](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_null(lpCtx);
    return;
  });
  if (FAILED(hRes))
    return hRes;
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                              NullCallback());
}

HRESULT CJavascriptVM::AddJsObjectProperty(_In_z_ LPCSTR szPropertyNameA, _In_ CJsObjectBase *lpObject,
                                           _In_opt_ int nFlags)
{
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  hRes = lpObject->PushThis(lpCtx);
  if (SUCCEEDED(hRes))
  {
    hRes = Internals::JsLib::AddPropertyCommon(lpCtx, NULL, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                               NullCallback());
  }
  //done
  return hRes;
}

HRESULT CJavascriptVM::AddPropertyWithCallback(_In_z_ LPCSTR szPropertyNameA,
                                               _In_ OnGetPropertyCallback cGetValueCallback,
                                               _In_opt_ OnSetPropertyCallback cSetValueCallback, _In_opt_ int nFlags)
{
  if (!cGetValueCallback)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  nFlags &= ~PropertyFlagWritable;
  return Internals::JsLib::AddPropertyCommon(lpCtx, NULL, -1, szPropertyNameA, FALSE, nFlags, cGetValueCallback,
                                             cSetValueCallback);
}

HRESULT CJavascriptVM::RemoveProperty(_In_z_ LPCSTR szPropertyNameA)
{
  if (lpCtx == NULL)
    return E_FAIL;
  return Internals::JsLib::RemovePropertyCommon(lpCtx, NULL, szPropertyNameA);
}

HRESULT CJavascriptVM::HasProperty(_In_z_ LPCSTR szPropertyNameA)
{
  if (lpCtx == NULL)
    return E_FAIL;
  return Internals::JsLib::HasPropertyCommon(lpCtx, NULL, szPropertyNameA);
}

VOID CJavascriptVM::PushProperty(_In_z_ LPCSTR szPropertyNameA)
{
  Internals::JsLib::PushPropertyCommon(lpCtx, NULL, szPropertyNameA);
  return;
}

HRESULT CJavascriptVM::CreateObject(_In_z_ LPCSTR szObjectNameA, _In_opt_ CProxyCallbacks *lpCallbacks)
{
  HRESULT hRes;

  if (szObjectNameA == NULL)
    return E_POINTER;
  if (*szObjectNameA == 0)
    return E_INVALIDARG;
  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtectedAndGetError(0, 0, [szObjectNameA, lpCallbacks](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    LPCSTR sA, szObjectNameA_2;
    HRESULT hRes;

    hRes = Internals::JsLib::FindObject(lpCtx, szObjectNameA, TRUE, FALSE); //NOTE: Should the parent be the proxy???
    if (FAILED(hRes))
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
    //if FindObject returned OK, then the object we are trying to create, already exists
    if (hRes == S_OK)
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_AlreadyExists);
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
      DukTape::duk_push_c_function(lpCtx, &CJavascriptVM::_ProxyOwnKeysHelper, 1);
      DukTape::duk_put_prop_string(lpCtx, -2, "ownKeys");
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

HRESULT CJavascriptVM::AddObjectNativeFunction(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szFuncNameA,
                                               _In_ OnNativeFunctionCallback cCallback,
                                               _In_ int nArgsCount)
{
  if (szObjectNameA == NULL || szFuncNameA == NULL || !cCallback)
    return E_POINTER;
  if (*szObjectNameA == 0 || *szFuncNameA == 0 || nArgsCount < -1)
    return E_INVALIDARG;
  if (lpCtx == NULL)
    return E_FAIL;
  return Internals::JsLib::AddNativeFunctionCommon(lpCtx, szObjectNameA, -1, szFuncNameA, cCallback, nArgsCount);
}

HRESULT CJavascriptVM::AddObjectProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA,
                                         _In_opt_ BOOL bInitialValueOnStack, _In_opt_ int nFlags)
{
  if (szObjectNameA == NULL)
    return E_POINTER;
  return Internals::JsLib::AddPropertyCommon(lpCtx, szObjectNameA, -1, szPropertyNameA, bInitialValueOnStack, nFlags,
                                             NullCallback(), NullCallback());
}

HRESULT CJavascriptVM::AddObjectStringProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA,
                                               _In_z_ LPCSTR szValueA, _In_opt_ int nFlags)
{
  HRESULT hRes;

  if (szObjectNameA == NULL)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtectedAndGetError(0, 1, [szValueA](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    if (szValueA != NULL)
      DukTape::duk_push_string(lpCtx, szValueA);
    else
      DukTape::duk_push_null(lpCtx);
    return;
  });
  if (FAILED(hRes))
    return hRes;
  return Internals::JsLib::AddPropertyCommon(lpCtx, szObjectNameA, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                             NullCallback());
}

HRESULT CJavascriptVM::AddObjectStringProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA,
                                               _In_z_ LPCWSTR szValueW, _In_opt_ int nFlags)
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

HRESULT CJavascriptVM::AddObjectBooleanProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA,
                                                _In_ BOOL bValue, _In_opt_ int nFlags)
{
  HRESULT hRes;

  if (szObjectNameA == NULL)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtectedAndGetError(0, 1, [bValue](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_boolean(lpCtx, (bValue != FALSE) ? true : false);
    return;
  });
  if (FAILED(hRes))
    return hRes;
  return Internals::JsLib::AddPropertyCommon(lpCtx, szObjectNameA, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                             NullCallback());
}

HRESULT CJavascriptVM::AddObjectIntegerProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA,
                                                _In_ int nValue, _In_opt_ int nFlags)
{
  HRESULT hRes;

  if (szObjectNameA == NULL)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtectedAndGetError(0, 1, [nValue](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)nValue);
    return;
  });
  if (FAILED(hRes))
    return hRes;
  return Internals::JsLib::AddPropertyCommon(lpCtx, szObjectNameA, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                             NullCallback());
}

HRESULT CJavascriptVM::AddObjectNumericProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA,
                                                _In_ double nValue, _In_opt_ int nFlags)
{
  HRESULT hRes;

  if (szObjectNameA == NULL)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtectedAndGetError(0, 1, [nValue](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_number(lpCtx, nValue);
    return;
  });
  if (FAILED(hRes))
    return hRes;
  return Internals::JsLib::AddPropertyCommon(lpCtx, szObjectNameA, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                             NullCallback());
}

HRESULT CJavascriptVM::AddObjectNullProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA,
                                             _In_opt_ int nFlags)
{
  HRESULT hRes;

  if (szObjectNameA == NULL)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtectedAndGetError(0, 1, [](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_null(lpCtx);
    return;
  });
  if (FAILED(hRes))
    return hRes;
  return Internals::JsLib::AddPropertyCommon(lpCtx, szObjectNameA, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                             NullCallback());
}

HRESULT CJavascriptVM::AddObjectJsObjectProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA,
                                                 _In_ CJsObjectBase *lpObject, _In_opt_ int nFlags)
{
  HRESULT hRes;

  if (szObjectNameA == NULL)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  hRes = lpObject->PushThis(lpCtx);
  if (SUCCEEDED(hRes))
  {
    hRes = Internals::JsLib::AddPropertyCommon(lpCtx, szObjectNameA, -1, szPropertyNameA, TRUE, nFlags, NullCallback(),
                                               NullCallback());
  }
  //done
  return hRes;
}

HRESULT CJavascriptVM::AddObjectPropertyWithCallback(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA,
                                                     _In_ OnGetPropertyCallback cGetValueCallback,
                                                     _In_opt_ OnSetPropertyCallback cSetValueCallback,
                                                     _In_opt_ int nFlags)
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

HRESULT CJavascriptVM::RemoveObjectProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA)
{
  if (szObjectNameA == NULL)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  return Internals::JsLib::RemovePropertyCommon(lpCtx, szObjectNameA, szPropertyNameA);
}

HRESULT CJavascriptVM::HasObjectProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA)
{
  if (szObjectNameA == NULL)
    return E_POINTER;
  if (lpCtx == NULL)
    return E_FAIL;
  return Internals::JsLib::HasPropertyCommon(lpCtx, szObjectNameA, szPropertyNameA);
}

VOID CJavascriptVM::PushObjectProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA)
{
  if (szObjectNameA == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_POINTER);
  Internals::JsLib::PushPropertyCommon(lpCtx, szObjectNameA, szPropertyNameA);
  return;
}

VOID CJavascriptVM::PushString(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCWSTR szStrW, _In_opt_ SIZE_T nStrLen)
{
  MX::CStringA cStrTempA;
  HRESULT hRes;

  hRes = Utf8_Encode(cStrTempA, szStrW, nStrLen);
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  DukTape::duk_push_string(lpCtx, cStrTempA);
  return;
}

VOID CJavascriptVM::PushString(_In_z_ LPCWSTR szStrW, _In_opt_ SIZE_T nStrLen)
{
  PushString(lpCtx, szStrW, nStrLen);
  return;
}

VOID CJavascriptVM::GetString(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nStackIndex,
                              _Inout_ CStringW &cStrW, _In_opt_ BOOL bAppend)
{
  LPCSTR sA;
  HRESULT hRes;

  if (bAppend == FALSE)
    cStrW.Empty();
  sA = DukTape::duk_require_string(lpCtx, nStackIndex);
  hRes = Utf8_Decode(cStrW, sA, (SIZE_T)-1, bAppend);
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  return;
}

VOID CJavascriptVM::GetString(_In_ DukTape::duk_idx_t nStackIndex, _Inout_ CStringW &cStrW, _In_opt_ BOOL bAppend)
{
  GetString(lpCtx, nStackIndex, cStrW, bAppend);
  return;
}

VOID CJavascriptVM::PushDate(_In_ DukTape::duk_context *lpCtx, _In_ LPSYSTEMTIME lpSt, _In_opt_ BOOL bAsUtc)
{
  DukTape::duk_idx_t nObjIdx;

  if (lpSt == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_POINTER);
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

VOID CJavascriptVM::PushDate(_In_ LPSYSTEMTIME lpSt, _In_opt_ BOOL bAsUtc)
{
  PushDate(lpCtx, lpSt, bAsUtc);
  return;
}

VOID CJavascriptVM::GetDate(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nObjIdx, _Out_ LPSYSTEMTIME lpSt)
{
  static WORD wDaysInMonths[12] ={ 31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  static WORD wMonthOffsets[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
  WORD y, w;

  if (lpSt == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_POINTER);
  ::MxMemSet(lpSt, 0, sizeof(SYSTEMTIME));

  nObjIdx = DukTape::duk_normalize_index(lpCtx, nObjIdx);

  if (DukTape::duk_is_object(lpCtx, nObjIdx) == 0)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);

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
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  if (lpSt->wMonth != 2)
  {
    if (lpSt->wDay > wDaysInMonths[lpSt->wMonth - 1])
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  }
  else
  {
    w = ((lpSt->wYear % 400) == 0 || ((lpSt->wYear % 4) == 0 && (lpSt->wYear % 100) != 0)) ? 29 : 28;
    if (lpSt->wDay > w)
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  }
  //calculate day of week
  y = lpSt->wYear - ((lpSt->wMonth < 3) ? 1 : 0);
  lpSt->wDayOfWeek = (lpSt->wYear + lpSt->wYear / 4 - lpSt->wYear / 100 + lpSt->wYear / 400 +
                      wMonthOffsets[lpSt->wMonth - 1] + lpSt->wDay) % 7;
  return;
}

VOID CJavascriptVM::GetDate(_In_ DukTape::duk_idx_t nObjIdx, _Out_ LPSYSTEMTIME lpSt)
{
  GetDate(lpCtx, nObjIdx, lpSt);
  return;
}

//NOTE: Setting 'var a=0;' can lead to 'a == FALSE'. No idea if JS or DukTape.
//      These methods checks for boolean values first and for integer values
DukTape::duk_int_t CJavascriptVM::GetInt(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nObjIdx)
{
  if (DukTape::duk_is_boolean(lpCtx, nObjIdx))
    return (DukTape::duk_get_boolean(lpCtx, nObjIdx) != 0) ? 1 : 0;
  return DukTape::duk_require_int(lpCtx, nObjIdx);
}

DukTape::duk_int_t CJavascriptVM::GetInt(_In_ DukTape::duk_idx_t nObjIdx)
{
  return GetInt(lpCtx, nObjIdx);
}

DukTape::duk_uint_t CJavascriptVM::GetUInt(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nObjIdx)
{
  if (DukTape::duk_is_boolean(lpCtx, nObjIdx))
    return (DukTape::duk_get_boolean(lpCtx, nObjIdx) != 0) ? 1 : 0;
  return DukTape::duk_require_uint(lpCtx, nObjIdx);
}

DukTape::duk_uint_t CJavascriptVM::GetUInt(_In_ DukTape::duk_idx_t nObjIdx)
{
  return GetUInt(lpCtx, nObjIdx);
}

DukTape::duk_double_t CJavascriptVM::GetDouble(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nObjIdx)
{
  if (DukTape::duk_is_boolean(lpCtx, nObjIdx))
    return (DukTape::duk_get_boolean(lpCtx, nObjIdx) != 0) ? 1.0 : 0.0;
  return DukTape::duk_require_number(lpCtx, nObjIdx);
}

DukTape::duk_double_t CJavascriptVM::GetDouble(_In_ DukTape::duk_idx_t nObjIdx)
{
  return GetDouble(lpCtx, nObjIdx);
}

VOID CJavascriptVM::GetObjectType(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nObjIdx,
                                  _Out_ MX::CStringA &cStrTypeA)
{
  LPCSTR sA;
  SIZE_T nLen;
  HRESULT hRes = E_INVALIDARG;

  nObjIdx = DukTape::duk_normalize_index(lpCtx, nObjIdx);

  DukTape::duk_eval_raw(lpCtx, "Object.prototype.toString", 0, 0 | DUK_COMPILE_EVAL | DUK_COMPILE_NOSOURCE |
                                                               DUK_COMPILE_STRLEN | DUK_COMPILE_NOFILENAME);
  DukTape::duk_dup(lpCtx, nObjIdx);
  DukTape::duk_call_method(lpCtx, 0);
  sA = DukTape::duk_require_string(lpCtx, -1);
  if (StrNCompareA(sA, "[object ", 8) == 0)
  {
    sA += 8;
    for (nLen = 0; sA[nLen] != 0 && sA[nLen] != ']'; nLen++);
    hRes = (cStrTypeA.CopyN(sA, nLen) != FALSE) ? S_OK : E_OUTOFMEMORY;
  }
  DukTape::duk_pop(lpCtx);
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  return;
}

VOID CJavascriptVM::GetObjectType(_In_ DukTape::duk_idx_t nObjIdx, _Out_ MX::CStringA &cStrTypeA)
{
  GetObjectType(lpCtx, nObjIdx, cStrTypeA);
  return;
}

HRESULT CJavascriptVM::Reset()
{
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtectedAndGetError(0, 0, [](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_global_stash(lpCtx);
    DukTape::duk_push_bare_object(lpCtx);
    DukTape::duk_put_prop_string(lpCtx, -2, "\xff" "requireCache");
    DukTape::duk_pop(lpCtx);

    //reset unhandled exception handler
    DukTape::duk_push_global_stash(lpCtx);
    DukTape::duk_get_prop_string(lpCtx, -1, "mxjslib");
    if (DukTape::duk_is_bare_object(lpCtx, -1) != 0)
    {
      DukTape::duk_push_null(lpCtx);
      DukTape::duk_put_prop_string(lpCtx, -2, "\xff" "unhandledExceptionHandler");
    }
    DukTape::duk_pop_2(lpCtx);

    //called twice. see: https://duktape.org/api.html#duk_gc
    DukTape::duk_gc(lpCtx, 0);
    DukTape::duk_gc(lpCtx, 0);
    return;
  });
  return hRes;
}

HRESULT CJavascriptVM::RunGC()
{
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;
  hRes = RunNativeProtectedAndGetError(0, 0, [](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    //called twice. see: https://duktape.org/api.html#duk_gc
    DukTape::duk_gc(lpCtx, 0);
    DukTape::duk_gc(lpCtx, 0);
    return;
  });
  return hRes;
}

HRESULT CJavascriptVM::AddSafeString(_Inout_ CStringA &cStrCodeA, _In_z_ LPCSTR szStrA, _In_opt_ SIZE_T nStrLen)
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

HRESULT CJavascriptVM::AddSafeString(_Inout_ CStringA &cStrCodeA, _In_z_ LPCWSTR szStrW, _In_opt_ SIZE_T nStrLen)
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

VOID CJavascriptVM::ThrowWindowsError(_In_ DukTape::duk_context *lpCtx, _In_ HRESULT hr, _In_opt_ LPCSTR filename,
                                      _In_opt_ DukTape::duk_int_t line)
{
  DukTape::duk_get_global_string(lpCtx, "WindowsError");
  DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)hr);
  DukTape::duk_new(lpCtx, 1);

  if (filename != NULL && *filename != 0)
    DukTape::duk_push_string(lpCtx, filename);
  else
    DukTape::duk_push_undefined(lpCtx);
  DukTape::duk_put_prop_string(lpCtx, -2, "fileName");

  if (filename != NULL && *filename != 0 && line != 0)
    DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)line);
  else
    DukTape::duk_push_undefined(lpCtx);
  DukTape::duk_put_prop_string(lpCtx, -2, "lineNumber");

  DukTape::duk_throw_raw(lpCtx);
  return;
}

HRESULT CJavascriptVM::AddBigIntegerSupport(_In_ DukTape::duk_context *lpCtx)
{
  return RunNativeProtectedAndGetError(lpCtx, 0, 0, [](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_global_object(lpCtx);
    DukTape::duk_get_prop_string(lpCtx, -1, "BigInteger");
    if (DukTape::duk_is_undefined(lpCtx, -1) != 0)
    {
      CSecureStringA cStrTempA;
      LPSTR sA;
      SIZE_T i;

      DukTape::duk_pop(lpCtx); //pop undefined

      //generate code
      if (cStrTempA.EnsureBuffer(sizeof(aBigIntegerJs) + 1) == FALSE)
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);

      sA = (LPSTR)cStrTempA;
      for (i = 0; i < sizeof(aBigIntegerJs); i++)
        sA[i] = X_BYTE_ENC(aBigIntegerJs[i], i);
      sA[i] = 0;
      cStrTempA.Refresh();
      if (cStrTempA.InsertN("(function (undefined) {\r\n", 0, 25) == FALSE ||
          cStrTempA.ConcatN("return bigInt; })()\r\n", 20) == FALSE)
      {
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
      }

      //run code
      DukTape::duk_eval_raw(lpCtx, (LPCSTR)cStrTempA, 0, DUK_COMPILE_EVAL | DUK_COMPILE_NOSOURCE | DUK_COMPILE_STRLEN |
                                                         DUK_COMPILE_NOFILENAME);
      DukTape::duk_put_prop_string(lpCtx, -2, "BigInteger");

      DukTape::duk_pop(lpCtx); //pop global object
    }
    else
    {
      DukTape::duk_pop_2(lpCtx); //pop BigInteger and global object
    }
    return;
  });
}

HRESULT CJavascriptVM::AddBigIntegerSupport()
{
  return AddBigIntegerSupport(lpCtx);
}

DukTape::duk_ret_t CJavascriptVM::OnNodeJsResolveModule(_In_ DukTape::duk_context *lpCtx)
{
  MX::CStringA cStrTempA;
  LPCSTR szRequestedIdA, szParentIdA;
  LPSTR p, q, q_last;

  szParentIdA = DukTape::duk_get_string(lpCtx, 1);
  if (szParentIdA == NULL)
    szParentIdA = "";

  szRequestedIdA = (LPCSTR)DukTape::duk_get_string(lpCtx, 0);
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
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
  }
  else
  {
    if (cStrTempA.Copy(szRequestedIdA) == FALSE)
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
  }

  // Resolution loop.  At the top of the loop we're expecting a valid
  // term: '.', '..', or a non-empty identifier not starting with a period.
  p = q = (LPSTR)cStrTempA;
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

        MX_ASSERT(q >= (LPSTR)cStrTempA);
        if (q == (LPSTR)cStrTempA)
          goto resolve_error;

        MX_ASSERT(*(q - 1) == '/');
        q--;  //Backtrack to last output slash (dups already eliminated).
        for (;;)
        {
          //Backtrack to previous slash or start of buffer.
          MX_ASSERT(q >= (LPSTR)cStrTempA);
          if (q == (LPSTR)cStrTempA)
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
  p = (LPSTR)cStrTempA;
  MX_ASSERT(q >= p);
  DukTape::duk_push_lstring(lpCtx, p, (size_t)(q - p));
  return 1;

resolve_error:
  MX_JS_THROW_ERROR(lpCtx, DUK_ERR_TYPE_ERROR, "cannot resolve module id: %s", (LPCSTR)cStrTempA);
  return 0;
}

DukTape::duk_ret_t CJavascriptVM::OnNodeJsLoadModule(_In_ DukTape::duk_context *lpCtx)
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
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  if (cStrCodeA.IsEmpty() == FALSE)
    DukTape::duk_push_string(lpCtx, (LPCSTR)cStrCodeA);
  else
    DukTape::duk_push_undefined(lpCtx);
  return 1;
}

DukTape::duk_ret_t CJavascriptVM::_ProxyHasPropHelper(_In_ DukTape::duk_context *lpCtx)
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

DukTape::duk_ret_t CJavascriptVM::_ProxyGetPropHelper(_In_ DukTape::duk_context *lpCtx)
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

DukTape::duk_ret_t CJavascriptVM::_ProxySetPropHelper(_In_ DukTape::duk_context *lpCtx)
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

DukTape::duk_ret_t CJavascriptVM::_ProxyDeletePropHelper(_In_ DukTape::duk_context *lpCtx)
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

DukTape::duk_ret_t CJavascriptVM::_ProxyOwnKeysHelper(_In_ DukTape::duk_context *lpCtx)
{
  CProxyCallbacks cProxyCallbacks;
  LPCSTR szObjNameA;
  LPVOID p;
  DukTape::duk_size_t nBufSize;
  int nIndex;

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
  //create an array
  DukTape::duk_push_object(lpCtx);
  if (cProxyCallbacks.cProxyGetPropertyNameCallback)
  {
    for (nIndex = 0; ; nIndex++)
    {
      LPCSTR szPropNameA = cProxyCallbacks.cProxyGetPropertyNameCallback(lpCtx, szObjNameA, nIndex);
      if (szPropNameA == NULL || *szPropNameA == 0)
        break;
      DukTape::duk_push_string(lpCtx, szPropNameA);
      DukTape::duk_put_prop_index(lpCtx, -2, (DukTape::duk_uarridx_t)nIndex);
    }
  }
  //done (return array)
  return 1;
}

DukTape::duk_ret_t CJavascriptVM::_RunNativeProtectedHelper(_In_ DukTape::duk_context *lpCtx, _In_ void *udata)
{
  RUN_NATIVE_PROTECTED_DATA *lpData = (RUN_NATIVE_PROTECTED_DATA*)udata;
  DukTape::duk_idx_t nStackTop;

  lpData->fnFunc(lpCtx);
  nStackTop = DukTape::duk_get_top(lpCtx);
  if (nStackTop == 0 && lpData->nRetValuesCount == 0)
  {
    DukTape::duk_push_null(lpCtx);
    return 1;
  }
  return (DukTape::duk_ret_t)(lpData->nRetValuesCount);
}

BOOL CJavascriptVM::HandleException(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nStackIndex,
                                    _In_opt_ BOOL bCatchUnhandled)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  static const struct {
    DukTape::duk_uint_t code;
    LPCSTR szMsgA;
    HRESULT hRes;
  } sTypedErrorCodes[] = {
    { DUK_ERR_TYPE_ERROR,      DUK_STR_INVALID_INDEX,        E_INVALIDARG },
    { DUK_ERR_ERROR,           DUK_STR_ALLOC_FAILED,         E_OUTOFMEMORY },
    { DUK_ERR_RANGE_ERROR,     DUK_STR_STRING_TOO_LONG,      MX_E_BufferOverflow },
    { DUK_ERR_RANGE_ERROR,     DUK_STR_BUFFER_TOO_LONG,      MX_E_BufferOverflow },
    { DUK_ERR_ERROR,           DUK_STR_ALLOC_FAILED,         E_OUTOFMEMORY },
    { DUK_ERR_RANGE_ERROR,     DUK_STR_RESULT_TOO_LONG,      MX_E_ArithmeticOverflow },
    { DUK_ERR_RANGE_ERROR,     DUK_STR_NUMBER_OUTSIDE_RANGE, MX_E_ArithmeticOverflow },
  };

  nStackIndex = DukTape::duk_normalize_index(lpCtx, nStackIndex);
  try
  {
    LPCSTR sA;
    SIZE_T i;
    HRESULT hRes;

    //check if there is an unhandled exception handler
    if (bCatchUnhandled != FALSE && DukTape::duk_is_object(lpCtx, nStackIndex) != 0)
    {
      DukTape::duk_get_prop_string(lpCtx, nStackIndex, "\xff""\xff""nonCatcheable");
      if (DukTape::duk_get_boolean(lpCtx, -1) != 0)
      {
        bCatchUnhandled = FALSE;
      }
      DukTape::duk_pop(lpCtx);
    }
    if (bCatchUnhandled != FALSE)
    {
      //push our internal global object
      DukTape::duk_push_global_stash(lpCtx);
      DukTape::duk_get_prop_string(lpCtx, -1, "mxjslib");

      //get handler
      if (DukTape::duk_is_bare_object(lpCtx, -1) != 0)
        DukTape::duk_get_prop_string(lpCtx, -1, "\xff" "unhandledExceptionHandler");
      else
        DukTape::duk_push_null(lpCtx);

      //done with internal global object
      DukTape::duk_remove(lpCtx, -2); //mxjslib
      DukTape::duk_remove(lpCtx, -2); //global stash

      //call the handler
      if (DukTape::duk_is_function(lpCtx, -1) != 0)
      {
        DukTape::duk_dup(lpCtx, nStackIndex);
        if (DukTape::duk_pcall(lpCtx, 1) == DUK_EXEC_SUCCESS)
        {
          return TRUE;
        }
        //the returned value/error object will be the new exception
        nStackIndex = DukTape::duk_normalize_index(lpCtx, -1);
      }
      else
      {
        DukTape::duk_pop(lpCtx);
      }
    }
    //----
    if (DukTape::duk_is_object(lpCtx, nStackIndex) != 0)
    {
      //check for registered exceptions
      if (lpJVM != NULL)
      {
        i = lpJVM->aRegisteredExceptionsList.GetCount();
        while (i > 0)
        {
          i--;
          DukTape::duk_get_global_string(lpCtx, lpJVM->aRegisteredExceptionsList[i].szExceptionNameA);
          if (DukTape::duk_instanceof(lpCtx, nStackIndex, -1) != 0)
          {
            lpJVM->aRegisteredExceptionsList[i].fnThrowExceptionCallback(lpCtx, nStackIndex);
            break; //should be unreachable
          }
        }
      }
      //check for Windows exception
      DukTape::duk_get_global_string(lpCtx, "WindowsError");
      if (DukTape::duk_instanceof(lpCtx, nStackIndex, -1) != 0)
      {
        throw CJsWindowsError(lpCtx, nStackIndex);
      }
      DukTape::duk_pop(lpCtx);

      //check for typed errors
      DukTape::duk_get_prop_string(lpCtx, nStackIndex, "message");
      sA = DukTape::duk_get_string(lpCtx, -1);
      if (sA != NULL)
      {
        DukTape::duk_errcode_t nErrCode = DukTape::duk_get_error_code(lpCtx, nStackIndex);
        for (i=0; i<MX_ARRAYLEN(sTypedErrorCodes); i++)
        {
          if (nErrCode == sTypedErrorCodes[i].code)
          {
            if (sTypedErrorCodes[i].szMsgA == NULL || MX::StrCompareA(sTypedErrorCodes[i].szMsgA, sA) == 0)
            {
              throw CJsWindowsError(sTypedErrorCodes[i].hRes, lpCtx, nStackIndex);
              break; //should be unreachable
            }
          }
        }
      }

      //if we reach here, we have a standard error object
      throw CJsError(lpCtx, nStackIndex);
    }
    //if the thrown item is a number, assume it is a HRESULT error
    if (DukTape::duk_is_number(lpCtx, nStackIndex) != 0)
    {
      hRes = (HRESULT)(DukTape::duk_to_int32(lpCtx, nStackIndex));
      throw CJsWindowsError(hRes, lpCtx, __FILE__, __LINE__);
    }
    sA = DukTape::duk_to_string(lpCtx, nStackIndex);
    throw CJsError(sA, lpCtx, __FILE__, __LINE__);
  }
  catch (CJsError &)
  {
    throw;
  }
  catch (...)
  {
    throw CJsWindowsError(MX_E_UnhandledException, NULL, 0);
  }
  //done
  return FALSE;
}

} //namespace MX

//-----------------------------------------------------------

static DukTape::duk_ret_t OnDebugString(_In_ DukTape::duk_context *lpCtx)
{
  LPCSTR szBufA;

  szBufA = DukTape::duk_require_string(lpCtx, 0);
  if (szBufA != NULL && *szBufA != 0)
    MX::DebugPrint("%s\r\n", szBufA);
  //done
  return 0;
}

static DukTape::duk_ret_t OnSetUnhandledExceptionHandler(_In_ DukTape::duk_context *lpCtx)
{
  LPVOID func = 0;

  //push our internal global object
  DukTape::duk_push_global_stash(lpCtx);
  DukTape::duk_get_prop_string(lpCtx, -1, "mxjslib");

  //push the new handler
  if (DukTape::duk_is_undefined(lpCtx, 0) == 0 && DukTape::duk_is_null(lpCtx, 0) == 0)
  {
    DukTape::duk_require_function(lpCtx, 0);
    DukTape::duk_dup(lpCtx, 0);
  }
  else
  {
    DukTape::duk_push_null(lpCtx);
  }

  //set new handler
  DukTape::duk_put_prop_string(lpCtx, -2, "\xff" "unhandledExceptionHandler");

  //done with internal global object
  DukTape::duk_pop_2(lpCtx);
  return 0;
}

static DukTape::duk_ret_t OnFormatErrorMessage(_In_ DukTape::duk_context *lpCtx)
{
  LPWSTR szBufW;
  HRESULT hRes;
  DWORD dwErr;

  hRes = (HRESULT)(MX::CJavascriptVM::GetInt(lpCtx, 0));
  dwErr = (DWORD)hRes;
  if ((dwErr & 0xFFFF0000) == 0x80070000)
    dwErr &= 0x0000FFFF;
  if (::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwErr, 0, (LPWSTR)&szBufW,
                       1024, NULL) > 0)
  {
    MX::CStringA cStrTempA;

    if (SUCCEEDED(MX::Utf8_Encode(cStrTempA, szBufW)))
    {
      ::LocalFree((HLOCAL)szBufW);
      DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTempA, cStrTempA.GetLength());
      return 1;
    }
    ::LocalFree((HLOCAL)szBufW);
  }
  DukTape::duk_push_sprintf(lpCtx, "0x%08X", hRes);
  return 1;
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
