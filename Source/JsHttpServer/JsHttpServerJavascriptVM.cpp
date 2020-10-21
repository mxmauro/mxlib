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
#include "JsHttpServerCommon.h"

//-----------------------------------------------------------

namespace MX {

CJsHttpServer::CJvmManager::CJvmManager() : TRefCounted<CBaseMemObj>()
{
  FastLock_Initialize(&nMutex);
  return;
}

CJsHttpServer::CJvmManager::~CJvmManager()
{
  CFastLock cLock(&nMutex);
  CLnkLstNode *lpNode;

  while ((lpNode = cJvmList.PopHead()) != NULL)
  {
    CJsHttpServer::CJvm *lpJVM = CONTAINING_RECORD(lpNode, CJsHttpServer::CJvm, cListNode);

    delete lpJVM;
  }
  return;
}

HRESULT CJsHttpServer::CJvmManager::AllocAndInitVM(_Out_ CJvm **lplpJVM, _Out_ BOOL &bIsNew,
                                                   _In_ OnRequireJsModuleCallback cRequireJsModuleCallback,
                                                   _In_ CClientRequest *lpRequest)
{
  TAutoDeletePtr<CJvm> cJVM;
  CStringA cStrTempA;
  CStringW cStrTempW;
  SOCKADDR_INET sSockAddr;
  CUrl *lpUrl;
  TAutoRefCounted<CHttpBodyParserBase> cBodyParser;
  CSockets *lpSckMgr;
  HANDLE hConn;
  SIZE_T i, nCount;
  HRESULT hRes;

  {
    CFastLock cLock(&nMutex);
    CLnkLstNode *lpNode;

    lpNode = cJvmList.PopHead();
    if (lpNode != NULL)
      cJVM.Attach(CONTAINING_RECORD(lpNode, CJsHttpServer::CJvm, cListNode));
  }

  bIsNew = FALSE;
  if (cJVM && FAILED(cJVM->Reset()))
  {
    cJVM.Reset();
  }

  if (!cJVM)
  {
    cJVM.Attach(MX_DEBUG_NEW CJvm());
    if (!cJVM)
      return E_OUTOFMEMORY;
    bIsNew = TRUE;
  }

  //set require module callback
  lpRequest->cRequireJsModuleCallback = cRequireJsModuleCallback;
  cJVM->SetRequireModuleCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::CClientRequest::OnRequireJsModule,
                                                         lpRequest));

  //init JVM
  if (bIsNew != FALSE)
  {
    hRes = cJVM->Initialize();
    __EXIT_ON_ERROR(hRes);

    hRes = cJVM->RegisterException("SystemExit", [](_In_ DukTape::duk_context *lpCtx,
                                                    _In_ DukTape::duk_idx_t nExceptionObjectIndex) -> VOID
    {
      throw CJsHttpServerSystemExit(lpCtx, nExceptionObjectIndex);
      return;
    });
    __EXIT_ON_ERROR(hRes);

    //register C++ objects
    hRes = Internals::CFileFieldJsObject::Register(*cJVM.Get());
    __EXIT_ON_ERROR(hRes);
    hRes = Internals::CRawBodyJsObject::Register(*cJVM.Get());
    __EXIT_ON_ERROR(hRes);

    //response functions
    hRes = Internals::JsHttpServer::AddResponseMethods(*cJVM.Get());
    __EXIT_ON_ERROR(hRes);

    //helper functions
    hRes = Internals::JsHttpServer::AddHelpersMethods(*cJVM.Get());
    __EXIT_ON_ERROR(hRes);
  }
  else
  {
    //delete a previously set request object
    hRes = cJVM->RunNativeProtectedAndGetError(0, 0, [](_In_ DukTape::duk_context *lpCtx) -> VOID
    {
      DukTape::duk_push_global_object(lpCtx);
      DukTape::duk_del_prop_string(lpCtx, -1, "request"); //does not raise error if does not exist
      DukTape::duk_pop(lpCtx);
    });
    __EXIT_ON_ERROR(hRes);
  }

  //store request pointer
  hRes = cJVM->RunNativeProtectedAndGetError(0, 0, [lpRequest](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_global_object(lpCtx);
    DukTape::duk_push_pointer(lpCtx, lpRequest);
    DukTape::duk_put_prop_string(lpCtx, -2, INTERNAL_REQUEST_PROPERTY);
    DukTape::duk_pop(lpCtx);
    return;
  });
  __EXIT_ON_ERROR(hRes);

  //create request object
  hRes = cJVM->CreateObject("request");
  __EXIT_ON_ERROR(hRes);
  hRes = cJVM->AddObjectStringProperty("request", "method", lpRequest->GetMethod(),
                                       CJavascriptVM::PropertyFlagEnumerable | CJavascriptVM::PropertyFlagConfigurable);
  __EXIT_ON_ERROR(hRes);
  lpUrl = lpRequest->GetUrl();

  //host
  hRes = Utf8_Encode(cStrTempA, lpUrl->GetHost());
  __EXIT_ON_ERROR(hRes);
  hRes = cJVM->AddObjectStringProperty("request", "host", (LPCSTR)cStrTempA,
                                       CJavascriptVM::PropertyFlagEnumerable | CJavascriptVM::PropertyFlagConfigurable);
  __EXIT_ON_ERROR(hRes);
  //port
  hRes = cJVM->AddObjectNumericProperty("request", "port", (double)(lpUrl->GetPort()),
                                        CJavascriptVM::PropertyFlagEnumerable |
                                        CJavascriptVM::PropertyFlagConfigurable);
  __EXIT_ON_ERROR(hRes);
  //path
  hRes = Utf8_Encode(cStrTempA, lpUrl->GetPath());
  __EXIT_ON_ERROR(hRes);
  hRes = cJVM->AddObjectStringProperty("request", "path", (LPCSTR)cStrTempA,
                                       CJavascriptVM::PropertyFlagEnumerable | CJavascriptVM::PropertyFlagConfigurable);
  __EXIT_ON_ERROR(hRes);

  //query strings
  hRes = cJVM->CreateObject("request.query");
  __EXIT_ON_ERROR(hRes);
  nCount = lpUrl->GetQueryStringCount();
  for (i = 0; i < nCount; i++)
  {
    if (StrCompareW(lpUrl->GetQueryStringName(i), L"session_id") != 0) //skip session id query
    {
      CStringA cStrValueA;

      hRes = Utf8_Encode(cStrTempA, lpUrl->GetQueryStringName(i));
      __EXIT_ON_ERROR(hRes);
      hRes = Utf8_Encode(cStrValueA, lpUrl->GetQueryStringValue(i));
      __EXIT_ON_ERROR(hRes);
      hRes = cJVM->AddObjectStringProperty("request.query", (LPCSTR)cStrTempA, (LPCSTR)cStrValueA,
                                           CJavascriptVM::PropertyFlagEnumerable |
                                           CJavascriptVM::PropertyFlagConfigurable);
      __EXIT_ON_ERROR(hRes);
    }
  }

  //remote ip and port
  lpSckMgr = lpRequest->GetUnderlyingSocketManager();
  hConn = lpRequest->GetUnderlyingSocketHandle();
  if (hConn == NULL || lpSckMgr == NULL)
    return E_UNEXPECTED;
  hRes = lpSckMgr->GetPeerAddress(hConn, &sSockAddr);
  __EXIT_ON_ERROR(hRes);
  hRes = cJVM->CreateObject("request.remote");
  __EXIT_ON_ERROR(hRes);

  switch (sSockAddr.si_family)
  {
    case AF_INET:
      hRes = HostResolver::FormatAddress(&sSockAddr, cStrTempW);
      __EXIT_ON_ERROR(hRes);
      hRes = cJVM->AddObjectStringProperty("request.remote", "family", "ipv4",
                                           CJavascriptVM::PropertyFlagEnumerable |
                                           CJavascriptVM::PropertyFlagConfigurable);
      __EXIT_ON_ERROR(hRes);
      hRes = cJVM->AddObjectStringProperty("request.remote", "address", (LPCWSTR)cStrTempW,
                                           CJavascriptVM::PropertyFlagEnumerable |
                                           CJavascriptVM::PropertyFlagConfigurable);
      __EXIT_ON_ERROR(hRes);
      hRes = cJVM->AddObjectNumericProperty("request.remote", "port", (double)htons(sSockAddr.Ipv4.sin_port),
                                            CJavascriptVM::PropertyFlagEnumerable |
                                            CJavascriptVM::PropertyFlagConfigurable);
      __EXIT_ON_ERROR(hRes);
      break;

    case AF_INET6:
      hRes = HostResolver::FormatAddress(&sSockAddr, cStrTempW);
      __EXIT_ON_ERROR(hRes);
      hRes = cJVM->AddObjectStringProperty("request.family", "family", "ipv6",
                                           CJavascriptVM::PropertyFlagEnumerable |
                                           CJavascriptVM::PropertyFlagConfigurable);
      __EXIT_ON_ERROR(hRes);
      hRes = cJVM->AddObjectStringProperty("request.remote", "address", (LPCWSTR)cStrTempW,
                                           CJavascriptVM::PropertyFlagEnumerable |
                                           CJavascriptVM::PropertyFlagConfigurable);
      __EXIT_ON_ERROR(hRes);
      hRes = cJVM->AddObjectNumericProperty("request.remote", "port", (double)htons(sSockAddr.Ipv6.sin6_port),
                                            CJavascriptVM::PropertyFlagEnumerable |
                                            CJavascriptVM::PropertyFlagConfigurable);
      __EXIT_ON_ERROR(hRes);
      break;

    default:
      __EXIT_ON_ERROR(MX_E_Unsupported);
      break;
  }

  //add request headers
  hRes = cJVM->CreateObject("request.headers");
  __EXIT_ON_ERROR(hRes);

  for (i = 0; ; i++)
  {
    TAutoRefCounted<CHttpHeaderBase> cHeader;

    cHeader.Attach(lpRequest->GetRequestHeader(i));
    if (!cHeader)
      break;

    hRes = cHeader->Build(cStrTempA, lpRequest->GetBrowser());
    __EXIT_ON_ERROR(hRes);
    hRes = cJVM->AddObjectStringProperty("request.headers", cHeader->GetHeaderName(), (LPCSTR)cStrTempA,
                                         CJavascriptVM::PropertyFlagEnumerable |
                                         CJavascriptVM::PropertyFlagConfigurable);
    __EXIT_ON_ERROR(hRes);
  }

  //add cookies
  hRes = cJVM->CreateObject("request.cookies");
  __EXIT_ON_ERROR(hRes);
  for (i = 0; ; i++)
  {
    TAutoRefCounted<CHttpCookie> cCookie;

    cCookie.Attach(lpRequest->GetRequestCookie(i));
    if (!cCookie)
      break;

    if (cStrTempA.Copy(cCookie->GetName()) == FALSE)
      return E_OUTOFMEMORY;
    for (LPSTR sA = (LPSTR)cStrTempA; *sA != 0; sA++)
    {
      if (*sA == '.')
        *sA = '_';
    }
    hRes = cJVM->HasObjectProperty("request.cookies", (LPCSTR)cStrTempA);
    if (hRes == S_FALSE)
    {
      //don't add duplicates
      hRes = cJVM->AddObjectStringProperty("request.cookies", (LPCSTR)cStrTempA, cCookie->GetValue(),
                                           CJavascriptVM::PropertyFlagEnumerable |
                                           CJavascriptVM::PropertyFlagConfigurable);
    }
    __EXIT_ON_ERROR(hRes);
  }

  //add body
  hRes = cJVM->CreateObject("request.post");
  __EXIT_ON_ERROR(hRes);
  hRes = cJVM->CreateObject("request.files");
  __EXIT_ON_ERROR(hRes);
  cBodyParser.Attach(lpRequest->GetRequestBodyParser());
  if (cBodyParser)
  {
    if (StrCompareA(cBodyParser->GetType(), "default") == 0)
    {
      CHttpBodyParserDefault *lpParser = (CHttpBodyParserDefault*)(cBodyParser.Get());
      Internals::CRawBodyJsObject *lpJsObj;

      hRes = lpParser->ToString(cStrTempA);
      __EXIT_ON_ERROR(hRes);
      lpJsObj = MX_DEBUG_NEW Internals::CRawBodyJsObject();
      if (!lpJsObj)
        return E_OUTOFMEMORY;
      lpJsObj->Initialize(lpParser);
      hRes = cJVM->AddObjectJsObjectProperty("request", "rawbody", lpJsObj,
                                             CJavascriptVM::PropertyFlagEnumerable |
                                             CJavascriptVM::PropertyFlagConfigurable);
      lpJsObj->Release();
      __EXIT_ON_ERROR(hRes);
    }
    else if (StrCompareA(cBodyParser->GetType(), "multipart/form-data") == 0 ||
             StrCompareA(cBodyParser->GetType(), "application/x-www-form-urlencoded") == 0)
    {
      CHttpBodyParserFormBase *lpParser = (CHttpBodyParserFormBase*)(cBodyParser.Get());

      //add post fields
      nCount = lpParser->GetFieldsCount();
      for (i = 0; i < nCount; i++)
      {
        hRes = InsertPostField(*cJVM, lpParser->GetField(i), "request.post");
        __EXIT_ON_ERROR(hRes);
      }
      //add files fields
      nCount = lpParser->GetFileFieldsCount();
      for (i = 0; i < nCount; i++)
      {
        hRes = InsertPostFileField(*cJVM, lpParser->GetFileField(i), "request.files");
        __EXIT_ON_ERROR(hRes);
      }
    }
    else if (StrCompareA(cBodyParser->GetType(), "application/json") == 0)
    {
      CHttpBodyParserJSON *lpParser = (CHttpBodyParserJSON*)(cBodyParser.Get());

      rapidjson::Document &d = lpParser->GetDocument();

      hRes = cJVM->CreateObject("request.json");
      __EXIT_ON_ERROR(hRes);

      hRes = cJVM->RunNativeProtectedAndGetError(0, 0, [this, &d](_In_ DukTape::duk_context *lpCtx) -> VOID
      {
        const rapidjson::Value *v;

        DukTape::duk_push_global_object(lpCtx);
        DukTape::duk_get_prop_string(lpCtx, -1, "request");
        v = &d;
        ParseJsonBody(lpCtx, (LPVOID)v, "json", 0);
        DukTape::duk_pop_2(lpCtx);
        return;
      });
      __EXIT_ON_ERROR(hRes);
    }
  }

  //done
  *lplpJVM = cJVM.Detach();
  return S_OK;
}

