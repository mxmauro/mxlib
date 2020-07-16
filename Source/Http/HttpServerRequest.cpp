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

//-----------------------------------------------------------

static const CHAR szServerInfoA[] = "MX-Library";
static const SIZE_T nServerInfoLen = MX_ARRAYLEN(szServerInfoA) - 1;
static MX::CUrl cZeroUrl;

//-----------------------------------------------------------

static HRESULT WriteToStream(_In_ MX::CMemoryStream *lpStream, _In_ LPCSTR szStrA,
                             _In_opt_ SIZE_T nStrLen = (SIZE_T)-1);

//-----------------------------------------------------------

namespace MX {

CHttpServer::CClientRequest::CClientRequest() : CIpc::CUserData()
{
  lpHttpServer = NULL;
  lpSocketMgr = NULL;
  hConn = NULL;
  ::MxMemSet(&sPeerAddr, 0, sizeof(sPeerAddr));
  nState = StateInactive;
  RundownProt_Initialize(&nTimerCallbackRundownLock);
  _InterlockedExchange(&nFlags, 0);
  hrErrorCode = S_OK;
  _InterlockedExchange(&nHeadersTimeoutTimerId, 0);
  _InterlockedExchange(&nThroughputTimerId, 0);
  dwLowThroughputCounter = 0;
  _InterlockedExchange(&nGracefulTerminationTimerId, 0);
  _InterlockedExchange(&nKeepAliveTimeoutTimerId, 0);
  sResponse.nStatus = 0;
  sResponse.bLastStreamIsData = FALSE;
  sResponse.szMimeTypeHintA = NULL;
  sResponse.bIsInline = FALSE;
  sResponse.bDirect = sResponse.bPreserveWebSocketHeaders = FALSE;
  return;
}

CHttpServer::CClientRequest::~CClientRequest()
{
  MX_ASSERT(nState == StateInactive || nState == StateWebSocket || nState == StateTerminated);
  MX_ASSERT(__InterlockedRead(&nHeadersTimeoutTimerId) == 0);
  MX_ASSERT(__InterlockedRead(&nThroughputTimerId) == 0);
  MX_ASSERT(__InterlockedRead(&nGracefulTerminationTimerId) == 0);
  MX_ASSERT(__InterlockedRead(&nKeepAliveTimeoutTimerId) == 0);
  return;
}

VOID CHttpServer::CClientRequest::End(_In_opt_ HRESULT hrErrorCode)
{
  if (lpHttpServer)
  {
    lpHttpServer->OnRequestEnding(this, hrErrorCode);
  }
  return;
}

BOOL CHttpServer::CClientRequest::IsAlive() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));

  if (nState == StateAfterHeaders || nState == StateBuildingResponse || nState == StateSendingResponse ||
      nState == StateNegotiatingWebSocket)
  {
    if (lpHttpServer != NULL)
    {
      if (lpHttpServer->cShutdownEv.Wait(0) == FALSE)
        return TRUE;
    }
  }
  return FALSE;
}

BOOL CHttpServer::CClientRequest::MustAbort() const
{
  return (lpHttpServer != NULL) ? lpHttpServer->cShutdownEv.Wait(0) : TRUE;
}

HANDLE CHttpServer::CClientRequest::GetAbortEvent() const
{
  return (lpHttpServer != NULL) ? lpHttpServer->cShutdownEv.Get() : NULL;
}

HRESULT CHttpServer::CClientRequest::EnableDirectResponse()
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));
  HRESULT hRes;

  if (nState != StateBuildingResponse && nState != StateAfterHeaders)
    return MX_E_InvalidState;

  hRes = SetState(StateSendingResponse);
  if (SUCCEEDED(hRes))
    sResponse.bDirect = TRUE;
  return hRes;
}

LPCSTR CHttpServer::CClientRequest::GetMethod() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return "";
  return cRequestParser.GetRequestMethod();
}

CUrl* CHttpServer::CClientRequest::GetUrl() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return &cZeroUrl;
  return cRequestParser.GetRequestUri();
}

CHttpBodyParserBase* CHttpServer::CClientRequest::GetRequestBodyParser() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateBuildingResponse)
    return NULL;
  return cRequestParser.GetBodyParser();
}

SIZE_T CHttpServer::CClientRequest::GetRequestHeadersCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return 0;
  return cRequestParser.Headers().GetCount();
}

CHttpHeaderBase* CHttpServer::CClientRequest::GetRequestHeader(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));
  CHttpHeaderArray *lpHeadersArray;
  CHttpHeaderBase *lpHeader;

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return NULL;
  lpHeadersArray = &(cRequestParser.Headers());
  if (nIndex >= lpHeadersArray->GetCount())
    return NULL;
  lpHeader = lpHeadersArray->GetElementAt(nIndex);
  lpHeader->AddRef();
  return lpHeader;
}

CHttpHeaderBase* CHttpServer::CClientRequest::GetRequestHeaderByName(_In_z_ LPCSTR szNameA) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));
  CHttpHeaderArray *lpHeadersArray;
  CHttpHeaderBase *lpHeader;
  SIZE_T nIndex;

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return NULL;
  lpHeadersArray = &(cRequestParser.Headers());
  nIndex = lpHeadersArray->Find(szNameA);
  if (nIndex == (SIZE_T)-1)
    return NULL;
  lpHeader = lpHeadersArray->GetElementAt(nIndex);
  lpHeader->AddRef();
  return lpHeader;
}

SIZE_T CHttpServer::CClientRequest::GetRequestCookiesCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return 0;
  return cRequestParser.Cookies().GetCount();
}

CHttpCookie* CHttpServer::CClientRequest::GetRequestCookie(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));
  CHttpCookieArray *lpCookieArray;
  CHttpCookie *lpCookie;

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return NULL;
  lpCookieArray = &(cRequestParser.Cookies());
  if (nIndex >= lpCookieArray->GetCount())
    return NULL;
  lpCookie = lpCookieArray->GetElementAt(nIndex);
  lpCookie->AddRef();
  return lpCookie;
}

