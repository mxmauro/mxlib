/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2019. All rights reserved.
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

CJsObjectBase* CJsObjectBase::FromObject(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_int_t nIndex)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  CJsObjectBase *lpObj = NULL;

  HRESULT hRes;

  if (lpCtx == NULL)
    return NULL;
  hRes = lpJVM->RunNativeProtectedAndGetError(0, 0, [&lpObj, nIndex](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_get_prop_string(lpCtx, nIndex, "\xff""\xff""data");
    if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
      lpObj = reinterpret_cast<CJsObjectBase*>(DukTape::duk_to_pointer(lpCtx, -1));
    DukTape::duk_pop(lpCtx);
    return;
  });
  if (FAILED(hRes))
    return NULL;
  if (lpObj != NULL)
    lpObj->AddRef();
  return lpObj;
}

int CJsObjectBase::OnProxyHasNamedProperty(_In_z_ LPCSTR szPropNameA)
{
  return -1; //pass original
}

int CJsObjectBase::OnProxyHasIndexedProperty(_In_ int nIndex)
{
  return -1; //pass original
}

int CJsObjectBase::OnProxyGetNamedProperty(_In_z_ LPCSTR szPropNameA)
{
  return 0; //pass original
}

int CJsObjectBase::OnProxyGetIndexedProperty(_In_ int nIndex)
{
  return 0; //pass original
}

int CJsObjectBase::OnProxySetNamedProperty(_In_z_ LPCSTR szPropNameA, _In_ DukTape::duk_idx_t nValueIndex)
{
  return 0; //set original
}

int CJsObjectBase::OnProxySetIndexedProperty(_In_ int nIndex, _In_ DukTape::duk_idx_t nValueIndex)
{
  return 0; //set original
}

int CJsObjectBase::OnProxyDeleteNamedProperty(_In_z_ LPCSTR szPropNameA)
{
  return 1; //delete original
}

int CJsObjectBase::OnProxyDeleteIndexedProperty(_In_ int nIndex)
{
  return 1; //delete original
}

LPCSTR CJsObjectBase::OnProxyGetPropertyName(_In_ int nIndex)
{
  return NULL;
}

HRESULT CJsObjectBase::_RegisterHelper(_In_ DukTape::duk_context *lpCtx, _In_ MAP_ENTRY *lpEntries, _In_ BOOL bUseProxy,
                                       _In_z_ LPCSTR szObjectNameA, _In_opt_ lpfnCreateObject fnCreateObject,
                                       _In_ lpfnRegisterUnregisterCallback fnRegisterCallback,
                                       _In_ lpfnRegisterUnregisterCallback fnUnregisterCallback)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  HRESULT hRes;

  if (lpCtx == NULL || lpEntries == NULL || szObjectNameA == NULL)
    return E_POINTER;

  hRes = lpJVM->RunNativeProtectedAndGetError(0, 0, [=](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_global_object(lpCtx);

    //push the constructor object, all of our object will have a constructor
    DukTape::duk_push_c_function(lpCtx, &CJsObjectBase::_ConstructorHelper, MX_JS_VARARGS);

    //but just register callback function if the object is really creatable
    if (fnCreateObject != NULL)
    {
      DukTape::duk_push_pointer(lpCtx, fnCreateObject);
      DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""createObject");
    }

    //set internal property to check if we have to create a proxy object too
    DukTape::duk_push_boolean(lpCtx, (bUseProxy != FALSE) ? 1 : 0);
    DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""useProxy");

    //set internal property to store map entries
    DukTape::duk_push_pointer(lpCtx, lpEntries);
    DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""mapEntries");

    //create the prototype object
    DukTape::duk_push_object(lpCtx);

    //store as the prototype
    DukTape::duk_put_prop_string(lpCtx, -2, "prototype");

    //add the static properties and methods to the object
    _SetupMapEntries(lpCtx, lpEntries, TRUE);

    //register the object globally
    DukTape::duk_put_prop_string(lpCtx, -2, szObjectNameA);

    //pop global
    DukTape::duk_pop(lpCtx);
    return;
  });
  if (SUCCEEDED(hRes))
  {
    hRes = lpJVM->RunNativeProtectedAndGetError(0, 0, [fnRegisterCallback](_In_ DukTape::duk_context *lpCtx) -> VOID
    {
      fnRegisterCallback(lpCtx);
    });
  }
  if (FAILED(hRes))
  {
    _UnregisterHelper(lpCtx, szObjectNameA, fnUnregisterCallback);
  }
  return hRes;
}

