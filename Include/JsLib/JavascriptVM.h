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
#ifndef _MX_JAVASCRIPT_VM_H
#define _MX_JAVASCRIPT_VM_H

#include "..\Defines.h"
#include "..\RefCounted.h"
#include "..\Callbacks.h"
#include "..\ArrayList.h"
#include "..\Debug.h"
#include "..\Strings\Strings.h"
#include "..\DateTime\DateTime.h"
#include <exception>
#include <stdexcept>
#include <functional>
namespace DukTape {
#include "DukTape\duktape.h"
} //namespace DukTape

//-----------------------------------------------------------

#define MX_JS_VARARGS             ((DukTape::duk_int_t)(-1))

#define MX_JS_DECLARE(_cls, _name)                                                              \
public:                                                                                         \
  static HRESULT Register(_In_ DukTape::duk_context *lpCtx)                                     \
    { return _Register(lpCtx, FALSE, NULL); };                                                  \
    __MX_JS_DECLARE_COMMON(_cls, _name)

#define MX_JS_DECLARE_CREATABLE(_cls, _name)                                                    \
public:                                                                                         \
  static HRESULT Register(_In_ DukTape::duk_context *lpCtx)                                     \
    { return _Register(lpCtx, FALSE, &_cls::_CreateObject); };                                  \
    __MX_JS_DECLARE_COMMON(_cls, _name)                                                         \
    __MX_JS_DECLARE_CREATE_OBJECT(_cls)

#define MX_JS_DECLARE_WITH_PROXY(_cls, _name)                                                   \
public:                                                                                         \
  static HRESULT Register(_In_ DukTape::duk_context *lpCtx)                                     \
    { return _Register(lpCtx, TRUE, NULL); };                                                   \
    __MX_JS_DECLARE_COMMON(_cls, _name)

#define MX_JS_DECLARE_CREATABLE_WITH_PROXY(_cls, _name)                                         \
public:                                                                                         \
  static HRESULT Register(_In_ DukTape::duk_context *lpCtx)                                     \
    { return _Register(lpCtx, TRUE, &_cls::_CreateObject); };                                   \
    __MX_JS_DECLARE_COMMON(_cls, _name)                                                         \
    __MX_JS_DECLARE_CREATE_OBJECT(_cls)

#define __MX_JS_DECLARE_COMMON(_cls, _name)                                                     \
public:                                                                                         \
  static HRESULT _Register(_In_ DukTape::duk_context *lpCtx, _In_ BOOL bUseProxy,               \
                           _In_opt_ lpfnCreateObject fnCreateObject)                            \
    {                                                                                           \
    return _RegisterHelper(lpCtx, _GetMapEntries(), bUseProxy, _name, fnCreateObject,           \
                           &_cls::OnRegister, &_cls::OnUnregister);                             \
    };                                                                                          \
                                                                                                \
public:                                                                                         \
  static HRESULT Unregister(_In_ DukTape::duk_context *lpCtx)                                   \
    {                                                                                           \
    _UnregisterHelper(lpCtx, _name, &_cls::OnUnregister);                                       \
    };                                                                                          \
                                                                                                \
  HRESULT PushThis(_In_ DukTape::duk_context *lpCtx)                                            \
    {                                                                                           \
    return _PushThisHelper(lpCtx, _name);                                                       \
    };

#define __MX_JS_DECLARE_CREATE_OBJECT(_cls)                                                     \
private:                                                                                        \
  static CJsObjectBase* _CreateObject()                                                         \
    {                                                                                           \
    return MX_DEBUG_NEW _cls();                                                                 \
    };

#define MX_JS_BEGIN_MAP(_cls)                                                                   \
private:                                                                                        \
  typedef DukTape::duk_ret_t (_cls::*lpfnFunc)(_In_ DukTape::duk_context *);                    \
                                                                                                \
  static MAP_ENTRY* _GetMapEntries()                                                            \
    {                                                                                           \
    static MAP_ENTRY _entries[] = {

#define MX_JS_END_MAP()                                                                         \
      { NULL, NULL, NULL, NULL, 0, 0, 0 }                                                       \
    };                                                                                          \
    return _entries;                                                                            \
    };

