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

#define _HTTP_STATUS_SwitchingProtocols                  101
#define _HTTP_STATUS_MovedPermanently                    301
#define _HTTP_STATUS_MovedTemporarily                    302
#define _HTTP_STATUS_SeeOther                            303
#define _HTTP_STATUS_UseProxy                            305
#define _HTTP_STATUS_TemporaryRedirect                   307
#define _HTTP_STATUS_Unauthorized                        401
#define _HTTP_STATUS_ProxyAuthenticationRequired         407
#define _HTTP_STATUS_RequestTimeout                      408

#define MSG_BUFFER_SIZE                                32768

#define _DYNAMIC_FLAG_IsActive                        0x0001
#define _DYNAMIC_FLAG_EndMark                         0x0002

//-----------------------------------------------------------

static const LPCSTR szDefaultUserAgentA = "Mozilla/5.0 (compatible; MX-Lib 1.0)";

//-----------------------------------------------------------

static BOOL _GetTempPath(_Out_ MX::CStringW &cStrPathW);

//-----------------------------------------------------------

namespace MX {

CHttpClient::CHttpClient(_In_ CSockets &_cSocketMgr,
                         _In_opt_ CLoggable *lpLogParent) : TRefCounted<CBaseMemObj>(), CLoggable(), CNonCopyableObj(),
                                                            cSocketMgr(_cSocketMgr)
{
  SetLogParent(lpLogParent);
  sResponse.cParser.SetLogParent(this);
  //----
  dwMaxRedirCount = DEFAULT_MAX_REDIRECTIONS_COUNT;
  dwMaxFieldSize = 256000;
  ullMaxFileSize = 2097152ui64;
  dwMaxFilesCount = 4;
  dwMaxBodySizeInMemory = 32768;
  ullMaxBodySize = 10485760ui64;
  dwMaxRawRequestBodySizeInMemory = 131072;
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
  cWebSocketHandshakeCompletedCallback = NullCallback();
  cQueryCertificatesCallback = NullCallback();
  _InterlockedExchange(&nPendingHandlesCounter, 0);
  //----
  sRequest.sPostData.bHasRaw = FALSE;
  sRequest.sPostData.nDynamicFlags = 0;
  MxMemSet(sRequest.szBoundaryA, 0, sizeof(sRequest.szBoundaryA));
  sRequest.bUsingMultiPartFormData = FALSE;
  sRequest.bUsingProxy = FALSE;
  sResponse.aMsgBuf.Attach((LPBYTE)MX_MALLOC(MSG_BUFFER_SIZE));
  return;
}

CHttpClient::~CHttpClient()
{
  LONG volatile nWait = 0;
  CLnkLstNode *lpNode;

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
  MX::TimedEvent::Clear(&(sRedirectOrRetryAuth.nTimerId));

  //wait until pending handles are closed
  while (__InterlockedRead(&nPendingHandlesCounter) > 0)
    _YieldProcessor();

  //delete request's post data
  while ((lpNode = sRequest.sPostData.cList.PopHead()) != NULL)
  {
    CPostDataItem *lpPostDataItem = CONTAINING_RECORD(lpNode, CPostDataItem, cListNode);

    delete lpPostDataItem;
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
    sResponse.cParser.SetOption_MaxHeaderSize(dwSize);
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

VOID CHttpClient::SetOption_MaxRawRequestBodySizeInMemory(_In_ DWORD dwSize)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == StateClosed)
  {
    dwMaxRawRequestBodySizeInMemory = dwSize;
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

VOID CHttpClient::SetWebSocketHandshakeCompletedCallback(_In_ OnWebSocketHandshakeCompletedCallback
                                                         _cWebSocketHandshakeCompletedCallback)
{
  cWebSocketHandshakeCompletedCallback = _cWebSocketHandshakeCompletedCallback;
  return;
}

VOID CHttpClient::SetDymanicRequestBodyStartCallback(_In_ OnDymanicRequestBodyStartCallback
                                                     _cDymanicRequestBodyStartCallback)
{
  cDymanicRequestBodyStartCallback = _cDymanicRequestBodyStartCallback;
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
  if (Http::IsValidVerb(szMethodA, StrLenA(szMethodA)) == FALSE)
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

HRESULT CHttpClient::AddRequestHeader(_In_ CHttpHeaderBase *lpHeader)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  HRESULT hRes;

  if (lpHeader == NULL)
    return E_POINTER;

  //add to list
  hRes = sRequest.cHeaders.AddElement(lpHeader);
  if (FAILED(hRes))
    return hRes;
  lpHeader->AddRef();

  //done
  return S_OK;
}

HRESULT CHttpClient::AddRequestHeader(_In_z_ LPCSTR szNameA, _Out_opt_ CHttpHeaderBase **lplpHeader,
                                      _In_opt_ BOOL bReplaceExisting)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  TAutoRefCounted<CHttpHeaderBase> cNewHeader;
  HRESULT hRes;

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateClosed && nState != StateDocumentCompleted)
    return MX_E_NotReady;

  //create and add to list
  hRes = CHttpHeaderBase::Create(szNameA, TRUE, &cNewHeader);
  if (SUCCEEDED(hRes))
    hRes = sRequest.cHeaders.AddElement(cNewHeader.Get());
  if (FAILED(hRes))
    return hRes;
  cNewHeader->AddRef();

  //done
  if (lplpHeader != NULL)
    *lplpHeader = cNewHeader.Detach();
  return S_OK;
}

HRESULT CHttpClient::AddRequestHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen,
                                      _Out_opt_ CHttpHeaderBase **lplpHeader, _In_opt_ BOOL bReplaceExisting)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  TAutoRefCounted<CHttpHeaderBase> cNewHeader;
  HRESULT hRes;

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateClosed && nState != StateDocumentCompleted)
    return MX_E_NotReady;

  //create and add to list
  hRes = CHttpHeaderBase::Create(szNameA, TRUE, &cNewHeader);
  if (SUCCEEDED(hRes))
    hRes = cNewHeader->Parse(szValueA, nValueLen);
  if (SUCCEEDED(hRes))
    hRes = sRequest.cHeaders.AddElement(cNewHeader.Get());
  if (FAILED(hRes))
    return hRes;
  cNewHeader->AddRef();

  //done
  if (lplpHeader != NULL)
    *lplpHeader = cNewHeader.Detach();
  return S_OK;
}

HRESULT CHttpClient::AddRequestHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW, _In_opt_ SIZE_T nValueLen,
                                      _Out_opt_ CHttpHeaderBase **lplpHeader, _In_opt_ BOOL bReplaceExisting)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  TAutoRefCounted<CHttpHeaderBase> cNewHeader;
  HRESULT hRes;

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (nState != StateClosed && nState != StateDocumentCompleted)
    return MX_E_NotReady;
  
  //create and add to list
  hRes = CHttpHeaderBase::Create(szNameA, TRUE, &cNewHeader);
  if (SUCCEEDED(hRes))
    hRes = cNewHeader->Parse(szValueW, nValueLen);
  if (SUCCEEDED(hRes))
    hRes = sRequest.cHeaders.AddElement(cNewHeader.Get());
  if (FAILED(hRes))
    return hRes;
  cNewHeader->AddRef();

  //done
  if (lplpHeader != NULL)
    *lplpHeader = cNewHeader.Detach();
  return S_OK;
}

HRESULT CHttpClient::RemoveRequestHeader(_In_z_ LPCSTR szNameA)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  SIZE_T nIndex;
  HRESULT hRes = MX_E_NotFound;

  if (nState != StateClosed && nState != StateDocumentCompleted)
    return MX_E_NotReady;

  while ((nIndex = sRequest.cHeaders.Find(szNameA)) != (SIZE_T)-1)
  {
    sRequest.cHeaders.RemoveElementAt(nIndex);
    hRes = S_OK;
  }
  return hRes;
}

HRESULT CHttpClient::RemoveAllRequestHeaders()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed && nState != StateDocumentCompleted)
    return MX_E_NotReady;

  sRequest.cHeaders.RemoveAllElements();
  return S_OK;
}

SIZE_T CHttpClient::GetRequestHeadersCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));

  if (nState != StateClosed && nState != StateDocumentCompleted)
    return MX_E_NotReady;
  return sRequest.cHeaders.GetCount();
}

CHttpHeaderBase* CHttpClient::GetRequestHeader(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));
  CHttpHeaderBase *lpHeader;

  if (nState != StateClosed && nState != StateDocumentCompleted)
    return NULL;
  if (nIndex >= sRequest.cHeaders.GetCount())
    return NULL;
  lpHeader = sRequest.cHeaders.GetElementAt(nIndex);
  lpHeader->AddRef();
  return lpHeader;
}

CHttpHeaderBase* CHttpClient::GetRequestHeaderByName(_In_z_ LPCSTR szNameA) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));
  CHttpHeaderBase *lpHeader;
  SIZE_T nIndex;

  if (nState != StateClosed && nState != StateDocumentCompleted)
    return NULL;
  nIndex = sRequest.cHeaders.Find(szNameA);
  if (nIndex == (SIZE_T)-1)
    return NULL;
  lpHeader = sRequest.cHeaders.GetElementAt(nIndex);
  lpHeader->AddRef();
  return lpHeader;
}

HRESULT CHttpClient::AddRequestCookie(_In_ CHttpCookie *lpCookie)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  if (lpCookie == NULL)
    return E_POINTER;

  if (sRequest.cCookies.AddElement(lpCookie) == FALSE)
    return E_OUTOFMEMORY;
  lpCookie->AddRef();
  return S_OK;
}

HRESULT CHttpClient::AddRequestCookie(_In_ CHttpCookieArray &cSrc, _In_opt_ BOOL bReplaceExisting)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  return sRequest.cCookies.Merge(cSrc, bReplaceExisting);
}

