/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2006-2015. All rights reserved.
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
#include "TestJavascript.h"
#include <JsLib\JavascriptVM.h>
#include <Strings\Strings.h>
#include <Strings\Utf8.h>

//-----------------------------------------------------------

static HRESULT OnRequireModule(__in DukTape::duk_context *lpCtx,
                               __in MX::CJavascriptVM::CRequireModuleContext *lpReqCtx,
                               __inout MX::CStringA &cStrCodeA);

static DukTape::duk_ret_t OnGetSessionValue2(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                             __in_z LPCSTR szPropertyNameA);
static DukTape::duk_ret_t OnSetSessionValue2(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                             __in_z LPCSTR szPropertyNameA, __in DukTape::duk_idx_t nValueIndex);

static int OnProxyHasNamedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                   __in_z LPCSTR szPropertyNameA);
static int OnProxyHasIndexedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA, __in int nIndex);
static int OnProxyGetNamedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                   __in_z LPCSTR szPropertyNameA);
static int OnProxyGetIndexedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA, __in int nIndex);
static int OnProxySetNamedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                   __in_z LPCSTR szPropertyNameA, __in DukTape::duk_idx_t nValueIndex);
static int OnProxySetIndexedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA, __in int nIndex,
                                     __in DukTape::duk_idx_t nValueIndex);
static int OnProxyDeleteNamedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                      __in_z LPCSTR szPropertyNameA);
static int OnProxyDeleteIndexedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA, __in int nIndex);

//-----------------------------------------------------------

class CMyObject : public MX::CJsObjectBase
{
public:
  CMyObject(__in DukTape::duk_context *lpCtx);
  ~CMyObject();

  MX_JS_BEGIN_MAP(CMyObject, "myobject", 0)
    MX_JS_MAP_METHOD("add", &CMyObject::add, 2)
    MX_JS_MAP_METHOD("Print", &CMyObject::Print, 1)
  MX_JS_END_MAP()

public:
  DukTape::duk_ret_t add();
  DukTape::duk_ret_t Print();

  int OnProxyHasNamedProperty(__in_z LPCSTR szPropNameA);
  int OnProxyHasIndexedProperty(__in int nIndex);

  int OnProxyGetNamedProperty(__in_z LPCSTR szPropNameA);
  int OnProxyGetIndexedProperty(__in int nIndex);

  int OnProxySetNamedProperty(__in_z LPCSTR szPropNameA, __in DukTape::duk_idx_t nValueIndex);
  int OnProxySetIndexedProperty(__in int nIndex, __in DukTape::duk_idx_t nValueIndex);

  int OnProxyDeleteNamedProperty(__in_z LPCSTR szPropNameA);
  int OnProxyDeleteIndexedProperty(__in int nIndex);
};

//-----------------------------------------------------------

