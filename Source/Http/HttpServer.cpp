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

CHttpServer::CHttpServer(_In_ CSockets &_cSocketMgr, _In_ CIoCompletionPortThreadPool &_cWorkerPool) : CBaseMemObj(),
                         CCriticalSection(), cSocketMgr(_cSocketMgr), cWorkerPool(_cWorkerPool)
{
  cRequestCompletedWP = MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnRequestCompleted, this);
  //----
  dwMaxRequestTimeoutMs = 60000;
  dwMaxHeaderSize = 16384;
  dwMaxFieldSize = 256000;
  ullMaxFileSize = 2097152ui64;
  dwMaxFilesCount = 4;
  dwMaxBodySizeInMemory = 32768;
  ullMaxBodySize = 10485760ui64;
  //----
  SlimRWL_Initialize(&(sSsl.nRwMutex));
  sSsl.nProtocol = CIpcSslLayer::ProtocolUnknown;
  lpTimedEventQueue = NULL;
  RundownProt_Initialize(&nRundownLock);
  hAcceptConn = NULL;
  cNewRequestObjectCallback = NullCallback();
  cRequestHeadersReceivedCallback = NullCallback();
  cRequestCompletedCallback = NullCallback();
  cErrorCallback = NullCallback();
  _InterlockedExchange(&nRequestsMutex, 0);
  _InterlockedExchange(&nDownloadNameGeneratorCounter, 0);
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

VOID CHttpServer::SetOption_MaxRequestTimeoutMs(_In_ DWORD dwTimeoutMs)
{
  CCriticalSection::CAutoLock cLock(*this);

  if (hAcceptConn == NULL)
  {
    dwMaxRequestTimeoutMs = dwTimeoutMs;
    if (dwMaxRequestTimeoutMs < 1000)
      dwMaxRequestTimeoutMs = 1000;
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

VOID CHttpServer::On(_In_ OnNewRequestObjectCallback _cNewRequestObjectCallback)
{
  cNewRequestObjectCallback = _cNewRequestObjectCallback;
  return;
}

VOID CHttpServer::On(_In_ OnRequestHeadersReceivedCallback _cRequestHeadersReceivedCallback)
{
  cRequestHeadersReceivedCallback = _cRequestHeadersReceivedCallback;
  return;
}

VOID CHttpServer::On(_In_ OnRequestCompletedCallback _cRequestCompletedCallback)
{
  cRequestCompletedCallback = _cRequestCompletedCallback;
  return;
}

VOID CHttpServer::On(_In_ OnErrorCallback _cErrorCallback)
{
  cErrorCallback = _cErrorCallback;
  return;
}

HRESULT CHttpServer::StartListening(_In_ int nPort, _In_opt_ CIpcSslLayer::eProtocol nProtocol,
                                    _In_opt_ CSslCertificate *lpSslCertificate, _In_opt_ CCryptoRSA *lpSslKey)
{
  return StartListening((LPCSTR)NULL, nPort, nProtocol, lpSslCertificate, lpSslKey);
}

HRESULT CHttpServer::StartListening(_In_opt_z_ LPCSTR szBindAddressA, _In_ int nPort,
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

      cShutdownEv.Close();
    }
  }
  //done
  return hRes;
}

HRESULT CHttpServer::StartListening(_In_opt_z_ LPCWSTR szBindAddressW, _In_ int nPort,
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
  return StartListening((LPSTR)cStrTempA, nPort, nProtocol, lpSslCertificate, lpSslKey);
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
      TAutoRefCounted<CRequest> cNewRequest;
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
        cNewRequest.Attach(MX_DEBUG_NEW CRequest());
      }
      if (!cNewRequest)
        return E_OUTOFMEMORY;
      cNewRequest->lpHttpServer = this;
      cNewRequest->lpSocketMgr = &cSocketMgr;
      cNewRequest->hConn = h;
      cNewRequest->sRequest.cHttpCmn.SetOption_MaxHeaderSize(dwMaxHeaderSize);
      cNewRequest->sResponse.cHttpCmn.SetOption_MaxHeaderSize(dwMaxHeaderSize);
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
      sData.cUserData = cNewRequest;
      }
      break;
  }
  //done
  return S_OK;
}

VOID CHttpServer::OnListenerSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                                          _In_ HRESULT hErrorCode)
{
  CCriticalSection::CAutoLock cLock(*this);

  if (h == hAcceptConn)
    hAcceptConn = NULL;
  return;
}