HRESULT CHttpClient::AddRequestCookie(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA)
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
    if (SUCCEEDED(hRes))
      hRes = AddRequestCookie(cCookie.Get());
  }
  //done
  return hRes;
}

HRESULT CHttpClient::AddRequestCookie(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szValueW)
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
    if (SUCCEEDED(hRes))
      hRes = AddRequestCookie(cCookie.Get());
  }
  //done
  return hRes;
}

HRESULT CHttpClient::RemoveRequestCookie(_In_z_ LPCSTR szNameA)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  SIZE_T nIndex;
  HRESULT hRes = MX_E_NotFound;

  if (nState != StateClosed)
    return MX_E_NotReady;

  while ((nIndex = sRequest.cCookies.Find(szNameA)) != (SIZE_T)-1)
  {
    sRequest.cCookies.RemoveElementAt(nIndex);
    hRes = S_OK;
  }
  return hRes;
}

HRESULT CHttpClient::RemoveRequestCookie(_In_z_ LPCWSTR szNameW)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  SIZE_T nIndex;
  HRESULT hRes = MX_E_NotFound;

  if (nState != StateClosed)
    return MX_E_NotReady;

  while ((nIndex = sRequest.cCookies.Find(szNameW)) != (SIZE_T)-1)
  {
    sRequest.cCookies.RemoveElementAt(nIndex);
    hRes = S_OK;
  }
  return hRes;
}

HRESULT CHttpClient::RemoveAllRequestCookies()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  sRequest.cCookies.RemoveAllElements();
  return S_OK;
}

SIZE_T CHttpClient::GetRequestCookiesCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));

  if (nState != StateClosed)
    return 0;
  return sRequest.cCookies.GetCount();
}

CHttpCookie* CHttpClient::GetRequestCookie(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));
  CHttpCookie *lpCookie;

  if (nState != StateClosed)
    return NULL;
  if (nIndex >= sRequest.cCookies.GetCount())
    return NULL;
  lpCookie = sRequest.cCookies.GetElementAt(nIndex);
  lpCookie->AddRef();
  return lpCookie;
}

CHttpCookie *CHttpClient::GetRequestCookieByName(_In_z_ LPCSTR szNameA) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));
  SIZE_T nIndex;
  CHttpCookie *lpCookie;

  if (nState != StateClosed)
    return NULL;
  nIndex = sRequest.cCookies.Find(szNameA);
  if (nIndex == (SIZE_T)-1)
    return NULL;
  lpCookie = sRequest.cCookies.GetElementAt(nIndex);
  lpCookie->AddRef();
  return lpCookie;
}

CHttpCookie* CHttpClient::GetRequestCookieByName(_In_z_ LPCWSTR szNameW) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));
  SIZE_T nIndex;
  CHttpCookie *lpCookie;

  if (nState != StateClosed)
    return NULL;
  nIndex = sRequest.cCookies.Find(szNameW);
  if (nIndex == (SIZE_T)-1)
    return NULL;
  lpCookie = sRequest.cCookies.GetElementAt(nIndex);
  lpCookie->AddRef();
  return lpCookie;
}

HRESULT CHttpClient::AddRequestPostData(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  CPostDataItem *lpNewItem;
  LPCSTR sA;

  if (nState != StateClosed)
    return MX_E_NotReady;
  if (szNameA == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  for (sA = szNameA; *sA != 0 && Http::IsValidNameChar(*sA) != FALSE; sA++);
  if (*sA != 0)
    return E_INVALIDARG;
  if (sRequest.sPostData.cList.IsEmpty() == FALSE)
  {
    if (sRequest.sPostData.bHasRaw != FALSE)
      return E_FAIL;
  }
  //create new pair
  lpNewItem = MX_DEBUG_NEW CPostDataItem(szNameA, ((szValueA != NULL) ? szValueA : ""));
  if (lpNewItem == NULL)
    return E_OUTOFMEMORY;
  if (lpNewItem->szNameA == NULL)
  {
    delete lpNewItem;
    return E_OUTOFMEMORY;
  }
  sRequest.sPostData.cList.PushTail(&(lpNewItem->cListNode));
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
  CPostDataItem *lpNewItem;
  LPCSTR sA;

  if (nState != StateClosed)
    return MX_E_NotReady;
  if (szNameA == NULL || szFileNameA == NULL || lpStream == NULL)
    return E_POINTER;
  if (*szNameA == 0 || *szFileNameA == 0)
    return E_INVALIDARG;
  for (sA = szNameA; *sA != 0 && Http::IsValidNameChar(*sA) != FALSE; sA++);
  if (*sA != 0)
    return E_INVALIDARG;
  for (sA = szFileNameA; *sA != 0 && *sA != '\"' && *((LPBYTE)sA) >= 32; sA++);
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
  lpNewItem = MX_DEBUG_NEW CPostDataItem(szNameA, sA, lpStream);
  if (lpNewItem == NULL)
    return E_OUTOFMEMORY;
  if (lpNewItem->szNameA == NULL)
  {
    delete lpNewItem;
    return E_OUTOFMEMORY;
  }
  sRequest.sPostData.cList.PushTail(&(lpNewItem->cListNode));
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
  HRESULT hRes;

  if (nState != StateClosed && nState != StateSendingDynamicRequestBody && nState != StateWaitingForRedirection &&
      nState != StateRetryingAuthentication && nState != StateEstablishingProxyTunnelConnection &&
      nState != StateWaitingProxyTunnelConnectionResponse)
  {
    return MX_E_NotReady;
  }
  if (nLength == 0)
    return S_OK;
  if (lpData == NULL)
    return E_POINTER;

  if (nState != StateSendingDynamicRequestBody)
  {
    TAutoRefCounted<CMemoryStream> cStream;
    SIZE_T nWritten;

    if (sRequest.sPostData.cList.IsEmpty() == FALSE)
    {
      if (sRequest.sPostData.bHasRaw == FALSE)
        return E_FAIL;

      //try to append to last stream
      if (nLength < 65536)
      {
        CLnkLstNode *lpNode;
        CPostDataItem *lpPostDataItem;

        lpNode = sRequest.sPostData.cList.GetTail();
        lpPostDataItem = CONTAINING_RECORD(lpNode, CPostDataItem, cListNode);

        if (lpPostDataItem->cStream && lpPostDataItem->cStream->GetLength() + nLength < 65536)
        {
          return cStream->Write(lpData, nLength, nWritten);
        }
      }
    }

    //create stream
    cStream.Attach(MX_DEBUG_NEW CMemoryStream());
    if (cStream)
    {
      hRes = cStream->Create(65536);
      if (SUCCEEDED(hRes))
      {
        CPostDataItem *lpNewPostDataItem;

        hRes = cStream->Write(lpData, nLength, nWritten);
        if (SUCCEEDED(hRes))
        {
          //create new pair
          lpNewPostDataItem = MX_DEBUG_NEW CPostDataItem(cStream.Get());
          if (lpNewPostDataItem != NULL)
          {
            sRequest.sPostData.cList.PushTail(&(lpNewPostDataItem->cListNode));
            sRequest.sPostData.bHasRaw = TRUE;
          }
          else
          {
            hRes = E_OUTOFMEMORY;
          }
        }
      }
    }
    else
    {
      hRes = E_OUTOFMEMORY;
    }
  }
  else
  {
    //direct
    hRes = cSocketMgr.SendMsg(hConn, lpData, nLength);
  }

  //done
  return hRes;
}

HRESULT CHttpClient::AddRequestRawPostData(_In_ CStream *lpStream)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed && nState != StateSendingDynamicRequestBody)
    return MX_E_NotReady;
  if (lpStream == NULL)
    return E_POINTER;
  if (nState == StateClosed)
  {
    CPostDataItem *lpNewItem;

    if (sRequest.sPostData.cList.IsEmpty() == FALSE)
    {
      if (sRequest.sPostData.bHasRaw == FALSE)
        return E_FAIL;
    }
    //create new pair
    lpNewItem = MX_DEBUG_NEW CPostDataItem(lpStream);
    if (lpNewItem == NULL)
      return E_OUTOFMEMORY;
    sRequest.sPostData.cList.PushTail(&(lpNewItem->cListNode));
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

HRESULT CHttpClient::RemoveRequestPostData(_In_opt_z_ LPCSTR szNameA)
{
  CCriticalSection::CAutoLock cLock(cMutex);
  CLnkLst::Iterator it;
  CLnkLstNode *lpNode;
  BOOL bGotOne;

  if (nState != StateClosed)
    return MX_E_NotReady;
  bGotOne = FALSE;
  if (szNameA != NULL && *szNameA != 0)
  {
    for (lpNode = it.Begin(sRequest.sPostData.cList); lpNode != NULL; lpNode = it.Next())
    {
      CPostDataItem *lpPostDataItem = CONTAINING_RECORD(lpNode, CPostDataItem, cListNode);

      if (lpPostDataItem->szNameA != NULL && StrCompareA(szNameA, lpPostDataItem->szNameA) == 0)
      {
        lpNode->Remove();
        delete lpPostDataItem;
        bGotOne = TRUE;
      }
    }
  }
  else
  {
    for (lpNode = it.Begin(sRequest.sPostData.cList); lpNode != NULL; lpNode = it.Next())
    {
      CPostDataItem *lpPostDataItem = CONTAINING_RECORD(lpNode, CPostDataItem, cListNode);

      if (lpPostDataItem->szNameA == NULL)
      {
        lpNode->Remove();
        delete lpPostDataItem;
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
  CLnkLstNode *lpNode;

  if (nState != StateClosed)
    return MX_E_NotReady;
  while ((lpNode = sRequest.sPostData.cList.PopHead()) != NULL)
  {
    CPostDataItem *lpPostDataItem = CONTAINING_RECORD(lpNode, CPostDataItem, cListNode);

    delete lpPostDataItem;
  }
  return S_OK;
}

HRESULT CHttpClient::EnableDynamicPostData()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != StateClosed)
    return MX_E_NotReady;
  if (sRequest.sPostData.nDynamicFlags != 0)
    return MX_E_InvalidState;
  sRequest.sPostData.nDynamicFlags = _DYNAMIC_FLAG_IsActive;
  return S_OK;
}

HRESULT CHttpClient::SignalEndOfPostData()
{
  CCriticalSection::CAutoLock cLock(cMutex);
  HRESULT hRes;

  if (nState != StateClosed && nState != StateSendingDynamicRequestBody && nState != StateWaitingForRedirection &&
      nState != StateRetryingAuthentication && nState != StateEstablishingProxyTunnelConnection &&
      nState != StateWaitingProxyTunnelConnectionResponse)
  {
    return MX_E_NotReady;
  }
  if (sRequest.sPostData.nDynamicFlags != _DYNAMIC_FLAG_IsActive)
    return MX_E_InvalidState;

  if (nState == StateSendingDynamicRequestBody)
  {
    //send end of body mark
    hRes = cSocketMgr.SendMsg(hConn, "\r\n", 2);
    if (FAILED(hRes))
    {
      SetErrorOnRequestAndClose(hRes);
      return hRes;
    }

    nState = StateReceivingResponseHeaders;
  }

  sRequest.sPostData.nDynamicFlags |= _DYNAMIC_FLAG_EndMark;

  //done
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
    return (HRESULT)hr;
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

HRESULT CHttpClient::Open(_In_ CUrl &cUrl, _In_opt_ LPOPEN_OPTIONS lpOptions)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  CCriticalSection::CAutoLock cLock(cMutex);

  if (cAutoRundownProt.IsAcquired() == FALSE)
    return MX_E_NotReady;
  return InternalOpen(cUrl, lpOptions, FALSE);
}

HRESULT CHttpClient::Open(_In_z_ LPCSTR szUrlA, _In_opt_ LPOPEN_OPTIONS lpOptions)
{
  CUrl cUrl;
  HRESULT hRes;

  if (szUrlA == NULL)
    return E_POINTER;
  if (*szUrlA == 0)
    return E_INVALIDARG;
  hRes = cUrl.ParseFromString(szUrlA);
  if (SUCCEEDED(hRes))
    hRes = Open(cUrl, lpOptions);
  return hRes;
}

HRESULT CHttpClient::Open(_In_z_ LPCWSTR szUrlW, _In_opt_ LPOPEN_OPTIONS lpOptions)
{
  CUrl cUrl;
  HRESULT hRes;

  if (szUrlW == NULL)
    return E_POINTER;
  if (*szUrlW == 0)
    return E_INVALIDARG;
  hRes = cUrl.ParseFromString(szUrlW);
  if (SUCCEEDED(hRes))
    hRes = Open(cUrl, lpOptions);
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
  MX::TimedEvent::Clear(&(sRedirectOrRetryAuth.nTimerId));
  return;
}

HRESULT CHttpClient::GetLastRequestError() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));

  return hLastErrorCode;
}

