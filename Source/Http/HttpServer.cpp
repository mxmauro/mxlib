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
#include "HttpServerCommon.h"
#include "..\..\Include\Http\WebSockets.h"

//-----------------------------------------------------------

static const LPCSTR szServerInfoA = "MX-Library";

static const LPCSTR szServerErrorFormatA = "<html><head><title>Server Error: %03d %s</title></head>"
                                           "<body style=\"text-align:left; font-family:Arial,Helvetica; font-size:10pt;"
                                           " padding:5px\">Server error: <b>%03d %s</b><hr />%s</body></html>";

static const LPCSTR szServerErrorMessages100[] ={
  /*100*/ "*Continue",
  /*101*/ "*Switching Protocols",
  /*102*/ "*Processing"
};

static const LPCSTR szServerErrorMessages200[] ={
  /*200*/ "OK",
  /*201*/ "Created",
  /*202*/ "Accepted",
  /*203*/ "Non-Authoritative Information",
  /*204*/ "*No Content",
  /*205*/ "Reset Content",
  /*206*/ "Partial Content",
  /*207*/ "Multi-Status",
  /*208*/ "Already Reported"
};

static const LPCSTR szServerErrorMessages301[] = {
  /*301*/ "Moved Permanently",
  /*302*/ "Found",
  /*303*/ "See Other",
  /*304*/ "*Not Modified",
  /*305*/ "Use Proxy",
  /*306*/ "Switch Proxy",
  /*307*/ "Temporary Redirect"
};

static const LPCSTR szServerErrorMessages400[] = {
  /*400*/ "Bad Request",
  /*401*/ "Authorization Required",
  /*402*/ "Payment Required",
  /*403*/ "Forbidden",
  /*404*/ "Not Found",
  /*405*/ "Not Allowed",
  /*406*/ "Not Acceptable",
  /*407*/ "Proxy Authentication Required",
  /*408*/ "Request Time-out",
  /*409*/ "Conflict",
  /*410*/ "Gone",
  /*411*/ "Length Required",
  /*412*/ "Precondition Failed",
  /*413*/ "Request Entity Too Large",
  /*414*/ "Request-URI Too Large",
  /*415*/ "Unsupported Media Type",
  /*416*/ "Requested Range Not Satisfiable"
};

static const LPCSTR szServerErrorMessages431[] = {
  /*431*/ "Request Header Fields Too Large"
};

static const LPCSTR szServerErrorMessages500[] = {
  /*500*/ "Internal Server Error",
  /*501*/ "Not Implemented",
  /*502*/ "Bad Gateway",
  /*503*/ "Service Temporarily Unavailable",
  /*504*/ "Gateway Time-out",
  /*505*/ "HTTP Version Not Supported",
  /*506*/ "Variant Also Negotiates",
  /*507*/ "Insufficient Storage"
};

static const struct {
  LONG nErrorCode;
  int nMessagesCount;
  LPCSTR *lpszMessagesA;
} lpServerErrorMessages[] = {
  { 100, (int)MX_ARRAYLEN(szServerErrorMessages100), (LPCSTR*)szServerErrorMessages100 },
  { 200, (int)MX_ARRAYLEN(szServerErrorMessages200), (LPCSTR*)szServerErrorMessages200 },
  { 301, (int)MX_ARRAYLEN(szServerErrorMessages301), (LPCSTR*)szServerErrorMessages301 },
  { 400, (int)MX_ARRAYLEN(szServerErrorMessages400), (LPCSTR*)szServerErrorMessages400 },
  { 431, (int)MX_ARRAYLEN(szServerErrorMessages431), (LPCSTR*)szServerErrorMessages431 },
  { 500, (int)MX_ARRAYLEN(szServerErrorMessages500), (LPCSTR*)szServerErrorMessages500 }
};

//-----------------------------------------------------------

static LONG DecrementIfGreaterThanZero(_Inout_ LONG volatile *lpnValue);

//-----------------------------------------------------------

namespace MX {

CHttpServer::CHttpServer(_In_ CSockets &_cSocketMgr, _In_ CIoCompletionPortThreadPool &_cWorkerPool,
                         _In_opt_ CLoggable *lpLogParent) : CBaseMemObj(), CLoggable(),
                                                            CCriticalSection(), cSocketMgr(_cSocketMgr),
                                                            cWorkerPool(_cWorkerPool)
{
  cRequestCompletedWP = MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnRequestCompleted, this);
  //----
  SetLogParent(lpLogParent);
  //----
  dwMaxConnectionsPerIp = 0xFFFFFFFFUL;
  dwRequestHeaderTimeoutMs = 30000;
  dwRequestBodyMinimumThroughputInBps = 256;
  dwRequestBodySecondsOfLowThroughput = 10;
  dwResponseMinimumThroughputInBps = 256;
  dwResponseSecondsOfLowThroughput = 10;
  dwMaxHeaderSize = 16384;
  dwMaxFieldSize = 256000;
  ullMaxFileSize = 2097152ui64;
  dwMaxFilesCount = 4;
  dwMaxBodySizeInMemory = 32768;
  ullMaxBodySize = 10485760ui64;
  //----
  SlimRWL_Initialize(&(sSsl.nRwMutex));
  sSsl.nProtocol = CIpcSslLayer::ProtocolUnknown;
  RundownProt_Initialize(&nRundownLock);
  hAcceptConn = NULL;
  cNewRequestObjectCallback = NullCallback();
  cRequestHeadersReceivedCallback = NullCallback();
  cRequestCompletedCallback = NullCallback();
  cWebSocketRequestReceivedCallback = NullCallback();
  cErrorCallback = NullCallback();
  SlimRWL_Initialize(&nRequestsListRwMutex);
  _InterlockedExchange(&nDownloadNameGeneratorCounter, 0);
  //----
  SlimRWL_Initialize(&(sRequestLimiter.nRwMutex));
  return;
}

CHttpServer::~CHttpServer()
{
  TLnkLst<CClientRequest>::Iterator it;
  CClientRequest *lpRequest;
  BOOL b;

  RundownProt_WaitForRelease(&nRundownLock);
  //stop current server
  StopListening();
  //stop worker thread
  Stop();
  //terminate current connections and wait
  do
  {
    {
      CAutoSlimRWLShared cListLock(&nRequestsListRwMutex);

      b = cRequestsList.IsEmpty();
      for (lpRequest = it.Begin(cRequestsList); lpRequest != NULL; lpRequest = it.Next())
      {
        if ((_InterlockedOr(&(lpRequest->nFlags), REQUEST_FLAG_CloseInDestructorMark) &
             REQUEST_FLAG_CloseInDestructorMark) == 0)
        {
          lpRequest->AddRef();
          break;
        }
      }
    }
    if (lpRequest != NULL)
    {
      cSocketMgr.Close(lpRequest->hConn, MX_E_Cancelled);
      lpRequest->Release();
    }
    if (b == FALSE)
      _YieldProcessor();
  }
  while (b == FALSE);

  {
    CAutoSlimRWLExclusive cLimiterLock(&(sRequestLimiter.nRwMutex));
    CRequestLimiter *lpLimiter;

    while ((lpLimiter = sRequestLimiter.cTree.GetFirst()) != NULL)
    {
      lpLimiter->RemoveNode();
      delete lpLimiter;
    }
  }
  return;
}

