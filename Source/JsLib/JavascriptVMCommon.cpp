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

namespace Internals {

namespace JsLib {

HRESULT AddNativeFunctionCommon(_In_ DukTape::duk_context *lpCtx, _In_opt_z_ LPCSTR szObjectNameA,
                                _In_ DukTape::duk_idx_t nObjectIndex, _In_z_ LPCSTR szFunctionNameA,
                                _In_ MX::CJavascriptVM::OnNativeFunctionCallback cNativeFunctionCallback,
                                _In_ int nArgsCount)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  HRESULT hRes;

  if (szFunctionNameA == NULL)
    return E_POINTER;
  if (*szFunctionNameA == 0)
    return E_INVALIDARG;
  if (szObjectNameA != NULL && *szObjectNameA == 0)
    return E_INVALIDARG;
  if (lpCtx == NULL)
    return E_FAIL;

  hRes = lpJVM->RunNativeProtectedAndGetError(0, 0, [=](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_c_function fnNativeFunc;
    HRESULT hRes;
    LPVOID p;

    //push object
    if (nObjectIndex >= 0)
    {
      DukTape::duk_dup(lpCtx, nObjectIndex);
    }
    else
    {
      hRes = FindObject(lpCtx, szObjectNameA, FALSE, TRUE);
      if (FAILED(hRes))
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
    }

    fnNativeFunc = [](_In_ DukTape::duk_context *lpCtx) mutable -> DukTape::duk_ret_t
    {
      MX::CJavascriptVM::OnNativeFunctionCallback cCallback;
      MX::CJavascriptVM *lpVM;
      MX::CStringA cStrObjNameA, cStrFuncNameA;
      DukTape::duk_size_t nBufSize;
      LPCSTR sA;
      LPVOID p;

      //get virtual machine pointer
      lpVM = MX::CJavascriptVM::FromContext(lpCtx);
      //get function's name
      DukTape::duk_push_current_function(lpCtx);
      DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""name");
      sA = DukTape::duk_require_string(lpCtx, -1);
      if (cStrFuncNameA.Copy(sA) == FALSE)
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
      DukTape::duk_pop(lpCtx);
      //get function's callback
      DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""callback");
      p = DukTape::duk_require_buffer(lpCtx, -1, &nBufSize);
      MX_ASSERT(p != NULL);
      MX_ASSERT(nBufSize == MX::CJavascriptVM::OnNativeFunctionCallback::serialization_buffer_size());
      cCallback.deserialize(p);
      DukTape::duk_pop_2(lpCtx);
      //push object/global object
      DukTape::duk_push_this(lpCtx);
      //retrieve object's name (undefined if global object)
      if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
      {
        DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""name");
        if (DukTape::duk_is_string(lpCtx, -1) != 0)
        {
          if (cStrObjNameA.Copy(DukTape::duk_get_string(lpCtx, -1)) == FALSE)
            MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
        }
        DukTape::duk_pop_2(lpCtx);
      }
      else
      {
        DukTape::duk_pop(lpCtx);
      }
      //call callback
      sA = (LPCSTR)cStrObjNameA;
      return cCallback(lpCtx, (*sA != 0) ? sA : NULL, (LPCSTR)cStrFuncNameA);
    };

    //create c function callback
    DukTape::duk_push_c_function(lpCtx, fnNativeFunc, (DukTape::duk_int_t)nArgsCount);
    //store callback
    p = DukTape::duk_push_fixed_buffer(lpCtx, MX::CJavascriptVM::OnNativeFunctionCallback::
                                              serialization_buffer_size());
    const_cast<MX::CJavascriptVM::OnNativeFunctionCallback&>(cNativeFunctionCallback).serialize(p);
    DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""callback");
    //store function's name
    DukTape::duk_push_string(lpCtx, szFunctionNameA);
    DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""name");
    //add function to object
    DukTape::duk_put_prop_string(lpCtx, -2, szFunctionNameA);
    //pop the object/global object
    DukTape::duk_pop(lpCtx);
    return;
  });
  //done
  return hRes;
}

HRESULT AddPropertyCommon(_In_ DukTape::duk_context *lpCtx, _In_opt_z_ LPCSTR szObjectNameA,
                          _In_ DukTape::duk_idx_t nObjectIndex, _In_z_ LPCSTR szPropertyNameA,
                          _In_ BOOL bInitialValueOnStack, _In_ CJavascriptVM::ePropertyFlags nFlags,
                          _In_ MX::CJavascriptVM::OnGetPropertyCallback cGetValueCallback,
                          _In_ MX::CJavascriptVM::OnSetPropertyCallback cSetValueCallback)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  HRESULT hRes;

