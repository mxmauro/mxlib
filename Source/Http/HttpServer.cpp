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

namespace MX {

CHttpServer::CHttpServer(__in CSockets &_cSocketMgr, __in CPropertyBag &_cPropBag) :
             CBaseMemObj(), CCriticalSection(), cSocketMgr(_cSocketMgr), cPropBag(_cPropBag)
{
  cPropBag.GetDWord(MX_HTTP_SERVER_RequestTimeoutMs, dwRequestTimeoutMs, MX_HTTP_SERVER_RequestTimeoutMs_DEFVAL);
  if (dwRequestTimeoutMs < 1000)
    dwRequestTimeoutMs = 1000;
  //----
  SlimRWL_Initialize(&(sSsl.nRwMutex));
  sSsl.nProtocol = CIpcSslLayer::ProtocolUnknown;
  lpTimedEventQueue = NULL;
  RundownProt_Initialize(&nRundownLock);
  hAcceptConn = NULL;
  cRequestHeadersReceivedCallback = NullCallback();
  cRequestCompletedCallback = NullCallback();
  cErrorCallback = NullCallback();
  _InterlockedExchange(&nRequestsMutex, 0);
  return;
}

CHttpServer::~CHttpServer()
{
  TLnkLst<CRequest>::Iterator it;
  CRequest *lpRequest;
  BOOL b;

  RundownProt_WaitForRelease(&nRundownLock);
  //stop current server
  StopListening();
  //terminate current connections and wait
  do
  {
    {
      CFastLock cRequestsListLock(&nRequestsMutex);

      b = cRequestsList.IsEmpty();
      for (lpRequest=it.Begin(cRequestsList); lpRequest!=NULL; lpRequest=it.Next())
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
  //release queue
  if (lpTimedEventQueue != NULL)
    lpTimedEventQueue->Release();
  return;
}

VOID CHttpServer::On(__in OnRequestHeadersReceivedCallback _cRequestHeadersReceivedCallback)
{
  cRequestHeadersReceivedCallback = _cRequestHeadersReceivedCallback;
  return;
}

VOID CHttpServer::On(__in OnRequestCompletedCallback _cRequestCompletedCallback)
{
  cRequestCompletedCallback = _cRequestCompletedCallback;
  return;
}

VOID CHttpServer::On(__in OnErrorCallback _cErrorCallback)
{
  cErrorCallback = _cErrorCallback;
  return;
}

HRESULT CHttpServer::StartListening(__in int nPort, __in_opt CIpcSslLayer::eProtocol nProtocol,
                                    __in_opt CSslCertificate *lpSslCertificate, __in_opt CCryptoRSA *lpSslKey)
{
  return StartListening((LPCSTR)NULL, nPort, nProtocol, lpSslCertificate, lpSslKey);
}

HRESULT CHttpServer::StartListening(__in_z LPCSTR szBindAddressA, __in int nPort,
                                    __in_opt CIpcSslLayer::eProtocol nProtocol,
                                    __in_opt CSslCertificate *lpSslCertificate, __in_opt CCryptoRSA *lpSslKey)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  HRESULT hRes;

  switch (nProtocol)
  {
    case CIpcSslLayer::ProtocolUnknown:
      break;
    case CIpcSslLayer::ProtocolSSLv3:
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
  {
    CCriticalSection::CAutoLock cLock(*this);

    //set SSL
    hRes = S_OK;
    if (nProtocol != CIpcSslLayer::ProtocolUnknown)
    {
      CAutoSlimRWLExclusive cSslLock(&(sSsl.nRwMutex));

      sSsl.nProtocol = nProtocol;
      sSsl.cSslCertificate.Attach(MX_DEBUG_NEW CSslCertificate());
      if (sSsl.cSslCertificate)
        hRes = ((*sSsl.cSslCertificate.Get()) = (*lpSslCertificate));
      else
        hRes = E_OUTOFMEMORY;
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
    //get system timeout queue manager
    if (SUCCEEDED(hRes) && lpTimedEventQueue == NULL)
    {
      lpTimedEventQueue = CSystemTimedEventQueue::Get();
      if (lpTimedEventQueue == NULL)
        hRes = E_OUTOFMEMORY;
    }
    if (SUCCEEDED(hRes))
    {
      hRes = cSocketMgr.CreateListener(CSockets::FamilyUnknown, nPort,
                                       MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnSocketCreate, this), szBindAddressA,
                                       NULL, &hAcceptConn);
    }
    if (FAILED(hRes))
    {
      CAutoSlimRWLExclusive cSslLock(&(sSsl.nRwMutex));

      sSsl.nProtocol = CIpcSslLayer::ProtocolUnknown;
      sSsl.cSslCertificate.Reset();
      sSsl.cSslPrivateKey.Reset();
    }
  }
  //done
  return hRes;
}

HRESULT CHttpServer::StartListening(__in_z LPCWSTR szBindAddressW, __in int nPort,
                                    __in_opt CIpcSslLayer::eProtocol nProtocol,
                                    __in_opt CSslCertificate *lpSslCertificate, __in_opt CCryptoRSA *lpSslKey)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (szBindAddressW != NULL && szBindAddressW[0] != 0)
  {
    hRes = Punycode_Encode(cStrTempA, szBindAddressW);
    if (FAILED(hRes))
      return hRes;
  }
  return StartListening((LPSTR)cStrTempA, nPort, nProtocol, lpSslCertificate, lpSslKey);
}

VOID CHttpServer::StopListening()
{
  BOOL bWait = FALSE;

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
  return;
}

HRESULT CHttpServer::OnSocketCreate(__in CIpc *lpIpc, __in HANDLE h, __deref_inout CIpc::CUserData **lplpUserData,
                                    __inout CIpc::CREATE_CALLBACK_DATA &sData)
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
      TAutoRefCounted<CRequest> cNewRequest;
      TAutoDeletePtr<CIpcSslLayer> cLayer;
      HRESULT hRes;

      //setup callbacks
      sData.cConnectCallback = MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnSocketConnect, this);
      sData.cDataReceivedCallback = MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnSocketDataReceived, this);
      sData.cDestroyCallback = MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnSocketDestroy, this);
      //create new request object
      cNewRequest.Attach(MX_DEBUG_NEW CRequest(this, cPropBag));
      if (!cNewRequest)
        return E_OUTOFMEMORY;
      cNewRequest->lpSocketMgr = &cSocketMgr;
      cNewRequest->hConn = h;
      //add ssl layer
      {
        CAutoSlimRWLShared cSslLock(&(sSsl.nRwMutex));

        if (sSsl.nProtocol != CIpcSslLayer::ProtocolUnknown)
        {
          cLayer.Attach(MX_DEBUG_NEW CIpcSslLayer());
          if (!cLayer)
            return E_OUTOFMEMORY;
          hRes = cLayer->Initialize(TRUE, sSsl.nProtocol, NULL, sSsl.cSslCertificate.Get(), sSsl.cSslPrivateKey.Get());
          if (FAILED(hRes))
            return hRes;
          sData.cLayersList.PushTail(cLayer.Detach());
        }
      }
      //add to list
      {
        CFastLock cListLock(&nRequestsMutex);

        cRequestsList.PushTail(cNewRequest.Get());
      }
      *lplpUserData = cNewRequest.Detach();
      }
      break;
  }
  //done
  return S_OK;
}

