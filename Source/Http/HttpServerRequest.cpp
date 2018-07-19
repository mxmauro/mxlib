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

//-----------------------------------------------------------

namespace MX {

CHttpServer::CRequest::CRequest() : CIpc::CUserData(), TLnkLstNode<CRequest>(), sRequest(), sResponse()
{
  MemSet(&sOvr, 0, sizeof(sOvr));
  lpHttpServer = NULL;
  hConn = NULL;
  nState = CRequest::StateInitializing;
  MX_HTTP_DEBUG_PRINT(1, ("HttpServer(State/0x%p): StateInitializing\n", this));
  _InterlockedExchange(&nFlags, 0);
  sRequest.liLastRead.QuadPart = 0ui64;
  _InterlockedExchange(&(sRequest.sTimeout.nMutex), 0);
  ResetForNewRequest();
  return;
}

CHttpServer::CRequest::~CRequest()
{
  MX_ASSERT(sRequest.sTimeout.cActiveList.HasPending() == FALSE);
  if (lpHttpServer != NULL)
    lpHttpServer->OnRequestDestroyed(this);
  return;
}

VOID CHttpServer::CRequest::End(_In_opt_ HRESULT hErrorCode)
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState == StateBuildingResponse)
  {
    lpHttpServer->OnRequestEnding(this, hErrorCode);
  }
  return;
}

VOID CHttpServer::CRequest::EnableDirectResponse()
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState == StateBuildingResponse)
  {
    sResponse.bDirect = TRUE;
  }
  return;
}

LPCSTR CHttpServer::CRequest::GetMethod() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return "";
  return sRequest.cHttpCmn.GetRequestMethod();
}

CUrl* CHttpServer::CRequest::GetUrl() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sRequest.cHttpCmn.GetRequestUri();
}

SIZE_T CHttpServer::CRequest::GetRequestHeadersCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return 0;
  return sRequest.cHttpCmn.GetHeadersCount();
}

CHttpHeaderBase* CHttpServer::CRequest::GetRequestHeader(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sRequest.cHttpCmn.GetHeader(nIndex);
}

CHttpHeaderBase* CHttpServer::CRequest::GetRequestHeader(_In_z_ LPCSTR szNameA) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sRequest.cHttpCmn.GetHeader(szNameA);
}

SIZE_T CHttpServer::CRequest::GetRequestCookiesCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return 0;
  return sRequest.cHttpCmn.GetCookiesCount();
}

CHttpCookie* CHttpServer::CRequest::GetRequestCookie(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sRequest.cHttpCmn.GetCookie(nIndex);
}

CHttpBodyParserBase* CHttpServer::CRequest::GetRequestBodyParser() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sRequest.cHttpCmn.GetBodyParser();
}

VOID CHttpServer::CRequest::ResetResponse()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == StateBuildingResponse && HasHeadersBeenSent() == FALSE)
  {
    sResponse.ResetForNewRequest();
  }
  return;
}

HRESULT CHttpServer::CRequest::SetResponseStatus(_In_ LONG nStatus, _In_opt_ LPCSTR szReasonA)
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
    if (sResponse.cStrReasonA.Copy(szReasonA) == FALSE)
      return E_OUTOFMEMORY;
  }
  else
  {
    if (CHttpServer::LocateError(nStatus) == NULL)
      return E_INVALIDARG;
    sResponse.cStrReasonA.Empty();
  }
  sResponse.nStatus = nStatus;
  //done
  return S_OK;
}

HRESULT CHttpServer::CRequest::AddResponseHeader(_In_z_ LPCSTR szNameA, _Out_opt_ CHttpHeaderBase **lplpHeader,
                                                 _In_ BOOL bReplaceExisting)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  return sResponse.cHttpCmn.AddHeader(szNameA, lplpHeader, bReplaceExisting);
}

HRESULT CHttpServer::CRequest::AddResponseHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA,
                                                 _In_opt_ SIZE_T nValueLen, _Out_opt_ CHttpHeaderBase **lplpHeader,
                                                 _In_ BOOL bReplaceExisting)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  return sResponse.cHttpCmn.AddHeader(szNameA, szValueA, nValueLen, lplpHeader, bReplaceExisting);
}

HRESULT CHttpServer::CRequest::AddResponseHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW,
                                                 _In_opt_ SIZE_T nValueLen, _Out_opt_ CHttpHeaderBase **lplpHeader,
                                                 _In_ BOOL bReplaceExisting)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  return sResponse.cHttpCmn.AddHeader(szNameA, szValueW, nValueLen, lplpHeader, bReplaceExisting);
}

