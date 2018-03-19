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
#include "TestHttpServer.h"
#include <Http\HttpServer.h>

//-----------------------------------------------------------

static VOID OnEngineError(_In_ MX::CIpc *lpIpc, _In_ HRESULT hErrorCode);
static HRESULT OnRequestHeadersReceived(_In_ MX::CHttpServer *lpHttp, _In_ MX::CHttpServer::CRequest *lpRequest,
                                        _Inout_ MX::CHttpBodyParserBase *&lpBodyParser);
static HRESULT OnRequestCompleted(_In_ MX::CHttpServer *lpHttp, _In_ MX::CHttpServer::CRequest *lpRequest);
static VOID OnError(_In_ MX::CHttpServer *lpHttp, _In_ MX::CHttpServer::CRequest *lpRequest, _In_ HRESULT hErrorCode);

static HRESULT LoadTxtFile(_Inout_ MX::CStringA &cStrContentsA, _In_z_ LPCWSTR szFileNameW);

//-----------------------------------------------------------

int TestHttpServer(_In_ BOOL bUseSSL)
{
  MX::CIoCompletionPortThreadPool cDispatcherPool;
  MX::CPropertyBag cPropBag;
  MX::CSockets cSckMgr(cDispatcherPool, cPropBag);
  MX::CHttpServer cHttpServer(cSckMgr, cPropBag);
  MX::CSslCertificate cSslCert;
  MX::CCryptoRSA cSslPrivateKey;
  HRESULT hRes;

  hRes = cDispatcherPool.Initialize();
  if (SUCCEEDED(hRes))
  {
    cSckMgr.On(MX_BIND_CALLBACK(&OnEngineError));
    hRes = cSckMgr.Initialize();
  }
  if (SUCCEEDED(hRes) && bUseSSL != FALSE)
  {
    MX::CStringA cStrTempA;
    MX::CStringW cStrTempW;

    //load SSL certificate
    hRes = GetAppPath(cStrTempW);
    if (SUCCEEDED(hRes) && cStrTempW.Concat(L"Web\\Certificates\\ssl.crt") == FALSE)
      hRes = E_OUTOFMEMORY;
    if (SUCCEEDED(hRes))
      hRes = LoadTxtFile(cStrTempA, (LPCWSTR)cStrTempW);
    if (SUCCEEDED(hRes))
      hRes = cSslCert.InitializeFromPEM((LPCSTR)cStrTempA);
    //load private key
    if (SUCCEEDED(hRes))
      hRes = GetAppPath(cStrTempW);
    if (SUCCEEDED(hRes) && cStrTempW.Concat(L"Web\\Certificates\\ssl.key") == FALSE)
      hRes = E_OUTOFMEMORY;
    if (SUCCEEDED(hRes))
      hRes = LoadTxtFile(cStrTempA, (LPCWSTR)cStrTempW);
    if (SUCCEEDED(hRes))
      hRes = cSslPrivateKey.SetPrivateKeyFromPEM((LPCSTR)cStrTempA);
  }
  if (SUCCEEDED(hRes))
  {
    cHttpServer.On(MX_BIND_CALLBACK(&OnRequestHeadersReceived));
    cHttpServer.On(MX_BIND_CALLBACK(&OnRequestCompleted));
    cHttpServer.On(MX_BIND_CALLBACK(&OnError));
  }
  if (SUCCEEDED(hRes))
  {
    if (bUseSSL != FALSE)
      hRes = cHttpServer.StartListening(443, MX::CIpcSslLayer::ProtocolTLSv1_2, &cSslCert, &cSslPrivateKey);
    else
      hRes = cHttpServer.StartListening(80);
  }
  //----
  if (SUCCEEDED(hRes))
  {
    while (ShouldAbort() == FALSE)
      ::Sleep(100);
  }
  //done
  return (int)hRes;
}

static VOID OnEngineError(_In_ MX::CIpc *lpIpc, _In_ HRESULT hErrorCode)
{
  return;
}

static HRESULT OnRequestHeadersReceived(_In_ MX::CHttpServer *lpHttp, _In_ MX::CHttpServer::CRequest *lpRequest,
                                        _Inout_ MX::CHttpBodyParserBase *&lpBodyParser)
{
  return S_OK;
}

static HRESULT OnRequestCompleted(_In_ MX::CHttpServer *lpHttp, _In_ MX::CHttpServer::CRequest *lpRequest)
{
  HRESULT hRes;

  //HANDLE h = lpRequest->GetUnderlyingSocket();
  //MX::CSockets *lpMgr = lpRequest->GetUnderlyingSocketManager();

  //lpMgr->PauseOutputProcessing(h);
  hRes = lpRequest->SendResponse("<html><body>test OK</body></html>", 6+6+ 7 +7+7);
  //lpMgr->ResumeOutputProcessing(h);
  return hRes;
}

static VOID OnError(_In_ MX::CHttpServer *lpHttp, _In_ MX::CHttpServer::CRequest *lpRequest, _In_ HRESULT hErrorCode)
{
  return;
}

static HRESULT LoadTxtFile(_Inout_ MX::CStringA &cStrContentsA, _In_z_ LPCWSTR szFileNameW)
{
  MX::CWindowsHandle cFileH;
  DWORD dw, dw2;

  cStrContentsA.Empty();
  if (szFileNameW == NULL)
    return E_POINTER;
  cFileH.Attach(::CreateFileW(szFileNameW, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              NULL));
  if (!cFileH)
    return MX_HRESULT_FROM_LASTERROR();
  dw = ::GetFileSize(cFileH, &dw2);
  if (dw == 0 || dw == DWORD_MAX || dw2 != 0)
    return MX_E_InvalidData;
  if (cStrContentsA.EnsureBuffer((SIZE_T)(dw+1)) == FALSE)
    return E_OUTOFMEMORY;
  if (::ReadFile(cFileH, (LPSTR)cStrContentsA, dw, &dw2, NULL) == FALSE)
    return MX_HRESULT_FROM_LASTERROR();
  if (dw != dw2)
    return MX_E_ReadFault;
  ((LPSTR)cStrContentsA)[dw] = 0;
  cStrContentsA.Refresh();
  return S_OK;
}
