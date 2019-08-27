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
#include "TestJavascript.h"
#include <JsLib\JavascriptVM.h>
#include <Strings\Strings.h>
#include <Strings\Utf8.h>

//-----------------------------------------------------------

static HRESULT OnRequireModule(_In_ DukTape::duk_context *lpCtx,
                               _In_ MX::CJavascriptVM::CRequireModuleContext *lpReqCtx,
                               _Inout_ MX::CStringA &cStrCodeA);

static DukTape::duk_ret_t OnGetSessionValue2(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                             _In_z_ LPCSTR szPropertyNameA);
static DukTape::duk_ret_t OnSetSessionValue2(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                             _In_z_ LPCSTR szPropertyNameA, _In_ DukTape::duk_idx_t nValueIndex);

static int OnProxyHasNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                   _In_z_ LPCSTR szPropertyNameA);
static int OnProxyHasIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA, _In_ int nIndex);
static int OnProxyGetNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                   _In_z_ LPCSTR szPropertyNameA);
static int OnProxyGetIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA, _In_ int nIndex);
static int OnProxySetNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                   _In_z_ LPCSTR szPropertyNameA, _In_ DukTape::duk_idx_t nValueIndex);
static int OnProxySetIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA, _In_ int nIndex,
                                     _In_ DukTape::duk_idx_t nValueIndex);
static int OnProxyDeleteNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                      _In_z_ LPCSTR szPropertyNameA);
static int OnProxyDeleteIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA, _In_ int nIndex);

//-----------------------------------------------------------

class CMyObject : public MX::CJsObjectBase
{
public:
  CMyObject(_In_ DukTape::duk_context *lpCtx);
  ~CMyObject();

  MX_JS_DECLARE_CREATABLE_WITH_PROXY(CMyObject, "myobject")

  MX_JS_BEGIN_MAP(CMyObject)
    MX_JS_MAP_METHOD("add", &CMyObject::add, 2)
    MX_JS_MAP_METHOD("Print", &CMyObject::Print, 1)
  MX_JS_END_MAP()

public:
  DukTape::duk_ret_t add();
  DukTape::duk_ret_t Print();

  int OnProxyHasNamedProperty(_In_z_ LPCSTR szPropNameA);
  int OnProxyHasIndexedProperty(_In_ int nIndex);

  int OnProxyGetNamedProperty(_In_z_ LPCSTR szPropNameA);
  int OnProxyGetIndexedProperty(_In_ int nIndex);

  int OnProxySetNamedProperty(_In_z_ LPCSTR szPropNameA, _In_ DukTape::duk_idx_t nValueIndex);
  int OnProxySetIndexedProperty(_In_ int nIndex, _In_ DukTape::duk_idx_t nValueIndex);

  int OnProxyDeleteNamedProperty(_In_z_ LPCSTR szPropNameA);
  int OnProxyDeleteIndexedProperty(_In_ int nIndex);
};

//-----------------------------------------------------------