VOID CHttpServer::SetOption_MaxConnectionsPerIp(_In_ DWORD dwLimit)
{
  CCriticalSection::CAutoLock cLock(*this);

  if (hAcceptConn == NULL)
  {
    dwMaxConnectionsPerIp = (dwLimit == 0) ? 0xFFFFFFFFUL : dwLimit;
  }
  return;
}

VOID CHttpServer::SetOption_RequestHeaderTimeout(_In_ DWORD dwTimeoutMs)
{
  CCriticalSection::CAutoLock cLock(*this);

  if (hAcceptConn == NULL)
  {
    if (dwTimeoutMs < 5 * 1000)
      dwRequestHeaderTimeoutMs = 5 * 1000;
    else if (dwTimeoutMs > 60 * 60 * 1000)
      dwRequestHeaderTimeoutMs = 60 * 60 * 1000;
    else
      dwRequestHeaderTimeoutMs = dwTimeoutMs;
  }
  return;
}

VOID CHttpServer::SetOption_RequestBodyLimits(_In_ DWORD dwMinimumThroughputInBps, _In_ DWORD dwSecondsOfLowThroughput)
{
  CCriticalSection::CAutoLock cLock(*this);

  if (hAcceptConn == NULL)
  {
    dwRequestBodyMinimumThroughputInBps = dwMinimumThroughputInBps;
    dwRequestBodySecondsOfLowThroughput = dwSecondsOfLowThroughput;
  }
  return;
}

VOID CHttpServer::SetOption_ResponseLimits(_In_ DWORD dwMinimumThroughputInBps, _In_ DWORD dwSecondsOfLowThroughput)
{
  CCriticalSection::CAutoLock cLock(*this);

  if (hAcceptConn == NULL)
  {
    dwResponseMinimumThroughputInBps = dwMinimumThroughputInBps;
    dwResponseSecondsOfLowThroughput = dwSecondsOfLowThroughput;
  }
  return;
}

VOID CHttpServer::SetOption_MaxHeaderSize(_In_ DWORD dwSize)
{
  CCriticalSection::CAutoLock cLock(*this);

  if (hAcceptConn == NULL)
  {
    dwMaxHeaderSize = dwSize;
  }
  return;
}

VOID CHttpServer::SetOption_MaxFieldSize(_In_ DWORD dwSize)
{
  CCriticalSection::CAutoLock cLock(*this);

  if (hAcceptConn == NULL)
  {
    dwMaxFieldSize = dwSize;
  }
  return;
}

VOID CHttpServer::SetOption_MaxFileSize(_In_ ULONGLONG ullSize)
{
  CCriticalSection::CAutoLock cLock(*this);

  if (hAcceptConn == NULL)
  {
    ullMaxFileSize = ullSize;
  }
  return;
}

VOID CHttpServer::SetOption_MaxFilesCount(_In_ DWORD dwCount)
{
  CCriticalSection::CAutoLock cLock(*this);

  if (hAcceptConn == NULL)
  {
    dwMaxFilesCount = dwCount;
  }
  return;
}

BOOL CHttpServer::SetOption_TemporaryFolder(_In_opt_z_ LPCWSTR szFolderW)
{
  CCriticalSection::CAutoLock cLock(*this);

  if (hAcceptConn == NULL)
  {
    if (szFolderW != NULL && *szFolderW != NULL)
    {
      if (cStrTemporaryFolderW.Copy(szFolderW) == FALSE)
        return FALSE;
    }
    else
    {
      cStrTemporaryFolderW.Empty();
    }
  }
  return TRUE;
}

VOID CHttpServer::SetOption_MaxBodySizeInMemory(_In_ DWORD dwSize)
{
  CCriticalSection::CAutoLock cLock(*this);

  if (hAcceptConn == NULL)
  {
    dwMaxBodySizeInMemory = dwSize;
  }
  return;
}

VOID CHttpServer::SetOption_MaxBodySize(_In_ ULONGLONG ullSize)
{
  CCriticalSection::CAutoLock cLock(*this);

  if (hAcceptConn == NULL)
  {
    ullMaxBodySize = ullSize;
  }
  return;
}

VOID CHttpServer::SetNewRequestObjectCallback(_In_ OnNewRequestObjectCallback _cNewRequestObjectCallback)
{
  cNewRequestObjectCallback = _cNewRequestObjectCallback;
  return;
}

VOID CHttpServer::SetRequestHeadersReceivedCallback(_In_ OnRequestHeadersReceivedCallback
                                                    _cRequestHeadersReceivedCallback)
{
  cRequestHeadersReceivedCallback = _cRequestHeadersReceivedCallback;
  return;
}

VOID CHttpServer::SetRequestCompletedCallback(_In_ OnRequestCompletedCallback _cRequestCompletedCallback)
{
  cRequestCompletedCallback = _cRequestCompletedCallback;
  return;
}

VOID CHttpServer::SetWebSocketRequestReceivedCallback(_In_ OnWebSocketRequestReceivedCallback
                                                      _cWebSocketRequestReceivedCallback)
{
  cWebSocketRequestReceivedCallback = _cWebSocketRequestReceivedCallback;
  return;
}

VOID CHttpServer::SetErrorCallback(_In_ OnErrorCallback _cErrorCallback)
{
  cErrorCallback = _cErrorCallback;
  return;
}

HRESULT CHttpServer::StartListening(_In_ CSockets::eFamily nFamily, _In_ int nPort
                                    , _In_opt_ CIpcSslLayer::eProtocol nProtocol,
                                    _In_opt_ CSslCertificate *lpSslCertificate, _In_opt_ CCryptoRSA *lpSslKey)
{
  return StartListening((LPCSTR)NULL, nFamily, nPort, nProtocol, lpSslCertificate, lpSslKey);
}

HRESULT CHttpServer::StartListening(_In_opt_z_ LPCSTR szBindAddressA, _In_ CSockets::eFamily nFamily, _In_ int nPort,
                                    _In_opt_ CIpcSslLayer::eProtocol nProtocol,
                                    _In_opt_ CSslCertificate *lpSslCertificate, _In_opt_ CCryptoRSA *lpSslKey)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  HRESULT hRes;

  switch (nProtocol)
  {
    case CIpcSslLayer::ProtocolUnknown:
      break;
    case CIpcSslLayer::ProtocolTLSv1_0:
    case CIpcSslLayer::ProtocolTLSv1_1:
    case CIpcSslLayer::ProtocolTLSv1_2:
      if (lpSslCertificate == NULL)
        return E_POINTER;
      break;

    default:
      return E_INVALIDARG;
  }

  StopListening();
  if (cAutoRundownProt.IsAcquired() == FALSE)
    return MX_E_NotReady;

  hRes = cShutdownEv.Create(TRUE, FALSE);
  //start worker thread
  if (SUCCEEDED(hRes) && IsRunning() == FALSE)
  {
    if (Start() == FALSE)
      hRes = E_OUTOFMEMORY;
  }
  if (SUCCEEDED(hRes))
  {
    CCriticalSection::CAutoLock cLock(*this);

    //set SSL
    if (nProtocol != CIpcSslLayer::ProtocolUnknown)
    {
      CAutoSlimRWLExclusive cSslLock(&(sSsl.nRwMutex));

      sSsl.nProtocol = nProtocol;
      sSsl.cSslCertificate.Attach(MX_DEBUG_NEW CSslCertificate());
      if (sSsl.cSslCertificate)
      {
        try
        {
          *(sSsl.cSslCertificate.Get()) = *lpSslCertificate;
        }
        catch (LONG hr)
        {
          hRes = hr;
        }
      }
      else
      {
        hRes = E_OUTOFMEMORY;
      }
      if (SUCCEEDED(hRes) && lpSslKey != NULL)
      {
        SIZE_T nSize;
        LPVOID lpBuf;

        nSize = lpSslKey->GetPrivateKey();
        if (nSize > 0)
        {
          sSsl.cSslPrivateKey.Attach(MX_DEBUG_NEW CCryptoRSA());
          lpBuf = MX_MALLOC(nSize);
          if (sSsl.cSslPrivateKey && lpBuf != NULL)
          {
            nSize = lpSslKey->GetPrivateKey(lpBuf);
            hRes = sSsl.cSslPrivateKey->SetPrivateKeyFromDER(lpBuf, nSize);
          }
          else
          {
            hRes = E_OUTOFMEMORY;
          }
          MX_FREE(lpBuf);
        }
      }
    }
    if (SUCCEEDED(hRes))
    {
      hRes = cSocketMgr.CreateListener(nFamily, nPort, MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnSocketCreate, this),
                                       szBindAddressA, NULL, &hAcceptConn);
    }
    if (FAILED(hRes))
    {
      CAutoSlimRWLExclusive cSslLock(&(sSsl.nRwMutex));

      sSsl.nProtocol = CIpcSslLayer::ProtocolUnknown;
      sSsl.cSslCertificate.Reset();
      sSsl.cSslPrivateKey.Reset();

      cShutdownEv.Close();
    }
  }
  //done
  return hRes;
}