#define MX_JS_MAP_STATIC_METHOD(_name, _func, _argsCount)                                       \
        { _name,                                                                                \
          [](_In_ DukTape::duk_context *lpCtx) -> DukTape::duk_ret_t                            \
          {                                                                                     \
            return _CallStaticMethodHelper(lpCtx, static_cast<lpfnCallStaticFunc>(_func));      \
          },                                                                                    \
          NULL, NULL, _argsCount, 0, 1 },

#define MX_JS_MAP_METHOD(_name, _func, _argsCount)                                              \
        { _name,                                                                                \
          [](_In_ DukTape::duk_context *lpCtx) -> DukTape::duk_ret_t                            \
          {                                                                                     \
            return _CallMethodHelper(lpCtx, static_cast<lpfnCallFunc>(_func));                  \
          },                                                                                    \
          NULL, NULL, _argsCount, 0, 0 },

#define MX_JS_MAP_STATIC_PROPERTY(_name, _getFunc, _setFunc, _enumerable )                      \
        { _name, NULL,                                                                          \
          [](_In_ DukTape::duk_context *lpCtx) -> DukTape::duk_ret_t                            \
          {                                                                                     \
            return _CallStaticMethodHelper(lpCtx, static_cast<lpfnCallStaticFunc>(_getFunc));   \
          },                                                                                    \
          [](_In_ DukTape::duk_context *lpCtx) -> DukTape::duk_ret_t                            \
          {                                                                                     \
            if (_setFunc == NULL)                                                               \
            {                                                                                   \
              MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_NOTIMPL);                                      \
              return 0;                                                                         \
            }                                                                                   \
            return _CallStaticMethodHelper(lpCtx, static_cast<lpfnCallStaticFunc>(_setFunc));   \
          },                                                                                    \
          0, _enumerable, 1 },

#define MX_JS_MAP_PROPERTY(_name, _getFunc, _setFunc, _enumerable )                             \
        { _name, NULL,                                                                          \
          [](_In_ DukTape::duk_context *lpCtx) -> DukTape::duk_ret_t                            \
          {                                                                                     \
            return _CallMethodHelper(lpCtx, static_cast<lpfnCallFunc>(_getFunc));               \
          },                                                                                    \
          [](_In_ DukTape::duk_context *lpCtx) -> DukTape::duk_ret_t                            \
          {                                                                                     \
            if (_setFunc == NULL)                                                               \
            {                                                                                   \
              MX_JS_THROW_WINDOWS_ERROR(lpCtx, E_NOTIMPL);                                      \
              return 0;                                                                         \
            }                                                                                   \
            return _CallMethodHelper(lpCtx, static_cast<lpfnCallFunc>(_setFunc));               \
          },                                                                                    \
          0, _enumerable, 0 },

#define MX_JS_THROW_WINDOWS_ERROR(ctx, hr)                                                      \
          MX::CJavascriptVM::ThrowWindowsError((ctx), (HRESULT)(hr), (LPCSTR)(__FILE__),        \
                                               (DukTape::duk_int_t)__LINE__)

#define MX_JS_THROW_ERROR(ctx, code, fmt, ...)                                                  \
          DukTape::duk_error_raw((ctx), (DukTape::duk_errcode_t)(code), (LPCSTR)(__FILE__),     \
                                 (DukTape::duk_int_t)__LINE__, (fmt), __VA_ARGS__)

//-----------------------------------------------------------

namespace MX {

class CJsObjectBase;

class CJavascriptVM : public virtual CBaseMemObj, public CNonCopyableObj
{
public:
  enum class ePropertyFlags
  {
    Writable     = 0x01,
    Enumerable   = 0x02,
    Configurable = 0x04
  };

  friend inline ePropertyFlags operator|(ePropertyFlags lhs, ePropertyFlags rhs);
  friend inline ePropertyFlags operator&(ePropertyFlags lhs, ePropertyFlags rhs);
  friend inline ePropertyFlags operator~(ePropertyFlags v);

  //----

  class CRequireModuleContext;

  //--------

public:
  typedef Callback<HRESULT (_In_ DukTape::duk_context *lpCtx, _In_ CRequireModuleContext *lpReqContext,
                            _Inout_ CStringA &cStrCodeA)> OnRequireModuleCallback;

  typedef Callback<DukTape::duk_ret_t (_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                       _In_z_ LPCSTR szFunctionNameA)> OnNativeFunctionCallback;

