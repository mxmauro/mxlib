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
#include "HttpServerCommon.h"
#include "..\..\Include\Http\HttpBodyParserDefault.h"
#include "..\..\Include\Http\HttpBodyParserMultipartFormData.h"
#include "..\..\Include\Http\HttpBodyParserUrlEncodedForm.h"
#include "..\..\Include\Http\HttpBodyParserJSON.h"
#include "..\..\Include\Http\HttpBodyParserIgnore.h"
#include "..\..\Include\Http\WebSockets.h"

//-----------------------------------------------------------

static const LPCSTR szServerInfoA = "MX-Library";

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
  LONG nBaseStatusCode;
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
static BOOL _GetTempPath(_Out_ MX::CStringW &cStrPathW);

//-----------------------------------------------------------

namespace MX {

CHttpServer::CHttpServer(_In_ CSockets &_cSocketMgr,
                         _In_opt_ CLoggable *lpLogParent) : CBaseMemObj(), CLoggable(), CNonCopyableObj(),
                                                            cSocketMgr(_cSocketMgr)
{
  SetLogParent(lpLogParent);
  //----
  dwMaxConnectionsPerIp = 0xFFFFFFFFUL;
  dwRequestHeaderTimeoutMs = 30000;
  dwGracefulTerminationTimeoutMs = 30000;
  dwKeepAliveTimeoutMs = 60000;
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
  dwMaxIncomingBytesWhileSending = 52428800; //50mb
  //----
  RundownProt_Initialize(&nRundownLock);
  hAcceptConn = NULL;
  cQuerySslCertificatesCallback = NullCallback();
  cNewRequestObjectCallback = NullCallback();
  cRequestHeadersReceivedCallback = NullCallback();
  cRequestCompletedCallback = NullCallback();
  cWebSocketRequestReceivedCallback = NullCallback();
  cRequestDestroyedCallback = NullCallback();
  cCustomErrorPageCallback = NullCallback();
  SlimRWL_Initialize(&sRequestsListRwMutex);
  _InterlockedExchange(&nDownloadNameGeneratorCounter, 0);
  //----
  SlimRWL_Initialize(&(sRequestLimiter.sRwMutex));
  return;
}

CHttpServer::~CHttpServer()
{
  TArrayListWithRelease<CClientRequest*> cFastClose;
  CLnkLst::Iterator it;
  CClientRequest *lpRequest;

  RundownProt_WaitForRelease(&nRundownLock);
  BOOL b;

  //stop current server
  StopListening();

  //terminate all requests
  b = TRUE;
  {
    CAutoSlimRWLShared cListLock(&sRequestsListRwMutex);

    if (cRequestsList.GetCount() > 0 && cFastClose.SetSize(cRequestsList.GetCount()) != FALSE)
    {
      for (CLnkLstNode *lpNode = it.Begin(cRequestsList); lpNode != NULL; lpNode = it.Next())
      {
        lpRequest = CONTAINING_RECORD(lpNode, CClientRequest, cListNode);
        b = cFastClose.AddElement(lpRequest);
        if (b == FALSE)
          break;
        lpRequest->AddRef();
      }
    }
  }

  if (b != FALSE)
  {
    //using fast close
    CClientRequest **lplpRequest = cFastClose.GetBuffer();
    for (SIZE_T nCount = cFastClose.GetCount(); nCount > 0; nCount--, lplpRequest++)
    {
      CCriticalSection::CAutoLock cLock((*lplpRequest)->cMutex);

      if ((*lplpRequest)->nState != CClientRequest::StateTerminated)
      {
        TerminateRequest((*lplpRequest), MX_E_Cancelled);
      }
    }

    cFastClose.RemoveAllElements();
  }
  else
  {
    //using slow path
    do
    {
      lpRequest = NULL;

      {
        CAutoSlimRWLShared cListLock(&sRequestsListRwMutex);

        for (CLnkLstNode *lpNode = it.Begin(cRequestsList); lpNode != NULL; lpNode = it.Next())
        {
          CClientRequest *_lpRequest = CONTAINING_RECORD(lpNode, CClientRequest, cListNode);
          CCriticalSection::CAutoLock cLock(_lpRequest->cMutex);

          if ((_InterlockedOr(&(_lpRequest->nFlags), REQUEST_FLAG_ClosingOnShutdown) &
               REQUEST_FLAG_ClosingOnShutdown) == 0 &&
              _lpRequest->nState != CClientRequest::StateTerminated)
          {
            lpRequest = _lpRequest;
            lpRequest->AddRef();

            TerminateRequest(lpRequest, MX_E_Cancelled);
            break;
          }
        }
      }

      if (lpRequest != NULL)
      {
        lpRequest->Release();
      }
    }
    while (lpRequest != NULL);
  }

  do
  {
    {
      CAutoSlimRWLShared cListLock(&sRequestsListRwMutex);

      b = cRequestsList.IsEmpty();
    }
    if (b == FALSE)
      ::MxSleep(10);
  }
  while (b == FALSE);

  //remove limiters
  {
    CAutoSlimRWLExclusive cLimiterLock(&(sRequestLimiter.sRwMutex));
    CRedBlackTreeNode *lpNode;

    while ((lpNode = sRequestLimiter.cTree.GetFirst()) != NULL)
    {
      CRequestLimiter *lpLimiter = CONTAINING_RECORD(lpNode, CRequestLimiter, cTreeNode);

      lpNode->Remove();
      delete lpLimiter;
    }
  }

  //done
  return;
}

VOID CHttpServer::SetOption_MaxConnectionsPerIp(_In_ DWORD dwLimit)
{
  CCriticalSection::CAutoLock cLock(cs);

  if (hAcceptConn == NULL)
  {
    dwMaxConnectionsPerIp = (dwLimit == 0) ? 0xFFFFFFFFUL : dwLimit;
  }
  return;
}

VOID CHttpServer::SetOption_RequestHeaderTimeout(_In_ DWORD dwTimeoutMs)
{
  CCriticalSection::CAutoLock cLock(cs);

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

VOID CHttpServer::SetOption_GracefulTerminationTimeout(_In_ DWORD dwTimeoutMs)
{
  CCriticalSection::CAutoLock cLock(cs);

  if (hAcceptConn == NULL)
  {
    if (dwTimeoutMs < 5 * 1000)
      dwGracefulTerminationTimeoutMs = 5 * 1000;
    else if (dwTimeoutMs > 60 * 60 * 1000)
      dwGracefulTerminationTimeoutMs = 60 * 60 * 1000;
    else
      dwGracefulTerminationTimeoutMs = dwTimeoutMs;
  }
  return;
}

VOID CHttpServer::SetOption_KeepAliveTimeout(_In_ DWORD dwTimeoutMs)
{
  CCriticalSection::CAutoLock cLock(cs);

  if (hAcceptConn == NULL)
  {
    if (dwTimeoutMs < 5 * 1000)
      dwKeepAliveTimeoutMs = 5 * 1000;
    else if (dwTimeoutMs > 60 * 60 * 1000)
      dwKeepAliveTimeoutMs = 60 * 60 * 1000;
    else
      dwKeepAliveTimeoutMs = dwTimeoutMs;
  }
  return;
}

VOID CHttpServer::SetOption_RequestBodyLimits(_In_ DWORD dwMinimumThroughputInBps, _In_ DWORD dwSecondsOfLowThroughput)
{
  CCriticalSection::CAutoLock cLock(cs);

  if (hAcceptConn == NULL)
  {
    dwRequestBodyMinimumThroughputInBps = dwMinimumThroughputInBps;
    dwRequestBodySecondsOfLowThroughput = dwSecondsOfLowThroughput;
  }
  return;
}

VOID CHttpServer::SetOption_ResponseLimits(_In_ DWORD dwMinimumThroughputInBps, _In_ DWORD dwSecondsOfLowThroughput)
{
  CCriticalSection::CAutoLock cLock(cs);

  if (hAcceptConn == NULL)
  {
    dwResponseMinimumThroughputInBps = dwMinimumThroughputInBps;
    dwResponseSecondsOfLowThroughput = dwSecondsOfLowThroughput;
  }
  return;
}

VOID CHttpServer::SetOption_MaxHeaderSize(_In_ DWORD dwSize)
{
  CCriticalSection::CAutoLock cLock(cs);

  if (hAcceptConn == NULL)
  {
    dwMaxHeaderSize = dwSize;
  }
  return;
}

VOID CHttpServer::SetOption_MaxFieldSize(_In_ DWORD dwSize)
{
  CCriticalSection::CAutoLock cLock(cs);

  if (hAcceptConn == NULL)
  {
    dwMaxFieldSize = dwSize;
  }
  return;
}

VOID CHttpServer::SetOption_MaxFileSize(_In_ ULONGLONG ullSize)
{
  CCriticalSection::CAutoLock cLock(cs);

  if (hAcceptConn == NULL)
  {
    ullMaxFileSize = ullSize;
  }
  return;
}

VOID CHttpServer::SetOption_MaxFilesCount(_In_ DWORD dwCount)
{
  CCriticalSection::CAutoLock cLock(cs);

  if (hAcceptConn == NULL)
  {
    dwMaxFilesCount = dwCount;
  }
  return;
}

BOOL CHttpServer::SetOption_TemporaryFolder(_In_opt_z_ LPCWSTR szFolderW)
{
  CCriticalSection::CAutoLock cLock(cs);

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
  CCriticalSection::CAutoLock cLock(cs);

  if (hAcceptConn == NULL)
  {
    dwMaxBodySizeInMemory = dwSize;
  }
  return;
}

VOID CHttpServer::SetOption_MaxBodySize(_In_ ULONGLONG ullSize)
{
  CCriticalSection::CAutoLock cLock(cs);

  if (hAcceptConn == NULL)
  {
    ullMaxBodySize = ullSize;
  }
  return;
}

VOID CHttpServer::SetOption_MaxIncomingBytesWhileSending(_In_ DWORD _dwMaxIncomingBytesWhileSending)
{
  CCriticalSection::CAutoLock cLock(cs);

  if (hAcceptConn == NULL)
  {
    dwMaxIncomingBytesWhileSending = (_dwMaxIncomingBytesWhileSending > 16384) ? _dwMaxIncomingBytesWhileSending
                                                                               : 16384;
  }
  return;
}

VOID CHttpServer::SetQuerySslCertificatesCallback(_In_ OnQuerySslCertificatesCallback _cQuerySslCertificatesCallback)
{
  cQuerySslCertificatesCallback = _cQuerySslCertificatesCallback;
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

VOID CHttpServer::SetRequestDestroyedCallback(_In_ OnRequestDestroyedCallback _cRequestDestroyedCallback)
{
  cRequestDestroyedCallback = _cRequestDestroyedCallback;
  return;
}

VOID CHttpServer::SetCustomErrorPageCallback(_In_ OnCustomErrorPageCallback _cCustomErrorPageCallback)
{
  cCustomErrorPageCallback = _cCustomErrorPageCallback;
  return;
}

HRESULT CHttpServer::StartListening(_In_ CSockets::eFamily nFamily, _In_ int nPort,
                                    _In_opt_ CSockets::LPLISTENER_OPTIONS lpOptions)
{
  return StartListening((LPCSTR)NULL, nFamily, nPort, lpOptions);
}

HRESULT CHttpServer::StartListening(_In_opt_z_ LPCSTR szBindAddressA, _In_ CSockets::eFamily nFamily, _In_ int nPort,
                                    _In_opt_ CSockets::LPLISTENER_OPTIONS lpOptions)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  HRESULT hRes;

  StopListening();
  if (cAutoRundownProt.IsAcquired() != FALSE)
  {
    CCriticalSection::CAutoLock cLock(cs);

    hRes = cShutdownEv.Create(TRUE, FALSE);
    if (SUCCEEDED(hRes))
    {
      hRes = cSocketMgr.CreateListener(nFamily, nPort, MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnSocketCreate, this),
                                        szBindAddressA, NULL, lpOptions, &hAcceptConn);
    }
    if (FAILED(hRes))
    {
      cShutdownEv.Close();
    }
  }
  else
  {
    hRes = MX_E_NotReady;
  }
  //done
  return hRes;
}

HRESULT CHttpServer::StartListening(_In_opt_z_ LPCWSTR szBindAddressW, _In_ CSockets::eFamily nFamily, _In_ int nPort,
                                    _In_opt_ CSockets::LPLISTENER_OPTIONS lpOptions)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (szBindAddressW != NULL && szBindAddressW[0] != 0)
  {
    hRes = Punycode_Encode(cStrTempA, szBindAddressW);
    if (FAILED(hRes))
      return hRes;
  }
  return StartListening((LPSTR)cStrTempA, nFamily, nPort, lpOptions);
}