CHttpCookie* CHttpServer::CClientRequest::GetRequestCookieByName(_In_z_ LPCSTR szNameA) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));
  CHttpCookieArray *lpCookieArray;
  CHttpCookie *lpCookie;
  SIZE_T nIndex;

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return NULL;
  lpCookieArray = &(cRequestParser.Cookies());
  nIndex = lpCookieArray->Find(szNameA);
  if (nIndex == (SIZE_T)-1)
    return NULL;
  lpCookie = lpCookieArray->GetElementAt(nIndex);
  lpCookie->AddRef();
  return lpCookie;
}

CHttpCookie* CHttpServer::CClientRequest::GetRequestCookieByName(_In_z_ LPCWSTR szNameW) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));
  CHttpCookieArray *lpCookieArray;
  CHttpCookie *lpCookie;
  SIZE_T nIndex;

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return NULL;
  lpCookieArray = &(cRequestParser.Cookies());
  nIndex = lpCookieArray->Find(szNameW);
  if (nIndex == (SIZE_T)-1)
    return NULL;
  lpCookie = lpCookieArray->GetElementAt(nIndex);
  lpCookie->AddRef();
  return lpCookie;
}

HRESULT CHttpServer::CClientRequest::ResetResponse()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_InvalidState;
  if (HasHeadersBeenSent() != FALSE)
    return MX_E_InvalidState;
  ResetResponseForNewRequest(FALSE);
  //done
  return S_OK;
}

HRESULT CHttpServer::CClientRequest::SetResponseStatus(_In_ LONG nStatusCode, _In_opt_ LPCSTR szReasonA)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_InvalidState;
  if (HasHeadersBeenSent() != FALSE)
    return MX_E_InvalidState;
  if (nStatusCode < 101 || nStatusCode > 999)
    return E_INVALIDARG;
  if (szReasonA != NULL && szReasonA[0] != 0)
  {
    if (sResponse.cStrReasonA.Copy(szReasonA) == FALSE)
      return E_OUTOFMEMORY;
  }
  else
  {
    if (CHttpServer::GetStatusCodeMessage(nStatusCode) == NULL)
      return E_INVALIDARG;
    sResponse.cStrReasonA.Empty();
  }
  sResponse.nStatus = nStatusCode;
  //done
  return S_OK;
}

HRESULT CHttpServer::CClientRequest::AddResponseHeader(_In_ CHttpHeaderBase *lpHeader)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  HRESULT hRes;

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_InvalidState;
  if (HasHeadersBeenSent() != FALSE)
    return MX_E_InvalidState;
  if (lpHeader == NULL)
    return E_POINTER;

  //add to list
  hRes = sResponse.cHeaders.AddElement(lpHeader);
  if (FAILED(hRes))
    return hRes;
  lpHeader->AddRef();

  //done
  return S_OK;
}

HRESULT CHttpServer::CClientRequest::AddResponseHeader(_In_z_ LPCSTR szNameA, _Out_opt_ CHttpHeaderBase **lplpHeader,
                                                       _In_opt_ BOOL bReplaceExisting)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  TAutoRefCounted<CHttpHeaderBase> cNewHeader;
  HRESULT hRes;

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_InvalidState;
  if (HasHeadersBeenSent() != FALSE)
    return MX_E_InvalidState;

  //create and add to list
  hRes = CHttpHeaderBase::Create(szNameA, FALSE, &cNewHeader);
  if (SUCCEEDED(hRes))
    hRes = sResponse.cHeaders.AddElement(cNewHeader.Get());
  if (FAILED(hRes))
    return hRes;
  cNewHeader->AddRef();

  //done
  if (lplpHeader != NULL)
    *lplpHeader = cNewHeader.Detach();
  return S_OK;
}

HRESULT CHttpServer::CClientRequest::AddResponseHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA,
                                                       _In_opt_ SIZE_T nValueLen,
                                                       _Out_opt_ CHttpHeaderBase **lplpHeader,
                                                       _In_opt_ BOOL bReplaceExisting)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  TAutoRefCounted<CHttpHeaderBase> cNewHeader;
  HRESULT hRes;

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_InvalidState;
  if (HasHeadersBeenSent() != FALSE)
    return MX_E_InvalidState;

  //create and add to list
  hRes = CHttpHeaderBase::Create(szNameA, FALSE, &cNewHeader);
  if (FAILED(hRes))
    return hRes;

  hRes = cNewHeader->Parse(szValueA, nValueLen);
  if (FAILED(hRes))
    return hRes;

  if (bReplaceExisting != FALSE)
  {
    SIZE_T nIndex;

    while ((nIndex = sResponse.cHeaders.Find(szNameA)) != (SIZE_T)-1)
    {
      sResponse.cHeaders.RemoveElementAt(nIndex);
    }
  }

  hRes = sResponse.cHeaders.AddElement(cNewHeader.Get());
  if (FAILED(hRes))
    return hRes;
  cNewHeader->AddRef();

  //done
  if (lplpHeader != NULL)
    *lplpHeader = cNewHeader.Detach();
  return S_OK;
}

HRESULT CHttpServer::CClientRequest::AddResponseHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW,
                                                       _In_opt_ SIZE_T nValueLen,
                                                       _Out_opt_ CHttpHeaderBase **lplpHeader,
                                                       _In_opt_ BOOL bReplaceExisting)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  TAutoRefCounted<CHttpHeaderBase> cNewHeader;
  HRESULT hRes;

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_InvalidState;
  if (HasHeadersBeenSent() != FALSE)
    return MX_E_InvalidState;

  //create and add to list
  hRes = CHttpHeaderBase::Create(szNameA, FALSE, &cNewHeader);
  if (FAILED(hRes))
    return hRes;

  hRes = cNewHeader->Parse(szValueW, nValueLen);
  if (FAILED(hRes))
    return hRes;

  if (bReplaceExisting != FALSE)
  {
    SIZE_T nIndex;

    while ((nIndex = sResponse.cHeaders.Find(szNameA)) != (SIZE_T)-1)
    {
      sResponse.cHeaders.RemoveElementAt(nIndex);
    }
  }

  hRes = sResponse.cHeaders.AddElement(cNewHeader.Get());
  if (FAILED(hRes))
    return hRes;
  cNewHeader->AddRef();

  //done
  if (lplpHeader != NULL)
    *lplpHeader = cNewHeader.Detach();
  return S_OK;
}

