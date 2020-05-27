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

CJsHttpServer::CJsHttpServer(_In_ CSockets &cSocketMgr,
                             _In_opt_ CLoggable *lpLogParent) : CBaseMemObj(), CHttpServer(cSocketMgr, lpLogParent)
{
  cNewRequestObjectCallback = NullCallback();
  cRequestHeadersReceivedCallback = NullCallback();
  cRequestCompletedCallback = NullCallback();
  cRequireJsModuleCallback = NullCallback();
  cWebSocketRequestReceivedCallback = NullCallback();
  cRequestDestroyedCallback = NullCallback();
  cCustomErrorPageCallback = NullCallback();
  //----
  cJvmManager.Attach(MX_DEBUG_NEW CJvmManager());
  //----
  CHttpServer::SetNewRequestObjectCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnNewRequestObject, this));
  CHttpServer::SetRequestHeadersReceivedCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnRequestHeadersReceived,
                                                                         this));
  CHttpServer::SetRequestCompletedCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnRequestCompleted, this));
  CHttpServer::SetWebSocketRequestReceivedCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnWebSocketRequestReceived,
                                                                           this));
  CHttpServer::SetRequestDestroyedCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnRequestDestroyed, this));
  CHttpServer::SetCustomErrorPageCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnCustomErrorPage, this));
  return;
}

CJsHttpServer::~CJsHttpServer()
{
  StopListening();
  return;
}

VOID CJsHttpServer::SetNewRequestObjectCallback(_In_ OnNewRequestObjectCallback _cNewRequestObjectCallback)
{
  cNewRequestObjectCallback = _cNewRequestObjectCallback;
  return;
}

VOID CJsHttpServer::SetRequestHeadersReceivedCallback(_In_ OnRequestHeadersReceivedCallback
                                                      _cRequestHeadersReceivedCallback)
{
  cRequestHeadersReceivedCallback = _cRequestHeadersReceivedCallback;
  return;
}

VOID CJsHttpServer::SetRequestCompletedCallback(_In_ OnRequestCompletedCallback _cRequestCompletedCallback)
{
  cRequestCompletedCallback = _cRequestCompletedCallback;
  return;
}

VOID CJsHttpServer::SetRequireJsModuleCallback(_In_ OnRequireJsModuleCallback _cRequireJsModuleCallback)
{
  cRequireJsModuleCallback = _cRequireJsModuleCallback;
  return;
}

VOID CJsHttpServer::SetWebSocketRequestReceivedCallback(_In_ OnWebSocketRequestReceivedCallback
                                                        _cWebSocketRequestReceivedCallback)
{
  cWebSocketRequestReceivedCallback = _cWebSocketRequestReceivedCallback;
  return;
}

VOID CJsHttpServer::SetRequestDestroyedCallback(_In_ OnRequestDestroyedCallback _cRequestDestroyedCallback)
{
  cRequestDestroyedCallback = _cRequestDestroyedCallback;
  return;
}

VOID CJsHttpServer::SetCustomErrorPageCallback(_In_ OnCustomErrorPageCallback _cCustomErrorPageCallback)
{
  cCustomErrorPageCallback = _cCustomErrorPageCallback;
  return;
}

HRESULT CJsHttpServer::OnNewRequestObject(_In_ CHttpServer *lpHttp, _Out_ CHttpServer::CClientRequest **lplpRequest)
{
  CClientRequest *lpJsRequest;
  HRESULT hRes;

  if (lplpRequest == NULL)
    return E_POINTER;
  *lplpRequest = NULL;

  if (!cJvmManager)
    return E_OUTOFMEMORY;

  if (cNewRequestObjectCallback)
  {
    lpJsRequest = NULL;
    hRes = cNewRequestObjectCallback(this, &lpJsRequest);
  }
  else
  {
    lpJsRequest = MX_DEBUG_NEW CClientRequest();
    hRes = (lpJsRequest != NULL) ? S_OK : E_OUTOFMEMORY;
  }
  if (SUCCEEDED(hRes) && lpJsRequest != NULL)
  {
    lpJsRequest->lpJsHttpServer = this;
    lpJsRequest->cJvmManager = cJvmManager;
    lpJsRequest->cRequireJsModuleCallback = cRequireJsModuleCallback;
    *lplpRequest = lpJsRequest;
  }
  return hRes;
}

HRESULT CJsHttpServer::OnRequestHeadersReceived(_In_ CHttpServer *lpHttp, _In_ CHttpServer::CClientRequest *_lpRequest,
                                                _Outptr_ _Maybenull_ CHttpBodyParserBase **lplpBodyParser)
{
  if (cRequestHeadersReceivedCallback)
  {
    CClientRequest *lpRequest = static_cast<CClientRequest*>(_lpRequest);

    return cRequestHeadersReceivedCallback(this, lpRequest, lplpBodyParser);
  }
  return S_OK;
}

VOID CJsHttpServer::OnRequestCompleted(_In_ MX::CHttpServer *lpHttp, _In_ CHttpServer::CClientRequest *_lpRequest)
{
  CClientRequest *lpRequest = static_cast<CClientRequest*>(_lpRequest);
  HRESULT hRes = S_OK;

  if (cRequestCompletedCallback)
  {
    CUrl *lpUrl;

    //build path and only accept absolute ones
    lpUrl = lpRequest->GetUrl();
    if ((lpUrl->GetPath())[0] == L'/')
    {
      cRequestCompletedCallback(this, lpRequest);
    }
    else
    {
      hRes = MX_E_Unsupported;
    }
  }

  if (FAILED(hRes))
  {
    lpRequest->SendErrorPage(500, hRes);
  }
  return;
}

HRESULT CJsHttpServer::OnWebSocketRequestReceived(_In_ CHttpServer *lpHttp, _In_ CHttpServer::CClientRequest *_lpReq,
                                                  _In_ int nVersion, _In_opt_ LPCSTR *szProtocolsA,
                                                  _In_ SIZE_T nProtocolsCount, _Out_ int &nSelectedProtocol,
                                                  _In_ TArrayList<int> &aSupportedVersions,
                                                  _Out_ _Maybenull_ CWebSocket **lplpWebSocket)
{
  if (cWebSocketRequestReceivedCallback)
  {
    CClientRequest *lpRequest = static_cast<CClientRequest*>(_lpReq);

    return cWebSocketRequestReceivedCallback(this, lpRequest, nVersion, szProtocolsA, nProtocolsCount,
                                             nSelectedProtocol, aSupportedVersions, lplpWebSocket);
  }
  return MX_E_Unsupported;
}

HRESULT CJsHttpServer::OnCustomErrorPage(_In_ CHttpServer *lpHttp, _Inout_ CSecureStringA &cStrBodyA,
                                         _In_ LONG nStatusCode, _In_ LPCSTR szStatusMessageA,
                                         _In_z_ LPCSTR szAdditionalExplanationA)
{
  if (cCustomErrorPageCallback)
  {
    return cCustomErrorPageCallback(this, cStrBodyA, nStatusCode, szStatusMessageA, szAdditionalExplanationA);
  }
  return S_OK;
}

VOID CJsHttpServer::OnRequestDestroyed(_In_ CHttpServer *lpHttp, _In_ CHttpServer::CClientRequest *_lpRequest)
{
  if (cRequestDestroyedCallback)
  {
    CClientRequest *lpRequest = static_cast<CClientRequest*>(_lpRequest);

    cRequestDestroyedCallback(this, lpRequest);
  }
  return;
}

} //namespace MX