int TestJavascript(_In_ DWORD dwLogLevel)
{
  MX::CJavascriptVM cJvm;
  HRESULT hRes;

  wprintf_s(L"Initializing Javascript VM... ");
  cJvm.SetRequireModuleCallback(MX_BIND_CALLBACK(&OnRequireModule));
  hRes = cJvm.Initialize();
  if (FAILED(hRes))
  {
    wprintf_s(L"Error: %08X\n", hRes);
    return (int)hRes;
  }
  wprintf_s(L"OK\n");

  wprintf_s(L"Registering 'myobject'... ");
  hRes = CMyObject::Register(cJvm);
  if (FAILED(hRes))
  {
    wprintf_s(L"Error: %08X\n", hRes);
    return (int)hRes;
  }
  wprintf_s(L"OK\n");

  wprintf_s(L"Testing call to zarasa...\n");
  try
  {
    cJvm.Run("zarasa('hola');");
  }
  catch (MX::CJsError &e)
  {
    wprintf_s(L"Error: %S in %S(%lu)\n", e.GetDescription(), e.GetFileName(), e.GetLineNumber());
    wprintf_s(L"%S\n", e.GetStackTrace());
  }

  //--------

  wprintf_s(L"Registering 'Session'... ");
  hRes = cJvm.AddNumericProperty("globalValue", 10.0);
  if (SUCCEEDED(hRes))
  {
    MX::CJavascriptVM::CProxyCallbacks cCallbacks;

    cCallbacks.cProxyHasNamedPropertyCallback = MX_BIND_CALLBACK(&OnProxyHasNamedProperty);
    cCallbacks.cProxyHasIndexedPropertyCallback = MX_BIND_CALLBACK(&OnProxyHasIndexedProperty);
    cCallbacks.cProxyGetNamedPropertyCallback = MX_BIND_CALLBACK(&OnProxyGetNamedProperty);
    cCallbacks.cProxyGetIndexedPropertyCallback = MX_BIND_CALLBACK(&OnProxyGetIndexedProperty);
    cCallbacks.cProxySetNamedPropertyCallback = MX_BIND_CALLBACK(&OnProxySetNamedProperty);
    cCallbacks.cProxySetIndexedPropertyCallback = MX_BIND_CALLBACK(&OnProxySetIndexedProperty);
    cCallbacks.cProxyDeleteNamedPropertyCallback = MX_BIND_CALLBACK(&OnProxyDeleteNamedProperty);
    cCallbacks.cProxyDeleteIndexedPropertyCallback = MX_BIND_CALLBACK(&OnProxyDeleteIndexedProperty);
    hRes = cJvm.CreateObject("session", &cCallbacks);
  }
  if (SUCCEEDED(hRes))
    hRes = cJvm.AddObjectNumericProperty("session", "value", 20.0);
  if (SUCCEEDED(hRes))
  {
    hRes = cJvm.AddObjectPropertyWithCallback("session", "value2", MX_BIND_CALLBACK(&OnGetSessionValue2),
                                              MX_BIND_CALLBACK(&OnSetSessionValue2));
  }
  if (FAILED(hRes))
  {
    wprintf_s(L"Error: %08X\n", hRes);
    return (int)hRes;
  }
  wprintf_s(L"OK\n");

  wprintf_s(L"Testing myobject creation...\n");
  try
  {
    cJvm.Run("var ext = require('../child/add.js');\n"
             "obj = new myobject();\n"
             "obj.blabla = 2;\n"
             "obj[1] = obj.blabla;\n"
             "delete obj.blabla;\n"
             "obj[1] = 'ñandú';\n" //�and�
             "obj.Print('          v1: ' + globalValue.toString());\n"
             "obj.Print('          v2: ' + session.value.toString());\n"
             "obj.Print('          Suma: ' + ext.add(globalValue, session['value']).toString());\n"
             "obj.Print('Multiplicacion: ' + ext.mult(globalValue, session['value']).toString());\n"
             "session.value2 = 200.0;\n"
             "obj.Print('Session value2: ' + session.value2.toString());\n",
             L"c�digo/html/main/index.js");
  }
  catch (MX::CJsError &e)
  {
    wprintf_s(L"Error: %S in %S(%lu)\n", e.GetDescription(), e.GetFileName(), e.GetLineNumber());
    wprintf_s(L"%S\n", e.GetStackTrace());
  }

  //done
  return (int)S_OK;
}

static HRESULT OnRequireModule(_In_ DukTape::duk_context *lpCtx,
                               _In_ MX::CJavascriptVM::CRequireModuleContext *lpReqCtx,
                               _Inout_ MX::CStringA &cStrCodeA)
{
  if (wcscmp(lpReqCtx->GetId(), L"c�digo/html/child/add.js") == 0)
  {
    return (cStrCodeA.Copy("DebugPrint('Start Hi from add.js');\n"
                           "var _mult = require('./mult.js');\n"
                           "exports.add = function()\n"
                           "{\n"
                           "  var sum = 0, i = 0, args = arguments, l = args.length;\n"
                           "  while (i < l)\n"
                           "  {\n"
                           "    sum += args[i++];\n"
                           "  }\n"
                           "  return sum;\n"
                           "};\n"
                           "exports.mult = function()\n"
                           "{\n"
                           "  var mul = 1, i = 0, args = arguments, l = args.length;\n"
                           "  while (i < l)\n"
                           "  {\n"
                           "    mul = _mult.mult(mul, args[i++]);\n"
                           "  }\n"
                           "  return mul;\n"
                           "};\n"
                           "DebugPrint('End Hi from add.js');\n") != FALSE) ? S_OK : E_OUTOFMEMORY;
  }
  else if (wcscmp(lpReqCtx->GetId(), L"c�digo/html/child/mult.js") == 0)
  {
    return (cStrCodeA.Copy("DebugPrint('Start Hi from mult.js');\n"
                           "exports.mult = function(a, b)\n"
                           "{\n"
                           "  return a*b;\n"
                           "};\n"
                           "DebugPrint('End Hi from mult.js');\n") != FALSE) ? S_OK : E_OUTOFMEMORY;
  }
  return E_NOTIMPL;
}