VOID CJsHttpServer::CJvmManager::FreeVM(_In_ CJvm *lpJVM)
{
  CFastLock cLock(&nMutex);

  cJvmList.PushTail(&(lpJVM->cListNode));
  return;
}

HRESULT CJsHttpServer::CJvmManager::InsertPostField(_In_ CJavascriptVM &cJvm,
                                                    _In_ CHttpBodyParserFormBase::CField *lpField,
                                                    _In_ LPCSTR szBaseObjectNameA)
{
  CStringA cStrNameA;
  SIZE_T i, nCount;
  HRESULT hRes;

  hRes = Utf8_Encode(cStrNameA, lpField->GetName());
  __EXIT_ON_ERROR(hRes);
  nCount = lpField->GetSubIndexesCount();
  if (nCount > 0)
  {
    if (cStrNameA.InsertN(".", 0, 1) == FALSE || cStrNameA.Insert(szBaseObjectNameA, 0) == FALSE)
      return E_OUTOFMEMORY;
    hRes = cJvm.CreateObject((LPCSTR)cStrNameA);
    if (hRes == MX_E_AlreadyExists)
      hRes = S_OK;
    for (i = 0; SUCCEEDED(hRes) && i < nCount; i++)
      hRes = InsertPostField(cJvm, lpField->GetSubIndexAt(i), (LPCSTR)cStrNameA);
  }
  else
  {
    CStringA cStrValueA;

    hRes = Utf8_Encode(cStrValueA, lpField->GetValue());
    __EXIT_ON_ERROR(hRes);

    hRes = cJvm.AddObjectStringProperty(szBaseObjectNameA, (LPCSTR)cStrNameA, (LPCSTR)cStrValueA,
                                        CJavascriptVM::PropertyFlagEnumerable |
                                        CJavascriptVM::PropertyFlagConfigurable);
  }
  return hRes;
}

