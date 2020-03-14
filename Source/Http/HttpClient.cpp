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
#include "..\..\Include\Http\HttpClient.h"
#include "..\..\Include\Strings\Utf8.h"
#include "..\..\Include\FnvHash.h"
#include "..\..\Include\MemoryStream.h"
#include "..\..\Include\TimedEvent.h"
#include "..\..\Include\DateTime\DateTime.h"
#include "..\..\Include\Comm\IpcSslLayer.h"
#include "..\..\Include\Http\Url.h"
#include "..\..\Include\Http\HttpCommon.h"
#include "HttpAuthCache.h"

//-----------------------------------------------------------

#define MAX_FORM_SIZE_4_REQUEST                 (128 * 1024)
#define DEFAULT_MAX_REDIRECTIONS_COUNT                    10
#define DEFAULT_MAX_AUTHORIZATION_COUNT                   10

#define _HTTP_STATUS_MovedPermanently                    301
#define _HTTP_STATUS_MovedTemporarily                    302
#define _HTTP_STATUS_SeeOther                            303
#define _HTTP_STATUS_UseProxy                            305
#define _HTTP_STATUS_TemporaryRedirect                   307
#define _HTTP_STATUS_Unauthorized                        401
#define _HTTP_STATUS_ProxyAuthenticationRequired         407
#define _HTTP_STATUS_RequestTimeout                      408

//-----------------------------------------------------------

static const LPCSTR szDefaultUserAgentA = "Mozilla/5.0 (compatible; MX-Lib 1.0)";

//-----------------------------------------------------------

namespace MX {

CHttpClient::CHttpClient(_In_ CSockets &_cSocketMgr, _In_opt_ CLoggable *lpLogParent) : CBaseMemObj(),
                         CLoggable(), CNonCopyableObj(), cSocketMgr(_cSocketMgr), sRequest{ {TRUE, this} },
                         sResponse{ { FALSE, this} }
{
  SetLogParent(lpLogParent);
  //----
  dwResponseHeaderTimeoutMs = 30000;
  dwResponseBodyMinimumThroughputInBps = 256;
  dwResponseBodySecondsOfLowThroughput = 10;
  dwMaxRedirCount = DEFAULT_MAX_REDIRECTIONS_COUNT;
  dwMaxFieldSize = 256000;
  ullMaxFileSize = 2097152ui64;
  dwMaxFilesCount = 4;
  dwMaxBodySizeInMemory = 32768;
  ullMaxBodySize = 10485760ui64;
  //----
  RundownProt_Initialize(&nRundownLock);
  nState = StateClosed;
  bKeepConnectionOpen = TRUE;
  bAcceptCompressedContent = TRUE;
  hConn = NULL;
  hLastErrorCode = S_OK;
  sRedirectOrRetryAuth.dwRedirectCounter = 0;
  _InterlockedExchange(&(sRedirectOrRetryAuth.nTimerId), 0);
  sAuthorization.bSeen401 = sAuthorization.bSeen407 = FALSE;
  sAuthorization.nSeen401Or407Counter = 0;
  cHeadersReceivedCallback = NullCallback();
  cDymanicRequestBodyStartCallback = NullCallback();
  cDocumentCompletedCallback = NullCallback();
  cErrorCallback = NullCallback();
  cQueryCertificatesCallback = NullCallback();
  _InterlockedExchange(&nPendingHandlesCounter, 0);
  //----
  sRequest.sPostData.bHasRaw = FALSE;
  sRequest.sPostData.bIsDynamic = FALSE;
  MxMemSet(sRequest.szBoundaryA, 0, sizeof(sRequest.szBoundaryA));
  sRequest.bUsingMultiPartFormData = FALSE;
  sRequest.bUsingProxy = FALSE;
  _InterlockedExchange(&(sResponse.nTimerId), 0);
  sResponse.dwBodyLowThroughputCcounter = 0;
  return;
}

CHttpClient::~CHttpClient()
{
  LONG volatile nWait = 0;
  CPostDataItem *lpItem;

  RundownProt_WaitForRelease(&nRundownLock);
  //close current connection
  {
    CCriticalSection::CAutoLock cLock(cMutex);

    if (hConn != NULL)
    {
      cSocketMgr.Close(hConn, MX_E_Cancelled);
      hConn = NULL;
    }
    nState = StateClosed;
  }
  //clear timeouts
  MX::TimedEvent::Clear(&(sResponse.nTimerId));
  MX::TimedEvent::Clear(&(sRedirectOrRetryAuth.nTimerId));
  //wait until pending handles are closed
  while (__InterlockedRead(&nPendingHandlesCounter) > 0)
    _YieldProcessor();
  //delete request's post data
  while ((lpItem = sRequest.sPostData.cList.PopHead()) != NULL)
    delete lpItem;
  return;
}

VOID CHttpClient::SetOption_ResponseHeaderTimeout(_In_ DWORD dwTimeoutMs)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == StateClosed)
  {
    if (dwTimeoutMs < 5 * 1000)
      dwResponseHeaderTimeoutMs = 5 * 1000;
    else if (dwTimeoutMs > 60 * 60 * 1000)
      dwResponseHeaderTimeoutMs = 60 * 60 * 1000;
    else
      dwResponseHeaderTimeoutMs = dwTimeoutMs;
  }
  return;
}

VOID CHttpClient::SetOption_ResponseBodyLimits(_In_ DWORD dwMinimumThroughputInBps, _In_ DWORD dwSecondsOfLowThroughput)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == StateClosed)
  {
    dwResponseBodyMinimumThroughputInBps = dwMinimumThroughputInBps;
    dwResponseBodySecondsOfLowThroughput = dwSecondsOfLowThroughput;
  }
  return;
}

VOID CHttpClient::SetOption_MaxRedirectionsCount(_In_ DWORD dwCount)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == StateClosed)
  {
    dwMaxRedirCount = dwCount;
    if (dwMaxRedirCount > 1000)
      dwMaxRedirCount = 1000;
  }
  return;
}

VOID CHttpClient::SetOption_MaxHeaderSize(_In_ DWORD dwSize)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == StateClosed)
  {
    sRequest.cHttpCmn.SetOption_MaxHeaderSize(dwSize);
    sResponse.cHttpCmn.SetOption_MaxHeaderSize(dwSize);
  }
  return;
}

VOID CHttpClient::SetOption_MaxFieldSize(_In_ DWORD dwSize)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == StateClosed)
  {
    dwMaxFieldSize = dwSize;
  }
  return;
}

VOID CHttpClient::SetOption_MaxFileSize(_In_ ULONGLONG ullSize)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == StateClosed)
  {
    ullMaxFileSize = ullSize;
  }
  return;
}

VOID CHttpClient::SetOption_MaxFilesCount(_In_ DWORD dwCount)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == StateClosed)
  {
    dwMaxFilesCount = dwCount;
  }
  return;
}

BOOL CHttpClient::SetOption_TemporaryFolder(_In_opt_z_ LPCWSTR szFolderW)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == StateClosed)
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

VOID CHttpClient::SetOption_MaxBodySizeInMemory(_In_ DWORD dwSize)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == StateClosed)
  {
    dwMaxBodySizeInMemory = dwSize;
  }
  return;
}

VOID CHttpClient::SetOption_MaxBodySize(_In_ ULONGLONG ullSize)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == StateClosed)
  {
    ullMaxBodySize = ullSize;
  }
  return;
}

VOID CHttpClient::SetOption_KeepConnectionOpen(_In_ BOOL bKeep)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == StateClosed)
  {
    bKeepConnectionOpen = bKeep;
  }
  return;
}

VOID CHttpClient::SetOption_AcceptCompressedContent(_In_ BOOL bAccept)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == StateClosed)
  {
    bAcceptCompressedContent = bAccept;
  }
  return;
}

VOID CHttpClient::SetHeadersReceivedCallback(_In_ OnHeadersReceivedCallback _cHeadersReceivedCallback)
{
  cHeadersReceivedCallback = _cHeadersReceivedCallback;
  return;
}

VOID CHttpClient::SetDocumentCompletedCallback(_In_ OnDocumentCompletedCallback _cDocumentCompletedCallback)
{
  cDocumentCompletedCallback = _cDocumentCompletedCallback;
  return;
}

VOID CHttpClient::SetDymanicRequestBodyStartCallback(_In_ OnDymanicRequestBodyStartCallback
                                                     _cDymanicRequestBodyStartCallback)
{
  cDymanicRequestBodyStartCallback = _cDymanicRequestBodyStartCallback;
  return;
}

VOID CHttpClient::SetErrorCallback(_In_ OnErrorCallback _cErrorCallback)
{
  cErrorCallback = _cErrorCallback;
  return;
}

VOID CHttpClient::SetQueryCertificatesCallback(_In_ OnQueryCertificatesCallback _cQueryCertificatesCallback)
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

HRESULT CHttpClient::SetRequestMethod(_In_z_ LPCSTR szMethodA)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  if (szMethodA == NULL || *szMethodA == 0)
    return SetRequestMethodAuto();
  if (CHttpCommon::IsValidVerb(szMethodA, StrLenA(szMethodA)) == FALSE)
    return E_INVALIDARG;
  return (sRequest.cStrMethodA.Copy(szMethodA) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

HRESULT CHttpClient::SetRequestMethod(_In_z_ LPCWSTR szMethodW)
{
  CStringA cStrTempA;

  if (szMethodW == NULL || *szMethodW == 0)
    return SetRequestMethodAuto();
  if (cStrTempA.Copy(szMethodW) == FALSE)
    return E_OUTOFMEMORY;
  return SetRequestMethod((LPCSTR)cStrTempA);
}

HRESULT CHttpClient::AddRequestHeader(_In_z_ LPCSTR szNameA, _Out_ CHttpHeaderBase **lplpHeader,
                                      _In_ BOOL bReplaceExisting)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (lplpHeader == NULL)
    return E_POINTER;
  *lplpHeader = NULL;
  if (nState != StateClosed && nState != StateDocumentCompleted)
    return MX_E_NotReady;
  return sRequest.cHttpCmn.AddHeader(szNameA, lplpHeader, bReplaceExisting);
}

HRESULT CHttpClient::AddRequestHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA,
                                      _Out_opt_ CHttpHeaderBase **lplpHeader, _In_ BOOL bReplaceExisting)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateClosed && nState != StateDocumentCompleted)
    return MX_E_NotReady;
  return sRequest.cHttpCmn.AddHeader(szNameA, szValueA, (SIZE_T)-1, lplpHeader, bReplaceExisting);
}

HRESULT CHttpClient::AddRequestHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW,
                                      _Out_opt_ CHttpHeaderBase **lplpHeader, _In_ BOOL bReplaceExisting)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateClosed && nState != StateDocumentCompleted)
    return MX_E_NotReady;
  return sRequest.cHttpCmn.AddHeader(szNameA, szValueW, (SIZE_T)-1, lplpHeader, bReplaceExisting);
}

HRESULT CHttpClient::RemoveRequestHeader(_In_z_ LPCSTR szNameA)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed && nState != StateDocumentCompleted)
    return MX_E_NotReady;
  sRequest.cHttpCmn.RemoveHeader(szNameA);
  return S_OK;
}

