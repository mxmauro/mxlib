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

#ifndef _MX_JAVASCRIPT_VM_H
#define _MX_JAVASCRIPT_VM_H

#include "..\Defines.h"
#include "..\RefCounted.h"
#include "..\Callbacks.h"
#include "..\ArrayList.h"
#include "..\Strings\Strings.h"

#include <functional>

namespace DukTape {
#define DUK_OPT_HAVE_CUSTOM_H
#include "DukTape\duktape.h"
} //namespace DukTape
#include "..\Debug.h"

//-----------------------------------------------------------

#define MX_JS_VARARGS             ((DukTape::duk_int_t)(-1))

#define MX_JS_DECLARE(_cls, _name)                                                              \
public:                                                                                         \
  static HRESULT Register(__in DukTape::duk_context *lpCtx)                                     \
    { return _Register(lpCtx, FALSE, NULL); };                                                  \
    __MX_JS_DECLARE_COMMON(_cls, _name)

#define MX_JS_DECLARE_CREATABLE(_cls, _name)                                                    \
public:                                                                                         \
  static HRESULT Register(__in DukTape::duk_context *lpCtx)                                     \
    { return _Register(lpCtx, FALSE, &_cls::_CreateObject); };                                  \
    __MX_JS_DECLARE_COMMON(_cls, _name)                                                         \
    __MX_JS_DECLARE_CREATE_OBJECT(_cls)

#define MX_JS_DECLARE_WITH_PROXY(_cls, _name)                                                   \
public:                                                                                         \
  static HRESULT Register(__in DukTape::duk_context *lpCtx)                                     \
    { return _Register(lpCtx, TRUE, NULL); };                                                   \
    __MX_JS_DECLARE_COMMON(_cls, _name)

#define MX_JS_DECLARE_CREATABLE_WITH_PROXY(_cls, _name)                                         \
public:                                                                                         \
  static HRESULT Register(__in DukTape::duk_context *lpCtx)                                     \
    { return _Register(lpCtx, TRUE, &_cls::_CreateObject); };                                   \
    __MX_JS_DECLARE_COMMON(_cls, _name)                                                         \
    __MX_JS_DECLARE_CREATE_OBJECT(_cls)

#define __MX_JS_DECLARE_COMMON(_cls, _name)                                                     \
public:                                                                                         \
  static HRESULT _Register(__in DukTape::duk_context *lpCtx, __in BOOL bUseProxy,               \
                           __in_opt lpfnCreateObject fnCreateObject)                            \
    {                                                                                           \
    return _RegisterHelper(lpCtx, _GetMapEntries(), _name, "\xff""\xff" _name "_prototype",     \
                            fnCreateObject, bUseProxy, &_cls::OnRegister, &_cls::OnUnregister); \
    };                                                                                          \
                                                                                                \
public:                                                                                         \
  static HRESULT Unregister(__in DukTape::duk_context *lpCtx)                                   \
    {                                                                                           \
    _UnregisterHelper(lpCtx, _name, "\xff""\xff" _name "_prototype", &_cls::OnUnregister);      \
    };                                                                                          \
                                                                                                \
  HRESULT PushThis()                                                                            \
    {                                                                                           \
    return _PushThisHelper(_name, "\xff""\xff" _name "_prototype");                             \
    };

#define __MX_JS_DECLARE_CREATE_OBJECT(_cls)                                                     \
private:                                                                                        \
  static CJsObjectBase* _CreateObject(__in DukTape::duk_context *lpCtx)                         \
    { return MX_DEBUG_NEW _cls(lpCtx); };

#define MX_JS_BEGIN_MAP(_cls)                                                                   \
private:                                                                                        \
  typedef DukTape::duk_ret_t (_cls::*lpfnFunc)(__in DukTape::duk_context *);                    \
                                                                                                \
  static MAP_ENTRY* _GetMapEntries()                                                            \
    {                                                                                           \
    static MAP_ENTRY _entries[] = {

#define MX_JS_END_MAP()                                                                         \
      { NULL, NULL, NULL, NULL, 0 }                                                             \
    };                                                                                          \
    return _entries;                                                                            \
    };

