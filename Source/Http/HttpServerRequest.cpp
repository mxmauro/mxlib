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

static const CHAR szServerInfoA[] = "MX-Library";
static const SIZE_T nServerInfoLen = MX_ARRAYLEN(szServerInfoA) - 1;

//-----------------------------------------------------------

static HRESULT WriteToStream(_In_ MX::CMemoryStream *lpStream, _In_ LPCSTR szStrA,
                             _In_opt_ SIZE_T nStrLen = (SIZE_T)-1);

//-----------------------------------------------------------

namespace MX {

CHttpServer::CClientRequest::CClientRequest() : CIpc::CUserData(), TLnkLstNode<CClientRequest>()
{
  MemSet(&sOvr, 0, sizeof(sOvr));
  lpHttpServer = NULL;
  lpSocketMgr = NULL;
  hConn = NULL;
  MemSet(&sPeerAddr, 0, sizeof(sPeerAddr));
  nState = CClientRequest::StateInitializing;
  nBrowser = CHttpHeaderBase::BrowserOther;
  _InterlockedExchange(&nFlags, 0);
  return;
}

CHttpServer::CClientRequest::~CClientRequest()
{
  if (lpHttpServer != NULL)
    lpHttpServer->OnRequestDestroyed(this);
  return;
}

VOID CHttpServer::CClientRequest::End(_In_opt_ HRESULT hrErrorCode)
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState == StateBuildingResponse)
  {
    lpHttpServer->OnRequestEnding(this, hrErrorCode);
  }
  return;
}

VOID CHttpServer::CClientRequest::EnableDirectResponse()
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState == StateBuildingResponse)
  {
    cResponse->bDirect = TRUE;
  }
  return;
}

LPCSTR CHttpServer::CClientRequest::GetMethod() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return "";
  return cRequest->cHttpCmn.GetRequestMethod();
}

CUrl* CHttpServer::CClientRequest::GetUrl() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return NULL;
  return cRequest->cHttpCmn.GetRequestUri();
}

SIZE_T CHttpServer::CClientRequest::GetRequestHeadersCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return 0;
  return cRequest->cHttpCmn.GetHeadersCount();
}

CHttpHeaderBase* CHttpServer::CClientRequest::GetRequestHeader(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return NULL;
  return cRequest->cHttpCmn.GetHeader(nIndex);
}

CHttpHeaderBase* CHttpServer::CClientRequest::GetRequestHeader(_In_z_ LPCSTR szNameA) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return NULL;
  return cRequest->cHttpCmn.GetHeader(szNameA);
}

SIZE_T CHttpServer::CClientRequest::GetRequestCookiesCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return 0;
  return cRequest->cHttpCmn.GetCookiesCount();
}

CHttpCookie* CHttpServer::CClientRequest::GetRequestCookie(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return NULL;
  return cRequest->cHttpCmn.GetCookie(nIndex);
}

CHttpBodyParserBase* CHttpServer::CClientRequest::GetRequestBodyParser() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return cRequest->cHttpCmn.GetBodyParser();
}

VOID CHttpServer::CClientRequest::ResetResponse()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == StateBuildingResponse && HasHeadersBeenSent() == FALSE)
  {
    cResponse->ResetForNewRequest();
  }
  return;
}

HRESULT CHttpServer::CClientRequest::SetResponseStatus(_In_ LONG nStatus, _In_opt_ LPCSTR szReasonA)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  if (nStatus < 101 || nStatus > 599)
    return E_INVALIDARG;
  if (szReasonA != NULL && szReasonA[0] != 0)
  {
    if (cResponse->cStrReasonA.Copy(szReasonA) == FALSE)
      return E_OUTOFMEMORY;
  }
  else
  {
    if (CHttpServer::LocateError(nStatus) == NULL)
      return E_INVALIDARG;
    cResponse->cStrReasonA.Empty();
  }
  cResponse->nStatus = nStatus;
  //done
  return S_OK;
}

HRESULT CHttpServer::CClientRequest::AddResponseHeader(_In_z_ LPCSTR szNameA, _Out_opt_ CHttpHeaderBase **lplpHeader,
                                                       _In_ BOOL bReplaceExisting)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  return cResponse->cHttpCmn.AddHeader(szNameA, lplpHeader, bReplaceExisting);
}

HRESULT CHttpServer::CClientRequest::AddResponseHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA,
                                                       _In_opt_ SIZE_T nValueLen,
                                                       _Out_opt_ CHttpHeaderBase **lplpHeader,
                                                       _In_ BOOL bReplaceExisting)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  return cResponse->cHttpCmn.AddHeader(szNameA, szValueA, nValueLen, lplpHeader, bReplaceExisting);
}