HRESULT CHttpServer::CRequest::RemoveResponseHeader(_In_z_ LPCSTR szNameA)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  sResponse.cHttpCmn.RemoveHeader(szNameA);
  return S_OK;
}

HRESULT CHttpServer::CRequest::RemoveResponseHeader(_In_ CHttpHeaderBase *lpHeader)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  sResponse.cHttpCmn.RemoveHeader(lpHeader);
  return S_OK;
}

HRESULT CHttpServer::CRequest::RemoveAllResponseHeaders()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  sResponse.cHttpCmn.RemoveAllHeaders();
  return S_OK;
}

SIZE_T CHttpServer::CRequest::GetResponseHeadersCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return 0;
  return sResponse.cHttpCmn.GetHeadersCount();
}

CHttpHeaderBase* CHttpServer::CRequest::GetResponseHeader(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sResponse.cHttpCmn.GetHeader(nIndex);
}

CHttpHeaderBase* CHttpServer::CRequest::GetResponseHeader(_In_z_ LPCSTR szNameA) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sResponse.cHttpCmn.GetHeader(szNameA);
}

HRESULT CHttpServer::CRequest::AddResponseCookie(_In_ CHttpCookie &cSrc)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  return sResponse.cHttpCmn.AddCookie(cSrc);
}

HRESULT CHttpServer::CRequest::AddResponseCookie(_In_ CHttpCookieArray &cSrc)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  return sResponse.cHttpCmn.AddCookie(cSrc);
}

HRESULT CHttpServer::CRequest::AddResponseCookie(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA,
                                                 _In_opt_z_ LPCSTR szDomainA, _In_opt_z_ LPCSTR szPathA,
                                                 _In_opt_ const CDateTime *lpDate, _In_opt_ BOOL bIsSecure,
                                                 _In_opt_ BOOL bIsHttpOnly)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  return sResponse.cHttpCmn.AddCookie(szNameA, szValueA, szDomainA, szPathA, lpDate, bIsSecure, bIsHttpOnly);
}

HRESULT CHttpServer::CRequest::AddResponseCookie(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szValueW,
                                                 _In_opt_z_ LPCWSTR szDomainW, _In_opt_z_ LPCWSTR szPathW,
                                                 _In_opt_ const CDateTime *lpDate, _In_opt_ BOOL bIsSecure,
                                                 _In_opt_ BOOL bIsHttpOnly)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  return sResponse.cHttpCmn.AddCookie(szNameW, szValueW, szDomainW, szPathW, lpDate, bIsSecure, bIsHttpOnly);
}

HRESULT CHttpServer::CRequest::RemoveResponseCookie(_In_z_ LPCSTR szNameA)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  return sResponse.cHttpCmn.RemoveCookie(szNameA);
}

HRESULT CHttpServer::CRequest::RemoveResponseCookie(_In_z_ LPCWSTR szNameW)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  return sResponse.cHttpCmn.RemoveCookie(szNameW);
}

HRESULT CHttpServer::CRequest::RemoveAllResponseCookies()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (HasHeadersBeenSent() != FALSE)
    return E_FAIL;
  sResponse.cHttpCmn.RemoveAllCookies();
  return S_OK;
}

CHttpCookie* CHttpServer::CRequest::GetResponseCookie(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sResponse.cHttpCmn.GetCookie(nIndex);
}

CHttpCookie* CHttpServer::CRequest::GetResponseCookie(_In_z_ LPCSTR szNameA) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sResponse.cHttpCmn.GetCookie(szNameA);
}

CHttpCookie* CHttpServer::CRequest::GetResponseCookie(_In_z_ LPCWSTR szNameW) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sResponse.cHttpCmn.GetCookie(szNameW);
}

CHttpCookieArray* CHttpServer::CRequest::GetResponseCookies() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sResponse.cHttpCmn.GetCookies();
}

SIZE_T CHttpServer::CRequest::GetResponseCookiesCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return 0;
  return sResponse.cHttpCmn.GetCookiesCount();
}

HRESULT CHttpServer::CRequest::SendHeaders()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (sResponse.bDirect == FALSE)
    return MX_E_InvalidState;
  return BuildAndInsertOrSendHeaderStream();
}

HRESULT CHttpServer::CRequest::SendResponse(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen)
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
  if (sResponse.bDirect == FALSE)
  {
    if (sResponse.aStreamsList.GetCount() > 0 && sResponse.bLastStreamIsData != FALSE)
    {
      CStream *lpStream = sResponse.aStreamsList.GetElementAt(sResponse.aStreamsList.GetCount()-1);
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
        if (sResponse.aStreamsList.AddElement(cStream.Get()) != FALSE)
          cStream.Detach();
        else
          hRes = E_OUTOFMEMORY;
      }
      if (SUCCEEDED(hRes))
        sResponse.bLastStreamIsData = TRUE;
    }
    if (FAILED(hRes))
      return hRes;
    sResponse.szMimeTypeHintA = NULL;
    sResponse.cStrFileNameA.Empty();
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