HRESULT CHttpClient::RemoveRequestHeader(_In_ CHttpHeaderBase *lpHeader)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed && nState != StateDocumentCompleted)
    return MX_E_NotReady;
  sRequest.cHttpCmn.RemoveHeader(lpHeader);
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

HRESULT CHttpClient::AddRequestCookie(_In_ CHttpCookie &cSrc)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  return sRequest.cHttpCmn.AddCookie(cSrc);
}

HRESULT CHttpClient::AddRequestCookie(_In_ CHttpCookieArray &cSrc)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  return sRequest.cHttpCmn.AddCookie(cSrc);
}

HRESULT CHttpClient::AddRequestCookie(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  return sRequest.cHttpCmn.AddCookie(szNameA, szValueA);
}

HRESULT CHttpClient::AddRequestCookie(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szValueW)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  return sRequest.cHttpCmn.AddCookie(szNameW, szValueW);
}

HRESULT CHttpClient::RemoveRequestCookie(_In_z_ LPCSTR szNameA)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  return sRequest.cHttpCmn.RemoveCookie(szNameA);
}

HRESULT CHttpClient::RemoveRequestCookie(_In_z_ LPCWSTR szNameW)
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

HRESULT CHttpClient::AddRequestPostData(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA)
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
  if (sRequest.sPostData.cList.IsEmpty() == FALSE)
  {
    if (sRequest.sPostData.bHasRaw != FALSE)
      return E_FAIL;
  }
  //create new pair
  cItem.Attach(MX_DEBUG_NEW CPostDataItem(szNameA, ((szValueA != NULL) ? szValueA : "")));
  if (!cItem || cItem->szNameA == NULL)
    return E_OUTOFMEMORY;
  sRequest.sPostData.cList.PushTail(cItem.Detach());
  sRequest.sPostData.bHasRaw = FALSE;
  return S_OK;
}

HRESULT CHttpClient::AddRequestPostData(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szValueW)
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

HRESULT CHttpClient::AddRequestPostDataFile(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szFileNameA, _In_ CStream *lpStream)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  TAutoDeletePtr<CPostDataItem> cItem;
  LPCSTR sA;

  if (nState != StateClosed)
    return MX_E_NotReady;
  if (szNameA == NULL || szFileNameA == NULL || lpStream == NULL)
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
  if (sRequest.sPostData.cList.IsEmpty() == FALSE)
  {
    if (sRequest.sPostData.bHasRaw != FALSE)
      return E_FAIL;
  }
  //create new pair
  cItem.Attach(MX_DEBUG_NEW CPostDataItem(szNameA, sA, lpStream));
  if (!cItem || cItem->szNameA == NULL)
    return E_OUTOFMEMORY;
  sRequest.sPostData.cList.PushTail(cItem.Detach());
  sRequest.sPostData.bHasRaw = FALSE;
  return S_OK;
}

HRESULT CHttpClient::AddRequestPostDataFile(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szFileNameW, _In_ CStream *lpStream)
{
  CStringA cStrTempA[2];
  HRESULT hRes;

  if (szNameW == NULL || szFileNameW == NULL || lpStream == NULL)
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

HRESULT CHttpClient::AddRequestRawPostData(_In_ LPCVOID lpData, _In_ SIZE_T nLength)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  TAutoDeletePtr<CPostDataItem> cItem;
  TAutoRefCounted<CMemoryStream> cStream;
  SIZE_T nWritten;
  HRESULT hRes;

  if (nState != StateClosed && nState != StateSendingDynamicRequestBody)
    return MX_E_NotReady;
  if (lpData == NULL && nLength > 0)
    return E_POINTER;
  
  if (nState == StateClosed)
  {
    if (sRequest.sPostData.cList.IsEmpty() == FALSE)
    {
      if (sRequest.sPostData.bHasRaw == FALSE)
        return E_FAIL;
    }
    //create stream
    cStream.Attach(MX_DEBUG_NEW CMemoryStream());
    if (!cStream)
      return E_OUTOFMEMORY;
    hRes = cStream->Create(nLength);
    if (SUCCEEDED(hRes) && nLength > 0)
    {
      hRes = cStream->Write(lpData, nLength, nWritten);
      if (SUCCEEDED(hRes))
        hRes = cStream->Seek(0);
    }
    if (FAILED(hRes))
      return hRes;
    //create new pair
    cItem.Attach(MX_DEBUG_NEW CPostDataItem(cStream.Get()));
    if (!cItem)
      return E_OUTOFMEMORY;
    sRequest.sPostData.cList.PushTail(cItem.Detach());
    sRequest.sPostData.bHasRaw = TRUE;
  }
  else
  {
    //direct
    hRes = cSocketMgr.SendMsg(hConn, lpData, nLength);
    if (FAILED(hRes))
      return hRes;
  }
  //done
  return S_OK;
}

HRESULT CHttpClient::AddRequestRawPostData(_In_ CStream *lpStream)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  TAutoDeletePtr<CPostDataItem> cItem;

  if (nState != StateClosed && nState != StateSendingDynamicRequestBody)
    return MX_E_NotReady;
  if (lpStream == NULL)
    return E_POINTER;
  if (nState == StateClosed)
  {
    if (sRequest.sPostData.cList.IsEmpty() == FALSE)
    {
      if (sRequest.sPostData.bHasRaw == FALSE)
        return E_FAIL;
    }
    //create new pair
    cItem.Attach(MX_DEBUG_NEW CPostDataItem(lpStream));
    if (!cItem)
      return E_OUTOFMEMORY;
    sRequest.sPostData.cList.PushTail(cItem.Detach());
    sRequest.sPostData.bHasRaw = TRUE;
  }
  else
  {
    HRESULT hRes;

    hRes = cSocketMgr.SendStream(hConn, lpStream);
    if (FAILED(hRes))
      return hRes;
  }
  //done
  return S_OK;
}

HRESULT CHttpClient::EnableDynamicPostData()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  sRequest.sPostData.bIsDynamic = TRUE;
  return S_OK;
}

HRESULT CHttpClient::SignalEndOfPostData()
{
  CCriticalSection::CAutoLock cLock(cMutex);
  HRESULT hRes;

  if (nState != StateSendingDynamicRequestBody)
    return MX_E_InvalidState;

  //send end of body mark
  hRes = cSocketMgr.SendMsg(hConn, "\r\n", 2);
  if (SUCCEEDED(hRes))
  {
    nState = StateReceivingResponseHeaders;

    hRes = cSocketMgr.AfterWriteSignal(hConn, MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnAfterSendRequestBody, this),
                                       NULL);
  }
  if (FAILED(hRes))
    SetErrorOnRequestAndClose(hRes);
  //done
  return hRes;
}

HRESULT CHttpClient::RemoveRequestPostData(_In_opt_z_ LPCSTR szNameA)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  TLnkLst<CPostDataItem>::Iterator it;
  CPostDataItem *lpItem;
  BOOL bGotOne;

  if (nState != StateClosed)
    return MX_E_NotReady;
  bGotOne = FALSE;
  if (szNameA != NULL && *szNameA != 0)
  {
    for (lpItem=it.Begin(sRequest.sPostData.cList); lpItem!=NULL; lpItem=it.Next())
    {
      if (lpItem->szNameA != NULL && StrCompareA(szNameA, lpItem->szNameA) == 0)
      {
        lpItem->RemoveNode();
        delete lpItem;
        bGotOne = TRUE;
      }
    }
  }
  else
  {
    for (lpItem=it.Begin(sRequest.sPostData.cList); lpItem!=NULL; lpItem=it.Next())
    {
      if (lpItem->szNameA == NULL)
      {
        lpItem->RemoveNode();
        delete lpItem;
        bGotOne = TRUE;
      }
    }
  }
  return (bGotOne != FALSE) ? S_OK : MX_E_NotFound;
}

HRESULT CHttpClient::RemoveRequestPostData(_In_opt_z_ LPCWSTR szNameW)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (szNameW == NULL)
    return RemoveRequestPostData((LPCSTR)NULL);
  if (*szNameW == 0)
    return E_INVALIDARG;
  hRes = Utf8_Encode(cStrTempA, szNameW);
  if (FAILED(hRes))
    return hRes;
  return RemoveRequestPostData((LPCSTR)cStrTempA);
}

HRESULT CHttpClient::RemoveAllRequestPostData()
{
  CCriticalSection::CAutoLock cLock(cMutex);
  CPostDataItem *lpItem;

  if (nState != StateClosed)
    return MX_E_NotReady;
  while ((lpItem=sRequest.sPostData.cList.PopHead()) != NULL)
    delete lpItem;
  return S_OK;
}

HRESULT CHttpClient::SetProxy(_In_ CProxy &_cProxy)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  try
  {
    cProxy = _cProxy;
  }
  catch (LONG hr)
  {
    return hr;
  }
  return S_OK;
}

HRESULT CHttpClient::SetAuthCredentials(_In_opt_z_ LPCWSTR szUserNameW, _In_opt_z_ LPCWSTR szPasswordW)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  if (szUserNameW != NULL && *szUserNameW != 0)
  {
    if (cStrAuthUserNameW.Copy(szUserNameW) == FALSE)
      return E_OUTOFMEMORY;
    if (szPasswordW != NULL && *szPasswordW != 0)
    {
      if (cStrAuthUserPasswordW.Copy(szPasswordW) == FALSE)
      {
        cStrAuthUserNameW.Empty();
        return E_OUTOFMEMORY;
      }
    }
  }
  return S_OK;
}

HRESULT CHttpClient::Open(_In_ CUrl &cUrl)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  CCriticalSection::CAutoLock cLock(cMutex);

  if (cAutoRundownProt.IsAcquired() == FALSE)
    return MX_E_NotReady;
  sRedirectOrRetryAuth.dwRedirectCounter = 0;
  sAuthorization.bSeen401 = sAuthorization.bSeen407 = FALSE;
  sAuthorization.nSeen401Or407Counter = 0;
  return InternalOpen(cUrl);
}

HRESULT CHttpClient::Open(_In_z_ LPCSTR szUrlA)
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

HRESULT CHttpClient::Open(_In_z_ LPCWSTR szUrlW)
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