HRESULT CHttpServer::CClientRequest::AddResponseHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW,
                                                       _In_opt_ SIZE_T nValueLen,
                                                       _Out_opt_ CHttpHeaderBase **lplpHeader,
                                                       _In_ BOOL bReplaceExisting)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_NotReady;
  return cResponse->cHttpCmn.AddHeader(szNameA, szValueW, nValueLen, lplpHeader, bReplaceExisting);
}

HRESULT CHttpServer::CClientRequest::RemoveResponseHeader(_In_z_ LPCSTR szNameA)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  cResponse->cHttpCmn.RemoveHeader(szNameA);
  return S_OK;
}

HRESULT CHttpServer::CClientRequest::RemoveResponseHeader(_In_ CHttpHeaderBase *lpHeader)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  cResponse->cHttpCmn.RemoveHeader(lpHeader);
  return S_OK;
}

HRESULT CHttpServer::CClientRequest::RemoveAllResponseHeaders()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  cResponse->cHttpCmn.RemoveAllHeaders();
  return S_OK;
}

SIZE_T CHttpServer::CClientRequest::GetResponseHeadersCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return 0;
  return cResponse->cHttpCmn.GetHeadersCount();
}

CHttpHeaderBase* CHttpServer::CClientRequest::GetResponseHeader(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return NULL;
  return cResponse->cHttpCmn.GetHeader(nIndex);
}

CHttpHeaderBase* CHttpServer::CClientRequest::GetResponseHeader(_In_z_ LPCSTR szNameA) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return NULL;
  return cResponse->cHttpCmn.GetHeader(szNameA);
}

HRESULT CHttpServer::CClientRequest::AddResponseCookie(_In_ CHttpCookie &cSrc)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  return cResponse->cHttpCmn.AddCookie(cSrc);
}

HRESULT CHttpServer::CClientRequest::AddResponseCookie(_In_ CHttpCookieArray &cSrc)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  return cResponse->cHttpCmn.AddCookie(cSrc);
}

HRESULT CHttpServer::CClientRequest::AddResponseCookie(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA,
                                                       _In_opt_z_ LPCSTR szDomainA, _In_opt_z_ LPCSTR szPathA,
                                                       _In_opt_ const CDateTime *lpDate, _In_opt_ BOOL bIsSecure,
                                                       _In_opt_ BOOL bIsHttpOnly)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  return cResponse->cHttpCmn.AddCookie(szNameA, szValueA, szDomainA, szPathA, lpDate, bIsSecure, bIsHttpOnly);
}

HRESULT CHttpServer::CClientRequest::AddResponseCookie(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szValueW,
                                                       _In_opt_z_ LPCWSTR szDomainW, _In_opt_z_ LPCWSTR szPathW,
                                                       _In_opt_ const CDateTime *lpDate, _In_opt_ BOOL bIsSecure,
                                                       _In_opt_ BOOL bIsHttpOnly)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  return cResponse->cHttpCmn.AddCookie(szNameW, szValueW, szDomainW, szPathW, lpDate, bIsSecure, bIsHttpOnly);
}

HRESULT CHttpServer::CClientRequest::RemoveResponseCookie(_In_z_ LPCSTR szNameA)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  return cResponse->cHttpCmn.RemoveCookie(szNameA);
}

HRESULT CHttpServer::CClientRequest::RemoveResponseCookie(_In_z_ LPCWSTR szNameW)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  return cResponse->cHttpCmn.RemoveCookie(szNameW);
}

HRESULT CHttpServer::CClientRequest::RemoveAllResponseCookies()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  cResponse->cHttpCmn.RemoveAllCookies();
  return S_OK;
}

CHttpCookie* CHttpServer::CClientRequest::GetResponseCookie(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return cResponse->cHttpCmn.GetCookie(nIndex);
}

CHttpCookie* CHttpServer::CClientRequest::GetResponseCookie(_In_z_ LPCSTR szNameA) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return cResponse->cHttpCmn.GetCookie(szNameA);
}

CHttpCookie* CHttpServer::CClientRequest::GetResponseCookie(_In_z_ LPCWSTR szNameW) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return cResponse->cHttpCmn.GetCookie(szNameW);
}

CHttpCookieArray* CHttpServer::CClientRequest::GetResponseCookies() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return cResponse->cHttpCmn.GetCookies();
}

SIZE_T CHttpServer::CClientRequest::GetResponseCookiesCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return 0;
  return cResponse->cHttpCmn.GetCookiesCount();
}

HRESULT CHttpServer::CClientRequest::SendHeaders()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (cResponse->bDirect == FALSE)
    return MX_E_InvalidState;
  return BuildAndInsertOrSendHeaderStream();
}