VOID CHttpServer::OnListenerSocketDestroy(__in CIpc *lpIpc, __in HANDLE h, __in CIpc::CUserData *lpUserData,
                                          __in HRESULT hErrorCode)
{
  CCriticalSection::CAutoLock cLock(*this);

  if (h == hAcceptConn)
    hAcceptConn = NULL;
  return;
}

VOID CHttpServer::OnSocketDestroy(__in CIpc *lpIpc, __in HANDLE h, __in CIpc::CUserData *lpUserData,
                                  __in HRESULT hErrorCode)
{
  CRequest *lpRequest = (CRequest*)lpUserData;

  MX_ASSERT(lpUserData != NULL);
  //mark link as closed
  lpRequest->MarkLinkAsClosed();
  //cancel all timeout events
  CancelAllTimeoutEvents(lpRequest);
  //raise error notification
  if (FAILED(hErrorCode) && cErrorCallback)
    cErrorCallback(this, NULL, hErrorCode);
  return;
}

HRESULT CHttpServer::OnSocketConnect(__in CIpc *lpIpc, __in HANDLE h, __in CIpc::CUserData *lpUserData,
                                     __inout CIpc::CLayerList &cLayersList, __in HRESULT hErrorCode)
{
  CRequest *lpRequest = (CRequest*)lpUserData;
  HRESULT hRes;

  MX_ASSERT(lpUserData != NULL);
  //setup header timeout
  ::MxNtQuerySystemTime(&(lpRequest->sRequest.liLastRead));
  hRes = SetupTimeoutEvent(lpRequest, dwRequestTimeoutMs);
  //done
  return hRes;
}