VOID CHttpClient::Close(_In_opt_ BOOL bReuseConn)
{
  {
    CCriticalSection::CAutoLock cLock(cMutex);

    if (hConn != NULL && (nState != StateDocumentCompleted || bReuseConn == FALSE))
    {
      cSocketMgr.Close(hConn, S_OK);
      hConn = NULL;
    }
    ResetRequestForNewRequest();
    ResetResponseForNewRequest();
    nState = StateClosed;
  }
  //clear timeouts
  MX::TimedEvent::Clear(&(sResponse.nTimerId));
  MX::TimedEvent::Clear(&(sRedirectOrRetryAuth.nTimerId));
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

HRESULT CHttpClient::GetResponseReason(_Inout_ CStringA &cStrDestA)
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

CHttpHeaderBase* CHttpClient::GetResponseHeader(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return NULL;
  return sResponse.cHttpCmn.GetHeader(nIndex);
}

CHttpHeaderBase* CHttpClient::GetResponseHeader(_In_z_ LPCSTR szNameA) const
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

HRESULT CHttpClient::GetResponseCookie(_In_ SIZE_T nIndex, _Out_ CHttpCookie &cCookie)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  CHttpCookieArray *lpCookieArray;

  cCookie.Clear();
  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return MX_E_NotReady;
  lpCookieArray = sResponse.cHttpCmn.GetCookies();
  if (nIndex >= lpCookieArray->GetCount())
    return E_INVALIDARG;
  try
  {
    cCookie = *(lpCookieArray->GetElementAt(nIndex));
  }
  catch (LONG hr)
  {
    return hr;
  }
  return S_OK;
}

HRESULT CHttpClient::GetResponseCookies(_Out_ CHttpCookieArray &cCookieArray)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
  {
    cCookieArray.RemoveAllElements();
    return MX_E_NotReady;
  }
  try
  {
    cCookieArray = *(sResponse.cHttpCmn.GetCookies());
  }
  catch (LONG hr)
  {
    return hr;
  }
  return S_OK;
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

LPCSTR CHttpClient::GetRequestBoundary() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  return sRequest.szBoundaryA;
}

HRESULT CHttpClient::InternalOpen(_In_ CUrl &cUrl)
{
  CStringA cStrHostA;
  LPCWSTR szConnectHostW;
  int nUrlPort, nConnectPort;
  eState nOrigState;
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

  if (cProxy.GetType() != CProxy::TypeNone)
  {
    hRes = cProxy.Resolve(cUrl);
    if (hRes == E_OUTOFMEMORY)
      return hRes;
  }

  //clear timeouts
  nOrigState = nState;
  nState = StateNoOp;
  cMutex.Unlock();
  MX::TimedEvent::Clear(&(sResponse.nTimerId));
  MX::TimedEvent::Clear(&(sRedirectOrRetryAuth.nTimerId));
  cMutex.Lock();
  nState = nOrigState;

  ResetResponseForNewRequest();

  //check where we should connect
  szConnectHostW = NULL;
  nConnectPort = 0;
  if (cProxy.GetType() != CProxy::TypeNone)
  {
    szConnectHostW = cProxy.GetAddress();
    nConnectPort = cProxy.GetPort();
  }
  if (szConnectHostW == NULL || *szConnectHostW == 0)
  {
    szConnectHostW = cUrl.GetHost();
    nConnectPort = nUrlPort;
    sRequest.bUsingProxy = FALSE;
  }
  else
  {
    sRequest.bUsingProxy = TRUE;
  }
  //can reuse connection?
  if (hConn != NULL)
  {
    if (sResponse.cHttpCmn.GetParserState() != CHttpCommon::StateDone ||
        StrCompareW(sRequest.cUrl.GetScheme(), cUrl.GetScheme()) != 0 ||
        StrCompareW(sRequest.cUrl.GetHost(), szConnectHostW) != 0 ||
        nUrlPort != nConnectPort)
    {
      cSocketMgr.Close(hConn, S_OK);
      hConn = NULL;
    }
  }
  //setup new state
  nState = (cUrl.GetSchemeCode() == CUrl::SchemeHttps &&
            sRequest.bUsingProxy != FALSE) ? StateEstablishingProxyTunnelConnection : StateSendingRequestHeaders;
  //create a new connection if needed
  hRes = S_OK;
  if (hConn == NULL)
  {
    try
    {
      sRequest.cUrl = cUrl;
    }
    catch (LONG hr)
    {
      hRes = hr;
    }
    if (SUCCEEDED(hRes))
    {
      GenerateRequestBoundary();

      hRes = cSocketMgr.ConnectToServer(CSockets::FamilyIPv4, szConnectHostW, nConnectPort,
                                        MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnSocketCreate, this), NULL, &hConn);
    }
  }
  else
  {
    GenerateRequestBoundary();

    hRes = OnSocketConnect(&cSocketMgr, hConn, NULL, 0x12345678); //special error code value
  }
  if (FAILED(hRes))
  {
    nState = StateClosed;
    return hRes;
  }
  //done
  return S_OK;
}

HRESULT CHttpClient::OnSocketCreate(_In_ CIpc *lpIpc, _In_ HANDLE h, _Inout_ CIpc::CREATE_CALLBACK_DATA &sData)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);

  if (cAutoRundownProt.IsAcquired() == FALSE)
    return MX_E_NotReady;
  //setup socket
  sData.cConnectCallback = MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnSocketConnect, this);
  sData.cDataReceivedCallback = MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnSocketDataReceived, this);
  sData.cDestroyCallback = MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnSocketDestroy, this);
  //setup SSL layer
  if (sRequest.cUrl.GetSchemeCode() == CUrl::SchemeHttps && sRequest.bUsingProxy == FALSE)
  {
    HRESULT hRes = AddSslLayer(lpIpc, h);
    if (FAILED(hRes))
      return hRes;
  }
  //done
  _InterlockedIncrement(&nPendingHandlesCounter);
  return S_OK;
}

VOID CHttpClient::OnSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                                  _In_ HRESULT hrErrorCode)
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
        case StateRetryingAuthentication:
          cSocketMgr.Close(hConn, S_OK);
          hConn = NULL;
          break;

        case StateEstablishingProxyTunnelConnection:
        case StateWaitingProxyTunnelConnectionResponse:
        case StateSendingRequestHeaders:
        case StateSendingDynamicRequestBody:
        case StateReceivingResponseHeaders:
          if (SUCCEEDED(hrErrorCode))
            hrErrorCode = MX_E_Cancelled;
          SetErrorOnRequestAndClose(hrErrorCode);
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
    cErrorCallback(this, hrErrorCode);
  //done
  _InterlockedDecrement(&nPendingHandlesCounter);
  return;
}