HRESULT CHttpServer::CClientRequest::RemoveResponseHeader(_In_z_ LPCSTR szNameA)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  SIZE_T nIndex;
  HRESULT hRes = MX_E_NotFound;

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_InvalidState;
  if (HasHeadersBeenSent() != FALSE)
    return MX_E_InvalidState;

  while ((nIndex = sResponse.cHeaders.Find(szNameA)) != (SIZE_T)-1)
  {
    sResponse.cHeaders.RemoveElementAt(nIndex);
    hRes = S_OK;
  }
  return hRes;
}

HRESULT CHttpServer::CClientRequest::RemoveAllResponseHeaders()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_InvalidState;
  if (HasHeadersBeenSent() != FALSE)
    return MX_E_InvalidState;

  sResponse.cHeaders.RemoveAllElements();
  return S_OK;
}

SIZE_T CHttpServer::CClientRequest::GetResponseHeadersCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return 0;
  return sResponse.cHeaders.GetCount();
}

CHttpHeaderBase* CHttpServer::CClientRequest::GetResponseHeader(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));
  CHttpHeaderBase *lpHeader;

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return NULL;
  if (nIndex >= sResponse.cHeaders.GetCount())
    return NULL;
  lpHeader = sResponse.cHeaders.GetElementAt(nIndex);
  lpHeader->AddRef();
  return lpHeader;
}

CHttpHeaderBase* CHttpServer::CClientRequest::GetResponseHeaderByName(_In_z_ LPCSTR szNameA) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));
  CHttpHeaderBase *lpHeader;
  SIZE_T nIndex;

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return NULL;
  nIndex = sResponse.cHeaders.Find(szNameA);
  if (nIndex == (SIZE_T)-1)
    return NULL;
  lpHeader = sResponse.cHeaders.GetElementAt(nIndex);
  lpHeader->AddRef();
  return lpHeader;
}

HRESULT CHttpServer::CClientRequest::AddResponseCookie(_In_ CHttpCookie *lpCookie)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_InvalidState;
  if (HasHeadersBeenSent() != FALSE)
    return MX_E_InvalidState;
  if (lpCookie == NULL)
    return E_POINTER;

  if (sResponse.cCookies.AddElement(lpCookie) == FALSE)
    return E_OUTOFMEMORY;
  lpCookie->AddRef();
  return S_OK;
}

HRESULT CHttpServer::CClientRequest::AddResponseCookie(_In_ CHttpCookieArray &cSrc, _In_opt_ BOOL bReplaceExisting)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_InvalidState;
  if (HasHeadersBeenSent() != FALSE)
    return MX_E_InvalidState;

  return sResponse.cCookies.Merge(cSrc, bReplaceExisting);
}

HRESULT CHttpServer::CClientRequest::AddResponseCookie(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA,
                                                       _In_opt_z_ LPCSTR szDomainA, _In_opt_z_ LPCSTR szPathA,
                                                       _In_opt_ const CDateTime *lpDate, _In_opt_ BOOL bIsSecure,
                                                       _In_opt_ BOOL bIsHttpOnly)
{
  TAutoRefCounted<CHttpCookie> cCookie;
  HRESULT hRes;

  cCookie.Attach(MX_DEBUG_NEW CHttpCookie());
  if (!cCookie)
    return E_OUTOFMEMORY;
  hRes = cCookie->SetName(szNameA);
  if (SUCCEEDED(hRes))
  {
    hRes = cCookie->SetValue(szValueA);
    if (SUCCEEDED(hRes) && szDomainA != NULL)
      hRes = cCookie->SetDomain(szDomainA);
    if (SUCCEEDED(hRes) && szPathA != NULL)
      hRes = cCookie->SetPath(szPathA);
    if (SUCCEEDED(hRes))
    {
      if (lpDate != NULL)
        cCookie->SetExpireDate(lpDate);
      cCookie->SetSecureFlag(bIsSecure);
      cCookie->SetHttpOnlyFlag(bIsHttpOnly);

      hRes = AddResponseCookie(cCookie.Get());
    }
  }
  //done
  return hRes;
}

HRESULT CHttpServer::CClientRequest::AddResponseCookie(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szValueW,
                                                       _In_opt_z_ LPCWSTR szDomainW, _In_opt_z_ LPCWSTR szPathW,
                                                       _In_opt_ const CDateTime *lpDate, _In_opt_ BOOL bIsSecure,
                                                       _In_opt_ BOOL bIsHttpOnly)
{
  TAutoRefCounted<CHttpCookie> cCookie;
  HRESULT hRes;

  cCookie.Attach(MX_DEBUG_NEW CHttpCookie());
  if (!cCookie)
    return E_OUTOFMEMORY;
  hRes = cCookie->SetName(szNameW);
  if (SUCCEEDED(hRes))
  {
    hRes = cCookie->SetValue(szValueW);
    if (SUCCEEDED(hRes) && szDomainW != NULL)
      hRes = cCookie->SetDomain(szDomainW);
    if (SUCCEEDED(hRes) && szPathW != NULL)
      hRes = cCookie->SetPath(szPathW);
    if (SUCCEEDED(hRes))
    {
      if (lpDate != NULL)
        cCookie->SetExpireDate(lpDate);
      cCookie->SetSecureFlag(bIsSecure);
      cCookie->SetHttpOnlyFlag(bIsHttpOnly);

      hRes = AddResponseCookie(cCookie.Get());
    }
  }
  return hRes;
}