HRESULT CHttpClient::IsResponseHeaderComplete() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));

  return (nState == StateReceivingResponseBody || nState == StateDocumentCompleted) ? TRUE : FALSE;
}

BOOL CHttpClient::IsDocumentComplete() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));

  return (nState == StateDocumentCompleted) ? TRUE : FALSE;
}

BOOL CHttpClient::IsClosed() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));

  return (nState == StateClosed) ? TRUE : FALSE;
}

LONG CHttpClient::GetResponseStatus() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));

  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return 0;
  return sResponse.cParser.GetResponseStatus();
}

HRESULT CHttpClient::GetResponseReason(_Inout_ CStringA &cStrDestA) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));

  cStrDestA.Empty();
  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return MX_E_NotReady;
  return (cStrDestA.Copy(sResponse.cParser.GetResponseReasonA()) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

CHttpBodyParserBase* CHttpClient::GetResponseBodyParser() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));

  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return NULL;
  return sResponse.cParser.GetBodyParser();
}

SIZE_T CHttpClient::GetResponseHeadersCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return 0;
  return sResponse.cParser.Headers().GetCount();
}

CHttpHeaderBase* CHttpClient::GetResponseHeader(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));
  CHttpHeaderArray *lpHeadersArray;
  CHttpHeaderBase *lpHeader;

  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return NULL;
  lpHeadersArray = &(sResponse.cParser.Headers());
  if (nIndex >= lpHeadersArray->GetCount())
    return NULL;
  lpHeader = lpHeadersArray->GetElementAt(nIndex);
  lpHeader->AddRef();
  return lpHeader;
}

CHttpHeaderBase* CHttpClient::GetResponseHeaderByName(_In_z_ LPCSTR szNameA) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));
  CHttpHeaderArray *lpHeadersArray;
  CHttpHeaderBase *lpHeader;
  SIZE_T nIndex;

  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return NULL;
  lpHeadersArray = &(sResponse.cParser.Headers());
  nIndex = lpHeadersArray->Find(szNameA);
  if (nIndex == (SIZE_T)-1)
    return NULL;
  lpHeader = lpHeadersArray->GetElementAt(nIndex);
  lpHeader->AddRef();
  return lpHeader;
}

SIZE_T CHttpClient::GetResponseCookiesCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return 0;
  return sResponse.cParser.Cookies().GetCount();
}

CHttpCookie* CHttpClient::GetResponseCookie(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));
  CHttpCookieArray *lpCookieArray;
  CHttpCookie *lpCookie;

  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return NULL;
  lpCookieArray = &(sResponse.cParser.Cookies());
  if (nIndex >= lpCookieArray->GetCount())
    return NULL;
  lpCookie = lpCookieArray->GetElementAt(nIndex);
  lpCookie->AddRef();
  return lpCookie;
}

CHttpCookie* CHttpClient::GetResponseCookieByName(_In_z_ LPCSTR szNameA) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));
  CHttpCookieArray *lpCookieArray;
  CHttpCookie *lpCookie;
  SIZE_T nIndex;

  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return NULL;
  lpCookieArray = &(sResponse.cParser.Cookies());
  nIndex = lpCookieArray->Find(szNameA);
  if (nIndex == (SIZE_T)-1)
    return NULL;
  lpCookie = lpCookieArray->GetElementAt(nIndex);
  lpCookie->AddRef();
  return lpCookie;
}

CHttpCookie* CHttpClient::GetResponseCookieByName(_In_z_ LPCWSTR szNameW) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));
  CHttpCookieArray *lpCookieArray;
  CHttpCookie *lpCookie;
  SIZE_T nIndex;

  if (nState != StateReceivingResponseBody && nState != StateDocumentCompleted)
    return NULL;
  lpCookieArray = &(sResponse.cParser.Cookies());
  nIndex = lpCookieArray->Find(szNameW);
  if (nIndex == (SIZE_T)-1)
    return NULL;
  lpCookie = lpCookieArray->GetElementAt(nIndex);
  lpCookie->AddRef();
  return lpCookie;
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

