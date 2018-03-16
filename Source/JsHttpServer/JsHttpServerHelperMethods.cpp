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
#include "JsHttpServerCommon.h"
#include "..\..\Include\Http\HtmlEntities.h"
#include "..\..\Include\Crypto\DigestAlgorithmMDx.h"
#include "..\..\Include\Crypto\DigestAlgorithmSHAx.h"

//-----------------------------------------------------------

static const LPCSTR szHexaNumA = "0123456789ABCDEF";
static const LPCWSTR szHexaNumW = L"0123456789ABCDEF";

//-----------------------------------------------------------

static DukTape::duk_ret_t OnHtmlEntities(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                         _In_z_ LPCSTR szFunctionNameA);
static DukTape::duk_ret_t OnXmlEntities(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                        _In_z_ LPCSTR szFunctionNameA);
static DukTape::duk_ret_t OnUrlEncode(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                      _In_z_ LPCSTR szFunctionNameA);
static DukTape::duk_ret_t OnUrlDecode(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                      _In_z_ LPCSTR szFunctionNameA);
static DukTape::duk_ret_t OnHash(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                 _In_z_ LPCSTR szFunctionNameA);
static DukTape::duk_ret_t OnDie(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                _In_z_ LPCSTR szFunctionNameA);

//-----------------------------------------------------------