  if (szPropertyNameA == NULL)
    return E_POINTER;
  if (*szPropertyNameA == 0)
    return E_INVALIDARG;
  if (szObjectNameA != NULL && *szObjectNameA == 0)
    return E_INVALIDARG;
  if (lpCtx == NULL)
    return E_FAIL;
  //properties with callbacks cannot have an initial value
  MX_ASSERT(bInitialValueOnStack ? ((!cGetValueCallback) && (!cSetValueCallback)) : true);

  hRes = lpJVM->RunNativeProtectedAndGetError((bInitialValueOnStack != FALSE) ? 1 : 0, 0,
                                              [&](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_uint_t nDukFlags = DUK_DEFPROP_HAVE_ENUMERABLE | DUK_DEFPROP_HAVE_CONFIGURABLE;
    DukTape::duk_idx_t nIndex;
    HRESULT hRes;

    //push object
    if (nObjectIndex >= 0)
    {
      DukTape::duk_dup(lpCtx, nObjectIndex);
    }
    else
    {
      hRes = FindObject(lpCtx, szObjectNameA, FALSE, TRUE);
      if (FAILED(hRes))
        MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
    }
    //set property name
    DukTape::duk_push_string(lpCtx, szPropertyNameA);
    //stack at -1 contains property name, at -2 contains the object
    nIndex = -2;
    if (bInitialValueOnStack != FALSE)
    {
      //stack at '-3' contains the value, move it at the top
      DukTape::duk_swap(lpCtx, -3, -1);
      DukTape::duk_swap(lpCtx, -3, -2);
      nDukFlags |= DUK_DEFPROP_HAVE_VALUE;
      nIndex--;
    }
    if ((!cGetValueCallback) && (!cSetValueCallback))
    {
      nDukFlags |= DUK_DEFPROP_HAVE_WRITABLE;
      if ((nFlags & MX::CJavascriptVM::ePropertyFlags::Writable) != (MX::CJavascriptVM::ePropertyFlags)0)
        nDukFlags |= DUK_DEFPROP_WRITABLE;
    }
    if ((nFlags & MX::CJavascriptVM::ePropertyFlags::Enumerable) != (MX::CJavascriptVM::ePropertyFlags)0)
      nDukFlags |= DUK_DEFPROP_ENUMERABLE;
    if ((nFlags & MX::CJavascriptVM::ePropertyFlags::Configurable) != (MX::CJavascriptVM::ePropertyFlags)0)
      nDukFlags |= DUK_DEFPROP_CONFIGURABLE;
    //create lambda for property getter
    if (cGetValueCallback)
    {
      DukTape::duk_c_function fnGetter;
      LPVOID p;

      fnGetter = [](_In_ DukTape::duk_context *lpCtx) mutable -> DukTape::duk_ret_t
      {
        MX::CJavascriptVM::OnGetPropertyCallback cCallback;
        MX::CJavascriptVM *lpVM;
        MX::CStringA cStrObjNameA, cStrPropNameA;
        DukTape::duk_size_t nBufSize;
        LPCSTR sA;
        LPVOID p;

        //get virtual machine pointer
        lpVM = MX::CJavascriptVM::FromContext(lpCtx);
        //get getter function's key name
        DukTape::duk_push_current_function(lpCtx);
        DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""name");
        sA = DukTape::duk_require_string(lpCtx, -1);
        if (cStrPropNameA.Copy(sA) == FALSE)
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
        DukTape::duk_pop(lpCtx);
        //get function's callback
        DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""callback");
        p = DukTape::duk_require_buffer(lpCtx, -1, &nBufSize);
        MX_ASSERT(p != NULL);
        MX_ASSERT(nBufSize == MX::CJavascriptVM::OnGetPropertyCallback::serialization_buffer_size());
        cCallback.deserialize(p);
        DukTape::duk_pop_2(lpCtx);
        //push object/global object
        DukTape::duk_push_this(lpCtx);
        //retrieve object's name (undefined if global object)
        if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
        {
          DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""name");
          if (DukTape::duk_is_string(lpCtx, -1) != 0)
          {
            if (cStrObjNameA.Copy(DukTape::duk_get_string(lpCtx, -1)) == FALSE)
              MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          }
          DukTape::duk_pop_2(lpCtx);
        }
        else
        {
          DukTape::duk_pop(lpCtx);
        }
        //call callback
        sA = (LPCSTR)cStrObjNameA;
        return cCallback(lpCtx, (*sA != 0) ? sA : NULL, (LPCSTR)cStrPropNameA);
      };

      //create c function callback
      DukTape::duk_push_c_function(lpCtx, fnGetter, 0);
      //store callback
      p = DukTape::duk_push_fixed_buffer(lpCtx, MX::CJavascriptVM::OnGetPropertyCallback::serialization_buffer_size());
      cGetValueCallback.serialize(p);
      DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""callback");
      //store function's name
      DukTape::duk_push_string(lpCtx, szPropertyNameA);
      DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""name");
      //----
      nDukFlags |= DUK_DEFPROP_HAVE_GETTER;
      nIndex--;
    }
    //create lambda for property setter
    if (cSetValueCallback)
    {
      DukTape::duk_c_function fnSetter;
      LPVOID p;

      fnSetter = [](_In_ DukTape::duk_context *lpCtx) mutable -> DukTape::duk_ret_t
      {
        MX::CJavascriptVM::OnSetPropertyCallback cCallback;
        MX::CJavascriptVM *lpVM;
        MX::CStringA cStrObjNameA, cStrPropNameA;
        DukTape::duk_size_t nBufSize;
        LPCSTR sA;
        LPVOID p;

        //get virtual machine pointer
        lpVM = MX::CJavascriptVM::FromContext(lpCtx);
        //get setter function's key name
        DukTape::duk_push_current_function(lpCtx);
        DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""name");
        sA = DukTape::duk_require_string(lpCtx, -1);
        if (cStrPropNameA.Copy(sA) == FALSE)
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
        DukTape::duk_pop(lpCtx);
        //get function's callback
        DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""callback");
        p = DukTape::duk_require_buffer(lpCtx, -1, &nBufSize);
        MX_ASSERT(p != NULL);
        MX_ASSERT(nBufSize == MX::CJavascriptVM::OnSetPropertyCallback::serialization_buffer_size());
        cCallback.deserialize(p);
        DukTape::duk_pop_2(lpCtx);
        //push object/global object
        DukTape::duk_push_this(lpCtx);
        //retrieve object's name (undefined if global object)
        if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
        {
          DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""name");
          if (DukTape::duk_is_string(lpCtx, -1) != 0)
          {
            if (cStrObjNameA.Copy(DukTape::duk_get_string(lpCtx, -1)) == FALSE)
              MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_OUTOFMEMORY);
          }
          DukTape::duk_pop_2(lpCtx);
        }
        else
        {
          DukTape::duk_pop(lpCtx);
        }
        //call callback
        sA = (LPCSTR)cStrObjNameA;
        return cCallback(lpCtx, (*sA != 0) ? sA : NULL, (LPCSTR)cStrPropNameA, 0);
      };

      //create c function callback
      DukTape::duk_push_c_function(lpCtx, fnSetter, 1);
      //store callback
      p = DukTape::duk_push_fixed_buffer(lpCtx, MX::CJavascriptVM::OnSetPropertyCallback::
                                                serialization_buffer_size());
      cSetValueCallback.serialize(p);
      DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""callback");
      //store function's name
      DukTape::duk_push_string(lpCtx, szPropertyNameA);
      DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""name");
      //----
      nDukFlags |= DUK_DEFPROP_HAVE_SETTER;
      nIndex--;
    }
    //define the property
    DukTape::duk_def_prop(lpCtx, nIndex, nDukFlags);
    //pop the object/global object
    DukTape::duk_pop(lpCtx);
    return;
  });
  //done
  return hRes;
}

