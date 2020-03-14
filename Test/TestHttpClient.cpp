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

//#define SIMPLE_TEST

//-----------------------------------------------------------

class CMyHttpClient : public MX::CHttpClient
{
public:
  CMyHttpClient(_In_ MX::CSockets &cSocketMgr) : MX::CHttpClient(cSocketMgr)
    {
    return;
    };

public:
  MX::CSslCertificateArray *lpCerts;
};

//-----------------------------------------------------------

static LONG volatile nLogMutex = 0;

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
static HRESULT OnDocumentCompleted(_In_ MX::CHttpClient *lpHttp);
static VOID OnError(_In_ MX::CHttpClient *lpHttp, _In_ HRESULT hrErrorCode);
static HRESULT OnQueryCertificates(_In_ MX::CHttpClient *lpHttp, _Inout_ MX::CSslCertificateArray **lplpCertificates,
                                   _Inout_ MX::CSslCertificate **lplpSelfCert,
                                   _Inout_ MX::CEncryptionKey **lplpPrivKey);

#ifdef SIMPLE_TEST
static HRESULT SimpleTest1(_In_ MX::CSockets *lpSckMgr, _In_ MX::CSslCertificateArray *lpCerts, _In_ DWORD dwLogLevel);
#else //SIMPLE_TEST
static VOID HttpClientJob(_In_ MX::CWorkerThread *lpWrkThread, _In_ LPVOID lpParam);
#endif //SIMPLE_TEST

static HRESULT CheckHttpClientResponse(_In_ CMyHttpClient &cHttpClient, _In_ BOOL bExpectHtml);

static HRESULT OnLog(_In_z_ LPCWSTR szInfoW);

//-----------------------------------------------------------

#ifndef SIMPLE_TEST
typedef struct {
  int nIndex;
  MX::CSockets *lpSckMgr;
  MX::CSslCertificateArray *lpCerts;
  MX::CWorkerThread cWorkerThreads;
  DWORD dwLogLevel;
} THREAD_DATA;
#endif //!SIMPLE_TEST

//-----------------------------------------------------------