HRESULT CHttpServer::StartListening(_In_opt_z_ LPCWSTR szBindAddressW, _In_ CSockets::eFamily nFamily, _In_ int nPort,
                                    _In_opt_ CIpcSslLayer::eProtocol nProtocol,
                                    _In_opt_ CSslCertificate *lpSslCertificate, _In_opt_ CCryptoRSA *lpSslKey)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (szBindAddressW != NULL && szBindAddressW[0] != 0)
  {
    hRes = Punycode_Encode(cStrTempA, szBindAddressW);
    if (FAILED(hRes))
      return hRes;
  }
  return StartListening((LPSTR)cStrTempA, nFamily, nPort, nProtocol, lpSslCertificate, lpSslKey);
}

VOID CHttpServer::StopListening()
{
  BOOL bWait = FALSE;

  cShutdownEv.Set();
  //shutdown current listener
  {
    CCriticalSection::CAutoLock cLock(*this);

    if (hAcceptConn != NULL)
    {
      cSocketMgr.Close(hAcceptConn, MX_E_Cancelled);
      bWait = TRUE;
    }
    //ssl
    {
      CAutoSlimRWLExclusive cSslLock(&(sSsl.nRwMutex));

      sSsl.nProtocol = CIpcSslLayer::ProtocolUnknown;
      sSsl.cSslCertificate.Reset();
      sSsl.cSslPrivateKey.Reset();
    }
  }
  //wait until terminated
  if (bWait != FALSE)
  {
    BOOL bDone;

    do
    {
      _YieldProcessor();
      {
        CCriticalSection::CAutoLock cLock(*this);

        bDone = (hAcceptConn == NULL) ? TRUE : FALSE;
      }
    }
    while (bDone == FALSE);
  }
  //done
  cShutdownEv.Close();
  return;
}

VOID CHttpServer::ThreadProc()
{
  while (CheckForAbort(1000) == FALSE)
  {
    CAutoRundownProtection cAutoRundownProt(&nRundownLock);
    TLnkLst<CClientRequest>::Iterator it;
    CClientRequest *lpRequest;

    if (cAutoRundownProt.IsAcquired() != FALSE)
    {
      HRESULT hRes = E_FAIL;

      {
        CAutoSlimRWLShared cListLock(&nRequestsListRwMutex);

        for (lpRequest = it.Begin(cRequestsList); lpRequest != NULL; lpRequest = it.Next())
        {
          _InterlockedAnd(&(lpRequest->nFlags), ~REQUEST_FLAG_RequestTimeoutProcessed);
        }
      }
      //process request's timeout
      do
      {
        {
          CAutoSlimRWLShared cListLock(&nRequestsListRwMutex);

          for (lpRequest = it.Begin(cRequestsList); lpRequest != NULL; lpRequest = it.Next())
          {
            if ((_InterlockedOr(&(lpRequest->nFlags), REQUEST_FLAG_RequestTimeoutProcessed) &
                 REQUEST_FLAG_RequestTimeoutProcessed) == 0)
            {
              CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

              if (lpRequest->IsLinkClosed() == FALSE && lpRequest->hConn != NULL)
              {
                int nTimedOut = 0;

                if (lpRequest->nState == CClientRequest::StateReceivingRequestHeaders)
                {
                  if (lpRequest->cRequest->dwHeadersTimeoutCounterSecs > 0)
                  {
                    if ((--(lpRequest->cRequest->dwHeadersTimeoutCounterSecs)) == 0)
                      nTimedOut = 1;
                  }
                }
                else if (lpRequest->nState == CClientRequest::StateReceivingRequestBody)
                {
                  float nReadKbps;

                  if (SUCCEEDED(cSocketMgr.GetReadStats(lpRequest->hConn, NULL, &nReadKbps)))
                  {
                    if ((DWORD)(int)nReadKbps < dwRequestBodyMinimumThroughputInBps)
                    {
                      if ((++(lpRequest->cRequest->dwBodyLowThroughputCcounter)) >= dwRequestBodySecondsOfLowThroughput)
                        nTimedOut = 1;
                    }
                    else
                    {
                      lpRequest->cRequest->dwBodyLowThroughputCcounter = 0;
                    }
                  }
                }
                else if (lpRequest->nState == CClientRequest::StateSendingResponse)
                {
                  float nWriteKbps;

                  if (SUCCEEDED(cSocketMgr.GetWriteStats(lpRequest->hConn, NULL, &nWriteKbps)))
                  {
                    if ((DWORD)(int)nWriteKbps < dwResponseMinimumThroughputInBps)
                    {
                      if ((++(lpRequest->cResponse->dwLowThroughputCcounter)) >= dwResponseMinimumThroughputInBps)
                        nTimedOut = 2;
                    }
                    else
                    {
                      lpRequest->cResponse->dwLowThroughputCcounter = 0;
                    }
                  }
                }

                if (nTimedOut == 1)
                {
                  hRes = QuickSendErrorResponseAndReset(lpRequest, 408, MX_E_Timeout, TRUE);
                  if (FAILED(hRes) && cErrorCallback)
                  {
                    lpRequest->AddRef();
                    break;
                  }
                }
                else if (nTimedOut == 2)
                {
                  cSocketMgr.Close(lpRequest->hConn, hRes);
                  lpRequest->SetState(CClientRequest::StateError);
                  if (ShouldLog(1) != FALSE)
                  {
                    Log(L"HttpServer(State/Req:0x%p/Conn:0x%p): StateError [%08X]", lpRequest, lpRequest->hConn,
                        MX_E_Timeout);
                  }
                }
              }
            }
          }
        }
        if (lpRequest != NULL)
        {
          cErrorCallback(this, lpRequest, hRes);
          lpRequest->Release();
        }
      }
      while (lpRequest != NULL);
    }
  }
  //done
  return;
}