HRESULT CHttpClient::OnSocketConnect(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_opt_ CIpc::CUserData *lpUserData,
                                     _In_ HRESULT hrErrorCode)
{
  HRESULT hRes;

  {
    CCriticalSection::CAutoLock cLock(cMutex);

    //does belong tu us?
    if (h != hConn)
      return MX_E_Cancelled;
    if (SUCCEEDED(hrErrorCode))
    {
      hRes = S_OK;
      switch (nState)
      {
        case StateEstablishingProxyTunnelConnection:
          hRes = SendTunnelConnect();
          break;

        case StateSendingRequestHeaders:
          hRes = SendRequestHeaders();
          break;
      }
    }
    else
    {
      hRes = hrErrorCode;
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
HRESULT CHttpClient::OnSocketDataReceived(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  BYTE aMsgBuf[4096];
  SIZE_T nMsgSize, nMsgUsed;
  BOOL bCheckConnection, bFireResponseHeadersReceivedCallback, bFireDocumentCompleted, nHandleResponseHeadersTimer;
  HRESULT hRes;

  if (cAutoRundownProt.IsAcquired() == FALSE)
  {
    return MX_E_Cancelled;
  }

  nMsgSize = nMsgUsed = 0;
  bCheckConnection = TRUE;
restart:
  bFireResponseHeadersReceivedCallback = bFireDocumentCompleted = FALSE;
  nHandleResponseHeadersTimer = 0;
  {
    CCriticalSection::CAutoLock cLock(cMutex);

    //does belong to us?
    if (bCheckConnection != FALSE && h != hConn)
      return MX_E_Cancelled;
    bCheckConnection = TRUE;
    //re-add timeout
    hRes = S_OK;
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
      if (nState == StateWaitingProxyTunnelConnectionResponse)
      {
        CHttpCommon::eState nParserState;
        SIZE_T nNewUsed;

        //process http being received
        hRes = sResponse.cHttpCmn.Parse(aMsgBuf + nMsgUsed, nMsgSize - nMsgUsed, nNewUsed);
        if ((nMsgUsed += nNewUsed) >= nMsgSize)
          nMsgSize = 0; //mark end of message
        if (SUCCEEDED(hRes))
        {
          switch (sResponse.cHttpCmn.GetErrorCode())
          {
            case 0:
              break;
            case 413:
              hRes = MX_E_BadLength;
              break;
            default:
              hRes = MX_E_InvalidData;
              break;
          }
        }
        if (FAILED(hRes))
        {
          SetErrorOnRequestAndClose(hRes);
          break;
        }
        //take action if parser's state changed
        nParserState = sResponse.cHttpCmn.GetParserState();

        //check for end
        if (nParserState == CHttpCommon::StateBodyStart || nParserState == CHttpCommon::StateDone)
        {
          LONG nRespStatus = sResponse.cHttpCmn.GetResponseStatus();

          //reset parser
          sResponse.cHttpCmn.ResetParser();
          //can proceed?
          if (nRespStatus == 200)
          {
            //add ssl layer
            hRes = AddSslLayer(lpIpc, h);
            if (SUCCEEDED(hRes))
            {
              nState = StateSendingRequestHeaders;
              hRes = SendRequestHeaders();
            }
          }
          else
          {
            hRes = MX_HRESULT_FROM_WIN32(WSAEREFUSED);
          }
          if (FAILED(hRes))
          {
            SetErrorOnRequestAndClose(hRes);
            break;
          }
        }
      }
      else if (nState == StateReceivingResponseHeaders || nState == StateReceivingResponseBody)
      {
        CHttpCommon::eState nParserState;
        SIZE_T nNewUsed;

        //process http being received
        hRes = sResponse.cHttpCmn.Parse(aMsgBuf+nMsgUsed, nMsgSize-nMsgUsed, nNewUsed);
        if ((nMsgUsed += nNewUsed) >= nMsgSize)
          nMsgSize = 0; //mark end of message
        if (SUCCEEDED(hRes))
        {
          switch (sResponse.cHttpCmn.GetErrorCode())
          {
            case 0:
              break;
            case 413:
              hRes = MX_E_BadLength;
              break;
            default:
              hRes = MX_E_InvalidData;
              break;
          }
        }
        if (FAILED(hRes))
        {
          SetErrorOnRequestAndClose(hRes);
          break;
        }
        //take action if parser's state changed
        nParserState = sResponse.cHttpCmn.GetParserState();
        //check if we hit a redirection or a site/proxy authentication
        if (nParserState == CHttpCommon::StateBodyStart)
        {
          LONG nRespStatus = sResponse.cHttpCmn.GetResponseStatus();
          CHttpBodyParserBase *lpParser;

          //cancel response header timeout and start body one
          nHandleResponseHeadersTimer = 1;

          if ((nRespStatus == _HTTP_STATUS_MovedPermanently || nRespStatus == _HTTP_STATUS_MovedTemporarily ||
               nRespStatus == _HTTP_STATUS_SeeOther || nRespStatus == _HTTP_STATUS_UseProxy ||
               nRespStatus == _HTTP_STATUS_TemporaryRedirect || nRespStatus == _HTTP_STATUS_RequestTimeout) &&
              sRedirectOrRetryAuth.dwRedirectCounter < dwMaxRedirCount)
          {
handle_redirect_or_auth_ignore_body:
            lpParser = MX_DEBUG_NEW CHttpBodyParserIgnore();
            if (lpParser == NULL)
            {
              SetErrorOnRequestAndClose(E_OUTOFMEMORY);
              break;
            }
            hRes = sResponse.cHttpCmn.SetBodyParser(lpParser);
            lpParser->Release();
            if (FAILED(hRes))
            {
              SetErrorOnRequestAndClose(hRes);
              break;
            }
          }
          else if (nRespStatus == _HTTP_STATUS_Unauthorized && cStrAuthUserNameW.IsEmpty() == FALSE &&
                   sAuthorization.nSeen401Or407Counter < DEFAULT_MAX_AUTHORIZATION_COUNT)
          {
            TAutoRefCounted<CHttpAuthBase> cHttpAuth;
            CHttpHeaderRespWwwAuthenticate *lpHeader;

            if (sAuthorization.bSeen401 == FALSE)
              goto handle_redirect_or_auth_ignore_body;

            lpHeader = sResponse.cHttpCmn.GetHeader<CHttpHeaderRespWwwAuthenticate>();
            if (lpHeader != NULL)
            {
              cHttpAuth = lpHeader->GetHttpAuth();
              if (cHttpAuth && cHttpAuth->IsReauthenticateRequst() != FALSE)
                goto handle_redirect_or_auth_ignore_body;
            }

            goto handle_redirect_or_auth_processbody;
          }
          else if (nRespStatus == _HTTP_STATUS_ProxyAuthenticationRequired && *(cProxy._GetUserName()) != 0 &&
                   sAuthorization.nSeen401Or407Counter < DEFAULT_MAX_AUTHORIZATION_COUNT)
          {
            TAutoRefCounted<CHttpAuthBase> cHttpAuth;
            CHttpHeaderRespProxyAuthenticate *lpHeader;

            if (sAuthorization.bSeen407 == FALSE)
              goto handle_redirect_or_auth_ignore_body;

            lpHeader = sResponse.cHttpCmn.GetHeader<CHttpHeaderRespProxyAuthenticate>();
            if (lpHeader != NULL)
            {
              cHttpAuth = lpHeader->GetHttpAuth();
              if (cHttpAuth && cHttpAuth->IsReauthenticateRequst() != FALSE)
                goto handle_redirect_or_auth_ignore_body;
            }

            goto handle_redirect_or_auth_processbody;
          }
          else
          {
handle_redirect_or_auth_processbody:
            bFireResponseHeadersReceivedCallback = TRUE;
          }
          nState = StateReceivingResponseBody;
        }
        else if (nParserState == CHttpCommon::StateDone)
        {
          LONG nRespStatus = sResponse.cHttpCmn.GetResponseStatus();

          //cancel response header timeout
          nHandleResponseHeadersTimer = 2;

          if ((nRespStatus == _HTTP_STATUS_MovedPermanently || nRespStatus == _HTTP_STATUS_MovedTemporarily ||
               nRespStatus == _HTTP_STATUS_SeeOther || nRespStatus == _HTTP_STATUS_UseProxy ||
               nRespStatus == _HTTP_STATUS_TemporaryRedirect || nRespStatus == _HTTP_STATUS_RequestTimeout) &&
               sRedirectOrRetryAuth.dwRedirectCounter < dwMaxRedirCount)
          {
            CUrl cUrlTemp;
            ULONGLONG nWaitTimeSecs;

            (sRedirectOrRetryAuth.dwRedirectCounter)++;
            nWaitTimeSecs = 0ui64;
            //build new url
            hRes = S_OK;
            try
            {
              sRedirectOrRetryAuth.cUrl = sRequest.cUrl;
            }
            catch (LONG hr)
            {
              hRes = hr;
            }
            if (SUCCEEDED(hRes))
            {
              if (sResponse.cHttpCmn.GetResponseStatus() != _HTTP_STATUS_RequestTimeout)
              {
                CHttpHeaderRespLocation *lpHeader = sResponse.cHttpCmn.GetHeader<CHttpHeaderRespLocation>();
                if (lpHeader != NULL)
                {
                  hRes = cUrlTemp.ParseFromString(lpHeader->GetLocation());
                  if (SUCCEEDED(hRes))
                    hRes = sRedirectOrRetryAuth.cUrl.Merge(cUrlTemp);
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
            if (SUCCEEDED(hRes))
            {
              nState = StateWaitingForRedirection;
              hRes = SetupRedirectOrRetry((DWORD)(nWaitTimeSecs * 1000ui64));
            }
            if (FAILED(hRes))
              SetErrorOnRequestAndClose(hRes);
          }
          else if (((nRespStatus == _HTTP_STATUS_Unauthorized && cStrAuthUserNameW.IsEmpty() == FALSE) ||
                    (nRespStatus == _HTTP_STATUS_ProxyAuthenticationRequired && *(cProxy._GetUserName()) != 0)) &&
                   sAuthorization.nSeen401Or407Counter < DEFAULT_MAX_AUTHORIZATION_COUNT)
          {
            TAutoRefCounted<CHttpAuthBase> cHttpAuth;

            (sAuthorization.nSeen401Or407Counter)++;

            hRes = S_OK;
            if (nRespStatus == _HTTP_STATUS_Unauthorized)
            {
              CHttpHeaderRespWwwAuthenticate *lpHeader;

              //reset seen 407 so the proxy does not complain on next round if credentials fails for any reason
              sAuthorization.bSeen407 = FALSE;

              lpHeader = sResponse.cHttpCmn.GetHeader<CHttpHeaderRespWwwAuthenticate>();
              if (lpHeader != NULL)
                cHttpAuth = lpHeader->GetHttpAuth();

              if (sAuthorization.bSeen401 == FALSE ||
                  (cHttpAuth && cHttpAuth->IsReauthenticateRequst() != FALSE))
              {
                sAuthorization.bSeen401 = TRUE;
              }
              else
              {
                goto handle_redirect_or_auth_processdocument;
              }
            }
            else //nRespStatus == _HTTP_STATUS_ProxyAuthenticationRequired
            {
              CHttpHeaderRespProxyAuthenticate *lpHeader;

              lpHeader = sResponse.cHttpCmn.GetHeader<CHttpHeaderRespProxyAuthenticate>();
              if (lpHeader != NULL)
                cHttpAuth = lpHeader->GetHttpAuth();

              if (sAuthorization.bSeen407 == FALSE ||
                  (cHttpAuth && cHttpAuth->IsReauthenticateRequst() != FALSE))
              {
                sAuthorization.bSeen407 = TRUE;
              }
              else
              {
                goto handle_redirect_or_auth_processdocument;
              }
            }

            if (cHttpAuth)
            {
              hRes = HttpAuthCache::Add(sRequest.cUrl, cHttpAuth.Get());
              if (SUCCEEDED(hRes))
              {
                nState = StateRetryingAuthentication;
                hRes = SetupRedirectOrRetry(0);
              }
            }
            else
            {
              hRes = MX_E_InvalidData;
            }

            if (FAILED(hRes))
              SetErrorOnRequestAndClose(hRes);
          }
          else
          {
handle_redirect_or_auth_processdocument:
            //no redirection or (re)auth
            bFireDocumentCompleted = TRUE;
            if (nState == StateReceivingResponseHeaders)
              bFireResponseHeadersReceivedCallback = TRUE;
            nState = StateDocumentCompleted;

            //on completion, keep the file
            if (sResponse.cStrDownloadFileNameW.IsEmpty() == FALSE)
            {
              CHttpBodyParserBase *lpBodyParser = sResponse.cHttpCmn.GetBodyParser();
              if (lpBodyParser != NULL)
              {
                if (MX::StrCompareA(lpBodyParser->GetType(), "default") == 0)
                {
                  ((CHttpBodyParserDefault*)lpBodyParser)->KeepFile();
                }
                lpBodyParser->Release();
              }
            }
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

  //should I cancel a request?
  switch (nHandleResponseHeadersTimer)
  {
    case 1:
      MX::TimedEvent::Clear(&(sResponse.nTimerId));
      break;

    case 2:
      MX::TimedEvent::Clear(&(sResponse.nTimerId));
      if (SUCCEEDED(hRes))
      {
        hRes = MX::TimedEvent::SetInterval(&(sResponse.nTimerId), 1000,
                                           MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnResponseBodyTimeout, this), NULL);
      }
      break;
  }

  //have some action to do?
  if (SUCCEEDED(hRes) && bFireResponseHeadersReceivedCallback != FALSE)
  {
    CHttpBodyParserBase *lpBodyParser = NULL;
    CStringW cStrFullFileNameW;

    if (ShouldLog(1) != FALSE)
    {
      Log(L"HttpClient(ResponseHeadersReceived/0x%p)", this);
    }
    if (cHeadersReceivedCallback)
    {
      CHttpHeaderEntContentType *lpContentTypeHeader;
      CHttpHeaderEntContentLength *lpContentLengthHeader;
      CHttpHeaderEntContentDisposition *lpContentDispHeader;
      LPCSTR szTypeA;
      ULONGLONG nContentLength;
      LPCWSTR sW[2], szFileNameW;
      BOOL bTreatAsAttachment;
      HRESULT hRes;

      lpContentTypeHeader = sResponse.cHttpCmn.GetHeader<CHttpHeaderEntContentType>();
      lpContentLengthHeader = sResponse.cHttpCmn.GetHeader<CHttpHeaderEntContentLength>();
      lpContentDispHeader = sResponse.cHttpCmn.GetHeader<CHttpHeaderEntContentDisposition>();

      szFileNameW = L"";
      if (lpContentDispHeader != NULL)
      {
        szFileNameW = lpContentDispHeader->GetFileName();
        if ((sW[0] = MX::StrChrW(szFileNameW, L'/', TRUE)) == NULL)
          sW[0] = szFileNameW;
        if ((sW[1] = MX::StrChrW(szFileNameW, L'\\', TRUE)) == NULL)
          sW[1] = szFileNameW;
        szFileNameW = (sW[1] > sW[0]) ? (sW[1] + 1) : (sW[0] + 1);
      }
      if (*szFileNameW == 0)
      {
        szFileNameW = sRequest.cUrl.GetPath();
        if ((sW[0] = MX::StrChrW(szFileNameW, L'/', TRUE)) == NULL)
          sW[0] = szFileNameW;
        if ((sW[1] = MX::StrChrW(szFileNameW, L'\\', TRUE)) == NULL)
          sW[1] = szFileNameW;
        szFileNameW = (sW[1] > sW[0]) ? (sW[1] + 1) : (sW[0] + 1);
      }
      if (*szFileNameW == 0)
      {
        szFileNameW = L"index";
      }

      nContentLength = (lpContentLengthHeader != NULL) ? lpContentLengthHeader->GetLength() : 0ui64;

      szTypeA = (lpContentTypeHeader != NULL) ? lpContentTypeHeader->GetType() : "";
      if (*szTypeA == 0)
        szTypeA = "application/octet-stream";

      if (lpContentDispHeader != NULL && MX::StrCompareA(lpContentDispHeader->GetType(), "attachment", TRUE) == 0)
        bTreatAsAttachment = TRUE;
      else
        bTreatAsAttachment = FALSE;

      hRes = cHeadersReceivedCallback(this, szFileNameW, ((lpContentLengthHeader != NULL) ? &nContentLength : NULL),
                                      szTypeA, bTreatAsAttachment, sResponse.cStrDownloadFileNameW, &lpBodyParser);
      if (FAILED(hRes))
        return hRes;
    }
    if (SUCCEEDED(hRes))
    {
      CCriticalSection::CAutoLock cLock(cMutex);
      TAutoRefCounted<CHttpBodyParserBase> cBodyParser;

      cBodyParser.Attach(lpBodyParser);
      if (h == hConn)
      {
        if (!cBodyParser)
        {
          cBodyParser.Attach(MX_DEBUG_NEW CHttpBodyParserDefault(
                             MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnDownloadStarted, this), NULL,
                             ((sResponse.cStrDownloadFileNameW.IsEmpty() == FALSE) ? 0 : dwMaxBodySizeInMemory),
                             ((sResponse.cStrDownloadFileNameW.IsEmpty() == FALSE) ? ULONGLONG_MAX : ullMaxBodySize)));
          if (!cBodyParser)
            hRes = E_OUTOFMEMORY;
        }
        if (SUCCEEDED(hRes))
          hRes = sResponse.cHttpCmn.SetBodyParser(cBodyParser.Get());
      }
    }
  }
  if (SUCCEEDED(hRes) && bFireDocumentCompleted != FALSE)
  {
    if (ShouldLog(1) != FALSE)
    {
      Log(L"HttpClient(DocumentCompleted/0x%p)", this);
    }
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

VOID CHttpClient::OnRedirectOrRetryAuth(_In_ LONG nTimerId, _In_ LPVOID lpUserData)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);

  if (cAutoRundownProt.IsAcquired() != FALSE)
  {
    HRESULT hRes;

    hRes = S_OK;
    {
      CCriticalSection::CAutoLock cLock(cMutex);

      if (_InterlockedCompareExchange(&(sRedirectOrRetryAuth.nTimerId), 0, nTimerId) == nTimerId)
      {
        switch (nState)
        {
          case StateWaitingForRedirection:
            hRes = InternalOpen(sRedirectOrRetryAuth.cUrl);
            break;

          case StateRetryingAuthentication:
            hRes = InternalOpen(sRequest.cUrl);
            break;

          default:
            hRes = MX_E_InvalidState;
            break;
        }
      }
    }
    //raise error event if any
    if (FAILED(hRes) && cErrorCallback)
      cErrorCallback(this, hRes);
  }
  return;
}

VOID CHttpClient::OnResponseHeadersTimeout(_In_ LONG nTimerId, _In_ LPVOID lpUserData)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  HRESULT hRes;

  if (cAutoRundownProt.IsAcquired() != FALSE)
    return;

  hRes = S_OK;

  {
    CCriticalSection::CAutoLock cLock(cMutex);

    if (_InterlockedCompareExchange(&(sResponse.nTimerId), 0, nTimerId) == nTimerId)
    {
      if (nState == StateReceivingResponseHeaders || nState == StateReceivingResponseBody ||
          nState == StateWaitingProxyTunnelConnectionResponse)
      {
        SetErrorOnRequestAndClose(hRes = MX_E_Timeout);
      }
    }
  }
  //raise error event if any
  if (FAILED(hRes) && cErrorCallback)
    cErrorCallback(this, hRes);
  return;
}

VOID CHttpClient::OnResponseBodyTimeout(_In_ LONG nTimerId, _In_ LPVOID lpUserData)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);

  if (cAutoRundownProt.IsAcquired() != FALSE)
  {
    HRESULT hRes = S_OK;

    if (__InterlockedRead(&(sResponse.nTimerId)) == nTimerId)
    {
      {
        CCriticalSection::CAutoLock cLock(cMutex);

        if (nState == StateReceivingResponseBody)
        {
          float nReadKbps;

          if (SUCCEEDED(cSocketMgr.GetReadStats(hConn, NULL, &nReadKbps)))
          {
            if ((DWORD)(int)nReadKbps < dwResponseBodySecondsOfLowThroughput)
            {
              if ((++(sResponse.dwBodyLowThroughputCcounter)) >= dwResponseBodySecondsOfLowThroughput)
              {
                SetErrorOnRequestAndClose(hRes = MX_E_Timeout);
              }
            }
            else
            {
              sResponse.dwBodyLowThroughputCcounter = 0;
            }
          }
        }
      }
    }
    //raise error event if any
    if (FAILED(hRes) && cErrorCallback)
      cErrorCallback(this, hRes);
  }
  return;
}

VOID CHttpClient::OnAfterSendRequestHeaders(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ LPVOID lpCookie,
                                            _In_ CIpc::CUserData *lpUserData)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  BOOL bFireDymanicRequestBodyStartCallback = FALSE;
  HRESULT hRes = S_OK;

  if (cAutoRundownProt.IsAcquired() == FALSE)
    return;

  {
    CCriticalSection::CAutoLock cLock(cMutex);

    if (h == hConn && nState == StateSendingRequestHeaders)
    {
      if (StrCompareA((LPCSTR)(sRequest.cStrMethodA), "HEAD") != 0 &&
          sRequest.sPostData.cList.IsEmpty() == FALSE && sRequest.sPostData.bIsDynamic == FALSE)
      {
        hRes = SendRequestBody();
        if (SUCCEEDED(hRes))
        {
          nState = StateReceivingResponseHeaders;

          hRes = cSocketMgr.AfterWriteSignal(hConn, MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnAfterSendRequestBody, this),
                                             NULL);
        }
      }
      else if (sRequest.sPostData.bIsDynamic == FALSE)
      {
        //no body to send, directly switch to receiving response headers
        nState = StateReceivingResponseHeaders;

        //set response headers timeout
        cMutex.Unlock();
        hRes = SetupResponseHeadersTimeout();
        cMutex.Lock();
      }
      else
      {
        //sending dynamic body
        nState = StateSendingDynamicRequestBody;

        bFireDymanicRequestBodyStartCallback = TRUE;
      }
    }
    if (FAILED(hRes))
      SetErrorOnRequestAndClose(hRes);
  }
  if (SUCCEEDED(hRes) && bFireDymanicRequestBodyStartCallback != FALSE && cDymanicRequestBodyStartCallback)
    cDymanicRequestBodyStartCallback(this);

  //raise error event if any
  if (FAILED(hRes) && cErrorCallback)
    cErrorCallback(this, hRes);
  return;
}

VOID CHttpClient::OnAfterSendRequestBody(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ LPVOID lpCookie,
                                         _In_ CIpc::CUserData *lpUserData)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  BOOL bSetTimeouts;

  if (cAutoRundownProt.IsAcquired() == FALSE)
    return;

  {
    CCriticalSection::CAutoLock cLock(cMutex);

    bSetTimeouts = (h == hConn && nState == StateReceivingResponseHeaders) ? TRUE : FALSE;
  }
  //set response headers timeout
  if (bSetTimeouts != FALSE)
  {
    HRESULT hRes;

    hRes = SetupResponseHeadersTimeout();
    if (FAILED(hRes))
    {
      {
        CCriticalSection::CAutoLock cLock(cMutex);

        SetErrorOnRequestAndClose(hRes);
      }

      //raise error event if any
      if (cErrorCallback)
        cErrorCallback(this, hRes);
    }
  }
  return;
}

VOID CHttpClient::SetErrorOnRequestAndClose(_In_ HRESULT hrErrorCode)
{
  nState = StateClosed;
  if (SUCCEEDED(hLastErrorCode)) //preserve first error
    hLastErrorCode = hrErrorCode;
  cSocketMgr.Close(hConn, hLastErrorCode);
  hConn = NULL;
  return;
}

HRESULT CHttpClient::SetupRedirectOrRetry(_In_ DWORD dwDelayMs)
{
  MX::TimedEvent::Clear(&(sRedirectOrRetryAuth.nTimerId));
  return MX::TimedEvent::SetTimeout(&(sRedirectOrRetryAuth.nTimerId), dwDelayMs,
                                    MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnRedirectOrRetryAuth, this), NULL);
}

HRESULT CHttpClient::SetupResponseHeadersTimeout()
{
  MX::TimedEvent::Clear(&(sResponse.nTimerId));
  return MX::TimedEvent::SetTimeout(&(sResponse.nTimerId), dwResponseHeaderTimeoutMs,
                                    MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnResponseHeadersTimeout, this), NULL);
}