HRESULT CHttpServer::CRequest::SendFile(_In_z_ LPCWSTR szFileNameW)
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

HRESULT CHttpServer::CRequest::SendStream(_In_ CStream *lpStream, _In_opt_z_ LPCWSTR szFileNameW)
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
  if (sResponse.bDirect == FALSE)
  {
    //extract name if needed
    bSetInfo = FALSE;
    if (szFileNameW != NULL && *szFileNameW != 0 && sResponse.aStreamsList.GetCount() == 0)
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
    if (sResponse.aStreamsList.AddElement(lpStream) == FALSE)
      return E_OUTOFMEMORY;
    lpStream->AddRef();
    //set response info
    if (bSetInfo != FALSE)
    {
      sResponse.szMimeTypeHintA = CHttpCommon::GetMimeType(szFileNameW);
      sResponse.cStrFileNameA.Attach(cStrTempNameA.Detach());
    }
    else
    {
      sResponse.szMimeTypeHintA = NULL;
      sResponse.cStrFileNameA.Empty();
    }
    sResponse.bLastStreamIsData = FALSE;
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

HRESULT CHttpServer::CRequest::SendErrorPage(_In_ LONG nStatusCode, _In_ HRESULT hErrorCode,
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
    hRes = lpHttpServer->GenerateErrorPage(cStrResponseA, this, nStatusCode, hErrorCode, szBodyExplanationA);
    if (FAILED(hRes))
      return hRes;
    _InterlockedOr(&nFlags, REQUEST_FLAG_ErrorPageSent);
  }
  return SendResponse((LPCSTR)cStrResponseA, cStrResponseA.GetLength());
}

HANDLE CHttpServer::CRequest::GetUnderlyingSocket() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  return (nState == StateBuildingResponse) ? hConn : NULL;
}

CSockets* CHttpServer::CRequest::GetUnderlyingSocketManager() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  return (nState == StateBuildingResponse) ? const_cast<CSockets*>(lpSocketMgr) : NULL;
}

VOID CHttpServer::CRequest::ResetForNewRequest()
{
  nState = CRequest::StateInitializing;
  MX_HTTP_DEBUG_PRINT(1, ("HttpServer(State/0x%p): StateInitializing [ResetForNewRequest]\n", this));
  _InterlockedExchange(&nFlags, 0);
  //----
  sRequest.ResetForNewRequest();
  sResponse.ResetForNewRequest();
  return;
}

BOOL CHttpServer::CRequest::IsKeepAliveRequest() const
{
  int nHttpVersion[2];

  nHttpVersion[0] = sRequest.cHttpCmn.GetRequestVersionMajor();
  nHttpVersion[1] = sRequest.cHttpCmn.GetRequestVersionMinor();
  return (nHttpVersion[0] > 0) ? sRequest.cHttpCmn.IsKeepAliveRequest() : FALSE;
}

BOOL CHttpServer::CRequest::HasErrorBeenSent() const
{
  return ((__InterlockedRead(const_cast<LONG volatile*>(&nFlags)) & REQUEST_FLAG_ErrorPageSent) != 0) ? TRUE : FALSE;
}

BOOL CHttpServer::CRequest::HasHeadersBeenSent() const
{
  return ((__InterlockedRead(const_cast<LONG volatile*>(&nFlags)) & REQUEST_FLAG_HeadersSent) != 0) ? TRUE : FALSE;
}

