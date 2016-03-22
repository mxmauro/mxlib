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
#include "..\..\Include\Http\HttpClient.h"
#include "..\..\Include\Strings\Utf8.h"
#include "..\..\Include\FnvHash.h"
#include "..\..\Include\DateTime\DateTime.h"
#include "..\..\Include\Comm\IpcSslLayer.h"
#include "..\..\Include\Http\Url.h"

//-----------------------------------------------------------

#define MAX_FORM_SIZE_4_REQUEST                 (128 * 1024)

//-----------------------------------------------------------

namespace MX {

CHttpClient::CHttpClient(__in CSockets &_cSocketMgr, __in CPropertyBag &_cPropBag) :
             CBaseMemObj(), cSocketMgr(_cSocketMgr), cPropBag(_cPropBag), sRequest(_cPropBag), sResponse(_cPropBag)
{
  dwResponseTimeoutMs = MX_HTTP_CLIENT_ResponseTimeoutMs_DEFVAL;
  dwMaxRedirCount = MX_HTTP_CLIENT_MaxRedirectionsCount_DEFVAL;
  //----
  RundownProt_Initialize(&nRundownLock);
  nState = StateClosed;
  nOptionFlags = (int)(OptionKeepConnectionOpen | OptionAcceptCompressedContent);
  hConn = NULL;
  hLastErrorCode = S_OK;
  sRedirect.dwRedirectCounter = 0;
  lpTimedEventQueue = NULL;
  cHeadersReceivedCallback = NullCallback();
  cDocumentCompletedCallback = NullCallback();
  cErrorCallback = NullCallback();
  return;
}

CHttpClient::~CHttpClient()
{
  LONG volatile nWait = 0;

  RundownProt_WaitForRelease(&nRundownLock);
  //close current connection
  {
    CCriticalSection::CAutoLock cLock(cMutex);

    if (hConn != NULL)
    {
      cSocketMgr.Close(hConn, MX_E_Cancelled);
      hConn = NULL;
    }
    if (lpTimedEventQueue != NULL)
    {
      if (sRedirect.lpEvent != NULL)
      {
        lpTimedEventQueue->Remove(sRedirect.lpEvent);
        sRedirect.lpEvent = NULL;
      }
      if (sResponse.lpTimeoutEvent != NULL)
      {
        lpTimedEventQueue->Remove(sResponse.lpTimeoutEvent);
        sResponse.lpTimeoutEvent = NULL;
      }
    }
  }
  //wait until pending events are processed
  cPendingEvents.WaitAll();
  cPendingHandles.WaitAll();
  //release queue
  if (lpTimedEventQueue != NULL)
    lpTimedEventQueue->Release();
  return;
}

VOID CHttpClient::On(__in OnHeadersReceivedCallback _cHeadersReceivedCallback)
{
  cHeadersReceivedCallback = _cHeadersReceivedCallback;
  return;
}

VOID CHttpClient::On(__in OnDocumentCompletedCallback _cDocumentCompletedCallback)
{
  cDocumentCompletedCallback = _cDocumentCompletedCallback;
  return;
}

VOID CHttpClient::On(__in OnErrorCallback _cErrorCallback)
{
  cErrorCallback = _cErrorCallback;
  return;
}

VOID CHttpClient::On(__in OnQueryCertificatesCallback _cQueryCertificatesCallback)
{
  cQueryCertificatesCallback = _cQueryCertificatesCallback;
  return;
}

HRESULT CHttpClient::SetRequestMethodAuto()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  sRequest.cStrMethodA.Empty();
  return S_OK;
}

HRESULT CHttpClient::SetRequestMethod(__in_z LPCSTR szMethodA)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  if (szMethodA == NULL || *szMethodA == 0)
    return SetRequestMethodAuto();
  if (CHttpCommon::IsValidVerb(szMethodA) == FALSE)
    return E_INVALIDARG;
  return (sRequest.cStrMethodA.Copy(szMethodA) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

HRESULT CHttpClient::SetRequestMethod(__in_z LPCWSTR szMethodW)
{
  CStringA cStrTempA;

  if (szMethodW == NULL || *szMethodW == 0)
    return SetRequestMethodAuto();
  if (cStrTempA.Copy(szMethodW) == FALSE)
    return E_OUTOFMEMORY;
  return SetRequestMethod((LPCSTR)cStrTempA);
}

HRESULT CHttpClient::AddRequestHeader(__in_z LPCSTR szNameA, __out CHttpHeaderBase **lplpHeader)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (lplpHeader == NULL)
    return E_POINTER;
  *lplpHeader = NULL;
  if (nState != StateClosed && nState != StateDocumentCompleted)
    return MX_E_NotReady;
  return sRequest.cHttpCmn.AddHeader(szNameA, lplpHeader);
}

HRESULT CHttpClient::AddRequestHeader(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA,
                                      __out_opt CHttpHeaderBase **lplpHeader)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateClosed && nState != StateDocumentCompleted)
    return MX_E_NotReady;
  return sRequest.cHttpCmn.AddHeader(szNameA, szValueA, (SIZE_T)-1, lplpHeader);
}

HRESULT CHttpClient::AddRequestHeader(__in_z LPCSTR szNameA, __in_z LPCWSTR szValueW,
                                      __out_opt CHttpHeaderBase **lplpHeader)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateClosed && nState != StateDocumentCompleted)
    return MX_E_NotReady;
  return sRequest.cHttpCmn.AddHeader(szNameA, szValueW, (SIZE_T)-1, lplpHeader);
}

HRESULT CHttpClient::RemoveRequestHeader(__in_z LPCSTR szNameA)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed && nState != StateDocumentCompleted)
    return MX_E_NotReady;
  sRequest.cHttpCmn.RemoveHeader(szNameA);
  return S_OK;
}

HRESULT CHttpClient::RemoveAllRequestHeaders()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed && nState != StateDocumentCompleted)
    return MX_E_NotReady;
  sRequest.cHttpCmn.RemoveAllHeaders();
  return S_OK;
}

HRESULT CHttpClient::AddRequestCookie(__in CHttpCookie &cSrc)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  return sRequest.cHttpCmn.AddCookie(cSrc);
}

HRESULT CHttpClient::AddRequestCookie(__in CHttpCookieArray &cSrc)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  return sRequest.cHttpCmn.AddCookie(cSrc);
}

HRESULT CHttpClient::AddRequestCookie(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  return sRequest.cHttpCmn.AddCookie(szNameA, szValueA);
}

HRESULT CHttpClient::AddRequestCookie(__in_z LPCWSTR szNameW, __in_z LPCWSTR szValueW)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  return sRequest.cHttpCmn.AddCookie(szNameW, szValueW);
}

HRESULT CHttpClient::RemoveRequestCookie(__in_z LPCSTR szNameA)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  return sRequest.cHttpCmn.RemoveCookie(szNameA);
}