HRESULT CHttpClient::AddSslLayer(_In_ CIpc *lpIpc, _In_ HANDLE h)
{
  CSslCertificateArray *lpCheckCertificates;
  CSslCertificate *lpSelfCert;
  CEncryptionKey *lpPrivKey;
  CStringA cStrHostNameA;
  TAutoDeletePtr<CIpcSslLayer> cLayer;
  HRESULT hRes;

  lpCheckCertificates = NULL;
  lpSelfCert = NULL;
  lpPrivKey = NULL;
  //query for client certificates
  if (!cQueryCertificatesCallback)
    return MX_E_NotReady;
  hRes = cQueryCertificatesCallback(this, &lpCheckCertificates, &lpSelfCert, &lpPrivKey);
  if (FAILED(hRes))
    return hRes;
  //get host name
  hRes = sRequest.cUrl.GetHost(cStrHostNameA);
  if (FAILED(hRes))
    return hRes;
  //add ssl layer
  cLayer.Attach(MX_DEBUG_NEW CIpcSslLayer(lpIpc));
  if (!cLayer)
    return E_OUTOFMEMORY;
  hRes = cLayer->Initialize(FALSE, (LPCSTR)cStrHostNameA, lpCheckCertificates, lpSelfCert, lpPrivKey);
  if (SUCCEEDED(hRes))
  {
    hRes = lpIpc->AddLayer(h, cLayer);
    if (SUCCEEDED(hRes))
      cLayer.Detach();
  }
  return hRes;
}