#define MX_JS_MAP_METHOD(_name, _func, _argsCount)                                              \
        { _name,                                                                                \
          [](__in DukTape::duk_context *lpCtx) -> DukTape::duk_ret_t                            \
          {                                                                                     \
            return _CallMethodHelper(lpCtx, static_cast<lpfnCallFunc>(_func));                  \
          },                                                                                    \
          NULL, NULL, _argsCount },

#define MX_JS_MAP_PROPERTY(_name, _getFunc, _setFunc, _enumerable )                             \
        { _name, NULL,                                                                          \
          [](__in DukTape::duk_context *lpCtx) -> DukTape::duk_ret_t                            \
          {                                                                                     \
            return _CallMethodHelper(lpCtx, static_cast<lpfnCallFunc>(_getFunc));               \
          },                                                                                    \
          [](__in DukTape::duk_context *lpCtx) -> DukTape::duk_ret_t                            \
          {                                                                                     \
            return _CallMethodHelper(lpCtx, static_cast<lpfnCallFunc>(_setFunc));               \
          },                                                                                    \
          _enumerable },

#define MX_JS_THROW_WINDOWS_ERROR(ctx, hr)                                                      \
          MX::CJavascriptVM::ThrowWindowsError((ctx), (HRESULT)(hr), (LPCSTR)(__FILE__),        \
                                               (DukTape::duk_int_t)__LINE__)

#define MX_JS_THROW_ERROR(ctx, code, fmt, ...)                                                  \
          DukTape::duk_error_raw((ctx), (DukTape::duk_errcode_t)(code), (LPCSTR)(__FILE__),     \
                                 (DukTape::duk_int_t)__LINE__, (fmt), __VA_ARGS__)

//-----------------------------------------------------------

namespace MX {

class CJsObjectBase;

class CJavascriptVM : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CJavascriptVM);
public:
  typedef enum {
    PropertyFlagWritable   = 0x01,
    PropertyFlagEnumerable = 0x02,
    PropertyFlagDeletable  = 0x04
  } ePropertyFlags;

  //----

  class CRequireModuleContext;

  //--------