HRESULT CHttpServer::CClientRequest::SendResponse(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  SIZE_T nWritten;
  HRESULT hRes;

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (nDataLen == 0)
    return S_OK;
  if (lpData == NULL)
    return E_POINTER;
  if (cResponse->bDirect == FALSE)
  {
    if (cResponse->aStreamsList.GetCount() > 0 && cResponse->bLastStreamIsData != FALSE)
    {
      CStream *lpStream = cResponse->aStreamsList.GetElementAt(cResponse->aStreamsList.GetCount()-1);
      hRes = lpStream->Write(lpData, nDataLen, nWritten);
      if (SUCCEEDED(hRes) && nDataLen != nWritten)
        hRes = MX_E_WriteFault;
    }
    else
    {
      TAutoRefCounted<CMemoryStream> cStream;

      cStream.Attach(MX_DEBUG_NEW CMemoryStream(128*1024));
      if (!cStream)
        return E_OUTOFMEMORY;
      hRes = cStream->Create(nDataLen);
      if (SUCCEEDED(hRes))
        hRes = cStream->Write(lpData, nDataLen, nWritten);
      if (SUCCEEDED(hRes))
      {
        if (cResponse->aStreamsList.AddElement(cStream.Get()) != FALSE)
          cStream.Detach();
        else
          hRes = E_OUTOFMEMORY;
      }
      if (SUCCEEDED(hRes))
        cResponse->bLastStreamIsData = TRUE;
    }
    if (FAILED(hRes))
      return hRes;
    cResponse->szMimeTypeHintA = NULL;
    cResponse->cStrFileNameA.Empty();
  }
  else
  {
    hRes = BuildAndInsertOrSendHeaderStream();
    if (SUCCEEDED(hRes))
      hRes = lpHttpServer->cSocketMgr.SendMsg(hConn, lpData, nDataLen);
    if (FAILED(hRes))
      return hRes;
  }
  //done
  return S_OK;
}

HRESULT CHttpServer::CClientRequest::SendFile(_In_z_ LPCWSTR szFileNameW)
{
  TAutoRefCounted<CFileStream> cStream;
  HRESULT hRes;

  if (szFileNameW == NULL)
    return E_POINTER;
  //create file stream
  cStream.Attach(MX_DEBUG_NEW CFileStream());
  if (!cStream)
    return E_OUTOFMEMORY;
  hRes = cStream->Create(szFileNameW);
  if (SUCCEEDED(hRes))
    hRes = SendStream(cStream, szFileNameW);
  //done
  return hRes;
}

HRESULT CHttpServer::CClientRequest::SendStream(_In_ CStream *lpStream, _In_opt_z_ LPCWSTR szFileNameW)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  CStringA cStrTempNameA;
  LPCWSTR sW[2];
  BOOL bSetInfo;
  HRESULT hRes;

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (lpStream == NULL)
    return E_POINTER;
  //direct send?
  if (cResponse->bDirect == FALSE)
  {
    //extract name if needed
    bSetInfo = FALSE;
    if (szFileNameW != NULL && *szFileNameW != 0 && cResponse->aStreamsList.GetCount() == 0)
    {
      sW[0] = StrChrW(szFileNameW, L'\\', TRUE);
      if (sW[0] == NULL)
        sW[0] = szFileNameW;
      sW[1] = StrChrW(szFileNameW, L'/', TRUE);
      if (sW[1] == NULL)
        sW[1] = szFileNameW;
      hRes = Utf8_Encode(cStrTempNameA, (sW[0] > sW[1]) ? sW[0]+1 : sW[1]+1);
      if (FAILED(hRes))
        return hRes;
      bSetInfo = TRUE;
    }
    //add to list
    if (cResponse->aStreamsList.AddElement(lpStream) == FALSE)
      return E_OUTOFMEMORY;
    lpStream->AddRef();
    //set response info
    if (bSetInfo != FALSE)
    {
      cResponse->szMimeTypeHintA = CHttpCommon::GetMimeType(szFileNameW);
      cResponse->cStrFileNameA.Attach(cStrTempNameA.Detach());
    }
    else
    {
      cResponse->szMimeTypeHintA = NULL;
      cResponse->cStrFileNameA.Empty();
    }
    cResponse->bLastStreamIsData = FALSE;
  }
  else
  {
    hRes = lpHttpServer->cSocketMgr.SendStream(hConn, lpStream);
    if (FAILED(hRes))
      return hRes;
  }
  //done
  return S_OK;
}

