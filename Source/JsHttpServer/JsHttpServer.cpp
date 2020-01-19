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
  cErrorCallback = NullCallback();
  //----
  cJvmManager.Attach(MX_DEBUG_NEW CJvmManager());
  //----
  CHttpServer::SetNewRequestObjectCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnNewRequestObject, this));
  CHttpServer::SetRequestHeadersReceivedCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnRequestHeadersReceived,
                                                                         this));
  CHttpServer::SetRequestCompletedCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnRequestCompleted, this));
  CHttpServer::SetWebSocketRequestReceivedCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnWebSocketRequestReceived,
                                                                           this));
  CHttpServer::SetErrorCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnError, this));
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

VOID CJsHttpServer::SetErrorCallback(_In_ OnErrorCallback _cErrorCallback)
{
  cErrorCallback = _cErrorCallback;
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
    if (lpUrl != NULL)
    {
      if ((lpUrl->GetPath())[0] == L'/')
      {
        cRequestCompletedCallback(this, lpRequest);
      }
      else
      {
        hRes = MX_E_Unsupported;
      }
    }
    else
    {
      hRes = E_NOTIMPL;
    }
  }

  if (FAILED(hRes))
  {
    lpRequest->ResetAndDisableClientCache();
    hRes = lpRequest->SendErrorPage(500, hRes);
    lpRequest->End(hRes);
  }
  return;
}

HRESULT CJsHttpServer::OnWebSocketRequestReceived(_In_ CHttpServer *lpHttp,
                                                  _In_ CHttpServer::CClientRequest *_lpRequest,
                                                  _Inout_ CHttpServer::WEBSOCKET_REQUEST_CALLBACK_DATA &sData)
{
  if (cWebSocketRequestReceivedCallback)
  {
    CClientRequest *lpRequest = static_cast<CClientRequest*>(_lpRequest);

    return cWebSocketRequestReceivedCallback(this, lpRequest, sData);
  }
  return MX_E_Unsupported;
}

VOID CJsHttpServer::OnError(_In_ CHttpServer *lpHttp, _In_ CHttpServer::CClientRequest *_lpRequest,
                            _In_ HRESULT hrErrorCode)
{
  if (cErrorCallback)
  {
    CClientRequest *lpRequest = static_cast<CClientRequest*>(_lpRequest);

    cErrorCallback(this, lpRequest, hrErrorCode);
  }
  return;
}

} //namespace MX