int TestJavascript()
{
  MX::CJavascriptVM cJvm;
  HRESULT hRes;

  wprintf_s(L"Initializing Javascript VM... ");
  cJvm.On(MX_BIND_CALLBACK(&OnRequireModule));
  hRes = cJvm.Initialize();
  if (FAILED(hRes))
  {
    wprintf_s(L"Error: %08X\n", hRes);
    return (int)hRes;
  }
  wprintf_s(L"OK\n");

  wprintf_s(L"Registering 'myobject'... ");
  hRes = CMyObject::Register(cJvm, TRUE, TRUE);
  if (FAILED(hRes))
  {
    wprintf_s(L"Error: %08X\n", hRes);
    return (int)hRes;
  }
  wprintf_s(L"OK\n");

  wprintf_s(L"Testing call to zarasa...\n");
  hRes = cJvm.Run("zarasa('hola');");
  if (FAILED(hRes))
  {
    if (hRes == E_FAIL)
    {
      wprintf_s(L"Error: %S in %S(%lu)\n", cJvm.GetLastRunErrorMessage(), cJvm.GetLastRunErrorFileName(),
                cJvm.GetLastRunErrorLineNumber());
      wprintf_s(L"%S\n", cJvm.GetLastRunErrorStackTrace());
    }
    else
    {
      wprintf_s(L"Error: %08X\n", hRes);
    }
  }

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
  hRes = cJvm.Run("var ext = require('../child/add.js');\n"
                  "obj = new myobject();\n"
                  "obj.blabla = 2;\n"
                  "obj[1] = obj.blabla;\n"
                  "delete obj.blabla;\n"
                  "obj[1] = 'Ã±andÃº';\n" //ñandú
                  "obj.Print('          v1: ' + globalValue.toString());\n"
                  "obj.Print('          v2: ' + session.value.toString());\n"
                  "obj.Print('          Suma: ' + ext.add(globalValue, session['value']).toString());\n"
                  "obj.Print('Multiplicacion: ' + ext.mult(globalValue, session['value']).toString());\n"
                  "session.value2 = 200.0;\n"
                  "obj.Print('Session value2: ' + session.value2.toString());\n",
                  L"código/html/main/index.js");
  if (FAILED(hRes))
  {
    if (hRes == E_FAIL)
    {
      wprintf_s(L"Error: %S in %S(%lu)\n", cJvm.GetLastRunErrorMessage(), cJvm.GetLastRunErrorFileName(),
                cJvm.GetLastRunErrorLineNumber());
      wprintf_s(L"%S\n", cJvm.GetLastRunErrorStackTrace());
    }
    else
    {
      wprintf_s(L"Error: %08X\n", hRes);
    }
  }
  //done
  return (int)S_OK;
}

static HRESULT OnRequireModule(__in DukTape::duk_context *lpCtx,
                               __in MX::CJavascriptVM::CRequireModuleContext *lpReqCtx,
                               __inout MX::CStringA &cStrCodeA)
{
  if (wcscmp(lpReqCtx->GetId(), L"código/html/child/add.js") == 0)
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
  else if (wcscmp(lpReqCtx->GetId(), L"código/html/child/mult.js") == 0)
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

static DukTape::duk_ret_t OnGetSessionValue2(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                             __in_z LPCSTR szPropertyNameA)
{
  DukTape::duk_push_number(lpCtx, nSessionValue2);
  return 1;
}

static DukTape::duk_ret_t OnSetSessionValue2(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                             __in_z LPCSTR szPropertyNameA, __in DukTape::duk_idx_t nValueIndex)
{
  nSessionValue2 = duk_require_number(lpCtx, nValueIndex);
  return DUK_ERR_NONE;
}

//-----------------------------------------------------------

static int OnProxyHasNamedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                   __in_z LPCSTR szPropertyNameA)
{
  if (szObjectNameA == NULL)
    szObjectNameA = "global";
  wprintf_s(L"[Object: %S] Checking for property '%S'\n", szObjectNameA, szPropertyNameA);
  return -1; //pass original
}

static int OnProxyHasIndexedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA, __in int nIndex)
{
  if (szObjectNameA == NULL)
    szObjectNameA = "global";
  wprintf_s(L"[Object: %S] Checking for indexed property #%ld\n", szObjectNameA, nIndex);
  return -1; //pass original
}

static int OnProxyGetNamedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                   __in_z LPCSTR szPropertyNameA)
{
  if (szObjectNameA == NULL)
    szObjectNameA = "global";
  wprintf_s(L"[Object: %S] Retrieving value of property '%S'\n", szObjectNameA, szPropertyNameA);
  return 0; //pass original
}

static int OnProxyGetIndexedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA, __in int nIndex)
{
  if (szObjectNameA == NULL)
    szObjectNameA = "global";
  wprintf_s(L"[Object: %S] Retrieving value of indexed property #%ld\n", szObjectNameA, nIndex);
  return 0; //pass original
}