HRESULT CHttpServer::OnSocketDataReceived(__in CIpc *lpIpc, __in HANDLE h, __in CIpc::CUserData *lpUserData)
{
  CRequest *lpRequest = (CRequest*)lpUserData;
  BYTE aMsgBuf[4096];
  SIZE_T nMsgSize;
  BOOL bFireRequestHeadersReceivedCallback, bFireRequestCompleted;
  HRESULT hRes;

  MX_ASSERT(lpUserData != NULL);
  hRes = S_OK;
  ::MxNtQuerySystemTime(&(lpRequest->sRequest.liLastRead));
restart:
  bFireRequestHeadersReceivedCallback = bFireRequestCompleted = FALSE;
  {
    CFastLock cLock(&(lpRequest->nMutex));
    CHttpCommon::eState nParserState;
    SIZE_T nMsgUsed;

    //loop
    while (SUCCEEDED(hRes) && bFireRequestHeadersReceivedCallback == FALSE && bFireRequestCompleted == FALSE)
    {
      //get buffered message
      nMsgSize = sizeof(aMsgBuf);
      hRes = cSocketMgr.GetBufferedMessage(h, aMsgBuf, &nMsgSize);
      if (SUCCEEDED(hRes) && nMsgSize == 0)
        break;
      if (FAILED(hRes))
      {
        CancelAllTimeoutEvents(lpRequest);
        hRes = QuickSendErrorResponseAndReset(lpRequest, 500, hRes);
        break;
      }
      //take action depending on current state
      switch (lpRequest->nState)
      {
        case CRequest::StateReceivingRequestHeaders:
        case CRequest::StateReceivingRequestBody:
          //process http being received
          hRes = lpRequest->sRequest.cHttpCmn.Parse(aMsgBuf, nMsgSize, nMsgUsed);
          if (SUCCEEDED(hRes) && nMsgUsed > 0)
            hRes = cSocketMgr.ConsumeBufferedMessage(h, nMsgUsed);
          if (SUCCEEDED(hRes))
          {
            //take action if parser's state changed
            nParserState = lpRequest->sRequest.cHttpCmn.GetParserState();
            switch (nParserState)
            {
              case CHttpCommon::StateBodyStart:
                bFireRequestHeadersReceivedCallback = TRUE;
                lpRequest->nState = CRequest::StateReceivingRequestBody;
                break;

              case CHttpCommon::StateDone:
                CancelAllTimeoutEvents(lpRequest);
                bFireRequestCompleted = TRUE;
                if (lpRequest->nState == CRequest::StateReceivingRequestHeaders)
                  bFireRequestHeadersReceivedCallback = TRUE;
                lpRequest->nState = CRequest::StateBuildingResponse;
                break;
            }
          }
          else if (hRes == HRESULT_FROM_WIN32(ERROR_REQ_NOT_ACCEP))
          {
            lpRequest->nState = CRequest::StateIgnoringRequest400;
            lpRequest->sRequest.cHttpCmn.SetParserIgnoreFlag();
            if (lpRequest->sRequest.cHttpCmn.GetParserState() == CHttpCommon::StateDone)
              goto ignoring_request_continue;
            hRes = S_OK;
          }
          else if (hRes == MX_E_BadLength)
          {
            lpRequest->nState = CRequest::StateIgnoringRequest413;
            lpRequest->sRequest.cHttpCmn.SetParserIgnoreFlag();
            if (lpRequest->sRequest.cHttpCmn.GetParserState() == CHttpCommon::StateDone)
              goto ignoring_request_continue;
            hRes = S_OK;
          }
          else
          {
            CancelAllTimeoutEvents(lpRequest);
            hRes = QuickSendErrorResponseAndReset(lpRequest, (hRes == E_INVALIDARG ||
                                                              hRes == MX_E_InvalidData) ? 400 : 500, hRes);
          }
          break;

        case CRequest::StateIgnoringRequest400:
        case CRequest::StateIgnoringRequest413:
ignoring_request_continue:
          //process http being received
          hRes = lpRequest->sRequest.cHttpCmn.Parse(aMsgBuf, nMsgSize, nMsgUsed);
          if (SUCCEEDED(hRes) && nMsgUsed > 0)
            hRes = cSocketMgr.ConsumeBufferedMessage(h, nMsgUsed);
          if (FAILED(hRes))
          {
            CancelAllTimeoutEvents(lpRequest);
            hRes = QuickSendErrorResponseAndReset(lpRequest, 500, hRes);
            break;
          }
          //take action if parser's state changed
          nParserState = lpRequest->sRequest.cHttpCmn.GetParserState();
          if (nParserState == CHttpCommon::StateDone)
          {
            CancelAllTimeoutEvents(lpRequest);
            hRes = QuickSendErrorResponseAndReset(lpRequest,
                                                  (lpRequest->nState == CRequest::StateIgnoringRequest400) ? 400 :
                                                  413, hRes, FALSE);
          }
          break;

        case CRequest::StateBuildingResponse:
          //we should not receive data after receiving the request, except some extra \r\n
          hRes = cSocketMgr.ConsumeBufferedMessage(h, nMsgSize);
          for (nMsgUsed=0; nMsgUsed<nMsgSize && (aMsgBuf[nMsgUsed] == '\r' || aMsgBuf[nMsgUsed] == '\n'); nMsgUsed++);
          if (nMsgUsed < nMsgSize)
          {
            CancelAllTimeoutEvents(lpRequest);
            _InterlockedOr(&(lpRequest->nFlags), REQUEST_FLAG_DontKeepAlive);
          }
          break;

        default:
        //case CRequest::StateClosed:
        //case CRequest::StateError:
          hRes = cSocketMgr.ConsumeBufferedMessage(h, nMsgSize);
          break;
      }
    }
  }

  //have some action to do?
  if (SUCCEEDED(hRes) && bFireRequestHeadersReceivedCallback != FALSE)
  {
    CHttpBodyParserBase *lpBodyParser = NULL;

    if (cRequestHeadersReceivedCallback)
      hRes = cRequestHeadersReceivedCallback(this, lpRequest, lpBodyParser);
    {
      CFastLock cLock(&(lpRequest->nMutex));

      if (SUCCEEDED(hRes) && bFireRequestCompleted == FALSE)
      {
        TAutoRefCounted<CHttpBodyParserBase> cBodyParser;

        cBodyParser.Attach(lpBodyParser);
        if (!cBodyParser)
        {
          cBodyParser.Attach(lpRequest->sRequest.cHttpCmn.GetDefaultBodyParser());
          if (!cBodyParser)
            hRes = E_OUTOFMEMORY;
        }
        if (SUCCEEDED(hRes))
          hRes = lpRequest->sRequest.cHttpCmn.SetBodyParser(cBodyParser.Get(), cPropBag);
      }
      if (FAILED(hRes))
      {
        CancelAllTimeoutEvents(lpRequest);
        QuickSendErrorResponseAndReset(lpRequest, 500, hRes, TRUE); //ignore return code
      }
    }
  }
  if (SUCCEEDED(hRes) && bFireRequestCompleted != FALSE)
  {
    if (cRequestCompletedCallback)
      hRes = cRequestCompletedCallback(this, lpRequest);
    {
      CFastLock cLock(&(lpRequest->nMutex));

      if (SUCCEEDED(hRes) && lpRequest->HasErrorBeenSent() == FALSE)
        hRes = lpRequest->BuildAndInsertHeaderStream();
      if (SUCCEEDED(hRes))
        hRes = SendStreams(lpRequest, FALSE);
      else
        cSocketMgr.Close(lpRequest->hConn, hRes);
    }
  }
  //restart on success
  if (SUCCEEDED(hRes))
  {
    if (bFireRequestHeadersReceivedCallback != FALSE || bFireRequestCompleted != FALSE)
      goto restart;
  }
  //raise error event if any
  if (FAILED(hRes) && cErrorCallback)
    cErrorCallback(this, lpRequest, hRes);
  return hRes;
}