VOID CHttpServer::StopListening()
{
  TAutoRefCounted<CSslCertificate> cSslCertificate;
  TAutoRefCounted<CEncryptionKey> cSslPrivateKey;
  BOOL bWait = FALSE;

  //shutdown current listener
  {
    CCriticalSection::CAutoLock cLock(cs);

    cShutdownEv.Set();

    if (hAcceptConn != NULL)
    {
      cSocketMgr.Close(hAcceptConn, MX_E_Cancelled);
      bWait = TRUE;
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
        CCriticalSection::CAutoLock cLock(cs);

        bDone = (hAcceptConn == NULL) ? TRUE : FALSE;
      }
    }
    while (bDone == FALSE);
  }

  //done
  cShutdownEv.Close();
  return;
}

VOID CHttpServer::OnHeadersTimeoutTimerCallback(_In_ LONG nTimerId, _In_ LPVOID lpUserData, _In_opt_ LPBOOL lpbCancel)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);

  UNREFERENCED_PARAMETER(lpbCancel);

  if (cAutoRundownProt.IsAcquired() != FALSE)
  {
    CClientRequest *lpRequest = (CClientRequest *)lpUserData;
    BOOL bStopTimers = FALSE;

    if (__InterlockedRead(&(lpRequest->nHeadersTimeoutTimerId)) == nTimerId)
    {
      CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

      switch (lpRequest->nState)
      {
        case CClientRequest::StateReceivingRequestHeaders:
          TerminateRequest(lpRequest, MX_E_Timeout);

          bStopTimers = TRUE;
          break;
      }
    }

    if (bStopTimers != FALSE)
    {
      lpRequest->StopTimeoutTimers(CClientRequest::TimeoutTimerAll & (~CClientRequest::TimeoutTimerHeaders));
    }
  }
  return;
}

VOID CHttpServer::OnThroughputTimerCallback(_In_ LONG nTimerId, _In_ LPVOID lpUserData, _In_opt_ LPBOOL lpbCancel)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);

  if (cAutoRundownProt.IsAcquired() != FALSE)
  {
    CClientRequest *lpRequest = (CClientRequest *)lpUserData;
    BOOL bStopTimers = FALSE;

    if (__InterlockedRead(&(lpRequest->nThroughputTimerId)) == nTimerId)
    {
      CCriticalSection::CAutoLock cLock(lpRequest->cMutex);
      float nKbps;

      switch (lpRequest->nState)
      {
        case CClientRequest::StateReceivingRequestBody:
          if (SUCCEEDED(cSocketMgr.GetReadStats(lpRequest->hConn, NULL, &nKbps)))
          {
            if ((DWORD)(int)nKbps < dwRequestBodyMinimumThroughputInBps)
            {
              if ((++(lpRequest->dwLowThroughputCounter)) >= dwRequestBodySecondsOfLowThroughput)
              {
                TerminateRequest(lpRequest, MX_E_Timeout);

                *lpbCancel = TRUE;
                bStopTimers = TRUE;
              }
            }
            else
            {
              lpRequest->dwLowThroughputCounter = 0;
            }
          }
          break;

        case CClientRequest::StateSendingResponse:
          if (SUCCEEDED(cSocketMgr.GetWriteStats(lpRequest->hConn, NULL, &nKbps)))
          {
            if ((DWORD)(int)nKbps < dwResponseMinimumThroughputInBps)
            {
              if ((++(lpRequest->dwLowThroughputCounter)) >= dwResponseMinimumThroughputInBps)
              {
                TerminateRequest(lpRequest, MX_E_Timeout);

                *lpbCancel = TRUE;
                bStopTimers = TRUE;
              }
            }
            else
            {
              lpRequest->dwLowThroughputCounter = 0;
            }
          }
          break;
      }
    }

    if (bStopTimers != FALSE)
    {
      lpRequest->StopTimeoutTimers(CClientRequest::TimeoutTimerAll & (~CClientRequest::TimeoutTimerThroughput));
    }
  }
  return;
}