HRESULT CHttpServer::CRequest::BuildAndInsertOrSendHeaderStream()
{
  TAutoRefCounted<CMemoryStream> cHdrStream;
  CStringA cStrTempA;
  int nHttpVersion[2];
  LONG nStatus;
  LPCSTR sA;
  HRESULT hRes;

  if ((_InterlockedOr(&nFlags, REQUEST_FLAG_HeadersSent) & REQUEST_FLAG_HeadersSent) != 0)
    return S_OK;

  cHdrStream.Attach(MX_DEBUG_NEW CMemoryStream(65536));
  if (!cHdrStream)
    return E_OUTOFMEMORY;
  hRes = cHdrStream->Create();
  //status
  if (SUCCEEDED(hRes))
  {
    if ((nStatus = sResponse.nStatus) == 0)
    {
      if (sResponse.cHttpCmn.GetHeader<CHttpHeaderRespLocation>() != NULL)
        nStatus = 302;
      else
        nStatus = 200;
      sA = CHttpServer::LocateError(nStatus);
    }
    else
    {
      sA = (sResponse.cStrReasonA.IsEmpty() != FALSE) ? CHttpServer::LocateError(nStatus) :
                                                        (LPCSTR)(sResponse.cStrReasonA);
    }
    if (sA == NULL || *sA == 0)
      sA = "Undefined";
    nHttpVersion[0] = sRequest.cHttpCmn.GetRequestVersionMajor();
    nHttpVersion[1] = sRequest.cHttpCmn.GetRequestVersionMinor();
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

    lpHdr = sResponse.cHttpCmn.GetHeader("Date");
    if (lpHdr != NULL)
    {
      hRes = lpHdr->Build(cStrTempA);
      if (SUCCEEDED(hRes))
        hRes = cHdrStream->WriteString("Date:%s\r\n", (LPSTR)cStrTempA);
    }
    else
    {
      CDateTime cDtNow;

      hRes = cDtNow.SetFromNow(FALSE);
      if (SUCCEEDED(hRes))
        hRes = cDtNow.Format(cStrTempA, "%a, %d %b %Y %H:%M:%S %z");
      if (SUCCEEDED(hRes))
        hRes = cHdrStream->WriteString("Date:%s\r\n", (LPSTR)cStrTempA);
    }
  }
  //location (if any)
  if (SUCCEEDED(hRes))
  {
    CHttpHeaderRespLocation *lpHdr;

    lpHdr = sResponse.cHttpCmn.GetHeader<CHttpHeaderRespLocation>();
    if (lpHdr != NULL)
    {
      hRes = lpHdr->Build(cStrTempA);
      if (SUCCEEDED(hRes))
        hRes = cHdrStream->WriteString("Location:%s\r\n", (LPSTR)cStrTempA);
    }
  }
  //server
  if (SUCCEEDED(hRes))
  {
    CHttpHeaderBase *lpHdr;

    lpHdr = sResponse.cHttpCmn.GetHeader("Server");
    if (lpHdr != NULL)
    {
      hRes = lpHdr->Build(cStrTempA);
      if (SUCCEEDED(hRes))
        hRes = cHdrStream->WriteString("Server:%s\r\n", (LPSTR)cStrTempA);
    }
    else
    {
      hRes = cHdrStream->WriteString("Server:%s\r\n", szServerInfoA);
    }
  }
  //connection
  if (SUCCEEDED(hRes))
  {
    CHttpHeaderBase *lpHdr;

    lpHdr = sResponse.cHttpCmn.GetHeader("Connection");
    if (lpHdr != NULL)
    {
      hRes = lpHdr->Build(cStrTempA);
      if (SUCCEEDED(hRes))
        hRes = cHdrStream->WriteString("Connection:%s\r\n", (LPSTR)cStrTempA);
    }
    else if (IsKeepAliveRequest() != FALSE)
    {
      hRes = cHdrStream->WriteString("Connection:Keep-Alive\r\n");
    }
  }
  //content type
  if (SUCCEEDED(hRes) && sResponse.aStreamsList.GetCount() > 0)
  {
    CHttpHeaderEntContentType *lpHdr;

    lpHdr = sResponse.cHttpCmn.GetHeader<CHttpHeaderEntContentType>();
    if (lpHdr == NULL)
    {
      //only send automatic "Content-Type" header if not direct response
      if (sResponse.bDirect == FALSE)
      {
        if (sResponse.szMimeTypeHintA != NULL)
          hRes = cHdrStream->WriteString("Content-Type:%s\r\n", sResponse.szMimeTypeHintA);
        else if (sResponse.bLastStreamIsData != FALSE)
          hRes = cHdrStream->WriteString("Content-Type:text/html; charset=utf-8\r\n");
        else
          hRes = cHdrStream->WriteString("Content-Type:application/octet-stream\r\n");
      }
    }
    else
    {
      hRes = lpHdr->Build(cStrTempA);
      if (SUCCEEDED(hRes))
        hRes = cHdrStream->WriteString("Content-Type:%s\r\n", (LPSTR)cStrTempA);
    }
  }
  //content length
  if (SUCCEEDED(hRes))
  {
    CHttpHeaderEntContentLength *lpHdr;
    CHttpHeaderGenTransferEncoding *lpHdr2;
    ULONGLONG nTotalLength, nLen;
    SIZE_T i, nCount;

    lpHdr2 = sResponse.cHttpCmn.GetHeader<CHttpHeaderGenTransferEncoding>();
    if (lpHdr2 == NULL || lpHdr2->GetEncoding() != CHttpHeaderGenTransferEncoding::EncodingChunked)
    {
      lpHdr = sResponse.cHttpCmn.GetHeader<CHttpHeaderEntContentLength>();
      if (lpHdr == NULL)
      {
        //only send automatic "Content-Length" header if not direct response
        if (sResponse.bDirect == FALSE)
        {
          nTotalLength = 0ui64;
          nCount = sResponse.aStreamsList.GetCount();
          for (i = 0; i < nCount; i++)
          {
            nLen = sResponse.aStreamsList[i]->GetLength();
            if (nLen + nTotalLength < nTotalLength)
              return MX_E_BadLength;
            nTotalLength += nLen;
          }
          hRes = cHdrStream->WriteString("Content-Length:%I64u\r\n", nTotalLength);
        }
      }
      else
      {
        hRes = lpHdr->Build(cStrTempA);
        if (SUCCEEDED(hRes))
          hRes = cHdrStream->WriteString("Content-Length:%s\r\n", (LPSTR)cStrTempA);
      }
    }
  }
  //rest of headers
  if (SUCCEEDED(hRes))
  {
    CHttpHeaderBase *lpHdr;
    SIZE_T i, nCount;

    nCount = sResponse.cHttpCmn.GetHeadersCount();
    for (i=0; SUCCEEDED(hRes) && i<nCount; i++)
    {
      lpHdr = sResponse.cHttpCmn.GetHeader(i);
      if (StrCompareA(lpHdr->GetName(), "Content-Type") != 0 &&
          StrCompareA(lpHdr->GetName(), "Content-Length") != 0 &&
          StrCompareA(lpHdr->GetName(), "Date") != 0 &&
          StrCompareA(lpHdr->GetName(), "Server") != 0 &&
          StrCompareA(lpHdr->GetName(), "Connection") != 0 &&
          StrCompareA(lpHdr->GetName(), "Location") != 0)
      {
        hRes = lpHdr->Build(cStrTempA);
        if (SUCCEEDED(hRes))
          hRes = cHdrStream->WriteString("%s:%s\r\n", lpHdr->GetName(), (LPSTR)cStrTempA);
      }
    }
  }
  //cookies
  if (SUCCEEDED(hRes))
  {
    CHttpCookie *lpCookie;
    SIZE_T i, nCount;

    nCount = sResponse.cHttpCmn.GetCookiesCount();
    for (i=0; SUCCEEDED(hRes) && i<nCount; i++)
    {
      lpCookie = sResponse.cHttpCmn.GetCookie(i);
      hRes = lpCookie->ToString(cStrTempA);
      if (SUCCEEDED(hRes))
        hRes = cHdrStream->WriteString("Set-Cookie:%s\r\n", (LPSTR)cStrTempA);
    }
  }
  //add end of header
  if (SUCCEEDED(hRes))
    hRes = cHdrStream->WriteString("\r\n");
  //add at the beginning of stream list if not direct, else send
  if (SUCCEEDED(hRes))
  {
    if (sResponse.bDirect == FALSE)
    {
      if (sResponse.aStreamsList.InsertElementAt(cHdrStream.Get(), 0) != FALSE)
        cHdrStream.Detach();
      else
        hRes = E_OUTOFMEMORY;
    }
    else
    {
      hRes = lpHttpServer->cSocketMgr.SendStream(hConn, cHdrStream.Get());
    }
  }
  MX_HTTP_DEBUG_PRINT(1, ("HttpServer(ResponseHeaders/0x%p): 0x%08X\n", this, hRes));
  //done
  return hRes;
}

HRESULT CHttpServer::CRequest::SendQueuedStreams()
{
  CStream *lpStream;
  SIZE_T i, nCount;
  HRESULT hRes;

  hRes = S_OK;
  nCount = sResponse.aStreamsList.GetCount();
  for (i=0; SUCCEEDED(hRes) && i<nCount; i++)
  {
    lpStream = sResponse.aStreamsList.GetElementAt(i);
    hRes = lpHttpServer->cSocketMgr.SendStream(hConn, lpStream);
  }
  //done
  return hRes;
}

VOID CHttpServer::CRequest::MarkLinkAsClosed()
{
  _InterlockedOr(&nFlags, REQUEST_FLAG_LinkClosed);
  return;
}

BOOL CHttpServer::CRequest::IsLinkClosed() const
{
  return ((__InterlockedRead(const_cast<LONG volatile*>(&nFlags)) & REQUEST_FLAG_LinkClosed) != 0) ? TRUE : FALSE;
}

} //namespace MX