HRESULT CHttpServer::QuickSendErrorResponseAndReset(__in CRequest *lpRequest, __in LONG nErrorCode,
                                                    __in HRESULT hErrorCode, __in_opt BOOL bForceClose)
{
  HRESULT hRes;

  hRes = SendGenericErrorPage(lpRequest, nErrorCode, hErrorCode);
  if (SUCCEEDED(hRes))
  {
    hRes = SendStreams(lpRequest, bForceClose);
  }
  else
  {
    cSocketMgr.Close(lpRequest->hConn, hRes);
  }
  //done
  lpRequest->nState = CRequest::StateError;
  return hRes;
}

VOID CHttpServer::OnRequestTimeout(__in CTimedEventQueue::CEvent *lpEvent)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  CRequest *lpRequest = (CRequest*)(lpEvent->GetUserData());
  ULARGE_INTEGER liCurrTime;
  DWORD dwDiffMs;
  HRESULT hRes;

  if (cAutoRundownProt.IsAcquired() != FALSE && lpEvent->IsCanceled() == FALSE)
  {
    {
      CFastLock cLock(&(lpRequest->nMutex));

      ::MxNtQuerySystemTime(&liCurrTime);
      dwDiffMs = 0;
      if (liCurrTime.QuadPart > lpRequest->sRequest.liLastRead.QuadPart)
        dwDiffMs = (DWORD)MX_100NS_TO_MILLISECONDS(liCurrTime.QuadPart - lpRequest->sRequest.liLastRead.QuadPart);
      if (dwDiffMs > dwRequestTimeoutMs)
        hRes = QuickSendErrorResponseAndReset(lpRequest, 408, S_OK);
      else if (lpRequest->IsLinkClosed() == FALSE)
        hRes = SetupTimeoutEvent(lpRequest, dwRequestTimeoutMs-dwDiffMs);
      else
        hRes = S_OK;
    }
    //raise error event if any
    if (FAILED(hRes) && cErrorCallback)
      cErrorCallback(this, lpRequest, hRes);
  }
  //delete the event and remove from pending list
  {
    CFastLock cRequestTimeoutLock(&(lpRequest->sRequest.sTimeout.nMutex));

    lpRequest->sRequest.sTimeout.cActiveList.Remove(lpEvent);
  }
  lpEvent->Release();
  lpRequest->Release();
  return;
}