HRESULT CHttpClient::RemoveRequestCookie(__in_z LPCWSTR szNameW)
{
  CStringA cStrTempA;

  if (szNameW == NULL)
    return E_POINTER;
  if (*szNameW == 0)
    return E_INVALIDARG;
  if (cStrTempA.Copy(szNameW) == FALSE)
    return E_OUTOFMEMORY;
  return RemoveRequestCookie((LPSTR)cStrTempA);
}

HRESULT CHttpClient::RemoveAllRequestCookies()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  sRequest.cHttpCmn.RemoveAllCookies();
  return S_OK;
}

HRESULT CHttpClient::AddRequestPostData(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  TAutoDeletePtr<CPostDataItem> cItem;
  LPCSTR sA;

  if (nState != StateClosed)
    return MX_E_NotReady;
  if (szNameA == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  for (sA=szNameA; *sA!=0 && CHttpCommon::IsValidNameChar(*sA)!=FALSE; sA++);
  if (*sA != 0)
    return E_INVALIDARG;
  //create new pair
  cItem.Attach(MX_DEBUG_NEW CPostDataItem(szNameA, (szValueA != NULL) ? szValueA : ""));
  if (!cItem || cItem->GetName() == NULL)
    return E_OUTOFMEMORY;
  sRequest.cPostData.PushTail(cItem.Detach());
  return S_OK;
}

HRESULT CHttpClient::AddRequestPostData(__in_z LPCWSTR szNameW, __in_z LPCWSTR szValueW)
{
  CStringA cStrTempA[2];
  HRESULT hRes;

  if (szNameW == NULL)
    return E_POINTER;
  if (*szNameW == 0)
    return E_INVALIDARG;
  hRes = Utf8_Encode(cStrTempA[0], szNameW);
  if (SUCCEEDED(hRes))
    hRes = Utf8_Encode(cStrTempA[1], (szValueW != NULL) ? szValueW : L"");
  if (FAILED(hRes))
    return hRes;
  return AddRequestPostData((LPSTR)cStrTempA[0], (LPSTR)cStrTempA[1]);
}

HRESULT CHttpClient::AddRequestPostDataFile(__in_z LPCSTR szNameA, __in_z LPCSTR szFileNameA, __in CStream *lpStream)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  TAutoDeletePtr<CPostDataItem> cItem;
  LPCSTR sA;

  if (nState != StateClosed)
    return MX_E_NotReady;
  if (szNameA == NULL || szFileNameA == NULL)
    return E_POINTER;
  if (*szNameA == 0 || *szFileNameA == 0)
    return E_INVALIDARG;
  for (sA=szNameA; *sA!=0 && CHttpCommon::IsValidNameChar(*sA)!=FALSE; sA++);
  if (*sA != 0)
    return E_INVALIDARG;
  for (sA=szFileNameA; *sA!=0 && *sA!='\"' && *((LPBYTE)sA)>=32; sA++);
  if (*sA != 0)
    return E_INVALIDARG;
  //get name part
  while (sA > szFileNameA && *(sA-1) != '\\' && *(sA-1) != '/')
    sA--;
  while (*sA == ' ')
    sA++;
  if (*sA == '.')
    return E_INVALIDARG;
  //create new pair
  cItem.Attach(MX_DEBUG_NEW CPostDataItem(szNameA, sA, lpStream));
  if (!cItem || cItem->GetName() == NULL)
    return E_OUTOFMEMORY;
  sRequest.cPostData.PushTail(cItem.Detach());
  return S_OK;
}

HRESULT CHttpClient::AddRequestPostDataFile(__in_z LPCWSTR szNameW, __in_z LPCWSTR szFileNameW, __in CStream *lpStream)
{
  CStringA cStrTempA[2];
  HRESULT hRes;

  if (szNameW == NULL || szFileNameW == NULL)
    return E_POINTER;
  if (*szNameW == 0 || *szFileNameW == 0)
    return E_INVALIDARG;
  hRes = Utf8_Encode(cStrTempA[0], szNameW);
  if (SUCCEEDED(hRes))
    hRes = Utf8_Encode(cStrTempA[1], szFileNameW);
  if (FAILED(hRes))
    return hRes;
  return AddRequestPostDataFile((LPSTR)cStrTempA[0], (LPSTR)cStrTempA[1], lpStream);
}

HRESULT CHttpClient::RemoveRequestPostData(__in_z LPCSTR szNameA)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  TLnkLst<CPostDataItem>::Iterator it;
  CPostDataItem *lpItem;
  BOOL bGotOne;

  if (nState != StateClosed)
    return MX_E_NotReady;
  if (szNameA == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  bGotOne = FALSE;
  for (lpItem=it.Begin(sRequest.cPostData); lpItem!=NULL; lpItem=it.Next())
  {
    if (StrCompareA(szNameA, lpItem->GetName()) == 0)
    {
      lpItem->RemoveNode();
      delete lpItem;
      bGotOne = TRUE;
    }
  }
  return (bGotOne != FALSE) ? S_OK : MX_E_NotFound;
}

HRESULT CHttpClient::RemoveRequestPostData(__in_z LPCWSTR szNameW)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (szNameW == NULL)
    return E_POINTER;
  if (*szNameW == 0)
    return E_INVALIDARG;
  hRes = Utf8_Encode(cStrTempA, szNameW);
  if (FAILED(hRes))
    return hRes;
  return RemoveRequestPostData((LPSTR)cStrTempA);
}

HRESULT CHttpClient::RemoveAllRequestPostData()
{
  CCriticalSection::CAutoLock cLock(cMutex);
  CPostDataItem *lpItem;

  if (nState != StateClosed)
    return MX_E_NotReady;
  while ((lpItem=sRequest.cPostData.PopHead()) != NULL)
    delete lpItem;
  return S_OK;
}

HRESULT CHttpClient::SetOptionFlags(__in int _nOptionFlags)
{
  if (nState != StateClosed)
    return MX_E_NotReady;
  nOptionFlags = _nOptionFlags;
  return S_OK;
}

int CHttpClient::GetOptionFlags()
{
  return nOptionFlags;
}

HRESULT CHttpClient::Open(__in CUrl &cUrl)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  CCriticalSection::CAutoLock cLock(cMutex);

  if (cAutoRundownProt.IsAcquired() == FALSE)
    return MX_E_NotReady;

  //read properties from property bag
  cPropBag.GetDWord(MX_HTTP_CLIENT_ResponseTimeoutMs, dwResponseTimeoutMs, MX_HTTP_CLIENT_ResponseTimeoutMs_DEFVAL);
  if (dwResponseTimeoutMs < 1000)
    dwResponseTimeoutMs = 1000;
  cPropBag.GetDWord(MX_HTTP_CLIENT_MaxRedirectionsCount, dwMaxRedirCount, MX_HTTP_CLIENT_MaxRedirectionsCount_DEFVAL);
  if (dwMaxRedirCount < 1)
    dwMaxRedirCount = 1;

  sRedirect.dwRedirectCounter = 0;
  return InternalOpen(cUrl);
}

