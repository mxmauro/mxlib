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
#include "..\Strings\Strings.h"

#include <functional>

namespace DukTape {
#define DUK_OPT_HAVE_CUSTOM_H
#include "DukTape\duktape.h"
} //namespace DukTape
#include "..\Debug.h"

//-----------------------------------------------------------

#define MX_JS_VARARGS             ((DukTape::duk_int_t)(-1))

#define MX_JS_BEGIN_MAP(_cls, _name, _constructorArgsCount)                                     \
public:                                                                                         \
  static HRESULT Register(__in DukTape::duk_context *lpCtx, __in_opt BOOL bCreatable=TRUE,      \
                          __in_opt BOOL bCreateProxy=FALSE)                                     \
    {                                                                                           \
    return _RegisterHelper(lpCtx, _GetMapEntries(), _name, "\xff""\xff" _name "_prototype",     \
                           (bCreatable) ? &_cls::_CreateObject : NULL, _constructorArgsCount,   \
                           bCreateProxy);                                                       \
    };                                                                                          \
                                                                                                \
  VOID PushThis()                                                                               \
    {                                                                                           \
    _PushThisHelper(_name, "\xff""\xff" _name "_prototype");                                    \
    return;                                                                                     \
    };                                                                                          \
                                                                                                \
private:                                                                                        \
  typedef DukTape::duk_ret_t (_cls::*lpfnFunc)(__in DukTape::duk_context *);                    \
                                                                                                \
  static CJsObjectBase* _CreateObject(__in DukTape::duk_context *lpCtx)                         \
    {                                                                                           \
    return MX_DEBUG_NEW _cls(lpCtx);                                                            \
    };                                                                                          \
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

#define MX_JS_THROW_HRESULT_ERROR(ctx, hres)                                                    \
          DukTape::duk_error_raw((ctx), DUK_ERR_ERROR, (LPCSTR)(__FILE__),                      \
                                 (DukTape::duk_int_t)__LINE__, "\xFF\xFF%08X", (HRESULT)(hres))

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
  public:
    CRequireModuleContext();

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

    HRESULT ReplaceModuleExports(__in DukTape::duk_idx_t nObjIndex, __in_opt BOOL bRemoveFromStack=TRUE);
    HRESULT ReplaceModuleExportsWithObject(__in CJsObjectBase *lpObject);

  private:
    friend CJavascriptVM;

    DukTape::duk_context *lpCtx;
    LPCWSTR szIdW;
    DukTape::duk_idx_t nRequireModuleIndex, nExportsObjectIndex, nModuleObjectIndex;
  };

  //--------

  typedef std::function<DukTape::duk_ret_t (__in DukTape::duk_context*)> protected_function;

public:
  CJavascriptVM();
  ~CJavascriptVM();

  VOID On(__in OnRequireModuleCallback cRequireModuleCallback);

  HRESULT Initialize();
  VOID Finalize();

  operator DukTape::duk_context*()
    {
    MX_ASSERT(lpCtx != NULL);
    return lpCtx;
    };

  static CJavascriptVM* FromContext(__in DukTape::duk_context *lpCtx);

  HRESULT Run(__in_z LPCSTR szCodeA, __in_z_opt LPCWSTR szFileNameW=NULL, __in_opt BOOL bIgnoreResult=TRUE);

  HRESULT RunNativeProtected(__in DukTape::duk_idx_t nArgsCount, __in DukTape::duk_idx_t nRetValuesCount,
                             __in protected_function func);

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

  HRESULT PushProperty(__in_z LPCSTR szPropertyNameA);

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

  HRESULT PushObjectProperty(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA);

  LPCSTR GetLastRunErrorMessage() const
    {
    return (LPCSTR)sLastExecError.cStrMessageA;
    };

  LPCSTR GetLastRunErrorFileName() const
    {
    return (LPCSTR)sLastExecError.cStrFileNameA;
    };

  ULONG GetLastRunErrorLineNumber() const
    {
    return sLastExecError.nLine;
    };

  LPCSTR GetLastRunErrorStackTrace() const
    {
    return (LPCSTR)sLastExecError.cStrStackTraceA;
    };

  static HRESULT AddSafeString(__inout CStringA &cStrCodeA, __in_z LPCSTR szStrA, __in_opt SIZE_T nStrLen=(SIZE_T)-1);
  static HRESULT AddSafeString(__inout CStringA &cStrCodeA, __in_z LPCWSTR szStrW, __in_opt SIZE_T nStrLen=(SIZE_T)-1);

private:
  static DukTape::duk_ret_t OnModSearch(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyHasPropHelper(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyGetPropHelper(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxySetPropHelper(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyDeletePropHelper(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _RunNativeProtectedHelper(__in DukTape::duk_context *lpCtx, __in void *udata);

  VOID GetErrorInfoFromException(__in DukTape::duk_idx_t nStackIndex);

protected:
  DukTape::duk_context *lpCtx;
  OnRequireModuleCallback cRequireModuleCallback;
  struct {
    HRESULT hRes;
    CStringA cStrMessageA;
    CStringA cStrFileNameA;
    ULONG nLine;
    CStringA cStrStackTraceA;
  } sLastExecError;
};

//-----------------------------------------------------------

class CJsObjectBase : public virtual CBaseMemObj, public TRefCounted<CJsObjectBase>
{
  MX_DISABLE_COPY_CONSTRUCTOR(CJsObjectBase);
public:
  CJsObjectBase(__in DukTape::duk_context *lpCtx) : CBaseMemObj(), TRefCounted<CJsObjectBase>()
    {
    MX_ASSERT(lpCtx != NULL);
    lpJvm = CJavascriptVM::FromContext(lpCtx);
    MX_ASSERT(lpJvm != NULL);
    return;
    };

  ~CJsObjectBase()
    { };

  virtual VOID PushThis() = 0;

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

  CJavascriptVM& GetJavascriptVM() const
    {
    return *lpJvm;
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

  static HRESULT _RegisterHelper(__in DukTape::duk_context *lpCtx, __in MAP_ENTRY *lpEntries,
                                 __in_z LPCSTR szObjectNameA, __in_z LPCSTR szPrototypeNameA,
                                 __in_opt lpfnCreateObject fnCreateObject, __in_opt int nCreateArgsCount,
                                 __in BOOL bCreateProxy);
  static DukTape::duk_ret_t _CreateHelper(__in DukTape::duk_context *lpCtx);
  VOID _PushThisHelper(__in_z LPCSTR szObjectNameA, __in_z LPCSTR szPrototypeNameA);
  static DukTape::duk_ret_t _FinalReleaseHelper(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _CallMethodHelper(__in DukTape::duk_context *lpCtx, __in lpfnCallFunc fnFunc);
  static DukTape::duk_ret_t _ProxyHasPropHelper(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyGetPropHelper(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxySetPropHelper(__in DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyDeletePropHelper(__in DukTape::duk_context *lpCtx);

private:
  CJavascriptVM *lpJvm;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JAVASCRIPT_VM_H