HRESULT CHttpClient::InternalOpen(_In_ CUrl &cUrl, _In_opt_ LPOPEN_OPTIONS lpOptions, _In_ BOOL bRedirectionAuth)
{
  CStringA cStrHostA;
  LPCWSTR szConnectHostW;
  int nUrlPort, nConnectPort;
  eState nOrigState;
  HRESULT hRes;

  if (!(sResponse.aMsgBuf))
    return E_OUTOFMEMORY;

  if (cUrl.GetSchemeCode() != CUrl::SchemeHttp && cUrl.GetSchemeCode() != CUrl::SchemeHttps &&
      cUrl.GetSchemeCode() != CUrl::SchemeWebSocket && cUrl.GetSchemeCode() != CUrl::SchemeSecureWebSocket)
  {
    return E_INVALIDARG;
  }
  if (cUrl.GetHost()[0] == 0)
    return E_INVALIDARG;

  if (bRedirectionAuth == FALSE)
  {
    sRequest.cWebSocket.Release();
    sRedirectOrRetryAuth.dwRedirectCounter = 0;
    sAuthorization.bSeen401 = sAuthorization.bSeen407 = FALSE;
    sAuthorization.nSeen401Or407Counter = 0;
  }

  //check WebSocket opttions
  if ((cUrl.GetSchemeCode() == CUrl::SchemeWebSocket || cUrl.GetSchemeCode() == CUrl::SchemeSecureWebSocket) &&
      bRedirectionAuth == FALSE)
  {
    TAutoRefCounted<CHttpHeaderReqSecWebSocketVersion> cReqSecWebSocketVersion;
    TAutoRefCounted<CHttpHeaderReqSecWebSocketKey> cReqSecWebSocketKey;
    TAutoRefCounted<CHttpHeaderGenUpgrade> cGenUpgrade;
    TAutoRefCounted<CHttpHeaderGenConnection> cGenConnection;

    //first, only GET is allowed
    if (sRequest.cStrMethodA.IsEmpty() != FALSE)
    {
      if (sRequest.sPostData.cList.IsEmpty() == FALSE || sRequest.sPostData.nDynamicFlags != 0)
        return E_INVALIDARG;
    }
    else
    {
      if (StrCompareA((LPCSTR)(sRequest.cStrMethodA), "GET") != 0)
        return E_INVALIDARG;
    }

    //second, check options
    if (lpOptions == NULL)
      return E_INVALIDARG;
    if (lpOptions->sWebSocket.lpSocket == NULL)
      return E_POINTER;

    sRequest.cWebSocket = lpOptions->sWebSocket.lpSocket;

    //version
    cReqSecWebSocketVersion.Attach(MX_DEBUG_NEW CHttpHeaderReqSecWebSocketVersion());
    if (!cReqSecWebSocketVersion)
      return E_OUTOFMEMORY;
    hRes = cReqSecWebSocketVersion->SetVersion(lpOptions->sWebSocket.nVersion);
    if (SUCCEEDED(hRes))
    {
      hRes = sRequest.cHeaders.AddElement(cReqSecWebSocketVersion.Get());
      if (SUCCEEDED(hRes))
        cReqSecWebSocketVersion.Detach();
    }
    if (FAILED(hRes))
      return hRes;

    //protocols
    if (lpOptions->sWebSocket.lpszProtocolsA != NULL)
    {
      TAutoRefCounted<CHttpHeaderReqSecWebSocketProtocol> cReqSecWebSocketProtocol;

      cReqSecWebSocketProtocol.Attach(MX_DEBUG_NEW CHttpHeaderReqSecWebSocketProtocol());
      if (!cReqSecWebSocketProtocol)
        return E_OUTOFMEMORY;
      for (SIZE_T i = 0; lpOptions->sWebSocket.lpszProtocolsA[i] != NULL; i++)
      {
        hRes = cReqSecWebSocketProtocol->AddProtocol(lpOptions->sWebSocket.lpszProtocolsA[i], (SIZE_T)-1);
        if (FAILED(hRes))
          return hRes;
      }
      hRes = sRequest.cHeaders.AddElement(cReqSecWebSocketProtocol.Get());
      if (FAILED(hRes))
        return hRes;
      cReqSecWebSocketProtocol.Detach();
    }

    //key
    cReqSecWebSocketKey.Attach(MX_DEBUG_NEW CHttpHeaderReqSecWebSocketKey());
    if (!cReqSecWebSocketKey)
      return E_OUTOFMEMORY;
    hRes = cReqSecWebSocketKey->GenerateKey(32);
    if (SUCCEEDED(hRes))
    {
      hRes = sRequest.cHeaders.AddElement(cReqSecWebSocketKey.Get());
      if (SUCCEEDED(hRes))
        cReqSecWebSocketKey.Detach();
    }
    if (FAILED(hRes))
      return hRes;

    //upgrade
    cGenUpgrade.Attach(MX_DEBUG_NEW CHttpHeaderGenUpgrade());
    if (!cGenUpgrade)
      return E_OUTOFMEMORY;
    hRes = cGenUpgrade->AddProduct("websocket");
    if (SUCCEEDED(hRes))
    {
      hRes = sRequest.cHeaders.AddElement(cGenUpgrade.Get());
      if (SUCCEEDED(hRes))
        cGenUpgrade.Detach();
    }
    if (FAILED(hRes))
      return hRes;

    //connection
    cGenConnection.Attach(MX_DEBUG_NEW CHttpHeaderGenConnection());
    if (!cGenConnection)
      return E_OUTOFMEMORY;
    hRes = cGenConnection->AddConnection("Upgrade");
    if (SUCCEEDED(hRes))
    {
      hRes = sRequest.cHeaders.AddElement(cGenConnection.Get());
      if (SUCCEEDED(hRes))
        cGenConnection.Detach();
    }
    if (FAILED(hRes))
      return hRes;
  }

  //get port
  if (cUrl.GetPort() >= 0)
    nUrlPort = cUrl.GetPort();
  else if (cUrl.GetSchemeCode() == CUrl::SchemeHttps || cUrl.GetSchemeCode() == CUrl::SchemeSecureWebSocket)
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
    if (sResponse.cParser.GetState() != Internals::CHttpParser::StateDone ||
        StrCompareW(sRequest.cUrl.GetScheme(), cUrl.GetScheme()) != 0 ||
        StrCompareW(sRequest.cUrl.GetHost(), szConnectHostW) != 0 ||
        nUrlPort != nConnectPort)
    {
      cSocketMgr.Close(hConn, S_OK);
      hConn = NULL;
    }
  }
  //setup new state
  nState = ((cUrl.GetSchemeCode() == CUrl::SchemeHttps || cUrl.GetSchemeCode() == CUrl::SchemeWebSocket) &&
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
      hRes = (HRESULT)hr;
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

    hRes = OnSocketConnect(&cSocketMgr, hConn, NULL);
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

  if (cAutoRundownProt.IsAcquired() != FALSE)
  {
    CCriticalSection::CAutoLock cLock(cMutex);

    if (h == hConn)
    {
      switch (nState)
      {
        case StateDocumentCompleted:
          break;

        case StateWaitingForRedirection:
        case StateRetryingAuthentication:
        case StateEstablishingProxyTunnelConnection:
        case StateWaitingProxyTunnelConnectionResponse:
        case StateSendingRequestHeaders:
        case StateSendingDynamicRequestBody:
        case StateReceivingResponseHeaders:
        case StateReceivingResponseBody:
          nState = StateClosed;
          if (SUCCEEDED(hLastErrorCode)) //preserve first error
            hLastErrorCode = SUCCEEDED(hrErrorCode) ? MX_E_Cancelled : hrErrorCode;
          break;

        default:
          nState = StateClosed;
          break;
      }

      hConn = NULL;
    }
  }

  //done
  _InterlockedDecrement(&nPendingHandlesCounter);
  return;
}

HRESULT CHttpClient::OnSocketConnect(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_opt_ CIpc::CUserData *lpUserData)
{
  HRESULT hRes = S_OK;

  {
    CCriticalSection::CAutoLock cLock(cMutex);

    //does belong tu us?
    if (h != hConn)
      return MX_E_Cancelled;

    switch (nState)
    {
      case StateEstablishingProxyTunnelConnection:
        hRes = SendTunnelConnect();
        break;

      case StateSendingRequestHeaders:
        hRes = SendRequestHeaders();
        break;
    }
    if (FAILED(hRes))
      SetErrorOnRequestAndClose(hRes);
  }

  //done
  return S_OK;
}