HRESULT CHttpClient::BuildRequestHeaders(_Inout_ CStringA &cStrReqHdrsA)
{
  CHttpHeaderBase *lpHeader;
  CStringA cStrTempA;
  CHttpCookie *lpCookie;
  SIZE_T nHdrIndex_Accept, nHdrIndex_AcceptLanguage, nHdrIndex_Referer, nHdrIndex_UserAgent, nHdrIndex_WWWAuthenticate;
  SIZE_T nIndex;
  CHttpHeaderBase::eBrowser nBrowser = CHttpHeaderBase::BrowserOther;
  HRESULT hRes;

  if (cStrReqHdrsA.EnsureBuffer(32768) == FALSE)
    return E_OUTOFMEMORY;

  nHdrIndex_Accept = (SIZE_T)-1;
  nHdrIndex_AcceptLanguage = (SIZE_T)-1;
  nHdrIndex_Referer = (SIZE_T)-1;
  nHdrIndex_UserAgent = (SIZE_T)-1;
  nHdrIndex_WWWAuthenticate = (SIZE_T)-1;
  for (nIndex = 0; (lpHeader = sRequest.cHttpCmn.GetHeader(nIndex)) != NULL; nIndex++)
  {
    LPCSTR sA = lpHeader->GetHeaderName();

    switch (*sA)
    {
      case 'A':
      case 'a':
        if (StrCompareA(sA, "Accept", TRUE) == 0)
          nHdrIndex_Accept = nIndex;
        else if (StrCompareA(sA, "Accept-Language", TRUE) == 0)
          nHdrIndex_AcceptLanguage = nIndex;
        break;

      case 'R':
      case 'r':
        if (StrCompareA(sA, "Referer", TRUE) == 0)
          nHdrIndex_Referer = nIndex;
        break;

      case 'U':
      case 'u':
        if (StrCompareA(sA, "User-Agent", TRUE) == 0)
        {
          nHdrIndex_UserAgent = nIndex;
          nBrowser = CHttpHeaderBase::GetBrowserFromUserAgent(reinterpret_cast<CHttpHeaderGeneric*>
                                                              (lpHeader)->GetValue());
        }
        break;

      case 'W':
      case 'w':
        if (StrCompareA(sA, "WWW-Authenticate", TRUE) == 0)
          nHdrIndex_WWWAuthenticate = nIndex;
        break;
    }
  }
  //1) GET/POST/{methood} url HTTP/1.1
  if (sRequest.cStrMethodA.IsEmpty() != FALSE)
  {
    if (cStrReqHdrsA.Copy((sRequest.sPostData.cList.IsEmpty() == FALSE ||
                           sRequest.sPostData.bIsDynamic != FALSE) ? "POST " : "GET ") == FALSE)
    {
      return E_OUTOFMEMORY;
    }
  }
  else
  {
    if (cStrReqHdrsA.Copy((LPSTR)(sRequest.cStrMethodA)) == FALSE ||
        cStrReqHdrsA.ConcatN(" ", 1) == FALSE)
    {
      return E_OUTOFMEMORY;
    }
  }
  if (sRequest.bUsingProxy != FALSE && sRequest.cUrl.GetSchemeCode() != CUrl::SchemeHttps)
    hRes = sRequest.cUrl.ToString(cStrTempA, CUrl::ToStringAddAll);
  else
    hRes = sRequest.cUrl.ToString(cStrTempA, CUrl::ToStringAddPath | CUrl::ToStringAddQueryStrings);
  if (FAILED(hRes))
    return hRes;
  if (cStrReqHdrsA.Concat((LPSTR)cStrTempA) == FALSE ||
      cStrReqHdrsA.Concat(" HTTP/1.1\r\n") == FALSE)
  {
    return E_OUTOFMEMORY;
  }
  //2) Accept: */*
  if (nHdrIndex_Accept == (SIZE_T)-1)
  {
    hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "Accept", "*/*", nBrowser);
    if (FAILED(hRes))
      return hRes;
  }
  //3) Accept-Encoding: gzip, deflate
  hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "Accept-Encoding",
                               ((bAcceptCompressedContent != FALSE)  ? "gzip, deflate, identity" : "identity"),
                               nBrowser);
  if (FAILED(hRes))
    return hRes;
  //4) Accept-Language: en-us,ie_ee;q=0.5
  if (nHdrIndex_AcceptLanguage == (SIZE_T)-1)
  {
    hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "Accept-Language", "en-us", nBrowser);
    if (FAILED(hRes))
      return hRes;
  }
  //5) Host: www.mydomain.com
  hRes = sRequest.cUrl.ToString(cStrTempA, CUrl::ToStringAddHostPort);
  if (SUCCEEDED(hRes))
    hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "Host", (LPCSTR)cStrTempA, nBrowser);
  if (FAILED(hRes))
    return hRes;
  //6) Referer
  if (nHdrIndex_Referer == (SIZE_T)-1)
  {
    hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "Referer", "", nBrowser);
    if (FAILED(hRes))
      return hRes;
  }
  //7) User-Agent: Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322) (opcional)
  if (nHdrIndex_UserAgent == (SIZE_T)-1)
  {
    hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "User-Agent", szDefaultUserAgentA, nBrowser);
    if (FAILED(hRes))
      return hRes;
  }
  //8) Connection
  hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "Connection",
                               ((bKeepConnectionOpen != FALSE) ? "Keep-Alive" : "Close"), nBrowser);
  if (FAILED(hRes))
    return hRes;
  //9) Proxy-Connection (if using a proxy)
  if (sRequest.bUsingProxy != FALSE && sRequest.cUrl.GetSchemeCode() != CUrl::SchemeHttps)
  {
    hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "Proxy-Connection",
                                 ((bKeepConnectionOpen != FALSE) ? "Keep-Alive" : "Close"), nBrowser);
    if (FAILED(hRes))
      return hRes;
  }
  //10) Upgrade-Insecure-Requests
  hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "Upgrade-Insecure-Requests", "1", nBrowser);
  if (FAILED(hRes))
    return hRes;
  //12) WWW-Authenticate
  if (nHdrIndex_WWWAuthenticate == (SIZE_T)-1)
  {
    if (cStrAuthUserNameW.IsEmpty() == FALSE)
    {
      CHttpAuthBase *lpHttpAuth;

      //check if there is a previous authentication for this web request
      lpHttpAuth = HttpAuthCache::Lookup(sRequest.cUrl);
      if (lpHttpAuth != NULL)
      {
        switch (lpHttpAuth->GetType())
        {
          case CHttpAuthBase::TypeBasic:
            hRes = ((CHttpAuthBasic*)lpHttpAuth)->MakeAuthenticateResponse(cStrTempA, (LPCWSTR)cStrAuthUserNameW,
                                                                           (LPCWSTR)cStrAuthUserPasswordW, FALSE);
            if (FAILED(hRes))
              return hRes;
            if (cStrReqHdrsA.ConcatN((LPCSTR)cStrTempA, cStrTempA.GetLength()) == FALSE)
              return E_OUTOFMEMORY;
            break;

          case CHttpAuthBase::TypeDigest:
            {
            CStringA cStrPathA;
            LPCSTR szMethodA;

            if (sRequest.cStrMethodA.IsEmpty() != FALSE)
            {
              szMethodA = (sRequest.sPostData.cList.IsEmpty() == FALSE ||
                           sRequest.sPostData.bIsDynamic != FALSE) ? "POST" : "GET";
            }
            else
            {
              szMethodA = (LPCSTR)(sRequest.cStrMethodA);
            }

            hRes = sRequest.cUrl.ToString(cStrPathA, CUrl::ToStringAddPath);
            if (FAILED(hRes))
              return hRes;

            hRes = ((CHttpAuthDigest*)lpHttpAuth)->MakeAuthenticateResponse(cStrTempA, (LPCWSTR)cStrAuthUserNameW,
                                                                            (LPCWSTR)cStrAuthUserPasswordW, szMethodA,
                                                                            (LPCSTR)cStrPathA, FALSE);
            if (FAILED(hRes))
              return hRes;
            if (cStrReqHdrsA.ConcatN((LPCSTR)cStrTempA, cStrTempA.GetLength()) == FALSE)
              return E_OUTOFMEMORY;
            }
            break;

          default:
            MX_ASSERT(FALSE);
            return MX_E_Unsupported;
        }
      }
    }
  }
  //12) the rest of the headers
  for (nIndex = 0; (lpHeader=sRequest.cHttpCmn.GetHeader(nIndex)) != NULL; nIndex++)
  {
    LPCSTR sA = lpHeader->GetHeaderName();

    switch (*sA)
    {
      case 'A':
      case 'a':
        if (StrCompareA(sA, "Accept-Encoding", TRUE) != 0)
          goto add_header;
        break;

      case 'C':
      case 'c':
        if (StrCompareA(sA, "Connection", TRUE) != 0 &&
            StrCompareA(sA, "Cookie", TRUE) != 0 &&
            StrCompareA(sA, "Cookie2", TRUE) != 0)
        {
          goto add_header;
        }
        break;

      case 'E':
      case 'e':
        if (StrCompareA(sA, "Expect", TRUE) != 0)
          goto add_header;
        break;

      case 'H':
      case 'h':
        if (StrCompareA(sA, "Host", TRUE) != 0)
          goto add_header;
        break;

      case 'P':
      case 'p':
        if (StrCompareA(sA, "Proxy-Connection", TRUE) != 0)
          goto add_header;
        break;

      case 'U':
      case 'u':
        if (StrCompareA(sA, "Upgrade-Insecure-Requests", TRUE) != 0)
          goto add_header;
        break;

      default:
add_header:
        hRes = lpHeader->Build(cStrTempA, nBrowser);
        if (FAILED(hRes))
          return hRes;
        if (cStrTempA.IsEmpty() == FALSE)
        {
          if (cStrReqHdrsA.AppendFormat("%s: %s\r\n", lpHeader->GetHeaderName(), (LPSTR)cStrTempA) == FALSE)
            return E_OUTOFMEMORY;
        }
        break;
    }
  }
  //12) Cookies: ????
  cStrTempA.Empty();
  for (nIndex=0; (lpCookie = sRequest.cHttpCmn.GetCookie(nIndex)) != NULL; nIndex++)
  {
    if (lpCookie->DoesDomainMatch(sRequest.cUrl.GetHost()) != FALSE)
    {
      if (cStrTempA.AppendFormat("%s%s=%s", ((cStrTempA.IsEmpty() == FALSE) ? "; " : ""),
                                 lpCookie->GetName(), lpCookie->GetValue()) == FALSE)
      {
        return E_OUTOFMEMORY;
      }
    }
  }
  if (cStrTempA.IsEmpty() == FALSE)
  {
    if (cStrReqHdrsA.AppendFormat("Cookie: %s\r\n", (LPCSTR)cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CHttpClient::BuildRequestHeaderAdd(_Inout_ CStringA &cStrReqHdrsA, _In_z_ LPCSTR szNameA,
                                           _In_z_ LPCSTR szDefaultValueA, _In_ CHttpHeaderBase::eBrowser nBrowser)
{
  CHttpHeaderBase *lpHeader;
  CStringA cStrTempA;
  LPCSTR szValueA;
  HRESULT hRes;

  szValueA = szDefaultValueA;
  lpHeader = sRequest.cHttpCmn.GetHeader(szNameA);
  if (lpHeader != NULL)
  {
    hRes = lpHeader->Build(cStrTempA, nBrowser);
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
    {
      return E_OUTOFMEMORY;
    }
  }
  return S_OK;
}

HRESULT CHttpClient::AddRequestHeadersForBody(_Inout_ CStringA &cStrReqHdrsA)
{
  TLnkLst<CPostDataItem>::Iterator it;
  CPostDataItem *lpItem;
  BOOL bHasContentLengthHeader, bHasContentTypeHeader;
  ULONGLONG nLen;

  bHasContentLengthHeader = (sRequest.cHttpCmn.GetHeader<CHttpHeaderEntContentLength>() != NULL) ? TRUE : FALSE;
  bHasContentTypeHeader = (sRequest.cHttpCmn.GetHeader<CHttpHeaderEntContentType>() != NULL) ? TRUE : FALSE;

  nLen = 0ui64;
  if (sRequest.sPostData.cList.IsEmpty() == FALSE && sRequest.sPostData.bHasRaw != FALSE)
  {
    if (bHasContentLengthHeader == FALSE)
    {
      for (lpItem = it.Begin(sRequest.sPostData.cList); lpItem != NULL; lpItem = it.Next())
      {
        nLen += lpItem->cStream->GetLength();
      }
      //add content length
      if (cStrReqHdrsA.AppendFormat("Content-Length: %I64u\r\n", nLen) == FALSE)
        return E_OUTOFMEMORY;
    }

    sRequest.bUsingMultiPartFormData = FALSE;
  }
  else
  {
    for (lpItem=it.Begin(sRequest.sPostData.cList); lpItem!=NULL; lpItem=it.Next())
    {
      if (lpItem->cStream)
      {
        nLen = ULONGLONG_MAX;
        break;
      }
      if (nLen > 0ui64)
        nLen++; //size of '&'
      nLen += (ULONGLONG)CUrl::GetEncodedLength(lpItem->szNameA); //size of encoded name
      nLen++; //size of '='
      nLen += (ULONGLONG)CUrl::GetEncodedLength(lpItem->szValueA); //size of encoded value
    }
    //no files specified and post data length is small
    if (nLen < (ULONGLONG)MAX_FORM_SIZE_4_REQUEST)
    {
      //add content type
      if (bHasContentTypeHeader == FALSE)
      {
        if (cStrReqHdrsA.Concat("Content-Type: application/x-www-form-urlencoded\r\n") == FALSE)
          return E_OUTOFMEMORY;
      }
      //add content length
      if (bHasContentLengthHeader == FALSE)
      {
        if (cStrReqHdrsA.AppendFormat("Content-Length: %I64u\r\n", nLen) == FALSE)
          return E_OUTOFMEMORY;
      }
      //----
      sRequest.bUsingMultiPartFormData = FALSE;
    }
    else
    {
      //add content type
      if (bHasContentTypeHeader == FALSE)
      {
        if (cStrReqHdrsA.ConcatN("Content-Type:multipart/form-data; boundary=----", 47) == FALSE ||
            cStrReqHdrsA.ConcatN(sRequest.szBoundaryA, 27) == FALSE ||
            cStrReqHdrsA.ConcatN("\r\n", 2) == FALSE)
        {
          return E_OUTOFMEMORY;
        }
      }

      //add content length
      if (bHasContentLengthHeader == FALSE)
      {
        nLen = 0ui64;
        for (lpItem = it.Begin(sRequest.sPostData.cList); lpItem != NULL; lpItem = it.Next())
        {
          nLen += 4ui64 + 27ui64 + 2ui64; //size of boundary start
          nLen += 38ui64; //size of 'Content-Disposition: form-data; name="'
          nLen += (ULONGLONG)StrLenA(lpItem->szNameA);
          nLen += 1ui64; //size of '"'
          if (lpItem->cStream)
          {
            nLen += 12ui64; //size of '; filename="'
            nLen += (ULONGLONG)StrLenA(lpItem->szValueA); //size of filename
            nLen += 3ui64; //size of '"\r\n'
            nLen += 14ui64; //size of 'Content-type: '
            nLen += (ULONGLONG)StrLenA(CHttpCommon::GetMimeType(lpItem->szValueA)); //size of mime type
            nLen += 2ui64; //size of '"\r\n'
            nLen += 33ui64; //size of 'Content-Transfer-Encoding: binary\r\n'
            nLen += 2ui64; //size of '\r\n'
            nLen += lpItem->cStream->GetLength(); //size of data
            nLen += 2ui64; //size of '\r\n'
          }
          else
          {
            nLen += 2ui64; //size of '\r\n'
            nLen += 27ui64; //size of 'Content-Type: text/plain;\r\n'
            nLen += 2ui64; //size of '\r\n'
            nLen += (ULONGLONG)StrLenA(lpItem->szValueA); //size of data
            nLen += 2ui64; //size of '\r\n'
          }
        }
        nLen += 4ui64 + 27ui64 + 2ui64 + 2ui64; //size of boundary end
        if (cStrReqHdrsA.AppendFormat("Content-Length: %I64u\r\n", ++nLen) == FALSE)
          return E_OUTOFMEMORY;
      }
      //----
      sRequest.bUsingMultiPartFormData = TRUE;
    }
  }
  //done
  return S_OK;
}

HRESULT CHttpClient::SendTunnelConnect()
{
  MX::CStringA cStrReqHdrsA, cStrTempA;
  CHttpHeaderBase *lpHeader;
  CHttpAuthBase *lpHttpAuth;
  SIZE_T nIndex;
  HRESULT hRes;

  hRes = sRequest.cUrl.ToString(cStrTempA, CUrl::ToStringAddHostPort);
  if (FAILED(hRes))
    return hRes;
  if (cStrReqHdrsA.Format("CONNECT %s HTTP/1.1\r\nContent-Length: 0\r\nPragma: no-cache\r\nHost: ",
                          (LPCSTR)cStrTempA) == FALSE)
  {
    return E_OUTOFMEMORY;
  }
  hRes = sRequest.cUrl.GetHost(cStrTempA);
  if (FAILED(hRes))
    return hRes;
  if (cStrReqHdrsA.AppendFormat("%s\r\n", (LPCSTR)cStrTempA) == FALSE)
    return E_OUTOFMEMORY;
  //user agent
  for (nIndex=0; (lpHeader=sRequest.cHttpCmn.GetHeader(nIndex)) != NULL; nIndex++)
  {
    if (StrCompareA(lpHeader->GetHeaderName(), "User-Agent", TRUE) == 0)
    {
      hRes = lpHeader->Build(cStrTempA, CHttpHeaderBase::BrowserOther);
      if (FAILED(hRes))
        return hRes;
      if (cStrTempA.IsEmpty() == FALSE)
      {
        if (cStrReqHdrsA.AppendFormat("%s: %s\r\n", lpHeader->GetHeaderName(), (LPCSTR)cStrTempA) == FALSE)
          return E_OUTOFMEMORY;
      }
      break;
    }
  }
  if (lpHeader == NULL)
  {
    hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "User-Agent", szDefaultUserAgentA, CHttpHeaderBase::BrowserOther);
    if (FAILED(hRes))
      return hRes;
  }
  //proxy connection
  hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "Proxy-Connection",
                               ((bKeepConnectionOpen != FALSE) ? "Keep-Alive" : "Close"),
                               CHttpHeaderBase::BrowserOther);
  if (FAILED(hRes))
    return hRes;
  //check if there is a previous authentication for this proxy
  lpHttpAuth = HttpAuthCache::Lookup(CUrl::SchemeNone, cProxy.GetAddress(), cProxy.GetPort(), L"/");
  if (lpHttpAuth != NULL)
  {
    switch (lpHttpAuth->GetType())
    {
      case CHttpAuthBase::TypeBasic:
        hRes = ((CHttpAuthBasic*)lpHttpAuth)->MakeAuthenticateResponse(cStrTempA, cProxy._GetUserName(),
                                                                       cProxy.GetUserPassword(), TRUE);
        if (FAILED(hRes))
          return hRes;
        if (cStrReqHdrsA.ConcatN((LPCSTR)cStrTempA, cStrTempA.GetLength()) == FALSE)
          return E_OUTOFMEMORY;
        break;

      case CHttpAuthBase::TypeDigest:
        {
        //for proxy the url is the same than the one being requested
        CStringA cStrPathA;
        LPCSTR szMethodA;

        if (sRequest.cStrMethodA.IsEmpty() != FALSE)
        {
          szMethodA = (sRequest.sPostData.cList.IsEmpty() == FALSE ||
                        sRequest.sPostData.bIsDynamic != FALSE) ? "POST" : "GET";
        }
        else
        {
          szMethodA = (LPCSTR)(sRequest.cStrMethodA);
        }

        hRes = sRequest.cUrl.ToString(cStrPathA, CUrl::ToStringAddPath);
        if (FAILED(hRes))
          return hRes;

        hRes = ((CHttpAuthDigest*)lpHttpAuth)->MakeAuthenticateResponse(cStrTempA, cProxy._GetUserName(),
                                                                        cProxy.GetUserPassword(), szMethodA,
                                                                        (LPCSTR)cStrPathA, TRUE);
        if (FAILED(hRes))
          return hRes;
        if (cStrReqHdrsA.ConcatN((LPCSTR)cStrTempA, cStrTempA.GetLength()) == FALSE)
          return E_OUTOFMEMORY;
        }
        break;

      default:
        MX_ASSERT(FALSE);
        return MX_E_Unsupported;
    }
  }
  //send CONNECT
  if (cStrReqHdrsA.ConcatN("\r\n", 2) == FALSE)
    return E_OUTOFMEMORY;
  hRes = cSocketMgr.SendMsg(hConn, (LPSTR)cStrReqHdrsA, cStrReqHdrsA.GetLength());
  if (FAILED(hRes))
    return hRes;
  //done
  nState = StateWaitingProxyTunnelConnectionResponse;
  return S_OK;
}

HRESULT CHttpClient::SendRequestHeaders()
{
  CStringA cStrReqHdrsA;
  HRESULT hRes;

  hRes = BuildRequestHeaders(cStrReqHdrsA);
  //NOTE: Dynamic POST requires "Content-###" headers to be provided by the sender
  if (SUCCEEDED(hRes) && StrCompareA((LPCSTR)(sRequest.cStrMethodA), "HEAD") != 0 &&
      sRequest.sPostData.cList.IsEmpty() == FALSE)
  {
    hRes = AddRequestHeadersForBody(cStrReqHdrsA);
  }
  if (SUCCEEDED(hRes) && cStrReqHdrsA.ConcatN("\r\n", 2) == FALSE)
    hRes = E_OUTOFMEMORY;
  if (SUCCEEDED(hRes))
  {
    if (ShouldLog(1) != FALSE)
    {
      Log(L"HttpClient(ReqHeaders/0x%p): %S", this, (LPCSTR)cStrReqHdrsA);
    }
    hRes = cSocketMgr.SendMsg(hConn, (LPSTR)cStrReqHdrsA, cStrReqHdrsA.GetLength());
  }
  if (SUCCEEDED(hRes))
  {
    hRes = cSocketMgr.AfterWriteSignal(hConn, MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnAfterSendRequestHeaders, this),
                                       NULL);
  }
  //done
  return hRes;
}