VOID CHttpServer::OnAfterSendResponse(__in CIpc *lpIpc, __in HANDLE h, __in LPVOID lpCookie,
                                      __in CIpc::CUserData *lpUserData)
{
  CRequest *lpRequest = (CRequest*)lpUserData;

  {
    CFastLock cLock(&(lpRequest->nMutex));

    if (lpRequest->IsKeepAliveRequest() != FALSE)
    {
      lpRequest->ResetForNewRequest();
    }
    else
    {
      cSocketMgr.Close(lpRequest->hConn);
    }
  }
  return;
}

HRESULT CHttpServer::SendStreams(__in CRequest *lpRequest, __in BOOL bForceClose)
{
  CStream *lpStream;
  SIZE_T i, nCount;
  HRESULT hRes;

  hRes = S_OK;
  nCount = lpRequest->sResponse.aStreamsList.GetCount();
  for (i=0; SUCCEEDED(hRes) && i<nCount; i++)
  {
    lpStream = lpRequest->sResponse.aStreamsList.GetElementAt(i);
    hRes = lpStream->Seek(0, CStream::SeekStart);
    if (SUCCEEDED(hRes))
      hRes = cSocketMgr.SendStream(lpRequest->hConn, lpStream);
  }
  if (bForceClose == FALSE && SUCCEEDED(hRes))
  {
    hRes = cSocketMgr.AfterWriteSignal(lpRequest->hConn,
                                        MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnAfterSendResponse, this), lpRequest);
  }
  if (bForceClose != FALSE || FAILED(hRes))
  {
    cSocketMgr.Close(lpRequest->hConn, hRes);
    lpRequest->nState = CRequest::StateError;
  }
  //done
  return hRes;
}

