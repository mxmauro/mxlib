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
#include "TestHttpClient.h"
#include <Http\HttpClient.h>
#include <RefCounted.h>

//-----------------------------------------------------------

class CMyHttpClient : public MX::CHttpClient
{
public:
  CMyHttpClient(_In_ MX::CSockets &cSocketMgr) : MX::CHttpClient(cSocketMgr)
    {
    return;
    };

public:
  MX::CSslCertificateArray *lpCertificates;
};

//-----------------------------------------------------------

static LONG volatile nLogMutex = 0;

//-----------------------------------------------------------

class CTestHttpClientWebSocket : public MX::CWebSocket
{
public:
  CTestHttpClientWebSocket();
  ~CTestHttpClientWebSocket();

  virtual HRESULT OnConnected();
  virtual HRESULT OnTextMessage(_In_ LPCSTR szMsgA, _In_ SIZE_T nMsgLength);
  virtual HRESULT OnBinaryMessage(_In_ LPVOID lpData, _In_ SIZE_T nDataSize);

  virtual SIZE_T GetMaxMessageSize() const;
  virtual VOID OnPongFrame();
  virtual VOID OnCloseFrame(_In_ USHORT wCode, _In_  HRESULT hrErrorCode);
};

//-----------------------------------------------------------

static VOID OnEngineError(_In_ MX::CIpc *lpIpc, _In_ HRESULT hrErrorCode);
static HRESULT OnResponseHeadersReceived(_In_ MX::CHttpClient *lpHttp, _In_z_ LPCWSTR szFileNameW,
                                         _In_opt_ PULONGLONG lpnContentSize, _In_z_ LPCSTR szTypeA,
                                         _In_ BOOL bTreatAsAttachment, _Inout_ MX::CStringW &cStrFullFileNameW,
                                         _Inout_ MX::CHttpBodyParserBase **lplpBodyParser);
static HRESULT OnResponseHeadersReceived_BigDownload(_In_ MX::CHttpClient *lpHttp, _In_z_ LPCWSTR szFileNameW,
                                         _In_opt_ PULONGLONG lpnContentSize, _In_z_ LPCSTR szTypeA,
                                          _In_ BOOL bTreatAsAttachment, _Inout_ MX::CStringW &cStrFullFileNameW,
                                         _Inout_ MX::CHttpBodyParserBase **lplpBodyParser);
static VOID OnDocumentCompleted(_In_ MX::CHttpClient *lpHttp);
static VOID OnError(_In_ MX::CHttpClient *lpHttp, _In_ HRESULT hrErrorCode);
static HRESULT OnQueryCertificates(_In_ MX::CHttpClient *lpHttp, _Inout_ MX::CSslCertificateArray **lplpCertificates,
                                   _Inout_ MX::CSslCertificate **lplpSelfCert,
                                   _Inout_ MX::CEncryptionKey **lplpPrivKey);

static HRESULT SimpleTest1(_In_ MX::CSockets &cSocketkMgr, _In_ MX::CSslCertificateArray *lpCertificates);
static HRESULT WebSocketTest(_In_ MX::CSockets &cSocketkMgr, _In_ MX::CSslCertificateArray *lpCertificates);
static VOID HttpClientJob(_In_ MX::CWorkerThread *lpWrkThread, _In_ LPVOID lpParam);

static HRESULT CheckHttpClientResponse(_In_ CMyHttpClient *lpHttpClient, _In_ BOOL bExpectHtml);

static HRESULT OnLog(_In_z_ LPCWSTR szInfoW);

static BOOL _GetTempPath(_Out_ MX::CStringW &cStrPathW);

//-----------------------------------------------------------

typedef struct {
  int nIndex;
  MX::CSockets *lpSocketMgr;
  MX::CSslCertificateArray *lpCertificates;
  MX::CWorkerThread cWorkerThreads;
  DWORD dwLogLevel;
} THREAD_DATA;

//-----------------------------------------------------------