static int OnProxySetNamedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                   __in_z LPCSTR szPropertyNameA, __in DukTape::duk_idx_t nValueIndex)
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

static int OnProxySetIndexedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA, __in int nIndex,
                                     __in DukTape::duk_idx_t nValueIndex)
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

static int OnProxyDeleteNamedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                                      __in_z LPCSTR szPropertyNameA)
{
  if (szObjectNameA == NULL)
    szObjectNameA = "global";
  wprintf_s(L"[Object: %S] Deleting property '%S'\n", szObjectNameA, szPropertyNameA);
  return 1; //delete original
}

static int OnProxyDeleteIndexedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA, __in int nIndex)
{
  if (szObjectNameA == NULL)
    szObjectNameA = "global";
  wprintf_s(L"[Object: %S] Deleting property #%ld\n", szObjectNameA, nIndex);
  return 1; //delete original
}

//-----------------------------------------------------------

CMyObject::CMyObject(__in DukTape::duk_context *lpCtx) : MX::CJsObjectBase(lpCtx)
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

int CMyObject::OnProxyHasNamedProperty(__in_z LPCSTR szPropNameA)
{
  wprintf_s(L"[CMyObject] Checking for property '%S'\n", szPropNameA);
  return MX::CJsObjectBase::OnProxyHasNamedProperty(szPropNameA);
}

int CMyObject::OnProxyHasIndexedProperty(__in int nIndex)
{
  wprintf_s(L"[CMyObject] Checking for indexed property #%ld\n", nIndex);
  return MX::CJsObjectBase::OnProxyHasIndexedProperty(nIndex);
}

int CMyObject::OnProxyGetNamedProperty(__in_z LPCSTR szPropNameA)
{
  wprintf_s(L"[CMyObject] Retrieving value of property '%S'\n", szPropNameA);
  return MX::CJsObjectBase::OnProxyGetNamedProperty(szPropNameA);
}

int CMyObject::OnProxyGetIndexedProperty(__in int nIndex)
{
  wprintf_s(L"[CMyObject] Retrieving value of indexed property #%ld\n", nIndex);
  return MX::CJsObjectBase::OnProxyGetIndexedProperty(nIndex);
}

int CMyObject::OnProxySetNamedProperty(__in_z LPCSTR szPropNameA, __in DukTape::duk_idx_t nValueIndex)
{
  DukTape::duk_dup(GetContext(), nValueIndex);
  DukTape::duk_to_string(GetContext(), -1);
  wprintf_s(L"[CMyObject] Setting value of property '%S' to \"%S\"\n", szPropNameA,
            DukTape::duk_get_string(GetContext(), -1));
  DukTape::duk_pop(GetContext());
  return MX::CJsObjectBase::OnProxySetNamedProperty(szPropNameA, nValueIndex);
}

int CMyObject::OnProxySetIndexedProperty(__in int nIndex, __in DukTape::duk_idx_t nValueIndex)
{
  DukTape::duk_dup(GetContext(), nValueIndex);
  DukTape::duk_to_string(GetContext(), -1);
  wprintf_s(L"[CMyObject] Setting value of indexed property #%ld to \"%S\"\n", nIndex,
            DukTape::duk_get_string(GetContext(), -1));
  DukTape::duk_pop(GetContext());
  return MX::CJsObjectBase::OnProxySetIndexedProperty(nIndex, nValueIndex);
}

int CMyObject::OnProxyDeleteNamedProperty(__in_z LPCSTR szPropNameA)
{
  wprintf_s(L"[CMyObject] Deleting property '%S'\n", szPropNameA);
  return MX::CJsObjectBase::OnProxyGetNamedProperty(szPropNameA);
}

int CMyObject::OnProxyDeleteIndexedProperty(__in int nIndex)
{
  wprintf_s(L"[CMyObject] Deleting property #%ld\n", nIndex);
  return MX::CJsObjectBase::OnProxyDeleteIndexedProperty(nIndex);
}
