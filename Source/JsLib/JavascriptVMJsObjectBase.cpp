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

CJsObjectBase* CJsObjectBase::FromObject(__in DukTape::duk_context *lpCtx, __in DukTape::duk_int_t nIndex)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  CJsObjectBase *lpObj = NULL;
  HRESULT hRes;

  hRes = lpJVM->RunNativeProtected(0, 0, [&lpObj, nIndex](__in DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_get_prop_string(lpCtx, nIndex, "\xff""\xff""data");
    if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
      lpObj = reinterpret_cast<CJsObjectBase*>(DukTape::duk_to_pointer(lpCtx, -1));
    DukTape::duk_pop(lpCtx);
    return;
  });
  if (lpObj == NULL || FAILED(hRes))
    return NULL;
  lpObj->AddRef();
  return lpObj;
}

int CJsObjectBase::OnProxyHasNamedProperty(__in_z LPCSTR szPropNameA)
{
  return -1; //pass original
}

int CJsObjectBase::OnProxyHasIndexedProperty(__in int nIndex)
{
  return -1; //pass original
}

int CJsObjectBase::OnProxyGetNamedProperty(__in_z LPCSTR szPropNameA)
{
  return 0; //pass original
}

int CJsObjectBase::OnProxyGetIndexedProperty(__in int nIndex)
{
  return 0; //pass original
}

int CJsObjectBase::OnProxySetNamedProperty(__in_z LPCSTR szPropNameA, __in DukTape::duk_idx_t nValueIndex)
{
  return 0; //set original
}

int CJsObjectBase::OnProxySetIndexedProperty(__in int nIndex, __in DukTape::duk_idx_t nValueIndex)
{
  return 0; //set original
}

int CJsObjectBase::OnProxyDeleteNamedProperty(__in_z LPCSTR szPropNameA)
{
  return 1; //delete original
}

int CJsObjectBase::OnProxyDeleteIndexedProperty(__in int nIndex)
{
  return 1; //delete original
}

HRESULT CJsObjectBase::_RegisterHelper(__in DukTape::duk_context *lpCtx, __in MAP_ENTRY *lpEntries,
                                       __in_z LPCSTR szObjectNameA, __in_z LPCSTR szPrototypeNameA,
                                       __in_opt lpfnCreateObject fnCreateObject, __in_opt int nCreateArgsCount,
                                       __in BOOL bCreateProxy)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  HRESULT hRes;

  if (lpCtx == NULL || lpEntries == NULL || szObjectNameA == NULL || szPrototypeNameA == NULL)
    return E_POINTER;
  hRes = lpJVM->RunNativeProtected(0, 0, [=](__in DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_uint_t nDukFlags;
    MAP_ENTRY *lpEntries_2 = lpEntries;

    //create a prototype with functions
    DukTape::duk_push_global_object(lpCtx);
    DukTape::duk_push_object(lpCtx);
    //set internal property to check if we have to create a proxy object too
    DukTape::duk_push_boolean(lpCtx, (bCreateProxy != FALSE) ? 1 : 0);
    DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""useProxy");
    //setup mapped entries
    while (lpEntries_2->szNameA != NULL)
    {
      if (lpEntries_2->fnInvoke != NULL)
      {
        //a method
        DukTape::duk_push_c_function(lpCtx, lpEntries_2->fnInvoke, lpEntries_2->nArgsCount);
        DukTape::duk_put_prop_string(lpCtx, -2, lpEntries_2->szNameA);
      }
      else
      {
        //a property
        DukTape::duk_push_string(lpCtx, lpEntries_2->szNameA);
        nDukFlags = DUK_DEFPROP_HAVE_ENUMERABLE | DUK_DEFPROP_HAVE_CONFIGURABLE | DUK_DEFPROP_HAVE_GETTER;
        if (lpEntries_2->bEnumerable != FALSE)
          nDukFlags |= DUK_DEFPROP_ENUMERABLE;
        DukTape::duk_push_c_function(lpCtx, lpEntries_2->fnGetter, 0);
        if (lpEntries_2->fnSetter != NULL)
        {
          DukTape::duk_push_c_function(lpCtx, lpEntries_2->fnSetter, 1);
          nDukFlags |= DUK_DEFPROP_HAVE_SETTER;
        }
        DukTape::duk_def_prop(lpCtx, (lpEntries_2->fnSetter != NULL) ? -4 : -3, nDukFlags);
      }
      lpEntries_2++;
    }
    //store as the prototype
    DukTape::duk_put_prop_string(lpCtx, -2, szPrototypeNameA);
    //register constructor in global if object is creatable
    if (fnCreateObject != NULL)
    {
      DukTape::duk_push_c_function(lpCtx, &CJsObjectBase::_CreateHelper, nCreateArgsCount);
      DukTape::duk_push_pointer(lpCtx, fnCreateObject);
      DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""createObject");
      DukTape::duk_put_prop_string(lpCtx, -2, szObjectNameA );
    }
    //pop global
    DukTape::duk_pop(lpCtx);
    return;
  });
  return hRes;
}