VOID CJsObjectBase::_UnregisterHelper(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                      _In_ lpfnRegisterUnregisterCallback fnUnregisterCallback)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);

  if (lpCtx == NULL)
    return;
  lpJVM->RunNativeProtectedAndGetError(0, 0, [fnUnregisterCallback](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    fnUnregisterCallback(lpCtx);
    return;
  });

  lpJVM->RunNativeProtectedAndGetError(0, 0, [szObjectNameA](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_global_object(lpCtx);
    DukTape::duk_del_prop_string(lpCtx, -1, szObjectNameA);
    DukTape::duk_pop(lpCtx);
    return;
  });
  return;
}

DukTape::duk_ret_t CJsObjectBase::_ConstructorHelper(_In_ DukTape::duk_context *lpCtx)
{
  lpfnCreateObject fnCreateObject;
  CJsObjectBase *lpNewObj;
  HRESULT hRes;

  if (!DukTape::duk_is_constructor_call(lpCtx))
    return DUK_RET_TYPE_ERROR;

  //get the create object callback. Will raise an exception if the object cannot be constructed
  DukTape::duk_push_current_function(lpCtx);
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""createObject");
  fnCreateObject = (lpfnCreateObject)DukTape::duk_require_pointer(lpCtx, -1);
  DukTape::duk_pop_2(lpCtx);

  //create object
  lpNewObj = fnCreateObject(lpCtx);
  if (lpNewObj != NULL)
  {
    hRes = lpNewObj->PushThis();
    lpNewObj->Release(); //remove extra reference added by PushThis or destroy object on error
  }
  else
  {
    hRes = E_OUTOFMEMORY;
  }
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  //returning the 'result' object replaces the default instance created by 'new'
  return 1;
}

HRESULT CJsObjectBase::_PushThisHelper(_In_z_ LPCSTR szObjectNameA)
{
  DukTape::duk_context *lpCtx = GetContext();
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  HRESULT hRes;

  if (lpCtx == NULL)
    return E_FAIL;

  AddRef();

  hRes = lpJVM->RunNativeProtectedAndGetError(0, 1, [this, szObjectNameA](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    MAP_ENTRY *lpEntries;
    BOOL bCreateProxy;

    //get definition from global object
    DukTape::duk_get_global_string(lpCtx, szObjectNameA);

    //get the object map entries
    DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""mapEntries");
    lpEntries = (MAP_ENTRY*)DukTape::duk_get_pointer(lpCtx, -1);
    DukTape::duk_pop(lpCtx); //pops prototype too
    if (lpEntries == NULL)
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_UNEXPECTED);

    //check if we have to create a proxy from the prototype's internal property
    DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""useProxy");
    bCreateProxy = (DukTape::duk_is_boolean(lpCtx, -1) != 0 && DukTape::duk_get_boolean(lpCtx, -1) != 0) ? TRUE : FALSE;
    DukTape::duk_pop_2(lpCtx); //also pop definition

    //create javascript object
    DukTape::duk_push_object(lpCtx);

    //store object's name as internal property
    DukTape::duk_push_string(lpCtx, szObjectNameA);
    DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""name");

    //setup mapped entries
    _SetupMapEntries(lpCtx, lpEntries, FALSE);

    //store underlying object
    DukTape::duk_push_pointer(lpCtx, this);
    DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""data");

    //store map entries pointer
    DukTape::duk_push_pointer(lpCtx, lpEntries);
    DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""mapEntries");

    //store boolean flag to mark the object as deleted because the destructor may be called several times
    DukTape::duk_push_boolean(lpCtx, false);
    DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""released");

    //store function destructor
    DukTape::duk_push_c_function(lpCtx, &CJsObjectBase::_FinalReleaseHelper, 1);
    DukTape::duk_set_finalizer(lpCtx, -2);

    //create proxy handler object
    if (bCreateProxy != FALSE)
    {
      //create handler object
      DukTape::duk_push_object(lpCtx);
      DukTape::duk_push_pointer(lpCtx, this);
      DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""data");

      //add handler functions
      DukTape::duk_push_c_function(lpCtx, &CJsObjectBase::_ProxyHasPropHelper, 2);
      DukTape::duk_put_prop_string(lpCtx, -2, "has");
      DukTape::duk_push_c_function(lpCtx, &CJsObjectBase::_ProxyGetPropHelper, 3);
      DukTape::duk_put_prop_string(lpCtx, -2, "get");
      DukTape::duk_push_c_function(lpCtx, &CJsObjectBase::_ProxySetPropHelper, 4);
      DukTape::duk_put_prop_string(lpCtx, -2, "set");
      DukTape::duk_push_c_function(lpCtx, &CJsObjectBase::_ProxyDeletePropHelper, 2);
      DukTape::duk_put_prop_string(lpCtx, -2, "deleteProperty");
      DukTape::duk_push_c_function(lpCtx, &CJsObjectBase::_ProxyOwnKeysHelper, 1);
      DukTape::duk_put_prop_string(lpCtx, -2, "ownKeys");

      //push Proxy object's constructor
      DukTape::duk_get_global_string(lpCtx, "Proxy");

      //reorder stack [target...handler...Proxy] -> [Proxy...target...handler]
      DukTape::duk_swap(lpCtx, -3, -1);
      DukTape::duk_swap(lpCtx, -2, -1);

      //create proxy object
      DukTape::duk_new(lpCtx, 2);
    }
    return;
  });
  if (FAILED(hRes))
  {
    Release();
  }
  //done
  return hRes;
}