HRESULT CHttpServer::CClientRequest::RemoveResponseCookie(_In_z_ LPCSTR szNameA)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  SIZE_T nIndex;
  HRESULT hRes = MX_E_NotFound;

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_InvalidState;
  if (HasHeadersBeenSent() != FALSE)
    return MX_E_InvalidState;

  while ((nIndex = sResponse.cCookies.Find(szNameA)) != (SIZE_T)-1)
  {
    sResponse.cCookies.RemoveElementAt(nIndex);
    hRes = S_OK;
  }
  return hRes;
}

HRESULT CHttpServer::CClientRequest::RemoveResponseCookie(_In_z_ LPCWSTR szNameW)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  SIZE_T nIndex;
  HRESULT hRes = MX_E_NotFound;

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_InvalidState;
  if (HasHeadersBeenSent() != FALSE)
    return MX_E_InvalidState;

  while ((nIndex = sResponse.cCookies.Find(szNameW)) != (SIZE_T)-1)
  {
    sResponse.cCookies.RemoveElementAt(nIndex);
    hRes = S_OK;
  }
  return hRes;
}

HRESULT CHttpServer::CClientRequest::RemoveAllResponseCookies()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_InvalidState;
  if (HasHeadersBeenSent() != FALSE)
    return MX_E_InvalidState;
  sResponse.cCookies.RemoveAllElements();
  return S_OK;
}

SIZE_T CHttpServer::CClientRequest::GetResponseCookiesCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return 0;
  return sResponse.cCookies.GetCount();
}

CHttpCookie* CHttpServer::CClientRequest::GetResponseCookie(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));
  CHttpCookie *lpCookie;

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return NULL;
  if (nIndex >= sResponse.cCookies.GetCount())
    return NULL;
  lpCookie = sResponse.cCookies.GetElementAt(nIndex);
  lpCookie->AddRef();
  return lpCookie;
}

CHttpCookie* CHttpServer::CClientRequest::GetResponseCookieByName(_In_z_ LPCSTR szNameA) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));
  SIZE_T nIndex;
  CHttpCookie *lpCookie;

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return NULL;
  nIndex = sResponse.cCookies.Find(szNameA);
  if (nIndex == (SIZE_T)-1)
    return NULL;
  lpCookie = sResponse.cCookies.GetElementAt(nIndex);
  lpCookie->AddRef();
  return lpCookie;
}

CHttpCookie* CHttpServer::CClientRequest::GetResponseCookieByName(_In_z_ LPCWSTR szNameW) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));
  SIZE_T nIndex;
  CHttpCookie *lpCookie;

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return NULL;
  nIndex = sResponse.cCookies.Find(szNameW);
  if (nIndex == (SIZE_T)-1)
    return NULL;
  lpCookie = sResponse.cCookies.GetElementAt(nIndex);
  lpCookie->AddRef();
  return lpCookie;
}

HRESULT CHttpServer::CClientRequest::SendResponse(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  SIZE_T nWritten;
  HRESULT hRes;

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateSendingResponse &&
      nState != StateNegotiatingWebSocket)
  {
    return MX_E_InvalidState;
  }
  if (nDataLen == 0)
    return S_OK;
  if (lpData == NULL)
    return E_POINTER;
  //send a direct response?
  if (sResponse.bDirect != FALSE)
  {
    if (nState != StateSendingResponse)
      return MX_E_InvalidState;

    hRes = SendHeaders(); //send headers if we didn't before
    if (SUCCEEDED(hRes))
      hRes = lpHttpServer->cSocketMgr.SendMsg(hConn, lpData, nDataLen);
    return hRes;
  }
  if (nState == StateSendingResponse)
    return MX_E_InvalidState;

  if (sResponse.aStreamsList.GetCount() > 0 && sResponse.bLastStreamIsData != FALSE)
  {
    CStream *lpStream = sResponse.aStreamsList.GetElementAt(sResponse.aStreamsList.GetCount() - 1);

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

  //done
  sResponse.szMimeTypeHintA = NULL;
  sResponse.cStrFileNameW.Empty();
  sResponse.bIsInline = FALSE;
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
    hRes = SendStream(cStream);
  //done
  return hRes;
}

HRESULT CHttpServer::CClientRequest::SendStream(_In_ CStream *lpStream)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateSendingResponse &&
      nState != StateNegotiatingWebSocket)
  {
    return MX_E_InvalidState;
  }
  if (lpStream == NULL)
    return E_POINTER;

  //sending a direct response?
  if (sResponse.bDirect != FALSE)
  {
    HRESULT hRes;

    if (nState != StateSendingResponse)
      return MX_E_InvalidState;

    hRes = SendHeaders(); //send headers if we didn't before
    if (SUCCEEDED(hRes))
      hRes = lpHttpServer->cSocketMgr.SendStream(hConn, lpStream);
    return hRes;
  }

  if (nState == StateSendingResponse)
    return MX_E_InvalidState;

  //add to list
  if (sResponse.aStreamsList.AddElement(lpStream) == FALSE)
    return E_OUTOFMEMORY;
  lpStream->AddRef();

  //done
  sResponse.bLastStreamIsData = FALSE;
  return S_OK;
}

HRESULT CHttpServer::CClientRequest::SetMimeTypeFromFileName(_In_opt_z_ LPCWSTR szFileNameW)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_InvalidState;

  //sending a direct response?
  if (sResponse.bDirect != FALSE)
    return MX_E_InvalidState;

  //set response info
  sResponse.szMimeTypeHintA = (szFileNameW != FALSE) ? Http::GetMimeType(szFileNameW) : NULL;

  //done
  return S_OK;
}