int TestHttpClient()
{
  MX::CIoCompletionPortThreadPool cDispatcherPool;
  MX::CSockets cSocketMgr(cDispatcherPool);
  MX::CSslCertificateArray cCertificates;
  HRESULT hRes;

  hRes = cDispatcherPool.Initialize();
  if (SUCCEEDED(hRes))
  {
    cSocketMgr.SetEngineErrorCallback(MX_BIND_CALLBACK(&OnEngineError));
    hRes = cSocketMgr.Initialize();
  }
  if (SUCCEEDED(hRes))
  {
    cSocketMgr.SetLogCallback(MX_BIND_CALLBACK(&OnLog));
    cSocketMgr.SetLogLevel(dwLogLevel);

    hRes = cCertificates.ImportFromWindowsStore();
  }

  if (SUCCEEDED(hRes))
  {
    if (DoesCmdLineParamExist(L"simple1") != FALSE)
    {
      hRes = SimpleTest1(cSocketMgr, &cCertificates);
    }
    else if (DoesCmdLineParamExist(L"websocket") != FALSE)
    {
      hRes = WebSocketTest(cSocketMgr, &cCertificates);
    }
    else
    {
      THREAD_DATA sThreadData[20];
      BOOL bActive;
      SIZE_T i;

      for (i = 0; SUCCEEDED(hRes) && i < MX_ARRAYLEN(sThreadData); i++)
      {
        sThreadData[i].nIndex = (int)i + 1;
        sThreadData[i].lpSocketMgr = &cSocketMgr;
        sThreadData[i].lpCertificates = &cCertificates;
        sThreadData[i].dwLogLevel = dwLogLevel;
        if (sThreadData[i].cWorkerThreads.SetRoutine(&HttpClientJob, &sThreadData[i]) == FALSE ||
            sThreadData[i].cWorkerThreads.Start() == FALSE)
        {
          hRes = E_OUTOFMEMORY;
        }
      }
      //----
      if (SUCCEEDED(hRes))
      {
        do
        {
          bActive = FALSE;
          for (i = 0; i < MX_ARRAYLEN(sThreadData); i++)
          {
            if (sThreadData[i].cWorkerThreads.Wait(10) == FALSE)
              bActive = TRUE;
          }
        }
        while (bActive != FALSE && ShouldAbort() == FALSE);
      }

      for (i = 0; i < MX_ARRAYLEN(sThreadData); i++)
        sThreadData[i].cWorkerThreads.Stop();
    }
  }

  //done
  return (int)hRes;
}

static VOID OnEngineError(_In_ MX::CIpc *lpIpc, _In_ HRESULT hrErrorCode)
{
  return;
}

static HRESULT OnResponseHeadersReceived(_In_ MX::CHttpClient *lpHttp, _In_z_ LPCWSTR szFileNameW,
                                         _In_opt_ PULONGLONG lpnContentSize, _In_z_ LPCSTR szTypeA,
                                         _In_ BOOL bTreatAsAttachment, _Inout_ MX::CStringW &cStrFullFileNameW,
                                         _Inout_ MX::CHttpBodyParserBase **lplpBodyParser)
{
  cStrFullFileNameW.Empty();
  return S_OK;
}

static HRESULT OnResponseHeadersReceived_BigDownload(_In_ MX::CHttpClient *lpHttp, _In_z_ LPCWSTR szFileNameW,
                                                     _In_opt_ PULONGLONG lpnContentSize, _In_z_ LPCSTR szTypeA,
                                                     _In_ BOOL bTreatAsAttachment, _Inout_ MX::CStringW &cStrFullFileNameW,
                                                     _Inout_ MX::CHttpBodyParserBase **lplpBodyParser)
{
  if (_GetTempPath(cStrFullFileNameW) == FALSE)
    return E_OUTOFMEMORY;
  if (cStrFullFileNameW.Concat(szFileNameW) == FALSE)
    return E_OUTOFMEMORY;
  return S_OK;
}

static VOID OnDocumentCompleted(_In_ MX::CHttpClient *lpHttp)
{
  return;
}

static VOID OnError(_In_ MX::CHttpClient *lpHttp, _In_ HRESULT hrErrorCode)
{
  return;
}

static HRESULT OnQueryCertificates(_In_ MX::CHttpClient *_lpHttp, _Inout_ MX::CSslCertificateArray **lplpCertificates,
                                   _Inout_ MX::CSslCertificate **lplpSelfCert, _Inout_ MX::CEncryptionKey **lplpPrivKey)
{
  CMyHttpClient *lpHttp = static_cast<CMyHttpClient*>(_lpHttp);

  *lplpCertificates = lpHttp->lpCertificates;
  return S_OK;
}

