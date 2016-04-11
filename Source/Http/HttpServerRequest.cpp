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

CHttpServer::CRequest::CRequest(__in CHttpServer *_lpHttpServer, __in CPropertyBag &_cPropBag) :
                       CBaseMemObj(), TLnkLstNode<CRequest>(), CIpc::CUserData(), sRequest(_cPropBag),
                       sResponse(_cPropBag)
{
  _InterlockedExchange(&nMutex, 0);
  MX_ASSERT(_lpHttpServer != NULL);
  lpHttpServer = _lpHttpServer;
  hConn = NULL;
  nState = CRequest::StateReceivingRequestHeaders;
  _InterlockedExchange(&nFlags, 0);
  sRequest.liLastRead.QuadPart = 0ui64;
  _InterlockedExchange(&(sRequest.sTimeout.nMutex), 0);
  sResponse.nStatus = 0;
  sResponse.bLastStreamIsData = FALSE;
  sResponse.szMimeTypeHintA = NULL;
  ResetForNewRequest();
  return;
}

CHttpServer::CRequest::~CRequest()
{
  MX_ASSERT(sRequest.sTimeout.cActiveList.HasPending() == FALSE);
  lpHttpServer->OnRequestDestroyed(this);
  return;
}

LPCSTR CHttpServer::CRequest::GetMethod() const
{
  CFastLock cLock(const_cast<LONG volatile*>(&nMutex));

  if (nState != StateBuildingResponse)
    return "";
  return sRequest.cHttpCmn.GetRequestMethod();
}

