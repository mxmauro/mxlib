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

//-----------------------------------------------------------

namespace MX {

CJsHttpServer::CJsHttpServer(__in MX::CSockets &_cSocketMgr, __in MX::CPropertyBag &_cPropBag) : CBaseMemObj(),
               cSocketMgr(_cSocketMgr), cPropBag(_cPropBag), cHttpServer(_cSocketMgr, _cPropBag)
{
  cRequestCallback = NullCallback();
  cRequestCleanupCallback = NullCallback();
  cRequireJsModuleCallback = NullCallback();
  cErrorCallback = NullCallback();
  cJavascriptErrorCallback = NullCallback();
  //----
  bShowStackTraceOnError = TRUE;
  //----
  cHttpServer.On(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnNewRequestObject, this));
  cHttpServer.On(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnRequestCompleted, this));
  cHttpServer.On(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnError, this));
  return;
}

CJsHttpServer::~CJsHttpServer()
{
  StopListening();
  return;
}

VOID CJsHttpServer::On(__in OnRequestCallback _cRequestCallback)
{
  cRequestCallback = _cRequestCallback;
  return;
}

VOID CJsHttpServer::On(__in OnRequestCleanupCallback _cRequestCleanupCallback)
{
  cRequestCleanupCallback = _cRequestCleanupCallback;
  return;
}

VOID CJsHttpServer::On(__in OnRequireJsModuleCallback _cRequireJsModuleCallback)
{
  cRequireJsModuleCallback = _cRequireJsModuleCallback;
  return;
}

VOID CJsHttpServer::On(__in OnErrorCallback _cErrorCallback)
{
  cErrorCallback = _cErrorCallback;
  return;
}

VOID CJsHttpServer::On(__in OnJavascriptErrorCallback _cJavascriptErrorCallback)
{
  cJavascriptErrorCallback = _cJavascriptErrorCallback;
  return;
}

HRESULT CJsHttpServer::StartListening(__in int nPort, __in_opt CIpcSslLayer::eProtocol nProtocol,
                                      __in_opt CSslCertificate *lpSslCertificate, __in_opt CCryptoRSA *lpSslKey)
{
  return cHttpServer.StartListening(nPort, nProtocol, lpSslCertificate, lpSslKey);
}

HRESULT CJsHttpServer::StartListening(__in_z LPCSTR szBindAddressA, __in int nPort,
                                      __in_opt CIpcSslLayer::eProtocol nProtocol,
                                      __in_opt CSslCertificate *lpSslCertificate, __in_opt CCryptoRSA *lpSslKey)
{
  return cHttpServer.StartListening(szBindAddressA, nPort, nProtocol, lpSslCertificate, lpSslKey);
}

HRESULT CJsHttpServer::StartListening(__in_z LPCWSTR szBindAddressW, __in int nPort,
                                      __in_opt CIpcSslLayer::eProtocol nProtocol,
                                      __in_opt CSslCertificate *lpSslCertificate, __in_opt CCryptoRSA *lpSslKey)
{
  return cHttpServer.StartListening(szBindAddressW, nPort, nProtocol, lpSslCertificate, lpSslKey);
}

VOID CJsHttpServer::StopListening()
{
  cHttpServer.StopListening();
  return;
}