VOID CHttpServer::OnSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                                  _In_ HRESULT hErrorCode)
{
  CRequest *lpRequest = (CRequest*)lpUserData;

  if (lpRequest != NULL)
  {
    CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

    if (lpRequest->hConn == h)
    {
      //mark link as closed
      lpRequest->MarkLinkAsClosed();
      //cancel all timeout events
      CancelAllTimeoutEvents(lpRequest);

      lpRequest->hConn = NULL;
    }
  }
  //raise error notification
  if (FAILED(hErrorCode) && cErrorCallback)
    cErrorCallback(this, NULL, hErrorCode);
  return;
}

HRESULT CHttpServer::OnSocketConnect(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                                     _Inout_ CIpc::CLayerList &cLayersList, _In_ HRESULT hErrorCode)
{
  CRequest *lpRequest = (CRequest*)lpUserData;
  HRESULT hRes;

  MX_ASSERT(lpUserData != NULL);
  //setup header timeout
  ::MxNtQuerySystemTime(&(lpRequest->sRequest.liLastRead));
  hRes = SetupTimeoutEvent(lpRequest, dwMaxRequestTimeoutMs);
  //done
  return hRes;
}

HRESULT CHttpServer::OnSocketDataReceived(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData)
{
  CRequest *lpRequest = (CRequest*)lpUserData;
  BYTE aMsgBuf[4096];
  SIZE_T nMsgSize;
  BOOL bFireRequestHeadersReceivedCallback, bFireRequestCompleted;
  HRESULT hRes;

  MX_ASSERT(lpUserData != NULL);

  if (lpRequest->nState == CRequest::StateInitializing)
  {
    hRes = lpRequest->OnSetup();
    if (FAILED(hRes))
      goto done;
    lpRequest->nState = CRequest::StateReceivingRequestHeaders;
    MX_HTTP_DEBUG_PRINT(1, ("HttpServer(State/0x%p): StateReceivingRequestHeaders\n", this));
  }

  hRes = S_OK;
  ::MxNtQuerySystemTime(&(lpRequest->sRequest.liLastRead));
restart:
  bFireRequestHeadersReceivedCallback = bFireRequestCompleted = FALSE;
  {
    CCriticalSection::CAutoLock cLock(lpRequest->cMutex);
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
        hRes = QuickSendErrorResponseAndReset(lpRequest, 500, hRes, TRUE);
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
                if (lpRequest->sRequest.cHttpCmn.GetErrorCode() == 0)
                  bFireRequestHeadersReceivedCallback = TRUE; //fire events only if no error
                lpRequest->nState = CRequest::StateReceivingRequestBody;
                MX_HTTP_DEBUG_PRINT(1, ("HttpServer(State/0x%p): StateReceivingRequestBody\n", this));
                break;

              case CHttpCommon::StateDone:
                CancelAllTimeoutEvents(lpRequest);
                //check if there was a parse error
                if (lpRequest->sRequest.cHttpCmn.GetErrorCode() != 0)
                {
                  hRes = (lpRequest->sRequest.cHttpCmn.GetErrorCode() == 413) ? MX_E_BadLength : MX_E_InvalidData;
                  hRes = QuickSendErrorResponseAndReset(lpRequest, lpRequest->sRequest.cHttpCmn.GetErrorCode(),
                                                        hRes, TRUE);
                  break;
                }
                //check if the uploaded body is too large
                if (lpRequest->nState == CRequest::StateReceivingRequestBody)
                {
                  TAutoRefCounted<CHttpBodyParserBase> cBodyParser;

                  cBodyParser.Attach(lpRequest->sRequest.cHttpCmn.GetBodyParser());
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
                if (lpRequest->nState == CRequest::StateReceivingRequestHeaders)
                {
                  bFireRequestHeadersReceivedCallback = TRUE;
                }
                lpRequest->nState = CRequest::StateBuildingResponse;
                MX_HTTP_DEBUG_PRINT(1, ("HttpServer(State/0x%p): StateBuildingResponse\n", this));
                break;
            }
          }
          else
          {
            CancelAllTimeoutEvents(lpRequest);
            hRes = QuickSendErrorResponseAndReset(lpRequest, (hRes == E_INVALIDARG ||
                                                              hRes == MX_E_InvalidData) ? 400 : 500, hRes, TRUE);
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
        //case CRequest::StateEnd:
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
    {
      cRequestHeadersReceivedCallback(this, lpRequest, cShutdownEv.Get(), &lpBodyParser);
    }
    {
      CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

      if (lpRequest->nState != CRequest::StateEnded)
      {
        TAutoRefCounted<CHttpBodyParserBase> cBodyParser;

        cBodyParser.Attach(lpBodyParser);
        if (!cBodyParser)
        {
          CHttpHeaderEntContentType *lpHeader = lpRequest->sRequest.cHttpCmn.GetHeader<CHttpHeaderEntContentType>();
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
          hRes = lpRequest->sRequest.cHttpCmn.SetBodyParser(cBodyParser.Get());
        }
        if (FAILED(hRes))
        {
          CancelAllTimeoutEvents(lpRequest);
          QuickSendErrorResponseAndReset(lpRequest, 500, hRes, TRUE); //ignore return code
        }
      }
    }
  }
  if (SUCCEEDED(hRes) && bFireRequestCompleted != FALSE)
  {
    CancelAllTimeoutEvents(lpRequest);
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
  //raise error event if any
  if (FAILED(hRes) && cErrorCallback)
    cErrorCallback(this, lpRequest, hRes);
  return hRes;
}

VOID CHttpServer::OnRequestCompleted(_In_ CIoCompletionPortThreadPool *lpPool, _In_ DWORD dwBytes,
                                     _In_ OVERLAPPED *lpOvr, _In_ HRESULT hRes)
{
  CRequest *lpRequest = (CRequest*)((char*)lpOvr - (char*)&(((CRequest*)0)->sOvr));

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

HRESULT CHttpServer::QuickSendErrorResponseAndReset(_In_ CRequest *lpRequest, _In_ LONG nErrorCode,
                                                    _In_ HRESULT hErrorCode, _In_ BOOL bForceClose)
{
  CStringA cStrResponseA;
  HRESULT hRes;

  hRes = GenerateErrorPage(cStrResponseA, lpRequest, nErrorCode, hErrorCode);
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
  if (bForceClose != FALSE || FAILED(hRes))
  {
    cSocketMgr.Close(lpRequest->hConn, hRes);
  }
  //done
  lpRequest->nState = CRequest::StateError;
  MX_HTTP_DEBUG_PRINT(1, ("HttpServer(State/0x%p): StateError [%ld/%08X]\n", this, nErrorCode, hErrorCode));
  return hRes;
}

VOID CHttpServer::OnRequestTimeout(_In_ CTimedEventQueue::CEvent *lpEvent)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  CRequest *lpRequest = (CRequest*)(lpEvent->GetUserData());
  ULARGE_INTEGER liCurrTime;
  DWORD dwDiffMs;
  HRESULT hRes;

  if (cAutoRundownProt.IsAcquired() != FALSE && lpEvent->IsCanceled() == FALSE)
  {
    {
      CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

      ::MxNtQuerySystemTime(&liCurrTime);
      dwDiffMs = 0;
      if (liCurrTime.QuadPart > lpRequest->sRequest.liLastRead.QuadPart)
        dwDiffMs = (DWORD)MX_100NS_TO_MILLISECONDS(liCurrTime.QuadPart - lpRequest->sRequest.liLastRead.QuadPart);
      if (dwDiffMs > dwMaxRequestTimeoutMs)
        hRes = QuickSendErrorResponseAndReset(lpRequest, 408, S_OK, TRUE);
      else if (lpRequest->IsLinkClosed() == FALSE)
        hRes = SetupTimeoutEvent(lpRequest, dwMaxRequestTimeoutMs-dwDiffMs);
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
  //done
  lpEvent->Release();
  lpRequest->Release();
  return;
}

VOID CHttpServer::OnAfterSendResponse(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ LPVOID lpCookie,
                                      _In_ CIpc::CUserData *lpUserData)
{
  CRequest *lpRequest = (CRequest*)lpUserData;

  {
    CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

    CancelAllTimeoutEvents(lpRequest);
    if (lpRequest->nState != CRequest::StateError && lpRequest->IsKeepAliveRequest() != FALSE)
    {
      lpRequest->ResetForNewRequest();
    }
    else
    {
      MX_HTTP_DEBUG_PRINT(1, ("HttpServer(OnAfterSendResponse/0x%p): Closing\n", lpRequest));
      cSocketMgr.Close(lpRequest->hConn);
    }
  }
  return;
}

HRESULT CHttpServer::GenerateErrorPage(_Out_ CStringA &cStrResponseA, _In_ CRequest *lpRequest, _In_ LONG nErrorCode,
                                       _In_ HRESULT hErrorCode, _In_opt_z_ LPCSTR szBodyExplanationA)
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
  nHttpVersion[0] = lpRequest->sRequest.cHttpCmn.GetRequestVersionMajor();
  nHttpVersion[1] = lpRequest->sRequest.cHttpCmn.GetRequestVersionMinor();
  if (nHttpVersion[0] == 0)
  {
    nHttpVersion[0] = 1;
    nHttpVersion[1] = 0;
  }
  if (cStrResponseA.Format("HTTP/%d.%d %03d %s\r\nServer:%s\r\n", nHttpVersion[0], nHttpVersion[1], nErrorCode,
                           szErrMsgA, szServerInfoA) == FALSE)
  {
    return E_OUTOFMEMORY;
  }
  if (SUCCEEDED(hErrorCode))
  {
    if (cStrResponseA.AppendFormat("X-ErrorCode:0x%08X\r\n", hErrorCode) == FALSE)
      return E_OUTOFMEMORY;
  }
  hRes = cDtNow.SetFromNow(FALSE);
  if (SUCCEEDED(hRes))
    hRes = cDtNow.AppendFormat(cStrResponseA, "Date:%a, %d %b %Y %H:%M:%S %z\r\n");
  if (FAILED(hRes))
    return hRes;
  if (bHasBody != FALSE)
  {
    CStringA cStrBodyA;

    if (szBodyExplanationA == NULL)
      szBodyExplanationA = "";
    if (cStrBodyA.Format(szServerErrorFormatA, nErrorCode, szErrMsgA, nErrorCode, szErrMsgA,
                         szBodyExplanationA) == FALSE ||
        cStrResponseA.AppendFormat("Cache-Control:private\r\nContent-Length:%Iu\r\nContent-Type:text/html\r\n\r\n",
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

HRESULT CHttpServer::SetupTimeoutEvent(_In_ CRequest *lpRequest, _In_ DWORD dwTimeoutMs)
{
  CFastLock cRequestTimeoutLock(&(lpRequest->sRequest.sTimeout.nMutex));
  CTimedEventQueue::CEvent *lpEvent;
  HRESULT hRes;

  lpRequest->AddRef();
  lpEvent = MX_DEBUG_NEW CTimedEventQueue::CEvent(MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnRequestTimeout, this));
  if (lpEvent == NULL)
  {
    lpRequest->Release();
    return E_OUTOFMEMORY;
  }
  lpEvent->SetUserData(lpRequest);
  //add to queues
  hRes = lpRequest->sRequest.sTimeout.cActiveList.Add(lpEvent);
  if (FAILED(hRes))
  {
    lpEvent->Release();
    lpRequest->Release();
    return hRes;
  }
  hRes = lpTimedEventQueue->Add(lpEvent, dwMaxRequestTimeoutMs);
  if (FAILED(hRes))
  {
    lpRequest->sRequest.sTimeout.cActiveList.Remove(lpEvent);
    lpEvent->Release();
    lpRequest->Release();
    return hRes;
  }
  //done
  return S_OK;
}

VOID CHttpServer::CancelAllTimeoutEvents(_In_ CRequest *lpRequest)
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

VOID CHttpServer::DoCancelEventsCallback(_In_ __TEventArray &cEventsList)
{
  SIZE_T i, nCount;

  nCount = cEventsList.GetCount();
  for (i=0; i<nCount; i++)
    lpTimedEventQueue->Remove(cEventsList.GetElementAt(i));
  return;
}

VOID CHttpServer::OnRequestDestroyed(_In_ CRequest *lpRequest)
{
  CFastLock cListLock(&nRequestsMutex);

  lpRequest->RemoveNode();
  return;
}

VOID CHttpServer::OnRequestEnding(_In_ CRequest *lpRequest, _In_ HRESULT hErrorCode)
{
  HRESULT hRes = hErrorCode;

  CancelAllTimeoutEvents(lpRequest);
  if (lpRequest->sResponse.bDirect == FALSE)
  {
    if (SUCCEEDED(hRes) && lpRequest->HasErrorBeenSent() == FALSE)
      hRes = lpRequest->BuildAndInsertOrSendHeaderStream();
    if (SUCCEEDED(hRes))
      hRes = lpRequest->SendQueuedStreams();
  }
  if (SUCCEEDED(hRes))
  {
    hRes = cSocketMgr.AfterWriteSignal(lpRequest->hConn,
                                        MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnAfterSendResponse, this), lpRequest);
  }
  if (SUCCEEDED(hRes))
  {
    lpRequest->nState = CRequest::StateEnded;
    MX_HTTP_DEBUG_PRINT(1, ("HttpServer(State/0x%p): StateEnded\n", this));
  }
  else
  {
    cSocketMgr.Close(lpRequest->hConn, hRes);
    lpRequest->nState = CRequest::StateError;
    MX_HTTP_DEBUG_PRINT(1, ("HttpServer(State/0x%p): StateError [%08X]\n", this, hErrorCode));
  }
  lpRequest->OnCleanup();
  return;
}

HRESULT CHttpServer::OnDownloadStarted(_Out_ LPHANDLE lphFile, _In_z_ LPCWSTR szFileNameW, _In_ LPVOID lpUserParam)
{
  CRequest *lpRequest = (CRequest*)lpUserParam;
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
  nHash = fnv_64a_buf(lpRequest, sizeof(CRequest), nHash);
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