HRESULT CHttpServer::OnSocketCreate(_In_ CIpc *lpIpc, _In_ HANDLE h, _Inout_ CIpc::CREATE_CALLBACK_DATA &sData)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);

  if (cAutoRundownProt.IsAcquired() == FALSE)
    return MX_E_NotReady;
  //setup
  switch (cSocketMgr.GetClass(h))
  {
    case CIpc::ConnectionClassListener:
      //setup callbacks
      sData.cDestroyCallback = MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnListenerSocketDestroy, this);
      break;

    case CIpc::ConnectionClassServer:
      {
      TAutoRefCounted<CClientRequest> cNewRequest;
      TAutoDeletePtr<CIpcSslLayer> cLayer;
      HRESULT hRes;

      //setup callbacks
      sData.cConnectCallback = MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnSocketConnect, this);
      sData.cDataReceivedCallback = MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnSocketDataReceived, this);
      sData.cDestroyCallback = MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnSocketDestroy, this);
      //create new request object
      if (cNewRequestObjectCallback)
      {
        hRes = cNewRequestObjectCallback(&cNewRequest);
        if (FAILED(hRes))
          return hRes;
      }
      else
      {
        cNewRequest.Attach(MX_DEBUG_NEW CClientRequest());
      }
      if (!cNewRequest)
        return E_OUTOFMEMORY;
      hRes = cNewRequest->Initialize(this, &cSocketMgr, h, dwMaxHeaderSize);
      if (FAILED(hRes))
        return hRes;
      //setup header timeout
      cNewRequest->cRequest->dwHeadersTimeoutCounterSecs = dwRequestHeaderTimeoutMs;
      //add ssl layer
      {
        CAutoSlimRWLShared cSslLock(&(sSsl.nRwMutex));

        if (sSsl.nProtocol != CIpcSslLayer::ProtocolUnknown)
        {
          cLayer.Attach(MX_DEBUG_NEW CIpcSslLayer());
          if (!cLayer)
            return E_OUTOFMEMORY;
          hRes = cLayer->Initialize(TRUE, sSsl.nProtocol, NULL, NULL, sSsl.cSslCertificate.Get(),
                                    sSsl.cSslPrivateKey.Get());
          if (SUCCEEDED(hRes))
          {
            hRes = lpIpc->AddLayer(h, cLayer.Get());
            if (SUCCEEDED(hRes))
              cLayer.Detach();
          }
          if (FAILED(hRes))
            return hRes;
        }
      }
      //add to list
      {
        CAutoSlimRWLExclusive cListLock(&nRequestsListRwMutex);

        cRequestsList.PushTail(cNewRequest.Get());
      }
      sData.cUserData = cNewRequest;
      }
      break;
  }
  //done
  return S_OK;
}

VOID CHttpServer::OnListenerSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                                          _In_ HRESULT hrErrorCode)
{
  CCriticalSection::CAutoLock cLock(*this);

  if (h == hAcceptConn)
    hAcceptConn = NULL;
  return;
}

VOID CHttpServer::OnSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                                  _In_ HRESULT hrErrorCode)
{
  CClientRequest *lpRequest = (CClientRequest*)lpUserData;

  if (lpRequest != NULL)
  {
    if (lpRequest->sPeerAddr.si_family != 0)
    {
      CAutoSlimRWLShared cLimiterLock(&(sRequestLimiter.nRwMutex));
      CRequestLimiter *lpLimiter;

      lpLimiter = sRequestLimiter.cTree.Find(&(lpRequest->sPeerAddr));
      if (lpLimiter != NULL)
      {
        if ((DWORD)_InterlockedDecrement(&(lpLimiter->nCount)) == 0)
        {
          cLimiterLock.UpgradeToExclusive();

          lpLimiter = sRequestLimiter.cTree.Find(&(lpRequest->sPeerAddr));
          if (lpLimiter != NULL)
          {
            if (__InterlockedRead(&(lpLimiter->nCount)) == 0)
            {
              lpLimiter->RemoveNode();
              delete lpLimiter;
            }
          }
        }
      }
    }

    {
      CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

      if (lpRequest->hConn == h)
      {
        //mark link as closed
        lpRequest->MarkLinkAsClosed();

        lpRequest->hConn = NULL;
      }
    }
  }
  //raise error notification
  if (FAILED(hrErrorCode) && cErrorCallback)
    cErrorCallback(this, NULL, hrErrorCode);
  return;
}

HRESULT CHttpServer::OnSocketConnect(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                                     _In_ HRESULT hrErrorCode)
{
  CClientRequest *lpNewRequest = (CClientRequest*)lpUserData;

  MX_ASSERT(lpUserData != NULL);
  //count connections from the same IP
  if (SUCCEEDED(cSocketMgr.GetPeerAddress(h, &(lpNewRequest->sPeerAddr))) && dwMaxConnectionsPerIp != 0xFFFFFFFFUL)
  {
    CAutoSlimRWLShared cLimiterLock(&(sRequestLimiter.nRwMutex));
    CRequestLimiter *lpLimiter;

    lpLimiter = sRequestLimiter.cTree.Find(&(lpNewRequest->sPeerAddr));
    if (lpLimiter != NULL)
    {
      if ((DWORD)_InterlockedIncrement(&(lpLimiter->nCount)) > dwMaxConnectionsPerIp)
        return E_ACCESSDENIED;
    }
    else
    {
      MX::TRedBlackTreeNode<MX::CHttpServer::CRequestLimiter,PSOCKADDR_INET> *_lpMatchingLimiter;

      lpLimiter = MX_DEBUG_NEW CRequestLimiter(&(lpNewRequest->sPeerAddr));
      if (lpLimiter == NULL)
        return E_OUTOFMEMORY;
      cLimiterLock.UpgradeToExclusive();

      if (sRequestLimiter.cTree.Insert(lpLimiter, FALSE, &_lpMatchingLimiter) == FALSE)
      {
        CRequestLimiter *lpMatchingLimiter = static_cast<CRequestLimiter*>(_lpMatchingLimiter);

        delete lpLimiter;

        if ((DWORD)_InterlockedIncrement(&(lpMatchingLimiter->nCount)) > dwMaxConnectionsPerIp)
          return E_ACCESSDENIED;
      }
    }
  }
  else
  {
    lpNewRequest->sPeerAddr.si_family = 0;
  }
  //done
  return S_OK;
}