HRESULT RemovePropertyCommon(_In_ DukTape::duk_context *lpCtx, _In_opt_z_ LPCSTR szObjectNameA,
                             _In_z_ LPCSTR szPropertyNameA)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  HRESULT hRes;

  if (szPropertyNameA == NULL)
    return E_POINTER;
  if (*szPropertyNameA == 0)
    return E_INVALIDARG;
  if (szObjectNameA != NULL && *szObjectNameA == 0)
    return E_INVALIDARG;
  if (lpCtx == NULL)
    return E_FAIL;

  //properties with callbacks cannot have an initial value
  hRes = lpJVM->RunNativeProtectedAndGetError(0, 0,
                                              [szObjectNameA, szPropertyNameA](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    HRESULT hRes = FindObject(lpCtx, szObjectNameA, FALSE, TRUE);
    if (FAILED(hRes))
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
    //get property
    DukTape::duk_get_prop_string(lpCtx, -1, szPropertyNameA);
    //pop the object/global object
    DukTape::duk_pop(lpCtx);
    return;
  });
  //done
  return hRes;
}

HRESULT HasPropertyCommon(_In_ DukTape::duk_context *lpCtx, _In_opt_z_ LPCSTR szObjectNameA,
                          _In_z_ LPCSTR szPropertyNameA)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  HRESULT hRes;

  if (szPropertyNameA == NULL)
    return E_POINTER;
  if (*szPropertyNameA == 0)
    return E_INVALIDARG;
  if (szObjectNameA != NULL && *szObjectNameA == 0)
    return E_INVALIDARG;
  if (lpCtx == NULL)
    return E_FAIL;

  //properties with callbacks cannot have an initial value
  hRes = lpJVM->RunNativeProtectedAndGetError(0, 0,
                                              [szObjectNameA, szPropertyNameA](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    HRESULT hRes = FindObject(lpCtx, szObjectNameA, FALSE, TRUE);
    if (FAILED(hRes))
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
    //get property
    DukTape::duk_get_prop_string(lpCtx, -1, szPropertyNameA);
    //does exists?
    hRes = (DukTape::duk_is_undefined(lpCtx, -1) != 0) ? S_FALSE : S_OK;
    //pop the property and the object/global object
    DukTape::duk_pop_2(lpCtx);
    return;
  });
  
  //done
  return hRes;
}

