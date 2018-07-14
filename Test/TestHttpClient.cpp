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
  CMyHttpClient(_In_ MX::CSockets &cSocketMgr) : MX::CHttpClient(cSocketMgr)
    {
    return;
    };

public:
  MX::CSslCertificateArray *lpCerts;
};

//-----------------------------------------------------------

static VOID OnEngineError(_In_ MX::CIpc *lpIpc, _In_ HRESULT hErrorCode);
static HRESULT OnResponseHeadersReceived(_In_ MX::CHttpClient *lpHttp, _In_z_ LPCWSTR szFileNameW,
                                         _In_opt_ PULONGLONG lpnContentSize, _In_z_ LPCSTR szTypeA,
                                         _In_ BOOL bTreatAsAttachment, _Inout_ MX::CStringW &cStrFullFileNameW,
                                         _Inout_ MX::CHttpBodyParserBase **lplpBodyParser);
static HRESULT OnResponseHeadersReceived_BigDownload(_In_ MX::CHttpClient *lpHttp, _In_z_ LPCWSTR szFileNameW,
                                         _In_opt_ PULONGLONG lpnContentSize, _In_z_ LPCSTR szTypeA,
                                          _In_ BOOL bTreatAsAttachment, _Inout_ MX::CStringW &cStrFullFileNameW,
                                         _Inout_ MX::CHttpBodyParserBase **lplpBodyParser);
static HRESULT OnDocumentCompleted(_In_ MX::CHttpClient *lpHttp);
static VOID OnError(_In_ MX::CHttpClient *lpHttp, _In_ HRESULT hErrorCode);
static HRESULT OnQueryCertificates(_In_ MX::CHttpClient *lpHttp, _Inout_ MX::CIpcSslLayer::eProtocol &nProtocol,
                                   _Inout_ MX::CSslCertificateArray **lplpCheckCertificates,
                                   _Inout_ MX::CSslCertificate **lplpSelfCert, _Inout_ MX::CCryptoRSA **lplpPrivKey);

static VOID HttpClientJob(_In_ MX::CWorkerThread *lpWrkThread, _In_ LPVOID lpParam);

//-----------------------------------------------------------

typedef struct {
  int nIndex;
  MX::CSockets *lpSckMgr;
  MX::CSslCertificateArray *lpCerts;
  MX::CWorkerThread cWorkerThreads;
} THREAD_DATA;

//-----------------------------------------------------------

int TestHttpClient()
{
  MX::CIoCompletionPortThreadPool cDispatcherPool;
  MX::CSockets cSckMgr(cDispatcherPool);
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
    for (i=1; SUCCEEDED(hRes) && i<MX_ARRAYLEN(sThreadData); i++)
    {
      sThreadData[i].nIndex = (int)i + 1;
      sThreadData[i].lpSckMgr = &cSckMgr;
      sThreadData[i].lpCerts = &cCerts;
      if (sThreadData[i].cWorkerThreads.SetRoutine(&HttpClientJob, &sThreadData[i]) == FALSE ||
          sThreadData[i].cWorkerThreads.Start() == FALSE)
      {
        hRes = E_OUTOFMEMORY;
      }
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

static VOID OnEngineError(_In_ MX::CIpc *lpIpc, _In_ HRESULT hErrorCode)
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

static VOID OnError(_In_ MX::CHttpClient *lpHttp, _In_ HRESULT hErrorCode)
{
  return;
}

static HRESULT OnQueryCertificates(_In_ MX::CHttpClient *_lpHttp, _Inout_ MX::CIpcSslLayer::eProtocol &nProtocol,
                                   _Inout_ MX::CSslCertificateArray **lplpCheckCertificates,
                                   _Inout_ MX::CSslCertificate **lplpSelfCert, _Inout_ MX::CCryptoRSA **lplpPrivKey)
{
  CMyHttpClient *lpHttp = static_cast<CMyHttpClient*>(_lpHttp);

  nProtocol = MX::CIpcSslLayer::ProtocolTLSv1_2;
  *lplpCheckCertificates = lpHttp->lpCerts;
  return S_OK;
}

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
  cHttpClient.On(MX_BIND_CALLBACK(&OnDocumentCompleted));
  cHttpClient.On(MX_BIND_CALLBACK(&OnError));
  cHttpClient.On(MX_BIND_CALLBACK(&OnQueryCertificates));
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
    cHttpClient.On(MX_BIND_CALLBACK(&OnResponseHeadersReceived_BigDownload));
    hRes = cHttpClient.Open("http://ipv4.download.thinkbroadband.com/512MB.zip");
  }
  else
  {
    bExpectHtml = TRUE;
    cHttpClient.On(MX_BIND_CALLBACK(&OnResponseHeadersReceived));
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
      if (cHttpClient.GetResponseStatus() == 200)
      {
        MX::CHttpBodyParserBase *lpBodyParser;

        lpBodyParser = cHttpClient.GetResponseBodyParser();
        if (lpBodyParser != NULL)
        {
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
        }
        else
        {
          hRes = S_FALSE;
        }
      }
      else
      {
        hRes = cHttpClient.GetLastRequestError();;
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
                  cHttpClient.GetResponseStatus());
        break;
    }
    Console::SetCursorPosition(nCurPosX, nCurPosY);
  }
  return;
}
