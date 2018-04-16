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

CJsHttpServer::CJsHttpServer(_In_ MX::CSockets &_cSocketMgr) : CBaseMemObj(), cSocketMgr(_cSocketMgr),
                                                               cHttpServer(_cSocketMgr)
{
  cNewRequestObjectCallback = NullCallback();
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

VOID CJsHttpServer::SetOption_MaxRequestTimeoutMs(_In_ DWORD dwTimeoutMs)
{
  cHttpServer.SetOption_MaxRequestTimeoutMs(dwTimeoutMs);
  return;
}

VOID CJsHttpServer::SetOption_MaxHeaderSize(_In_ DWORD dwSize)
{
  cHttpServer.SetOption_MaxHeaderSize(dwSize);
  return;
}

VOID CJsHttpServer::SetOption_MaxFieldSize(_In_ DWORD dwSize)
{
  cHttpServer.SetOption_MaxFieldSize(dwSize);
  return;
}

VOID CJsHttpServer::SetOption_MaxFileSize(_In_ ULONGLONG ullSize)
{
  cHttpServer.SetOption_MaxFileSize(ullSize);
  return;
}

VOID CJsHttpServer::SetOption_MaxFilesCount(_In_ DWORD dwCount)
{
  cHttpServer.SetOption_MaxFilesCount(dwCount);
  return;
}

BOOL CJsHttpServer::SetOption_TemporaryFolder(_In_opt_z_ LPCWSTR szFolderW)
{
  return cHttpServer.SetOption_TemporaryFolder(szFolderW);
}

VOID CJsHttpServer::SetOption_MaxBodySizeInMemory(_In_ DWORD dwSize)
{
  cHttpServer.SetOption_MaxBodySizeInMemory(dwSize);
  return;
}

VOID CJsHttpServer::SetOption_MaxBodySize(_In_ ULONGLONG ullSize)
{
  cHttpServer.SetOption_MaxBodySize(ullSize);
  return;
}

VOID CJsHttpServer::On(_In_ OnNewRequestObjectCallback _cNewRequestObjectCallback)
{
  cNewRequestObjectCallback = _cNewRequestObjectCallback;
  return;
}

VOID CJsHttpServer::On(_In_ OnRequestCallback _cRequestCallback)
{
  cRequestCallback = _cRequestCallback;
  return;
}

VOID CJsHttpServer::On(_In_ OnRequestCleanupCallback _cRequestCleanupCallback)
{
  cRequestCleanupCallback = _cRequestCleanupCallback;
  return;
}

VOID CJsHttpServer::On(_In_ OnRequireJsModuleCallback _cRequireJsModuleCallback)
{
  cRequireJsModuleCallback = _cRequireJsModuleCallback;
  return;
}

VOID CJsHttpServer::On(_In_ OnErrorCallback _cErrorCallback)
{
  cErrorCallback = _cErrorCallback;
  return;
}

VOID CJsHttpServer::On(_In_ OnJavascriptErrorCallback _cJavascriptErrorCallback)
{
  cJavascriptErrorCallback = _cJavascriptErrorCallback;
  return;
}

HRESULT CJsHttpServer::StartListening(_In_ int nPort, _In_opt_ CIpcSslLayer::eProtocol nProtocol,
                                      _In_opt_ CSslCertificate *lpSslCertificate, _In_opt_ CCryptoRSA *lpSslKey)
{
  return cHttpServer.StartListening(nPort, nProtocol, lpSslCertificate, lpSslKey);
}

HRESULT CJsHttpServer::StartListening(_In_z_ LPCSTR szBindAddressA, _In_ int nPort,
                                      _In_opt_ CIpcSslLayer::eProtocol nProtocol,
                                      _In_opt_ CSslCertificate *lpSslCertificate, _In_opt_ CCryptoRSA *lpSslKey)
{
  return cHttpServer.StartListening(szBindAddressA, nPort, nProtocol, lpSslCertificate, lpSslKey);
}

HRESULT CJsHttpServer::StartListening(_In_z_ LPCWSTR szBindAddressW, _In_ int nPort,
                                      _In_opt_ CIpcSslLayer::eProtocol nProtocol,
                                      _In_opt_ CSslCertificate *lpSslCertificate, _In_opt_ CCryptoRSA *lpSslKey)
{
  return cHttpServer.StartListening(szBindAddressW, nPort, nProtocol, lpSslCertificate, lpSslKey);
}

VOID CJsHttpServer::StopListening()
{
  cHttpServer.StopListening();
  return;
}

HRESULT CJsHttpServer::OnRequestCompleted(_In_ MX::CHttpServer *lpHttp, _In_ CHttpServer::CRequest *_lpRequest)
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

            hRes = ResetAndDisableClientCache(lpRequest);
            if (SUCCEEDED(hRes))
            {
              if (cStrBodyA.Format("Error: %s", e.GetDescription()) != FALSE)
              {
                hRes = HtmlEntities::ConvertTo(cStrBodyA);
                if (SUCCEEDED(hRes))
                {
                  if (cStrBodyA.InsertN("<html><body><pre>", 0, 6+6+5) != FALSE &&
                      cStrBodyA.ConcatN("</pre></body></html>", 6+7+7) != FALSE)
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
    }
  }
  if (FAILED(hRes))
  {
    ResetAndDisableClientCache(lpRequest);
    hRes = lpRequest->SendErrorPage(500, hRes);
  }
  if (cRequestCleanupCallback)
    cRequestCleanupCallback(this, lpRequest, cJvm);
  //done
  return hRes;
}

HRESULT CJsHttpServer::OnRequireJsModule(_In_ DukTape::duk_context *lpCtx,
                                         _In_ CJavascriptVM::CRequireModuleContext *lpReqContext,
                                         _Inout_ CStringA &cStrCodeA)
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

VOID CJsHttpServer::OnError(_In_ CHttpServer *lpHttp, _In_ CHttpServer::CRequest *_lpRequest, _In_ HRESULT hErrorCode)
{
  if (cErrorCallback)
  {
    CJsRequest *lpRequest = static_cast<CJsRequest*>(_lpRequest);

    cErrorCallback(this, lpRequest, hErrorCode);
  }
  return;
}

HRESULT CJsHttpServer::ResetAndDisableClientCache(_In_ CJsRequest *lpRequest)
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

HRESULT CJsHttpServer::BuildErrorPage(_In_ CJsRequest *lpRequest, _In_ HRESULT hr, _In_z_ LPCSTR szDescriptionA,
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