public:
  typedef Callback<HRESULT (__in DukTape::duk_context *lpCtx, __in CRequireModuleContext *lpReqContext,
                            __inout CStringA &cStrCodeA)> OnRequireModuleCallback;

  typedef Callback<DukTape::duk_ret_t (__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                       __in_z LPCSTR szFunctionNameA)> OnNativeFunctionCallback;

  typedef Callback<DukTape::duk_ret_t (__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                       __in_z LPCSTR szPropertyNameA)> OnGetPropertyCallback;
  typedef Callback<DukTape::duk_ret_t (__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                       __in_z LPCSTR szPropertyNameA,
                                       __in DukTape::duk_idx_t nValueIndex)> OnSetPropertyCallback;

  //return 1 if has, 0 if has not, -1 to pass to original object
  typedef Callback<int (__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                        __in_z LPCSTR szPropertyNameA)> OnProxyHasNamedPropertyCallback;
  typedef Callback<int (__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                        __in int nIndex)> OnProxyHasIndexedPropertyCallback;

  //return 1 if a value was pushed, 0 to pass to original object, -1 to throw an error
  typedef Callback<int (__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                        __in_z LPCSTR szPropertyNameA)> OnProxyGetNamedPropertyCallback;
  typedef Callback<int (__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                        __in int nIndex)> OnProxyGetIndexedPropertyCallback;

  //return 1 if a new value was pushed, 0 to set the original passed value, -1 to throw an error
  typedef Callback<int (__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA,
                        __in DukTape::duk_idx_t nValueIndex)> OnProxySetNamedPropertyCallback;
  typedef Callback<int (__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA, __in int nIndex,
                        __in DukTape::duk_idx_t nValueIndex)> OnProxySetIndexedPropertyCallback;

  //return 1 if delete must proceed, 0 to silently ignore, -1 to throw an error
  typedef Callback<int (__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                        __in_z LPCSTR szPropertyNameA)> OnProxyDeleteNamedPropertyCallback;
  typedef Callback<int (__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                        __in int nIndex)> OnProxyDeleteIndexedPropertyCallback;

  //return NULL/Empty String to end enumeration
  typedef Callback<LPCSTR (__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                           __in int nIndex)> OnProxyGetPropertyNameCallback;

  //--------

  class CProxyCallbacks
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CProxyCallbacks);
  public:
    CProxyCallbacks()
      {
      cProxyHasNamedPropertyCallback = NullCallback();
      cProxyHasIndexedPropertyCallback = NullCallback();
      cProxyGetNamedPropertyCallback = NullCallback();
      cProxyGetIndexedPropertyCallback = NullCallback();
      cProxySetNamedPropertyCallback = NullCallback();
      cProxySetIndexedPropertyCallback = NullCallback();
      cProxyDeleteNamedPropertyCallback = NullCallback();
      cProxyDeleteIndexedPropertyCallback = NullCallback();
      cProxyGetPropertyNameCallback = NullCallback();
      return;
      };

  public:
    OnProxyHasNamedPropertyCallback cProxyHasNamedPropertyCallback;
    OnProxyHasIndexedPropertyCallback cProxyHasIndexedPropertyCallback;
    OnProxyGetNamedPropertyCallback cProxyGetNamedPropertyCallback;
    OnProxyGetIndexedPropertyCallback cProxyGetIndexedPropertyCallback;
    OnProxySetNamedPropertyCallback cProxySetNamedPropertyCallback;
    OnProxySetIndexedPropertyCallback cProxySetIndexedPropertyCallback;
    OnProxyDeleteNamedPropertyCallback cProxyDeleteNamedPropertyCallback;
    OnProxyDeleteIndexedPropertyCallback cProxyDeleteIndexedPropertyCallback;
    OnProxyGetPropertyNameCallback cProxyGetPropertyNameCallback;

  private:
    friend CJavascriptVM;

    void serialize(__in void *p);
    void deserialize(__in void *p);
    static size_t serialization_buffer_size();
  };

  //--------

  class CRequireModuleContext
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CRequireModuleContext);
  protected:
    CRequireModuleContext(__in DukTape::duk_context *lpCtx, __in_z LPCWSTR szIdW,
                          __in DukTape::duk_idx_t nModuleObjectIndex, __in DukTape::duk_idx_t nExportsObjectIndex);

  public:
    LPCWSTR GetId() const
      {
      return szIdW;
      };

    HRESULT RequireModule(__in_z LPCWSTR szModuleIdW);

    HRESULT AddNativeFunction(__in_z LPCSTR szFuncNameA, __in OnNativeFunctionCallback cNativeFunctionCallback,
                              __in int nArgsCount);

    HRESULT AddProperty(__in_z LPCSTR szPropertyNameA, __in_opt BOOL bInitialValueOnStack=FALSE,
                        __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
    HRESULT AddStringProperty(__in_z LPCSTR szPropertyNameA, __in_z LPCSTR szValueA,
                              __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
    HRESULT AddBooleanProperty(__in_z LPCSTR szPropertyNameA, __in BOOL bValue,
                               __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
    HRESULT AddNumericProperty(__in_z LPCSTR szPropertyNameA, __in double nValue,
                               __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
    HRESULT AddNullProperty(__in_z LPCSTR szPropertyNameA,
                            __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
    HRESULT AddJsObjectProperty(__in_z LPCSTR szPropertyNameA, __in CJsObjectBase *lpObject,
                                __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
    HRESULT AddPropertyWithCallback(__in_z LPCSTR szPropertyNameA, __in OnGetPropertyCallback cGetValueCallback,
                                __in_opt OnSetPropertyCallback cSetValueCallback=NullCallback(),
                                __in_opt int nFlags=PropertyFlagEnumerable);

    HRESULT ReplaceModuleExports();
    HRESULT ReplaceModuleExportsWithObject(__in CJsObjectBase *lpObject);

  private:
    friend CJavascriptVM;

    DukTape::duk_context *lpCtx;
    LPCWSTR szIdW;
    DukTape::duk_idx_t nModuleObjectIndex, nExportsObjectIndex;
  };

  //--------

  typedef std::function<VOID (__in DukTape::duk_context *lpCtx)> lpfnProtectedFunction;

  typedef VOID (*lpfnThrowExceptionCallback)(__in DukTape::duk_context *lpCtx,
                                             __in DukTape::duk_idx_t nExceptionObjectIndex);

public:
  CJavascriptVM();
  ~CJavascriptVM();

  VOID On(__in OnRequireModuleCallback cRequireModuleCallback);

  HRESULT Initialize();
  VOID Finalize();

  __inline operator DukTape::duk_context*() const
    {
    MX_ASSERT(lpCtx != NULL);
    return lpCtx;
    };

  static CJavascriptVM* FromContext(__in DukTape::duk_context *lpCtx);

  VOID Run(__in_z LPCSTR szCodeA, __in_z_opt LPCWSTR szFileNameW=NULL, __in_opt BOOL bIgnoreResult=TRUE);

  static VOID RunNativeProtected(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nArgsCount,
                                 __in DukTape::duk_idx_t nRetValuesCount, __in lpfnProtectedFunction fnFunc,
                                 __in_opt BOOL bCatchUnhandled=FALSE);
  VOID RunNativeProtected(__in DukTape::duk_idx_t nArgsCount, __in DukTape::duk_idx_t nRetValuesCount,
                          __in lpfnProtectedFunction fnFunc, __in_opt BOOL bCatchUnhandled=FALSE);

  HRESULT RegisterException(__in_z LPCSTR szExceptionNameA, __in lpfnThrowExceptionCallback fnThrowExceptionCallback);
  HRESULT UnregisterException(__in_z LPCSTR szExceptionNameA);

  HRESULT AddNativeFunction(__in_z LPCSTR szFuncNameA, __in OnNativeFunctionCallback cNativeFunctionCallback,
                            __in int nArgsCount);

  HRESULT AddProperty(__in_z LPCSTR szPropertyNameA, __in_opt BOOL bInitialValueOnStack=FALSE,
                      __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
  HRESULT AddStringProperty(__in_z LPCSTR szPropertyNameA, __in_z LPCSTR szValueA,
                            __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
  HRESULT AddStringProperty(__in_z LPCSTR szPropertyNameA, __in_z LPCWSTR szValueW,
                            __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
  HRESULT AddBooleanProperty(__in_z LPCSTR szPropertyNameA, __in BOOL bValue,
                             __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
  HRESULT AddIntegerProperty(__in_z LPCSTR szPropertyNameA, __in int nValue,
                             __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
  HRESULT AddNumericProperty(__in_z LPCSTR szPropertyNameA, __in double nValue,
                             __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
  HRESULT AddNullProperty(__in_z LPCSTR szPropertyNameA,
                          __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
  HRESULT AddJsObjectProperty(__in_z LPCSTR szPropertyNameA, __in CJsObjectBase *lpObject,
                              __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
  HRESULT AddPropertyWithCallback(__in_z LPCSTR szPropertyNameA, __in OnGetPropertyCallback cGetValueCallback,
                                  __in_opt OnSetPropertyCallback cSetValueCallback=NullCallback(),
                                  __in_opt int nFlags=PropertyFlagEnumerable);

  HRESULT RemoveProperty(__in_z LPCSTR szPropertyNameA);

  HRESULT HasProperty(__in_z LPCSTR szPropertyNameA);

  VOID PushProperty(__in_z LPCSTR szPropertyNameA);

  HRESULT CreateObject(__in_z LPCSTR szObjectNameA, __in_opt CProxyCallbacks *lpCallbacks=NULL);

  HRESULT AddObjectNativeFunction(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szFuncNameA,
                                  __in OnNativeFunctionCallback cNativeFunctionCallback, __in int nArgsCount);

  HRESULT AddObjectProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA,
                            __in_opt BOOL bInitialValueOnStack=FALSE,
                            __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
  HRESULT AddObjectStringProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA, __in_z LPCSTR szValueA,
                                  __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
  HRESULT AddObjectStringProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA, __in_z LPCWSTR szValueW,
                                  __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
  HRESULT AddObjectBooleanProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA, __in BOOL bValue,
                                   __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
  HRESULT AddObjectIntegerProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA, __in int nValue,
                                   __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
  HRESULT AddObjectNumericProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA, __in double nValue,
                                   __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
  HRESULT AddObjectNullProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA,
                                __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
  HRESULT AddObjectJsObjectProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA,
                                    __in CJsObjectBase *lpObject,
                                    __in_opt int nFlags=PropertyFlagWritable|PropertyFlagEnumerable);
  HRESULT AddObjectPropertyWithCallback(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA,
                                        __in OnGetPropertyCallback cGetValueCallback,
                                        __in_opt OnSetPropertyCallback cSetValueCallback=NullCallback(),
                                        __in_opt int nFlags=PropertyFlagEnumerable);

  HRESULT RemoveObjectProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA);

  HRESULT HasObjectProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA);

  VOID PushObjectProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA);

  //converts from/to utf-8
  static VOID PushString(__in DukTape::duk_context *lpCtx, __in_z LPCWSTR szStrW, __in_opt SIZE_T nStrLen=(SIZE_T)-1);
  VOID PushString(__in_z LPCWSTR szStrW, __in_opt SIZE_T nStrLen=(SIZE_T)-1);

  static VOID GetString(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nStackIndex, __inout CStringW &cStrW,
                        __in_opt BOOL bAppend=FALSE);
  VOID GetString(__in DukTape::duk_idx_t nStackIndex, __inout CStringW &cStrW, __in_opt BOOL bAppend=FALSE);

  //convert SYSTEMTIME from/to JS Date object
  static VOID PushDate(__in DukTape::duk_context *lpCtx, __in LPSYSTEMTIME lpSt, __in_opt BOOL bAsUtc=TRUE);
  VOID PushDate(__in LPSYSTEMTIME lpSt, __in_opt BOOL bAsUtc=TRUE);

  static VOID GetDate(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nObjIdx, __out LPSYSTEMTIME lpSt);
  VOID GetDate(__in DukTape::duk_idx_t nObjIdx, __out LPSYSTEMTIME lpSt);

  //NOTE: Setting 'var a=0;' can lead to 'a == FALSE'. No idea if JS or DukTape.
  //      These methods checks for boolean values first and for integer values
  //IMPORTANT: Unsafe execution. May throw exception if object at specified stack position is not a number.
  static DukTape::duk_int_t GetInt(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nObjIdx);
  DukTape::duk_int_t GetInt(__in DukTape::duk_idx_t nObjIdx);

  static DukTape::duk_uint_t GetUInt(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nObjIdx);
  DukTape::duk_uint_t GetUInt(__in DukTape::duk_idx_t nObjIdx);

  static DukTape::duk_double_t GetDouble(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nObjIdx);
  DukTape::duk_double_t GetDouble(__in DukTape::duk_idx_t nObjIdx);

  static HRESULT AddSafeString(__inout CStringA &cStrCodeA, __in_z LPCSTR szStrA, __in_opt SIZE_T nStrLen=(SIZE_T)-1);
  static HRESULT AddSafeString(__inout CStringA &cStrCodeA, __in_z LPCWSTR szStrW, __in_opt SIZE_T nStrLen=(SIZE_T)-1);

  static VOID ThrowWindowsError(__in DukTape::duk_context *lpCtx, __in HRESULT hr, __in_opt LPCSTR filename=NULL,
                                __in_opt DukTape::duk_int_t line=0);

private:
  //static DukTape::duk_ret_t OnModSearch(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t OnNodeJsResolveModule(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t OnNodeJsLoadModule(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyHasPropHelper(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyGetPropHelper(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxySetPropHelper(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyDeletePropHelper(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyOwnKeysHelper(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _RunNativeProtectedHelper(__in DukTape::duk_context *lpCtx, __in void *udata);

  static BOOL HandleException(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nStackIndex,
                              __in_opt BOOL bCatchUnhandled);

private:
  typedef struct {
    LPCSTR szExceptionNameA;
    lpfnThrowExceptionCallback fnThrowExceptionCallback;
  } EXCEPTION_ITEM;

  DukTape::duk_context *lpCtx;
  OnRequireModuleCallback cRequireModuleCallback;
  TArrayList4Structs<EXCEPTION_ITEM> aRegisteredExceptionsList;
};

//-----------------------------------------------------------

class CJsObjectBase : public virtual TRefCounted<CBaseMemObj>
{
  MX_DISABLE_COPY_CONSTRUCTOR(CJsObjectBase);
public:
  CJsObjectBase(__in DukTape::duk_context *lpCtx) : TRefCounted<CBaseMemObj>()
    {
    MX_ASSERT(lpCtx != NULL);
    lpJVM = CJavascriptVM::FromContext(lpCtx);
    MX_ASSERT(lpJVM != NULL);
    return;
    };

  ~CJsObjectBase()
    { };

  virtual HRESULT PushThis() = 0;

  static CJsObjectBase* FromObject(__in DukTape::duk_context *lpCtx, __in DukTape::duk_int_t nIndex);

  //return 1 if has, 0 if has not, -1 to pass to original object
  virtual int OnProxyHasNamedProperty(__in_z LPCSTR szPropNameA);
  virtual int OnProxyHasIndexedProperty(__in int nIndex);

  //return 1 if a value was pushed, 0 to pass to original object, -1 to throw an error
  virtual int OnProxyGetNamedProperty(__in_z LPCSTR szPropNameA);
  virtual int OnProxyGetIndexedProperty(__in int nIndex);

  //return 1 if a new value was pushed, 0 to set the original passed value, -1 to throw an error
  virtual int OnProxySetNamedProperty(__in_z LPCSTR szPropNameA, __in DukTape::duk_idx_t nValueIndex);
  virtual int OnProxySetIndexedProperty(__in int nIndex, __in DukTape::duk_idx_t nValueIndex);

  //return 1 if delete must proceed, 0 to silently ignore, -1 to throw an error
  virtual int OnProxyDeleteNamedProperty(__in_z LPCSTR szPropNameA);
  virtual int OnProxyDeleteIndexedProperty(__in int nIndex);

  //return NULL/Empty String to end enumeration
  virtual LPCSTR OnProxyGetPropertyName(__in int nIndex);

  CJavascriptVM& GetJavascriptVM() const
    {
    return *lpJVM;
    };

  DukTape::duk_context* GetContext() const
    {
    return (DukTape::duk_context*)GetJavascriptVM();
    };

protected:
  typedef DukTape::duk_ret_t (CJsObjectBase::*lpfnCallFunc)();

  typedef struct {
    LPCSTR szNameA;
    DukTape::duk_c_function fnInvoke, fnGetter, fnSetter;
    union {
      DukTape::duk_idx_t nArgsCount;
      BOOL bEnumerable;
    };
  } MAP_ENTRY;

protected:
  typedef CJsObjectBase* (*lpfnCreateObject)(__in DukTape::duk_context *lpCtx);
  typedef VOID (*lpfnRegisterUnregisterCallback)(__in DukTape::duk_context *lpCtx);

  static VOID OnRegister(__in DukTape::duk_context *lpCtx)
    { };
  static VOID OnUnregister(__in DukTape::duk_context *lpCtx)
    { };

  static HRESULT _RegisterHelper(__in DukTape::duk_context *lpCtx, __in MAP_ENTRY *lpEntries,
                                 __in_z LPCSTR szObjectNameA, __in_z LPCSTR szPrototypeNameA,
                                 __in_opt lpfnCreateObject fnCreateObject, __in BOOL bUseProxy,
                                 __in lpfnRegisterUnregisterCallback fnRegisterCallback,
                                 __in lpfnRegisterUnregisterCallback fnUnregisterCallback);
  static VOID _UnregisterHelper(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                __in_z LPCSTR szPrototypeNameA,
                                __in lpfnRegisterUnregisterCallback fnUnregisterCallback);

  static DukTape::duk_ret_t _CreateHelper(__in DukTape::duk_context *lpCtx);
  HRESULT _PushThisHelper(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPrototypeNameA);
  static DukTape::duk_ret_t _FinalReleaseHelper(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _CallMethodHelper(__in DukTape::duk_context *lpCtx, __in lpfnCallFunc fnFunc);
  static DukTape::duk_ret_t _ProxyHasPropHelper(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyGetPropHelper(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxySetPropHelper(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyDeletePropHelper(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyOwnKeysHelper(__in DukTape::duk_context *lpCtx);

private:
  CJavascriptVM *lpJVM;
};

//-----------------------------------------------------------

class CJsError
{
protected:
  CJsError(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nStackIndex);
  CJsError(__in_z LPCSTR szMessageA, __in DukTape::duk_context *lpCtx, __in_z LPCSTR szFileNameA, __in ULONG nLine);

public:
  CJsError(__in const CJsError &obj);
  CJsError& operator=(__in const CJsError &obj);

  ~CJsError();

public:
  __inline LPCSTR GetDescription() const
    {
    return (lpStrMessageA != NULL) ? (LPCSTR)(*lpStrMessageA) : "";
    };

  __inline LPCSTR GetFileName() const
    {
    return (lpStrFileNameA != NULL) ? (LPCSTR)(*lpStrFileNameA) : "";
    };

  __inline ULONG GetLineNumber() const
    {
    return nLine;
    };

  __inline LPCSTR GetStackTrace() const
    {
    return (lpStrStackTraceA != NULL) ? (LPCSTR)(*lpStrStackTraceA) : "N/A";
    };

protected:
  typedef TRefCounted<CStringA> CRefCountedStringA;

private:
  friend class CJavascriptVM;

  VOID Cleanup();

protected:
  CRefCountedStringA *lpStrMessageA;
  CRefCountedStringA *lpStrFileNameA;
  ULONG nLine;
  CRefCountedStringA *lpStrStackTraceA;
};

//-----------------------------------------------------------

class CJsWindowsError : public CJsError
{
protected:
  CJsWindowsError(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nStackIndex);
  CJsWindowsError(__in HRESULT hRes, __in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nStackIndex);
  CJsWindowsError(__in HRESULT hRes, __in DukTape::duk_context *lpCtx, __in_z LPCSTR szFileNameA, __in ULONG nLine);
  CJsWindowsError(__in HRESULT hRes);

public:
  CJsWindowsError(__in const CJsWindowsError &obj);
  CJsWindowsError& operator=(__in const CJsWindowsError &obj);

  __inline HRESULT GetHResult() const
    {
    return hRes;
    };

private:
  friend class CJavascriptVM;
  friend class CJsError;

  VOID QueryMessageString();

private:
  HRESULT hRes;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JAVASCRIPT_VM_H