//-----------------------------------------------------------

static HRESULT SimpleTest1(_In_ MX::CSockets &cSocketkMgr, _In_ MX::CSslCertificateArray *lpCertificates)
{
  MX::TAutoRefCounted<CMyHttpClient> cHttpClient;
  MX::CProxy cProxy;
  DWORD dwStartTime, dwEndTime;
  HRESULT hRes;

  cHttpClient.Attach(MX_DEBUG_NEW CMyHttpClient(cSocketkMgr));
  if (!cHttpClient)
    return E_OUTOFMEMORY;
    cProxy.SetUseIE();
  //cProxy.SetManual(L"127.0.0.1:808");
  //cProxy.SetCredentials(L"guest", L"invitado");
  //cHttpClient->SetProxy(cProxy);
  cHttpClient->lpCertificates = lpCertificates;
  //cHttpClient->SetOptionFlags(0);
  //cHttpClient->SetOptionFlags(MX::CHttpClient::OptionKeepConnectionOpen);

  cHttpClient->SetDocumentCompletedCallback(MX_BIND_CALLBACK(&OnDocumentCompleted));
  cHttpClient->SetErrorCallback(MX_BIND_CALLBACK(&OnError));
  cHttpClient->SetQueryCertificatesCallback(MX_BIND_CALLBACK(&OnQueryCertificates));
  cHttpClient->SetLogLevel(dwLogLevel);
  cHttpClient->SetLogCallback(MX_BIND_CALLBACK(&OnLog));

  wprintf_s(L"[HttpClient/SimpleTest1] Downloading...\n");
  dwStartTime = dwEndTime = ::GetTickCount();

  //cHttpClient->SetHeadersReceivedCallback(MX_BIND_CALLBACK(&OnResponseHeadersReceived));
  cHttpClient->SetHeadersReceivedCallback(MX_BIND_CALLBACK(&OnResponseHeadersReceived_BigDownload));

  hRes = cHttpClient->Open("http://www.sitepoint.com/forums/showthread.php?"
                           "390414-Reading-from-socket-connection-SLOW");
  /*
  //hRes = cHttpClient->SetAuthCredentials(L"guest", L"guest");
  hRes = S_OK;
  if (SUCCEEDED(hRes))
  {
    cHttpClient->SetOption_MaxBodySizeInMemory(0);

    hRes = cHttpClient->AddRequestHeader("x-version", L"3");
    if (SUCCEEDED(hRes))
      hRes = cHttpClient->AddRequestHeader("license", L"39F2E487ED3D3489C49756833E5F7C7D1CEF7482FE9EF46B2549A0968DE83C99");
    if (SUCCEEDED(hRes))
      hRes = cHttpClient->Open("https://airgap.trapmine.com/ml_model_check");
    //hRes = cHttpClient->Open("https://jigsaw.w3.org/HTTP/Basic/");
    //hRes = cHttpClient->Open("https://jigsaw.w3.org/HTTP/Digest/");
  }
  */

  if (SUCCEEDED(hRes))
  {
    while (ShouldAbort() == FALSE && cHttpClient->IsDocumentComplete() == FALSE && cHttpClient->IsClosed() == FALSE)
    {
      ::Sleep(50);
    }
#pragma warning(suppress : 28159)
    dwEndTime = ::GetTickCount();
    if (ShouldAbort() == FALSE)
      hRes = CheckHttpClientResponse(cHttpClient.Get(), TRUE);
    else
      hRes = MX_E_Cancelled;

    //print results
    switch (hRes)
    {
      case 0x80070000 | ERROR_CANCELLED:
        wprintf_s(L"[HttpClient/SimpleTest1] Cancelled by user.\n");
        break;

      case S_FALSE:
        wprintf_s(L"[HttpClient/SimpleTest1] Body is NOT complete. Download in %lums\n", dwEndTime - dwStartTime);
        break;

      case S_OK:
        wprintf_s(L"[HttpClient/SimpleTest1] Successful download in %lums / Status:%ld\n", dwEndTime - dwStartTime,
                  cHttpClient->GetResponseStatus());
        break;

      default:
        wprintf_s(L"[HttpClient/SimpleTest1] Error 0x%08X / Status:%ld\n", hRes, cHttpClient->GetResponseStatus());
        break;
    }
  }
  return hRes;
}