  typedef Callback<DukTape::duk_ret_t (_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                       _In_z_ LPCSTR szPropertyNameA)> OnGetPropertyCallback;
  typedef Callback<DukTape::duk_ret_t (_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                       _In_z_ LPCSTR szPropertyNameA,
                                       _In_ DukTape::duk_idx_t nValueIndex)> OnSetPropertyCallback;

  //return 1 if has, 0 if has not, -1 to pass to original object
  typedef Callback<int (_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                        _In_z_ LPCSTR szPropertyNameA)> OnProxyHasNamedPropertyCallback;
  typedef Callback<int (_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                        _In_ int nIndex)> OnProxyHasIndexedPropertyCallback;

  //return 1 if a value was pushed, 0 to pass to original object, -1 to throw an error
  typedef Callback<int (_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                        _In_z_ LPCSTR szPropertyNameA)> OnProxyGetNamedPropertyCallback;
  typedef Callback<int (_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                        _In_ int nIndex)> OnProxyGetIndexedPropertyCallback;

  //return 1 if a new value was pushed, 0 to set the original passed value, -1 to throw an error
  typedef Callback<int (_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA,
                        _In_ DukTape::duk_idx_t nValueIndex)> OnProxySetNamedPropertyCallback;
  typedef Callback<int (_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA, _In_ int nIndex,
                        _In_ DukTape::duk_idx_t nValueIndex)> OnProxySetIndexedPropertyCallback;

  //return 1 if delete must proceed, 0 to silently ignore, -1 to throw an error
  typedef Callback<int (_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                        _In_z_ LPCSTR szPropertyNameA)> OnProxyDeleteNamedPropertyCallback;
  typedef Callback<int (_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                        _In_ int nIndex)> OnProxyDeleteIndexedPropertyCallback;

  //return NULL/Empty String to end enumeration
  typedef Callback<LPCSTR (_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                           _In_ int nIndex)> OnProxyGetPropertyNameCallback;

  //--------

  class CProxyCallbacks : public virtual CBaseMemObj
  {
  public:
    CProxyCallbacks() : CBaseMemObj()
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

    CProxyCallbacks(CProxyCallbacks const &cSrc) : CBaseMemObj()
      {
      operator=(cSrc);
      return;
      };

    CProxyCallbacks& operator=(CProxyCallbacks const &cSrc)
      {
      cProxyHasNamedPropertyCallback = cSrc.cProxyHasNamedPropertyCallback;
      cProxyHasIndexedPropertyCallback = cSrc.cProxyHasIndexedPropertyCallback;
      cProxyGetNamedPropertyCallback = cSrc.cProxyGetNamedPropertyCallback;
      cProxyGetIndexedPropertyCallback = cSrc.cProxyGetIndexedPropertyCallback;
      cProxySetNamedPropertyCallback = cSrc.cProxySetNamedPropertyCallback;
      cProxySetIndexedPropertyCallback = cSrc.cProxySetIndexedPropertyCallback;
      cProxyDeleteNamedPropertyCallback = cSrc.cProxyDeleteNamedPropertyCallback;
      cProxyDeleteIndexedPropertyCallback = cSrc.cProxyDeleteIndexedPropertyCallback;
      cProxyGetPropertyNameCallback = cSrc.cProxyGetPropertyNameCallback;
      return *this;
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

    void serialize(_In_ void *p);
    void deserialize(_In_ void *p);
    static size_t serialization_buffer_size();
  };

  //--------

  class CRequireModuleContext : public virtual CBaseMemObj
  {
  protected:
    CRequireModuleContext(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCWSTR szIdW,
                          _In_ DukTape::duk_idx_t nModuleObjectIndex, _In_ DukTape::duk_idx_t nExportsObjectIndex);

  public:
    LPCWSTR GetId() const
      {
      return szIdW;
      };

    HRESULT RequireModule(_In_z_ LPCWSTR szModuleIdW);

    HRESULT AddNativeFunction(_In_z_ LPCSTR szFuncNameA, _In_ OnNativeFunctionCallback cNativeFunctionCallback,
                              _In_ int nArgsCount);

