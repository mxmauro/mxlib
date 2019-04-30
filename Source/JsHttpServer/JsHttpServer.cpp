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
#include "..\..\include\Http\HtmlEntities.h"

//-----------------------------------------------------------

namespace MX {

CJsHttpServer::CJsHttpServer(_In_ CSockets &cSocketMgr, _In_ CIoCompletionPortThreadPool &cWorkerPool,
                             _In_opt_ CLoggable *lpLogParent) : CBaseMemObj(),
                                                                CHttpServer(cSocketMgr, cWorkerPool, lpLogParent)
{
  cNewRequestObjectCallback = NullCallback();
  cRequestCallback = NullCallback();
  cRequireJsModuleCallback = NullCallback();
  cErrorCallback = NullCallback();
  cJavascriptErrorCallback = NullCallback();
  cWebSocketRequestReceivedCallback = NullCallback();
  //----
  bShowStackTraceOnError = TRUE;
  //----
  CHttpServer::SetNewRequestObjectCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnNewRequestObject, this));
  CHttpServer::SetRequestCompletedCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnRequestCompleted, this));
  CHttpServer::SetErrorCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnError, this));
  CHttpServer::SetWebSocketRequestReceivedCallback(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnWebSocketRequestReceived,
                                                                           this));
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

VOID CJsHttpServer::SetRequestCallback(_In_ OnRequestCallback _cRequestCallback)
{
  cRequestCallback = _cRequestCallback;
  return;
}

VOID CJsHttpServer::SetRequireJsModuleCallback(_In_ OnRequireJsModuleCallback _cRequireJsModuleCallback)
{
  cRequireJsModuleCallback = _cRequireJsModuleCallback;
  return;
}

VOID CJsHttpServer::SetErrorCallback(_In_ OnErrorCallback _cErrorCallback)
{
  cErrorCallback = _cErrorCallback;
  return;
}

VOID CJsHttpServer::SetJavascriptErrorCallback(_In_ OnJavascriptErrorCallback _cJavascriptErrorCallback)
{
  cJavascriptErrorCallback = _cJavascriptErrorCallback;
  return;
}

VOID CJsHttpServer::SetWebSocketRequestReceivedCallback(_In_ OnWebSocketRequestReceivedCallback
                                                        _cWebSocketRequestReceivedCallback)
{
  cWebSocketRequestReceivedCallback = _cWebSocketRequestReceivedCallback;
  return;
}

VOID CJsHttpServer::OnRequestCompleted(_In_ MX::CHttpServer *lpHttp, _In_ CHttpServer::CClientRequest *_lpRequest,
                                       _In_ HANDLE hShutdownEv)
{
  CClientRequest *lpRequest = static_cast<CClientRequest*>(_lpRequest);
  TAutoDeletePtr<CJavascriptVM> cJvm;
  CStringA cStrCodeA;
  CUrl *lpUrl;
  BOOL bDetached = FALSE;
  HRESULT hRes;

  if (!cRequestCallback)
  {
    hRes = E_FAIL;
on_init_error:
    ResetAndDisableClientCache(lpRequest);
    hRes = lpRequest->SendErrorPage(500, hRes);
    lpRequest->End(hRes);
    return;
  }
  //build path
  lpUrl = lpRequest->GetUrl();
  if (lpUrl == NULL) //only accept absolute paths
  {
    hRes = E_NOTIMPL;
    goto on_init_error;
  }
  if ((lpUrl->GetPath())[0] != L'/') //only accept absolute paths
  {
    hRes = lpRequest->SendErrorPage(403, E_INVALIDARG);
    ResetAndDisableClientCache(lpRequest);
    hRes = lpRequest->SendErrorPage(500, hRes);
    lpRequest->End(hRes);
    return;
  }

  hRes = cRequestCallback(this, lpRequest, &cJvm, cStrCodeA);
  if (FAILED(hRes))
    goto on_init_error;
  if (hRes == S_FALSE)
    return; //the callback owns the request now

  //transform provided code
  if (cStrCodeA.IsEmpty() == FALSE)
  {
    hRes = (cJvm) ? TransformJavascriptCode(cStrCodeA) : MX_E_NotReady;
    if (FAILED(hRes))
      goto on_init_error;

    //execute JS code
    if (cStrCodeA.IsEmpty() == FALSE)
    {
      try
      {
        cJvm->Run(cStrCodeA, lpUrl->GetPath());
        bDetached = lpRequest->bDetached;
      }
      catch (CJsHttpServerSystemExit &e)
      {
        if (*(e.GetDescription()) != 0)
        {
          CStringA cStrBodyA;

          hRes = ResetAndDisableClientCache(lpRequest);
          if (SUCCEEDED(hRes))
          {
            if (cStrBodyA.Format("Error: %s", e.GetDescription()) != FALSE)
            {
              hRes = HtmlEntities::ConvertTo(cStrBodyA);
              if (SUCCEEDED(hRes))
              {
                if (cStrBodyA.InsertN("<html><body><pre>", 0, 6 + 6 + 5) != FALSE &&
                    cStrBodyA.ConcatN("</pre></body></html>", 6 + 7 + 7) != FALSE)
                {
                  hRes = lpRequest->SendResponse((LPCSTR)cStrBodyA, cStrBodyA.GetLength());
                }
                else
                {
                  hRes = E_OUTOFMEMORY;
                }
              }
            }
            else
            {
              hRes = E_OUTOFMEMORY;
            }
          }
        }
      }
      catch (CJsWindowsError &e)
      {
        hRes = ResetAndDisableClientCache(lpRequest);
        if (SUCCEEDED(hRes))
        {
          hRes = BuildErrorPage(lpRequest, e.GetHResult(), e.GetDescription(), e.GetFileName(), e.GetLineNumber(),
                                e.GetStackTrace());
        }
      }
      catch (CJsError &e)
      {
        hRes = ResetAndDisableClientCache(lpRequest);
        if (SUCCEEDED(hRes))
        {
          hRes = BuildErrorPage(lpRequest, E_FAIL, e.GetDescription(), e.GetFileName(), e.GetLineNumber(),
                                e.GetStackTrace());
        }
      }
      catch (...)
      {
        hRes = E_FAIL;
      }
    }
    if (FAILED(hRes))
      goto on_init_error;
  }

  //done
  if (bDetached == FALSE)
    lpRequest->End(S_OK);
  return;
}