namespace MX {

namespace Internals {

namespace JsHttpServer {

HRESULT AddHelpersMethods(_In_ CJavascriptVM &cJvm, _In_ MX::CHttpServer::CRequest *lpRequest)
{
  HRESULT hRes;

  hRes = cJvm.AddNativeFunction("htmlentities", MX_BIND_CALLBACK(&OnHtmlEntities), 1);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddNativeFunction("xmlentities", MX_BIND_CALLBACK(&OnXmlEntities), 1);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddNativeFunction("urlencode", MX_BIND_CALLBACK(&OnUrlEncode), 1);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddNativeFunction("urldecode", MX_BIND_CALLBACK(&OnUrlDecode), 1);
  __EXIT_ON_ERROR(hRes);

  try
  {
    cJvm.RunNativeProtected(0, 0, [](_In_ DukTape::duk_context *lpCtx) -> VOID
    {
      DukTape::duk_push_string(lpCtx, "function vardump(obj)\n"
                               "{ return Duktape.enc('jx', obj, null, 2); }\n");
      DukTape::duk_push_string(lpCtx, (const char *)(__FILE__));
      DukTape::duk_eval_raw(lpCtx, NULL, 0, DUK_COMPILE_EVAL);
      DukTape::duk_pop(lpCtx);

      DukTape::duk_eval_raw(lpCtx, "function SystemExit(msg) {\r\n"
                                     "Error.call(this, \"\");\r\n"
                                     "this.message = msg;\r\n"
                                     "this.name = \"SystemExit\";\r\n"
                                     "return this; }\r\n"
                                   "SystemExit.prototype = Object.create(Error.prototype);\r\n"
                                   "SystemExit.prototype.constructor=SystemExit;\r\n", 0,
                            DUK_COMPILE_EVAL | DUK_COMPILE_NOSOURCE | DUK_COMPILE_STRLEN | DUK_COMPILE_NOFILENAME);
      return;
    });
  }
  catch (CJsWindowsError &e)
  {
    return e.GetHResult();
  }
  catch (CJsError &)
  {
    return E_FAIL;
  }

  hRes = cJvm.CreateObject("HASH");
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddObjectNumericProperty("HASH", "MD5", 1.0, CJavascriptVM::PropertyFlagEnumerable);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddObjectNumericProperty("HASH", "SHA1", 2.0, CJavascriptVM::PropertyFlagEnumerable);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddObjectNumericProperty("HASH", "SHA256", 3.0, CJavascriptVM::PropertyFlagEnumerable);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddNativeFunction("hash", MX_BIND_CALLBACK(&OnHash), 2);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddNativeFunction("die", MX_BIND_CALLBACK(&OnDie), MX_JS_VARARGS);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddNativeFunction("exit", MX_BIND_CALLBACK(&OnDie), MX_JS_VARARGS);
  __EXIT_ON_ERROR(hRes);
  //done
  return S_OK;
}

} //namespace JsHttpServer

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

static DukTape::duk_ret_t OnHtmlEntities(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                         _In_z_ LPCSTR szFunctionNameA)
{
  MX::CStringA cStrTempA;
  LPCSTR szBufA;
  HRESULT hRes;

  szBufA = DukTape::duk_to_string(lpCtx, 0); //convert to string if other type
  hRes = (cStrTempA.Copy(szBufA) != FALSE) ? S_OK : E_OUTOFMEMORY;
  if (SUCCEEDED(hRes))
    hRes = MX::HtmlEntities::ConvertTo(cStrTempA);
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  //push result
  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTempA, cStrTempA.GetLength());
  return 1;
}

static DukTape::duk_ret_t OnXmlEntities(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                        _In_z_ LPCSTR szFunctionNameA)
{
  MX::CStringA cStrTempA;
  MX::CStringW cStrTempW;
  WCHAR szTempBufW[8];
  LPCSTR szBufA;
  LPCWSTR sW, szReplacementW;
  SIZE_T nOfs;
  HRESULT hRes;

  szBufA = DukTape::duk_to_string(lpCtx, 0); //convert to string if other type
  hRes = MX::Utf8_Decode(cStrTempW, szBufA);
  if (SUCCEEDED(hRes))
  {
    szTempBufW[0] = L'&';
    szTempBufW[1] = L'#';
    szTempBufW[2] = L'x';
    szTempBufW[5] = L';';
    szTempBufW[6] = 0;
    sW = (LPCWSTR)cStrTempW;
    while (*sW != 0)
    {
      szReplacementW = NULL;
      switch (*sW)
      {
        case L'&':
          szReplacementW = L"&amp;";
          break;
        case L'\"':
          szReplacementW = L"&quot;";
          break;
        case L'\'':
          szReplacementW = L"&apos;";
          break;
        case L'<':
          szReplacementW = L"&lt;";
          break;
        case L'>':
          szReplacementW = L"&gt;";
          break;
        default:
          if (*sW < 32)
          {
            szTempBufW[3] = szHexaNumW[((ULONG)(*sW) >> 4) & 0x0F];
            szTempBufW[4] = szHexaNumW[(ULONG)(*sW)       & 0x0F];
            szReplacementW = szTempBufW;
          }
          break;
      }
      if (szReplacementW != NULL)
      {
        nOfs = (SIZE_T)(sW - (LPCWSTR)cStrTempW);
        cStrTempW.Delete(nOfs, 1);
        if (cStrTempW.Insert(szReplacementW, nOfs) == FALSE)
        {
          hRes = E_OUTOFMEMORY;
          break;
        }
        sW = (LPCWSTR)cStrTempW + (nOfs + MX::StrLenW(szReplacementW));
      }
      else
      {
        sW++;
      }
    }
    if (SUCCEEDED(hRes))
      hRes = MX::Utf8_Encode(cStrTempA, (LPCWSTR)cStrTempW, cStrTempW.GetLength());
    if (FAILED(hRes))
      MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  }
  //push result
  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTempA, cStrTempA.GetLength());
  return 1;
}

static DukTape::duk_ret_t OnUrlEncode(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                      _In_z_ LPCSTR szFunctionNameA)
{
  MX::CStringA cStrTempA;
  LPCSTR szBufA;
  HRESULT hRes;

  szBufA = DukTape::duk_require_string(lpCtx, 0);
  hRes = MX::CUrl::Encode(cStrTempA, szBufA);
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  //push result
  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTempA, cStrTempA.GetLength());
  return 1;
}