HRESULT CHttpClient::Open(__in_z LPCSTR szUrlA)
{
  CUrl cUrl;
  HRESULT hRes;

  if (szUrlA == NULL)
    return E_POINTER;
  if (*szUrlA == 0)
    return E_INVALIDARG;
  hRes = cUrl.ParseFromString(szUrlA);
  if (SUCCEEDED(hRes))
    hRes = Open(cUrl);
  return hRes;
}

HRESULT CHttpClient::Open(__in_z LPCWSTR szUrlW)
{
  CUrl cUrl;
  HRESULT hRes;

  if (szUrlW == NULL)
    return E_POINTER;
  if (*szUrlW == 0)
    return E_INVALIDARG;
  hRes = cUrl.ParseFromString(szUrlW);
  if (SUCCEEDED(hRes))
    hRes = Open(cUrl);
  return hRes;
}

VOID CHttpClient::Close(__in_opt BOOL bReuseConn)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (hConn != NULL && (nState != StateDocumentCompleted || bReuseConn == FALSE))
  {
    cSocketMgr.Close(hConn, S_OK);
    hConn = NULL;
  }
  if (lpTimedEventQueue != NULL)
  {
    if (sRedirect.lpEvent != NULL)
    {
      lpTimedEventQueue->Remove(sRedirect.lpEvent);
      sRedirect.lpEvent = NULL;
    }
    if (sResponse.lpTimeoutEvent != NULL)
    {
      lpTimedEventQueue->Remove(sResponse.lpTimeoutEvent);
      sResponse.lpTimeoutEvent = NULL;
    }
  }
  nState = StateClosed;
  return;
}

HRESULT CHttpClient::GetLastRequestError()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  return hLastErrorCode;
}

HRESULT CHttpClient::IsResponseHeaderComplete()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  return (nState == StateReceivingResponseBody || nState == StateDocumentCompleted) ? TRUE : FALSE;
}

BOOL CHttpClient::IsDocumentComplete()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  return (nState == StateDocumentCompleted) ? TRUE : FALSE;
}

BOOL CHttpClient::IsClosed()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  return (nState == StateClosed) ? TRUE : FALSE;
}

LONG CHttpClient::GetResponseStatus()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return 0;
  return sResponse.cHttpCmn.GetResponseStatus();
}

HRESULT CHttpClient::GetResponseReason(__inout CStringA &cStrDestA)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  cStrDestA.Empty();
  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return MX_E_NotReady;
  return (cStrDestA.Copy(sResponse.cHttpCmn.GetResponseReasonA()) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

CHttpBodyParserBase* CHttpClient::GetResponseBodyParser()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return NULL;
  return sResponse.cHttpCmn.GetBodyParser();
}

SIZE_T CHttpClient::GetResponseHeadersCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return 0;
  return sResponse.cHttpCmn.GetHeadersCount();
}

CHttpHeaderBase* CHttpClient::GetResponseHeader(__in SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return NULL;
  return sResponse.cHttpCmn.GetHeader(nIndex);
}

CHttpHeaderBase* CHttpClient::GetResponseHeader(__in_z LPCSTR szNameA) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return NULL;
  return sResponse.cHttpCmn.GetHeader(szNameA);
}

SIZE_T CHttpClient::GetResponseCookiesCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return 0;
  return const_cast<CHttpClient*>(this)->sResponse.cHttpCmn.GetCookies()->GetCount();
}

HRESULT CHttpClient::GetResponseCookie(__in SIZE_T nIndex, __out CHttpCookie &cCookie)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  CHttpCookieArray *lpCookieArray;

  cCookie.Clear();
  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return MX_E_NotReady;
  lpCookieArray = sResponse.cHttpCmn.GetCookies();
  if (nIndex >= lpCookieArray->GetCount())
    return E_INVALIDARG;
  return (cCookie = *(lpCookieArray->GetElementAt(nIndex)));
}

HRESULT CHttpClient::GetResponseCookies(__out CHttpCookieArray &cCookieArray)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
  {
    cCookieArray.RemoveAllElements();
    return MX_E_NotReady;
  }
  return (cCookieArray = *sResponse.cHttpCmn.GetCookies());
}

HANDLE CHttpClient::GetUnderlyingSocket() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  return hConn;
}

CSockets* CHttpClient::GetUnderlyingSocketManager() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  return const_cast<CSockets*>(&cSocketMgr);
}

HRESULT CHttpClient::InternalOpen(__in CUrl &cUrl)
{
  CStringA cStrHostA;
  int nUrlPort;
  HRESULT hRes;

  if (cUrl.GetSchemeCode() != CUrl::SchemeHttp && cUrl.GetSchemeCode() != CUrl::SchemeHttps)
    return E_INVALIDARG;
  if (cUrl.GetHost()[0] == 0)
    return E_INVALIDARG;
  if (cUrl.GetPort() >= 0)
    nUrlPort = cUrl.GetPort();
  else if (cUrl.GetSchemeCode() == CUrl::SchemeHttps)
    nUrlPort = 443;
  else
    nUrlPort = 80;
  //disable redirection
  if (lpTimedEventQueue != NULL)
  {
    if (sRedirect.lpEvent != NULL)
    {
      lpTimedEventQueue->Remove(sRedirect.lpEvent);
      sRedirect.lpEvent = NULL;
    }
    if (sResponse.lpTimeoutEvent != NULL)
    {
      lpTimedEventQueue->Remove(sResponse.lpTimeoutEvent);
      sResponse.lpTimeoutEvent = NULL;
    }
  }
  //can reuse connection?
  if (hConn != NULL)
  {
    if (sResponse.cHttpCmn.GetParserState() != CHttpCommon::StateDone ||
        StrCompareW(sRequest.cUrl.GetScheme(), cUrl.GetScheme()) != 0 ||
        StrCompareW(sRequest.cUrl.GetHost(), cUrl.GetHost()) != 0 ||
        sRequest.cUrl.GetPort() != nUrlPort)
    {
      cSocketMgr.Close(hConn, S_OK);
      hConn = NULL;
    }
  }
  //cleanup some stuff
  sResponse.cHttpCmn.ResetParser();
  //setup new state
  nState = StateSendingRequest;
  //create a new connection if needed
  if (hConn == NULL)
  {
    hRes = (sRequest.cUrl = cUrl);
    if (SUCCEEDED(hRes))
    {
      sRequest.cUrl.SetPort(nUrlPort);
      hRes = cSocketMgr.ConnectToServer(CSockets::FamilyIPv4, sRequest.cUrl.GetHost(), nUrlPort,
                                        MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnSocketCreate, this), NULL, &hConn);
    }
  }
  else
  {
    CIpc::CLayerList cDummyLayersList;

    hRes = OnSocketConnect(&cSocketMgr, hConn, NULL, cDummyLayersList, 0x12345678); //special error code value
  }
  if (FAILED(hRes))
  {
    nState = StateClosed;
    return hRes;
  }
  //done
  return S_OK;
}