VOID CJsObjectBase::_SetupMapEntries(_In_ DukTape::duk_context *lpCtx, _In_ MAP_ENTRY *lpEntries, _In_ BOOL bStatic)
{
  DukTape::duk_uint_t nDukFlags;

  for (; lpEntries->szNameA != NULL; lpEntries++)
  {
    if (bStatic != FALSE)
    {
      if (lpEntries->nStatic == 0)
        continue;
    }
    else
    {
      if (lpEntries->nStatic != 0)
        continue;
    }
    if (lpEntries->fnInvoke != NULL)
    {
      //a method
      DukTape::duk_push_c_function(lpCtx, lpEntries->fnInvoke, lpEntries->nArgsCount);
      DukTape::duk_put_prop_string(lpCtx, -2, lpEntries->szNameA);
    }
    else
    {
      //a property
      DukTape::duk_push_string(lpCtx, lpEntries->szNameA);
      nDukFlags = DUK_DEFPROP_HAVE_ENUMERABLE | DUK_DEFPROP_HAVE_CONFIGURABLE | DUK_DEFPROP_HAVE_GETTER;
      if (lpEntries->nEnumerable != 0)
        nDukFlags |= DUK_DEFPROP_ENUMERABLE;
      DukTape::duk_push_c_function(lpCtx, lpEntries->fnGetter, 0);
      if (lpEntries->fnSetter != NULL)
      {
        DukTape::duk_push_c_function(lpCtx, lpEntries->fnSetter, 1);
        nDukFlags |= DUK_DEFPROP_HAVE_SETTER;
      }
      DukTape::duk_def_prop(lpCtx, (lpEntries->fnSetter != NULL) ? -4 : -3, nDukFlags);
    }
  }
  return;
}

DukTape::duk_ret_t CJsObjectBase::_FinalReleaseHelper(_In_ DukTape::duk_context *lpCtx)
{
  CJsObjectBase *lpObj;

  /*the object to delete is passed as first argument instead*/
  DukTape::duk_get_prop_string(lpCtx, 0, "\xff""\xff""released");
  if (!DukTape::duk_to_boolean(lpCtx, -1))
  {
    DukTape::duk_pop(lpCtx);
    DukTape::duk_get_prop_string(lpCtx, 0, "\xff""\xff""data");
    lpObj = reinterpret_cast<CJsObjectBase*>(DukTape::duk_to_pointer(lpCtx, -1));
    lpObj->Release();
    DukTape::duk_pop(lpCtx);
    DukTape::duk_push_boolean(lpCtx, true); //mark as deleted
    DukTape::duk_put_prop_string(lpCtx, 0, "\xff""\xff""released");
  }
  else
  {
    DukTape::duk_pop(lpCtx);
  }
  return 0;
}