//-----------------------------------------------------------

static double nSessionValue2 = 100.0;

static DukTape::duk_ret_t OnGetSessionValue2(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                             _In_z_ LPCSTR szPropertyNameA)
{
  DukTape::duk_push_number(lpCtx, nSessionValue2);
  return 1;
}

static DukTape::duk_ret_t OnSetSessionValue2(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                             _In_z_ LPCSTR szPropertyNameA, _In_ DukTape::duk_idx_t nValueIndex)
{
  nSessionValue2 = duk_require_number(lpCtx, nValueIndex);
  return DUK_ERR_NONE;
}

//-----------------------------------------------------------

static int OnProxyHasNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                   _In_z_ LPCSTR szPropertyNameA)
{
  if (szObjectNameA == NULL)
    szObjectNameA = "global";
  wprintf_s(L"[Object: %S] Checking for property '%S'\n", szObjectNameA, szPropertyNameA);
  return -1; //pass original
}

static int OnProxyHasIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA, _In_ int nIndex)
{
  if (szObjectNameA == NULL)
    szObjectNameA = "global";
  wprintf_s(L"[Object: %S] Checking for indexed property #%ld\n", szObjectNameA, nIndex);
  return -1; //pass original
}

static int OnProxyGetNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                   _In_z_ LPCSTR szPropertyNameA)
{
  if (szObjectNameA == NULL)
    szObjectNameA = "global";
  wprintf_s(L"[Object: %S] Retrieving value of property '%S'\n", szObjectNameA, szPropertyNameA);
  return 0; //pass original
}

static int OnProxyGetIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA, _In_ int nIndex)
{
  if (szObjectNameA == NULL)
    szObjectNameA = "global";
  wprintf_s(L"[Object: %S] Retrieving value of indexed property #%ld\n", szObjectNameA, nIndex);
  return 0; //pass original
}

static int OnProxySetNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                   _In_z_ LPCSTR szPropertyNameA, _In_ DukTape::duk_idx_t nValueIndex)
{
  if (szObjectNameA == NULL)
    szObjectNameA = "global";
  DukTape::duk_dup(lpCtx, nValueIndex);
  DukTape::duk_to_string(lpCtx, -1);
  wprintf_s(L"[Object: %S] Setting value of property '%S' to \"%S\"\n", szObjectNameA, szPropertyNameA,
            DukTape::duk_get_string(lpCtx, -1));
  DukTape::duk_pop(lpCtx);
  return 0; //set original
}

static int OnProxySetIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA, _In_ int nIndex,
                                     _In_ DukTape::duk_idx_t nValueIndex)
{
  if (szObjectNameA == NULL)
    szObjectNameA = "global";
  DukTape::duk_dup(lpCtx, nValueIndex);
  DukTape::duk_to_string(lpCtx, -1);
  wprintf_s(L"[Object: %S] Setting value of indexed property #%ld to \"%S\"\n", szObjectNameA, nIndex,
            DukTape::duk_get_string(lpCtx, -1));
  DukTape::duk_pop(lpCtx);
  return 0; //set original
}

static int OnProxyDeleteNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                      _In_z_ LPCSTR szPropertyNameA)
{
  if (szObjectNameA == NULL)
    szObjectNameA = "global";
  wprintf_s(L"[Object: %S] Deleting property '%S'\n", szObjectNameA, szPropertyNameA);
  return 1; //delete original
}