int TestHttpClient(_In_ DWORD dwLogLevel)
{
  MX::CIoCompletionPortThreadPool cDispatcherPool;
  MX::CSockets cSckMgr(cDispatcherPool);
  MX::CSslCertificateArray cCerts;
  HRESULT hRes;

  hRes = cDispatcherPool.Initialize();
  if (SUCCEEDED(hRes))
  {
    cSckMgr.SetEngineErrorCallback(MX_BIND_CALLBACK(&OnEngineError));
    hRes = cSckMgr.Initialize();
  }
  if (SUCCEEDED(hRes))
  {
    cSckMgr.SetLogCallback(MX_BIND_CALLBACK(&OnLog));
    cSckMgr.SetLogLevel(dwLogLevel);

    hRes = cCerts.ImportFromWindowsStore();
  }

#ifdef SIMPLE_TEST
  if (SUCCEEDED(hRes))
  {
    hRes = SimpleTest1(&cSckMgr, &cCerts, dwLogLevel);
  }
#else //SIMPLE_TEST
  if (SUCCEEDED(hRes))
  {
    THREAD_DATA sThreadData[20];
    BOOL bActive;
    SIZE_T i;

    for (i = 0; SUCCEEDED(hRes) && i < MX_ARRAYLEN(sThreadData); i++)
    {
      sThreadData[i].nIndex = (int)i + 1;
      sThreadData[i].lpSckMgr = &cSckMgr;
      sThreadData[i].lpCerts = &cCerts;
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
#endif //SIMPLE_TEST

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
  HRESULT hRes;

  hRes = MX::CHttpCommon::_GetTempPath(cStrFullFileNameW);
  if (FAILED(hRes))
    return hRes;
  if (cStrFullFileNameW.Concat(szFileNameW) == FALSE)
    return E_OUTOFMEMORY;
  return S_OK;
}

static HRESULT OnDocumentCompleted(_In_ MX::CHttpClient *lpHttp)
{
  return S_OK;
}

static VOID OnError(_In_ MX::CHttpClient *lpHttp, _In_ HRESULT hrErrorCode)
{
  return;
}

static HRESULT OnQueryCertificates(_In_ MX::CHttpClient *_lpHttp, _Inout_ MX::CSslCertificateArray **lplpCertificates,
                                   _Inout_ MX::CSslCertificate **lplpSelfCert, _Inout_ MX::CEncryptionKey **lplpPrivKey)
{
  CMyHttpClient *lpHttp = static_cast<CMyHttpClient*>(_lpHttp);

  *lplpCertificates = lpHttp->lpCerts;
  return S_OK;
}

#ifdef SIMPLE_TEST
static HRESULT SimpleTest1(_In_ MX::CSockets *lpSckMgr, _In_ MX::CSslCertificateArray *lpCerts, _In_ DWORD dwLogLevel)
{
  CMyHttpClient cHttpClient(*lpSckMgr);
  MX::CProxy cProxy;
  DWORD dwStartTime, dwEndTime;
  HRESULT hRes;

  cProxy.SetUseIE();
  //cProxy.SetManual(L"127.0.0.1:808");
  //cProxy.SetCredentials(L"guest", L"invitado");
  //cHttpClient.SetProxy(cProxy);
  cHttpClient.lpCerts = lpCerts;
  //cHttpClient.SetOptionFlags(0);
  //cHttpClient.SetOptionFlags(MX::CHttpClient::OptionKeepConnectionOpen);

  cHttpClient.SetOption_ResponseHeaderTimeout(INFINITE);

  cHttpClient.SetDocumentCompletedCallback(MX_BIND_CALLBACK(&OnDocumentCompleted));
  cHttpClient.SetErrorCallback(MX_BIND_CALLBACK(&OnError));
  cHttpClient.SetQueryCertificatesCallback(MX_BIND_CALLBACK(&OnQueryCertificates));
  cHttpClient.SetLogLevel(dwLogLevel);
  cHttpClient.SetLogCallback(MX_BIND_CALLBACK(&OnLog));

  wprintf_s(L"[HttpClient/SimpleTest1] Downloading...\n");
  dwStartTime = dwEndTime = ::GetTickCount();

  //cHttpClient.SetHeadersReceivedCallback(MX_BIND_CALLBACK(&OnResponseHeadersReceived));
  cHttpClient.SetHeadersReceivedCallback(MX_BIND_CALLBACK(&OnResponseHeadersReceived_BigDownload));

  hRes = cHttpClient.Open("http://www.sitepoint.com/forums/showthread.php?"
                          "390414-Reading-from-socket-connection-SLOW");
  /*
  //hRes = cHttpClient.SetAuthCredentials(L"guest", L"guest");
  hRes = S_OK;
  if (SUCCEEDED(hRes))
  {
    cHttpClient.SetOption_MaxBodySizeInMemory(0);

    hRes = cHttpClient.AddRequestHeader("x-version", L"3");
    if (SUCCEEDED(hRes))
      hRes = cHttpClient.AddRequestHeader("license", L"39F2E487ED3D3489C49756833E5F7C7D1CEF7482FE9EF46B2549A0968DE83C99");
    if (SUCCEEDED(hRes))
      hRes = cHttpClient.Open("https://airgap.trapmine.com/ml_model_check");
    //hRes = cHttpClient.Open("https://jigsaw.w3.org/HTTP/Basic/");
    //hRes = cHttpClient.Open("https://jigsaw.w3.org/HTTP/Digest/");
  }
  */

  if (SUCCEEDED(hRes))
  {
    while (cHttpClient.IsDocumentComplete() == FALSE && cHttpClient.IsClosed() == FALSE)
      ::Sleep(50);
#pragma warning(suppress : 28159)
    dwEndTime = ::GetTickCount();
    hRes = CheckHttpClientResponse(cHttpClient, TRUE);

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
                  cHttpClient.GetResponseStatus());
        break;
      default:
        wprintf_s(L"[HttpClient/SimpleTest1] Error 0x%08X / Status:%ld\n", hRes,
                  cHttpClient.GetResponseStatus());
        break;
    }
  }
  return hRes;
}