HRESULT CJsHttpServer::OnRequireJsModule(_In_ DukTape::duk_context *lpCtx,
                                         _In_ CJavascriptVM::CRequireModuleContext *lpReqContext,
                                         _Inout_ CStringA &cStrCodeA)
{
  CClientRequest *lpRequest;
  HRESULT hRes;

  if (!cRequireJsModuleCallback)
    return E_NOTIMPL;
  lpRequest = GetServerRequestFromContext(lpCtx);
  if (lpRequest == NULL)
    return E_FAIL;
  hRes = cRequireJsModuleCallback(this, lpRequest, *CJavascriptVM::FromContext(lpCtx), lpReqContext, cStrCodeA);
  if (SUCCEEDED(hRes))
    hRes = TransformJavascriptCode(cStrCodeA);
  //done
  return hRes;
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

HRESULT CJsHttpServer::OnWebSocketRequestReceived(_In_ CHttpServer *lpHttp,
                                                  _In_ CHttpServer::CClientRequest *_lpRequest, _In_ HANDLE hShutdownEv,
                                                  _Inout_ CHttpServer::WEBSOCKET_REQUEST_CALLBACK_DATA &sData)
{
  if (cWebSocketRequestReceivedCallback)
  {
    CClientRequest *lpRequest = static_cast<CClientRequest*>(_lpRequest);

    return cWebSocketRequestReceivedCallback(this, lpRequest, hShutdownEv, sData);
  }
  return MX_E_Unsupported;
}

HRESULT CJsHttpServer::ResetAndDisableClientCache(_In_ CClientRequest *lpRequest)
{
  HRESULT hRes;

  lpRequest->ResetResponse();
  hRes = lpRequest->AddResponseHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  if (SUCCEEDED(hRes))
    hRes = lpRequest->AddResponseHeader("Pragma", "no-cache");
  if (SUCCEEDED(hRes))
    hRes = lpRequest->AddResponseHeader("Expires", "0");
  return hRes;
}

HRESULT CJsHttpServer::BuildErrorPage(_In_ CClientRequest *lpRequest, _In_ HRESULT hr, _In_z_ LPCSTR szDescriptionA,
                                      _In_z_ LPCSTR szFileNameA, _In_ int nLine, _In_z_ LPCSTR szStackTraceA)
{
  CStringA cStrBodyA;
  HRESULT hRes;

  if (cStrBodyA.Format("Error 0x%08X: %s", hr, szDescriptionA) == FALSE)
    return E_OUTOFMEMORY;
  if (bShowStackTraceOnError != FALSE)
  {
    if (cStrBodyA.AppendFormat(" @ %s(%lu)\r\nStack trace:\r\n%s", szFileNameA, nLine, szStackTraceA) == FALSE)
      return E_OUTOFMEMORY;
  }
  hRes = HtmlEntities::ConvertTo(cStrBodyA);
  if (FAILED(hRes))
    return hRes;
  if (cStrBodyA.InsertN("<pre>", 0, 5) == FALSE || cStrBodyA.ConcatN("</pre>", 6) == FALSE)
    return E_OUTOFMEMORY;
  return lpRequest->SendErrorPage(500, hr, (LPCSTR)cStrBodyA);
}

} //namespace MX