HRESULT CHttpServer::CClientRequest::SendErrorPage(_In_ LONG nStatusCode, _In_ HRESULT hrErrorCode,
                                                   _In_opt_z_ LPCSTR szBodyExplanationA)
{
  CStringA cStrResponseA;

  {
    CCriticalSection::CAutoLock cLock(cMutex);
    HRESULT hRes;

    if (nState != StateBuildingResponse)
      return MX_E_NotReady;
    if (HasHeadersBeenSent() != FALSE)
      return E_FAIL;
    _InterlockedOr(&nFlags, REQUEST_FLAG_DontKeepAlive);
    hRes = lpHttpServer->GenerateErrorPage(cStrResponseA, this, nStatusCode, hrErrorCode, szBodyExplanationA);
    if (FAILED(hRes))
      return hRes;
    _InterlockedOr(&nFlags, REQUEST_FLAG_ErrorPageSent);
  }
  return SendResponse((LPCSTR)cStrResponseA, cStrResponseA.GetLength());
}

HANDLE CHttpServer::CClientRequest::GetUnderlyingSocketHandle() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  return (nState == StateBuildingResponse || nState == StateNegotiatingWebSocket) ? hConn : NULL;
}

CSockets* CHttpServer::CClientRequest::GetUnderlyingSocketManager() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  return (nState == StateBuildingResponse ||
          nState == StateNegotiatingWebSocket) ? const_cast<CSockets*>(lpSocketMgr) : NULL;
}

HRESULT CHttpServer::CClientRequest::Initialize(_In_ CHttpServer *_lpHttpServer, _In_ CSockets *_lpSocketMgr,
                                                _In_ HANDLE _hConn, _In_ DWORD dwMaxHeaderSize)
{
  MX_ASSERT(_lpHttpServer != NULL);
  MX_ASSERT(_lpSocketMgr != NULL);
  MX_ASSERT(_hConn != NULL);
  lpHttpServer = _lpHttpServer;
  lpSocketMgr = _lpSocketMgr;
  hConn = _hConn;
  if (_lpHttpServer->ShouldLog(1) != FALSE)
  {
    _lpHttpServer->Log(L"HttpServer(State/Req:0x%p/Conn:0x%p): StateInitializing", this, hConn);
  }

  cRequest.Attach(MX_DEBUG_NEW CRequest(lpHttpServer));
  cResponse.Attach(MX_DEBUG_NEW CResponse(lpHttpServer));
  if ((!cRequest) || (!cResponse))
    return E_OUTOFMEMORY;
  cRequest->cHttpCmn.SetOption_MaxHeaderSize(dwMaxHeaderSize);
  cResponse->cHttpCmn.SetOption_MaxHeaderSize(dwMaxHeaderSize);
  //done
  return S_OK;
}

VOID CHttpServer::CClientRequest::ResetForNewRequest()
{
  nState = CClientRequest::StateInitializing;
  if (lpHttpServer->ShouldLog(1) != FALSE)
  {
    lpHttpServer->Log(L"HttpServer(State/Req:0x%p/Conn:0x%p): StateInitializing [ResetForNewRequest]", this, hConn);
  }
  _InterlockedExchange(&nFlags, 0);
  //----
  cRequest->ResetForNewRequest();
  cResponse->ResetForNewRequest();
  cWebSocketInfo.Reset();
  return;
}

BOOL CHttpServer::CClientRequest::IsKeepAliveRequest() const
{
  int nHttpVersion[2];

  nHttpVersion[0] = cRequest->cHttpCmn.GetRequestVersionMajor();
  nHttpVersion[1] = cRequest->cHttpCmn.GetRequestVersionMinor();
  return (nHttpVersion[0] > 0) ? cRequest->cHttpCmn.IsKeepAliveRequest() : FALSE;
}

BOOL CHttpServer::CClientRequest::HasErrorBeenSent() const
{
  return ((__InterlockedRead(const_cast<LONG volatile*>(&nFlags)) & REQUEST_FLAG_ErrorPageSent) != 0) ? TRUE : FALSE;
}

BOOL CHttpServer::CClientRequest::HasHeadersBeenSent() const
{
  return ((__InterlockedRead(const_cast<LONG volatile*>(&nFlags)) & REQUEST_FLAG_HeadersSent) != 0) ? TRUE : FALSE;
}