HRESULT CHttpClient::OnSocketCreate(__in CIpc *lpIpc, __in HANDLE h, __deref_inout CIpc::CUserData **lplpUserData,
                                    __inout CIpc::CREATE_CALLBACK_DATA &sData)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);

  if (cAutoRundownProt.IsAcquired() == FALSE)
    return MX_E_NotReady;
  //setup socket
  sData.dwWriteTimeoutMs = 1000;
  sData.cConnectCallback = MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnSocketConnect, this);
  sData.cDataReceivedCallback = MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnSocketDataReceived, this);
  sData.cDestroyCallback = MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnSocketDestroy, this);
  //setup SSL layer
  if (sRequest.cUrl.GetSchemeCode() == CUrl::SchemeHttps)
  {
    CIpcSslLayer::eProtocol nProtocol;
    CSslCertificateArray *lpCheckCertificates;
    CSslCertificate *lpSelfCert;
    CCryptoRSA *lpPrivKey;
    TAutoDeletePtr<CIpcSslLayer> cLayer;
    HRESULT hRes;

    nProtocol = CIpcSslLayer::ProtocolUnknown;
    lpCheckCertificates = NULL;
    lpSelfCert = NULL;
    lpPrivKey = NULL;
    //query for client certificates
    if (!cQueryCertificatesCallback)
      return MX_E_NotReady;
    hRes = cQueryCertificatesCallback(this, nProtocol, lpCheckCertificates, lpSelfCert, lpPrivKey);
    if (FAILED(hRes))
      return hRes;
    //add ssl layer
    cLayer.Attach(MX_DEBUG_NEW CIpcSslLayer());
    if (!cLayer)
      return E_OUTOFMEMORY;
    hRes = cLayer->Initialize(FALSE, nProtocol, lpCheckCertificates, lpSelfCert, lpPrivKey);
    if (FAILED(hRes))
      return hRes;
    sData.cLayersList.PushTail(cLayer.Detach());
  }
  //done
  return cPendingHandles.Add(h);
}

VOID CHttpClient::OnSocketDestroy(__in CIpc *lpIpc, __in HANDLE h, __in CIpc::CUserData *lpUserData,
                                  __in HRESULT hErrorCode)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  BOOL bRaiseDocCompletedCallback, bRaiseErrorCallback;

  bRaiseDocCompletedCallback = bRaiseErrorCallback = FALSE;
  if (cAutoRundownProt.IsAcquired() != FALSE)
  {
    CCriticalSection::CAutoLock cLock(cMutex);

    if (h == hConn)
    {
      switch (nState)
      {
        case StateWaitingForRedirection:
          cSocketMgr.Close(hConn, S_OK);
          hConn = NULL;
          break;

        case StateSendingRequest:
        case StateReceivingResponseHeaders:
          if (SUCCEEDED(hErrorCode))
            hErrorCode = MX_E_Cancelled;
          SetErrorOnRequestAndClose(hErrorCode);
          bRaiseErrorCallback = TRUE;
          break;

        case StateReceivingResponseBody:
          cSocketMgr.Close(hConn, S_OK);
          hConn = NULL;
          nState = StateDocumentCompleted;
          bRaiseDocCompletedCallback = TRUE;
          break;

        case StateDocumentCompleted:
          cSocketMgr.Close(hConn, S_OK);
          hConn = NULL;
          break;

        default:
          SetErrorOnRequestAndClose(S_OK);
          break;
      }
    }
  }
  if (bRaiseDocCompletedCallback && cDocumentCompletedCallback)
    cDocumentCompletedCallback(this);
  if (bRaiseErrorCallback && cErrorCallback)
    cErrorCallback(this, hErrorCode);
  //done
  cPendingHandles.Remove(h);
  return;
}

HRESULT CHttpClient::OnSocketConnect(__in CIpc *lpIpc, __in HANDLE h, __in CIpc::CUserData *lpUserData,
                                     __inout CIpc::CLayerList &cLayersList, __in HRESULT hErrorCode)
{
  HRESULT hRes;

  {
    CCriticalSection::CAutoLock cLock(cMutex);

    //does belong tu us?
    if (h != hConn)
      return MX_E_Cancelled;
    if (SUCCEEDED(hErrorCode))
    {
      if (nState == StateSendingRequest)
      {
        CStringA cStrReqHdrsA;
        BOOL bHasBody;

        bHasBody = (StrCompareA((LPSTR)(sRequest.cStrMethodA), "HEAD") != 0 &&
                    sRequest.cPostData.IsEmpty() == FALSE) ? TRUE : FALSE;
        //----
        hRes = BuildRequestHeaders(cStrReqHdrsA);
        if (SUCCEEDED(hRes) && bHasBody != FALSE)
          hRes = AddRequestHeadersForBody(cStrReqHdrsA);
        if (SUCCEEDED(hRes) && cStrReqHdrsA.ConcatN("\r\n", 2) == FALSE)
          hRes = E_OUTOFMEMORY;
        if (SUCCEEDED(hRes))
        {
#ifdef HTTP_DEBUG_OUTPUT
          MX::DebugPrint("HttpClient(ReqHeaders/0x%p): %s\n", this, (LPSTR)cStrReqHdrsA);
#endif //HTTP_DEBUG_OUTPUT
          hRes = cSocketMgr.SendMsg(hConn, (LPSTR)cStrReqHdrsA, cStrReqHdrsA.GetLength());
        }
        if (SUCCEEDED(hRes) && bHasBody != FALSE)
          hRes = SendRequestBody();
        if (SUCCEEDED(hRes))
          hRes = cSocketMgr.AfterWriteSignal(h, MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnAfterSendRequest, this), NULL);
        if (SUCCEEDED(hRes))
          nState = StateReceivingResponseHeaders;
      }
      else
      {
        hRes = S_OK;
      }
    }
    else
    {
      hRes = hErrorCode;
    }
    if (FAILED(hRes))
      SetErrorOnRequestAndClose(hRes);
  }

  //raise error event if any
  if (FAILED(hRes) && cErrorCallback)
    cErrorCallback(this, hRes);
  return S_OK;
}

