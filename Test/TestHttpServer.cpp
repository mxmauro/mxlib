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

static VOID OnEngineError(__in MX::CIpc *lpIpc, __in HRESULT hErrorCode);
static HRESULT OnRequestHeadersReceived(__in MX::CHttpServer *lpHttp, __in MX::CHttpServer::CRequest *lpRequest,
                                        __inout MX::CHttpBodyParserBase *&lpBodyParser);
static HRESULT OnRequestCompleted(__in MX::CHttpServer *lpHttp, __in MX::CHttpServer::CRequest *lpRequest);
static VOID OnError(__in MX::CHttpServer *lpHttp, __in MX::CHttpServer::CRequest *lpRequest, __in HRESULT hErrorCode);

static HRESULT LoadTxtFile(__inout MX::CStringA &cStrContentsA, __in_z LPCWSTR szFileNameW);

//-----------------------------------------------------------

int TestHttpServer()
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
  if (SUCCEEDED(hRes))
  {
    MX::CStringA cStrTempA;
    MX::CStringW cStrTempW;
    HRESULT hRes;

    //load SSL certificate
    hRes = GetAppPath(cStrTempW);
    if (SUCCEEDED(hRes) && cStrTempW.Concat(L"ssl.crt") == FALSE)
      hRes = E_OUTOFMEMORY;
    if (SUCCEEDED(hRes))
      hRes = LoadTxtFile(cStrTempA, (LPCWSTR)cStrTempW);
    if (SUCCEEDED(hRes))
      hRes = cSslCert.InitializeFromPEM((LPCSTR)cStrTempA);
    //load private key
    if (SUCCEEDED(hRes))
      hRes = GetAppPath(cStrTempW);
    if (SUCCEEDED(hRes) && cStrTempW.Concat(L"ssl.key") == FALSE)
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
    hRes = cHttpServer.StartListening(443, MX::CIpcSslLayer::ProtocolTLSv1_2, &cSslCert, &cSslPrivateKey);
  //----
  if (SUCCEEDED(hRes))
  {
    while (ShouldAbort() == FALSE)
      ::Sleep(100);
  }
  //done
  return (int)hRes;
}

static VOID OnEngineError(__in MX::CIpc *lpIpc, __in HRESULT hErrorCode)
{
  return;
}

static HRESULT OnRequestHeadersReceived(__in MX::CHttpServer *lpHttp, __in MX::CHttpServer::CRequest *lpRequest,
                                        __inout MX::CHttpBodyParserBase *&lpBodyParser)
{
  return S_OK;
}

static HRESULT OnRequestCompleted(__in MX::CHttpServer *lpHttp, __in MX::CHttpServer::CRequest *lpRequest)
{
  HRESULT hRes;

  hRes = lpRequest->SendResponse("<html><body>test OK</body></html>", 6+6+ 7 +7+7);
  return hRes;
}

static VOID OnError(__in MX::CHttpServer *lpHttp, __in MX::CHttpServer::CRequest *lpRequest, __in HRESULT hErrorCode)
{
  return;
}

static HRESULT LoadTxtFile(__inout MX::CStringA &cStrContentsA, __in_z LPCWSTR szFileNameW)
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