HRESULT CHttpServer::CClientRequest::SetFileName(_In_opt_z_ LPCWSTR szFileNameW, _In_opt_ BOOL bInline)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  CStringW cStrTempFileNameW;

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_InvalidState;

  //sending a direct response?
  if (sResponse.bDirect != FALSE)
    return MX_E_InvalidState;

  //extract name
  if (szFileNameW != NULL && *szFileNameW != 0)
  {
    LPCWSTR sW[2];

    sW[0] = StrChrW(szFileNameW, L'\\', TRUE);
    sW[0] = (sW[0] != NULL) ? (sW[0] + 1) : szFileNameW;
    sW[1] = StrChrW(szFileNameW, L'/', TRUE);
    sW[1] = (sW[1] != NULL) ? (sW[1] + 1) : szFileNameW;

    if (cStrTempFileNameW.Copy((sW[0] > sW[1]) ? sW[0] : sW[1]) == FALSE)
      return E_OUTOFMEMORY;
  }

  //set response info
  if (cStrTempFileNameW.IsEmpty() == FALSE)
  {
    sResponse.szMimeTypeHintA = Http::GetMimeType(szFileNameW);
    sResponse.cStrFileNameW.Attach(cStrTempFileNameW.Detach());
    sResponse.bIsInline = bInline;
  }
  else
  {
    sResponse.szMimeTypeHintA = NULL;
    sResponse.cStrFileNameW.Empty();
    sResponse.bIsInline = FALSE;
  }

  //done
  return S_OK;
}

HRESULT CHttpServer::CClientRequest::SendErrorPage(_In_ LONG nStatusCode, _In_ HRESULT hrErrorCode,
                                                   _In_opt_z_ LPCSTR szAdditionalExplanationA)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  HRESULT hRes;

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_InvalidState;
  if (HasHeadersBeenSent() != FALSE)
    return MX_E_InvalidState;
  _InterlockedOr(&nFlags, REQUEST_FLAG_DontKeepAlive);
  hRes = lpHttpServer->FillResponseWithError(this, nStatusCode, hrErrorCode, szAdditionalExplanationA);

  //done
  return hRes;
}

HRESULT CHttpServer::CClientRequest::DisableClientCache()
{
  CCriticalSection::CAutoLock cLock(cMutex);
  HRESULT hRes;

  if (nState != StateAfterHeaders && nState != StateBuildingResponse && nState != StateNegotiatingWebSocket)
    return MX_E_InvalidState;
  if (HasHeadersBeenSent() != FALSE)
    return MX_E_InvalidState;

  RemoveResponseHeader("Cache-Control");
  RemoveResponseHeader("Pragma");
  RemoveResponseHeader("Expires");

  hRes = AddResponseHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  if (SUCCEEDED(hRes))
    hRes = AddResponseHeader("Pragma", "no-cache");
  if (SUCCEEDED(hRes))
    hRes = AddResponseHeader("Expires", "0");
  //done
  return hRes;
}

VOID CHttpServer::CClientRequest::IgnoreKeepAlive()
{
  _InterlockedOr(&nFlags, REQUEST_FLAG_DontKeepAlive);
  return;
}

HANDLE CHttpServer::CClientRequest::GetUnderlyingSocketHandle() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  return hConn;
}

CSockets* CHttpServer::CClientRequest::GetUnderlyingSocketManager() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  return lpSocketMgr;
}

HRESULT CHttpServer::CClientRequest::Initialize(_In_ CHttpServer *_lpHttpServer, _In_ CSockets *_lpSocketMgr,
                                                _In_ HANDLE _hConn, _In_ DWORD dwMaxHeaderSize)
{
  MX_ASSERT(_lpHttpServer != NULL);
  MX_ASSERT(_lpSocketMgr != NULL);
  MX_ASSERT(_hConn != NULL);
  lpHttpServer = _lpHttpServer;
  cRequestParser.SetLogParent(_lpHttpServer);
  lpSocketMgr = _lpSocketMgr;
  hConn = _hConn;
  if (_lpHttpServer->ShouldLog(1) != FALSE)
  {
    _lpHttpServer->Log(L"HttpServer(State/Req:0x%p/Conn:0x%p/Parser:0x%p): State=Inactive", this, hConn,
                       &cRequestParser);
  }
  cRequestParser.SetOption_MaxHeaderSize(dwMaxHeaderSize);
  //done
  return S_OK;
}

HRESULT CHttpServer::CClientRequest::ResetForNewRequest()
{
  HRESULT hRes;

  hRes = SetState(StateKeepingAlive);
  if (FAILED(hRes))
    return hRes;

  _InterlockedExchange(&nFlags, 0);
  cRequestParser.Reset();
  ResetResponseForNewRequest(FALSE);

  //done
  return S_OK;
}

HRESULT CHttpServer::CClientRequest::SetState(_In_ eState nNewState)
{
  HRESULT hRes;

  MX_ASSERT(nNewState == StateTerminated || nNewState != nState);
  MX_ASSERT(nNewState == StateTerminated || hConn != NULL);
  if (lpHttpServer->ShouldLog(1) != FALSE)
  {
    BOOL b = FALSE;

    switch (nState)
    {
      case StateInactive:
      case StateKeepingAlive:
        if (nNewState != StateReceivingRequestHeaders && nNewState != StateTerminated)
          b = TRUE;
        break;

      case StateReceivingRequestHeaders:
        if (nNewState != StateNegotiatingWebSocket && nNewState != StateAfterHeaders &&
            nNewState != StateBuildingResponse)
        {
          b = TRUE;
        }
        break;

      case StateAfterHeaders:
        if (nNewState != StateReceivingRequestBody && nNewState != StateBuildingResponse)
          b = TRUE;
        break;

      case StateReceivingRequestBody:
        if (nNewState != StateBuildingResponse)
          b = TRUE;
        break;

      case StateBuildingResponse:
        if (nNewState != StateSendingResponse)
          b = TRUE;
        break;

      case StateSendingResponse:
        if (nNewState != StateKeepingAlive && nNewState != StateTerminated && nNewState != StateLingerClose)
          b = TRUE;
        break;

      case StateNegotiatingWebSocket:
        if (nNewState != StateWebSocket)
          b = TRUE;
        break;
    }
    if (b != FALSE)
    {
      lpHttpServer->Log(L"HttpServer(State/Req:0x%p/Conn:0x%p): Unexpected NewState=%s from State=%s", this, hConn,
                        GetNamedState(nNewState), GetNamedState(nState));
    }
  }

  switch (nState)
  {
    case StateBuildingResponse:
    case StateAfterHeaders:
    case StateNegotiatingWebSocket:
      if (nNewState != nState && nNewState != StateTerminated)
      {
        hRes = lpHttpServer->cSocketMgr.ResumeInputProcessing(hConn);
        if (FAILED(hRes))
          return hRes;
      }
      break;
  }
  switch (nNewState)
  {
    case StateBuildingResponse:
    case StateAfterHeaders:
    case StateNegotiatingWebSocket:
      if (nState != nNewState)
      {
        hRes = lpHttpServer->cSocketMgr.PauseInputProcessing(hConn);
        if (FAILED(hRes))
          return hRes;
      }
      break;
  }
  nState = nNewState;

  if (lpHttpServer->ShouldLog(1) != FALSE)
  {
    lpHttpServer->Log(L"HttpServer(State/Req:0x%p/Conn:0x%p): State=%s", this, hConn, GetNamedState(nState));
  }

  //done
  return S_OK;
}