HRESULT CHttpServer::OnSocketDataReceived(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData)
{
  CClientRequest *lpRequest = (CClientRequest*)lpUserData;
  BYTE aMsgBuf[4096];
  SIZE_T nMsgSize;
  BOOL bFireRequestHeadersReceivedCallback, bFireRequestCompleted;
  BOOL bFireWebSocketRequestReceivedCallback;
  TAutoRefCounted<CWebSocket> cFireWebSocketOnConnected;
  HRESULT hRes;

  MX_ASSERT(lpUserData != NULL);

full_restart:
  if (lpRequest->nState == CClientRequest::StateInitializing)
  {
    hRes = lpRequest->OnSetup();
    if (FAILED(hRes))
      goto done;
    hRes = lpRequest->SetState(CClientRequest::StateReceivingRequestHeaders);
    if (FAILED(hRes))
      goto done;
    if (ShouldLog(1) != FALSE)
    {
      Log(L"HttpServer(State/Req:0x%p/Conn:0x%p): StateReceivingRequestHeaders", lpRequest, h);
    }
  }

  hRes = S_OK;
restart:
  bFireRequestHeadersReceivedCallback = bFireRequestCompleted = FALSE;
  bFireWebSocketRequestReceivedCallback = FALSE;
  {
    CCriticalSection::CAutoLock cLock(lpRequest->cMutex);
    CHttpCommon::eState nParserState;
    SIZE_T nMsgUsed;
    BOOL bBreakLoop;

    //loop
    bBreakLoop = FALSE;
    while (SUCCEEDED(hRes) && bBreakLoop == FALSE && bFireRequestHeadersReceivedCallback == FALSE &&
           bFireRequestCompleted == FALSE && bFireWebSocketRequestReceivedCallback == FALSE)
    {
      //get buffered message
      nMsgSize = sizeof(aMsgBuf);
      hRes = cSocketMgr.GetBufferedMessage(h, aMsgBuf, &nMsgSize);
      if (SUCCEEDED(hRes) && nMsgSize == 0)
        break;
      if (FAILED(hRes))
      {
        hRes = QuickSendErrorResponseAndReset(lpRequest, 500, hRes, TRUE);
        break;
      }
      //take action depending on current state
      switch (lpRequest->nState)
      {
        case CClientRequest::StateReceivingRequestHeaders:
        case CClientRequest::StateReceivingRequestBody:
          //process http being received
          hRes = lpRequest->cRequest->cHttpCmn.Parse(aMsgBuf, nMsgSize, nMsgUsed);
          if (SUCCEEDED(hRes) && nMsgUsed > 0)
            hRes = cSocketMgr.ConsumeBufferedMessage(h, nMsgUsed);
          if (SUCCEEDED(hRes))
          {
            //take action if parser's state changed
            nParserState = lpRequest->cRequest->cHttpCmn.GetParserState();
            switch (nParserState)
            {
              case CHttpCommon::StateBodyStart:
                lpRequest->QueryBrowser();
                //check if we are dealing with a websocket connection
                hRes = CheckForWebSocket(lpRequest);
                if (FAILED(hRes))
                  goto on_request_error;
                if (hRes == S_OK)
                {
on_websocket_request:
                  if (lpRequest->cRequest->cHttpCmn.GetErrorCode() != 0)
                  {
                    hRes = MX_E_InvalidData;
                    goto on_request_error;
                  }
                  bFireWebSocketRequestReceivedCallback = TRUE;
                  hRes = lpRequest->SetState(CClientRequest::StateNegotiatingWebSocket);
                  if (FAILED(hRes))
                    goto on_request_error;
                }
                else
                {
                  //normal http request
                  if (lpRequest->cRequest->cHttpCmn.GetErrorCode() == 0)
                    bFireRequestHeadersReceivedCallback = TRUE; //fire events only if no error
                  hRes = lpRequest->SetState(CClientRequest::StateReceivingRequestBody);
                  if (FAILED(hRes))
                    goto on_request_error;
                  if (ShouldLog(1) != FALSE)
                  {
                    Log(L"HttpServer(State/Req:0x%p/Conn:0x%p): StateReceivingRequestBody", lpRequest,
                        lpRequest->hConn);
                  }
                }
                break;

              case CHttpCommon::StateDone:
                if (lpRequest->nState == CClientRequest::StateReceivingRequestHeaders)
                {
                  lpRequest->QueryBrowser();
                  //check if we are dealing with a websocket connection
                  hRes = CheckForWebSocket(lpRequest);
                  if (FAILED(hRes))
                    goto on_request_error;
                  if (hRes == S_OK)
                    goto on_websocket_request;
                }
                //cancel headers timeout
                lpRequest->cRequest->dwHeadersTimeoutCounterSecs = 0;
                //check if there was a parse error
                if (lpRequest->cRequest->cHttpCmn.GetErrorCode() != 0)
                {
                  hRes = (lpRequest->cRequest->cHttpCmn.GetErrorCode() == 413) ? MX_E_BadLength : MX_E_InvalidData;
                  hRes = QuickSendErrorResponseAndReset(lpRequest, lpRequest->cRequest->cHttpCmn.GetErrorCode(),
                                                        hRes, TRUE);
                  break;
                }
                //check if the uploaded body is too large
                if (lpRequest->nState == CClientRequest::StateReceivingRequestBody)
                {
                  TAutoRefCounted<CHttpBodyParserBase> cBodyParser;

                  cBodyParser.Attach(lpRequest->cRequest->cHttpCmn.GetBodyParser());
                  if (cBodyParser)
                  {
                    if (cBodyParser->IsEntityTooLarge() != FALSE)
                    {
                      hRes = QuickSendErrorResponseAndReset(lpRequest, 413, MX_E_BadLength, TRUE);
                      break;
                    }
                  }
                }
                //all is ok
                bFireRequestCompleted = TRUE;
                lpRequest->cRequest->dwHeadersTimeoutCounterSecs = 0;
                if (lpRequest->nState == CClientRequest::StateReceivingRequestHeaders)
                {
                  bFireRequestHeadersReceivedCallback = TRUE;
                }
                hRes = lpRequest->SetState(CClientRequest::StateBuildingResponse);
                if (FAILED(hRes))
                  goto on_request_error;
                if (ShouldLog(1) != FALSE)
                {
                  Log(L"HttpServer(State/Req:0x%p/Conn:0x%p): StateBuildingResponse", lpRequest, lpRequest->hConn);
                }
                break;
            }
          }
          else
          {
on_request_error:
            hRes = QuickSendErrorResponseAndReset(lpRequest, (hRes == E_INVALIDARG ||
                                                              hRes == MX_E_InvalidData) ? 400 : 500, hRes, TRUE);
          }
          break;

        case CClientRequest::StateInitializing:
          goto full_restart;

        default:
          bBreakLoop = TRUE;
          break;
      }
    }
  }

  //have some action to do?
  if (SUCCEEDED(hRes) && bFireWebSocketRequestReceivedCallback != FALSE)
  {
    WEBSOCKET_REQUEST_CALLBACK_DATA sData = {
      lpRequest->cWebSocketInfo->szProtocolsA,
      -1,
      NULL
    };

    hRes = (cWebSocketRequestReceivedCallback) ? cWebSocketRequestReceivedCallback(this, lpRequest, cShutdownEv.Get(),
                                                                                   sData)
                                                : MX_E_Unsupported;
    {
      CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

      if (lpRequest->nState == CClientRequest::StateNegotiatingWebSocket)
      {
        BOOL bFatal = FALSE;

        if (SUCCEEDED(hRes))
        {
          hRes = InitiateWebSocket(lpRequest, sData, bFatal);
          if (SUCCEEDED(hRes))
            cFireWebSocketOnConnected = sData.lpWebSocket;
        }
        if (FAILED(hRes))
        {
          if (bFatal == FALSE)
          {
            QuickSendErrorResponseAndReset(lpRequest, (hRes == MX_E_Unsupported ||
                                            hRes == MX_E_InvalidData) ? 400 : 500,
                                            hRes, TRUE); //ignore return code
          }
          else
          {
            cSocketMgr.Close(lpRequest->hConn, hRes);
          }
        }
        goto done;
      }
    }
  }
  if (SUCCEEDED(hRes) && bFireRequestHeadersReceivedCallback != FALSE)
  {
    CHttpBodyParserBase *lpBodyParser = NULL;

    hRes = (cRequestHeadersReceivedCallback) ? cRequestHeadersReceivedCallback(this, lpRequest, cShutdownEv.Get(),
                                                                               &lpBodyParser)
                                             : S_OK;
    {
      CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

      if (lpRequest->nState == CClientRequest::StateReceivingRequestHeaders ||
          lpRequest->nState == CClientRequest::StateReceivingRequestBody ||
          lpRequest->nState == CClientRequest::StateBuildingResponse)
      {
        if (SUCCEEDED(hRes))
        {
          TAutoRefCounted<CHttpBodyParserBase> cBodyParser;

          cBodyParser.Attach(lpBodyParser);
          if (!cBodyParser)
          {
            CHttpHeaderEntContentType *lpHeader = lpRequest->cRequest->cHttpCmn.GetHeader<CHttpHeaderEntContentType>();
            if (lpHeader != NULL)
            {
              if (StrCompareA(lpHeader->GetType(), "application/x-www-form-urlencoded") == 0)
              {
                cBodyParser.Attach(MX_DEBUG_NEW CHttpBodyParserUrlEncodedForm(dwMaxFieldSize));
              }
              else if (StrCompareA(lpHeader->GetType(), "multipart/form-data") == 0)
              {
                cBodyParser.Attach(MX_DEBUG_NEW CHttpBodyParserMultipartFormData(
                                                      MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnDownloadStarted, this),
                                                      lpRequest, dwMaxFieldSize, ullMaxFileSize, dwMaxFilesCount));
              }
            }
            if (!cBodyParser)
            {
              cBodyParser.Attach(MX_DEBUG_NEW CHttpBodyParserDefault(
                                                    MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnDownloadStarted, this),
                                                    lpRequest, dwMaxBodySizeInMemory, ullMaxBodySize));
            }
            if (!cBodyParser)
              hRes = E_OUTOFMEMORY;
          }
          if (SUCCEEDED(hRes))
          {
            hRes = lpRequest->cRequest->cHttpCmn.SetBodyParser(cBodyParser.Get());
          }
        }
        if (FAILED(hRes))
        {
          QuickSendErrorResponseAndReset(lpRequest, 500, hRes, TRUE); //ignore return code
        }
      }
    }
  }
  if (SUCCEEDED(hRes) && bFireRequestCompleted != FALSE)
  {
    lpRequest->AddRef();
    hRes = cWorkerPool.Post(cRequestCompletedWP, 0, &(lpRequest->sOvr));
    if (FAILED(hRes))
    {
      lpRequest->Release();
      QuickSendErrorResponseAndReset(lpRequest, 500, hRes, TRUE); //ignore return code
    }
  }
  //restart on success
  if (SUCCEEDED(hRes))
  {
    if (bFireRequestHeadersReceivedCallback != FALSE || bFireRequestCompleted != FALSE)
      goto restart;
  }