HRESULT CJsHttpServer::CJvmManager::InsertPostFileField(_In_ CJavascriptVM &cJvm,
                                                        _In_ CHttpBodyParserFormBase::CFileField *lpFileField,
                                                        _In_ LPCSTR szBaseObjectNameA)
{
  CStringA cStrNameA;
  SIZE_T i, nCount;
  HRESULT hRes;

  hRes = Utf8_Encode(cStrNameA, lpFileField->GetName());
  __EXIT_ON_ERROR(hRes);
  nCount = lpFileField->GetSubIndexesCount();
  if (nCount > 0)
  {
    if (cStrNameA.InsertN(".", 0, 1) == FALSE || cStrNameA.Insert(szBaseObjectNameA, 0) == FALSE)
      return E_OUTOFMEMORY;
    hRes = cJvm.CreateObject((LPCSTR)cStrNameA);
    if (hRes == MX_E_AlreadyExists)
      hRes = S_OK;
    for (i = 0; SUCCEEDED(hRes) && i < nCount; i++)
      hRes = InsertPostFileField(cJvm, lpFileField->GetSubIndexAt(i), (LPCSTR)cStrNameA);
  }
  else
  {
    Internals::CFileFieldJsObject *lpJsObj;

    lpJsObj = MX_DEBUG_NEW Internals::CFileFieldJsObject();
    if (!lpJsObj)
      return E_OUTOFMEMORY;
    lpJsObj->Initialize(lpFileField);
    hRes = cJvm.AddObjectJsObjectProperty(szBaseObjectNameA, (LPCSTR)cStrNameA, lpJsObj,
                                          CJavascriptVM::PropertyFlagEnumerable |
                                          CJavascriptVM::PropertyFlagConfigurable);
    lpJsObj->Release();
  }
  return hRes;
}