static DukTape::duk_ret_t OnUrlDecode(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                      _In_z_ LPCSTR szFunctionNameA)
{
  MX::CStringA cStrTempA;
  LPCSTR szBufA;
  HRESULT hRes;

  szBufA = DukTape::duk_require_string(lpCtx, 0);
  hRes = MX::CUrl::Decode(cStrTempA, szBufA);
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  //push result
  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTempA, cStrTempA.GetLength());
  return 1;
}

static DukTape::duk_ret_t OnHash(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                 _In_z_ LPCSTR szFunctionNameA)

{
  MX::TAutoDeletePtr<MX::CBaseDigestAlgorithm> cDigest;
  MX::CStringA cStrTempA;
  SIZE_T i, nSize;
  LPBYTE lpResult;
  LPCSTR szBufA;
  int nType;
  HRESULT hRes;

  szBufA = DukTape::duk_require_string(lpCtx, 0);
  nType = (int)DukTape::duk_require_int(lpCtx, 1);
  switch (nType)
  {
    case 1: //MD5
      {
      MX::CDigestAlgorithmMessageDigest *lpPtr;

      lpPtr = MX_DEBUG_NEW MX::CDigestAlgorithmMessageDigest();
      if (lpPtr != NULL)
        hRes = lpPtr->BeginDigest(MX::CDigestAlgorithmMessageDigest::AlgorithmMD5);
      else
        hRes = E_OUTOFMEMORY;
      cDigest.Attach(lpPtr);
      }
      break;

    case 2: //SHA1
    case 3: //SHA256
      {
      MX::CDigestAlgorithmSecureHash *lpPtr;

      lpPtr = MX_DEBUG_NEW MX::CDigestAlgorithmSecureHash();
      if (lpPtr != NULL)
      {
        switch (nType)
        {
          case 2:
            hRes = lpPtr->BeginDigest(MX::CDigestAlgorithmSecureHash::AlgorithmSHA1);
            break;
          case 3:
            hRes = lpPtr->BeginDigest(MX::CDigestAlgorithmSecureHash::AlgorithmSHA256);
            break;
        }
      }
      else
      {
        hRes = E_OUTOFMEMORY;
      }
      cDigest.Attach(lpPtr);
      }
      break;

    default:
      hRes = E_INVALIDARG;
      break;
  }
  if (SUCCEEDED(hRes))
  {
    hRes = cDigest->DigestStream(szBufA, MX::StrLenA(szBufA));
    if (SUCCEEDED(hRes))
      hRes = cDigest->EndDigest();
    if (SUCCEEDED(hRes))
    {
      lpResult = cDigest->GetResult();
      nSize = cDigest->GetResultSize();
      for (i=0; i<nSize; i++)
      {
        if (cStrTempA.AppendFormat(L"%02X", lpResult[i]) == FALSE)
        {
          hRes = E_OUTOFMEMORY;
          break;
        }
      }
    }
  }
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  //push result
  DukTape::duk_push_lstring(lpCtx, (LPCSTR)cStrTempA, cStrTempA.GetLength());
  return 1;
}

static DukTape::duk_ret_t OnDie(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                _In_z_ LPCSTR szFunctionNameA)
{
  BOOL bHasMessage = (DukTape::duk_get_top(lpCtx) > 0) ? TRUE : FALSE;

  DukTape::duk_get_global_string(lpCtx, "SystemExit");
  if (bHasMessage != FALSE)
  {
    DukTape::duk_dup(lpCtx, 0); //copy message
    DukTape::duk_safe_to_string(lpCtx, -1);
  }
  else
  {
    DukTape::duk_push_string(lpCtx, "");
  }
  DukTape::duk_new(lpCtx, 1);

  DukTape::duk_push_boolean(lpCtx, 1);
  DukTape::duk_put_prop_string(lpCtx, -2, "\xff""\xff""nonCatcheable");

  DukTape::duk_throw_raw(lpCtx);
  return 0;
}