HRESULT CHttpClient::SendRequestBody()
{
  TLnkLst<CPostDataItem>::Iterator it;
  CPostDataItem *lpItem;
  CStringA cStrTempA;
  HRESULT hRes;

  if (sRequest.sPostData.cList.IsEmpty() == FALSE && sRequest.sPostData.bHasRaw != FALSE)
  {
    for (lpItem=it.Begin(sRequest.sPostData.cList); lpItem!=NULL; lpItem=it.Next())
    {
      //send data stream
      hRes = cSocketMgr.SendStream(hConn, lpItem->cStream.Get());
      if (FAILED(hRes))
        return hRes;
    }
    //send end of data stream
    hRes = cSocketMgr.SendMsg(hConn, "\r\n", 2);
    if (FAILED(hRes))
      return hRes;
  }
  else if (sRequest.bUsingMultiPartFormData == FALSE)
  {
    CStringA cStrValueEncA;

    if (cStrTempA.EnsureBuffer(MAX_FORM_SIZE_4_REQUEST + 2) == FALSE)
      return E_OUTOFMEMORY;
    for (lpItem=it.Begin(sRequest.sPostData.cList); lpItem!=NULL; lpItem=it.Next())
    {
      //add separator
      if (cStrTempA.IsEmpty() == FALSE)
      {
        if (cStrTempA.Concat("&") == FALSE)
          return E_OUTOFMEMORY;
      }
      //add encoded name
      hRes = CUrl::Encode(cStrValueEncA, lpItem->szNameA);
      if (FAILED(hRes))
        return hRes;
      if (cStrTempA.Concat((LPSTR)cStrValueEncA) == FALSE)
        return E_OUTOFMEMORY;
      //add equal sign
      if (cStrTempA.Concat("=") == FALSE)
        return E_OUTOFMEMORY;
      //add encoded value
      hRes = CUrl::Encode(cStrValueEncA, lpItem->szValueA);
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
    for (lpItem=it.Begin(sRequest.sPostData.cList); lpItem!=NULL; lpItem=it.Next())
    {
      //add boundary start and content disposition
      if (cStrTempA.Format("----%s\r\nContent-Disposition: form-data; name=\"%s\"", sRequest.szBoundaryA,
                           lpItem->szNameA) == FALSE)
        return E_OUTOFMEMORY;

      if (lpItem->cStream)
      {
        //add filename, content type and content transfer encoding
        if (cStrTempA.AppendFormat("; filename=\"%s\"\r\nContent-Type: %s;\r\nContent-Transfer-Encoding: binary\r\n"
                                   "\r\n", lpItem->szValueA, CHttpCommon::GetMimeType(lpItem->szValueA)) == FALSE)
        {
          return E_OUTOFMEMORY;
        }
        //send
        hRes = cSocketMgr.SendMsg(hConn, (LPSTR)cStrTempA, cStrTempA.GetLength());
        if (FAILED(hRes))
          return hRes;

        //send data stream
        hRes = cSocketMgr.SendStream(hConn, lpItem->cStream.Get());
        if (FAILED(hRes))
          return hRes;

        //send end of data stream
        hRes = cSocketMgr.SendMsg(hConn, "\r\n", 2);
        if (FAILED(hRes))
          return hRes;
      }
      else
      {
        //add content type and value
        if (cStrTempA.ConcatN("\r\nContent-Type: text/plain;\r\n\r\n", 31) == FALSE ||
            cStrTempA.Concat(lpItem->szValueA) == FALSE ||
            cStrTempA.ConcatN("\r\n", 2) == FALSE)
        {
          return E_OUTOFMEMORY;
        }

        //send
        hRes = cSocketMgr.SendMsg(hConn, (LPSTR)cStrTempA, cStrTempA.GetLength());
        if (FAILED(hRes))
          return hRes;
      }
    }

    //send boundary end and end of body '\r\n'
    if (cStrTempA.Format("----%s--\r\n\r\n", sRequest.szBoundaryA) == FALSE)
      return E_OUTOFMEMORY;
    hRes = cSocketMgr.SendMsg(hConn, (LPSTR)cStrTempA, cStrTempA.GetLength());
    if (FAILED(hRes))
      return hRes;
  }
  //done
  return S_OK;
}

VOID CHttpClient::GenerateRequestBoundary()
{
  Fnv64_t value;
  LPVOID lpPtr;
  DWORD dw;
  SIZE_T i, val;

  //build boundary name
  MxMemCopy(sRequest.szBoundaryA, "MXLIB_HTTP_", 11);
#pragma warning(suppress : 28159)
  dw = ::GetTickCount();
  value = fnv_64a_buf(&dw, 4, FNV1A_64_INIT);
  dw = MxGetCurrentProcessId();
  value = fnv_64a_buf(&dw, 4, value);
  lpPtr = this;
  value = fnv_64a_buf(&lpPtr, sizeof(lpPtr), value);
  value = fnv_64a_buf(&sRequest, sizeof(sRequest), value);
  value = fnv_64a_buf(&sResponse, sizeof(sResponse), value);
  for (i=0; i<16; i++)
  {
    val = (SIZE_T)(value & 15);
    sRequest.szBoundaryA[11 + i] = (CHAR)(((val >= 10) ? 55 : 48) + val);
    value >>= 4;
  }
  sRequest.szBoundaryA[27] = 0;
  return;
}

HRESULT CHttpClient::OnDownloadStarted(_Out_ LPHANDLE lphFile, _In_z_ LPCWSTR szFileNameW, _In_ LPVOID lpUserParam)
{
  MX::CStringW cStrFileNameW;
#ifndef _DEBUG
  MX_IO_STATUS_BLOCK sIoStatusBlock;
  UCHAR _DeleteFile;
#endif //!_DEBUG
  HRESULT hRes;

  if (sResponse.cStrDownloadFileNameW.IsEmpty() != FALSE)
  {
    SIZE_T nLen;
    DWORD dw;
    Fnv64_t nHash;

    //generate filename
    if (cStrTemporaryFolderW.IsEmpty() != FALSE)
    {
      hRes = CHttpCommon::_GetTempPath(cStrFileNameW);
      if (FAILED(hRes))
        return hRes;
    }
    else
    {
      nLen = cStrTemporaryFolderW.GetLength();
      if (cStrFileNameW.CopyN((LPCWSTR)cStrTemporaryFolderW, nLen) == FALSE)
        return E_OUTOFMEMORY;
      if (nLen > 0 && ((LPWSTR)(cStrFileNameW))[nLen - 1] != L'\\')
      {
        if (cStrFileNameW.Concat(L"\\") == FALSE)
          return E_OUTOFMEMORY;
      }
    }

    dw = ::GetCurrentProcessId();
    nHash = fnv_64a_buf(&dw, sizeof(dw), FNV1A_64_INIT);
#pragma warning(suppress : 28159)
    dw = ::GetTickCount();
    nHash = fnv_64a_buf(&dw, sizeof(dw), nHash);
    nLen = (SIZE_T)this;
    nHash = fnv_64a_buf(&nLen, sizeof(nLen), nHash);
    nHash = fnv_64a_buf(&sResponse, sizeof(sResponse), nHash);
    if (cStrFileNameW.AppendFormat(L"tmp%016I64x", (ULONGLONG)nHash) == FALSE)
      return E_OUTOFMEMORY;
  }

  //create target file
  *lphFile = ::CreateFileW(((sResponse.cStrDownloadFileNameW.IsEmpty() == FALSE)
                           ? (LPCWSTR)(sResponse.cStrDownloadFileNameW) : (LPCWSTR)cStrFileNameW),
#ifdef _DEBUG
                           GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
#else //_DEBUG
                           GENERIC_READ | GENERIC_WRITE | DELETE, 0,
#endif //_DEBUG
                           NULL, CREATE_ALWAYS,
#ifdef _DEBUG
                           FILE_ATTRIBUTE_NORMAL,
#else //_DEBUG
                           FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_TEMPORARY,
#endif //_DEBUG
                           NULL);
  if ((*lphFile) == NULL || (*lphFile) == INVALID_HANDLE_VALUE)
  {
    *lphFile = NULL;
    return MX_HRESULT_FROM_LASTERROR();
  }
#ifndef _DEBUG
  _DeleteFile = 1;
  ::MxNtSetInformationFile(*lphFile, &sIoStatusBlock, &_DeleteFile, (ULONG)sizeof(_DeleteFile),
                           MxFileDispositionInformation);
#endif //!_DEBUG
  //done
  return S_OK;
}

VOID CHttpClient::ResetRequestForNewRequest()
{
  CPostDataItem *lpItem;

  while ((lpItem = sRequest.sPostData.cList.PopHead()) != NULL)
    delete lpItem;
  sRequest.sPostData.bHasRaw = FALSE;
  sRequest.sPostData.bIsDynamic = FALSE;
  return;
}

VOID CHttpClient::ResetResponseForNewRequest()
{
  sResponse.cHttpCmn.ResetParser();
  sResponse.cStrDownloadFileNameW.Empty();
  sResponse.dwBodyLowThroughputCcounter = 0;
  return;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CHttpClient::CPostDataItem::CPostDataItem(_In_z_ LPCSTR _szNameA, _In_z_ LPCSTR _szValueA) :
                            CBaseMemObj(), TLnkLstNode<CPostDataItem>(), CNonCopyableObj()
{
  SIZE_T nNameLen, nValueLen;

  szNameA = szValueA = NULL;
  nNameLen = StrLenA(_szNameA);
  nValueLen = StrLenA(_szValueA);
  if (nNameLen + nValueLen > nNameLen && nNameLen + nValueLen + 2 > nNameLen + nValueLen)
  {
    szNameA = (LPSTR)MX_MALLOC(nNameLen + nValueLen + 2);
    if (szNameA != NULL)
    {
      szValueA = szNameA + nNameLen + 1;
      MxMemCopy(szNameA, _szNameA, nNameLen);
      szNameA[nNameLen] = 0;
      MxMemCopy(szValueA, _szValueA, nValueLen);
      szValueA[nValueLen] = 0;
    }
  }
  return;
}

CHttpClient::CPostDataItem::CPostDataItem(_In_z_ LPCSTR _szNameA, _In_z_ LPCSTR _szFileNameA, _In_ CStream *lpStream) :
                            CBaseMemObj(), TLnkLstNode<CPostDataItem>(), CNonCopyableObj()
{
  SIZE_T nNameLen, nValueLen;

  szNameA = szValueA = NULL;
  nNameLen = StrLenA(_szNameA);
  nValueLen = StrLenA(_szFileNameA);
  if (nNameLen + nValueLen > nNameLen && nNameLen + nValueLen + 2 > nNameLen + nValueLen)
  {
    szNameA = (LPSTR)MX_MALLOC(nNameLen + nValueLen + 2);
    if (szNameA != NULL)
    {
      szValueA = szNameA + nNameLen + 1;
      MxMemCopy(szNameA, _szNameA, nNameLen);
      szNameA[nNameLen] = 0;
      MxMemCopy(szValueA, _szFileNameA, nValueLen);
      szValueA[nValueLen] = 0;
    }
  }
  cStream = lpStream;
  return;
}

CHttpClient::CPostDataItem::CPostDataItem(_In_ CStream *lpStream) : CBaseMemObj(), TLnkLstNode<CPostDataItem>(),
                                                                    CNonCopyableObj()
{
  szNameA = szValueA = NULL;
  cStream = lpStream;
  return;
}

CHttpClient::CPostDataItem::~CPostDataItem()
{
  MX_FREE(szNameA);
  return;
}

} //namespace MX