//NOTE: "CIpc" guarantees no simultaneous calls to 'OnSocketDataReceived' will be received from different threads
HRESULT CHttpClient::OnSocketDataReceived(__in CIpc *lpIpc, __in HANDLE h, __in CIpc::CUserData *lpUserData)
{
  BYTE aMsgBuf[4096];
  SIZE_T nMsgSize, nMsgUsed;
  BOOL bCheckConnection, bFireResponseHeadersReceivedCallback, bFireDocumentCompleted;
  HRESULT hRes;

  nMsgSize = nMsgUsed = 0;
  bCheckConnection = TRUE;
  hRes = S_OK;
restart:
  bFireResponseHeadersReceivedCallback = bFireDocumentCompleted = FALSE;
  {
    CCriticalSection::CAutoLock cLock(cMutex);

    //does belong to us?
    if (bCheckConnection != FALSE && h != hConn)
      return MX_E_Cancelled;
    bCheckConnection = TRUE;
    //loop
    while (SUCCEEDED(hRes) && bFireResponseHeadersReceivedCallback == FALSE && bFireDocumentCompleted == FALSE)
    {
      //get buffered message
      if (nMsgSize == 0)
      {
        nMsgUsed = 0;
        nMsgSize = sizeof(aMsgBuf);
        hRes = cSocketMgr.GetBufferedMessage(h, aMsgBuf, &nMsgSize);
        if (SUCCEEDED(hRes))
        {
          if (nMsgSize == 0)
            break;
          hRes = cSocketMgr.ConsumeBufferedMessage(h, nMsgSize);
        }
        if (FAILED(hRes))
        {
          SetErrorOnRequestAndClose(hRes);
          break;
        }
      }
      //take action depending on current state
      if (nState == StateReceivingResponseHeaders || nState == StateReceivingResponseBody)
      {
        CHttpCommon::eState nParserState;
        SIZE_T nNewUsed;

        //process http being received
        hRes = sResponse.cHttpCmn.Parse(aMsgBuf+nMsgUsed, nMsgSize-nMsgUsed, nNewUsed);
        if ((nMsgUsed += nNewUsed) >= nMsgSize)
          nMsgSize = 0; //mark end of message
        if (FAILED(hRes))
        {
          SetErrorOnRequestAndClose(hRes);
          break;
        }
        //take action if parser's state changed
        nParserState = sResponse.cHttpCmn.GetParserState();
        //check if we hit a redirection
        if (nParserState == CHttpCommon::StateBodyStart)
        {
          LONG nRespStatus = sResponse.cHttpCmn.GetResponseStatus();

          if ((nRespStatus == 301 || nRespStatus == 302 || nRespStatus == 303 ||
                nRespStatus == 305 || nRespStatus == 307 || nRespStatus == 408) &&
              sRedirect.dwRedirectCounter < dwMaxRedirCount)
          {
            CHttpBodyParserBase *lpParser;

            lpParser = MX_DEBUG_NEW CHttpBodyParserIgnore();
            if (lpParser == NULL)
            {
              SetErrorOnRequestAndClose(E_OUTOFMEMORY);
              break;
            }
            hRes = sResponse.cHttpCmn.SetBodyParser(lpParser, cPropBag);
            lpParser->Release();
            if (FAILED(hRes))
            {
              SetErrorOnRequestAndClose(hRes);
              break;
            }
          }
          else
          {
            bFireResponseHeadersReceivedCallback = TRUE;
          }
          nState = StateReceivingResponseBody;
        }
        else if (nParserState == CHttpCommon::StateDone)
        {
          LONG nRespStatus = sResponse.cHttpCmn.GetResponseStatus();

          if ((nRespStatus == 301 || nRespStatus == 302 || nRespStatus == 303 ||
              nRespStatus == 305 || nRespStatus == 307 || nRespStatus == 408) &&
              sRedirect.dwRedirectCounter < dwMaxRedirCount)
          {
            CUrl cUrlTemp;
            ULONGLONG nWaitTimeSecs;

            sRedirect.dwRedirectCounter++;
            nWaitTimeSecs = 0ui64;
            //build new url
            hRes = (sRedirect.cUrl = sRequest.cUrl);
            if (SUCCEEDED(hRes))
            {
              if (sResponse.cHttpCmn.GetResponseStatus() != 408)
              {
                CHttpHeaderRespLocation *lpHeader = sResponse.cHttpCmn.GetHeader<CHttpHeaderRespLocation>();
                if (lpHeader != NULL)
                {
                  if (SUCCEEDED(hRes))
                    hRes = cUrlTemp.ParseFromString(lpHeader->GetLocation());
                  if (SUCCEEDED(hRes))
                    hRes = sRedirect.cUrl.Merge(cUrlTemp);
                }
                else
                {
                  hRes = MX_E_NotFound;
                }
              }
            }
            //check for a retry timeout value
            if (SUCCEEDED(hRes))
            {
              CHttpHeaderRespRetryAfter *lpHeader;

              lpHeader = sResponse.cHttpCmn.GetHeader<CHttpHeaderRespRetryAfter>();
              if (lpHeader != NULL)
              {
                nWaitTimeSecs = lpHeader->GetSeconds();
                if (nWaitTimeSecs > 600ui64)
                  nWaitTimeSecs = 600ui64;
              }
            }
            //merge cookies
            if (SUCCEEDED(hRes))
            {
              hRes = sRequest.cHttpCmn.GetCookies()->Update(*sResponse.cHttpCmn.GetCookies(), TRUE);
              if (SUCCEEDED(hRes))
                hRes = sRequest.cHttpCmn.GetCookies()->RemoveExpiredAndInvalid();
            }
            //start redirector/waiter thread
            if (SUCCEEDED(hRes) && lpTimedEventQueue == NULL)
            {
              lpTimedEventQueue = CSystemTimedEventQueue::Get();
              if (lpTimedEventQueue == NULL)
                hRes = E_OUTOFMEMORY;
            }
            if (SUCCEEDED(hRes))
            {
              nState = StateWaitingForRedirection;
              //----
              if (sRedirect.lpEvent != NULL)
                lpTimedEventQueue->Remove(sRedirect.lpEvent);
              sRedirect.lpEvent = MX_DEBUG_NEW CTimedEventQueue::CEvent(
                        MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnRedirect, this));
              if (sRedirect.lpEvent != NULL)
              {
                hRes = cPendingEvents.Add(sRedirect.lpEvent);
                if (SUCCEEDED(hRes))
                  hRes = lpTimedEventQueue->Add(sRedirect.lpEvent, (DWORD)(nWaitTimeSecs * 1000ui64));
                if (FAILED(hRes))
                {
                  cPendingEvents.Remove(sRedirect.lpEvent);
                  delete sRedirect.lpEvent;
                  sRedirect.lpEvent = NULL;
                }
              }
              else
              {
                hRes = E_OUTOFMEMORY;
              }
            }
            if (FAILED(hRes))
              SetErrorOnRequestAndClose(hRes);
          }
          else
          {
            //no redirection
            bFireDocumentCompleted = TRUE;
            if (nState == StateReceivingResponseHeaders)
              bFireResponseHeadersReceivedCallback = TRUE;
            nState = StateDocumentCompleted;
          }
          break;
        }
      }
      else
      {
        //else ignore received data
        nMsgSize = 0;
      }
    }
  }

  //have some action to do?
  if (SUCCEEDED(hRes) && bFireResponseHeadersReceivedCallback != FALSE)
  {
    CHttpBodyParserBase *lpBodyParser = NULL;

    if (cHeadersReceivedCallback)
      hRes = cHeadersReceivedCallback(this, lpBodyParser);
    if (SUCCEEDED(hRes))
    {
      CCriticalSection::CAutoLock cLock(cMutex);
      TAutoRefCounted<CHttpBodyParserBase> cBodyParser;

      cBodyParser.Attach(lpBodyParser);
      if (h == hConn)
      {
        if (!cBodyParser)
        {
          cBodyParser.Attach(sResponse.cHttpCmn.GetDefaultBodyParser());
          if (!cBodyParser)
            hRes = E_OUTOFMEMORY;
        }
        if (SUCCEEDED(hRes))
          hRes = sResponse.cHttpCmn.SetBodyParser(cBodyParser.Get(), cPropBag);
      }
    }
  }
  if (SUCCEEDED(hRes) && bFireDocumentCompleted != FALSE)
  {
    if (cDocumentCompletedCallback)
      hRes = cDocumentCompletedCallback(this);
  }
  //restart on success
  if (SUCCEEDED(hRes))
  {
    if (bFireResponseHeadersReceivedCallback != FALSE || bFireDocumentCompleted != FALSE)
      goto restart;
  }
  else
  {
    CCriticalSection::CAutoLock cLock(cMutex);

    SetErrorOnRequestAndClose(hRes);
  }
  //raise error event if any
  if (FAILED(hRes) && cErrorCallback)
    cErrorCallback(this, hRes);
  return hRes;
}

