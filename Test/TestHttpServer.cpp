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
#include "TestHttpServer.h"
#include "Logger.h"
#include <Http\HttpServer.h>

//-----------------------------------------------------------

static VOID OnEngineError(_In_ MX::CIpc *lpIpc, _In_ HRESULT hrErrorCode);
static HRESULT OnRequestHeadersReceived(_In_ MX::CHttpServer *lpHttp, _In_ MX::CHttpServer::CClientRequest *lpRequest,
                                        _Inout_ MX::CHttpBodyParserBase **lplpBodyParser);
static VOID OnRequestCompleted(_In_ MX::CHttpServer *lpHttp, _In_ MX::CHttpServer::CClientRequest *lpRequest);
static VOID OnError(_In_ MX::CHttpServer *lpHttp, _In_ MX::CHttpServer::CClientRequest *lpRequest,
                    _In_ HRESULT hrErrorCode);

static HRESULT LoadTxtFile(_Inout_ MX::CStringA &cStrContentsA, _In_z_ LPCWSTR szFileNameW);

static HRESULT BuildWebFileName(_Inout_ MX::CStringW &cStrFullFileNameW, _Out_ LPCWSTR &szExtensionW,
                                _In_z_ LPCWSTR szPathW);

static HRESULT OnLog(_In_z_ LPCWSTR szInfoW);

//-----------------------------------------------------------

int TestHttpServer(_In_ BOOL bUseSSL, _In_ DWORD dwLogLevel)
{
  MX::CIoCompletionPortThreadPool cDispatcherPool, cWorkerPool;
  MX::CSockets cSckMgr(cDispatcherPool);
  MX::CHttpServer cHttpServer(cSckMgr, cWorkerPool);
  MX::CSslCertificate cSslCert;
  MX::CCryptoRSA cSslPrivateKey;
  HRESULT hRes;

  cHttpServer.SetLogCallback(MX_BIND_CALLBACK(&OnLog));
  cHttpServer.SetLogLevel(dwLogLevel);
  cSckMgr.SetLogCallback(MX_BIND_CALLBACK(&OnLog));
  cSckMgr.SetLogLevel(dwLogLevel);

  cSckMgr.SetOption_MaxAcceptsToPost(128);
  //cSckMgr.SetOption_PacketSize(32768);
  cHttpServer.SetOption_MaxFilesCount(10);
  //cSckMgr.SetOption_EnableZeroReads(FALSE);

  //cDispatcherPool.SetOption_MaxThreadsCount(32);
  //cDispatcherPool.SetOption_WorkerThreadIdleTime(INFINITE);

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
    {
      hRes = cHttpServer.StartListening(MX::CSockets::FamilyIPv6, 443, MX::CIpcSslLayer::ProtocolTLSv1_2, &cSslCert,
                                        &cSslPrivateKey);
    }
    else
    {
      hRes = cHttpServer.StartListening(MX::CSockets::FamilyIPv6, 80);
    }
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
                                        _Inout_ MX::CHttpBodyParserBase **lplpBodyParser)
{
  return S_OK;
}

static VOID OnRequestCompleted(_In_ MX::CHttpServer *lpHttp, _In_ MX::CHttpServer::CClientRequest *lpRequest)
{
  MX::CStringW cStrFileNameW;
  LPCWSTR szExtensionW;
  HRESULT hRes;

  hRes = BuildWebFileName(cStrFileNameW, szExtensionW, lpRequest->GetUrl()->GetPath());
  if (SUCCEEDED(hRes))
  {
    if (MX::StrCompareW(szExtensionW, L".html", TRUE) == 0 ||
        MX::StrCompareW(szExtensionW, L".dat", TRUE) == 0)
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
      MX::CHttpHeaderRespAcceptRanges *lpHdrAcceptRanges;
      MX::CHttpHeaderGenConnection *lpHdrConnection;
      MX::CHttpHeaderEntLastModified *lpHdrLastModified;

      //HANDLE h = lpRequest->GetUnderlyingSocket();
      //MX::CSockets *lpMgr = lpRequest->GetUnderlyingSocketManager();

      //lpMgr->PauseOutputProcessing(h);
      hRes = lpRequest->AddResponseHeader<MX::CHttpHeaderRespAcceptRanges>(&lpHdrAcceptRanges);
      if (SUCCEEDED(hRes))
        hRes = lpHdrAcceptRanges->SetRange(MX::CHttpHeaderRespAcceptRanges::RangeBytes);
      //----
      if (SUCCEEDED(hRes))
      {
        hRes = lpRequest->AddResponseHeader<MX::CHttpHeaderGenConnection>(&lpHdrConnection);
        if (SUCCEEDED(hRes))
          hRes = lpHdrConnection->AddConnection("close", 5);
      }
      //----
      if (SUCCEEDED(hRes))
      {
        MX::CDateTime cDt;

        hRes = lpRequest->AddResponseHeader<MX::CHttpHeaderEntLastModified>(&lpHdrLastModified);
        if (SUCCEEDED(hRes))
        {
          hRes = cDt.SetFromNow(FALSE);
          if (SUCCEEDED(hRes))
            hRes = lpHdrLastModified->SetDate(cDt);
        }
      }
      //----
      if (SUCCEEDED(hRes))
      {
        static LPCSTR szMessageA = "<html><body>test OK</body></html>";
        static SIZE_T nMessageLen = strlen(szMessageA);

        hRes = lpRequest->SendResponse(szMessageA, nMessageLen);
      }
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
  Logger::Log(L"%s\n", szInfoW);
  return S_OK;
}