HRESULT CHttpServer::SendGenericErrorPage(__in CRequest *lpRequest, __in LONG nErrorCode, __in HRESULT hErrorCode,
                                          __in_z_opt LPCSTR szBodyExplanationA)
{
  CMemoryStream *lpStream;
  CDateTime cDtNow;
  CStringA cStrTempA, cStrBodyA;
  LPCSTR szErrMsgA;
  SIZE_T nWritten;
  BOOL bHasBody;
  int nHttpVersion[2];
  HRESULT hRes;

  szErrMsgA = LocateError(nErrorCode);
  if (szErrMsgA == NULL)
    return E_INVALIDARG;
  bHasBody = TRUE;
  if (*szErrMsgA == '*')
  {
    bHasBody = FALSE;
    szErrMsgA++;
  }
  nHttpVersion[0] = lpRequest->sRequest.cHttpCmn.GetRequestVersionMajor();
  nHttpVersion[1] = lpRequest->sRequest.cHttpCmn.GetRequestVersionMinor();
  if (nHttpVersion[0] == 0)
  {
    nHttpVersion[0] = 1;
    nHttpVersion[1] = 0;
  }
  if (cStrTempA.Format("HTTP/%d.%d %03d %s\r\nServer:%s\r\n", nHttpVersion[0], nHttpVersion[1], nErrorCode,
                       szErrMsgA, szServerInfoA) == FALSE)
    return E_OUTOFMEMORY;
  if (SUCCEEDED(hErrorCode))
  {
    if (cStrTempA.AppendFormat("X-ErrorCode:0x%08X\r\n", hErrorCode) == FALSE)
      return E_OUTOFMEMORY;
  }
  hRes = cDtNow.SetFromNow(FALSE);
  if (SUCCEEDED(hRes))
    hRes = cDtNow.AppendFormat(cStrTempA, "Date:%a, %d %b %Y %H:%M:%S %z\r\n");
  if (FAILED(hRes))
    return hRes;
  if (bHasBody != FALSE)
  {
    if (szBodyExplanationA == NULL)
      szBodyExplanationA = "";
    if (cStrBodyA.AppendFormat(szServerErrorFormatA, nErrorCode, szErrMsgA, nErrorCode, szErrMsgA,
                               szBodyExplanationA) == FALSE)
      return E_OUTOFMEMORY;
    if (cStrTempA.AppendFormat("Cache-Control:private\r\nContent-Length:%Iu\r\nContent-Type:text/html\r\n\r\n",
                               cStrBodyA.GetLength()) == FALSE)
      return E_OUTOFMEMORY;
    if (cStrTempA.Concat((LPSTR)cStrBodyA) == FALSE)
      return E_OUTOFMEMORY;
  }
  else
  {
    if (cStrTempA.ConcatN("\r\n", 2) == FALSE)
      return E_OUTOFMEMORY;
  }
  lpRequest->sResponse.aStreamsList.RemoveAllElements();
  //copy to stream
  lpStream = MX_DEBUG_NEW CMemoryStream();
  if (lpStream == NULL)
    return E_OUTOFMEMORY;
  hRes = lpStream->Create(cStrTempA.GetLength());
  if (SUCCEEDED(hRes))
    hRes = lpStream->Write((LPSTR)cStrTempA, cStrTempA.GetLength(), nWritten);
  if (SUCCEEDED(hRes))
  {
    if (lpRequest->sResponse.aStreamsList.AddElement(lpStream) == FALSE)
      hRes = E_OUTOFMEMORY;
  }
  if (FAILED(hRes))
    lpStream->Release();
  //done
  _InterlockedOr(&(lpRequest->nFlags), REQUEST_FLAG_ErrorPageSent);
  return hRes;
}