DukTape::duk_ret_t CJsObjectBase::_CreateHelper(__in DukTape::duk_context *lpCtx)
{
  lpfnCreateObject fnCreateObject;
  CJsObjectBase *lpNewObj;
  HRESULT hRes;

  if (!DukTape::duk_is_constructor_call(lpCtx))
    return DUK_RET_TYPE_ERROR;
  DukTape::duk_push_current_function(lpCtx);
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""createObject");
  fnCreateObject = (lpfnCreateObject)DukTape::duk_require_pointer(lpCtx, -1);
  DukTape::duk_pop_2(lpCtx);
  //create object
  lpNewObj = fnCreateObject(lpCtx);
  if (lpNewObj)
  {
    hRes = lpNewObj->PushThis();
    lpNewObj->Release(); //remove extra reference added by PushThis or destroy object on error
  }
  else
  {
    hRes = E_OUTOFMEMORY;
  }
  if (FAILED(hRes))
    MX_JS_THROW_HRESULT_ERROR(lpCtx, E_OUTOFMEMORY);
  //returning the 'result' object replaces the default instance created by 'new'
  return 1;
}

HRESULT CJsObjectBase::_PushThisHelper(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPrototypeNameA)
{
  DukTape::duk_context *lpCtx = GetContext();
  CJavascriptVM *lpJvm = CJavascriptVM::FromContext(lpCtx);
  HRESULT hRes;

  AddRef();
  hRes = lpJvm->RunNativeProtected(0, 1, [this, szObjectNameA, szPrototypeNameA]
                                   (__in DukTape::duk_context *lpCtx) -> VOID
  {
    BOOL bCreateProxy = FALSE;

    //create javascript object
    DukTape::duk_push_object(lpCtx);
    //store object's name as internal property
    DukTape::duk_push_string(lpCtx, szObjectNameA);
    DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""name");
    //get prototype from global object
    DukTape::duk_get_global_string(lpCtx, szPrototypeNameA);
    //check if we have to create a proxy from the prototype's internal property
    DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""useProxy");
    if (DukTape::duk_is_boolean(lpCtx, -1) != 0 && DukTape::duk_get_boolean(lpCtx, -1) != 0)
      bCreateProxy = TRUE;
    DukTape::duk_pop(lpCtx);
    //set new object's prototype
    DukTape::duk_set_prototype(lpCtx, -2);
    //store underlying object
    DukTape::duk_push_pointer(lpCtx, this);
    DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""data");
    //store boolean flag to mark the object as deleted because the destructor
    //may be called several times
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
  //done
  if (FAILED(hRes))
    Release();
  return hRes;
}

DukTape::duk_ret_t CJsObjectBase::_FinalReleaseHelper(__in DukTape::duk_context *lpCtx)
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

DukTape::duk_ret_t CJsObjectBase::_CallMethodHelper(__in DukTape::duk_context *lpCtx, __in lpfnCallFunc fnFunc)
{
  CJsObjectBase *lpObj;

  DukTape::duk_push_this(lpCtx);
  DukTape::duk_get_prop_string(lpCtx, -1, "\xff""\xff""data");
  lpObj = reinterpret_cast<CJsObjectBase*>(DukTape::duk_to_pointer(lpCtx, -1));
  DukTape::duk_pop_2(lpCtx);
  return ((*lpObj).*fnFunc)();
}

DukTape::duk_ret_t CJsObjectBase::_ProxyHasPropHelper(__in DukTape::duk_context *lpCtx)
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

DukTape::duk_ret_t CJsObjectBase::_ProxyGetPropHelper(__in DukTape::duk_context *lpCtx)
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

DukTape::duk_ret_t CJsObjectBase::_ProxySetPropHelper(__in DukTape::duk_context *lpCtx)
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

DukTape::duk_ret_t CJsObjectBase::_ProxyDeletePropHelper(__in DukTape::duk_context *lpCtx)
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
  else if (DukTape::duk_is_number(lpCtx, 1) || DukTape::duk_is_boolean(lpCtx, 1))
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

} //namespace MX