HRESULT CHttpServer::CClientRequest::BuildAndInsertOrSendHeaderStream()
{
  TAutoRefCounted<CMemoryStream> cHdrStream;
  CStringA cStrTempA;
  int nHttpVersion[2];
  LONG nStatus;
  LPCSTR sA;
  HRESULT hRes;

  if ((_InterlockedOr(&nFlags, REQUEST_FLAG_HeadersSent) & REQUEST_FLAG_HeadersSent) != 0)
  {
    if (lpHttpServer->ShouldLog(1) != FALSE)
    {
      lpHttpServer->Log(L"HttpServer(ResponseHeaders/Req:0x%p/Conn:0x%p): Already sent!", this, hConn);
    }
    return S_OK;
  }

  cHdrStream.Attach(MX_DEBUG_NEW CMemoryStream(65536));
  hRes = (cHdrStream) ? cHdrStream->Create() : E_OUTOFMEMORY;
  //status
  if (SUCCEEDED(hRes))
  {
    if ((nStatus = cResponse->nStatus) == 0)
    {
      if (cResponse->cHttpCmn.GetHeader<CHttpHeaderRespLocation>() != NULL)
        nStatus = 302;
      else
        nStatus = 200;
      sA = CHttpServer::LocateError(nStatus);
    }
    else
    {
      sA = (cResponse->cStrReasonA.IsEmpty() != FALSE) ? CHttpServer::LocateError(nStatus) :
                                                         (LPCSTR)(cResponse->cStrReasonA);
    }
    if (sA == NULL || *sA == 0)
      sA = "Undefined";
    nHttpVersion[0] = cRequest->cHttpCmn.GetRequestVersionMajor();
    nHttpVersion[1] = cRequest->cHttpCmn.GetRequestVersionMinor();
    if (nHttpVersion[0] == 0)
    {
      nHttpVersion[0] = 1;
      nHttpVersion[1] = 0;
    }
    hRes = cHdrStream->WriteString("HTTP/%d.%d %03d %s\r\n", nHttpVersion[0], nHttpVersion[1], nStatus, sA);
  }
  //date
  if (SUCCEEDED(hRes))
  {
    CHttpHeaderBase *lpHdr;

    lpHdr = cResponse->cHttpCmn.GetHeader("Date");
    if (lpHdr != NULL)
    {
      hRes = lpHdr->Build(cStrTempA, nBrowser);
      if (SUCCEEDED(hRes))
      {
        hRes = WriteToStream(cHdrStream, "Date: ", 5);
        if (SUCCEEDED(hRes))
        {
          hRes = WriteToStream(cHdrStream, (LPCSTR)cStrTempA, cStrTempA.GetLength());
          if (SUCCEEDED(hRes))
            hRes = WriteToStream(cHdrStream, "\r\n", 2);
        }
      }
    }
    else
    {
      CDateTime cDtNow;

      hRes = cDtNow.SetFromNow(FALSE);
      if (SUCCEEDED(hRes))
      {
        hRes = cDtNow.Format(cStrTempA, "Date: %a, %d %b %Y %H:%M:%S %z\r\n");
        if (SUCCEEDED(hRes))
          hRes = WriteToStream(cHdrStream, (LPCSTR)cStrTempA, cStrTempA.GetLength());
      }
    }
  }
  //location (if any)
  if (SUCCEEDED(hRes))
  {
    CHttpHeaderRespLocation *lpHdr;

    lpHdr = cResponse->cHttpCmn.GetHeader<CHttpHeaderRespLocation>();
    if (lpHdr != NULL)
    {
      hRes = lpHdr->Build(cStrTempA, nBrowser);
      if (SUCCEEDED(hRes))
      {
        hRes = WriteToStream(cHdrStream, "Location: ", 10);
        if (SUCCEEDED(hRes))
        {
          hRes = WriteToStream(cHdrStream, (LPCSTR)cStrTempA, cStrTempA.GetLength());
          if (SUCCEEDED(hRes))
            hRes = WriteToStream(cHdrStream, "\r\n", 2);
        }
      }
    }
  }
  //server
  if (SUCCEEDED(hRes))
  {
    hRes = WriteToStream(cHdrStream, "Server: ", 8);
    if (SUCCEEDED(hRes))
    {
      CHttpHeaderBase *lpHdr;

      lpHdr = cResponse->cHttpCmn.GetHeader("Server");
      if (lpHdr != NULL)
      {
        hRes = lpHdr->Build(cStrTempA, nBrowser);
        if (SUCCEEDED(hRes))
          hRes = WriteToStream(cHdrStream, (LPCSTR)cStrTempA, cStrTempA.GetLength());
      }
      else
      {
        hRes = WriteToStream(cHdrStream, szServerInfoA, nServerInfoLen);
      }
      if (SUCCEEDED(hRes))
        hRes = WriteToStream(cHdrStream, "\r\n", 2);
    }
  }
  //connection
  if (SUCCEEDED(hRes))
  {
    CHttpHeaderBase *lpHdr;

    lpHdr = cResponse->cHttpCmn.GetHeader("Connection");
    if (lpHdr != NULL)
    {
      hRes = lpHdr->Build(cStrTempA, nBrowser);
      if (SUCCEEDED(hRes))
      {
        hRes = WriteToStream(cHdrStream, "Connection: ", 12);
        if (SUCCEEDED(hRes))
        {
          hRes = WriteToStream(cHdrStream, (LPCSTR)cStrTempA, cStrTempA.GetLength());
          if (SUCCEEDED(hRes))
            hRes = WriteToStream(cHdrStream, "\r\n", 2);
        }
      }
    }
    else if (IsKeepAliveRequest() != FALSE)
    {
      hRes = WriteToStream(cHdrStream, "Connection: Keep-Alive\r\n", 24);
    }
  }
  //content type
  if (SUCCEEDED(hRes) && cResponse->aStreamsList.GetCount() > 0)
  {
    CHttpHeaderEntContentType *lpHdr;

    lpHdr = cResponse->cHttpCmn.GetHeader<CHttpHeaderEntContentType>();
    if (lpHdr == NULL)
    {
      //only send automatic "Content-Type" header if not direct response
      if (cResponse->bDirect == FALSE)
      {
        if (cResponse->szMimeTypeHintA != NULL)
        {
          hRes = WriteToStream(cHdrStream, "Content-Type: ", 14);
          if (SUCCEEDED(hRes))
          {
            hRes = WriteToStream(cHdrStream, cResponse->szMimeTypeHintA);
            if (SUCCEEDED(hRes))
              hRes = WriteToStream(cHdrStream, "\r\n", 2);
          }
        }
        else if (cResponse->bLastStreamIsData != FALSE)
        {
          hRes = WriteToStream(cHdrStream, "Content-Type: text/html; charset=utf-8\r\n", 40);
        }
        else
        {
          hRes = WriteToStream(cHdrStream, "Content-Type: application/octet-stream\r\n", 40);
        }
      }
    }
    else
    {
      hRes = lpHdr->Build(cStrTempA, nBrowser);
      if (SUCCEEDED(hRes))
      {
        hRes = WriteToStream(cHdrStream, "Content-Type: ", 14);
        if (SUCCEEDED(hRes))
        {
          hRes = WriteToStream(cHdrStream, (LPCSTR)cStrTempA, cStrTempA.GetLength());
          if (SUCCEEDED(hRes))
            hRes = WriteToStream(cHdrStream, "\r\n", 2);
        }
      }
    }
  }
  //content length
  if (SUCCEEDED(hRes))
  {
    CHttpHeaderEntContentLength *lpHdr;
    CHttpHeaderGenTransferEncoding *lpHdr2;
    ULONGLONG nTotalLength, nLen;
    SIZE_T i, nCount;

    lpHdr2 = cResponse->cHttpCmn.GetHeader<CHttpHeaderGenTransferEncoding>();
    if (lpHdr2 == NULL || lpHdr2->GetEncoding() != CHttpHeaderGenTransferEncoding::EncodingChunked)
    {
      lpHdr = cResponse->cHttpCmn.GetHeader<CHttpHeaderEntContentLength>();
      if (lpHdr == NULL)
      {
        //only send automatic "Content-Length" header if not direct response
        if (cResponse->bDirect == FALSE)
        {
          nTotalLength = 0ui64;
          nCount = cResponse->aStreamsList.GetCount();
          for (i = 0; i < nCount; i++)
          {
            nLen = cResponse->aStreamsList[i]->GetLength();
            if (nLen + nTotalLength < nTotalLength)
            {
              hRes = MX_E_BadLength;
              break;
            }
            nTotalLength += nLen;
          }
          if (SUCCEEDED(hRes))
            hRes = cHdrStream->WriteString("Content-Length: %I64u\r\n", nTotalLength);
        }
      }
      else
      {
        hRes = lpHdr->Build(cStrTempA, nBrowser);
        if (SUCCEEDED(hRes))
        {
          hRes = WriteToStream(cHdrStream, "Content-Length: ", 16);
          if (SUCCEEDED(hRes))
          {
            hRes = WriteToStream(cHdrStream, (LPCSTR)cStrTempA, cStrTempA.GetLength());
            if (SUCCEEDED(hRes))
              hRes = WriteToStream(cHdrStream, "\r\n", 2);
          }
        }
      }
    }
  }
  //rest of headers
  if (SUCCEEDED(hRes))
  {
    CHttpHeaderBase *lpHdr;
    SIZE_T i, nCount;

    nCount = cResponse->cHttpCmn.GetHeadersCount();
    for (i = 0; SUCCEEDED(hRes) && i < nCount; i++)
    {
      lpHdr = cResponse->cHttpCmn.GetHeader(i);
      if (StrCompareA(lpHdr->GetHeaderName(), "Content-Type") != 0 &&
          StrCompareA(lpHdr->GetHeaderName(), "Content-Length") != 0 &&
          StrCompareA(lpHdr->GetHeaderName(), "Date") != 0 &&
          StrCompareA(lpHdr->GetHeaderName(), "Server") != 0 &&
          StrCompareA(lpHdr->GetHeaderName(), "Connection") != 0 &&
          StrCompareA(lpHdr->GetHeaderName(), "Location") != 0)
      {
        hRes = lpHdr->Build(cStrTempA, nBrowser);
        if (SUCCEEDED(hRes))
        {
          hRes = WriteToStream(cHdrStream, lpHdr->GetHeaderName());
          if (SUCCEEDED(hRes))
          {
            hRes = WriteToStream(cHdrStream, (LPCSTR)cStrTempA, cStrTempA.GetLength());
            if (SUCCEEDED(hRes))
              hRes = WriteToStream(cHdrStream, "\r\n", 2);
          }
        }
      }
    }
  }
  //cookies
  if (SUCCEEDED(hRes))
  {
    CHttpCookie *lpCookie;
    SIZE_T i, nCount;

    nCount = cResponse->cHttpCmn.GetCookiesCount();
    for (i=0; SUCCEEDED(hRes) && i<nCount; i++)
    {
      lpCookie = cResponse->cHttpCmn.GetCookie(i);
      hRes = lpCookie->ToString(cStrTempA);
      if (SUCCEEDED(hRes))
      {
        hRes = WriteToStream(cHdrStream, "Set-Cookie: ", 12);
        if (SUCCEEDED(hRes))
        {
          hRes = WriteToStream(cHdrStream, (LPCSTR)cStrTempA, cStrTempA.GetLength());
          if (SUCCEEDED(hRes))
            hRes = WriteToStream(cHdrStream, "\r\n", 2);
        }
      }
    }
  }
  //add end of header
  if (SUCCEEDED(hRes))
    hRes = WriteToStream(cHdrStream, "\r\n", 2);
  //add at the beginning of stream list if not direct, else send
  if (SUCCEEDED(hRes))
  {
    if (cResponse->bDirect == FALSE)
    {
      if (cResponse->aStreamsList.InsertElementAt(cHdrStream.Get(), 0) != FALSE)
        cHdrStream.Detach();
      else
        hRes = E_OUTOFMEMORY;
    }
    else
    {
      hRes = lpHttpServer->cSocketMgr.SendStream(hConn, cHdrStream.Get());
    }
  }
  if (lpHttpServer->ShouldLog(1) != FALSE)
  {
    lpHttpServer->Log(L"HttpServer(ResponseHeaders/Req:0x%p/Conn:0x%p): 0x%08X", this, hConn, hRes);
  }
  //done
  return hRes;
}