done:
  if (SUCCEEDED(hRes) && cFireWebSocketOnConnected)
  {
    hRes = cFireWebSocketOnConnected->OnConnected();
    cFireWebSocketOnConnected.Release();
  }

  //raise error event if any
  if (FAILED(hRes) && cErrorCallback)
    cErrorCallback(this, lpRequest, hRes);
  return hRes;
}

VOID CHttpServer::OnRequestCompleted(_In_ CIoCompletionPortThreadPool *lpPool, _In_ DWORD dwBytes,
                                     _In_ OVERLAPPED *lpOvr, _In_ HRESULT hRes)
{
  CClientRequest *lpRequest = (CClientRequest*)((char*)lpOvr - (char*)&(((CClientRequest*)0)->sOvr));

  if (SUCCEEDED(hRes))
  {
    if (cRequestCompletedCallback)
      cRequestCompletedCallback(this, lpRequest, cShutdownEv.Get());
    else
      lpRequest->End();
  }
  else
  {
    lpRequest->End(hRes);
  }
  lpRequest->Release();
  return;
}

HRESULT CHttpServer::QuickSendErrorResponseAndReset(_In_ CClientRequest *lpRequest, _In_ LONG nErrorCode,
                                                    _In_ HRESULT hrErrorCode, _In_ BOOL bForceClose)
{
  CStringA cStrResponseA;
  HRESULT hRes;

  hRes = GenerateErrorPage(cStrResponseA, lpRequest, nErrorCode, hrErrorCode);
  if (SUCCEEDED(hRes))
  {
    _InterlockedOr(&(lpRequest->nFlags), REQUEST_FLAG_ErrorPageSent);
    hRes = cSocketMgr.SendMsg(lpRequest->hConn, (LPCSTR)cStrResponseA, cStrResponseA.GetLength());
    if (bForceClose == FALSE && SUCCEEDED(hRes))
    {
      hRes = cSocketMgr.AfterWriteSignal(lpRequest->hConn,
                                         MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnAfterSendResponse, this), lpRequest);
    }
  }
  if (SUCCEEDED(hRes))
  {
    hRes = lpRequest->SetState(CClientRequest::StateError);
  }
  if (bForceClose != FALSE || FAILED(hRes))
  {
    cSocketMgr.Close(lpRequest->hConn, hRes);
  }
  //done
  if (ShouldLog(1) != FALSE)
  {
    Log(L"HttpServer(State/Req:0x%p/Conn:0x%p): StateError [%ld/%08X]", lpRequest, lpRequest->hConn, hrErrorCode);
  }
  return hRes;
}

VOID CHttpServer::OnAfterSendResponse(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ LPVOID lpCookie,
                                      _In_ CIpc::CUserData *lpUserData)
{
  CClientRequest *lpRequest = (CClientRequest*)lpUserData;
  CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

  if ((__InterlockedRead(&(lpRequest->nFlags)) & REQUEST_FLAG_DontKeepAlive) == 0)
  {
    HRESULT hRes = lpRequest->ResetForNewRequest();
    if (SUCCEEDED(hRes))
      return;
  }
  if (ShouldLog(1) != FALSE)
  {
    Log(L"HttpServer(OnAfterSendResponse/Req:0x%p/Conn:0x%p): Closing", lpRequest, lpRequest->hConn);
  }
  cSocketMgr.Close(lpRequest->hConn);
  return;
}