//-----------------------------------------------------------

static HRESULT WebSocketTest(_In_ MX::CSockets &cSocketMgr, _In_ MX::CSslCertificateArray *lpCertificates)
{
  MX::TAutoRefCounted<CMyHttpClient> cHttpClient;
  MX::TAutoRefCounted<CTestHttpClientWebSocket> cWebSocket;
  MX::CHttpClient::OPEN_OPTIONS sOptions;
  MX::CProxy cProxy;
  HRESULT hRes;
  

  cHttpClient.Attach(MX_DEBUG_NEW CMyHttpClient(cSocketMgr));
  if (!cHttpClient)
    return E_OUTOFMEMORY;
  cProxy.SetUseIE();
  cHttpClient->lpCertificates = lpCertificates;
  cHttpClient->SetErrorCallback(MX_BIND_CALLBACK(&OnError));
  cHttpClient->SetQueryCertificatesCallback(MX_BIND_CALLBACK(&OnQueryCertificates));
  cHttpClient->SetLogLevel(dwLogLevel);
  cHttpClient->SetLogCallback(MX_BIND_CALLBACK(&OnLog));

  cWebSocket.Attach(MX_DEBUG_NEW CTestHttpClientWebSocket());
  if (!cWebSocket)
    return E_OUTOFMEMORY;

  ::MxMemSet(&sOptions, 0, sizeof(sOptions));
  sOptions.sWebSocket.lpSocket = cWebSocket.Get();
  sOptions.sWebSocket.nVersion = 13;
  sOptions.sWebSocket.lpszProtocolsA = NULL;

  hRes = cHttpClient->AddRequestHeader("Origin", "http://www.websocket.org");
  //hRes = cHttpClient->Open("ws://demos.kaazing.com/echo", &sOptions);
  if (SUCCEEDED(hRes))
  {
    hRes = cHttpClient->Open("ws://echo.websocket.org/", &sOptions);
  }
  if (SUCCEEDED(hRes))
  {
    while (ShouldAbort() == FALSE && cHttpClient->IsDocumentComplete() == FALSE && cHttpClient->IsClosed() == FALSE)
    {
      ::Sleep(50);
    }
    if (ShouldAbort() == FALSE)
    {
      if (cHttpClient->GetResponseStatus() == 101)
      {
        while (ShouldAbort() == FALSE && cWebSocket->IsClosed() == FALSE)
        {
          ::Sleep(50);
        }
      }
      else
      {
        hRes = cHttpClient->GetLastRequestError();
        wprintf_s(L"[HttpClient/WebSocketTest] Error 0x%08X / Status:%ld\n", hRes, cHttpClient->GetResponseStatus());
      }
    }
    else
    {
      hRes = MX_E_Cancelled;
      wprintf_s(L"[HttpClient/WebSocketTest] Cancelled by user.\n");
    }
  }
  return hRes;
}

//-----------------------------------------------------------