VOID CHttpServer::OnGracefulTerminationTimerCallback(_In_ LONG nTimerId, _In_ LPVOID lpUserData,
                                                     _In_opt_ LPBOOL lpbCancel)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);

  UNREFERENCED_PARAMETER(lpbCancel);

  if (cAutoRundownProt.IsAcquired() != FALSE)
  {
    CClientRequest *lpRequest = (CClientRequest *)lpUserData;
    BOOL bStopTimers = FALSE;

    if (__InterlockedRead(&(lpRequest->nGracefulTerminationTimerId)) == nTimerId)
    {
      CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

      switch (lpRequest->nState)
      {
        case CClientRequest::StateSendingResponse:
          TerminateRequest(lpRequest, MX_E_Timeout);

          bStopTimers = TRUE;
          break;

        case CClientRequest::StateLingerClose:
          TerminateRequest(lpRequest, S_OK);

          bStopTimers = TRUE;
          break;
      }
    }

    if (bStopTimers != FALSE)
    {
      lpRequest->StopTimeoutTimers(CClientRequest::TimeoutTimerAll &
                                   (~CClientRequest::TimeoutTimerGracefulTermination));
    }
  }
  return;
}

VOID CHttpServer::OnKeepAliveTimerCallback(_In_ LONG nTimerId, _In_ LPVOID lpUserData, _In_opt_ LPBOOL lpbCancel)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);

  if (cAutoRundownProt.IsAcquired() != FALSE)
  {
    CClientRequest *lpRequest = (CClientRequest *)lpUserData;
    BOOL bStopTimers = FALSE;

    if (__InterlockedRead(&(lpRequest->nKeepAliveTimeoutTimerId)) == nTimerId)
    {
      CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

      switch (lpRequest->nState)
      {
        case CClientRequest::StateKeepingAlive:
          TerminateRequest(lpRequest, S_OK);

          *lpbCancel = TRUE;
          bStopTimers = TRUE;
          break;
      }
    }

    if (bStopTimers != FALSE)
    {
      lpRequest->StopTimeoutTimers(CClientRequest::TimeoutTimerAll & (~CClientRequest::TimeoutTimerKeepAlive));
    }
  }
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
      HRESULT hRes;

      //setup callbacks
      sData.cConnectCallback = MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnSocketConnect, this);
      sData.cDataReceivedCallback = MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnSocketDataReceived, this);
      sData.cDestroyCallback = MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnSocketDestroy, this);

      //create new request object
      if (cNewRequestObjectCallback)
      {
        hRes = cNewRequestObjectCallback(this, &cNewRequest);
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

      sData.cUserData = cNewRequest;

      //add to list
      {
        CAutoSlimRWLExclusive cListLock(&sRequestsListRwMutex);

        cRequestsList.PushTail(&(cNewRequest.Detach()->cListNode));
      }
      }
      break;
  }
  //done
  return S_OK;
}

VOID CHttpServer::OnListenerSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                                          _In_ HRESULT hrErrorCode)
{
  CCriticalSection::CAutoLock cLock(cs);

  if (h == hAcceptConn)
  {
    hAcceptConn = NULL;
  }
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
      CAutoSlimRWLShared cLimiterLock(&(sRequestLimiter.sRwMutex));
      CRedBlackTreeNode *lpNode;

      lpNode = sRequestLimiter.cTree.Find(&(lpRequest->sPeerAddr), &CRequestLimiter::SearchCompareFunc);
      if (lpNode != NULL)
      {
        CRequestLimiter *lpLimiter = CONTAINING_RECORD(lpNode, CRequestLimiter, cTreeNode);

        if ((DWORD)_InterlockedDecrement(&(lpLimiter->nCount)) == 0)
        {
          cLimiterLock.UpgradeToExclusive();

          lpNode = sRequestLimiter.cTree.Find(&(lpRequest->sPeerAddr), &CRequestLimiter::SearchCompareFunc);
          if (lpNode != NULL)
          {
            lpLimiter = CONTAINING_RECORD(lpNode, CRequestLimiter, cTreeNode);

            if (__InterlockedRead(&(lpLimiter->nCount)) == 0)
            {
              lpNode->Remove();
              delete lpLimiter;
            }
          }
        }
      }
    }

    {
      CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

      lpRequest->SetState(CClientRequest::StateTerminated);

      if (lpRequest->hConn == h)
      {
        //mark link as closed
        lpRequest->MarkLinkAsClosed();

        lpRequest->hConn = NULL;
      }
    }

    //stop timers
    lpRequest->StopTimeoutTimers(CClientRequest::TimeoutTimerAll);

    {
      CAutoSlimRWLExclusive cListLock(&sRequestsListRwMutex);

      if (lpRequest->cListNode.GetList() == &cRequestsList)
      {
        lpRequest->cListNode.Remove();
      }
    }


    if (cRequestDestroyedCallback)
    {
      cRequestDestroyedCallback(this, lpRequest);
    }

    lpRequest->Release();
  }

  //done
  return;
}

