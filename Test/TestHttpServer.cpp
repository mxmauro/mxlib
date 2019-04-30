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

static LONG volatile nLogMutex = 0;

//-----------------------------------------------------------

static VOID OnEngineError(_In_ MX::CIpc *lpIpc, _In_ HRESULT hrErrorCode);
static HRESULT OnRequestHeadersReceived(_In_ MX::CHttpServer *lpHttp, _In_ MX::CHttpServer::CClientRequest *lpRequest,
                                        _In_ HANDLE hShutdownEv, _Inout_ MX::CHttpBodyParserBase **lplpBodyParser);
static VOID OnRequestCompleted(_In_ MX::CHttpServer *lpHttp, _In_ MX::CHttpServer::CClientRequest *lpRequest,
                               _In_ HANDLE hShutdownEv);
static VOID OnError(_In_ MX::CHttpServer *lpHttp, _In_ MX::CHttpServer::CClientRequest *lpRequest,
                    _In_ HRESULT hrErrorCode);

static HRESULT LoadTxtFile(_Inout_ MX::CStringA &cStrContentsA, _In_z_ LPCWSTR szFileNameW);

static HRESULT BuildWebFileName(_Inout_ MX::CStringW &cStrFullFileNameW, _Out_ LPCWSTR &szExtensionW,
                                _In_z_ LPCWSTR szPathW);

static HRESULT OnLog(_In_z_ LPCWSTR szInfoW);

//-----------------------------------------------------------

int TestHttpServer(_In_ BOOL bUseSSL)
{
  MX::CIoCompletionPortThreadPool cDispatcherPool, cWorkerPool;
  MX::CSockets cSckMgr(cDispatcherPool);
  MX::CHttpServer cHttpServer(cSckMgr, cWorkerPool);
  MX::CSslCertificate cSslCert;
  MX::CCryptoRSA cSslPrivateKey;
  HRESULT hRes;

  cHttpServer.SetLogCallback(MX_BIND_CALLBACK(&OnLog));
  cHttpServer.SetLogLevel(5);

  cSckMgr.SetOption_MaxAcceptsToPost(24);
  //cSckMgr.SetOption_PacketSize(32768);
  cHttpServer.SetOption_MaxFilesCount(10);

  hRes = cDispatcherPool.Initialize();
  if (SUCCEEDED(hRes))
    hRes = cWorkerPool.Initialize();
  if (SUCCEEDED(hRes))
  {
    cSckMgr.SetEngineErrorCallback(MX_BIND_CALLBACK(&OnEngineError));
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
    cHttpServer.SetRequestHeadersReceivedCallback(MX_BIND_CALLBACK(&OnRequestHeadersReceived));
    cHttpServer.SetRequestCompletedCallback(MX_BIND_CALLBACK(&OnRequestCompleted));
    cHttpServer.SetErrorCallback(MX_BIND_CALLBACK(&OnError));
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

static VOID OnEngineError(_In_ MX::CIpc *lpIpc, _In_ HRESULT hrErrorCode)
{
  return;
}

static HRESULT OnRequestHeadersReceived(_In_ MX::CHttpServer *lpHttp, _In_ MX::CHttpServer::CClientRequest *lpRequest,
                                        _In_ HANDLE hShutdownEv, _Inout_ MX::CHttpBodyParserBase **lplpBodyParser)
{
  return S_OK;
}

static VOID OnRequestCompleted(_In_ MX::CHttpServer *lpHttp, _In_ MX::CHttpServer::CClientRequest *lpRequest,
                               _In_ HANDLE hShutdownEv)
{
  MX::CStringW cStrFileNameW;
  LPCWSTR szExtensionW;
  HRESULT hRes;

  hRes = BuildWebFileName(cStrFileNameW, szExtensionW, lpRequest->GetUrl()->GetPath());
  if (SUCCEEDED(hRes))
  {
    if (MX::StrCompareW(szExtensionW, L".dat", TRUE) == 0)
    {
      hRes = lpRequest->SendFile((LPCWSTR)cStrFileNameW);
      if (hRes == MX_HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
          hRes == MX_HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND))
      {
        hRes = lpRequest->SendErrorPage(404, E_INVALIDARG);
      }
    }
    else
    {
      //HANDLE h = lpRequest->GetUnderlyingSocket();
      //MX::CSockets *lpMgr = lpRequest->GetUnderlyingSocketManager();

      //lpMgr->PauseOutputProcessing(h);
      hRes = lpRequest->SendResponse("<html><body>test OK</body></html>", 6 + 6 + 7 + 7 + 7);
      //lpMgr->ResumeOutputProcessing(h);
    }
  }
  lpRequest->End(hRes);
  return;
}

static VOID OnError(_In_ MX::CHttpServer *lpHttp, _In_ MX::CHttpServer::CClientRequest *lpRequest,
                    _In_ HRESULT hrErrorCode)
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

static HRESULT BuildWebFileName(_Inout_ MX::CStringW &cStrFullFileNameW, _Out_ LPCWSTR &szExtensionW,
                                _In_z_ LPCWSTR szPathW)
{
  LPWSTR sW;
  HRESULT hRes;

  szExtensionW = NULL;
  hRes = GetAppPath(cStrFullFileNameW);
  if (FAILED(hRes))
    return hRes;
  if (cStrFullFileNameW.Concat(L"Web\\") == FALSE)
    return E_OUTOFMEMORY;
  while (*szPathW == L'/')
    szPathW++;
  if (cStrFullFileNameW.Concat(szPathW) == FALSE)
    return FALSE;
  for (sW = (LPWSTR)cStrFullFileNameW; *sW != 0; sW++)
  {
    if (*sW == L'/')
      *sW = L'\\';
  }
  if (*(sW - 1) == L'\\')
  {
    if (cStrFullFileNameW.Concat(L"index.jss") == FALSE)
      return E_OUTOFMEMORY;
  }
  //----
  sW = (LPWSTR)cStrFullFileNameW + (cStrFullFileNameW.GetLength() - 1);
  while (sW >= (LPWSTR)cStrFullFileNameW && *sW != L'.' && *sW != L'/')
    sW--;
  szExtensionW = (sW >= (LPWSTR)cStrFullFileNameW && *sW == L'.') ? sW : L"";
  return S_OK;
}

static HRESULT OnLog(_In_z_ LPCWSTR szInfoW)
{
  MX::CFastLock cLock(&nLogMutex);

  wprintf_s(L"%s\n", szInfoW);
  return S_OK;
}