static VOID HttpClientJob(_In_ MX::CWorkerThread *lpWrkThread, _In_ LPVOID lpParam)
{
  THREAD_DATA *lpThreadData = (THREAD_DATA*)lpParam;
  MX::TAutoRefCounted<CMyHttpClient> cHttpClient;
  MX::CProxy cProxy;
  DWORD dwStartTime, dwEndTime;
  int nOrigPosX, nOrigPosY;
  BOOL bExpectHtml;
  HRESULT hRes;

  cHttpClient.Attach(MX_DEBUG_NEW CMyHttpClient(*(lpThreadData->lpSocketMgr)));
  if (!cHttpClient)
    return;
  cProxy.SetUseIE();
  cHttpClient->SetProxy(cProxy);
  cHttpClient->lpCertificates = lpThreadData->lpCertificates;
  cHttpClient->SetOption_MaxBodySize(1024 * 1048576);
  cHttpClient->SetOption_MaxBodySizeInMemory(1048576);
  //cHttpClient->SetOptionFlags(0);
  //cHttpClient->SetOptionFlags(MX::CHttpClient::OptionKeepConnectionOpen);
  cHttpClient->SetDocumentCompletedCallback(MX_BIND_CALLBACK(&OnDocumentCompleted));
  cHttpClient->SetErrorCallback(MX_BIND_CALLBACK(&OnError));
  cHttpClient->SetQueryCertificatesCallback(MX_BIND_CALLBACK(&OnQueryCertificates));
  cHttpClient->SetLogCallback(MX_BIND_CALLBACK(&OnLog));
  cHttpClient->SetLogLevel(lpThreadData->dwLogLevel);
  {
    Console::CPrintLock cPrintLock;

    wprintf_s(L"[HttpClient/%lu] Downloading...\n", lpThreadData->nIndex);
    Console::GetCursorPosition(&nOrigPosX, &nOrigPosY);
  }
#pragma warning(suppress : 28159)
  dwStartTime = dwEndTime = ::GetTickCount();

  hRes = E_FAIL;

  if (lpThreadData->nIndex == 1 && DoesCmdLineParamExist(L"bigdownload") != FALSE)
  {
    bExpectHtml = FALSE;
    cHttpClient->SetHeadersReceivedCallback(MX_BIND_CALLBACK(&OnResponseHeadersReceived_BigDownload));
    hRes = cHttpClient->Open("http://ipv4.download.thinkbroadband.com/512MB.zip");
  }
  else
  {
    bExpectHtml = TRUE;
    cHttpClient->SetHeadersReceivedCallback(MX_BIND_CALLBACK(&OnResponseHeadersReceived));
    switch (lpThreadData->nIndex % 3)
    {
      case 0:
        hRes = cHttpClient->Open("https://en.wikipedia.org/wiki/HTTPS");
        break;
      case 1:
        hRes = cHttpClient->Open("https://www.google.com");
        break;
      case 2:
        hRes = cHttpClient->Open("http://www.sitepoint.com/forums/showthread.php?"
                                 "390414-Reading-from-socket-connection-SLOW");
        break;
    }
  }

  if (SUCCEEDED(hRes))
  {
    while (lpThreadData->cWorkerThreads.CheckForAbort(10) == FALSE && cHttpClient->IsDocumentComplete() == FALSE &&
           cHttpClient->IsClosed() == FALSE);
#pragma warning(suppress : 28159)
    dwEndTime = ::GetTickCount();
    if (lpThreadData->cWorkerThreads.CheckForAbort(0) == FALSE)
    {
      hRes = CheckHttpClientResponse(cHttpClient.Get(), bExpectHtml);

      if (lpThreadData->nIndex == 1 && DoesCmdLineParamExist(L"bigdownload") != FALSE)
      {
        MX::TAutoRefCounted<MX::CHttpBodyParserBase> cBodyParser;

        cBodyParser.Attach(cHttpClient->GetResponseBodyParser());
      }
    }
    else
    {
      hRes = MX_E_Cancelled;
    }
  }

  //print results
  {
    Console::CPrintLock cPrintLock;
    int nCurPosX, nCurPosY;

    Console::GetCursorPosition(&nCurPosX, &nCurPosY);
    Console::SetCursorPosition(0, nOrigPosY-1);
    switch (hRes)
    {
      case 0x80070000 | ERROR_CANCELLED:
        wprintf_s(L"[HttpClient/%lu] Cancelled by user.\n", lpThreadData->nIndex);
        break;
      case S_FALSE:
        wprintf_s(L"[HttpClient/%lu] Body is NOT complete.\n", lpThreadData->nIndex);
        break;
      case S_OK:
        wprintf_s(L"[HttpClient/%lu] Successful download in %lums.\n", lpThreadData->nIndex, dwEndTime-dwStartTime);
        break;
      default:
        wprintf_s(L"[HttpClient/%lu] Error 0x%08X / Status:%ld\n", lpThreadData->nIndex, hRes,
                  cHttpClient->GetResponseStatus());
        break;
    }
    Console::SetCursorPosition(nCurPosX, nCurPosY);
  }
  return;
}