static int OnProxyDeleteIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA, _In_ int nIndex)
{
  if (szObjectNameA == NULL)
    szObjectNameA = "global";
  wprintf_s(L"[Object: %S] Deleting property #%ld\n", szObjectNameA, nIndex);
  return 1; //delete original
}

//-----------------------------------------------------------

CMyObject::CMyObject(_In_ DukTape::duk_context *lpCtx) : MX::CJsObjectBase(lpCtx)
{
  wprintf_s(L"[CMyObject] Constructor\n");
  return;
}

CMyObject::~CMyObject()
{
  wprintf_s(L"[CMyObject] Destructor\n");
  return;
}

DukTape::duk_ret_t CMyObject::add()
{
  double n1 = DukTape::duk_require_number(GetContext(), 0);
  double n2 = DukTape::duk_require_number(GetContext(), 1);

  DukTape::duk_push_number(GetContext(), n1+n2);
  return 1;
}

DukTape::duk_ret_t CMyObject::Print()
{
  LPCSTR szBufA;
  MX::CStringW cStrTempW;

  szBufA = duk_require_string(GetContext(), 0);
  MX::Utf8_Decode(cStrTempW, szBufA);
  wprintf_s(L"[CMyObject] %s\n", (LPWSTR)cStrTempW);
  return 0;
}

int CMyObject::OnProxyHasNamedProperty(_In_z_ LPCSTR szPropNameA)
{
  wprintf_s(L"[CMyObject] Checking for property '%S'\n", szPropNameA);
  return MX::CJsObjectBase::OnProxyHasNamedProperty(szPropNameA);
}

int CMyObject::OnProxyHasIndexedProperty(_In_ int nIndex)
{
  wprintf_s(L"[CMyObject] Checking for indexed property #%ld\n", nIndex);
  return MX::CJsObjectBase::OnProxyHasIndexedProperty(nIndex);
}

int CMyObject::OnProxyGetNamedProperty(_In_z_ LPCSTR szPropNameA)
{
  wprintf_s(L"[CMyObject] Retrieving value of property '%S'\n", szPropNameA);
  return MX::CJsObjectBase::OnProxyGetNamedProperty(szPropNameA);
}

int CMyObject::OnProxyGetIndexedProperty(_In_ int nIndex)
{
  wprintf_s(L"[CMyObject] Retrieving value of indexed property #%ld\n", nIndex);
  return MX::CJsObjectBase::OnProxyGetIndexedProperty(nIndex);
}

int CMyObject::OnProxySetNamedProperty(_In_z_ LPCSTR szPropNameA, _In_ DukTape::duk_idx_t nValueIndex)
{
  DukTape::duk_dup(GetContext(), nValueIndex);
  DukTape::duk_to_string(GetContext(), -1);
  wprintf_s(L"[CMyObject] Setting value of property '%S' to \"%S\"\n", szPropNameA,
            DukTape::duk_get_string(GetContext(), -1));
  DukTape::duk_pop(GetContext());
  return MX::CJsObjectBase::OnProxySetNamedProperty(szPropNameA, nValueIndex);
}

int CMyObject::OnProxySetIndexedProperty(_In_ int nIndex, _In_ DukTape::duk_idx_t nValueIndex)
{
  DukTape::duk_dup(GetContext(), nValueIndex);
  DukTape::duk_to_string(GetContext(), -1);
  wprintf_s(L"[CMyObject] Setting value of indexed property #%ld to \"%S\"\n", nIndex,
            DukTape::duk_get_string(GetContext(), -1));
  DukTape::duk_pop(GetContext());
  return MX::CJsObjectBase::OnProxySetIndexedProperty(nIndex, nValueIndex);
}

int CMyObject::OnProxyDeleteNamedProperty(_In_z_ LPCSTR szPropNameA)
{
  wprintf_s(L"[CMyObject] Deleting property '%S'\n", szPropNameA);
  return MX::CJsObjectBase::OnProxyGetNamedProperty(szPropNameA);
}

int CMyObject::OnProxyDeleteIndexedProperty(_In_ int nIndex)
{
  wprintf_s(L"[CMyObject] Deleting property #%ld\n", nIndex);
  return MX::CJsObjectBase::OnProxyDeleteIndexedProperty(nIndex);
}