DukTape::duk_ret_t CJsObjectBase::_CallMethodHelper(_In_ DukTape::duk_context *lpCtx, _In_ lpfnCallFunc fnFunc)
{
  CJsObjectBase *lpObj;

  DukTape::duk_push_this(lpCtx);
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""data");
  lpObj = reinterpret_cast<CJsObjectBase*>(DukTape::duk_to_pointer(lpCtx, -1));
  DukTape::duk_pop_2(lpCtx);
  return ((*lpObj).*fnFunc)();
}

DukTape::duk_ret_t CJsObjectBase::_CallStaticMethodHelper(_In_ DukTape::duk_context *lpCtx,
                                                          _In_ lpfnCallStaticFunc fnFunc)
{
  return fnFunc(lpCtx);
}

DukTape::duk_ret_t CJsObjectBase::_ProxyHasPropHelper(_In_ DukTape::duk_context *lpCtx)
{
  CJsObjectBase *lpObj;
  DukTape::duk_bool_t retVal;
  int nRet;

  //get target object
  DukTape::duk_push_this(lpCtx);
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""data");
  lpObj = reinterpret_cast<CJsObjectBase*>(DukTape::duk_to_pointer(lpCtx, -1));
  DukTape::duk_pop_2(lpCtx);
  //if key is a string or a number, raise event, else continue
  nRet = 0;
  if (DukTape::duk_is_string(lpCtx, 1))
  {
    nRet = lpObj->OnProxyHasNamedProperty(DukTape::duk_get_string(lpCtx, 1));
  }
  else if ((DukTape::duk_is_number(lpCtx, 1) || DukTape::duk_is_boolean(lpCtx, 1)) && !DukTape::duk_is_nan(lpCtx, 1))
  {
    nRet = lpObj->OnProxyHasIndexedProperty(DukTape::duk_get_int(lpCtx, 1));
  }
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

DukTape::duk_ret_t CJsObjectBase::_ProxyGetPropHelper(_In_ DukTape::duk_context *lpCtx)
{
  CJsObjectBase *lpObj;
  int nRet;

  //get target object
  DukTape::duk_push_this(lpCtx);
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""data");
  lpObj = reinterpret_cast<CJsObjectBase*>(DukTape::duk_to_pointer(lpCtx, -1));
  DukTape::duk_pop_2(lpCtx);
  //if key is a string or a number, raise event, else continue
  nRet = 0;
  if (DukTape::duk_is_string(lpCtx, 1))
  {
    LPCSTR szPropNameA = DukTape::duk_get_string(lpCtx, 1);
    nRet = lpObj->OnProxyGetNamedProperty(szPropNameA);
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
    nRet = lpObj->OnProxyGetIndexedProperty(nIndex);
    if (nRet < 0)
    {
      //throw error
      MX_JS_THROW_ERROR(lpCtx, DUK_ERR_TYPE_ERROR, "Cannot retrieve value of property at index #%d", nIndex);
      return DUK_RET_TYPE_ERROR;
    }
  }
  if (nRet == 0)
  {
    //get original property's value
    DukTape::duk_dup(lpCtx, 1);
    DukTape::duk_get_prop(lpCtx, 0);
  } //else a value was pushed
  //done
  return 1;
}

DukTape::duk_ret_t CJsObjectBase::_ProxySetPropHelper(_In_ DukTape::duk_context *lpCtx)
{
  CJsObjectBase *lpObj;
  int nRet;

  //get target object
  DukTape::duk_push_this(lpCtx);
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""data");
  lpObj = reinterpret_cast<CJsObjectBase*>(DukTape::duk_to_pointer(lpCtx, -1));
  DukTape::duk_pop_2(lpCtx);
  //if key is a string or a number, raise event, else continue
  nRet = 0;
  if (DukTape::duk_is_string(lpCtx, 1))
  {
    LPCSTR szPropNameA = DukTape::duk_get_string(lpCtx, 1);
    nRet = lpObj->OnProxySetNamedProperty(szPropNameA, 2);
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
    nRet = lpObj->OnProxySetIndexedProperty(nIndex, 2);
    if (nRet < 0)
    {
      //throw error
      MX_JS_THROW_ERROR(lpCtx, DUK_ERR_TYPE_ERROR, "Cannot set value of property at index #%d", nIndex);
      return DUK_RET_TYPE_ERROR;
    }
  }
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

DukTape::duk_ret_t CJsObjectBase::_ProxyDeletePropHelper(_In_ DukTape::duk_context *lpCtx)
{
  CJsObjectBase *lpObj;
  int nRet;

  //get target object
  DukTape::duk_push_this(lpCtx);
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""data");
  lpObj = reinterpret_cast<CJsObjectBase*>(DukTape::duk_to_pointer(lpCtx, -1));
  DukTape::duk_pop_2(lpCtx);
  //if key is a string or a number, raise event, else continue
  nRet = 1;
  if (DukTape::duk_is_string(lpCtx, 1))
  {
    LPCSTR szPropNameA = DukTape::duk_get_string(lpCtx, 1);
    nRet = lpObj->OnProxyDeleteNamedProperty(szPropNameA);
    if (nRet < 0)
    {
      //throw error
      MX_JS_THROW_ERROR(lpCtx, DUK_ERR_TYPE_ERROR, "Cannot delete property '%s'", szPropNameA);
      return DUK_RET_TYPE_ERROR;
    }
  }
  else if ((DukTape::duk_is_number(lpCtx, 1) || DukTape::duk_is_boolean(lpCtx, 1)) && !DukTape::duk_is_nan(lpCtx, 1))
  {
    int nIndex =  DukTape::duk_get_int(lpCtx, 1);
    nRet = lpObj->OnProxyDeleteIndexedProperty(nIndex);
    if (nRet < 0)
    {
      //throw error
      MX_JS_THROW_ERROR(lpCtx, DUK_ERR_TYPE_ERROR, "Cannot delete property at index #%d", nIndex);
      return DUK_RET_TYPE_ERROR;
    }
  }
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

DukTape::duk_ret_t CJsObjectBase::_ProxyOwnKeysHelper(_In_ DukTape::duk_context *lpCtx)
{
  CJsObjectBase *lpObj;
  MAP_ENTRY *lpEntries;
  DukTape::duk_uarridx_t ndx;
  int nIndex;

  //get target object
  DukTape::duk_push_this(lpCtx);
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""data");
  lpObj = reinterpret_cast<CJsObjectBase*>(DukTape::duk_to_pointer(lpCtx, -1));
  DukTape::duk_pop(lpCtx);
  //get the object map entries
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""mapEntries");
  lpEntries = (MAP_ENTRY*)DukTape::duk_get_pointer(lpCtx, -1);
  DukTape::duk_pop_2(lpCtx); //pops this too

  //create an array
  DukTape::duk_push_object(lpCtx);

  //setup mapped entries
  ndx = 0;
  while (lpEntries->szNameA != NULL)
  {
    if (lpEntries->fnInvoke == NULL /*&& lpEntries->bEnumerable != FALSE*/)
    {
      //a property
      DukTape::duk_push_string(lpCtx, lpEntries->szNameA);
      DukTape::duk_put_prop_index(lpCtx, -2, ndx++);
    }
    lpEntries++;
  }
  //add custom property names
  for (nIndex=0; ; nIndex++)
  {
    LPCSTR szPropNameA = lpObj->OnProxyGetPropertyName(nIndex);
    if (szPropNameA == NULL || *szPropNameA == 0)
      break;
    DukTape::duk_push_string(lpCtx, szPropNameA);
    DukTape::duk_put_prop_index(lpCtx, -2, ndx++);
  }
  //done (return array)
  return 1;
}

} //namespace MX