VOID CHttpClient::OnRedirect(__in CTimedEventQueue::CEvent *lpEvent)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);

  if (cAutoRundownProt.IsAcquired() != FALSE)
  {
    HRESULT hRes;

    hRes = S_OK;
    {
      CCriticalSection::CAutoLock cLock(cMutex);

      if (sRedirect.lpEvent == lpEvent)
      {
        if (lpEvent->IsCanceled() == FALSE)
        {
          if (nState == StateWaitingForRedirection)
            hRes = InternalOpen(sRedirect.cUrl);
        }
        sRedirect.lpEvent = NULL;
      }
    }
    //raise error event if any
    if (FAILED(hRes) && cErrorCallback)
      cErrorCallback(this, hRes);
  }
  //release the event and remove from pending list
  lpEvent->Release();
  cPendingEvents.Remove(lpEvent);
  return;
}

VOID CHttpClient::OnResponseTimeout(__in CTimedEventQueue::CEvent *lpEvent)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);

  if (cAutoRundownProt.IsAcquired() != FALSE)
  {
    HRESULT hRes;

    hRes = S_OK;
    {
      CCriticalSection::CAutoLock cLock(cMutex);

      if (sResponse.lpTimeoutEvent == lpEvent)
      {
        if (nState == StateReceivingResponseHeaders || nState == StateReceivingResponseBody)
        {
          SetErrorOnRequestAndClose(hRes = MX_E_Timeout);
        }
        sResponse.lpTimeoutEvent = NULL;
      }
    }
    //raise error event if any
    if (FAILED(hRes) && cErrorCallback)
      cErrorCallback(this, hRes);
  }
  //delete the event and remove from pending list
  lpEvent->Release();
  cPendingEvents.Remove(lpEvent);
  return;
}

VOID CHttpClient::OnAfterSendRequest(__in CIpc *lpIpc, __in HANDLE h, __in LPVOID lpCookie,
                                     __in CIpc::CUserData *lpUserData)
{
  HRESULT hRes;

  hRes = S_OK;
  {
    CCriticalSection::CAutoLock cLock(cMutex);

    if (h == hConn && (nState == StateReceivingResponseHeaders || nState == StateReceivingResponseBody))
    {
      //get system timeout queue manager
      if (lpTimedEventQueue == NULL)
      {
        lpTimedEventQueue = CSystemTimedEventQueue::Get();
        if (lpTimedEventQueue == NULL)
          hRes = E_OUTOFMEMORY;
      }
      //add timeout
      if (SUCCEEDED(hRes))
      {
        if (sResponse.lpTimeoutEvent != NULL)
          lpTimedEventQueue->Remove(sResponse.lpTimeoutEvent);
        sResponse.lpTimeoutEvent =
            MX_DEBUG_NEW CTimedEventQueue::CEvent(MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnResponseTimeout, this));
        if (sResponse.lpTimeoutEvent != NULL)
        {
          hRes = cPendingEvents.Add(sResponse.lpTimeoutEvent);
          if (SUCCEEDED(hRes))
            hRes = lpTimedEventQueue->Add(sResponse.lpTimeoutEvent, dwResponseTimeoutMs);
          if (FAILED(hRes))
          {
            cPendingEvents.Remove(sResponse.lpTimeoutEvent);
            delete sResponse.lpTimeoutEvent;
            sResponse.lpTimeoutEvent = NULL;
          }
        }
        else
        {
          hRes = E_OUTOFMEMORY;
        }
      }
      if (FAILED(hRes))
        SetErrorOnRequestAndClose(hRes);
    }
  }
  //raise error event if any
  if (FAILED(hRes) && cErrorCallback)
    cErrorCallback(this, hRes);
  return;
}

VOID CHttpClient::SetErrorOnRequestAndClose(__in HRESULT hErrorCode)
{
  nState = StateClosed;
  if (SUCCEEDED(hLastErrorCode)) //preserve first error
    hLastErrorCode = hErrorCode;
  cSocketMgr.Close(hConn, hLastErrorCode);
  hConn = NULL;
  return;
}