HRESULT CHttpServer::CClientRequest::BuildAndSendWebSocketHeaderStream()
{
  TAutoRefCounted<CMemoryStream> cHdrStream;
  CStringA cStrTempA;
  int nHttpVersion[2];
  HRESULT hRes;

  if ((_InterlockedOr(&nFlags, REQUEST_FLAG_HeadersSent) & REQUEST_FLAG_HeadersSent) != 0)
  {
    if (lpHttpServer->ShouldLog(1) != FALSE)
    {
      lpHttpServer->Log(L"HttpServer(WebSocketResponseHeaders/Req:0x%p/Conn:0x%p): Already sent!", this, hConn);
    }
    return S_OK;
  }

  cHdrStream.Attach(MX_DEBUG_NEW CMemoryStream(65536));
  hRes = (cHdrStream) ? cHdrStream->Create() : E_OUTOFMEMORY;
  //status
  if (SUCCEEDED(hRes))
  {
    LPCSTR szStatusMsgA;

    nHttpVersion[0] = cRequest->cHttpCmn.GetRequestVersionMajor();
    nHttpVersion[1] = cRequest->cHttpCmn.GetRequestVersionMinor();
    if (nHttpVersion[0] == 0)
    {
      nHttpVersion[0] = 1;
      nHttpVersion[1] = 0;
    }
    szStatusMsgA = CHttpServer::LocateError(101);
    if (*szStatusMsgA == '*')
      szStatusMsgA++;
    hRes = cHdrStream->WriteString("HTTP/%d.%d 101 %s\r\n", nHttpVersion[0], nHttpVersion[1], szStatusMsgA);
  }
  //server
  if (SUCCEEDED(hRes))
  {
    hRes = WriteToStream(cHdrStream, "Server: ", 8);
    if (SUCCEEDED(hRes))
    {
      CHttpHeaderBase *lpHdr;

      lpHdr = cResponse->cHttpCmn.GetHeader("Server");
      if (lpHdr != NULL)
      {
        hRes = lpHdr->Build(cStrTempA, nBrowser);
        if (SUCCEEDED(hRes))
          hRes = WriteToStream(cHdrStream, (LPCSTR)cStrTempA, cStrTempA.GetLength());
      }
      else
      {
        hRes = WriteToStream(cHdrStream, szServerInfoA, nServerInfoLen);
      }
      if (SUCCEEDED(hRes))
        hRes = WriteToStream(cHdrStream, "\r\n", 2);
    }
  }
  //rest of headers
  if (SUCCEEDED(hRes))
  {
    CHttpHeaderBase *lpHdr;
    SIZE_T i, nCount;

    nCount = cResponse->cHttpCmn.GetHeadersCount();
    for (i = 0; SUCCEEDED(hRes) && i < nCount; i++)
    {
      lpHdr = cResponse->cHttpCmn.GetHeader(i);
      if (StrCompareA(lpHdr->GetHeaderName(), "Content-Type") != 0 &&
          StrCompareA(lpHdr->GetHeaderName(), "Content-Length") != 0 &&
          StrCompareA(lpHdr->GetHeaderName(), "Date") != 0 &&
          StrCompareA(lpHdr->GetHeaderName(), "Server") != 0 &&
          StrCompareA(lpHdr->GetHeaderName(), "Connection") != 0 &&
          StrCompareA(lpHdr->GetHeaderName(), "Location") != 0 ||
          StrCompareA(lpHdr->GetHeaderName(), "Upgrade") != 0)
      {
        hRes = lpHdr->Build(cStrTempA, nBrowser);
        if (SUCCEEDED(hRes))
        {
          hRes = WriteToStream(cHdrStream, lpHdr->GetHeaderName());
          if (SUCCEEDED(hRes))
          {
            hRes = WriteToStream(cHdrStream, (LPCSTR)cStrTempA, cStrTempA.GetLength());
            if (SUCCEEDED(hRes))
              hRes = WriteToStream(cHdrStream, "\r\n", 2);
          }
        }
      }
    }
  }

  //add end of header
  if (SUCCEEDED(hRes))
    hRes = WriteToStream(cHdrStream, "\r\n", 2);
  //send headers
  if (SUCCEEDED(hRes))
  {
    hRes = lpHttpServer->cSocketMgr.SendStream(hConn, cHdrStream.Get());
  }
  if (lpHttpServer->ShouldLog(1) != FALSE)
  {
    lpHttpServer->Log(L"HttpServer(WebSocketResponseHeaders/Req:0x%p/Conn:0x%p): 0x%08X", this, hConn, hRes);
  }
  //done
  return hRes;
}