    HRESULT AddProperty(_In_z_ LPCSTR szPropertyNameA, _In_opt_ BOOL bInitialValueOnStack = FALSE,
                        _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
    HRESULT AddStringProperty(_In_z_ LPCSTR szPropertyNameA, _In_z_ LPCSTR szValueA,
                              _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
    HRESULT AddBooleanProperty(_In_z_ LPCSTR szPropertyNameA, _In_ BOOL bValue,
                               _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
    HRESULT AddNumericProperty(_In_z_ LPCSTR szPropertyNameA, _In_ double nValue,
                               _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
    HRESULT AddNullProperty(_In_z_ LPCSTR szPropertyNameA,
                            _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
    HRESULT AddJsObjectProperty(_In_z_ LPCSTR szPropertyNameA, _In_ CJsObjectBase *lpObject,
                                _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
    HRESULT AddPropertyWithCallback(_In_z_ LPCSTR szPropertyNameA, _In_ OnGetPropertyCallback cGetValueCallback,
                                _In_opt_ OnSetPropertyCallback cSetValueCallback = NullCallback(),
                                _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Enumerable);

    HRESULT ReplaceModuleExports();
    HRESULT ReplaceModuleExportsWithObject(_In_ CJsObjectBase *lpObject);

  private:
    friend CJavascriptVM;

    DukTape::duk_context *lpCtx;
    LPCWSTR szIdW;
    DukTape::duk_idx_t nModuleObjectIndex, nExportsObjectIndex;
  };

  //--------

  typedef std::function<VOID (_In_ DukTape::duk_context *lpCtx)> lpfnProtectedFunction;

  typedef VOID (*lpfnThrowExceptionCallback)(_In_ DukTape::duk_context *lpCtx,
                                             _In_ DukTape::duk_idx_t nExceptionObjectIndex);

public:
  CJavascriptVM();
  ~CJavascriptVM();

  VOID SetRequireModuleCallback(_In_ OnRequireModuleCallback cRequireModuleCallback);

  HRESULT Initialize();
  VOID Finalize();

  __inline operator DukTape::duk_context*() const
    {
    MX_ASSERT(lpCtx != NULL);
    return lpCtx;
    };

  static CJavascriptVM* FromContext(_In_ DukTape::duk_context *lpCtx);

  VOID Run(_In_z_ LPCSTR szCodeA, _In_opt_z_ LPCWSTR szFileNameW = NULL, _In_opt_ BOOL bIgnoreResult = TRUE);

  static VOID RunNativeProtected(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nArgsCount,
                                 _In_ DukTape::duk_idx_t nRetValuesCount, _In_ lpfnProtectedFunction fnFunc,
                                 _In_opt_ BOOL bCatchUnhandled = FALSE);
  VOID RunNativeProtected(_In_ DukTape::duk_idx_t nArgsCount, _In_ DukTape::duk_idx_t nRetValuesCount,
                          _In_ lpfnProtectedFunction fnFunc, _In_opt_ BOOL bCatchUnhandled = FALSE);

  static HRESULT RunNativeProtectedAndGetError(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nArgsCount,
                                               _In_ DukTape::duk_idx_t nRetValuesCount,
                                               _In_ lpfnProtectedFunction fnFunc,
                                               _In_opt_ BOOL bCatchUnhandled = FALSE);
  HRESULT RunNativeProtectedAndGetError(_In_ DukTape::duk_idx_t nArgsCount, _In_ DukTape::duk_idx_t nRetValuesCount,
                                        _In_ lpfnProtectedFunction fnFunc, _In_opt_ BOOL bCatchUnhandled = FALSE);

  HRESULT RegisterException(_In_z_ LPCSTR szExceptionNameA, _In_ lpfnThrowExceptionCallback fnThrowExceptionCallback);
  HRESULT UnregisterException(_In_z_ LPCSTR szExceptionNameA);

  HRESULT AddNativeFunction(_In_z_ LPCSTR szFuncNameA, _In_ OnNativeFunctionCallback cNativeFunctionCallback,
                            _In_ int nArgsCount);

  HRESULT AddProperty(_In_z_ LPCSTR szPropertyNameA, _In_opt_ BOOL bInitialValueOnStack = FALSE,
                      _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
  HRESULT AddStringProperty(_In_z_ LPCSTR szPropertyNameA, _In_z_ LPCSTR szValueA,
                            _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
  HRESULT AddStringProperty(_In_z_ LPCSTR szPropertyNameA, _In_z_ LPCWSTR szValueW,
                            _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
  HRESULT AddBooleanProperty(_In_z_ LPCSTR szPropertyNameA, _In_ BOOL bValue,
                             _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
  HRESULT AddIntegerProperty(_In_z_ LPCSTR szPropertyNameA, _In_ int nValue,
                             _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
  HRESULT AddNumericProperty(_In_z_ LPCSTR szPropertyNameA, _In_ double nValue,
                             _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
  HRESULT AddNullProperty(_In_z_ LPCSTR szPropertyNameA,
                          _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
  HRESULT AddJsObjectProperty(_In_z_ LPCSTR szPropertyNameA, _In_ CJsObjectBase *lpObject,
                              _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
  HRESULT AddPropertyWithCallback(_In_z_ LPCSTR szPropertyNameA, _In_ OnGetPropertyCallback cGetValueCallback,
                                  _In_opt_ OnSetPropertyCallback cSetValueCallback = NullCallback(),
                                  _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Enumerable);

  HRESULT RemoveProperty(_In_z_ LPCSTR szPropertyNameA);

  HRESULT HasProperty(_In_z_ LPCSTR szPropertyNameA);

  VOID PushProperty(_In_z_ LPCSTR szPropertyNameA);

  HRESULT CreateObject(_In_z_ LPCSTR szObjectNameA, _In_opt_ CProxyCallbacks *lpCallbacks = NULL);

  HRESULT AddObjectNativeFunction(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szFuncNameA,
                                  _In_ OnNativeFunctionCallback cNativeFunctionCallback, _In_ int nArgsCount);

  HRESULT AddObjectProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA,
                            _In_opt_ BOOL bInitialValueOnStack = FALSE,
                            _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
  HRESULT AddObjectStringProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA, _In_z_ LPCSTR szValueA,
                                  _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
  HRESULT AddObjectStringProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA, _In_z_ LPCWSTR szValueW,
                                  _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
  HRESULT AddObjectBooleanProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA, _In_ BOOL bValue,
                                   _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
  HRESULT AddObjectIntegerProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA, _In_ int nValue,
                                   _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
  HRESULT AddObjectNumericProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA, _In_ double nValue,
                                   _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
  HRESULT AddObjectNullProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA,
                                _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
  HRESULT AddObjectJsObjectProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA,
                                    _In_ CJsObjectBase *lpObject,
                                    _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Writable | ePropertyFlags::Enumerable);
  HRESULT AddObjectPropertyWithCallback(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA,
                                        _In_ OnGetPropertyCallback cGetValueCallback,
                                        _In_opt_ OnSetPropertyCallback cSetValueCallback = NullCallback(),
                                        _In_opt_ ePropertyFlags nFlags = ePropertyFlags::Enumerable);

  HRESULT RemoveObjectProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA);

  HRESULT HasObjectProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA);

  VOID PushObjectProperty(_In_z_ LPCSTR szObjectNameA, _In_z_ LPCSTR szPropertyNameA);

  //converts from/to utf-8
  static VOID PushString(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCWSTR szStrW, _In_opt_ SIZE_T nStrLen = (SIZE_T)-1);
  VOID PushString(_In_z_ LPCWSTR szStrW, _In_opt_ SIZE_T nStrLen = (SIZE_T)-1);

  static VOID GetString(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nStackIndex, _Inout_ CStringW &cStrW,
                        _In_opt_ BOOL bAppend = FALSE);
  VOID GetString(_In_ DukTape::duk_idx_t nStackIndex, _Inout_ CStringW &cStrW, _In_opt_ BOOL bAppend = FALSE);

  //convert SYSTEMTIME from/to JS Date object
  static VOID PushDate(_In_ DukTape::duk_context *lpCtx, _In_ LPSYSTEMTIME lpSt, _In_opt_ BOOL bAsUtc = TRUE);
  VOID PushDate(_In_ LPSYSTEMTIME lpSt, _In_opt_ BOOL bAsUtc = TRUE);
  static VOID PushDate(_In_ DukTape::duk_context *lpCtx, _In_ CDateTime &cDt, _In_opt_ BOOL bAsUtc = TRUE);
  VOID PushDate(_In_ CDateTime &cDt, _In_opt_ BOOL bAsUtc = TRUE);

  static VOID GetDate(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nObjIdx, _Out_ LPSYSTEMTIME lpSt);
  VOID GetDate(_In_ DukTape::duk_idx_t nObjIdx, _Out_ LPSYSTEMTIME lpSt);
  static VOID GetDate(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nObjIdx, _Out_ CDateTime &cDt);
  VOID GetDate(_In_ DukTape::duk_idx_t nObjIdx, _Out_ CDateTime &cDt);

  //NOTE: Setting 'var a=0;' can lead to 'a == FALSE'. No idea if JS or DukTape.
  //      These methods checks for boolean values first and for integer values
  //IMPORTANT: Unsafe execution. May throw exception if object at specified stack position is not a number.
  static DukTape::duk_int_t GetInt(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nObjIdx);
  DukTape::duk_int_t GetInt(_In_ DukTape::duk_idx_t nObjIdx);

  static DukTape::duk_uint_t GetUInt(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nObjIdx);
  DukTape::duk_uint_t GetUInt(_In_ DukTape::duk_idx_t nObjIdx);

  static DukTape::duk_double_t GetDouble(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nObjIdx);
  DukTape::duk_double_t GetDouble(_In_ DukTape::duk_idx_t nObjIdx);

  static VOID GetObjectType(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nObjIdx,
                            _Out_ MX::CStringA &cStrTypeA);
  VOID GetObjectType(_In_ DukTape::duk_idx_t nObjIdx, _Out_ MX::CStringA &cStrTypeA);

  HRESULT Reset();
  HRESULT RunGC();

  static HRESULT AddSafeString(_Inout_ CStringA &cStrCodeA, _In_z_ LPCSTR szStrA,
                               _In_opt_ SIZE_T nStrLen = (SIZE_T)-1);
  static HRESULT AddSafeString(_Inout_ CStringA &cStrCodeA, _In_z_ LPCWSTR szStrW,
                               _In_opt_ SIZE_T nStrLen = (SIZE_T)-1);

  static VOID ThrowWindowsError(_In_ DukTape::duk_context *lpCtx, _In_ HRESULT hr, _In_opt_z_ LPCSTR filename = NULL,
                                _In_opt_ DukTape::duk_int_t line = 0);
  VOID ThrowWindowsError(_In_ HRESULT hr, _In_opt_z_ LPCSTR filename = NULL, _In_opt_ DukTape::duk_int_t line = 0);

  static HRESULT AddBigIntegerSupport(_In_ DukTape::duk_context *lpCtx);
  HRESULT AddBigIntegerSupport();

private:
  //static DukTape::duk_ret_t OnModSearch(_In_ DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t OnNodeJsResolveModule(_In_ DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t OnNodeJsLoadModule(_In_ DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyHasPropHelper(_In_ DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyGetPropHelper(_In_ DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxySetPropHelper(_In_ DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyDeletePropHelper(_In_ DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyOwnKeysHelper(_In_ DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _RunNativeProtectedHelper(_In_ DukTape::duk_context *lpCtx, _In_ void *udata);

  static BOOL HandleException(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nStackIndex,
                              _In_opt_ BOOL bCatchUnhandled);

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

class MX_NOVTABLE CJsObjectBase : public virtual TRefCounted<CBaseMemObj>
{
protected:
  CJsObjectBase() : TRefCounted<CBaseMemObj>()
    {
    return;
    };

public:
  ~CJsObjectBase()
    { };

  virtual HRESULT PushThis(_In_ DukTape::duk_context *lpCtx) = 0;

  static CJsObjectBase* FromObject(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_int_t nIndex);

  //return 1 if has, 0 if has not, -1 to pass to original object
  virtual int OnProxyHasNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szPropNameA);
  virtual int OnProxyHasIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_ int nIndex);

  //return 1 if a value was pushed, 0 to pass to original object, -1 to throw an error
  virtual int OnProxyGetNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szPropNameA);
  virtual int OnProxyGetIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_ int nIndex);

  //return 1 if a new value was pushed, 0 to set the original passed value, -1 to throw an error
  virtual int OnProxySetNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szPropNameA,
                                      _In_ DukTape::duk_idx_t nValueIndex);
  virtual int OnProxySetIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_ int nIndex,
                                        _In_ DukTape::duk_idx_t nValueIndex);

  //return 1 if delete must proceed, 0 to silently ignore, -1 to throw an error
  virtual int OnProxyDeleteNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szPropNameA);
  virtual int OnProxyDeleteIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_ int nIndex);

  //return NULL/Empty String to end enumeration
  virtual LPCSTR OnProxyGetPropertyName(_In_ DukTape::duk_context *lpCtx, _In_ int nIndex);

protected:
  typedef DukTape::duk_ret_t (CJsObjectBase::*lpfnCallFunc)(_In_ DukTape::duk_context *lpCtx);
  typedef DukTape::duk_ret_t (*lpfnCallStaticFunc)(_In_ DukTape::duk_context *lpCtx);

  typedef struct {
    LPCSTR szNameA;
    DukTape::duk_c_function fnInvoke, fnGetter, fnSetter;
    DukTape::duk_idx_t nArgsCount;
    ULONG nEnumerable : 1;
    ULONG nStatic : 1;
  } MAP_ENTRY;

protected:
  typedef CJsObjectBase* (*lpfnCreateObject)();
  typedef VOID (*lpfnRegisterUnregisterCallback)(_In_ DukTape::duk_context *lpCtx);

  static VOID OnRegister(_In_ DukTape::duk_context *lpCtx)
    { };
  static VOID OnUnregister(_In_ DukTape::duk_context *lpCtx)
    { };

  static HRESULT _RegisterHelper(_In_ DukTape::duk_context *lpCtx, _In_ MAP_ENTRY *lpEntries, _In_ BOOL bUseProxy,
                                 _In_z_ LPCSTR szObjectNameA, _In_opt_ lpfnCreateObject fnCreateObject,
                                 _In_ lpfnRegisterUnregisterCallback fnRegisterCallback,
                                 _In_ lpfnRegisterUnregisterCallback fnUnregisterCallback);
  static VOID _UnregisterHelper(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                _In_ lpfnRegisterUnregisterCallback fnUnregisterCallback);

  static DukTape::duk_ret_t _ConstructorHelper(_In_ DukTape::duk_context *lpCtx);
  HRESULT _PushThisHelper(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA);
  static VOID _SetupMapEntries(_In_ DukTape::duk_context *lpCtx, _In_ MAP_ENTRY *lpEntries, _In_ BOOL bStatic);
  static DukTape::duk_ret_t _FinalReleaseHelper(_In_ DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _CallMethodHelper(_In_ DukTape::duk_context *lpCtx, _In_ lpfnCallFunc fnFunc);
  static DukTape::duk_ret_t _CallStaticMethodHelper(_In_ DukTape::duk_context *lpCtx, _In_ lpfnCallStaticFunc fnFunc);
  static DukTape::duk_ret_t _ProxyHasPropHelper(_In_ DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyGetPropHelper(_In_ DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxySetPropHelper(_In_ DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyDeletePropHelper(_In_ DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t _ProxyOwnKeysHelper(_In_ DukTape::duk_context *lpCtx);
};

//-----------------------------------------------------------

class CJsError
{
protected:
  CJsError();
  CJsError(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nStackIndex);
  CJsError(_In_z_ LPCSTR szMessageA, _In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szFileNameA, _In_ ULONG nLine);

public:
  CJsError(_In_ const CJsError &obj);
  CJsError& operator=(_In_ const CJsError &obj);

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
  class CRefCountedStringA : public TRefCounted<CStringA>
  {
  };

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
  CJsWindowsError(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nStackIndex);
  CJsWindowsError(_In_ HRESULT hRes, _In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nStackIndex);
  CJsWindowsError(_In_ HRESULT hRes, _In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szFileNameA, _In_ ULONG nLine);
  CJsWindowsError(_In_ HRESULT hRes);

public:
  CJsWindowsError(_In_ const CJsWindowsError &obj);
  CJsWindowsError& operator=(_In_ const CJsWindowsError &obj);

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

inline CJavascriptVM::ePropertyFlags operator|(CJavascriptVM::ePropertyFlags lhs, CJavascriptVM::ePropertyFlags rhs)
{
  return static_cast<CJavascriptVM::ePropertyFlags>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

inline CJavascriptVM::ePropertyFlags operator&(CJavascriptVM::ePropertyFlags lhs, CJavascriptVM::ePropertyFlags rhs)
{
  return static_cast<CJavascriptVM::ePropertyFlags>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

inline CJavascriptVM::ePropertyFlags operator~(CJavascriptVM::ePropertyFlags v)
{
  return static_cast<CJavascriptVM::ePropertyFlags>(~static_cast<int>(v));
}

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JAVASCRIPT_VM_H