HRESULT CHttpServer::OnSocketConnect(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData)
{
  CClientRequest *lpRequest = (CClientRequest*)lpUserData;
  HRESULT hRes;

  MX_ASSERT(lpUserData != NULL);
  //count connections from the same IP
  if (SUCCEEDED(cSocketMgr.GetPeerAddress(h, &(lpRequest->sPeerAddr))) && dwMaxConnectionsPerIp != 0xFFFFFFFFUL)
  {
    CAutoSlimRWLShared cLimiterLock(&(sRequestLimiter.sRwMutex));
    CRedBlackTreeNode *lpNode;

    lpNode = sRequestLimiter.cTree.Find(&(lpRequest->sPeerAddr), &CRequestLimiter::SearchCompareFunc);
    if (lpNode != NULL)
    {
      CRequestLimiter *lpLimiter = CONTAINING_RECORD(lpNode, CRequestLimiter, cTreeNode);

      if ((DWORD)_InterlockedIncrement(&(lpLimiter->nCount)) > dwMaxConnectionsPerIp)
        return E_ACCESSDENIED;
    }
    else
    {
      CRequestLimiter *lpNewLimiter;
      CRedBlackTreeNode *lpMatchingNode;

      lpNewLimiter = MX_DEBUG_NEW CRequestLimiter(&(lpRequest->sPeerAddr));
      if (lpNewLimiter == NULL)
        return E_OUTOFMEMORY;
      cLimiterLock.UpgradeToExclusive();

      if (sRequestLimiter.cTree.Insert(&(lpNewLimiter->cTreeNode), &CRequestLimiter::InsertCompareFunc, FALSE,
                                       &lpMatchingNode) == FALSE)
      {
        CRequestLimiter *lpMatchingLimiter = CONTAINING_RECORD(lpNode, CRequestLimiter, cTreeNode);

        delete lpNewLimiter;

        if ((DWORD)_InterlockedIncrement(&(lpMatchingLimiter->nCount)) > dwMaxConnectionsPerIp)
          return E_ACCESSDENIED;
      }
    }
  }
  else
  {
    lpRequest->sPeerAddr.si_family = 0;
  }


  //add ssl layer
  if (cQuerySslCertificatesCallback)
  {
    TAutoRefCounted<CSslCertificate> cCert;
    TAutoRefCounted<CEncryptionKey > cPrivKey;
    TAutoDeletePtr<CIpcSslLayer> cLayer;

    hRes = cQuerySslCertificatesCallback(this, &cCert, &cPrivKey);
    if (FAILED(hRes))
      return hRes;
    if (hRes != S_FALSE)
    {
      if (!(cCert && cPrivKey))
        return E_FAIL;

      cLayer.Attach(MX_DEBUG_NEW CIpcSslLayer(lpIpc));
      if (!cLayer)
        return E_OUTOFMEMORY;
      hRes = cLayer->Initialize(TRUE, NULL, NULL, cCert.Get(), cPrivKey.Get());
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

  //setup request
  hRes = lpRequest->OnSetup();
  if (FAILED(hRes))
    return hRes;

  hRes = lpRequest->SetState(CClientRequest::StateReceivingRequestHeaders);
  if (FAILED(hRes))
    return hRes;

  hRes = lpRequest->StartTimeoutTimers(CClientRequest::TimeoutTimerHeaders | CClientRequest::TimeoutTimerThroughput);
  if (FAILED(hRes))
    return hRes;

  //done
  return S_OK;
}

HRESULT CHttpServer::OnSocketDataReceived(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData)
{
  CClientRequest *lpRequest = (CClientRequest*)lpUserData;
  BYTE aMsgBuf[4096];
  SIZE_T nMsgSize;
  BOOL bFireRequestHeadersReceivedCallback, bFireRequestCompleted, bFireWebSocketRequestReceivedCallback;
  int nTimersToStart, nTimersToStop;
  WEBSOCKET_REQUEST_DATA sWebSocketRequestReceivedData;
  HRESULT hRes;

  MX_ASSERT(lpUserData != NULL);

  nTimersToStart = nTimersToStop = 0;

restart:
  bFireRequestHeadersReceivedCallback = bFireRequestCompleted = bFireWebSocketRequestReceivedCallback = FALSE;

  if (nTimersToStop != 0)
  {
    lpRequest->StopTimeoutTimers(nTimersToStop);
    nTimersToStop = 0;
  }
  if (nTimersToStart != 0)
  {
    hRes = lpRequest->StartTimeoutTimers(nTimersToStart);
    if (FAILED(hRes))
    {
      //just a fatal error if we cannot set up the timers
      CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

      TerminateRequest(lpRequest, hRes);
      return S_OK;
    }
  }

  {
    CCriticalSection::CAutoLock cLock(lpRequest->cMutex);
    Internals::CHttpParser::eState nParserState;
    SIZE_T nMsgUsed;
    BOOL bBreakLoop;

    //loop
    bBreakLoop = FALSE;
    while (lpRequest->nState != CClientRequest::StateTerminated && bBreakLoop == FALSE &&
           bFireRequestHeadersReceivedCallback == FALSE && bFireRequestCompleted == FALSE &&
           bFireWebSocketRequestReceivedCallback == FALSE)
    {
      //get buffered message
      nMsgSize = sizeof(aMsgBuf);
      hRes = cSocketMgr.GetBufferedMessage(h, aMsgBuf, &nMsgSize);
      if (SUCCEEDED(hRes))
      {
        if (nMsgSize == 0)
          break;
      }
      else
      {
on_request_error:
        nTimersToStop = CClientRequest::TimeoutTimerAll;
        nTimersToStart = 0;

        OnRequestError(lpRequest, hRes, nTimersToStart);
        goto restart;
      }

      //take action depending on current state
      switch (lpRequest->nState)
      {
        case CClientRequest::StateKeepingAlive:
          nTimersToStop |= CClientRequest::StateKeepingAlive;
          //fall into CClientRequest::StateInactive

        case CClientRequest::StateInactive:
          hRes = lpRequest->SetState(CClientRequest::StateReceivingRequestHeaders);
          if (FAILED(hRes))
            goto on_request_error;
          break;
      }

      switch (lpRequest->nState)
      {
        case CClientRequest::StateReceivingRequestHeaders:
        case CClientRequest::StateReceivingRequestBody:
          //process http being received
          hRes = lpRequest->cRequestParser.Parse(aMsgBuf, nMsgSize, nMsgUsed);
          if (FAILED(hRes))
            goto on_request_error;
          if (nMsgUsed > 0)
          {
            hRes = cSocketMgr.ConsumeBufferedMessage(h, nMsgUsed);
            if (FAILED(hRes))
              goto on_request_error;
          }

          //take action if parser's state changed
          nParserState = lpRequest->cRequestParser.GetState();
          switch (nParserState)
          {
            case Internals::CHttpParser::StateBodyStart:
              //check if we are dealing with a websocket connection
              if (IsWebSocket(lpRequest) != FALSE)
              {
                //throw an error because a body is not allowed
                hRes = MX_E_InvalidData;
                goto on_request_error;
              }

              //normal http request
              bFireRequestHeadersReceivedCallback = TRUE; //fire events only if no error
              hRes = lpRequest->SetState(CClientRequest::StateAfterHeaders);

              if (FAILED(hRes))
                goto on_request_error;
              nTimersToStop = CClientRequest::TimeoutTimerAll;
              break;

            case Internals::CHttpParser::StateDone:
              if (lpRequest->nState == CClientRequest::StateReceivingRequestHeaders)
              {
                //check if we are dealing with a websocket connection
                hRes = ValidateWebSocket(lpRequest, sWebSocketRequestReceivedData);
                if (FAILED(hRes))
                  goto on_request_error;
                if (hRes == S_OK)
                {
                  bFireWebSocketRequestReceivedCallback = TRUE;
                  hRes = lpRequest->SetState(CClientRequest::StateNegotiatingWebSocket);
                }
                else
                {
                  bFireRequestHeadersReceivedCallback = bFireRequestCompleted = TRUE;
                  hRes = lpRequest->SetState(CClientRequest::StateAfterHeaders);
                }
              }
              else
              {
                bFireRequestCompleted = TRUE;
                hRes = lpRequest->SetState(CClientRequest::StateBuildingResponse);
              }

              if (FAILED(hRes))
                goto on_request_error;
              nTimersToStop = CClientRequest::TimeoutTimerAll;
              nTimersToStart = 0;
              break;
          }
          break;

        case CClientRequest::StateTerminated:
        case CClientRequest::StateGracefulTermination:
        case CClientRequest::StateLingerClose:
          hRes = cSocketMgr.ConsumeBufferedMessage(h, nMsgSize);
          if (FAILED(hRes))
            goto on_request_error;
          break;

        case CClientRequest::StateBuildingResponse:
        case CClientRequest::StateSendingResponse:
          //if we start to receive data in other state, check if the client is not flooding us
          if (nMsgSize > (SIZE_T)dwMaxIncomingBytesWhileSending)
          {
            hRes = MX_E_InvalidState;
            goto on_request_error;
          }
          bBreakLoop = TRUE;
          break;

        default:
          bBreakLoop = TRUE;
          break;
      }
    }
  }

  if (nTimersToStop != 0)
  {
    lpRequest->StopTimeoutTimers(nTimersToStop);
    nTimersToStop = 0;
  }
  if (nTimersToStart != 0)
  {
    hRes = lpRequest->StartTimeoutTimers(nTimersToStart);
    if (FAILED(hRes))
    {
      //just a fatal error if we cannot set up the timers
      CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

      TerminateRequest(lpRequest, hRes);
      return S_OK;
    }
  }

  //have some action to do?
  if (bFireWebSocketRequestReceivedCallback != FALSE)
  {
    TAutoRefCounted<CClientRequest> cClientRequest(lpRequest);
    TAutoRefCounted<CWebSocket> cWebSocket;
    int nSelectedProtocol = -1;
    TArrayList<int> aSupportedVersions;
    BOOL bRestart = FALSE;
    BOOL bFireConnected = FALSE;

    if (cShutdownEv.Wait(0) == FALSE)
    {
      if (cWebSocketRequestReceivedCallback)
      {
        hRes = cWebSocketRequestReceivedCallback(this, lpRequest, sWebSocketRequestReceivedData.nVersion,
                                                 sWebSocketRequestReceivedData.aProtocols.GetBuffer(),
                                                 sWebSocketRequestReceivedData.aProtocols.GetCount(),
                                                 nSelectedProtocol, aSupportedVersions, &cWebSocket);
      }
      else
      {
        hRes = MX_E_Unsupported;
      }
    }
    else
    {
      hRes = MX_E_Cancelled;
    }

    {
      CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

      //resume processing ONLY if we are in the state we are supposed to be
      if (lpRequest->nState == CClientRequest::StateNegotiatingWebSocket)
      {
        BOOL bFatal = FALSE;

        if (SUCCEEDED(hRes) || hRes == MX_E_Unsupported)
        {
          if (hRes == MX_E_Unsupported)
            cWebSocket.Release();
          hRes = ProcessWebSocket(lpRequest, cWebSocket, nSelectedProtocol, aSupportedVersions,
                                  sWebSocketRequestReceivedData, bFatal);
          if (SUCCEEDED(hRes))
          {
            bFireConnected = TRUE;
          }
        }
        if (FAILED(hRes))
        {
          nTimersToStop = CClientRequest::TimeoutTimerAll;
          nTimersToStart = 0;

          if (bFatal == FALSE)
          {
            OnRequestError(lpRequest, hRes, nTimersToStart);
          }
          else
          {
            TerminateRequest(lpRequest, hRes);
          }
          bRestart = TRUE;
        }
      }
    }
    if (bRestart != FALSE)
      goto restart;

    if (bFireConnected != FALSE && cWebSocket)
    {
      cWebSocket->FireConnectedAndInitialRead();
    }
    return S_OK;
  }

  if (bFireRequestHeadersReceivedCallback != FALSE)
  {
    TAutoRefCounted<CClientRequest> cClientRequest(lpRequest);
    TAutoRefCounted<CHttpBodyParserBase> cBodyParser;
    BOOL bRestart = FALSE;

    if (cShutdownEv.Wait(0) == FALSE)
    {
      hRes = S_OK;

      if (cRequestHeadersReceivedCallback)
      {
        if (bFireRequestCompleted != FALSE)
        {
          //if both fire headers and fire completed are true, we are dealing with a request that does not expects a body
          hRes = cRequestHeadersReceivedCallback(this, lpRequest, NULL);
        }
        else
        {
          hRes = cRequestHeadersReceivedCallback(this, lpRequest, &cBodyParser);
        }
      }
    }
    else
    {
      hRes = MX_E_Cancelled;
    }

    {
      CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

      //resume processing ONLY if we are in the state we are supposed to be
      if (lpRequest->nState == CClientRequest::StateAfterHeaders)
      {
        if (SUCCEEDED(hRes))
        {
          if (bFireRequestCompleted == FALSE)
          {
            //add a default body if none was added
            if (!cBodyParser)
            {
              CHttpHeaderEntContentType *lpHeader;

              lpHeader = lpRequest->cRequestParser.Headers().Find<CHttpHeaderEntContentType>();
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
                else if (StrCompareA(lpHeader->GetType(), "application/json") == 0)
                {
                  cBodyParser.Attach(MX_DEBUG_NEW CHttpBodyParserJSON());
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
              hRes = lpRequest->cRequestParser.SetBodyParser(cBodyParser.Get());
            }
            if (SUCCEEDED(hRes))
            {
              hRes = lpRequest->SetState(CClientRequest::StateReceivingRequestBody);
            }
          }
          else
          {
            hRes = lpRequest->SetState(CClientRequest::StateBuildingResponse);
          }
        }
        if (SUCCEEDED(hRes))
        {
          if (bFireRequestCompleted == FALSE)
          {
            nTimersToStart |= CClientRequest::TimeoutTimerThroughput;
          }
        }
        else
        {
          nTimersToStop = CClientRequest::TimeoutTimerAll;
          nTimersToStart = 0;

          OnRequestError(lpRequest, hRes, nTimersToStart);
          bRestart = TRUE;
        }
      }
    }

    if (bRestart != FALSE || bFireRequestCompleted == FALSE)
      goto restart;
  }

  if (bFireRequestCompleted != FALSE)
  {
    TAutoRefCounted<CClientRequest> cClientRequest(lpRequest);

    if (lpRequest->nState != CClientRequest::StateTerminated)
    {
      BOOL bRestart = FALSE;

      if (cShutdownEv.Wait(0) == FALSE)
      {
        if (cRequestCompletedCallback)
        {
          cRequestCompletedCallback(this, lpRequest);
        }
        else
        {
          CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

          nTimersToStop = CClientRequest::TimeoutTimerAll;
          nTimersToStart = 0;

          OnRequestError(lpRequest, E_NOTIMPL, nTimersToStart);
          bRestart = TRUE;
        }
      }
      else
      {
        CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

        nTimersToStop = CClientRequest::TimeoutTimerAll;
        nTimersToStart = 0;

        OnRequestError(lpRequest, MX_E_Cancelled, nTimersToStart);
        bRestart = TRUE;
      }
      if (bRestart != FALSE)
        goto restart;
    }
  }

  //done
  return S_OK;
}

VOID CHttpServer::TerminateRequest(_In_ CClientRequest *lpRequest, _In_ HRESULT hrErrorCode)
{
  lpRequest->SetState(CClientRequest::StateTerminated);
  cSocketMgr.Close(lpRequest->hConn, hrErrorCode);
  return;
}

VOID CHttpServer::OnRequestError(_In_ CClientRequest *lpRequest, _In_ HRESULT hrErrorCode, _Inout_ int &nTimersToStart)
{
  CStringA cStrResponseA;

  //check if we can do a graceful termination
  if (hrErrorCode != MX_E_Timeout &&
      (__InterlockedRead(&(lpRequest->nFlags)) & REQUEST_FLAG_HeadersSent) == 0 &&
       (lpRequest->nState == CClientRequest::StateReceivingRequestHeaders ||
        lpRequest->nState == CClientRequest::StateAfterHeaders ||
        lpRequest->nState == CClientRequest::StateReceivingRequestBody ||
        lpRequest->nState == CClientRequest::StateBuildingResponse ||
        lpRequest->nState == CClientRequest::StateNegotiatingWebSocket))
  {
    HRESULT hRes;

    hRes = FillResponseWithError(lpRequest, 0, hrErrorCode, NULL);
    if (SUCCEEDED(hRes))
    {
      hRes = lpRequest->SendHeaders();
      if (SUCCEEDED(hRes))
      {
        hRes = lpRequest->SendQueuedStreams();
        if (SUCCEEDED(hRes))
          _InterlockedOr(&(lpRequest->nFlags), REQUEST_FLAG_ErrorPageSent | REQUEST_FLAG_DontKeepAlive);
      }

      if (SUCCEEDED(hRes) && lpRequest->nState == CClientRequest::StateBuildingResponse)
      {
        //if we are building the response, then we can assume no more data will be sent and close
        //else we will wait until the graceful termination timeout if the client does not close the
        //connection
        hRes = cSocketMgr.AfterWriteSignal(lpRequest->hConn,
                                            MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnAfterSendResponse, this),
                                            lpRequest);
        if (SUCCEEDED(hRes))
          return;
      }
    }
    if (SUCCEEDED(hRes))
    {
      hRes = lpRequest->SetState(CClientRequest::StateGracefulTermination);
      if (SUCCEEDED(hRes))
      {
        nTimersToStart |= CClientRequest::TimeoutTimerGracefulTermination;
        return; //done
      }
    }
  }

  //if we reach here, terminate the connection
  TerminateRequest(lpRequest, hrErrorCode);

  //done
  return;
}

VOID CHttpServer::OnAfterSendResponse(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ LPVOID lpCookie,
                                      _In_ CIpc::CUserData *lpUserData)
{
  CClientRequest *lpRequest = (CClientRequest*)lpUserData;
  BOOL bKeepAlive = FALSE;
  HRESULT hRes;

  lpRequest->StopTimeoutTimers(CClientRequest::TimeoutTimerAll);

  {
    CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

    if (lpRequest->nState == CClientRequest::StateGracefulTermination ||
        (__InterlockedRead(&(lpRequest->nFlags)) & REQUEST_FLAG_DontKeepAlive) != 0)
    {
linger_close:
      lpRequest->SetState(CClientRequest::StateLingerClose);

      if (FAILED(cSocketMgr.Disable(lpRequest->hConn, FALSE, TRUE)))
      {
        TerminateRequest(lpRequest, S_OK);
        return;
      }
    }
    else
    {
      //resetting request
      hRes = lpRequest->ResetForNewRequest();
      if (FAILED(hRes))
        goto linger_close;

      bKeepAlive = TRUE;
    }
  }

  if (bKeepAlive != FALSE)
  {
    //if we reach here, we reinitiated for a new consecutive request, might be some data already sent by the server
    //waiting to be read
    hRes = lpRequest->StartTimeoutTimers(CClientRequest::TimeoutTimerKeepAlive);
    if (SUCCEEDED(hRes))
    {
      hRes = OnSocketDataReceived(lpIpc, h, lpUserData);
    }

    {
      CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

      if (FAILED(hRes) && lpRequest->nState != CClientRequest::StateTerminated)
      {
        TerminateRequest(lpRequest, hRes);
      }
    }
  }
  else
  {
    //instead, if we reach here, start a linger close timeout
    hRes = lpRequest->StartTimeoutTimers(CClientRequest::TimeoutTimerGracefulTermination);
    if (FAILED(hRes))
    {
      CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

      //close link if we cannot setup the linger close
      TerminateRequest(lpRequest, S_OK);
    }
  }

  //done
  return;
}

HRESULT CHttpServer::FillResponseWithError(_In_ CClientRequest *lpRequest, _In_ LONG nStatusCode,
                                           _In_ HRESULT hrErrorCode, _In_opt_z_ LPCSTR szAdditionalExplanationA)
{
  CSecureStringA cStrTempA;
  LPCSTR szStatusMessageA;
  BOOL bHasBody;
  HRESULT hRes;

  if (nStatusCode == 0)
  {
    switch (hrErrorCode)
    {
      case E_ACCESSDENIED:
        nStatusCode = 403; //forbidden
        break;

      case MX_E_BadLength:
      case HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE):
        nStatusCode = 413; //entity too large
        break;

      case E_INVALIDARG:
      case MX_E_InvalidData:
        nStatusCode = 400; //bad request
        break;

      case MX_E_NotFound:
      case MX_E_FileNotFound:
      case MX_E_PathNotFound:
        nStatusCode = 404; //not found
        break;

      case MX_E_Timeout:
        nStatusCode = 408; //request timeout
        break;

      case MX_E_PrivilegeNotHeld:
        nStatusCode = 401; //unauthorized
        break;

      case E_NOTIMPL:
        nStatusCode = 501; //not implemented
        break;

      case MX_E_NotReady:
      case MX_E_InvalidState:
        nStatusCode = 503; //service unavailable
        break;

      default:
        nStatusCode = 500; //internal server error
        break;
    }
  }

  szStatusMessageA = GetStatusCodeMessage(nStatusCode);
  if (szStatusMessageA == NULL)
    return E_INVALIDARG;
  bHasBody = TRUE;
  if (*szStatusMessageA == '*')
  {
    bHasBody = FALSE;
    szStatusMessageA++;
  }

  lpRequest->ResetResponseForNewRequest(lpRequest->sResponse.bPreserveWebSocketHeaders);

  lpRequest->sResponse.nStatus = nStatusCode;

  hRes = S_OK;
  if (FAILED(hrErrorCode))
  {
    TAutoRefCounted<CHttpHeaderGeneric> cHeaderGeneric;

    hRes = CHttpHeaderBase::Create("X-ErrorCode", FALSE, (CHttpHeaderBase**)&cHeaderGeneric);
    if (SUCCEEDED(hRes))
    {
      if (cStrTempA.Format("0x%08X", hrErrorCode) != FALSE)
      {
        hRes = cHeaderGeneric->SetValue((LPCSTR)cStrTempA, cStrTempA.GetLength());
        if (SUCCEEDED(hRes))
        {
          hRes = lpRequest->sResponse.cHeaders.AddElement(cHeaderGeneric.Get());
          if (SUCCEEDED(hRes))
            cHeaderGeneric.Detach();
        }
      }
      else
      {
        hRes = E_OUTOFMEMORY;
      }
    }
  }

  if (SUCCEEDED(hRes))
  {
    TAutoRefCounted<CHttpHeaderGenConnection> cHeader;

    hRes = CHttpHeaderBase::Create<CHttpHeaderGenConnection>(FALSE, &cHeader);
    if (SUCCEEDED(hRes))
    {
      hRes = cHeader->AddConnection("close", 5);
      if (SUCCEEDED(hRes))
      {
        hRes = lpRequest->sResponse.cHeaders.AddElement(cHeader.Get());
        if (SUCCEEDED(hRes))
          cHeader.Detach();
      }
    }
  }

  if (SUCCEEDED(hRes))
  {
    TAutoRefCounted<CHttpHeaderRespCacheControl> cHeader;

    hRes = CHttpHeaderBase::Create<CHttpHeaderRespCacheControl>(FALSE, &cHeader);
    if (SUCCEEDED(hRes))
    {
      hRes = cHeader->SetNoCache(TRUE);
      if (SUCCEEDED(hRes))
      {
        hRes = cHeader->SetNoStore(TRUE);
        if (SUCCEEDED(hRes))
        {
          hRes = cHeader->SetMustRevalidate(TRUE);
          if (SUCCEEDED(hRes))
          {
            hRes = lpRequest->sResponse.cHeaders.AddElement(cHeader.Get());
            if (SUCCEEDED(hRes))
              cHeader.Detach();
          }
        }
      }
    }
  }

  if (SUCCEEDED(hRes))
  {
    TAutoRefCounted<CHttpHeaderGeneric> cHeaderGeneric;

    hRes = CHttpHeaderBase::Create("Pragma", FALSE, (CHttpHeaderBase**)&cHeaderGeneric);
    if (SUCCEEDED(hRes))
    {
      hRes = cHeaderGeneric->SetValue("no-cache", 8);
      if (SUCCEEDED(hRes))
      {
        hRes = lpRequest->sResponse.cHeaders.AddElement(cHeaderGeneric.Get());
        if (SUCCEEDED(hRes))
          cHeaderGeneric.Detach();
      }
    }
  }

  if (SUCCEEDED(hRes))
  {
    TAutoRefCounted<CHttpHeaderEntExpires> cHeader;

    hRes = CHttpHeaderBase::Create<CHttpHeaderEntExpires>(FALSE, &cHeader);
    if (SUCCEEDED(hRes))
    {
      CDateTime cDt;

      hRes = cDt.SetDateTime(1970, 1, 1, 0, 0, 0);
      if (SUCCEEDED(hRes))
      {
        hRes = cHeader->SetDate(cDt);
        if (SUCCEEDED(hRes))
        {
          hRes = lpRequest->sResponse.cHeaders.AddElement(cHeader.Get());
          if (SUCCEEDED(hRes))
            cHeader.Detach();
        }
      }
    }
  }

  if (FAILED(hRes))
    return hRes;

  if (bHasBody != FALSE)
  {
    TAutoRefCounted<CHttpHeaderEntContentType> cHeader;
    CSecureStringA cStrBodyA;
    TAutoRefCounted<CMemoryStream> cStream;

    if (szAdditionalExplanationA == NULL)
      szAdditionalExplanationA = "";

    cStrBodyA.Empty();
    if (cCustomErrorPageCallback)
    {
      hRes = cCustomErrorPageCallback(this, cStrBodyA, nStatusCode, szStatusMessageA, szAdditionalExplanationA);
      if (FAILED(hRes))
        return hRes;
    }
    if (cStrBodyA.IsEmpty() != FALSE)
    {
      static const LPCSTR szServerErrorFormatA = "<!DOCTYPE html>"
                                                 "<html lang=\"en\">"
                                                 "<head>"
                                                 "<meta name=\"viewport\" content=\"width=device-width, "
                                                                                   "initial-scale=1\">"
                                                 "<meta charset=\"utf-8\">"
                                                 "<title>Error: %03d %s</title>"
                                                 "<style>"
                                                 "body {"
                                                 "	font-family: 'Trebuchet MS', Geneva, Helvetica, sans-serif;"
                                                 "	font-size: 10pt;"
                                                 "}"
                                                 ".header {"
                                                 "	font-size: 11pt;"
                                                 "}"
                                                 "</style>"
                                                 "</head>"
                                                 "<body>"
                                                 "<div class=\"header\">Error <b>%03d</b>: %s</div>"
                                                 "%s%s"
                                                 "</body>"
                                                 "</html>";
      if (cStrBodyA.Format(szServerErrorFormatA, nStatusCode, szStatusMessageA, nStatusCode, szStatusMessageA,
                           (((*szAdditionalExplanationA) != 0) ? "<hr />" : ""), szAdditionalExplanationA) == FALSE)
      {
        return E_OUTOFMEMORY;
      }
    }

    hRes = CHttpHeaderBase::Create<CHttpHeaderEntContentType>(FALSE, &cHeader);
    if (SUCCEEDED(hRes))
    {
      switch (*((LPCSTR)cStrBodyA))
      {
        case '<':
          hRes = cHeader->SetType("text/html", 9);
          if (SUCCEEDED(hRes))
            hRes = cHeader->AddParam("charset", L"utf-8");
          break;

        case '{':
        case '[':
          hRes = cHeader->SetType("application/json", 16);
          if (SUCCEEDED(hRes))
            hRes = cHeader->AddParam("charset", L"utf-8");
          break;

        default:
          hRes = cHeader->SetType("text/plain", 10);
          if (SUCCEEDED(hRes))
            hRes = cHeader->AddParam("charset", L"utf-8");
          break;
      }
    }
    if (SUCCEEDED(hRes))
      hRes = lpRequest->sResponse.cHeaders.AddElement(cHeader.Get());
    if (FAILED(hRes))
      return hRes;
    cHeader.Detach();

    cStream.Attach(MX_DEBUG_NEW CMemoryStream(128 * 1024));
    if (!cStream)
      return E_OUTOFMEMORY;
    hRes = cStream->Create(cStrBodyA.GetLength());
    if (SUCCEEDED(hRes))
    {
      SIZE_T nBytesWritten;

      hRes = cStream->Write((LPCSTR)cStrBodyA, cStrBodyA.GetLength(), nBytesWritten);
    }
    if (SUCCEEDED(hRes))
    {
      if (lpRequest->sResponse.aStreamsList.AddElement(cStream.Get()) != FALSE)
        cStream.Detach();
      else
        hRes = E_OUTOFMEMORY;
    }
    if (FAILED(hRes))
      return hRes;
    lpRequest->sResponse.bLastStreamIsData = TRUE;
  }

  //done
  return S_OK;
}

LPCSTR CHttpServer::GetStatusCodeMessage(_In_ LONG nStatusCode)
{
  SIZE_T i;

  for (i = 0; i < MX_ARRAYLEN(lpServerErrorMessages); i++)
  {
    if (nStatusCode >= lpServerErrorMessages[i].nBaseStatusCode &&
        nStatusCode < lpServerErrorMessages[i].nBaseStatusCode + lpServerErrorMessages[i].nMessagesCount)
    {
      return lpServerErrorMessages[i].lpszMessagesA[nStatusCode - lpServerErrorMessages[i].nBaseStatusCode];
    }
  }
  return NULL;
}

BOOL CHttpServer::IsWebSocket(_In_ CClientRequest *lpRequest)
{
  CHttpHeaderGenConnection *lpHeaderGenConnection;

  lpHeaderGenConnection = lpRequest->cRequestParser.Headers().Find<CHttpHeaderGenConnection>();
  if (lpHeaderGenConnection != NULL && lpHeaderGenConnection->HasConnection("upgrade") != FALSE)
  {
    CHttpHeaderGenUpgrade *lpHeaderGenUpgrade;

    lpHeaderGenUpgrade = lpRequest->cRequestParser.Headers().Find<CHttpHeaderGenUpgrade>();
    if (lpHeaderGenUpgrade != NULL && lpHeaderGenUpgrade->HasProduct("websocket") != FALSE)
    {
      return TRUE;
    }
  }
  return FALSE;
}

HRESULT CHttpServer::ValidateWebSocket(_In_ CClientRequest *lpRequest, _Inout_ WEBSOCKET_REQUEST_DATA &sData)
{
  CHttpHeaderGenConnection *lpHeaderGenConnection;
  CHttpHeaderGenUpgrade *lpHeaderGenUpgrade;
  CHttpHeaderReqSecWebSocketKey *lpHeaderReqSecWebSocketKey;
  CHttpHeaderReqSecWebSocketProtocol *lpHeaderReqSecWebSocketProtocol;
  CHttpHeaderReqSecWebSocketVersion *lpHeaderReqSecWebSocketVersion;
  HRESULT hRes;

  sData.nVersion = 0;
  sData.aProtocols.RemoveAllElements();
  sData.cHeaderRespSecWebSocketAccept.Release();

  lpHeaderGenConnection = lpRequest->cRequestParser.Headers().Find<CHttpHeaderGenConnection>();
  if (lpHeaderGenConnection == NULL || lpHeaderGenConnection->HasConnection("upgrade") == FALSE)
    return S_FALSE;

  lpHeaderGenUpgrade = lpRequest->cRequestParser.Headers().Find<CHttpHeaderGenUpgrade>();
  if (lpHeaderGenUpgrade == NULL || lpHeaderGenUpgrade->HasProduct("websocket") == FALSE)
    return S_FALSE;

  //check the request method
  if (StrCompareA(lpRequest->cRequestParser.GetRequestMethod(), "GET") != 0)
    return MX_E_InvalidData;

  //get required headers
  lpHeaderReqSecWebSocketKey = lpRequest->cRequestParser.Headers().Find<CHttpHeaderReqSecWebSocketKey>();
  lpHeaderReqSecWebSocketVersion = lpRequest->cRequestParser.Headers().Find<CHttpHeaderReqSecWebSocketVersion>();
  if (lpHeaderReqSecWebSocketKey == NULL || lpHeaderReqSecWebSocketVersion == NULL)
    return MX_E_InvalidData;

  //verify supported versions
  sData.nVersion = lpHeaderReqSecWebSocketVersion->GetVersion();
  if (sData.nVersion < 13)
    return MX_E_InvalidData;

  lpHeaderReqSecWebSocketProtocol = lpRequest->cRequestParser.Headers().Find<CHttpHeaderReqSecWebSocketProtocol>();
  if (lpHeaderReqSecWebSocketProtocol != NULL)
  {
    CStringA cStrTempA;
    SIZE_T i, nCount;

    nCount = lpHeaderReqSecWebSocketProtocol->GetProtocolsCount();
    for (i = 0; i < nCount; i++)
    {
      if (cStrTempA.Copy(lpHeaderReqSecWebSocketProtocol->GetProtocol(i)) == FALSE)
        return E_OUTOFMEMORY;
      if (sData.aProtocols.AddElement((LPCSTR)cStrTempA) == FALSE)
        return E_OUTOFMEMORY;
      cStrTempA.Detach();
    }
  }

  hRes = CHttpHeaderBase::Create<CHttpHeaderRespSecWebSocketAccept>(FALSE, &(sData.cHeaderRespSecWebSocketAccept));
  if (SUCCEEDED(hRes))
  {
    hRes = sData.cHeaderRespSecWebSocketAccept->SetKey(lpHeaderReqSecWebSocketKey->GetKey(),
                                                       lpHeaderReqSecWebSocketKey->GetKeyLength());
  }

  //done
  return hRes;
}

HRESULT CHttpServer::ProcessWebSocket(_In_ CClientRequest *lpRequest, _In_ CWebSocket *lpWebSocket,
                                      _In_ int nSelectedProtocol, _In_ TArrayList<int> &aSupportedVersions,
                                      _In_ WEBSOCKET_REQUEST_DATA &sData, _Out_ BOOL &bFatal)
{
  SIZE_T nIndex;
  HRESULT hRes;

  bFatal = FALSE;

  while ((nIndex = lpRequest->sResponse.cHeaders.Find("Sec-WebSocket-Version")) != (SIZE_T)-1)
  {
    lpRequest->sResponse.cHeaders.RemoveElementAt(nIndex);
  }
  while ((nIndex = lpRequest->sResponse.cHeaders.Find("Sec-WebSocket-Protocol")) != (SIZE_T)-1)
  {
    lpRequest->sResponse.cHeaders.RemoveElementAt(nIndex);
  }
  while ((nIndex = lpRequest->sResponse.cHeaders.Find("Sec-WebSocket-Accept")) != (SIZE_T)-1)
  {
    lpRequest->sResponse.cHeaders.RemoveElementAt(nIndex);
  }

  if (lpWebSocket != NULL)
  {
    TAutoRefCounted<CHttpHeaderRespSecWebSocketVersion> cHeaderRespSecWebSocketVersion;

    //add websocket version
    hRes = CHttpHeaderBase::Create<CHttpHeaderRespSecWebSocketVersion>(FALSE, &cHeaderRespSecWebSocketVersion);
    if (SUCCEEDED(hRes))
    {
      hRes = cHeaderRespSecWebSocketVersion->AddVersion(sData.nVersion);
      if (SUCCEEDED(hRes))
      {
        hRes = lpRequest->sResponse.cHeaders.AddElement(cHeaderRespSecWebSocketVersion.Get());
        if (SUCCEEDED(hRes))
          cHeaderRespSecWebSocketVersion.Detach();
      }
    }

    //add websocket protocol
    if (SUCCEEDED(hRes) && sData.aProtocols.GetCount() > 0)
    {
      if (nSelectedProtocol >= 0 && (SIZE_T)nSelectedProtocol < sData.aProtocols.GetCount())
      {
        TAutoRefCounted<CHttpHeaderRespSecWebSocketProtocol> cHeaderRespSecWebSocketProtocol;

        hRes = CHttpHeaderBase::Create<CHttpHeaderRespSecWebSocketProtocol>(FALSE, &cHeaderRespSecWebSocketProtocol);
        if (SUCCEEDED(hRes))
        {
          hRes = cHeaderRespSecWebSocketProtocol->SetProtocol(sData.aProtocols.GetElementAt((SIZE_T)nSelectedProtocol));
          if (SUCCEEDED(hRes))
          {
            hRes = lpRequest->sResponse.cHeaders.AddElement(cHeaderRespSecWebSocketProtocol.Get());
            if (SUCCEEDED(hRes))
              cHeaderRespSecWebSocketProtocol.Detach();
          }
        }
      }
      else
      {
        hRes = E_FAIL; //if the client sent protocols, we have to choose one
      }
    }

    //add websocket accept
    if (SUCCEEDED(hRes))
    {
      hRes = lpRequest->sResponse.cHeaders.AddElement(sData.cHeaderRespSecWebSocketAccept.Get());
      if (SUCCEEDED(hRes))
        sData.cHeaderRespSecWebSocketAccept.Detach();
    }

    //add connection
    if (SUCCEEDED(hRes))
    {
      TAutoRefCounted<CHttpHeaderGenConnection> cHeaderGenConnection;

      hRes = CHttpHeaderBase::Create<CHttpHeaderGenConnection>(FALSE, &cHeaderGenConnection);
      if (SUCCEEDED(hRes))
      {
        hRes = cHeaderGenConnection->AddConnection("upgrade");
        if (SUCCEEDED(hRes))
        {
          hRes = lpRequest->sResponse.cHeaders.AddElement(cHeaderGenConnection.Get());
          if (SUCCEEDED(hRes))
            cHeaderGenConnection.Detach();
        }
      }
    }

    //add upgrade
    if (SUCCEEDED(hRes))
    {
      TAutoRefCounted<CHttpHeaderGenUpgrade> cHeaderGenUpgrade;

      hRes = CHttpHeaderBase::Create<CHttpHeaderGenUpgrade>(FALSE, &cHeaderGenUpgrade);
      if (SUCCEEDED(hRes))
      {
        hRes = cHeaderGenUpgrade->AddProduct("websocket");
        if (SUCCEEDED(hRes))
        {
          hRes = lpRequest->sResponse.cHeaders.AddElement(cHeaderGenUpgrade.Get());
          if (SUCCEEDED(hRes))
            cHeaderGenUpgrade.Detach();
        }
      }
    }

    //pause processing
    hRes = lpRequest->lpSocketMgr->PauseInputProcessing(lpRequest->hConn);
    if (FAILED(hRes))
      return hRes;

    //send headers
    lpRequest->sResponse.aStreamsList.RemoveAllElements();
    lpRequest->sResponse.nStatus = 101;
    lpRequest->sResponse.cStrReasonA.Empty();

    hRes = lpRequest->SendHeaders();
    if (FAILED(hRes))
    {
      if (FAILED(lpRequest->lpSocketMgr->ResumeInputProcessing(lpRequest->hConn)))
        bFatal = TRUE;
      return hRes;
    }

    //setup websocket ipc
    hRes = lpWebSocket->SetupIpc(lpRequest->lpSocketMgr, lpRequest->hConn, TRUE);
    if (FAILED(hRes))
      return hRes;

    //if we reach here, an error is fatal and leads to a hard close
    bFatal = TRUE;
    {
      CAutoSlimRWLExclusive cListLock(&sRequestsListRwMutex);

      lpRequest->cListNode.Remove();
      lpRequest->Release();
    }

    //set new state, just to avoid some background task doing stuff on client request
    hRes = lpRequest->SetState(CClientRequest::StateWebSocket);
    if (FAILED(hRes))
      return hRes;

    //resume input processing
    hRes = lpRequest->lpSocketMgr->ResumeInputProcessing(lpRequest->hConn);

    //at this point, http server does not longer control the connection
  }
  else
  {
    //add websocket version
    if (aSupportedVersions.GetCount() > 0)
    {
      TAutoRefCounted<CHttpHeaderRespSecWebSocketVersion> cHeaderRespSecWebSocketVersion;
      SIZE_T i, nCount;

      hRes = CHttpHeaderBase::Create<CHttpHeaderRespSecWebSocketVersion>(FALSE, &cHeaderRespSecWebSocketVersion);
      nCount = aSupportedVersions.GetCount();
      for (i = 0; SUCCEEDED(hRes) && i < nCount; i++)
      {
        hRes = cHeaderRespSecWebSocketVersion->AddVersion(aSupportedVersions[i]);
      }
      if (SUCCEEDED(hRes))
      {
        hRes = lpRequest->sResponse.cHeaders.AddElement(cHeaderRespSecWebSocketVersion.Get());
        if (SUCCEEDED(hRes))
          cHeaderRespSecWebSocketVersion.Detach();
      }
      if (FAILED(hRes))
        return hRes;
    }

    //this will preserve Sec-WebSocket-XXX stuff
    lpRequest->sResponse.bPreserveWebSocketHeaders = TRUE;

    hRes = MX_E_InvalidData;
  }

  //done
  return hRes;
}

VOID CHttpServer::OnRequestEnding(_In_ CClientRequest *lpRequest, _In_ HRESULT hrErrorCode)
{
  int nTimersToStart;
  HRESULT hRes;

  nTimersToStart = 0;

  {
    CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

    if (lpRequest->nState != CClientRequest::StateAfterHeaders &&
        lpRequest->nState != CClientRequest::StateBuildingResponse &&
        lpRequest->nState != CClientRequest::StateSendingResponse &&
        lpRequest->nState != CClientRequest::StateNegotiatingWebSocket)
    {
      return;
    }

    if (SUCCEEDED(hrErrorCode))
    {
      hRes = S_OK;
      if (lpRequest->nState != CClientRequest::StateSendingResponse)
      {
        hRes = lpRequest->SetState(CClientRequest::StateSendingResponse);
      }
      if (lpRequest->OnCleanup() == FALSE)
      {
        _InterlockedOr(&(lpRequest->nFlags), REQUEST_FLAG_DontKeepAlive);
      }

      //send header and response if not direct
      if (lpRequest->sResponse.bDirect == FALSE)
      {
        if (SUCCEEDED(hRes) && lpRequest->HasErrorBeenSent() == FALSE)
          hRes = lpRequest->SendHeaders();
        if (SUCCEEDED(hRes))
          hRes = lpRequest->SendQueuedStreams();
      }

      if (SUCCEEDED(hRes))
      {
        if (lpRequest->IsKeepAliveRequest() == FALSE)
        {
          _InterlockedOr(&(lpRequest->nFlags), REQUEST_FLAG_DontKeepAlive);
        }

        nTimersToStart |= CClientRequest::TimeoutTimerThroughput;
      }
      else
      {
        nTimersToStart = 0;
        TerminateRequest(lpRequest, hRes);
      }
    }
    else
    {
      TerminateRequest(lpRequest, hrErrorCode);
    }
  }

  lpRequest->StopTimeoutTimers(CClientRequest::TimeoutTimerAll);
  hRes = lpRequest->StartTimeoutTimers(nTimersToStart);

  {
    CCriticalSection::CAutoLock cLock(lpRequest->cMutex);

    if (SUCCEEDED(hRes))
    {
      //and wait until completed
      hRes = cSocketMgr.AfterWriteSignal(lpRequest->hConn,
                                         MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnAfterSendResponse, this), lpRequest);
    }
    if (FAILED(hRes))
    {
      TerminateRequest(lpRequest, hRes);
    }
  }

  if (FAILED(hRes))
    lpRequest->StopTimeoutTimers(CClientRequest::TimeoutTimerAll);
  return;
}

HRESULT CHttpServer::OnDownloadStarted(_Out_ LPHANDLE lphFile, _In_z_ LPCWSTR szFileNameW, _In_ LPVOID lpUserParam)
{
  CClientRequest *lpRequest = (CClientRequest*)lpUserParam;
  MX::CStringW cStrFileNameW;
  SIZE_T nLen;
  DWORD dw;
  Fnv64_t nHash;

  //generate filename
  if (cStrTemporaryFolderW.IsEmpty() != FALSE)
  {
    if (_GetTempPath(cStrTemporaryFolderW) == FALSE)
      return E_OUTOFMEMORY;
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