CUrl* CHttpServer::CRequest::GetUrl() const
{
  CFastLock cLock(const_cast<LONG volatile*>(&nMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sRequest.cHttpCmn.GetRequestUri();
}

SIZE_T CHttpServer::CRequest::GetRequestHeadersCount() const
{
  CFastLock cLock(const_cast<LONG volatile*>(&nMutex));

  if (nState != StateBuildingResponse)
    return 0;
  return sRequest.cHttpCmn.GetHeadersCount();
}

CHttpHeaderBase* CHttpServer::CRequest::GetRequestHeader(__in SIZE_T nIndex) const
{
  CFastLock cLock(const_cast<LONG volatile*>(&nMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sRequest.cHttpCmn.GetHeader(nIndex);
}

CHttpHeaderBase* CHttpServer::CRequest::GetRequestHeader(__in_z LPCSTR szNameA) const
{
  CFastLock cLock(const_cast<LONG volatile*>(&nMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sRequest.cHttpCmn.GetHeader(szNameA);
}

SIZE_T CHttpServer::CRequest::GetRequestCookiesCount() const
{
  CFastLock cLock(const_cast<LONG volatile*>(&nMutex));

  if (nState != StateBuildingResponse)
    return 0;
  return sRequest.cHttpCmn.GetCookiesCount();
}

CHttpCookie* CHttpServer::CRequest::GetRequestCookie(__in SIZE_T nIndex) const
{
  CFastLock cLock(const_cast<LONG volatile*>(&nMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sRequest.cHttpCmn.GetCookie(nIndex);
}

CHttpBodyParserBase* CHttpServer::CRequest::GetRequestBodyParser() const
{
  CFastLock cLock(const_cast<LONG volatile*>(&nMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sRequest.cHttpCmn.GetBodyParser();
}

VOID CHttpServer::CRequest::ResetResponse()
{
  CFastLock cLock(&nMutex);

  if (nState == StateBuildingResponse)
  {
    sResponse.nStatus = 0;
    sResponse.cStrReasonA.Empty();
    sResponse.cHttpCmn.ResetParser();
    sResponse.aStreamsList.RemoveAllElements();
    sResponse.bLastStreamIsData = FALSE;
    sResponse.szMimeTypeHintA = NULL;
    sResponse.cStrFileNameA.Empty();
  }
  return;
}

HRESULT CHttpServer::CRequest::SetResponseStatus(__in LONG nStatus, __in_opt LPCSTR szReasonA)
{
  CFastLock cLock(&nMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
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

HRESULT CHttpServer::CRequest::AddResponseHeader(__in_z LPCSTR szNameA, __out_opt CHttpHeaderBase **lplpHeader)
{
  CFastLock cLock(&nMutex);

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  return sResponse.cHttpCmn.AddHeader(szNameA, lplpHeader);
}

HRESULT CHttpServer::CRequest::AddResponseHeader(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA,
                                                 __in_opt SIZE_T nValueLen, __out_opt CHttpHeaderBase **lplpHeader)
{
  CFastLock cLock(&nMutex);

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  return sResponse.cHttpCmn.AddHeader(szNameA, szValueA, nValueLen, lplpHeader);
}

HRESULT CHttpServer::CRequest::AddResponseHeader(__in_z LPCSTR szNameA, __in_z LPCWSTR szValueW,
                                                 __in_opt SIZE_T nValueLen, __out_opt CHttpHeaderBase **lplpHeader)
{
  CFastLock cLock(&nMutex);

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  return sResponse.cHttpCmn.AddHeader(szNameA, szValueW, nValueLen, lplpHeader);
}

HRESULT CHttpServer::CRequest::RemoveResponseHeader(__in_z LPCSTR szNameA)
{
  CFastLock cLock(&nMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  return sResponse.cHttpCmn.RemoveHeader(szNameA);
}

HRESULT CHttpServer::CRequest::RemoveAllResponseHeaders()
{
  CFastLock cLock(&nMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  sResponse.cHttpCmn.RemoveAllHeaders();
  return S_OK;
}

SIZE_T CHttpServer::CRequest::GetResponseHeadersCount() const
{
  CFastLock cLock(const_cast<LONG volatile*>(&nMutex));

  if (nState != StateBuildingResponse)
    return 0;
  return sResponse.cHttpCmn.GetHeadersCount();
}

CHttpHeaderBase* CHttpServer::CRequest::GetResponseHeader(__in_z LPCSTR szNameA) const
{
  CFastLock cLock(const_cast<LONG volatile*>(&nMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sResponse.cHttpCmn.GetHeader(szNameA);
}

HRESULT CHttpServer::CRequest::AddResponseCookie(__in CHttpCookie &cSrc)
{
  CFastLock cLock(&nMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  return sResponse.cHttpCmn.AddCookie(cSrc);
}

HRESULT CHttpServer::CRequest::AddResponseCookie(__in CHttpCookieArray &cSrc)
{
  CFastLock cLock(&nMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  return sResponse.cHttpCmn.AddCookie(cSrc);
}

HRESULT CHttpServer::CRequest::AddResponseCookie(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA,
                                                 __in_z_opt LPCSTR szDomainA, __in_z_opt LPCSTR szPathA,
                                                 __in_opt const CDateTime *lpDate, __in_opt BOOL bIsSecure,
                                                 __in_opt BOOL bIsHttpOnly)
{
  CFastLock cLock(&nMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  return sResponse.cHttpCmn.AddCookie(szNameA, szValueA, szDomainA, szPathA, lpDate, bIsSecure, bIsHttpOnly);
}

HRESULT CHttpServer::CRequest::AddResponseCookie(__in_z LPCWSTR szNameW, __in_z LPCWSTR szValueW,
                                                 __in_z_opt LPCWSTR szDomainW, __in_z_opt LPCWSTR szPathW,
                                                 __in_opt const CDateTime *lpDate, __in_opt BOOL bIsSecure,
                                                 __in_opt BOOL bIsHttpOnly)
{
  CFastLock cLock(&nMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  return sResponse.cHttpCmn.AddCookie(szNameW, szValueW, szDomainW, szPathW, lpDate, bIsSecure, bIsHttpOnly);
}

HRESULT CHttpServer::CRequest::RemoveResponseCookie(__in_z LPCSTR szNameA)
{
  CFastLock cLock(&nMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  return sResponse.cHttpCmn.RemoveCookie(szNameA);
}

HRESULT CHttpServer::CRequest::RemoveResponseCookie(__in_z LPCWSTR szNameW)
{
  CFastLock cLock(&nMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  return sResponse.cHttpCmn.RemoveCookie(szNameW);
}

HRESULT CHttpServer::CRequest::RemoveAllResponseCookies()
{
  CFastLock cLock(&nMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  sResponse.cHttpCmn.RemoveAllCookies();
  return S_OK;
}

CHttpCookie* CHttpServer::CRequest::GetResponseCookie(__in SIZE_T nIndex) const
{
  CFastLock cLock(const_cast<LONG volatile*>(&nMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sResponse.cHttpCmn.GetCookie(nIndex);
}

CHttpCookie* CHttpServer::CRequest::GetResponseCookie(__in_z LPCSTR szNameA) const
{
  CFastLock cLock(const_cast<LONG volatile*>(&nMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sResponse.cHttpCmn.GetCookie(szNameA);
}

CHttpCookie* CHttpServer::CRequest::GetResponseCookie(__in_z LPCWSTR szNameW) const
{
  CFastLock cLock(const_cast<LONG volatile*>(&nMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sResponse.cHttpCmn.GetCookie(szNameW);
}

CHttpCookieArray* CHttpServer::CRequest::GetResponseCookies() const
{
  CFastLock cLock(const_cast<LONG volatile*>(&nMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return sResponse.cHttpCmn.GetCookies();
}

SIZE_T CHttpServer::CRequest::GetResponseCookiesCount() const
{
  CFastLock cLock(const_cast<LONG volatile*>(&nMutex));

  if (nState != StateBuildingResponse)
    return 0;
  return sResponse.cHttpCmn.GetCookiesCount();
}

HRESULT CHttpServer::CRequest::SendResponse(__in LPCVOID lpData, __in SIZE_T nDataLen)
{
  CFastLock cLock(&nMutex);
  SIZE_T nWritten;
  HRESULT hRes;

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (nDataLen == 0)
    return S_OK;
  if (lpData == NULL)
    return E_POINTER;
  if (sResponse.aStreamsList.GetCount() > 0 && sResponse.bLastStreamIsData != FALSE)
  {
    CStream *lpStream;

    lpStream = sResponse.aStreamsList.GetElementAt(sResponse.aStreamsList.GetCount()-1);
    hRes = lpStream->Write(lpData, nDataLen, nWritten);
  }
  else
  {
    CMemoryStream *lpStream;

    lpStream = MX_DEBUG_NEW CMemoryStream(128*1024);
    if (lpStream == NULL)
      return E_OUTOFMEMORY;
    hRes = lpStream->Create(nDataLen);
    if (SUCCEEDED(hRes))
      hRes = lpStream->Write(lpData, nDataLen, nWritten);
    if (SUCCEEDED(hRes) && sResponse.aStreamsList.AddElement(lpStream) == FALSE)
      hRes = E_OUTOFMEMORY;
    if (FAILED(hRes))
    {
      lpStream->Release();
      return hRes;
    }
    sResponse.bLastStreamIsData = TRUE;
  }
  sResponse.szMimeTypeHintA = NULL;
  sResponse.cStrFileNameA.Empty();
  //done
  return S_OK;
}

HRESULT CHttpServer::CRequest::SendFile(__in_z LPCWSTR szFileNameW)
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

HRESULT CHttpServer::CRequest::SendStream(__in CStream *lpStream, __in_z_opt LPCWSTR szFileNameW)
{
  CFastLock cLock(&nMutex);
  CStringA cStrTempNameA;
  LPCWSTR sW[2];
  BOOL bSetInfo;
  HRESULT hRes;

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  if (lpStream == NULL)
    return E_POINTER;
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
  //done
  return S_OK;
}

HRESULT CHttpServer::CRequest::SendErrorPage(__in LONG nStatusCode, __in HRESULT hErrorCode,
                                             __in_z_opt LPCSTR szBodyExplanationA)
{
  CFastLock cLock(&nMutex);

  if (nState != StateBuildingResponse)
    return MX_E_NotReady;
  _InterlockedOr(&nFlags, REQUEST_FLAG_DontKeepAlive);
  return lpHttpServer->SendGenericErrorPage(this, nStatusCode, hErrorCode, szBodyExplanationA);
}

HANDLE CHttpServer::CRequest::GetUnderlyingSocket() const
{
  CFastLock cLock(const_cast<LONG volatile*>(&nMutex));

  return (nState == StateBuildingResponse) ? hConn : NULL;
}

CSockets* CHttpServer::CRequest::GetUnderlyingSocketManager() const
{
  CFastLock cLock(const_cast<LONG volatile*>(&nMutex));

  return (nState == StateBuildingResponse) ? const_cast<CSockets*>(lpSocketMgr) : NULL;
}

VOID CHttpServer::CRequest::SetUserData(__in CRequestUserData *lpUserData)
{
  CFastLock cLock(&nMutex);

  cUserData = lpUserData;
  return;
}

CHttpServer::CRequestUserData* CHttpServer::CRequest::GetUserData() const
{
  CFastLock cLock(const_cast<LONG volatile*>(&nMutex));
  CRequestUserData *lpUserData;

  lpUserData = cUserData.Get();
  if (lpUserData != NULL)
    lpUserData->AddRef();
  return lpUserData;
}

VOID CHttpServer::CRequest::ResetForNewRequest()
{
  nState = CRequest::StateReceivingRequestHeaders;
  _InterlockedExchange(&nFlags, 0);
  //----
  sRequest.cUrl.Reset();
  sRequest.cStrMethodA.Empty();
  sRequest.cHttpCmn.ResetParser();
  //----
  sResponse.nStatus = 0;
  sResponse.cStrReasonA.Empty();
  sResponse.cHttpCmn.ResetParser();
  sResponse.aStreamsList.RemoveAllElements();
  sResponse.bLastStreamIsData = FALSE;
  sResponse.szMimeTypeHintA = NULL;
  sResponse.cStrFileNameA.Empty();
  //----
  cUserData.Release();
  return;
}

BOOL CHttpServer::CRequest::IsKeepAliveRequest() const
{
  return FALSE;
  int nHttpVersion[2];

  nHttpVersion[0] = sRequest.cHttpCmn.GetRequestVersionMajor();
  nHttpVersion[1] = sRequest.cHttpCmn.GetRequestVersionMinor();
  return (nHttpVersion[0] > 0) ? sRequest.cHttpCmn.IsKeepAliveRequest() : FALSE;
}

BOOL CHttpServer::CRequest::HasErrorBeenSent() const
{
  return ((__InterlockedRead(const_cast<LONG volatile*>(&nFlags)) & REQUEST_FLAG_ErrorPageSent) != 0) ? TRUE : FALSE;
}

HRESULT CHttpServer::CRequest::BuildAndInsertHeaderStream()
{
  TAutoRefCounted<CMemoryStream> cHdrStream;
  CStringA cStrTempA;
  int nHttpVersion[2];
  LONG nStatus;
  LPCSTR sA;
  HRESULT hRes;

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
      if (sResponse.szMimeTypeHintA != NULL)
        hRes = cHdrStream->WriteString("Content-Type:%s\r\n", sResponse.szMimeTypeHintA);
      else if (sResponse.bLastStreamIsData != FALSE)
        hRes = cHdrStream->WriteString("Content-Type:text/html; charset=utf-8\r\n");
      else
        hRes = cHdrStream->WriteString("Content-Type:application/octet-stream\r\n");
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
        nTotalLength = 0ui64;
        nCount = sResponse.aStreamsList.GetCount();
        for (i=0; i<nCount; i++)
        {
          nLen = sResponse.aStreamsList[i]->GetLength();
          if (nLen+nTotalLength < nTotalLength)
            return MX_E_BadLength;
          nTotalLength += nLen;
        }
        hRes = cHdrStream->WriteString("Content-Length:%I64u\r\n", nTotalLength);
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
  //add at the beginning of stream list
  if (SUCCEEDED(hRes))
  {
    if (sResponse.aStreamsList.InsertElementAt(cHdrStream.Get(), 0) != FALSE)
      cHdrStream.Detach();
    else
      hRes = E_OUTOFMEMORY;
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