//NOTE: "CIpc" guarantees no simultaneous calls to 'OnSocketDataReceived' will be received from different threads
HRESULT CHttpClient::OnSocketDataReceived(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  SIZE_T nMsgSize;
  BOOL bFireResponseHeadersReceivedCallback, bFireDocumentCompleted;
  BOOL bFireWebSocketHandshakeCompletedCallback;
  int nRedirectOrRetryAuthTimerAction;
  struct {
    CStringW cStrFileNameW;
    ULONGLONG nContentLength, *lpnContentLength;
    CStringA cStrTypeA;
    BOOL bTreatAsAttachment;
    CStringW cStrDownloadFileNameW;
  } sHeadersCallbackData;
  struct {
    TAutoRefCounted<CWebSocket> cWebSocket;
    CStringA cStrProtocolA;
  } sWebSocketHandshakeCallbackData;
  HRESULT hRes;

  if (cAutoRundownProt.IsAcquired() == FALSE)
    return MX_E_Cancelled;

  nMsgSize = 0;
  nRedirectOrRetryAuthTimerAction = 0;

restart:
  bFireResponseHeadersReceivedCallback = bFireDocumentCompleted = bFireWebSocketHandshakeCompletedCallback = FALSE;

  if (nRedirectOrRetryAuthTimerAction < 0)
  {
    MX::TimedEvent::Clear(&(sRedirectOrRetryAuth.nTimerId));
    nRedirectOrRetryAuthTimerAction = 0;
  }
  else if (nRedirectOrRetryAuthTimerAction > 0)
  {
    MX::TimedEvent::Clear(&(sRedirectOrRetryAuth.nTimerId));
    hRes = MX::TimedEvent::SetTimeout(&(sRedirectOrRetryAuth.nTimerId), (DWORD)nRedirectOrRetryAuthTimerAction,
                                      MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnRedirectOrRetryAuth, this), NULL);
    if (FAILED(hRes))
    {
      //just a fatal error if we cannot set up the timers
      CCriticalSection::CAutoLock cLock(cMutex);

      SetErrorOnRequestAndClose(hRes);
      return S_OK;
    }
  }

  {
    CCriticalSection::CAutoLock cLock(cMutex);
    Internals::CHttpParser::eState nParserState;
    SIZE_T nMsgUsed;
    LONG nRespStatus;
    BOOL bBreakLoop;

    //does belong to us?
    if (h != hConn)
      return MX_E_Cancelled;

    //loop
    bBreakLoop = FALSE;
    while (bBreakLoop == FALSE && bFireResponseHeadersReceivedCallback == FALSE && bFireDocumentCompleted == FALSE &&
           bFireWebSocketHandshakeCompletedCallback == FALSE)
    {
      //get buffered message
      nMsgSize = MSG_BUFFER_SIZE;
      hRes = cSocketMgr.GetBufferedMessage(h, sResponse.aMsgBuf.Get(), &nMsgSize);
      if (SUCCEEDED(hRes))
      {
        if (nMsgSize == 0)
          break;
      }
      else
      {
on_request_error:
        nRedirectOrRetryAuthTimerAction = -1;

        SetErrorOnRequestAndClose(hRes);
        goto restart;
      }

      //take action depending on current state
      switch (nState)
      {
        case StateWaitingProxyTunnelConnectionResponse:
          //process http being received
          hRes = sResponse.cParser.Parse(sResponse.aMsgBuf.Get(), nMsgSize, nMsgUsed);
          if (FAILED(hRes))
            goto on_request_error;
          if (nMsgUsed > 0)
          {
            hRes = cSocketMgr.ConsumeBufferedMessage(h, nMsgUsed);
            if (FAILED(hRes))
              goto on_request_error;
          }

          //take action if parser's state changed
          nParserState = sResponse.cParser.GetState();

          //check for end
          switch (nParserState)
          {
            case Internals::CHttpParser::StateBodyStart:
            case Internals::CHttpParser::StateDone:
              nRespStatus = sResponse.cParser.GetResponseStatus();

              //reset parser
              sResponse.cParser.Reset();
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
                goto on_request_error;
              break;
          }
          break;

        case StateReceivingResponseHeaders:
        case StateReceivingResponseBody:
          //process http being received
          hRes = sResponse.cParser.Parse(sResponse.aMsgBuf.Get(), nMsgSize, nMsgUsed);
          if (FAILED(hRes))
            goto on_request_error;
          if (nMsgUsed > 0)
          {
            hRes = cSocketMgr.ConsumeBufferedMessage(h, nMsgUsed);
            if (FAILED(hRes))
              goto on_request_error;
          }

          //take action if parser's state changed
          nParserState = sResponse.cParser.GetState();
          switch (nParserState)
          {
            case Internals::CHttpParser::StateBodyStart:
              nRespStatus = sResponse.cParser.GetResponseStatus();

              //we don't accept invalid status codes in response if we are dealing with a websocket
              if (nRespStatus <= 299 && nRespStatus != _HTTP_STATUS_SwitchingProtocols && sRequest.cWebSocket)
              {
                hRes = MX_E_InvalidData;
                goto on_request_error;
              }

              //check if we hit a redirection or a site/proxy authentication
              if ((nRespStatus == _HTTP_STATUS_MovedPermanently || nRespStatus == _HTTP_STATUS_MovedTemporarily ||
                   nRespStatus == _HTTP_STATUS_SeeOther || nRespStatus == _HTTP_STATUS_UseProxy ||
                   nRespStatus == _HTTP_STATUS_TemporaryRedirect || nRespStatus == _HTTP_STATUS_RequestTimeout) &&
                  sRedirectOrRetryAuth.dwRedirectCounter < dwMaxRedirCount)
              {
                hRes = SetupIgnoreBody();
                if (FAILED(hRes))
                  goto on_request_error;
              }
              else if (nRespStatus == _HTTP_STATUS_Unauthorized && cStrAuthUserNameW.IsEmpty() == FALSE &&
                       sAuthorization.nSeen401Or407Counter < DEFAULT_MAX_AUTHORIZATION_COUNT)
              {
                if (sAuthorization.bSeen401 == FALSE)
                {
                  CHttpHeaderRespWwwAuthenticate *lpHeader;

                  lpHeader = sResponse.cParser.Headers().Find<CHttpHeaderRespWwwAuthenticate>();
                  if (lpHeader != NULL)
                  {
                    TAutoRefCounted<CHttpAuthBase> cHttpAuth;

                    cHttpAuth = lpHeader->GetHttpAuth();
                    if (cHttpAuth && cHttpAuth->IsReauthenticateRequst() != FALSE)
                      goto ignore_body1;
                  }
                }
                else
                {
ignore_body1:     hRes = SetupIgnoreBody();
                  if (FAILED(hRes))
                    goto on_request_error;
                }
              }
              else if (nRespStatus == _HTTP_STATUS_ProxyAuthenticationRequired && *(cProxy._GetUserName()) != 0 &&
                       sAuthorization.nSeen401Or407Counter < DEFAULT_MAX_AUTHORIZATION_COUNT)
              {
                if (sAuthorization.bSeen407 != FALSE)
                {
                  CHttpHeaderRespWwwAuthenticate *lpHeader;

                  lpHeader = sResponse.cParser.Headers().Find<CHttpHeaderRespWwwAuthenticate>();
                  if (lpHeader != NULL)
                  {
                    TAutoRefCounted<CHttpAuthBase> cHttpAuth;

                    cHttpAuth = lpHeader->GetHttpAuth();
                    if (cHttpAuth && cHttpAuth->IsReauthenticateRequst() != FALSE)
                      goto ignore_body2;
                  }
                }
                else
                {
ignore_body2:     hRes = SetupIgnoreBody();
                  if (FAILED(hRes))
                    goto on_request_error;
                }
              }
              else if (nRespStatus == _HTTP_STATUS_SwitchingProtocols && sRequest.cWebSocket)
              {
                goto on_websocket_negotiated;
              }

              bFireResponseHeadersReceivedCallback = TRUE;
              nState = StateAfterHeaders;
              break;

            case Internals::CHttpParser::StateDone:
              nRespStatus = sResponse.cParser.GetResponseStatus();

              //we don't accept invalid status codes in response if we are dealing with a websocket
              if (nRespStatus <= 299 && nRespStatus != _HTTP_STATUS_SwitchingProtocols && sRequest.cWebSocket)
              {
                hRes = MX_E_InvalidData;
                goto on_request_error;
              }

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
                  hRes = (HRESULT)hr;
                }
                if (SUCCEEDED(hRes))
                {
                  if (sResponse.cParser.GetResponseStatus() != _HTTP_STATUS_RequestTimeout)
                  {
                    CHttpHeaderRespLocation *lpHeader;

                    lpHeader = sResponse.cParser.Headers().Find<CHttpHeaderRespLocation>();
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

                  lpHeader = sResponse.cParser.Headers().Find<CHttpHeaderRespRetryAfter>();
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
                  hRes = sRequest.cCookies.Merge(sResponse.cParser.Cookies(), TRUE);
                  if (SUCCEEDED(hRes))
                    hRes = sRequest.cCookies.RemoveExpiredAndInvalid();
                }
                if (FAILED(hRes))
                  goto on_request_error;

                //start redirector/waiter thread
                nState = StateWaitingForRedirection;
                nRedirectOrRetryAuthTimerAction = (nWaitTimeSecs > 0) ? ((int)nWaitTimeSecs * 1000) : 1;
                goto restart;
              }
              else if (((nRespStatus == _HTTP_STATUS_Unauthorized && cStrAuthUserNameW.IsEmpty() == FALSE) ||
                        (nRespStatus == _HTTP_STATUS_ProxyAuthenticationRequired && *(cProxy._GetUserName()) != 0)) &&
                       sAuthorization.nSeen401Or407Counter < DEFAULT_MAX_AUTHORIZATION_COUNT)
              {
                TAutoRefCounted<CHttpAuthBase> cHttpAuth;
                CHttpHeaderRespWwwAuthenticate *lpHeader;
                BOOL bDoNothing = FALSE;

                (sAuthorization.nSeen401Or407Counter)++;

                if (nRespStatus == _HTTP_STATUS_Unauthorized)
                {
                  //reset seen 407 so the proxy does not complain on next round if credentials fails for any reason
                  sAuthorization.bSeen407 = FALSE;

                  lpHeader = sResponse.cParser.Headers().Find<CHttpHeaderRespWwwAuthenticate>();
                  if (lpHeader != NULL)
                    cHttpAuth = lpHeader->GetHttpAuth();

                  if (sAuthorization.bSeen401 == FALSE ||
                      (cHttpAuth && cHttpAuth->IsReauthenticateRequst() != FALSE))
                  {
                    sAuthorization.bSeen401 = TRUE;
                  }
                  else
                  {
                    bDoNothing = TRUE;
                  }
                }
                else //nRespStatus == _HTTP_STATUS_ProxyAuthenticationRequired
                {
                  lpHeader = sResponse.cParser.Headers().Find<CHttpHeaderRespWwwAuthenticate>();
                  if (lpHeader != NULL)
                    cHttpAuth = lpHeader->GetHttpAuth();

                  if (sAuthorization.bSeen407 == FALSE ||
                      (cHttpAuth && cHttpAuth->IsReauthenticateRequst() != FALSE))
                  {
                    sAuthorization.bSeen407 = TRUE;
                  }
                  else
                  {
                    bDoNothing = TRUE;
                  }
                }

                if (bDoNothing == FALSE)
                {
                  if (!cHttpAuth)
                  {
                    hRes = MX_E_InvalidData;
                    goto on_request_error;
                  }

                  hRes = HttpAuthCache::Add(sRequest.cUrl, cHttpAuth.Get());
                  if (FAILED(hRes))
                    goto on_request_error;

                  nState = StateRetryingAuthentication;
                  nRedirectOrRetryAuthTimerAction = 1;
                  goto restart;
                }
              }
              else if (nRespStatus == _HTTP_STATUS_SwitchingProtocols && sRequest.cWebSocket)
              {
on_websocket_negotiated:
                //verify response
                CHttpHeaderRespSecWebSocketAccept *lpRespSecWebSocketAccept;
                CHttpHeaderReqSecWebSocketKey *lpReqSecWebSocketKey;

                lpRespSecWebSocketAccept = sResponse.cParser.Headers().Find<CHttpHeaderRespSecWebSocketAccept>();
                lpReqSecWebSocketKey = sRequest.cHeaders.Find<CHttpHeaderReqSecWebSocketKey>();
                MX_ASSERT(lpReqSecWebSocketKey != NULL);

                if (lpReqSecWebSocketKey != NULL && lpRespSecWebSocketAccept != NULL)
                {
                  hRes = lpRespSecWebSocketAccept->VerifyKey(lpReqSecWebSocketKey->GetKey(),
                                                             lpReqSecWebSocketKey->GetKeyLength());
                  if (hRes == S_FALSE)
                  {
                    hRes = MX_E_InvalidData;
                  }
                  else if (hRes == S_OK)
                  {
                    CHttpHeaderReqSecWebSocketProtocol *lpReqSecWebSocketProtocol;

                    lpReqSecWebSocketProtocol = sRequest.cHeaders.Find<CHttpHeaderReqSecWebSocketProtocol>();
                    if (lpReqSecWebSocketProtocol != NULL)
                    {
                      CHttpHeaderRespSecWebSocketProtocol *lpRespSecWebSocketProtocol;

                      lpRespSecWebSocketProtocol =
                        sResponse.cParser.Headers().Find<CHttpHeaderRespSecWebSocketProtocol>();

                      if (lpRespSecWebSocketProtocol == NULL ||
                          lpReqSecWebSocketProtocol->HasProtocol(lpRespSecWebSocketProtocol->GetProtocol()) == FALSE)
                      {
                        hRes = MX_E_InvalidData;
                      }
                    }
                  }
                }
                else
                {
                  hRes = MX_E_InvalidData;
                }

                if (FAILED(hRes))
                  goto on_request_error;

                bFireWebSocketHandshakeCompletedCallback = TRUE;
                nState = StateDocumentCompleted;
                break;
              }

              //no redirection or (re)auth
              bFireDocumentCompleted = TRUE;
              if (nState == StateReceivingResponseHeaders)
              {
                bFireResponseHeadersReceivedCallback = TRUE;
                nState = StateAfterHeaders;
              }
              else
              {
                nState = StateDocumentCompleted;
              }

              //on completion, keep the file
              if (sResponse.cStrDownloadFileNameW.IsEmpty() == FALSE)
              {
                TAutoRefCounted<CHttpBodyParserBase> cBodyParser;

                cBodyParser = sResponse.cParser.GetBodyParser();
                if (cBodyParser)
                {
                  if (MX::StrCompareA(cBodyParser->GetType(), "default") == 0)
                  {
                    ((CHttpBodyParserDefault*)(cBodyParser.Get()))->KeepFile();
                  }
                }
              }
              break;
          }
          break;

        case StateClosed:
          hRes = cSocketMgr.ConsumeBufferedMessage(h, nMsgSize);
          if (FAILED(hRes))
            goto on_request_error;
          break;

        default:
          bBreakLoop = TRUE;
          break;
      }
    }

    //prepare data to send in fire websocket handshake completed
    if (bFireWebSocketHandshakeCompletedCallback != FALSE)
    {
      CHttpHeaderRespSecWebSocketProtocol *lpRespSecWebSocketProtocol;

      sWebSocketHandshakeCallbackData.cWebSocket.Attach(sRequest.cWebSocket.Detach());

      lpRespSecWebSocketProtocol = sResponse.cParser.Headers().Find<CHttpHeaderRespSecWebSocketProtocol>();
      if (lpRespSecWebSocketProtocol != NULL)
      {
        if (sWebSocketHandshakeCallbackData.cStrProtocolA.Copy(lpRespSecWebSocketProtocol->GetProtocol()) == FALSE)
        {
          hRes = E_OUTOFMEMORY;
          goto on_request_error;
        }
      }
      else
      {
        sWebSocketHandshakeCallbackData.cStrProtocolA.Empty();
      }

      //setup websocket ipc
      hRes = sWebSocketHandshakeCallbackData.cWebSocket->SetupIpc(lpIpc, hConn, FALSE);
      if (FAILED(hRes))
        goto on_request_error;

      //the connection handle does not belong to us anymore
      hConn = NULL;
      _InterlockedDecrement(&nPendingHandlesCounter);
    }

    //prepare data to send in fire header callback
    if (bFireResponseHeadersReceivedCallback != FALSE)
    {
      CHttpHeaderEntContentType *lpEntContentType;
      CHttpHeaderEntContentLength *lpEntContentLength;
      CHttpHeaderEntContentDisposition *lpEntContentDisposition;
      LPCSTR szTypeA;
      LPCWSTR sW[2], szFileNameW;
      HRESULT hRes;

      lpEntContentType = sResponse.cParser.Headers().Find<CHttpHeaderEntContentType>();
      lpEntContentLength = sResponse.cParser.Headers().Find<CHttpHeaderEntContentLength>();
      lpEntContentDisposition = sResponse.cParser.Headers().Find<CHttpHeaderEntContentDisposition>();

      szFileNameW = L"";
      if (lpEntContentDisposition != NULL)
      {
        szFileNameW = lpEntContentDisposition->GetFileName();
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

      if (sHeadersCallbackData.cStrFileNameW.Copy(szFileNameW) == FALSE)
      {
        hRes = E_OUTOFMEMORY;
        goto on_request_error;
      }

      if (lpEntContentLength != NULL)
      {
        sHeadersCallbackData.lpnContentLength = &(sHeadersCallbackData.nContentLength);
        sHeadersCallbackData.nContentLength = lpEntContentLength->GetLength();
      }
      else
      {
        sHeadersCallbackData.lpnContentLength = NULL;
        sHeadersCallbackData.nContentLength = 0ui64;
      }

      szTypeA = (lpEntContentType != NULL) ? lpEntContentType->GetType() : "";
      if (*szTypeA == 0)
        szTypeA = "application/octet-stream";
      if (sHeadersCallbackData.cStrTypeA.Copy(szTypeA) == FALSE)
      {
        hRes = E_OUTOFMEMORY;
        goto on_request_error;
      }

      sHeadersCallbackData.bTreatAsAttachment = (lpEntContentDisposition != NULL &&
                                                 StrCompareA(lpEntContentDisposition->GetType(), "attachment",
                                                             TRUE) == 0) ? TRUE : FALSE;

      if (sHeadersCallbackData.cStrDownloadFileNameW.CopyN((LPCWSTR)(sResponse.cStrDownloadFileNameW),
                                                           sResponse.cStrDownloadFileNameW.GetLength()) == FALSE)
      {
        hRes = E_OUTOFMEMORY;
        goto on_request_error;
      }
    }
  }

  if (nRedirectOrRetryAuthTimerAction < 0)
  {
    MX::TimedEvent::Clear(&(sRedirectOrRetryAuth.nTimerId));
    nRedirectOrRetryAuthTimerAction = 0;
  }
  else if (nRedirectOrRetryAuthTimerAction > 0)
  {
    MX::TimedEvent::Clear(&(sRedirectOrRetryAuth.nTimerId));
    hRes = MX::TimedEvent::SetTimeout(&(sRedirectOrRetryAuth.nTimerId), (DWORD)nRedirectOrRetryAuthTimerAction,
                                      MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnRedirectOrRetryAuth, this), NULL);
    if (FAILED(hRes))
    {
      //just a fatal error if we cannot set up the timers
      CCriticalSection::CAutoLock cLock(cMutex);

      SetErrorOnRequestAndClose(hRes);
      return S_OK;
    }
  }

  //have some action to do?
  if (bFireWebSocketHandshakeCompletedCallback != FALSE)
  {
    if (cWebSocketHandshakeCompletedCallback)
    {
      cWebSocketHandshakeCompletedCallback(this, sWebSocketHandshakeCallbackData.cWebSocket.Get(),
                                           (LPCSTR)(sWebSocketHandshakeCallbackData.cStrProtocolA));
    }
    sWebSocketHandshakeCallbackData.cWebSocket->FireConnectedAndInitialRead();
    return S_OK;
  }

  if (bFireResponseHeadersReceivedCallback != FALSE)
  {
    TAutoRefCounted<CHttpBodyParserBase> cBodyParser;
    BOOL bRestart = FALSE;

    if (ShouldLog(1) != FALSE)
    {
      Log(L"HttpClient(ResponseHeadersReceived/0x%p)", this);
    }

    hRes = S_OK;
    if (cHeadersReceivedCallback)
    {
      hRes = cHeadersReceivedCallback(this, (LPCWSTR)(sHeadersCallbackData.cStrFileNameW),
                                      sHeadersCallbackData.lpnContentLength, (LPCSTR)(sHeadersCallbackData.cStrTypeA),
                                      sHeadersCallbackData.bTreatAsAttachment,
                                      sHeadersCallbackData.cStrDownloadFileNameW, &cBodyParser);
    }

    {
      CCriticalSection::CAutoLock cLock(cMutex);

      if (h == hConn && nState == StateAfterHeaders)
      {
        if (SUCCEEDED(hRes))
        {
          if (bFireDocumentCompleted == FALSE)
          {
            //add a default body if none was added
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
            {
              hRes = sResponse.cParser.SetBodyParser(cBodyParser.Get());
            }
            if (SUCCEEDED(hRes))
            {
              nState = StateReceivingResponseBody;
            }
          }
          else
          {
            nState = StateDocumentCompleted;
          }
        }

        if (FAILED(hRes))
        {
          nRedirectOrRetryAuthTimerAction = -1;

          SetErrorOnRequestAndClose(hRes);
          bRestart = TRUE;
        }
      }
    }

    if (bRestart != FALSE || bFireDocumentCompleted == FALSE)
      goto restart;
  }

  if (bFireDocumentCompleted != FALSE)
  {
    if (ShouldLog(1) != FALSE)
    {
      Log(L"HttpClient(DocumentCompleted/0x%p)", this);
    }
    if (cDocumentCompletedCallback)
    {
      cDocumentCompletedCallback(this);
    }
  }

  //done
  return S_OK;
}