LPCSTR CHttpServer::LocateError(__in LONG nErrorCode)
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

HRESULT CHttpServer::SetupTimeoutEvent(__in CRequest *lpRequest, __in DWORD dwTimeoutMs)
{
  CFastLock cRequestTimeoutLock(&(lpRequest->sRequest.sTimeout.nMutex));
  CTimedEventQueue::CEvent *lpEvent;
  HRESULT hRes;

  lpEvent = MX_DEBUG_NEW CTimedEventQueue::CEvent(MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnRequestTimeout, this));
  if (lpEvent == NULL)
    return E_OUTOFMEMORY;
  lpEvent->SetUserData(lpRequest);
  //add to queues
  hRes = lpRequest->sRequest.sTimeout.cActiveList.Add(lpEvent);
  if (FAILED(hRes))
  {
    lpEvent->Release();
    return hRes;
  }
  hRes = lpTimedEventQueue->Add(lpEvent, dwRequestTimeoutMs);
  if (FAILED(hRes))
  {
    lpRequest->sRequest.sTimeout.cActiveList.Remove(lpEvent);
    lpEvent->Release();
    return hRes;
  }
  //done
  lpRequest->AddRef();
  return S_OK;
}

VOID CHttpServer::CancelAllTimeoutEvents(__in CRequest *lpRequest)
{
  CFastLock cRequestTimeoutLock(&(lpRequest->sRequest.sTimeout.nMutex));

  //cancel all queued events
  if (lpTimedEventQueue != NULL)
  {
    lpRequest->sRequest.sTimeout.cActiveList.RunAction(MX_BIND_MEMBER_CALLBACK(&CHttpServer::DoCancelEventsCallback,
                                                                               this));
  }
  return;
}

VOID CHttpServer::DoCancelEventsCallback(__in __TEventArray &cEventsList)
{
  SIZE_T i, nCount;

  nCount = cEventsList.GetCount();
  for (i=0; i<nCount; i++)
    lpTimedEventQueue->Remove(cEventsList.GetElementAt(i));
  return;
}

VOID CHttpServer::OnRequestDestroyed(__in CRequest *lpRequest)
{
  CFastLock cListLock(&nRequestsMutex);

  lpRequest->RemoveNode();
  return;
}

} //namespace MX