VOID PushPropertyCommon(_In_ DukTape::duk_context *lpCtx, _In_opt_z_ LPCSTR szObjectNameA,
                        _In_z_ LPCSTR szPropertyNameA)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  HRESULT hRes;

  if (szPropertyNameA == NULL)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_POINTER);
  if (*szPropertyNameA == 0)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  if (szObjectNameA != NULL && *szObjectNameA == 0)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_INVALIDARG);
  //properties with callbacks cannot have an initial value
  hRes = FindObject(lpCtx, szObjectNameA, FALSE, TRUE);
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  //get property
  DukTape::duk_get_prop_string(lpCtx, -1, szPropertyNameA);
  //remove the object/global object
  DukTape::duk_remove(lpCtx, -2);
  //done
  return;
}

HRESULT FindObject(_In_ DukTape::duk_context *lpCtx, _In_opt_z_ LPCSTR szObjectNameA, _In_ BOOL bCreateIfNotExists,
                   _In_ BOOL bResolveProxyOnLast)
{
  LPCSTR szNameA;
  SIZE_T nNameLen;

  MX_ASSERT(szObjectNameA == NULL || szObjectNameA[0] != 0);
  //start from global
  DukTape::duk_push_global_object(lpCtx);
  if (szObjectNameA != NULL)
  {
    while (1)
    {
      for (nNameLen=0; szObjectNameA[nNameLen] != 0 && szObjectNameA[nNameLen] != L'.' &&
                       szObjectNameA[nNameLen] != L'['; nNameLen++);
      if (nNameLen == 0)
      {
err_invalid_name:
        DukTape::duk_pop(lpCtx); //remove current object before returning to clean stack
        return E_INVALIDARG;
      }
      //get object
      DukTape::duk_push_lstring(lpCtx, szObjectNameA, nNameLen);
      DukTape::duk_get_prop(lpCtx, -2);
      //check if this is an object
      if (DukTape::duk_is_object(lpCtx, -1) == 0)
      {
        DukTape::duk_pop(lpCtx); //remove "undefined"
        //if a parent object, it must exist
        if (szObjectNameA[nNameLen] != 0 || bCreateIfNotExists == FALSE)
        {
          DukTape::duk_pop(lpCtx); //remove current object before returning to clean stack
          return MX_E_NotFound;
        }
        //the stack contains the parent object, return S_FALSE to indicate the object must be created
        return S_FALSE;
      }
      //the object exists, first remove the parent from stack
      DukTape::duk_remove(lpCtx, -2);
      //resolve proxy on last object?
      if (szObjectNameA[nNameLen] == 0 && bResolveProxyOnLast != FALSE)
      {
        DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""proxyTarget");
        if (DukTape::duk_is_object(lpCtx, -1) != 0)
          DukTape::duk_remove(lpCtx, -2);
        else
          DukTape::duk_pop(lpCtx);
      }
      //parse properties if specified
      szObjectNameA += nNameLen;
      while (*szObjectNameA == '[')
      {
        szObjectNameA++;
        if (*szObjectNameA == '"' || *szObjectNameA == '\'')
        {
          szNameA = ++szObjectNameA;
          for (nNameLen=0; szObjectNameA[nNameLen] != 0 && szObjectNameA[nNameLen] != *(szObjectNameA-1); nNameLen++);
          if (nNameLen == 0 || szObjectNameA[nNameLen] != *(szObjectNameA-1) || szObjectNameA[nNameLen+1] != L']')
            goto err_invalid_name;
          szObjectNameA += nNameLen+2;
        }
        else
        {
          szNameA = szObjectNameA;
          for (nNameLen=0; szObjectNameA[nNameLen] != 0 && szObjectNameA[nNameLen] != L']'; nNameLen++);
          if (nNameLen == 0 || szObjectNameA[nNameLen] != L']')
            goto err_invalid_name;
          szObjectNameA += nNameLen+1;
        }
        //get object
        DukTape::duk_push_lstring(lpCtx, szNameA, nNameLen);
        DukTape::duk_get_prop(lpCtx, -2);
        //check if this is an object
        if (DukTape::duk_is_object(lpCtx, -1) == 0)
        {
          DukTape::duk_pop(lpCtx); //remove "undefined"
          DukTape::duk_pop(lpCtx); //remove current object before returning to clean stack
          return MX_E_NotFound;
        }
        //the object exists, remove the parent from stack
        DukTape::duk_remove(lpCtx, -2);
      }
      //final object? exit loop
      if (*szObjectNameA == 0)
        break;
      if (*szObjectNameA != '.')
        goto err_invalid_name;
      //skip dot and advance to next
      szObjectNameA++;
    }
  }
  //done
  return S_OK;
}

} //namespace JsLib

} //namespace Internals

} //namespace MX