#else //SIMPLE_TEST
static VOID HttpClientJob(_In_ MX::CWorkerThread *lpWrkThread, _In_ LPVOID lpParam)
{
  THREAD_DATA *lpThreadData = (THREAD_DATA*)lpParam;
  CMyHttpClient cHttpClient(*(lpThreadData->lpSckMgr));
  MX::CProxy cProxy;
  DWORD dwStartTime, dwEndTime;
  int nOrigPosX, nOrigPosY;
  BOOL bExpectHtml;
  HRESULT hRes;

  cProxy.SetUseIE();
  cHttpClient.SetProxy(cProxy);
  cHttpClient.lpCerts = lpThreadData->lpCerts;
  //cHttpClient.SetOptionFlags(0);
  //cHttpClient.SetOptionFlags(MX::CHttpClient::OptionKeepConnectionOpen);
  cHttpClient.SetDocumentCompletedCallback(MX_BIND_CALLBACK(&OnDocumentCompleted));
  cHttpClient.SetErrorCallback(MX_BIND_CALLBACK(&OnError));
  cHttpClient.SetQueryCertificatesCallback(MX_BIND_CALLBACK(&OnQueryCertificates));
  cHttpClient.SetLogCallback(MX_BIND_CALLBACK(&OnLog));
  cHttpClient.SetLogLevel(lpThreadData->dwLogLevel);
  {
    Console::CPrintLock cPrintLock;

    wprintf_s(L"[HttpClient/%lu] Downloading...\n", lpThreadData->nIndex);
    Console::GetCursorPosition(&nOrigPosX, &nOrigPosY);
  }
#pragma warning(suppress : 28159)
  dwStartTime = dwEndTime = ::GetTickCount();

  hRes = E_FAIL;
  if (lpThreadData->nIndex == 1)
  {
    bExpectHtml = FALSE;
    cHttpClient.SetHeadersReceivedCallback(MX_BIND_CALLBACK(&OnResponseHeadersReceived_BigDownload));
    hRes = cHttpClient.Open("http://ipv4.download.thinkbroadband.com/512MB.zip");
  }
  else
  {
    bExpectHtml = TRUE;
    cHttpClient.SetHeadersReceivedCallback(MX_BIND_CALLBACK(&OnResponseHeadersReceived));
    switch (lpThreadData->nIndex % 3)
    {
      case 0:
        hRes = cHttpClient.Open("https://en.wikipedia.org/wiki/HTTPS");
        break;
      case 1:
        hRes = cHttpClient.Open("https://www.google.com");
        break;
      case 2:
        hRes = cHttpClient.Open("http://www.sitepoint.com/forums/showthread.php?"
                                "390414-Reading-from-socket-connection-SLOW");
        break;
    }
  }

  if (SUCCEEDED(hRes))
  {
    while (lpThreadData->cWorkerThreads.CheckForAbort(10) == FALSE && cHttpClient.IsDocumentComplete() == FALSE &&
           cHttpClient.IsClosed() == FALSE);
#pragma warning(suppress : 28159)
    dwEndTime = ::GetTickCount();
    if (lpThreadData->cWorkerThreads.CheckForAbort(0) == FALSE)
    {
      hRes = CheckHttpClientResponse(cHttpClient, bExpectHtml);
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
                  cHttpClient.GetResponseStatus());
        break;
    }
    Console::SetCursorPosition(nCurPosX, nCurPosY);
  }
  return;
}
#endif //SIMPLE_TEST

static HRESULT CheckHttpClientResponse(_In_ CMyHttpClient &cHttpClient, _In_ BOOL bExpectHtml)
{
  MX::CHttpBodyParserBase *lpBodyParser;
  HRESULT hRes;

  if (cHttpClient.GetResponseStatus() != 200)
    return cHttpClient.GetLastRequestError();

  lpBodyParser = cHttpClient.GetResponseBodyParser();
  if (lpBodyParser == NULL)
    return S_FALSE;

  hRes = S_FALSE;
  if (MX::StrCompareA(lpBodyParser->GetType(), "default") == 0)
  {
    MX::CHttpBodyParserDefault *lpParser = (MX::CHttpBodyParserDefault*)lpBodyParser;
    MX::CStringA cStrBodyA;

    hRes = lpParser->ToString(cStrBodyA);
    if (SUCCEEDED(hRes) && bExpectHtml != FALSE)
    {
      if (MX::StrFindA((LPCSTR)cStrBodyA, "<html", FALSE, TRUE) == NULL ||
          MX::StrFindA((LPCSTR)cStrBodyA, "</html>", TRUE, TRUE) == NULL)
      {
        hRes = S_FALSE;
      }
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