VOID CHttpClient::OnRedirectOrRetryAuth(_In_ LONG nTimerId, _In_ LPVOID lpUserData, _In_opt_ LPBOOL lpbCancel)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);

  UNREFERENCED_PARAMETER(lpbCancel);

  if (cAutoRundownProt.IsAcquired() != FALSE)
  {
    HRESULT hRes = S_OK;

    if (_InterlockedCompareExchange(&(sRedirectOrRetryAuth.nTimerId), 0, nTimerId) == nTimerId)
    {
      CCriticalSection::CAutoLock cLock(cMutex);

      switch (nState)
      {
        case StateWaitingForRedirection:
          hRes = InternalOpen(sRedirectOrRetryAuth.cUrl, NULL, TRUE);
          break;

        case StateRetryingAuthentication:
          hRes = InternalOpen(sRequest.cUrl, NULL, TRUE);
          break;

        default:
          hRes = MX_E_InvalidState;
          break;
      }
      if (FAILED(hRes))
      {
        SetErrorOnRequestAndClose(hRes);
      }
    }
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
      if (StrCompareA((LPCSTR)(sRequest.cStrMethodA), "HEAD") != 0)
      {
        if (sRequest.sPostData.cList.IsEmpty() == FALSE)
        {
          hRes = SendRequestBody();
        }
        if (SUCCEEDED(hRes))
        {
          if (sRequest.sPostData.nDynamicFlags == 0 ||
              sRequest.sPostData.nDynamicFlags == (_DYNAMIC_FLAG_IsActive | _DYNAMIC_FLAG_EndMark))
          {
            //no body to send, directly switch to receiving response headers
            nState = StateReceivingResponseHeaders;
          }
          else
          {
            //sending dynamic body
            nState = StateSendingDynamicRequestBody;

            bFireDymanicRequestBodyStartCallback = TRUE;
          }
        }
      }
      else
      {
        nState = StateReceivingResponseHeaders;
      }
    }
    if (FAILED(hRes))
      SetErrorOnRequestAndClose(hRes);
  }
  if (SUCCEEDED(hRes) && bFireDymanicRequestBodyStartCallback != FALSE && cDymanicRequestBodyStartCallback)
  {
    cDymanicRequestBodyStartCallback(this);
  }

  //done
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
  SIZE_T nIndex, nCount;
  Http::eBrowser nBrowser = Http::BrowserOther;
  HRESULT hRes;

  if (cStrReqHdrsA.EnsureBuffer(32768) == FALSE)
    return E_OUTOFMEMORY;

  nHdrIndex_Accept = (SIZE_T)-1;
  nHdrIndex_AcceptLanguage = (SIZE_T)-1;
  nHdrIndex_Referer = (SIZE_T)-1;
  nHdrIndex_UserAgent = (SIZE_T)-1;
  nHdrIndex_WWWAuthenticate = (SIZE_T)-1;

  nCount = sRequest.cHeaders.GetCount();
  for (nIndex = 0; nIndex < nCount; nIndex++)
  {
    LPCSTR szNameA;

    lpHeader = sRequest.cHeaders.GetElementAt(nIndex);
    szNameA = lpHeader->GetHeaderName();

    switch (*szNameA)
    {
      case 'A':
      case 'a':
        if (StrCompareA(szNameA, "Accept", TRUE) == 0)
          nHdrIndex_Accept = nIndex;
        else if (StrCompareA(szNameA, "Accept-Language", TRUE) == 0)
          nHdrIndex_AcceptLanguage = nIndex;
        break;

      case 'R':
      case 'r':
        if (StrCompareA(szNameA, "Referer", TRUE) == 0)
          nHdrIndex_Referer = nIndex;
        break;

      case 'U':
      case 'u':
        if (StrCompareA(szNameA, "User-Agent", TRUE) == 0)
        {
          nHdrIndex_UserAgent = nIndex;
          nBrowser = Http::GetBrowserFromUserAgent(reinterpret_cast<CHttpHeaderGeneric*>(lpHeader)->GetValue());
        }
        break;

      case 'W':
      case 'w':
        if (StrCompareA(szNameA, "WWW-Authenticate", TRUE) == 0)
          nHdrIndex_WWWAuthenticate = nIndex;
        break;
    }
  }

  //1) GET/POST/{methood} url HTTP/1.1
  if (sRequest.cStrMethodA.IsEmpty() != FALSE)
  {
    if (cStrReqHdrsA.Copy((sRequest.sPostData.cList.IsEmpty() == FALSE ||
                           sRequest.sPostData.nDynamicFlags != 0) ? "POST " : "GET ") == FALSE)
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
      TAutoRefCounted<CHttpAuthBase> cHttpAuth;

      //check if there is a previous authentication for this web request
      cHttpAuth.Attach(HttpAuthCache::Lookup(sRequest.cUrl));
      if (cHttpAuth)
      {
        switch (cHttpAuth->GetType())
        {
          case CHttpAuthBase::TypeBasic:
            {
            CHttpAuthBasic *lpAuthBasic = (CHttpAuthBasic*)(cHttpAuth.Get());

            hRes = lpAuthBasic->MakeAuthenticateResponse(cStrTempA, (LPCWSTR)cStrAuthUserNameW,
                                                         (LPCWSTR)cStrAuthUserPasswordW, FALSE);
            if (FAILED(hRes))
              return hRes;
            if (cStrReqHdrsA.ConcatN((LPCSTR)cStrTempA, cStrTempA.GetLength()) == FALSE)
              return E_OUTOFMEMORY;
            }
            break;

          case CHttpAuthBase::TypeDigest:
            {
            CHttpAuthDigest *lpAuthDigest = (CHttpAuthDigest*)(cHttpAuth.Get());
            CStringA cStrPathA;
            LPCSTR szMethodA;

            if (sRequest.cStrMethodA.IsEmpty() != FALSE)
            {
              szMethodA = (sRequest.sPostData.cList.IsEmpty() == FALSE ||
                           sRequest.sPostData.nDynamicFlags != 0) ? "POST" : "GET";
            }
            else
            {
              szMethodA = (LPCSTR)(sRequest.cStrMethodA);
            }

            hRes = sRequest.cUrl.ToString(cStrPathA, CUrl::ToStringAddPath);
            if (FAILED(hRes))
              return hRes;

            hRes = lpAuthDigest->MakeAuthenticateResponse(cStrTempA, (LPCWSTR)cStrAuthUserNameW,
                                                          (LPCWSTR)cStrAuthUserPasswordW, szMethodA, (LPCSTR)cStrPathA,
                                                          FALSE);
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
  nCount = sRequest.cHeaders.GetCount();
  for (nIndex = 0; nIndex < nCount; nIndex++)
  {
    LPCSTR szNameA;

    lpHeader = sRequest.cHeaders.GetElementAt(nIndex);
    szNameA = lpHeader->GetHeaderName();

    switch (*szNameA)
    {
      case 'A':
      case 'a':
        if (StrCompareA(szNameA, "Accept-Encoding", TRUE) != 0)
          goto add_header;
        break;

      case 'C':
      case 'c':
        if (StrCompareA(szNameA, "Connection", TRUE) != 0 &&
            StrCompareA(szNameA, "Cookie", TRUE) != 0 &&
            StrCompareA(szNameA, "Cookie2", TRUE) != 0)
        {
          goto add_header;
        }
        break;

      case 'E':
      case 'e':
        if (StrCompareA(szNameA, "Expect", TRUE) != 0)
          goto add_header;
        break;

      case 'H':
      case 'h':
        if (StrCompareA(szNameA, "Host", TRUE) != 0)
          goto add_header;
        break;

      case 'P':
      case 'p':
        if (StrCompareA(szNameA, "Proxy-Connection", TRUE) != 0)
          goto add_header;
        break;

      case 'U':
      case 'u':
        if (StrCompareA(szNameA, "Upgrade-Insecure-Requests", TRUE) != 0)
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

  //13) Cookies: ????
  cStrTempA.Empty();
  nCount = sRequest.cCookies.GetCount();
  for (nIndex = 0; nIndex < nCount; nIndex++)
  {
    lpCookie = sRequest.cCookies.GetElementAt(nIndex);

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
                                           _In_z_ LPCSTR szDefaultValueA, _In_ MX::Http::eBrowser nBrowser)
{
  CStringA cStrTempA;
  LPCSTR szValueA;
  SIZE_T nIndex;
  HRESULT hRes;

  szValueA = szDefaultValueA;

  nIndex = sRequest.cHeaders.Find(szNameA);
  if (nIndex != (SIZE_T)-1)
  {
    hRes = sRequest.cHeaders.GetElementAt(nIndex)->Build(cStrTempA, nBrowser);
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
  //done
  return S_OK;
}

HRESULT CHttpClient::AddRequestHeadersForBody(_Inout_ CStringA &cStrReqHdrsA)
{
  CLnkLst::Iterator it;
  CLnkLstNode *lpNode;
  BOOL bHasContentLengthHeader, bHasContentTypeHeader;
  ULONGLONG nLen;

  bHasContentLengthHeader = (sRequest.cHeaders.Find<CHttpHeaderEntContentLength>() != NULL) ? TRUE : FALSE;
  bHasContentTypeHeader = (sRequest.cHeaders.Find<CHttpHeaderEntContentType>() != NULL) ? TRUE : FALSE;

  nLen = 0ui64;
  if (sRequest.sPostData.cList.IsEmpty() == FALSE && sRequest.sPostData.bHasRaw != FALSE)
  {
    if (bHasContentLengthHeader == FALSE)
    {
      for (lpNode = it.Begin(sRequest.sPostData.cList); lpNode != NULL; lpNode = it.Next())
      {
        CPostDataItem *lpPostDataItem = CONTAINING_RECORD(lpNode, CPostDataItem, cListNode);

        nLen += lpPostDataItem->cStream->GetLength();
      }
      //add content length
      if (cStrReqHdrsA.AppendFormat("Content-Length: %I64u\r\n", nLen) == FALSE)
        return E_OUTOFMEMORY;
    }

    sRequest.bUsingMultiPartFormData = FALSE;
  }
  else
  {
    for (lpNode = it.Begin(sRequest.sPostData.cList); lpNode != NULL; lpNode = it.Next())
    {
      CPostDataItem *lpPostDataItem = CONTAINING_RECORD(lpNode, CPostDataItem, cListNode);

      if (lpPostDataItem->cStream)
      {
        nLen = ULONGLONG_MAX;
        break;
      }
      if (nLen > 0ui64)
        nLen++; //size of '&'
      nLen += (ULONGLONG)CUrl::GetEncodedLength(lpPostDataItem->szNameA); //size of encoded name
      nLen++; //size of '='
      nLen += (ULONGLONG)CUrl::GetEncodedLength(lpPostDataItem->szValueA); //size of encoded value
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
        for (lpNode = it.Begin(sRequest.sPostData.cList); lpNode != NULL; lpNode = it.Next())
        {
          CPostDataItem *lpPostDataItem = CONTAINING_RECORD(lpNode, CPostDataItem, cListNode);

          nLen += 4ui64 + 27ui64 + 2ui64; //size of boundary start
          nLen += 38ui64; //size of 'Content-Disposition: form-data; name="'
          nLen += (ULONGLONG)StrLenA(lpPostDataItem->szNameA);
          nLen += 1ui64; //size of '"'
          if (lpPostDataItem->cStream)
          {
            nLen += 12ui64; //size of '; filename="'
            nLen += (ULONGLONG)StrLenA(lpPostDataItem->szValueA); //size of filename
            nLen += 3ui64; //size of '"\r\n'
            nLen += 14ui64; //size of 'Content-type: '
            nLen += (ULONGLONG)StrLenA(Http::GetMimeType(lpPostDataItem->szValueA)); //size of mime type
            nLen += 2ui64; //size of '"\r\n'
            nLen += 33ui64; //size of 'Content-Transfer-Encoding: binary\r\n'
            nLen += 2ui64; //size of '\r\n'
            nLen += lpPostDataItem->cStream->GetLength(); //size of data
            nLen += 2ui64; //size of '\r\n'
          }
          else
          {
            nLen += 2ui64; //size of '\r\n'
            nLen += 27ui64; //size of 'Content-Type: text/plain;\r\n'
            nLen += 2ui64; //size of '\r\n'
            nLen += (ULONGLONG)StrLenA(lpPostDataItem->szValueA); //size of data
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
  TAutoRefCounted<CHttpAuthBase> cHttpAuth;
  SIZE_T nIndex, nCount;
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
  nCount = sRequest.cHeaders.GetCount();
  for (nIndex = 0; nIndex < nCount; nIndex++)
  {
    lpHeader = sRequest.cHeaders.GetElementAt(nIndex);
    if (StrCompareA(lpHeader->GetHeaderName(), "User-Agent", TRUE) == 0)
    {
      hRes = lpHeader->Build(cStrTempA, Http::BrowserOther);
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
  if (nIndex >= nCount)
  {
    hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "User-Agent", szDefaultUserAgentA, Http::BrowserOther);
    if (FAILED(hRes))
      return hRes;
  }

  //proxy connection
  hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "Proxy-Connection",
                               ((bKeepConnectionOpen != FALSE) ? "Keep-Alive" : "Close"), Http::BrowserOther);
  if (FAILED(hRes))
    return hRes;

  //check if there is a previous authentication for this proxy
  cHttpAuth = HttpAuthCache::Lookup(CUrl::SchemeNone, cProxy.GetAddress(), cProxy.GetPort(), L"/");
  if (cHttpAuth)
  {
    switch (cHttpAuth->GetType())
    {
      case CHttpAuthBase::TypeBasic:
        {
        CHttpAuthBasic *lpAuthBasic = (CHttpAuthBasic*)(cHttpAuth.Get());

        hRes = lpAuthBasic->MakeAuthenticateResponse(cStrTempA, cProxy._GetUserName(), cProxy.GetUserPassword(), TRUE);
        if (FAILED(hRes))
          return hRes;
        if (cStrReqHdrsA.ConcatN((LPCSTR)cStrTempA, cStrTempA.GetLength()) == FALSE)
          return E_OUTOFMEMORY;
        }
        break;

      case CHttpAuthBase::TypeDigest:
        {
        //for proxy the url is the same than the one being requested
        CHttpAuthDigest *lpAuthDigest = (CHttpAuthDigest*)(cHttpAuth.Get());
        CStringA cStrPathA;
        LPCSTR szMethodA;

        if (sRequest.cStrMethodA.IsEmpty() != FALSE)
        {
          szMethodA = (sRequest.sPostData.cList.IsEmpty() == FALSE ||
                        sRequest.sPostData.nDynamicFlags != 0) ? "POST" : "GET";
        }
        else
        {
          szMethodA = (LPCSTR)(sRequest.cStrMethodA);
        }

        hRes = sRequest.cUrl.ToString(cStrPathA, CUrl::ToStringAddPath);
        if (FAILED(hRes))
          return hRes;

        hRes = lpAuthDigest->MakeAuthenticateResponse(cStrTempA, cProxy._GetUserName(), cProxy.GetUserPassword(),
                                                      szMethodA, (LPCSTR)cStrPathA, TRUE);
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
      sRequest.sPostData.nDynamicFlags == 0 && sRequest.sPostData.cList.IsEmpty() == FALSE)
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
  CLnkLst::Iterator it;
  CLnkLstNode *lpNode;
  CPostDataItem *lpPostDataItem;
  CStringA cStrTempA;
  HRESULT hRes;

  if (sRequest.sPostData.nDynamicFlags != 0 ||
      (sRequest.sPostData.cList.IsEmpty() == FALSE && sRequest.sPostData.bHasRaw != FALSE))
  {
    for (lpNode = it.Begin(sRequest.sPostData.cList); lpNode != NULL; lpNode = it.Next())
    {
      lpPostDataItem = CONTAINING_RECORD(lpNode, CPostDataItem, cListNode);

      //send data stream
      hRes = cSocketMgr.SendStream(hConn, lpPostDataItem->cStream.Get());
      if (FAILED(hRes))
        return hRes;
    }

    if ((sRequest.sPostData.nDynamicFlags & _DYNAMIC_FLAG_EndMark) != 0)
    {
      //send end of data stream if not dynamic
      hRes = cSocketMgr.SendMsg(hConn, "\r\n", 2);
      if (FAILED(hRes))
        return hRes;
    }
  }
  else if (sRequest.bUsingMultiPartFormData == FALSE)
  {
    CStringA cStrValueEncA;

    if (cStrTempA.EnsureBuffer(MAX_FORM_SIZE_4_REQUEST + 2) == FALSE)
      return E_OUTOFMEMORY;
    for (lpNode = it.Begin(sRequest.sPostData.cList); lpNode != NULL; lpNode = it.Next())
    {
      CPostDataItem *lpPostDataItem = CONTAINING_RECORD(lpNode, CPostDataItem, cListNode);

      //add separator
      if (cStrTempA.IsEmpty() == FALSE)
      {
        if (cStrTempA.Concat("&") == FALSE)
          return E_OUTOFMEMORY;
      }
      //add encoded name
      hRes = CUrl::Encode(cStrValueEncA, lpPostDataItem->szNameA);
      if (FAILED(hRes))
        return hRes;
      if (cStrTempA.Concat((LPSTR)cStrValueEncA) == FALSE)
        return E_OUTOFMEMORY;
      //add equal sign
      if (cStrTempA.Concat("=") == FALSE)
        return E_OUTOFMEMORY;
      //add encoded value
      hRes = CUrl::Encode(cStrValueEncA, lpPostDataItem->szValueA);
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
    for (lpNode = it.Begin(sRequest.sPostData.cList); lpNode != NULL; lpNode = it.Next())
    {
      lpPostDataItem = CONTAINING_RECORD(lpNode, CPostDataItem, cListNode);

      //add boundary start and content disposition
      if (cStrTempA.Format("----%s\r\nContent-Disposition: form-data; name=\"%s\"", sRequest.szBoundaryA,
                           lpPostDataItem->szNameA) == FALSE)
        return E_OUTOFMEMORY;

      if (lpPostDataItem->cStream)
      {
        //add filename, content type and content transfer encoding
        if (cStrTempA.AppendFormat("; filename=\"%s\"\r\nContent-Type: %s;\r\nContent-Transfer-Encoding: binary\r\n"
                                   "\r\n", lpPostDataItem->szValueA,
                                   Http::GetMimeType(lpPostDataItem->szValueA)) == FALSE)
        {
          return E_OUTOFMEMORY;
        }
        //send
        hRes = cSocketMgr.SendMsg(hConn, (LPSTR)cStrTempA, cStrTempA.GetLength());
        if (FAILED(hRes))
          return hRes;

        //send data stream
        hRes = cSocketMgr.SendStream(hConn, lpPostDataItem->cStream.Get());
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
            cStrTempA.Concat(lpPostDataItem->szValueA) == FALSE ||
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

  //remove post data to free some memory
  while ((lpNode = sRequest.sPostData.cList.PopHead()) != NULL)
  {
    lpPostDataItem = CONTAINING_RECORD(lpNode, CPostDataItem, cListNode);

    delete lpPostDataItem;
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

  if (sResponse.cStrDownloadFileNameW.IsEmpty() != FALSE)
  {
    SIZE_T nLen;
    DWORD dw;
    Fnv64_t nHash;

    //generate filename
    if (cStrTemporaryFolderW.IsEmpty() != FALSE)
    {
      if (_GetTempPath(cStrFileNameW) == FALSE)
        return E_OUTOFMEMORY;
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
  CLnkLstNode *lpNode;

  while ((lpNode = sRequest.sPostData.cList.PopHead()) != NULL)
  {
    CPostDataItem *lpPostDataItem = CONTAINING_RECORD(lpNode, CPostDataItem, cListNode);

    delete lpPostDataItem;
  }
  sRequest.sPostData.bHasRaw = FALSE;
  sRequest.sPostData.nDynamicFlags = 0;
  sRequest.cWebSocket.Release();
  return;
}

VOID CHttpClient::ResetResponseForNewRequest()
{
  sResponse.cParser.Reset();
  sResponse.cStrDownloadFileNameW.Empty();
  return;
}

HRESULT CHttpClient::SetupIgnoreBody()
{
  TAutoRefCounted<CHttpBodyParserBase> cBodyParser;

  cBodyParser.Attach(MX_DEBUG_NEW CHttpBodyParserIgnore());
  if (!cBodyParser)
    return E_OUTOFMEMORY;
  return sResponse.cParser.SetBodyParser(cBodyParser);
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CHttpClient::CPostDataItem::CPostDataItem(_In_z_ LPCSTR _szNameA, _In_z_ LPCSTR _szValueA) : CBaseMemObj(),
                                                                                             CNonCopyableObj()
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

CHttpClient::CPostDataItem::CPostDataItem(_In_z_ LPCSTR _szNameA, _In_z_ LPCSTR _szFileNameA,
                                          _In_ CStream *lpStream) : CBaseMemObj(), CNonCopyableObj()
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

CHttpClient::CPostDataItem::CPostDataItem(_In_ CStream *lpStream) : CBaseMemObj(), CNonCopyableObj()
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

//-----------------------------------------------------------

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