HRESULT CJsHttpServer::OnRequestCompleted(__in MX::CHttpServer *lpHttp, __in CHttpServer::CRequest *_lpRequest)
{
  CJsRequest *lpRequest = static_cast<CJsRequest*>(_lpRequest);
  CJavascriptVM cJvm;
  CStringA cStrCodeA;
  CUrl *lpUrl;
  HRESULT hRes;

  if (!cRequestCallback)
    return E_NOTIMPL;
  //build path
  lpUrl = lpRequest->GetUrl();
  if (lpUrl == NULL) //only accept absolute paths
    return MX_E_NotReady;
  if ((lpUrl->GetPath())[0] != L'/') //only accept absolute paths
    return lpRequest->SendErrorPage(403, E_INVALIDARG);
  //initialize javascript engine
  hRes = InitializeJVM(cJvm, lpRequest);
  if (SUCCEEDED(hRes))
  {
    cJvm.On(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnRequireJsModule, this));
    hRes = cRequestCallback(this, lpRequest, cJvm, cStrCodeA);
  }
  if (SUCCEEDED(hRes) && cStrCodeA.IsEmpty() == FALSE)
  {
    hRes = TransformJavascriptCode(cStrCodeA);
    if (SUCCEEDED(hRes))
    {
      if (cStrCodeA.IsEmpty() == FALSE)
      {
        try
        {
          cJvm.Run(cStrCodeA, lpUrl->GetPath());
        }
        catch (CJsHttpServerSystemExit &e)
        {
          if (*(e.GetDescription()) != 0)
          {
            CStringA cStrBodyA;
            HRESULT hRes2;

            lpRequest->ResetResponse();
            if (cStrBodyA.Format("Error: %s", e.GetDescription()) != FALSE)
              hRes2 = MX::CHttpCommon::ToHtmlEntities(cStrBodyA);
            else
              hRes2 = E_OUTOFMEMORY;
            if (SUCCEEDED(hRes2))
            {
              if (cStrBodyA.InsertN("<html><body><pre>", 0, 6+6+5) == FALSE ||
                  cStrBodyA.ConcatN("</pre></body></html>", 6+7+7) == FALSE)
              {
                hRes2 = E_OUTOFMEMORY;
              }
            }
            if (SUCCEEDED(hRes2))
              hRes2 = lpRequest->SendResponse((LPCSTR)cStrBodyA, cStrBodyA.GetLength());
            if (SUCCEEDED(hRes2))
              hRes = hRes2;
            else
              hRes = lpRequest->SendErrorPage(500, hRes2);
          }
        }
        catch (CJsError &e)
        {
          CStringA cStrBodyA;
          HRESULT hRes2;

          hRes2 = (cStrBodyA.Format("Error: %s", e.GetDescription()) != FALSE) ? S_OK : E_OUTOFMEMORY;
          if (SUCCEEDED(hRes2) && bShowStackTraceOnError != FALSE)
          {
            if (cStrBodyA.AppendFormat(" @ %s(%lu)\r\nStack trace:\r\n%s", e.GetFileName(), e.GetLineNumber(),
                                       e.GetStackTrace()) == FALSE)
            {
              hRes2 = E_OUTOFMEMORY;
            }
          }
          if (SUCCEEDED(hRes2))
            hRes2 = MX::CHttpCommon::ToHtmlEntities(cStrBodyA);
          if (SUCCEEDED(hRes2))
          {
            if (cStrBodyA.InsertN("<pre>", 0, 5) == FALSE || cStrBodyA.ConcatN("</pre>", 6) == FALSE)
              hRes2 = E_OUTOFMEMORY;
          }
          hRes = lpRequest->SendErrorPage(500, hRes, (SUCCEEDED(hRes2)) ? (LPCSTR)cStrBodyA : "");
        }
        catch (...)
        {
          hRes = lpRequest->SendErrorPage(500, E_FAIL);
        }
      }
    }
    else if (FAILED(hRes))
    {
      hRes = lpRequest->SendErrorPage(500, hRes, "");
    }
  }
  else if (FAILED(hRes))
  {
    hRes = lpRequest->SendErrorPage(500, hRes, "");
  }
  if (cRequestCleanupCallback)
    cRequestCleanupCallback(this, lpRequest, cJvm);
  //done
  return hRes;
}

HRESULT CJsHttpServer::OnRequireJsModule(__in DukTape::duk_context *lpCtx,
                                         __in CJavascriptVM::CRequireModuleContext *lpReqContext,
                                         __inout CStringA &cStrCodeA)
{
  CJsRequest *lpRequest;
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

VOID CJsHttpServer::OnError(__in CHttpServer *lpHttp, __in CHttpServer::CRequest *_lpRequest, __in HRESULT hErrorCode)
{
  if (cErrorCallback)
  {
    CJsRequest *lpRequest = static_cast<CJsRequest*>(_lpRequest);

    cErrorCallback(this, lpRequest, hErrorCode);
  }
  return;
}

} //namespace MX