HRESULT CHttpServer::CClientRequest::SendQueuedStreams()
{
  CStream *lpStream;
  SIZE_T i, nCount;
  HRESULT hRes;

  hRes = S_OK;
  nCount = cResponse->aStreamsList.GetCount();
  for (i = 0; SUCCEEDED(hRes) && i < nCount; i++)
  {
    lpStream = cResponse->aStreamsList.GetElementAt(i);
    hRes = lpHttpServer->cSocketMgr.SendStream(hConn, lpStream);
  }
  //done
  return hRes;
}

VOID CHttpServer::CClientRequest::MarkLinkAsClosed()
{
  _InterlockedOr(&nFlags, REQUEST_FLAG_LinkClosed);
  return;
}

BOOL CHttpServer::CClientRequest::IsLinkClosed() const
{
  return ((__InterlockedRead(const_cast<LONG volatile*>(&nFlags)) & REQUEST_FLAG_LinkClosed) != 0) ? TRUE : FALSE;
}

VOID CHttpServer::CClientRequest::QueryBrowser()
{
  CHttpHeaderBase *lpHeader;
  SIZE_T i, nCount;

  nCount = cRequest->cHttpCmn.GetHeadersCount();
  for (i = 0; i < nCount; i++)
  {
    lpHeader = cRequest->cHttpCmn.GetHeader(i);
    if (StrCompareA(lpHeader->GetHeaderName(), "User-Agent") == 0)
    {
      nBrowser = CHttpHeaderBase::GetBrowserFromUserAgent(reinterpret_cast<CHttpHeaderGeneric*>(lpHeader)->GetValue());
      break;
    }
  }
  return;
}

} //namespace MX

//-----------------------------------------------------------

static HRESULT WriteToStream(_In_ MX::CMemoryStream *lpStream, _In_ LPCSTR szStrA, _In_opt_ SIZE_T nStrLen)
{
  SIZE_T nWritten;
  HRESULT hRes;

  if (nStrLen == (SIZE_T)-1)
    nStrLen = MX::StrLenA(szStrA);
  hRes = lpStream->Write(szStrA, nStrLen, nWritten);
  if (SUCCEEDED(hRes) && nStrLen != nWritten)
    hRes = MX_E_WriteFault;
  return hRes;
}