HRESULT CHttpServer::GenerateErrorPage(_Out_ CStringA &cStrResponseA, _In_ CClientRequest *lpRequest,
                                       _In_ LONG nErrorCode, _In_ HRESULT hrErrorCode,
                                       _In_opt_z_ LPCSTR szBodyExplanationA)
{
  CDateTime cDtNow;
  LPCSTR szErrMsgA;
  BOOL bHasBody;
  int nHttpVersion[2];
  HRESULT hRes;

  cStrResponseA.Empty();

  szErrMsgA = LocateError(nErrorCode);
  if (szErrMsgA == NULL)
    return E_INVALIDARG;
  bHasBody = TRUE;
  if (*szErrMsgA == '*')
  {
    bHasBody = FALSE;
    szErrMsgA++;
  }
  nHttpVersion[0] = lpRequest->cRequest->cHttpCmn.GetRequestVersionMajor();
  nHttpVersion[1] = lpRequest->cRequest->cHttpCmn.GetRequestVersionMinor();
  if (nHttpVersion[0] == 0)
  {
    nHttpVersion[0] = 1;
    nHttpVersion[1] = 0;
  }
  if (cStrResponseA.Format("HTTP/%d.%d %03d %s\r\nServer: %s\r\n", nHttpVersion[0], nHttpVersion[1], nErrorCode,
                           szErrMsgA, szServerInfoA) == FALSE)
  {
    return E_OUTOFMEMORY;
  }
  if (SUCCEEDED(hrErrorCode))
  {
    if (cStrResponseA.AppendFormat("X-ErrorCode: 0x%08X\r\n", hrErrorCode) == FALSE)
      return E_OUTOFMEMORY;
  }
  hRes = cDtNow.SetFromNow(FALSE);
  if (SUCCEEDED(hRes))
    hRes = cDtNow.AppendFormat(cStrResponseA, "Date: %a, %d %b %Y %H:%M:%S %z\r\n");
  if (FAILED(hRes))
    return hRes;
  if (bHasBody != FALSE)
  {
    CStringA cStrBodyA;

    if (szBodyExplanationA == NULL)
      szBodyExplanationA = "";
    if (cStrBodyA.Format(szServerErrorFormatA, nErrorCode, szErrMsgA, nErrorCode, szErrMsgA,
                         szBodyExplanationA) == FALSE ||
        cStrResponseA.AppendFormat("Cache-Control: private\r\nContent-Length: %Iu\r\nContent-Type: text/html\r\n\r\n",
                                   cStrBodyA.GetLength()) == FALSE ||
        cStrResponseA.ConcatN((LPCSTR)cStrBodyA, cStrBodyA.GetLength()) == FALSE)
    {
      return E_OUTOFMEMORY;
    }
  }
  else
  {
    if (cStrResponseA.ConcatN("\r\n", 2) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

LPCSTR CHttpServer::LocateError(_In_ LONG nErrorCode)
{
  SIZE_T i;

  for (i=0; i<MX_ARRAYLEN(lpServerErrorMessages); i++)
  {
    if (nErrorCode >= lpServerErrorMessages[i].nErrorCode &&
        nErrorCode < lpServerErrorMessages[i].nErrorCode + lpServerErrorMessages[i].nMessagesCount)
    {
      return lpServerErrorMessages[i].lpszMessagesA[nErrorCode - lpServerErrorMessages[i].nErrorCode];
    }
  }
  return NULL;
}

HRESULT CHttpServer::CheckForWebSocket(_In_ CClientRequest *lpRequest)
{
  CHttpHeaderGenConnection *lpConnHeader = lpRequest->cRequest->cHttpCmn.GetHeader<CHttpHeaderGenConnection>();
  if (lpConnHeader != NULL && lpConnHeader->HasConnection("upgrade") != FALSE)
  {
    CHttpHeaderGenUpgrade *lpUpgradeHeader = lpRequest->cRequest->cHttpCmn.GetHeader<CHttpHeaderGenUpgrade>();
    if (lpUpgradeHeader != NULL && lpUpgradeHeader->HasProduct("websocket") != FALSE)
    {
      CHttpHeaderReqSecWebSocketKey *lpReqSecWebSocketKeyHeader;
      CHttpHeaderReqSecWebSocketProtocol *lpReqSecWebSocketProtocolHeader;
      CHttpHeaderReqSecWebSocketVersion *lpReqSecWebSocketVersionHeader;
      SIZE_T i, nCount, nSize, nOffset;
      LPCSTR sA;

      if (lpRequest->cRequest->cHttpCmn.GetHeader("Origin") == NULL)
        return MX_E_InvalidData;

      //check the request method
      if (StrCompareA(lpRequest->cRequest->cHttpCmn.GetRequestMethod(), "GET") != 0)
        return MX_E_InvalidData;

      //get required headers
      lpReqSecWebSocketKeyHeader = lpRequest->cRequest->cHttpCmn.GetHeader<CHttpHeaderReqSecWebSocketKey>();
      lpReqSecWebSocketProtocolHeader = lpRequest->cRequest->cHttpCmn.GetHeader<CHttpHeaderReqSecWebSocketProtocol>();
      lpReqSecWebSocketVersionHeader = lpRequest->cRequest->cHttpCmn.GetHeader<CHttpHeaderReqSecWebSocketVersion>();
      if (lpReqSecWebSocketKeyHeader == NULL || lpReqSecWebSocketProtocolHeader == NULL ||
          lpReqSecWebSocketVersionHeader == NULL)
      {
        return MX_E_InvalidData;
      }

      //verify supported versions
      if (lpReqSecWebSocketVersionHeader->GetVersion() != 8 && lpReqSecWebSocketVersionHeader->GetVersion() != 13)
        return MX_E_InvalidData;

      //build protocols for callback
      nSize = sizeof(CClientRequest::WEBSOCKET_INFO);
      nCount = lpReqSecWebSocketProtocolHeader->GetProtocolsCount();
      nSize += (nCount + 1) * sizeof(LPCSTR);
      for (i = 0; i < nCount; i++)
      {
        nSize += sizeof(LPCSTR) + StrLenA(lpReqSecWebSocketProtocolHeader->GetProtocol(i)) + 1;
      }
      lpRequest->cWebSocketInfo.Attach((CClientRequest::LPWEBSOCKET_INFO)MX_MALLOC(nSize));
      if (!(lpRequest->cWebSocketInfo))
        return E_OUTOFMEMORY;
      lpRequest->cWebSocketInfo->lpReqSecWebSocketKeyHeader = lpReqSecWebSocketKeyHeader;

      nOffset = sizeof(CClientRequest::WEBSOCKET_INFO) + (nCount + 1) * sizeof(LPCSTR);
      for (i = 0; i < nCount; i++)
      {
        lpRequest->cWebSocketInfo->szProtocolsA[i] = (LPCSTR)((LPBYTE)(lpRequest->cWebSocketInfo.Get()) + nOffset);

        sA = lpReqSecWebSocketProtocolHeader->GetProtocol(i);
        nSize = StrLenA(sA) + 1;
        MemCopy((LPSTR)(lpRequest->cWebSocketInfo->szProtocolsA[i]), sA, nSize);
        nOffset += nSize;
      }
      lpRequest->cWebSocketInfo->szProtocolsA[nCount] = NULL;

      //done
      return S_OK;
    }
  }
  //done
  return S_FALSE;
}

HRESULT CHttpServer::InitiateWebSocket(_In_ CClientRequest *lpRequest, _In_ WEBSOCKET_REQUEST_CALLBACK_DATA &sData,
                                       _Out_ BOOL &bFatal)
{
  TAutoRefCounted<CWebSocket> cWebSocket;
  int nProtocolsCount;
  HRESULT hRes;

  bFatal = FALSE;
  cWebSocket.Attach(sData.lpWebSocket);
  if (!cWebSocket)
    return E_POINTER;

  //setup response
  for (nProtocolsCount = 0; lpRequest->cWebSocketInfo->szProtocolsA[nProtocolsCount] != NULL; nProtocolsCount++);
  if (sData.nSelectedProtocol < 0)
    return MX_E_Unsupported;
  if (sData.nSelectedProtocol >= nProtocolsCount)
    return MX_E_InvalidData;

  //pause processing
  hRes = lpRequest->lpSocketMgr->PauseInputProcessing(lpRequest->hConn);
  if (FAILED(hRes))
    return hRes;

  //send headers
  hRes = lpRequest->BuildAndSendWebSocketHeaderStream(sData.lpszProtocolsA[sData.nSelectedProtocol]);
  if (FAILED(hRes))
  {
    if (FAILED(lpRequest->lpSocketMgr->ResumeInputProcessing(lpRequest->hConn)))
      bFatal = TRUE;
    return hRes;
  }

  //if we reach here, an error is fatal and leads to a hard close
  bFatal = TRUE;

  //set websocket parameters
  cWebSocket->lpIpc = lpRequest->lpSocketMgr;
  cWebSocket->hConn = lpRequest->hConn;
  cWebSocket->bServerSide = TRUE;

  //setup new connections callbacks
  hRes = lpRequest->lpSocketMgr->SetDataReceivedCallback(lpRequest->hConn,
                                                         MX_BIND_MEMBER_CALLBACK(&CWebSocket::OnSocketDataReceived,
                                                                                 cWebSocket.Get()));
  if (FAILED(hRes))
    return hRes;
  hRes = lpRequest->lpSocketMgr->SetDestroyCallback(lpRequest->hConn,
                                                    MX_BIND_MEMBER_CALLBACK(&CWebSocket::OnSocketDestroy,
                                                                            cWebSocket.Get()));
  if (FAILED(hRes))
    return hRes;

  //replace the client request object with the provided user data object
  hRes = lpRequest->lpSocketMgr->SetUserData(lpRequest->hConn, cWebSocket.Detach());
  if (FAILED(hRes))
    return hRes;

  //set new state, just to avoid some background task doing stuff on client request
  hRes = lpRequest->SetState(CClientRequest::StateWebSocket);
  if (FAILED(hRes))
    return hRes;

  //resume input processing
  hRes = lpRequest->lpSocketMgr->ResumeInputProcessing(lpRequest->hConn);
  if (FAILED(hRes))
    return hRes;

  //at this point, http server does not longer control the connection
  return S_OK;
}

VOID CHttpServer::OnRequestDestroyed(_In_ CClientRequest *lpRequest)
{
  CAutoSlimRWLExclusive cListLock(&nRequestsListRwMutex);

  lpRequest->RemoveNode();
  return;
}

VOID CHttpServer::OnRequestEnding(_In_ CClientRequest *lpRequest, _In_ HRESULT hrErrorCode)
{
  HRESULT hRes = hrErrorCode;

  lpRequest->cRequest->dwHeadersTimeoutCounterSecs = 0;
  //----
  if (SUCCEEDED(hRes))
  {
    hRes = lpRequest->SetState(CClientRequest::StateSendingResponse);
  }
  if (lpRequest->OnCleanup() == FALSE)
  {
    _InterlockedOr(&(lpRequest->nFlags), REQUEST_FLAG_DontKeepAlive);
  }
  if (lpRequest->cResponse->bDirect == FALSE)
  {
    if (SUCCEEDED(hRes) && lpRequest->HasErrorBeenSent() == FALSE)
      hRes = lpRequest->BuildAndInsertOrSendHeaderStream();
    if (SUCCEEDED(hRes))
      hRes = lpRequest->SendQueuedStreams();
  }
  if (SUCCEEDED(hRes))
  {
    if (lpRequest->IsKeepAliveRequest() == FALSE)
    {
      _InterlockedOr(&(lpRequest->nFlags), REQUEST_FLAG_DontKeepAlive);
    }
    hRes = cSocketMgr.AfterWriteSignal(lpRequest->hConn,
                                        MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnAfterSendResponse, this), lpRequest);
  }
  if (FAILED(hRes))
  {
    lpRequest->SetState(CClientRequest::StateError);
    cSocketMgr.Close(lpRequest->hConn, hRes);
    if (ShouldLog(1) != FALSE)
    {
      Log(L"HttpServer(State/Req:0x%p/Conn:0x%p): StateError [%08X]", lpRequest, lpRequest->hConn, hrErrorCode);
    }
  }
  return;
}

HRESULT CHttpServer::OnDownloadStarted(_Out_ LPHANDLE lphFile, _In_z_ LPCWSTR szFileNameW, _In_ LPVOID lpUserParam)
{
  CClientRequest *lpRequest = (CClientRequest*)lpUserParam;
  MX::CStringW cStrFileNameW;
  SIZE_T nLen;
  DWORD dw;
  Fnv64_t nHash;
  HRESULT hRes;

  //generate filename
  if (cStrTemporaryFolderW.IsEmpty() != FALSE)
  {
    hRes = CHttpCommon::_GetTempPath(cStrTemporaryFolderW);
    if (FAILED(hRes))
      return hRes;
  }
  else
  {
    nLen = cStrTemporaryFolderW.GetLength();
    if (cStrFileNameW.CopyN((LPCWSTR)cStrTemporaryFolderW, nLen) == FALSE)
      return E_OUTOFMEMORY;
    if (nLen > 0 && ((LPWSTR)cStrFileNameW)[nLen - 1] != L'\\')
    {
      if (cStrFileNameW.Concat(L"\\") == FALSE)
        return E_OUTOFMEMORY;
    }
  }

  dw = (DWORD)_InterlockedIncrement(&nDownloadNameGeneratorCounter);
  nHash = fnv_64a_buf(&dw, sizeof(dw), FNV1A_64_INIT);
  dw = ::GetCurrentProcessId();
  nHash = fnv_64a_buf(&dw, sizeof(dw), nHash);
#pragma warning(suppress : 28159)
  dw = ::GetTickCount();
  nHash = fnv_64a_buf(&dw, sizeof(dw), nHash);
  nLen = (SIZE_T)this;
  nHash = fnv_64a_buf(&nLen, sizeof(nLen), nHash);
  nHash = fnv_64a_buf(lpRequest, sizeof(CClientRequest), nHash);
  nHash = fnv_64a_buf(&szFileNameW, sizeof(LPCWSTR), nHash);
  nHash = fnv_64a_buf(szFileNameW, MX::StrLenW(szFileNameW) * 2, nHash);
  if (cStrFileNameW.AppendFormat(L"tmp%016I64x", (ULONGLONG)nHash) == FALSE)
    return E_OUTOFMEMORY;

  //create temporary file
  *lphFile = ::CreateFileW((LPCWSTR)cStrFileNameW, GENERIC_READ | GENERIC_WRITE,
#ifdef _DEBUG
                           FILE_SHARE_READ,
#else //_DEBUG
                           0,
#endif //_DEBUG
                           NULL, CREATE_ALWAYS,
#ifdef _DEBUG
                           FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
#else //_DEBUG
                           FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
#endif //_DEBUG
                           NULL);
  if ((*lphFile) == NULL || (*lphFile) == INVALID_HANDLE_VALUE)
  {
    *lphFile = NULL;
    return MX_HRESULT_FROM_LASTERROR();
  }
  //done
  return S_OK;
}

} //namespace MX

//-----------------------------------------------------------

static LONG DecrementIfGreaterThanZero(_Inout_ LONG volatile *lpnValue)
{
  LONG nInitVal, nOrigVal, nNewVal;

  nOrigVal = __InterlockedRead(lpnValue);
  do
  {
    nInitVal = nOrigVal;
    nNewVal = (nInitVal > 0) ? (nInitVal - 1) : nInitVal;
    nOrigVal = _InterlockedCompareExchange(lpnValue, nNewVal, nInitVal);
  }
  while (nOrigVal != nInitVal);
  return nNewVal;
}