BOOL CHttpServer::CClientRequest::IsKeepAliveRequest() const
{
  int nHttpVersion[2];

  nHttpVersion[0] = cRequestParser.GetRequestVersionMajor();
  nHttpVersion[1] = cRequestParser.GetRequestVersionMinor();
  return (nHttpVersion[0] > 0) ? cRequestParser.IsKeepAliveRequest() : FALSE;
}

BOOL CHttpServer::CClientRequest::HasErrorBeenSent() const
{
  return ((__InterlockedRead(const_cast<LONG volatile*>(&nFlags)) & REQUEST_FLAG_ErrorPageSent) != 0) ? TRUE : FALSE;
}

BOOL CHttpServer::CClientRequest::HasHeadersBeenSent() const
{
  return ((__InterlockedRead(const_cast<LONG volatile*>(&nFlags)) & REQUEST_FLAG_HeadersSent) != 0) ? TRUE : FALSE;
}

HRESULT CHttpServer::CClientRequest::SendHeaders()
{
  TAutoRefCounted<CMemoryStream> cHdrStream;
  CStringA cStrTempA;
  int nHttpVersion[2];
  LONG nStatus = 0;
  LPCSTR sA;
  HRESULT hRes;

  if ((_InterlockedOr(&nFlags, REQUEST_FLAG_HeadersSent) & REQUEST_FLAG_HeadersSent) != 0)
  {
    return S_FALSE;
  }

  cHdrStream.Attach(MX_DEBUG_NEW CMemoryStream(65536));
  hRes = (cHdrStream) ? cHdrStream->Create() : E_OUTOFMEMORY;
  //status
  if (SUCCEEDED(hRes))
  {
    if ((nStatus = sResponse.nStatus) == 0)
    {
      nStatus = (sResponse.cHeaders.Find<CHttpHeaderRespLocation>() != NULL) ? 302 : 200;
      sA = CHttpServer::GetStatusCodeMessage(nStatus);
    }
    else
    {
      sA = (sResponse.cStrReasonA.IsEmpty() != FALSE) ? CHttpServer::GetStatusCodeMessage(nStatus) :
                                                       (LPCSTR)(sResponse.cStrReasonA);
    }
    if (sA != NULL)
    {
      if (*sA == '*')
        sA++;
      if (*sA == 0)
        sA = NULL;
    }

    nHttpVersion[0] = cRequestParser.GetRequestVersionMajor();
    nHttpVersion[1] = cRequestParser.GetRequestVersionMinor();
    if (nHttpVersion[0] == 0)
    {
      nHttpVersion[0] = 1;
      nHttpVersion[1] = 0;
    }
    hRes = cHdrStream->WriteString("HTTP/%d.%d %03d %s\r\n", nHttpVersion[0], nHttpVersion[1], nStatus, 
                                   ((sA != NULL) ? sA : "Undefined"));
  }

  //date
  if (SUCCEEDED(hRes))
  {
    CHttpHeaderGenDate *lpHeader;

    lpHeader = sResponse.cHeaders.Find<CHttpHeaderGenDate>();
    if (lpHeader != NULL)
    {
      hRes = lpHeader->Build(cStrTempA, cRequestParser.GetRequestBrowser());
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
    CHttpHeaderRespLocation *lpHeader;

    lpHeader = sResponse.cHeaders.Find<CHttpHeaderRespLocation>();
    if (lpHeader != NULL)
    {
      hRes = lpHeader->Build(cStrTempA, cRequestParser.GetRequestBrowser());
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
      SIZE_T nIndex;

      nIndex = sResponse.cHeaders.Find("Server");
      if (nIndex != (SIZE_T)-1)
      {
        CHttpHeaderBase *lpHeader = sResponse.cHeaders.GetElementAt(nIndex);

        hRes = lpHeader->Build(cStrTempA, cRequestParser.GetRequestBrowser());
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
    CHttpHeaderGenConnection *lpHeader;

    lpHeader = sResponse.cHeaders.Find<CHttpHeaderGenConnection>();
    if (lpHeader != NULL)
    {
      hRes = lpHeader->Build(cStrTempA, cRequestParser.GetRequestBrowser());
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
    else
    {
      hRes = WriteToStream(cHdrStream, "Connection: Close\r\n", 19);
    }
  }

  //content type
  if (SUCCEEDED(hRes) && sResponse.aStreamsList.GetCount() > 0)
  {
    CHttpHeaderEntContentType *lpHeader;

    lpHeader = sResponse.cHeaders.Find<CHttpHeaderEntContentType>();
    if (lpHeader != NULL)
    {
      hRes = lpHeader->Build(cStrTempA, cRequestParser.GetRequestBrowser());
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
    else
    {
      //only send automatic "Content-Type" header if not direct response and has a body
      if (sResponse.bDirect == FALSE)
      {
        if (sResponse.szMimeTypeHintA != NULL)
        {
          hRes = WriteToStream(cHdrStream, "Content-Type: ", 14);
          if (SUCCEEDED(hRes))
          {
            hRes = WriteToStream(cHdrStream, sResponse.szMimeTypeHintA);
            if (SUCCEEDED(hRes))
            {
              if (StrNCompareA(sResponse.szMimeTypeHintA, "text/", 5) == 0)
                hRes = WriteToStream(cHdrStream, "; charset=utf-8\r\n", 17);
              else
                hRes = WriteToStream(cHdrStream, "\r\n", 2);
            }
          }
        }
        else if (sResponse.bLastStreamIsData != FALSE)
        {
          hRes = WriteToStream(cHdrStream, "Content-Type: text/html; charset=utf-8\r\n", 40);
        }
        else
        {
          hRes = WriteToStream(cHdrStream, "Content-Type: application/octet-stream\r\n", 40);
        }
      }
    }
  }

  //content disposition
  if (SUCCEEDED(hRes) && sResponse.aStreamsList.GetCount() > 0 && sResponse.cStrFileNameW.IsEmpty() == FALSE &&
      sResponse.cHeaders.Find<CHttpHeaderEntContentDisposition>() == NULL)
  {
    TAutoRefCounted<CHttpHeaderEntContentDisposition> cHeader;

    cHeader.Attach(MX_DEBUG_NEW CHttpHeaderEntContentDisposition());
    if (cHeader)
    {
      hRes = (sResponse.bIsInline == FALSE) ? cHeader->SetType("attachment", 10)
                                            : cHeader->SetType("inline", 6);
      if (SUCCEEDED(hRes))
      {
        hRes = cHeader->SetFileName((LPCWSTR)(sResponse.cStrFileNameW), sResponse.cStrFileNameW.GetLength());
        if (SUCCEEDED(hRes))
        {
          hRes = cHeader->Build(cStrTempA, cRequestParser.GetRequestBrowser());
          if (SUCCEEDED(hRes))
          {
            hRes = WriteToStream(cHdrStream, "Content-Disposition: ", 21);
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
    else
    {
      hRes = E_OUTOFMEMORY;
    }
  }

  //content length
  if (SUCCEEDED(hRes))
  {
    CHttpHeaderEntContentLength *lpHeaderEntContentLength;
    CHttpHeaderGenTransferEncoding *lpHeaderGenTransferEncoding;
    ULONGLONG nTotalLength, nLen;
    SIZE_T i, nCount;

    lpHeaderGenTransferEncoding = sResponse.cHeaders.Find<CHttpHeaderGenTransferEncoding>();
    if (lpHeaderGenTransferEncoding == NULL ||
        lpHeaderGenTransferEncoding->GetEncoding() != CHttpHeaderGenTransferEncoding::EncodingChunked)
    {
      lpHeaderEntContentLength = sResponse.cHeaders.Find<CHttpHeaderEntContentLength>();
      if (lpHeaderEntContentLength == NULL)
      {
        //only send automatic "Content-Length" header if not direct response and no switching protocol
        if (sResponse.bDirect == FALSE)
        {
          if (nStatus >= 200 && nStatus != 204 && nStatus != 304)
          {
            nTotalLength = 0ui64;
            nCount = sResponse.aStreamsList.GetCount();
            for (i = 0; i < nCount; i++)
            {
              nLen = sResponse.aStreamsList[i]->GetLength();
              if (nLen + nTotalLength < nTotalLength)
              {
                hRes = MX_E_BadLength;
                break;
              }
              nTotalLength += nLen;
            }
            if (SUCCEEDED(hRes))
            {
              hRes = cHdrStream->WriteString("Content-Length: %I64u\r\n", nTotalLength);
            }
          }
          else if (sResponse.aStreamsList.GetCount() > 0)
          {
            hRes = MX_HRESULT_FROM_WIN32(ERROR_DATA_NOT_ACCEPTED);
          }
        }
      }
      else
      {
        if (nStatus >= 200 && nStatus != 204 && nStatus != 304)
        {
          hRes = lpHeaderEntContentLength->Build(cStrTempA, cRequestParser.GetRequestBrowser());
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
        else
        {
          hRes = MX_HRESULT_FROM_WIN32(ERROR_DATA_NOT_ACCEPTED);
        }
      }
    }
  }

  //rest of headers
  if (SUCCEEDED(hRes))
  {
    CHttpHeaderBase *lpHeader;
    SIZE_T i, nCount;

    nCount = sResponse.cHeaders.GetCount();
    for (i = 0; SUCCEEDED(hRes) && i < nCount; i++)
    {
      lpHeader = sResponse.cHeaders.GetElementAt(i);

      if (StrCompareA(lpHeader->GetHeaderName(), "Content-Type") != 0 &&
          StrCompareA(lpHeader->GetHeaderName(), "Content-Length") != 0 &&
          StrCompareA(lpHeader->GetHeaderName(), "Date") != 0 &&
          StrCompareA(lpHeader->GetHeaderName(), "Server") != 0 &&
          StrCompareA(lpHeader->GetHeaderName(), "Connection") != 0 &&
          StrCompareA(lpHeader->GetHeaderName(), "Location") != 0)
      {
        hRes = lpHeader->Build(cStrTempA, cRequestParser.GetRequestBrowser());
        if (SUCCEEDED(hRes))
        {
          hRes = WriteToStream(cHdrStream, lpHeader->GetHeaderName());
          if (SUCCEEDED(hRes))
          {
            hRes = WriteToStream(cHdrStream, ": ", 2);
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
  }

  //cookies
  if (SUCCEEDED(hRes))
  {
    CHttpCookie *lpCookie;
    SIZE_T i, nCount;

    nCount = sResponse.cCookies.GetCount();
    for (i = 0; SUCCEEDED(hRes) && i < nCount; i++)
    {
      lpCookie = sResponse.cCookies.GetElementAt(i);

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
  {
    hRes = WriteToStream(cHdrStream, "\r\n", 2);
  }

  //send stream
  if (SUCCEEDED(hRes))
  {
    hRes = lpHttpServer->cSocketMgr.SendStream(hConn, cHdrStream.Get());
  }

  if (FAILED(hRes))
  {
    _InterlockedAnd(&nFlags, ~REQUEST_FLAG_HeadersSent);
  }
  if (lpHttpServer->ShouldLog(1) != FALSE)
  {
    lpHttpServer->Log(L"HttpServer(ResponseHeaders/Req:0x%p/Conn:0x%p): 0x%08X", this, hConn, hRes);
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
  nCount = sResponse.aStreamsList.GetCount();
  for (i = 0; SUCCEEDED(hRes) && i < nCount; i++)
  {
    lpStream = sResponse.aStreamsList.GetElementAt(i);

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

VOID CHttpServer::CClientRequest::ResetResponseForNewRequest(_In_ BOOL bPreserveWebSocket)
{
  if (bPreserveWebSocket == FALSE)
  {
    sResponse.nStatus = 0;
  }
  sResponse.cStrReasonA.Empty();
  if (bPreserveWebSocket == FALSE)
  {
    sResponse.cHeaders.RemoveAllElements();
  }
  else
  {
    SIZE_T nCount;

    nCount = sResponse.cHeaders.GetCount();
    while (nCount > 0)
    {
      CHttpHeaderBase *lpHeader = sResponse.cHeaders.GetElementAt(--nCount);

      if (StrCompareA(lpHeader->GetHeaderName(), "Sec-WebSocket-Version") != 0 &&
          StrCompareA(lpHeader->GetHeaderName(), "Sec-WebSocket-Protocol") != 0)
      {
        sResponse.cHeaders.RemoveElementAt(nCount);
      }
    }
  }
  sResponse.cCookies.RemoveAllElements();
  sResponse.aStreamsList.RemoveAllElements();
  sResponse.bLastStreamIsData = FALSE;
  sResponse.szMimeTypeHintA = NULL;
  sResponse.cStrFileNameW.Empty();
  sResponse.bDirect = sResponse.bPreserveWebSocketHeaders = FALSE;
  return;
}

HRESULT CHttpServer::CClientRequest::StartTimeoutTimers(_In_ int nTimers)
{
  HRESULT hRes;

  if ((nTimers & TimeoutTimerHeaders) != 0)
  {
    MX::TimedEvent::Clear(&nHeadersTimeoutTimerId);
    hRes = MX::TimedEvent::SetTimeout(&nHeadersTimeoutTimerId, lpHttpServer->dwRequestHeaderTimeoutMs,
                                      MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnHeadersTimeoutTimerCallback,
                                                              lpHttpServer),
                                      this);
    if (FAILED(hRes))
      goto done;
  }
  if ((nTimers & TimeoutTimerThroughput) != 0)
  {
    MX::TimedEvent::Clear(&nThroughputTimerId);
    hRes = MX::TimedEvent::SetInterval(&nThroughputTimerId, 1000,
                                       MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnThroughputTimerCallback, lpHttpServer),
                                       this);
    if (FAILED(hRes))
      goto done;
  }
  if ((nTimers & TimeoutTimerGracefulTermination) != 0)
  {
    MX::TimedEvent::Clear(&nGracefulTerminationTimerId);
    hRes = MX::TimedEvent::SetTimeout(&nGracefulTerminationTimerId, lpHttpServer->dwGracefulTerminationTimeoutMs,
                                      MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnGracefulTerminationTimerCallback,
                                                              lpHttpServer),
                                      this);
    if (FAILED(hRes))
      goto done;
  }
  if ((nTimers & TimeoutTimerKeepAlive) != 0)
  {
    MX::TimedEvent::Clear(&nKeepAliveTimeoutTimerId);
    hRes = MX::TimedEvent::SetInterval(&nKeepAliveTimeoutTimerId, lpHttpServer->dwKeepAliveTimeoutMs,
                                       MX_BIND_MEMBER_CALLBACK(&CHttpServer::OnKeepAliveTimerCallback, lpHttpServer),
                                       this);
    if (FAILED(hRes))
      goto done;
  }
  hRes = S_OK;

done:
  if (FAILED(hRes))
    StopTimeoutTimers(nTimers);
  return hRes;
}

VOID CHttpServer::CClientRequest::StopTimeoutTimers(_In_ int nTimers)
{
  if ((nTimers & TimeoutTimerHeaders) != 0)
  {
    MX::TimedEvent::Clear(&nHeadersTimeoutTimerId);
  }
  if ((nTimers & TimeoutTimerThroughput) != 0)
  {
    MX::TimedEvent::Clear(&nThroughputTimerId);
  }
  if ((nTimers & TimeoutTimerGracefulTermination) != 0)
  {
    MX::TimedEvent::Clear(&nGracefulTerminationTimerId);
  }
  if ((nTimers & TimeoutTimerKeepAlive) != 0)
  {
    MX::TimedEvent::Clear(&nKeepAliveTimeoutTimerId);
  }
  return;
}

LPCWSTR CHttpServer::CClientRequest::GetNamedState(_In_ eState nState)
{
  switch (nState)
  {
    case StateInactive:
      return L"Inactive";

    case StateReceivingRequestHeaders:
      return L"Receiving headers";

    case StateAfterHeaders:
      return L"Received headers";

    case StateReceivingRequestBody:
      return L"Receiving body";

    case StateBuildingResponse:
      return L"Building response";

    case StateSendingResponse:
      return L"Sending response";

    case StateNegotiatingWebSocket:
      return L"Negotiating WebSocket";

    case StateWebSocket:
      return L"WebSocket";

    case StateGracefulTermination:
      return L"Graceful Termination";

    case StateTerminated:
      return L"Terminated";

    case StateKeepingAlive:
      return L"Keeping Alive";

    case StateLingerClose:
      return L"Linger Close";
  }
  return L"Closed";
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
