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
#include "TestHttpClient.h"
#include <Http\HttpClient.h>

//-----------------------------------------------------------

class CMyHttpClient : public MX::CHttpClient
{
public:
  CMyHttpClient(__in MX::CSockets &cSocketMgr, __in MX::CPropertyBag &cPropBag) : MX::CHttpClient(cSocketMgr, cPropBag)
    {
    return;
    };

public:
  MX::CSslCertificateArray *lpCerts;
};

//-----------------------------------------------------------

static VOID OnEngineError(__in MX::CIpc *lpIpc, __in HRESULT hErrorCode);
static HRESULT OnResponseHeadersReceived(__in MX::CHttpClient *lpHttp, __inout MX::CHttpBodyParserBase *&lpBodyParser);
static HRESULT OnDocumentCompleted(__in MX::CHttpClient *lpHttp);
static VOID OnError(__in MX::CHttpClient *lpHttp, __in HRESULT hErrorCode);
static HRESULT OnQueryCertificates(__in MX::CHttpClient *lpHttp, __inout MX::CIpcSslLayer::eProtocol &nProtocol,
                                   __inout MX::CSslCertificateArray *&lpCheckCertificates,
                                   __inout MX::CSslCertificate *&lpSelfCert, __inout MX::CCryptoRSA *&lpPrivKey);

static VOID HttpClientJob(__in MX::CWorkerThread *lpWrkThread, __in LPVOID lpParam);

//-----------------------------------------------------------

typedef struct {
  int nIndex;
  MX::CPropertyBag *lpPropBag;
  MX::CSockets *lpSckMgr;
  MX::CSslCertificateArray *lpCerts;
  MX::CWorkerThread cWorkerThreads;
} THREAD_DATA;

//-----------------------------------------------------------

int TestHttpClient()
{
  MX::CIoCompletionPortThreadPool cDispatcherPool;
  MX::CPropertyBag cPropBag;
  MX::CSockets cSckMgr(cDispatcherPool, cPropBag);
  MX::CSslCertificateArray cCerts;
  THREAD_DATA sThreadData[20];
  BOOL bActive;
  SIZE_T i;
  HRESULT hRes;

  hRes = cDispatcherPool.Initialize();
  if (SUCCEEDED(hRes))
  {
    cSckMgr.On(MX_BIND_CALLBACK(&OnEngineError));
    hRes = cSckMgr.Initialize();
  }
  if (SUCCEEDED(hRes))
    hRes = cCerts.ImportFromWindowsStore();
  if (SUCCEEDED(hRes))
  {
    for (i=0; SUCCEEDED(hRes) && i<MX_ARRAYLEN(sThreadData); i++)
    {
      sThreadData[i].nIndex = (int)i + 1;
      sThreadData[i].lpPropBag = &cPropBag;
      sThreadData[i].lpSckMgr = &cSckMgr;
      sThreadData[i].lpCerts = &cCerts;
      if (sThreadData[i].cWorkerThreads.SetRoutine(&HttpClientJob, &sThreadData[i]) == FALSE ||
          sThreadData[i].cWorkerThreads.Start() == FALSE)
        hRes = E_OUTOFMEMORY;
    }
  }
  //----
  if (SUCCEEDED(hRes))
  {
    do
    {
      bActive = FALSE;
      for (i=0; i<MX_ARRAYLEN(sThreadData); i++)
      {
        if (sThreadData[i].cWorkerThreads.Wait(10) == FALSE)
          bActive = TRUE;
      }
    }
    while (bActive != FALSE && ShouldAbort() == FALSE);
  }

  for (i=0; i<MX_ARRAYLEN(sThreadData); i++)
    sThreadData[i].cWorkerThreads.Stop();

  //done
  return (int)hRes;
}

static VOID OnEngineError(__in MX::CIpc *lpIpc, __in HRESULT hErrorCode)
{
  return;
}

static HRESULT OnResponseHeadersReceived(__in MX::CHttpClient *lpHttp, __inout MX::CHttpBodyParserBase *&lpBodyParser)
{
  return S_OK;
}

static HRESULT OnDocumentCompleted(__in MX::CHttpClient *lpHttp)
{
  return S_OK;
}

static VOID OnError(__in MX::CHttpClient *lpHttp, __in HRESULT hErrorCode)
{
  return;
}