VOID CJsHttpServer::CJvmManager::ParseJsonBody(_In_ DukTape::duk_context *lpCtx, _In_ LPVOID _v,
                                               _In_opt_z_ LPCSTR szPropNameA,
                                               _In_ DukTape::duk_uarridx_t nArrayIndex) throw()
{
  const rapidjson::Value *v = (const rapidjson::Value*)_v;

  switch (v->GetType())
  {
    case rapidjson::kNullType:
      DukTape::duk_push_null(lpCtx);

      if (szPropNameA != NULL)
        DukTape::duk_put_prop_string(lpCtx, -2, szPropNameA);
      else
        DukTape::duk_put_prop_index(lpCtx, -2, nArrayIndex);
      break;

    case rapidjson::kFalseType:
    case rapidjson::kTrueType:
      DukTape::duk_push_boolean(lpCtx, ((v->GetType() == rapidjson::kFalseType) ? 0 : 1));

      if (szPropNameA != NULL)
        DukTape::duk_put_prop_string(lpCtx, -2, szPropNameA);
      else
        DukTape::duk_put_prop_index(lpCtx, -2, nArrayIndex);
      break;

    case rapidjson::kStringType:
      DukTape::duk_push_string(lpCtx, v->GetString());

      if (szPropNameA != NULL)
        DukTape::duk_put_prop_string(lpCtx, -2, szPropNameA);
      else
        DukTape::duk_put_prop_index(lpCtx, -2, nArrayIndex);
      break;

    case rapidjson::kNumberType:
      if (v->IsInt() != false)
      {
        DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)(v->GetInt()));
      }
      else if (v->IsLosslessDouble() != false)
      {
        DukTape::duk_push_number(lpCtx, (DukTape::duk_double_t)(v->GetDouble()));
      }
      else
      {
        HRESULT hRes;

        hRes = CJavascriptVM::AddBigIntegerSupport(lpCtx);
        if (FAILED(hRes))
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);

        //push big number constructor
        DukTape::duk_push_global_object(lpCtx);
        DukTape::duk_get_prop_string(lpCtx, -1, "BigInteger");
        DukTape::duk_remove(lpCtx, -2);

        //push argument
        if (v->IsUint64())
        {
          DukTape::duk_push_sprintf(lpCtx, "%I64u", v->GetUint64());
        }
        else if (v->IsInt64())
        {
          DukTape::duk_push_sprintf(lpCtx, "%I64d", v->GetInt64());
        }
        else
        {
          MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_Unsupported);
        }

        //create object
        DukTape::duk_new(lpCtx, 1);
      }

      if (szPropNameA != NULL)
        DukTape::duk_put_prop_string(lpCtx, -2, szPropNameA);
      else
        DukTape::duk_put_prop_index(lpCtx, -2, nArrayIndex);
      break;

    case rapidjson::kObjectType:
      {
      rapidjson::Value::ConstObject o = v->GetObjectW();

      DukTape::duk_push_object(lpCtx);

      for (rapidjson::Value::ConstMemberIterator it = o.MemberBegin(); it != o.MemberEnd(); ++it)
      {
        const rapidjson::Value *v = &(it->value);

        ParseJsonBody(lpCtx, (LPVOID)v, it->name.GetString(), 0);
      }

      if (szPropNameA != NULL)
        DukTape::duk_put_prop_string(lpCtx, -2, szPropNameA);
      else
        DukTape::duk_put_prop_index(lpCtx, -2, nArrayIndex);
      }
      break;

    case rapidjson::kArrayType:
      {
      rapidjson::Value::ConstArray a = v->GetArray();
      DukTape::duk_uarridx_t nIdx;

      DukTape::duk_push_array(lpCtx);

      nIdx = 0;
      for (rapidjson::Value::ConstValueIterator it = a.Begin(); it != a.End(); ++it)
      {
        const rapidjson::Value *__v = it;

        ParseJsonBody(lpCtx, (LPVOID)__v, NULL, nIdx++);
      }

      if (szPropNameA != NULL)
        DukTape::duk_put_prop_string(lpCtx, -2, szPropNameA);
      else
        DukTape::duk_put_prop_index(lpCtx, -2, nArrayIndex);
      }
      break;
  }
  return;
}

//-----------------------------------------------------------

CJsHttpServer::CClientRequest* CJsHttpServer::GetServerRequestFromContext(_In_ DukTape::duk_context *lpCtx)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  CClientRequest *lpRequest = NULL;
  HRESULT hRes;

  hRes = lpJVM->RunNativeProtectedAndGetError(0, 0, [&lpRequest](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_global_object(lpCtx);
    DukTape::duk_get_prop_string(lpCtx, -1, INTERNAL_REQUEST_PROPERTY);
    if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
      lpRequest = reinterpret_cast<CClientRequest*>(DukTape::duk_to_pointer(lpCtx, -1));
    DukTape::duk_pop_2(lpCtx);
    return;
  });
  return (SUCCEEDED(hRes)) ? lpRequest : NULL;
}

} //namespace MX