static HRESULT CheckHttpClientResponse(_In_ CMyHttpClient *lpHttpClient, _In_ BOOL bExpectHtml)
{
  MX::CHttpBodyParserBase *lpBodyParser;
  HRESULT hRes;

  if (lpHttpClient->GetResponseStatus() != 200)
    return lpHttpClient->GetLastRequestError();

  lpBodyParser = lpHttpClient->GetResponseBodyParser();
  if (lpBodyParser == NULL)
    return S_FALSE;

  hRes = S_FALSE;
  if (MX::StrCompareA(lpBodyParser->GetType(), "default") == 0)
  {
    MX::CHttpBodyParserDefault *lpParser = (MX::CHttpBodyParserDefault*)lpBodyParser;
    MX::CStringA cStrBodyA;

    if (bExpectHtml != FALSE)
    {
      hRes = lpParser->ToString(cStrBodyA);
      if (SUCCEEDED(hRes))
      {
        if (MX::StrFindA((LPCSTR)cStrBodyA, "<html", FALSE, TRUE) == NULL ||
            MX::StrFindA((LPCSTR)cStrBodyA, "</html>", TRUE, TRUE) == NULL)
        {
          hRes = S_FALSE;
        }
      }
    }
    else
    {
      lpParser->KeepFile();
      hRes = S_OK;
    }
  }
  lpBodyParser->Release();
  return hRes;
}

static HRESULT OnLog(_In_z_ LPCWSTR szInfoW)
{
  MX::CFastLock cLock(&nLogMutex);

  wprintf_s(L"%s\n", szInfoW);
  return S_OK;
}

static BOOL _GetTempPath(_Out_ MX::CStringW &cStrPathW)
{
  SIZE_T nLen, nThisLen = 0;

  for (nLen = 2048; nLen <= 65536; nLen <<= 1)
  {
    if (cStrPathW.EnsureBuffer(nLen) == FALSE)
      return FALSE;
    nThisLen = (SIZE_T)::GetTempPathW((DWORD)nLen, (LPWSTR)cStrPathW);
    if (nThisLen < nLen - 4)
      break;
  }
  if (nLen > 65536)
    return FALSE;
  ((LPWSTR)cStrPathW)[nThisLen] = 0;
  cStrPathW.Refresh();
  if (nThisLen > 0 && ((LPWSTR)cStrPathW)[nThisLen - 1] != L'\\')
  {
    if (cStrPathW.Concat(L"\\") == FALSE)
      return FALSE;
  }
  return TRUE;
}
//-----------------------------------------------------------

CTestHttpClientWebSocket::CTestHttpClientWebSocket() : MX::CWebSocket()
{
  return;
}

CTestHttpClientWebSocket::~CTestHttpClientWebSocket()
{
  return;
}

HRESULT CTestHttpClientWebSocket::OnConnected()
{
  static const LPCSTR szWelcomeMsgA = "Welcome to the MX HttpClient WebSocket demo!!!";
  HRESULT hRes;

  {
    Console::CPrintLock cPrintLock;

    wprintf_s(L"[HttpClientWS] WebSocket is now connected.\n");
  }

  hRes = BeginTextMessage();
  if (SUCCEEDED(hRes))
    hRes = SendTextMessage(szWelcomeMsgA);
  if (SUCCEEDED(hRes))
    hRes = EndMessage();
  //done
  return hRes;
}

HRESULT CTestHttpClientWebSocket::OnTextMessage(_In_ LPCSTR szMsgA, _In_ SIZE_T nMsgLength)
{
  {
    Console::CPrintLock cPrintLock;

    wprintf_s(L"[HttpClientWS] Received text frame [%.*S].\n", (int)nMsgLength, szMsgA);
  }

  //done
  return S_OK;
}

HRESULT CTestHttpClientWebSocket::OnBinaryMessage(_In_ LPVOID lpData, _In_ SIZE_T nDataSize)
{
  HRESULT hRes;

  hRes = BeginBinaryMessage();
  if (SUCCEEDED(hRes))
    hRes = SendBinaryMessage(lpData, nDataSize);
  if (SUCCEEDED(hRes))
    hRes = EndMessage();
  //done
  return S_OK;
}

SIZE_T CTestHttpClientWebSocket::GetMaxMessageSize() const
{
  return 1048596;
}

VOID CTestHttpClientWebSocket::OnPongFrame()
{
  Console::CPrintLock cPrintLock;

  wprintf_s(L"[HttpClientWS] Received pong frame.\n");
  return;
}

VOID CTestHttpClientWebSocket::OnCloseFrame(_In_ USHORT wCode, _In_  HRESULT hrErrorCode)
{
  Console::CPrintLock cPrintLock;

  wprintf_s(L"[HttpClientWS] Received close frame. Code: %u / Error: 0x%08X\n", wCode, hrErrorCode);
  return;
}