HRESULT CHttpClient::BuildRequestHeaders(__inout CStringA &cStrReqHdrsA)
{
  CHttpHeaderBase *lpHeader;
  CStringA cStrTempA;
  CHttpCookie *lpCookie;
  SIZE_T nIndex;
  HRESULT hRes;

  if (cStrReqHdrsA.EnsureBuffer(32768) == FALSE)
    return E_OUTOFMEMORY;
  //1) GET/POST/{methood} url HTTP/1.1
  if (sRequest.cStrMethodA.IsEmpty() != FALSE)
  {
    if (cStrReqHdrsA.Copy((sRequest.cPostData.IsEmpty() == FALSE) ? "POST " : "GET ") == FALSE)
      return E_OUTOFMEMORY;
  }
  else
  {
    if (cStrReqHdrsA.Copy((LPSTR)(sRequest.cStrMethodA)) == FALSE ||
      cStrReqHdrsA.ConcatN(" ", 1) == FALSE)
      return E_OUTOFMEMORY;
  }
  hRes = sRequest.cUrl.ToString(cStrTempA, CUrl::ToStringAddPath|CUrl::ToStringAddQueryStrings);
  if (FAILED(hRes))
    return hRes;
  if (cStrReqHdrsA.Concat((LPSTR)cStrTempA) == FALSE ||
      cStrReqHdrsA.Concat(" HTTP/1.1\r\n") == FALSE)
    return E_OUTOFMEMORY;
  //2) Accept: */*
  hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "Accept", "*/*");
  if (FAILED(hRes))
    return hRes;
  //3) Accept-Encoding: gzip, deflate
  hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "Accept-Encoding",
                               (nOptionFlags & OptionAcceptCompressedContent) ? "gzip,deflate" : "");
  if (FAILED(hRes))
    return hRes;
  //4) Accept-Language: en-us,ie_ee;q=0.5
  hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "Accept-Language", "en-us");
  if (FAILED(hRes))
    return hRes;
  //5) Host: www.mydomain.com
  hRes = sRequest.cUrl.ToString(cStrTempA, CUrl::ToStringAddHostPort);
  if (SUCCEEDED(hRes))
    hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "Host", (LPCSTR)cStrTempA);
  if (FAILED(hRes))
    return hRes;
  //6) Referer
  hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "Referer", "");
  if (FAILED(hRes))
    return hRes;
  //7) User-Agent: Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322) (opcional)
  hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "User-Agent", "Mozilla/5.0 (compatible; MX-Lib 1.0)");
  if (FAILED(hRes))
    return hRes;
  //8) Connection: close (si keepalive es falso)
  hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "Connection",
                               (nOptionFlags & OptionKeepConnectionOpen) ? "Keep-Alive" : "Close");
  if (FAILED(hRes))
    return hRes;
  //9) the rest of the headers
  nIndex = 0;
  while ((lpHeader = sRequest.cHttpCmn.GetHeader(nIndex++)) != NULL)
  {
    if (StrCompareA(lpHeader->GetName(), "Accept", TRUE) != 0 &&
        StrCompareA(lpHeader->GetName(), "Accept-Encoding", TRUE) != 0 &&
        StrCompareA(lpHeader->GetName(), "Accept-Language", TRUE) != 0 &&
        StrCompareA(lpHeader->GetName(), "Host", TRUE) != 0 &&
        StrCompareA(lpHeader->GetName(), "Referer", TRUE) != 0 &&
        StrCompareA(lpHeader->GetName(), "User-Agent", TRUE) != 0 &&
        StrCompareA(lpHeader->GetName(), "Connection", TRUE) != 0 &&
        StrCompareA(lpHeader->GetName(), "Cookie", TRUE) != 0 &&
        StrCompareA(lpHeader->GetName(), "Cookie2", TRUE) != 0 &&
        StrCompareA(lpHeader->GetName(), "Expect", TRUE) != 0)
    {
      hRes = lpHeader->Build(cStrTempA);
      if (FAILED(hRes))
        return hRes;
      if (cStrTempA.IsEmpty() == FALSE)
      {
        if (cStrReqHdrsA.AppendFormat("%s: %s\r\n", lpHeader->GetName(), (LPSTR)cStrTempA) == FALSE)
          return E_OUTOFMEMORY;
      }
    }
  }
  //10) Cookies: ????
  cStrTempA.Empty();
  for (nIndex=0; (lpCookie = sRequest.cHttpCmn.GetCookie(nIndex)) != NULL; nIndex++)
  {
    if (lpCookie->DoesDomainMatch(sRequest.cUrl.GetHost()) != FALSE)
    {
      if (cStrTempA.AppendFormat("%s%s=%s", ((cStrTempA.IsEmpty() == FALSE) ? "; " : ""),
        lpCookie->GetName(), lpCookie->GetValue()) == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  if (cStrTempA.IsEmpty() == FALSE)
  {
    if (cStrReqHdrsA.AppendFormat("Cookie: %s\r\n", (LPCSTR)cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
  }
  return S_OK;
}

HRESULT CHttpClient::BuildRequestHeaderAdd(__inout CStringA &cStrReqHdrsA, __in_z LPCSTR szNameA,
                                           __in_z LPCSTR szDefaultValueA)
{
  CHttpHeaderBase *lpHeader;
  CStringA cStrTempA;
  LPCSTR szValueA;
  HRESULT hRes;

  szValueA = szDefaultValueA;
  lpHeader = sRequest.cHttpCmn.GetHeader(szNameA);
  if (lpHeader != NULL)
  {
    hRes = lpHeader->Build(cStrTempA);
    if (FAILED(hRes))
      return hRes;
    if (cStrTempA.IsEmpty() == FALSE)
      szValueA = (LPCSTR)cStrTempA;
  }
  if (*szValueA != 0)
  {
    if (cStrReqHdrsA.Concat(szNameA) == FALSE ||
        cStrReqHdrsA.ConcatN(": ", 2) == FALSE ||
        cStrReqHdrsA.Concat(szValueA) == FALSE ||
        cStrReqHdrsA.ConcatN("\r\n", 2) == FALSE)
      return E_OUTOFMEMORY;
  }
  return S_OK;
}

HRESULT CHttpClient::AddRequestHeadersForBody(__inout CStringA &cStrReqHdrsA)
{
  TLnkLst<CPostDataItem>::Iterator it;
  CPostDataItem *lpItem;
  CStream *lpStream;
  DWORD dw;
  ULONGLONG nLen;

  nLen = 0ui64;
  for (lpItem=it.Begin(sRequest.cPostData); lpItem!=NULL; lpItem=it.Next())
  {
    if (lpItem->GetStream() != NULL)
    {
      nLen = ULONGLONG_MAX;
      break;
    }
    if (nLen > 0ui64)
      nLen++; //size of '&'
    nLen += (ULONGLONG)CUrl::GetEncodedLength(lpItem->GetName()); //size of encoded name
    nLen++; //size of '='
    nLen += (ULONGLONG)CUrl::GetEncodedLength(lpItem->GetValue()); //size of encoded value
  }
  //no files specified and post data length is small
  if (nLen < (ULONGLONG)MAX_FORM_SIZE_4_REQUEST)
  {
    //add content type
    if (cStrReqHdrsA.Concat("Content-Type: application/x-www-form-urlencoded\r\n") == FALSE)
      return E_OUTOFMEMORY;
    //add content length
    if (cStrReqHdrsA.AppendFormat("Content-Length: %I64u\r\n", nLen) == FALSE)
      return E_OUTOFMEMORY;
    //----
    sRequest.bUsingMultiPartFormData = FALSE;
  }
  else
  {
    Fnv64_t value;
    LPVOID lpPtr;
    SIZE_T i;

    //build boundary name
    MemCopy(sRequest.szBoundary, "MXLIB_HTTP_", 11);
    dw = ::GetTickCount();
    value = fnv_64a_buf(&dw, 4, FNV1A_64_INIT);
    dw = MxGetCurrentProcessId();
    value = fnv_64a_buf(&dw, 4, value);
    lpPtr = this;
    value = fnv_64a_buf(&lpPtr, sizeof(lpPtr), value);
    for (i=0; i<16; i++)
    {
      SIZE_T val = (SIZE_T)(value & 15);
      sRequest.szBoundary[11 + i] = (CHAR)((i >= 10) ? 55+val : 48+val);
      value >>= 4;
    }
    sRequest.szBoundary[27] = 0;

    //add content type
    if (cStrReqHdrsA.AppendFormat("Content-Type:multipart/form-data; boundary=----%s\r\n", sRequest.szBoundary) == FALSE)
      return E_OUTOFMEMORY;

    //add content length
    nLen = 0ui64;
    for (lpItem=it.Begin(sRequest.cPostData); lpItem!=NULL; lpItem=it.Next())
    {
      nLen += 4ui64 + 27ui64 + 2ui64; //size of boundary start
      nLen += 38ui64; //size of 'Content-Disposition: form-data; name="'
      nLen += (ULONGLONG)StrLenA(lpItem->GetName());
      nLen += 1ui64; //size of '"'
      lpStream = lpItem->GetStream();
      if (lpStream == NULL)
      {
        nLen += 2ui64; //size of '\r\n'
        nLen += 27ui64; //size of 'Content-Type: text/plain;\r\n'
        nLen += 2ui64; //size of '\r\n'
        nLen += (ULONGLONG)StrLenA(lpItem->GetValue()); //size of data
        nLen += 2ui64; //size of '\r\n'
      }
      else
      {
        nLen += 12ui64; //size of '; filename="'
        nLen += (ULONGLONG)StrLenA(lpItem->GetValue()); //size of filename
        nLen += 3ui64; //size of '"\r\n'
        nLen += 14ui64; //size of 'Content-type: '
        nLen += (ULONGLONG)StrLenA(CHttpCommon::GetMimeType(lpItem->GetValue())); //size of mime type
        nLen += 2ui64; //size of '"\r\n'
        nLen += 33ui64; //size of 'Content-Transfer-Encoding: binary\r\n'
        nLen += 2ui64; //size of '\r\n'
        nLen += lpStream->GetLength(); //size of data
        nLen += 2ui64; //size of '\r\n'
      }
    }
    nLen += 4ui64 + 27ui64 + 2ui64 + 2ui64; //size of boundary end
    if (cStrReqHdrsA.AppendFormat("Content-Length: %I64u\r\n", ++nLen) == FALSE)
      return E_OUTOFMEMORY;
    //----
    sRequest.bUsingMultiPartFormData = TRUE;
  }
  //done
  return S_OK;
}

HRESULT CHttpClient::SendRequestBody()
{
  TLnkLst<CPostDataItem>::Iterator it;
  CPostDataItem *lpItem;
  CStringA cStrTempA;
  HRESULT hRes;

  if (sRequest.bUsingMultiPartFormData == FALSE)
  {
    CStringA cStrValueEncA;

    if (cStrTempA.EnsureBuffer(MAX_FORM_SIZE_4_REQUEST + 2) == FALSE)
      return E_OUTOFMEMORY;
    for (lpItem=it.Begin(sRequest.cPostData); lpItem!=NULL; lpItem=it.Next())
    {
      //add separator
      if (cStrTempA.IsEmpty() == FALSE)
      {
        if (cStrTempA.Concat("&") == FALSE)
          return E_OUTOFMEMORY;
      }
      //add encoded name
      hRes = CUrl::Encode(cStrValueEncA, lpItem->GetName());
      if (FAILED(hRes))
        return hRes;
      if (cStrTempA.Concat((LPSTR)cStrValueEncA) == FALSE)
        return E_OUTOFMEMORY;
      //add equal sign
      if (cStrTempA.Concat("=") == FALSE)
        return E_OUTOFMEMORY;
      //add encoded value
      hRes = CUrl::Encode(cStrValueEncA, lpItem->GetValue());
      if (FAILED(hRes))
        return hRes;
      if (cStrTempA.Concat((LPSTR)cStrValueEncA) == FALSE)
        return E_OUTOFMEMORY;
    }
    //include end of body '\r\n'
    if (cStrTempA.Concat("\r\n\r\n") == FALSE)
      return E_OUTOFMEMORY;
    hRes = cSocketMgr.SendMsg(hConn, (LPSTR)cStrTempA, cStrTempA.GetLength());
    if (FAILED(hRes))
      return hRes;
  }
  else
  {
    CStream *lpStream;

    for (lpItem=it.Begin(sRequest.cPostData); lpItem!=NULL; lpItem=it.Next())
    {
      //add boundary start and content disposition
      if (cStrTempA.Format("----%s\r\nContent-Disposition: form-data; name=\"%s\"", sRequest.szBoundary,
                           lpItem->GetName()) == FALSE)
        return E_OUTOFMEMORY;

      lpStream = lpItem->GetStream();
      if (lpStream == NULL)
      {
        //add content type and value
        if (cStrTempA.Concat("\r\nContent-Type: text/plain;\r\n\r\n") == FALSE ||
            cStrTempA.Concat(lpItem->GetValue()) == FALSE ||
            cStrTempA.Concat("\r\n") == FALSE)
          return E_OUTOFMEMORY;

        //send
        hRes = cSocketMgr.SendMsg(hConn, (LPSTR)cStrTempA, cStrTempA.GetLength());
        if (FAILED(hRes))
          return hRes;
      }
      else
      {
        //add filename, content type and content transfer encoding
        if (cStrTempA.AppendFormat("; filename=\"%s\"\r\nContent-Type: %s;\r\nContent-Transfer-Encoding: binary\r\n"
                                   "\r\n", lpItem->GetValue(), CHttpCommon::GetMimeType(lpItem->GetValue())) == FALSE)
          return E_OUTOFMEMORY;
        //send
        hRes = cSocketMgr.SendMsg(hConn, (LPSTR)cStrTempA, cStrTempA.GetLength());
        if (FAILED(hRes))
          return hRes;

        //send data stream
        hRes = cSocketMgr.SendStream(hConn, lpStream);
        if (FAILED(hRes))
          return hRes;

        //send end of data stream
        hRes = cSocketMgr.SendMsg(hConn, "\r\n", 2);
        if (FAILED(hRes))
          return hRes;
      }
    }

    //send boundary end and end of body '\r\n'
    if (cStrTempA.Format("----%s--\r\n\r\n", sRequest.szBoundary) == FALSE)
      return E_OUTOFMEMORY;
    hRes = cSocketMgr.SendMsg(hConn, (LPSTR)cStrTempA, cStrTempA.GetLength());
    if (FAILED(hRes))
      return hRes;
  }
  //done
  return S_OK;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CHttpClient::CPostDataItem::CPostDataItem(__in_z LPCSTR _szNameA, __in_z LPCSTR _szValueA, __in_opt CStream *lpStream) :
                                          CBaseMemObj(), TLnkLstNode<CPostDataItem>()
{
  SIZE_T nNameLen, nValueLen;

  szNameA = szValueA = NULL;
  nNameLen = StrLenA(_szNameA);
  nValueLen = StrLenA(_szValueA);
  if (nNameLen+nValueLen > nNameLen && nNameLen+nValueLen+2 > nNameLen+nValueLen)
  {
    szNameA = (LPSTR)MX_MALLOC(nNameLen+nValueLen+2);
    if (szNameA != NULL)
    {
      szValueA = szNameA + nNameLen+1;
      MemCopy(szNameA, _szNameA, nNameLen);
      szNameA[nNameLen] = 0;
      MemCopy(szValueA, _szValueA, nValueLen);
      szValueA[nValueLen] = 0;
    }
  }
  cStream = lpStream;
  return;
}

CHttpClient::CPostDataItem::~CPostDataItem()
{
  MX_FREE(szNameA);
  return;
}

} //namespace MX