static HRESULT OnQueryCertificates(__in MX::CHttpClient *_lpHttp, __inout MX::CIpcSslLayer::eProtocol &nProtocol,
                                   __inout MX::CSslCertificateArray *&lpCheckCertificates,
                                   __inout MX::CSslCertificate *&lpSelfCert, __inout MX::CCryptoRSA *&lpPrivKey)
{
  CMyHttpClient *lpHttp = static_cast<CMyHttpClient*>(_lpHttp);

  nProtocol = MX::CIpcSslLayer::ProtocolTLSv1_2;
  lpCheckCertificates = lpHttp->lpCerts;
  return S_OK;
}

static VOID HttpClientJob(__in MX::CWorkerThread *lpWrkThread, __in LPVOID lpParam)
{
  THREAD_DATA *lpThreadData = (THREAD_DATA*)lpParam;
  CMyHttpClient cHttpClient(*(lpThreadData->lpSckMgr), *(lpThreadData->lpPropBag));
  DWORD dwStartTime, dwEndTime;
  int nOrigPosX, nOrigPosY;
  HRESULT hRes;

  cHttpClient.lpCerts = lpThreadData->lpCerts;
  //cHttpClient.SetOptionFlags(0);
  //cHttpClient.SetOptionFlags(MX::CHttpClient::OptionKeepConnectionOpen);
  cHttpClient.On(MX_BIND_CALLBACK(&OnResponseHeadersReceived));
  cHttpClient.On(MX_BIND_CALLBACK(&OnDocumentCompleted));
  cHttpClient.On(MX_BIND_CALLBACK(&OnError));
  cHttpClient.On(MX_BIND_CALLBACK(&OnQueryCertificates));
  {
    Console::CPrintLock cPrintLock;

    wprintf_s(L"[HttpClient/%lu] Downloading...\n", lpThreadData->nIndex);
    Console::GetCursorPosition(&nOrigPosX, &nOrigPosY);
  }
  dwStartTime = dwEndTime = ::GetTickCount();
  hRes = cHttpClient.Open("https://en.wikipedia.org/wiki/HTTPS");
  //hRes = cHttpClient.Open("https://www.google.com");
  //hRes = cHttpClient.Open("http://www.sitepoint.com/forums/showthread.php?390414-Reading-from-socket-connection-SLOW");
  if (SUCCEEDED(hRes))
  {
    while (lpThreadData->cWorkerThreads.CheckForAbort(100) == FALSE && cHttpClient.IsDocumentComplete() == FALSE &&
           cHttpClient.IsClosed() == FALSE);
    dwEndTime = ::GetTickCount();
    if (lpThreadData->cWorkerThreads.CheckForAbort(0) == FALSE)
    {
      MX::CHttpBodyParserBase *lpBodyParser;
      MX::CStringA cStrBodyA;

      lpBodyParser = cHttpClient.GetResponseBodyParser();
      if (lpBodyParser != NULL)
      {
        if (strcmp(lpBodyParser->GetType(), "default") == 0)
        {
          MX::CHttpBodyParserDefault *lpParser = (MX::CHttpBodyParserDefault*)lpBodyParser;

          hRes = lpParser->ToString(cStrBodyA);
          if (SUCCEEDED(hRes))
          {
            if (strstr((LPSTR)cStrBodyA, "<html") == NULL ||
                strstr((LPSTR)cStrBodyA, "</html>") == NULL)
              hRes = S_FALSE;
          }
        }
        lpBodyParser->Release();
      }
      else
      {
        hRes = S_FALSE;
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
      case 0x80070000|ERROR_CANCELLED:
        wprintf_s(L"[HttpClient/%lu] Cancelled by user.\n", lpThreadData->nIndex);
        break;
      case S_FALSE:
        wprintf_s(L"[HttpClient/%lu] Body is NOT complete.\n", lpThreadData->nIndex);
        break;
      case S_OK:
        wprintf_s(L"[HttpClient/%lu] Successful download in %lums.\n", lpThreadData->nIndex, dwEndTime-dwStartTime);
        break;
      default:
        wprintf_s(L"[HttpClient/%lu] Error 0x%08X\n", lpThreadData->nIndex, hRes);
        break;
    }
    Console::SetCursorPosition(nCurPosX, nCurPosY);
  }
  return;
}
