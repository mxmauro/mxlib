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
#include "..\..\Include\Http\Url.h"
#include "..\..\Include\Http\HttpCommon.h"
#include "..\..\Include\Http\HttpBodyParserJSON.h"
#include "..\..\Include\WaitableObjects.h"
#include "..\..\Include\Comm\IpcCommon.h"
#include "..\..\Include\TimedEvent.h"

//-----------------------------------------------------------

#define MAX_FORM_SIZE_4_REQUEST                 (128 * 1024)
#define DEFAULT_MAX_REDIRECTIONS_COUNT                    10

#define _HTTP_STATUS_SwitchingProtocols                  101
#define _HTTP_STATUS_MovedPermanently                    301
#define _HTTP_STATUS_MovedTemporarily                    302
#define _HTTP_STATUS_SeeOther                            303
#define _HTTP_STATUS_UseProxy                            305
#define _HTTP_STATUS_TemporaryRedirect                   307
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

CHttpClient::CHttpClient(_In_ CSockets &_cSocketMgr, _In_opt_ CLoggable *lpLogParent) :
                         TRefCounted<CBaseMemObj>(), CLoggable(), CNonCopyableObj(),
                         cSocketMgr(_cSocketMgr), dwMaxRedirCount(DEFAULT_MAX_REDIRECTIONS_COUNT)
{
  SetLogParent(lpLogParent);
  sResponse.cParser.SetLogParent(this);
  //----
  cHeadersReceivedCallback = NullCallback();
  cDymanicRequestBodyStartCallback = NullCallback();
  cDocumentCompletedCallback = NullCallback();
  cWebSocketHandshakeCompletedCallback = NullCallback();
  cQueryCertificatesCallback = NullCallback();
  cConnectionCreatedCallback = NullCallback();
  cConnectionDestroyedCallback = NullCallback();
  //----
  sResponse.aMsgBuf.Attach((LPBYTE)MX_MALLOC(MSG_BUFFER_SIZE));
  return;
}

CHttpClient::~CHttpClient()
{
  TAutoRefCounted<CConnection> cOrigConn;
  CLnkLstNode *lpNode;

  //clear timeouts
  MX::TimedEvent::Clear(&(sRequest.nTimeoutTimerId));
  MX::TimedEvent::Clear(&(sRedirection.nTimerId));

  //close and release current connection
  {
    CAutoSlimRWLExclusive cLock(&(sConnection.sRwMutex));

    cOrigConn.Attach(sConnection.cLink.Detach());
  }

  if (cOrigConn)
    cOrigConn->Close(MX_E_Cancelled);

  //delete request's post data
  while ((lpNode = sRequest.sPostData.cList.PopHead()) != NULL)
  {
    CPostDataItem *lpPostDataItem = CONTAINING_RECORD(lpNode, CPostDataItem, cListNode);

    delete lpPostDataItem;
  }
  return;
}

VOID CHttpClient::SetOption_Timeout(_In_ DWORD _dwTimeoutMs)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == eState::Closed)
  {
    dwTimeoutMs = _dwTimeoutMs;
  }
  return;
}

VOID CHttpClient::SetOption_MaxRedirectionsCount(_In_ DWORD dwCount)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == eState::Closed)
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

  if (nState == eState::Closed)
  {
    sResponse.cParser.SetOption_MaxHeaderSize(dwSize);
  }
  return;
}

VOID CHttpClient::SetOption_MaxFieldSize(_In_ DWORD dwSize)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == eState::Closed)
  {
    dwMaxFieldSize = dwSize;
  }
  return;
}

VOID CHttpClient::SetOption_MaxFileSize(_In_ ULONGLONG ullSize)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == eState::Closed)
  {
    ullMaxFileSize = ullSize;
  }
  return;
}

VOID CHttpClient::SetOption_MaxFilesCount(_In_ DWORD dwCount)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == eState::Closed)
  {
    dwMaxFilesCount = dwCount;
  }
  return;
}

BOOL CHttpClient::SetOption_TemporaryFolder(_In_opt_z_ LPCWSTR szFolderW)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == eState::Closed)
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

  if (nState == eState::Closed)
  {
    dwMaxBodySizeInMemory = dwSize;
  }
  return;
}

VOID CHttpClient::SetOption_MaxBodySize(_In_ ULONGLONG ullSize)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == eState::Closed)
  {
    ullMaxBodySize = ullSize;
  }
  return;
}

VOID CHttpClient::SetOption_MaxRawRequestBodySizeInMemory(_In_ DWORD dwSize)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == eState::Closed)
  {
    dwMaxRawRequestBodySizeInMemory = dwSize;
  }
  return;
}

VOID CHttpClient::SetOption_KeepConnectionOpen(_In_ BOOL bKeep)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == eState::Closed)
  {
    bKeepConnectionOpen = bKeep;
  }
  return;
}

VOID CHttpClient::SetOption_AcceptCompressedContent(_In_ BOOL bAccept)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState == eState::Closed)
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

VOID CHttpClient::SetConnectionCreatedCallback(_In_ OnConnectionCreatedCallback _cConnectionCreatedCallback)
{
  cConnectionCreatedCallback = _cConnectionCreatedCallback;
  return;
}

VOID CHttpClient::SetConnectionDestroyedCallback(_In_ OnConnectionDestroyedCallback _cConnectionDestroyedCallback)
{
  cConnectionDestroyedCallback = _cConnectionDestroyedCallback;
  return;
}

HRESULT CHttpClient::SetRequestMethodAuto()
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != eState::Closed)
    return MX_E_NotReady;
  sRequest.cStrMethodA.Empty();
  return S_OK;
}

HRESULT CHttpClient::SetRequestMethod(_In_z_ LPCSTR szMethodA)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != eState::Closed)
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

  if (lpHeader == NULL)
    return E_POINTER;

  //add to list
  if (sRequest.cHeaders.AddElement(lpHeader) == FALSE)
    return E_OUTOFMEMORY;
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
  if (nState != eState::Closed && nState != eState::DocumentCompleted)
    return MX_E_NotReady;

  //create and add to list
  hRes = CHttpHeaderBase::Create(szNameA, TRUE, &cNewHeader);
  if (FAILED(hRes))
    return hRes;
  if (sRequest.cHeaders.AddElement(cNewHeader.Get()) == FALSE)
    return E_OUTOFMEMORY;
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
  if (nState != eState::Closed && nState != eState::DocumentCompleted)
    return MX_E_NotReady;

  //create and add to list
  hRes = CHttpHeaderBase::Create(szNameA, TRUE, &cNewHeader);
  if (SUCCEEDED(hRes))
    hRes = cNewHeader->Parse(szValueA, nValueLen);
  if (FAILED(hRes))
    return hRes;
  if (sRequest.cHeaders.AddElement(cNewHeader.Get()) == FALSE)
    return E_OUTOFMEMORY;
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
  if (nState != eState::Closed && nState != eState::DocumentCompleted)
    return MX_E_NotReady;
  
  //create and add to list
  hRes = CHttpHeaderBase::Create(szNameA, TRUE, &cNewHeader);
  if (SUCCEEDED(hRes))
    hRes = cNewHeader->Parse(szValueW, nValueLen);
  if (FAILED(hRes))
    return hRes;
  if (sRequest.cHeaders.AddElement(cNewHeader.Get()) == FALSE)
    return E_OUTOFMEMORY;
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

  if (nState != eState::Closed && nState != eState::DocumentCompleted)
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

  if (nState != eState::Closed && nState != eState::DocumentCompleted)
    return MX_E_NotReady;

  sRequest.cHeaders.RemoveAllElements();
  return S_OK;
}

SIZE_T CHttpClient::GetRequestHeadersCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));

  if (nState != eState::Closed && nState != eState::DocumentCompleted)
    return MX_E_NotReady;
  return sRequest.cHeaders.GetCount();
}

CHttpHeaderBase* CHttpClient::GetRequestHeader(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));
  CHttpHeaderBase *lpHeader;

  if (nState != eState::Closed && nState != eState::DocumentCompleted)
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

  if (nState != eState::Closed && nState != eState::DocumentCompleted)
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

  if (nState != eState::Closed)
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

  if (nState != eState::Closed)
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

  if (nState != eState::Closed)
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

  if (nState != eState::Closed)
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

  if (nState != eState::Closed)
    return MX_E_NotReady;
  sRequest.cCookies.RemoveAllElements();
  return S_OK;
}

SIZE_T CHttpClient::GetRequestCookiesCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));

  if (nState != eState::Closed)
    return 0;
  return sRequest.cCookies.GetCount();
}

CHttpCookie* CHttpClient::GetRequestCookie(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));
  CHttpCookie *lpCookie;

  if (nState != eState::Closed)
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

  if (nState != eState::Closed)
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

  if (nState != eState::Closed)
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

  if (nState != eState::Closed)
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

  if (nState != eState::Closed)
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

  if (nState != eState::Closed && nState != eState::SendingDynamicRequestBody && nState != eState::WaitingForRedirection &&
      nState != eState::EstablishingProxyTunnelConnection && nState != eState::WaitingProxyTunnelConnectionResponse)
  {
    return MX_E_NotReady;
  }
  if (nLength == 0)
    return S_OK;
  if (lpData == NULL)
    return E_POINTER;

  if (nState != eState::SendingDynamicRequestBody)
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
    TAutoRefCounted<CConnection> cConnection;

    cConnection.Attach(GetConnection());
    hRes = (cConnection) ? cConnection->SendMsg(lpData, nLength) : MX_E_NotReady;
  }

  //done
  return hRes;
}

HRESULT CHttpClient::AddRequestRawPostData(_In_ CStream *lpStream)
{
  TAutoRefCounted<CConnection> cConnection;
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != eState::Closed && nState != eState::SendingDynamicRequestBody)
    return MX_E_NotReady;
  if (lpStream == NULL)
    return E_POINTER;
  if (nState == eState::Closed)
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

    cConnection.Attach(GetConnection());
    hRes = (cConnection) ? cConnection->SendStream(lpStream) : MX_E_NotReady;
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

  if (nState != eState::Closed)
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

  if (nState != eState::Closed)
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

  if (nState != eState::Closed)
    return MX_E_NotReady;
  if (sRequest.sPostData.nDynamicFlags != 0)
    return MX_E_InvalidState;
  sRequest.sPostData.nDynamicFlags = _DYNAMIC_FLAG_IsActive;
  return S_OK;
}

HRESULT CHttpClient::SignalEndOfPostData()
{
  TAutoRefCounted<CConnection> cConnection;
  CCriticalSection::CAutoLock cLock(cMutex);
  HRESULT hRes;

  if (nState != eState::Closed && nState != eState::SendingDynamicRequestBody && nState != eState::WaitingForRedirection &&
      nState != eState::EstablishingProxyTunnelConnection && nState != eState::WaitingProxyTunnelConnectionResponse)
  {
    return MX_E_NotReady;
  }
  if (sRequest.sPostData.nDynamicFlags != _DYNAMIC_FLAG_IsActive)
    return MX_E_InvalidState;

  if (nState == eState::SendingDynamicRequestBody)
  {
    cConnection.Attach(GetConnection());
    if (!cConnection)
      return MX_E_NotReady;

    //send end of body mark
    hRes = cConnection->SendMsg("\r\n", 2);
    if (FAILED(hRes))
    {
      SetErrorOnRequestAndClose(hRes);
      return hRes;
    }

    nState = eState::ReceivingResponseHeaders;
  }

  sRequest.sPostData.nDynamicFlags |= _DYNAMIC_FLAG_EndMark;

  //done
  return S_OK;
}

HRESULT CHttpClient::SetProxy(_In_ CProxy &_cProxy)
{
  CCriticalSection::CAutoLock cLock(cMutex);

  if (nState != eState::Closed)
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

HRESULT CHttpClient::Open(_In_ CUrl &cUrl, _In_opt_ LPOPEN_OPTIONS lpOptions)
{
  TAutoRefCounted<CConnection> cConnToClose;
  HRESULT hRes;

  {
    CCriticalSection::CAutoLock cLock(cMutex);

    hRes = InternalOpen(cUrl, lpOptions, FALSE, &cConnToClose);
  }

  if (cConnToClose)
    cConnToClose->Close(MX_E_Cancelled);
  return hRes;
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
  TAutoRefCounted<CConnection> cConnToClose;
  LONG nTid;
  BOOL bLocked;

  {
    CCriticalSection::CAutoLock cLock(cMutex);

    if (nState != eState::DocumentCompleted || bReuseConn == FALSE)
    {
      CAutoSlimRWLExclusive cLock(&(sConnection.sRwMutex));

      cConnToClose.Attach(sConnection.cLink.Detach());
    }

    ResetRequestForNewRequest();
    ResetResponseForNewRequest();
    nState = eState::Closed;
  }

  //clear timeouts
  MX::TimedEvent::Clear(&(sRedirection.nTimerId));
  MX::TimedEvent::Clear(&(sRequest.nTimeoutTimerId));

  bLocked = LockThreadCall(TRUE);
  cHeadersReceivedCallback = NullCallback();
  cDymanicRequestBodyStartCallback = NullCallback();
  cDocumentCompletedCallback = NullCallback();
  cWebSocketHandshakeCompletedCallback = NullCallback();
  cQueryCertificatesCallback = NullCallback();
  if (bLocked != FALSE)
    _InterlockedExchange(&nCallInProgressThread, 0);

  nTid = (LONG)::GetCurrentThreadId();
  for (;;)
  {
    LONG nCurrTid = __InterlockedRead(&nCallInProgressThread);
    if (nCurrTid == 0 || nCurrTid == nTid)
      break;
    ::MxSleep(25);
  }

  //done
  if (cConnToClose)
    cConnToClose->Close(MX_E_Cancelled);
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

  return (nState == eState::ReceivingResponseBody || nState == eState::DocumentCompleted) ? TRUE : FALSE;
}

BOOL CHttpClient::IsDocumentComplete() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));

  return (nState == eState::DocumentCompleted) ? TRUE : FALSE;
}

BOOL CHttpClient::IsClosed() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));

  return (nState == eState::Closed) ? TRUE : FALSE;
}

LONG CHttpClient::GetResponseStatus() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));

  if (nState != eState::AfterHeaders && nState != eState::ReceivingResponseBody && nState != eState::DocumentCompleted)
    return 0;
  return sResponse.cParser.GetResponseStatus();
}

HRESULT CHttpClient::GetResponseReason(_Inout_ CStringA &cStrDestA) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));

  cStrDestA.Empty();
  if (nState != eState::AfterHeaders && nState != eState::ReceivingResponseBody && nState != eState::DocumentCompleted)
    return MX_E_NotReady;
  return (cStrDestA.Copy(sResponse.cParser.GetResponseReasonA()) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

CHttpBodyParserBase* CHttpClient::GetResponseBodyParser() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));

  if (nState != eState::ReceivingResponseBody && nState != eState::DocumentCompleted)
    return NULL;
  return sResponse.cParser.GetBodyParser();
}

SIZE_T CHttpClient::GetResponseHeadersCount() const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));

  if (nState != eState::AfterHeaders && nState != eState::ReceivingResponseBody && nState != eState::DocumentCompleted)
    return 0;
  return sResponse.cParser.Headers().GetCount();
}

CHttpHeaderBase* CHttpClient::GetResponseHeader(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection&>(cMutex));
  CHttpHeaderArray *lpHeadersArray;
  CHttpHeaderBase *lpHeader;

  if (nState != eState::AfterHeaders && nState != eState::ReceivingResponseBody && nState != eState::DocumentCompleted)
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

  if (nState != eState::AfterHeaders && nState != eState::ReceivingResponseBody && nState != eState::DocumentCompleted)
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

  if (nState != eState::AfterHeaders && nState != eState::ReceivingResponseBody && nState != eState::DocumentCompleted)
    return 0;
  return sResponse.cParser.Cookies().GetCount();
}

CHttpCookie* CHttpClient::GetResponseCookie(_In_ SIZE_T nIndex) const
{
  CCriticalSection::CAutoLock cLock(const_cast<CCriticalSection &>(cMutex));
  CHttpCookieArray *lpCookieArray;
  CHttpCookie *lpCookie;

  if (nState != eState::AfterHeaders && nState != eState::ReceivingResponseBody && nState != eState::DocumentCompleted)
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

  if (nState != eState::AfterHeaders && nState != eState::ReceivingResponseBody && nState != eState::DocumentCompleted)
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

  if (nState != eState::AfterHeaders && nState != eState::ReceivingResponseBody && nState != eState::DocumentCompleted)
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
  TAutoRefCounted<CConnection> cConnection;

  cConnection.Attach(const_cast<CHttpClient*>(this)->GetConnection());
  return (cConnection) ? cConnection->GetConn() : NULL;
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

HRESULT CHttpClient::InternalOpen(_In_ CUrl &cUrl, _In_opt_ LPOPEN_OPTIONS lpOptions, _In_ BOOL bIsRedirecting,
                                  _Deref_out_opt_ CConnection **lplpConnectionToRelease)
{
  TAutoRefCounted<CConnection> cConnection;
  CStringA cStrHostA;
  LPCWSTR szConnectHostW;
  int nUrlPort, nConnectPort;
  eState nOrigState;
  HRESULT hRes;

  MX_ASSERT(lplpConnectionToRelease != NULL);
  *lplpConnectionToRelease = NULL;

  if (!(sResponse.aMsgBuf))
    return E_OUTOFMEMORY;

  if (cUrl.GetSchemeCode() != CUrl::eScheme::Http && cUrl.GetSchemeCode() != CUrl::eScheme::Https &&
      cUrl.GetSchemeCode() != CUrl::eScheme::WebSocket && cUrl.GetSchemeCode() != CUrl::eScheme::SecureWebSocket)
  {
    return E_INVALIDARG;
  }
  if (cUrl.GetHost()[0] == 0)
    return E_INVALIDARG;

  if (bIsRedirecting == FALSE)
  {
    sRequest.cWebSocket.Release();
    sRequest.cLocalIpHeader.Release();
    sRedirection.dwCounter = 0;
  }

  //check WebSocket options
  if (bIsRedirecting == FALSE &&
      (cUrl.GetSchemeCode() == CUrl::eScheme::WebSocket || cUrl.GetSchemeCode() == CUrl::eScheme::SecureWebSocket))
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
    if (FAILED(hRes))
      return hRes;
    if (sRequest.cHeaders.AddElement(cReqSecWebSocketVersion.Get()) == FALSE)
      return E_OUTOFMEMORY;
    cReqSecWebSocketVersion.Detach();

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
      if (sRequest.cHeaders.AddElement(cReqSecWebSocketProtocol.Get()) == FALSE)
        return E_OUTOFMEMORY;
      cReqSecWebSocketProtocol.Detach();
    }

    //key
    cReqSecWebSocketKey.Attach(MX_DEBUG_NEW CHttpHeaderReqSecWebSocketKey());
    if (!cReqSecWebSocketKey)
      return E_OUTOFMEMORY;
    hRes = cReqSecWebSocketKey->GenerateKey(16);
    if (FAILED(hRes))
      return hRes;
    if (sRequest.cHeaders.AddElement(cReqSecWebSocketKey.Get()) == FALSE)
      return E_OUTOFMEMORY;
    cReqSecWebSocketKey.Detach();

    //upgrade
    cGenUpgrade.Attach(MX_DEBUG_NEW CHttpHeaderGenUpgrade());
    if (!cGenUpgrade)
      return E_OUTOFMEMORY;
    hRes = cGenUpgrade->AddProduct("websocket");
    if (FAILED(hRes))
      return hRes;
    if (sRequest.cHeaders.AddElement(cGenUpgrade.Get()) == FALSE)
      return E_OUTOFMEMORY;
    cGenUpgrade.Detach();

    //connection
    cGenConnection.Attach(MX_DEBUG_NEW CHttpHeaderGenConnection());
    if (!cGenConnection)
      return E_OUTOFMEMORY;
    hRes = cGenConnection->AddConnection("Upgrade");
    if (FAILED(hRes))
      return hRes;
    if (sRequest.cHeaders.AddElement(cGenConnection.Get()) == FALSE)
      return E_OUTOFMEMORY;
    cGenConnection.Detach();
  }

  if (lpOptions != NULL && lpOptions->sSendLocalIP.szHeaderNameA != NULL &&
      *(lpOptions->sSendLocalIP.szHeaderNameA) != 0)
  {
    TAutoRefCounted<CHttpHeaderGeneric> cHeaderGeneric;

    cHeaderGeneric.Attach(MX_DEBUG_NEW CHttpHeaderGeneric());
    if (!cHeaderGeneric)
      return E_OUTOFMEMORY;
    hRes = cHeaderGeneric->SetHeaderName(lpOptions->sSendLocalIP.szHeaderNameA);
    if (FAILED(hRes))
      return hRes;
    if (sRequest.cHeaders.AddElement(cHeaderGeneric.Get()) == FALSE)
      return E_OUTOFMEMORY;
    sRequest.cLocalIpHeader = cHeaderGeneric.Detach();
  }

  //get port
  if (cUrl.GetPort() >= 0)
    nUrlPort = cUrl.GetPort();
  else if (cUrl.GetSchemeCode() == CUrl::eScheme::Https || cUrl.GetSchemeCode() == CUrl::eScheme::SecureWebSocket)
    nUrlPort = 443;
  else
    nUrlPort = 80;

  if (cProxy.GetType() != CProxy::eType::None)
  {
    hRes = cProxy.Resolve(cUrl);
    if (hRes == E_OUTOFMEMORY)
      return hRes;
  }

  //clear timeouts
  nOrigState = nState;
  nState = eState::NoOp;
  cMutex.Unlock();
  MX::TimedEvent::Clear(&(sRedirection.nTimerId));
  MX::TimedEvent::Clear(&(sRequest.nTimeoutTimerId));
  cMutex.Lock();
  nState = nOrigState;

  ResetResponseForNewRequest();

  //check where we should connect
  szConnectHostW = NULL;
  nConnectPort = 0;
  if (cProxy.GetType() != CProxy::eType::None)
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
  if (sResponse.cParser.GetState() != Internals::CHttpParser::eState::Done ||
      StrCompareW(sRequest.cUrl.GetScheme(), cUrl.GetScheme()) != 0 ||
      StrCompareW(sRequest.cUrl.GetHost(), szConnectHostW) != 0 ||
      nUrlPort != nConnectPort)
  {
    CAutoSlimRWLExclusive cLock(&(sConnection.sRwMutex));

    *lplpConnectionToRelease = sConnection.cLink.Detach();
  }
  else
  {
    cConnection.Attach(GetConnection());
  }

  //setup new state
  nState = ((cUrl.GetSchemeCode() == CUrl::eScheme::Https || cUrl.GetSchemeCode() == CUrl::eScheme::SecureWebSocket) &&
            sRequest.bUsingProxy != FALSE) ? eState::EstablishingProxyTunnelConnection : eState::SendingRequestHeaders;
  //create a new connection if needed
  hRes = S_OK;
  if (!cConnection)
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

      cConnection.Attach(MX_DEBUG_NEW CConnection(this));
      {
        CAutoSlimRWLExclusive cLock(&(sConnection.sRwMutex));

        sConnection.cLink = cConnection;
      }
      if (cConnection)
      {
        hRes = cConnection->Connect(CSockets::eFamily::IPv4, szConnectHostW, nConnectPort,
                                    ((sRequest.cUrl.GetSchemeCode() == CUrl::eScheme::Https ||
                                      sRequest.cUrl.GetSchemeCode() == CUrl::eScheme::SecureWebSocket) &&
                                     sRequest.bUsingProxy == FALSE) ? TRUE : FALSE);
        if (FAILED(hRes))
        {
          CAutoSlimRWLExclusive cLock(&(sConnection.sRwMutex));

          sConnection.cLink.Release();
        }
      }
      else
      {
        hRes = E_OUTOFMEMORY;
      }
    }
  }
  else
  {
    GenerateRequestBoundary();

    hRes = OnConnectionEstablished(cConnection.Get());
  }

  //setup timeout timer
  if (SUCCEEDED(hRes) && dwTimeoutMs > 0)
  {
    hRes = MX::TimedEvent::SetTimeout(&(sRequest.nTimeoutTimerId), dwTimeoutMs,
                                      MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnRequestTimeout, this), NULL);
  }

  if (FAILED(hRes))
  {
    nState = eState::Closed;
    return hRes;
  }
  //done
  return S_OK;
}

HRESULT CHttpClient::OnConnectionEstablished(_In_ CConnection *lpConn)
{ 
  TAutoRefCounted<CConnection> cConnection;
  HRESULT hRes = S_OK;

  {
    CCriticalSection::CAutoLock cLock(cMutex);

    //does belong tu us?
    cConnection.Attach(GetConnection());
    if (lpConn != cConnection.Get())
      return MX_E_Cancelled;

    switch (nState)
    {
      case eState::EstablishingProxyTunnelConnection:
        hRes = SendTunnelConnect(cConnection);
        break;

      case eState::SendingRequestHeaders:
        hRes = SendRequestHeaders(cConnection);
        break;
    }
    if (FAILED(hRes))
      SetErrorOnRequestAndClose(hRes);
  }

  //done
  return S_OK;
}

VOID CHttpClient::OnConnectionClosed(_In_ CConnection *lpConn, _In_ HRESULT hrErrorCode)
{
  TAutoRefCounted<CConnection> cConnToClose;
  BOOL bRaiseDocCompletedCallback = FALSE;

  if (cConnectionDestroyedCallback)
  {
    cConnectionDestroyedCallback(lpConn->lpIpc, lpConn->GetConn(), hrErrorCode);
  }

  {
    CCriticalSection::CAutoLock cLock(cMutex);

    {
      CAutoSlimRWLExclusive cLock(&(sConnection.sRwMutex));

      if (lpConn == sConnection.cLink.Get())
      {
        cConnToClose.Attach(sConnection.cLink.Detach());
      }
    }

    if (cConnToClose)
    {
      switch (nState)
      {
        case eState::DocumentCompleted:
          break;

        case eState::AfterHeaders:
          //if we are in this state but parsing is complete, then completion is about to be called
          if (sResponse.cParser.GetState() == Internals::CHttpParser::eState::Done)
            break;
          //fall into the rest

        case eState::WaitingForRedirection:
        case eState::EstablishingProxyTunnelConnection:
        case eState::WaitingProxyTunnelConnectionResponse:
        case eState::SendingRequestHeaders:
        case eState::SendingDynamicRequestBody:
        case eState::ReceivingResponseHeaders:
        case eState::ReceivingResponseBody:
          nState = eState::Closed;
          if (SUCCEEDED(hLastErrorCode)) //preserve first error
            hLastErrorCode = SUCCEEDED(hrErrorCode) ? MX_E_Cancelled : hrErrorCode;
          bRaiseDocCompletedCallback = TRUE;
          break;

        default:
          nState = eState::Closed;
          bRaiseDocCompletedCallback = TRUE;
          break;
      }
    }
  }

  //call callbacks
  if (bRaiseDocCompletedCallback != FALSE)
  {
    //clear timeouts
    MX::TimedEvent::Clear(&(sRedirection.nTimerId));
    MX::TimedEvent::Clear(&(sRequest.nTimeoutTimerId));

    LockThreadCall(FALSE);
    if (cDocumentCompletedCallback)
    {
      cDocumentCompletedCallback(this);
    }
    _InterlockedExchange(&nCallInProgressThread, 0);
  }

  //done
  return;
}

//NOTE: "CIpc" guarantees no simultaneous calls to 'OnSocketDataReceived' will be received from different threads
HRESULT CHttpClient::OnDataReceived(_In_ CConnection *lpConn)
{
  TAutoRefCounted<CConnection> cConnection;
  BOOL bFireResponseHeadersReceivedCallback, bFireDocumentCompleted, bFireWebSocketHandshakeCompletedCallback;
  SIZE_T nMsgSize;
  int nRedirectionTimerAction;
  struct {
    CStringW cStrFileNameW;
    ULONGLONG nContentLength{ 0 }, *lpnContentLength{ NULL };
    CStringA cStrTypeA;
    BOOL bTreatAsAttachment{ FALSE };
    CStringW cStrDownloadFileNameW;
  } sHeadersCallbackData;
  struct {
    TAutoRefCounted<CWebSocket> cWebSocket;
    CStringA cStrProtocolA;
  } sWebSocketHandshakeCallbackData;
  HRESULT hRes;

  nMsgSize = 0;
  nRedirectionTimerAction = 0;

restart:
  bFireResponseHeadersReceivedCallback = bFireDocumentCompleted = bFireWebSocketHandshakeCompletedCallback = FALSE;

  cConnection.Attach(GetConnection());
  //does belong to us?
  if (lpConn != cConnection.Get() || cConnection->IsClosed() != FALSE)
    return MX_E_Cancelled;

  if (nRedirectionTimerAction < 0)
  {
    MX::TimedEvent::Clear(&(sRedirection.nTimerId));
    nRedirectionTimerAction = 0;
  }
  else if (nRedirectionTimerAction > 0)
  {
    MX::TimedEvent::Clear(&(sRedirection.nTimerId));
    hRes = MX::TimedEvent::SetTimeout(&(sRedirection.nTimerId), (DWORD)nRedirectionTimerAction,
                                      MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnRedirection, this), NULL);
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

    //loop
    bBreakLoop = FALSE;
    while (bBreakLoop == FALSE && bFireResponseHeadersReceivedCallback == FALSE && bFireDocumentCompleted == FALSE &&
           bFireWebSocketHandshakeCompletedCallback == FALSE)
    {
      //get buffered message
      nMsgSize = MSG_BUFFER_SIZE;
      hRes = cConnection->GetBufferedMessage(sResponse.aMsgBuf.Get(), &nMsgSize);
      if (SUCCEEDED(hRes))
      {
        if (nMsgSize == 0)
          break;
      }
      else
      {
on_request_error:
        nRedirectionTimerAction = -1;

        SetErrorOnRequestAndClose(hRes);
        goto restart;
      }

      //take action depending on current state
      switch (nState)
      {
        case eState::WaitingProxyTunnelConnectionResponse:
          //process http being received
          hRes = sResponse.cParser.Parse(sResponse.aMsgBuf.Get(), nMsgSize, nMsgUsed);
          if (FAILED(hRes))
            goto on_request_error;
          if (nMsgUsed > 0)
          {
            hRes = cConnection->ConsumeBufferedMessage(nMsgUsed);
            if (FAILED(hRes))
              goto on_request_error;
          }

          //take action if parser's state changed
          nParserState = sResponse.cParser.GetState();

          //check for end
          switch (nParserState)
          {
            case Internals::CHttpParser::eState::BodyStart:
            case Internals::CHttpParser::eState::Done:
              nRespStatus = sResponse.cParser.GetResponseStatus();

              //reset parser
              sResponse.cParser.Reset();
              //can proceed?
              if (nRespStatus == 200)
              {
                //add ssl layer
                hRes = OnAddSslLayer(&cSocketMgr, cConnection->GetConn());
                if (SUCCEEDED(hRes))
                {
                  nState = eState::SendingRequestHeaders;
                  hRes = SendRequestHeaders(cConnection.Get());
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

        case eState::ReceivingResponseHeaders:
        case eState::ReceivingResponseBody:
          //process http being received
          hRes = sResponse.cParser.Parse(sResponse.aMsgBuf.Get(), nMsgSize, nMsgUsed);
          if (FAILED(hRes))
            goto on_request_error;
          if (nMsgUsed > 0)
          {
            hRes = cConnection->ConsumeBufferedMessage(nMsgUsed);
            if (FAILED(hRes))
              goto on_request_error;
          }

          //take action if parser's state changed
          nParserState = sResponse.cParser.GetState();
          switch (nParserState)
          {
            case Internals::CHttpParser::eState::BodyStart:
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
                  sRedirection.dwCounter < dwMaxRedirCount)
              {
                hRes = SetupIgnoreBody();
                if (FAILED(hRes))
                  goto on_request_error;
              }
              else if (nRespStatus == _HTTP_STATUS_SwitchingProtocols && sRequest.cWebSocket)
              {
                goto on_websocket_negotiated;
              }

              bFireResponseHeadersReceivedCallback = TRUE;
              nState = eState::AfterHeaders;
              break;

            case Internals::CHttpParser::eState::Done:
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
                  sRedirection.dwCounter < dwMaxRedirCount)
              {
                CUrl cUrlTemp;
                ULONGLONG nWaitTimeSecs;

                (sRedirection.dwCounter)++;
                nWaitTimeSecs = 0ui64;
                //build new url
                hRes = S_OK;
                try
                {
                  sRedirection.cUrl = sRequest.cUrl;
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
                        hRes = sRedirection.cUrl.Merge(cUrlTemp);
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
                nState = eState::WaitingForRedirection;
                nRedirectionTimerAction = (nWaitTimeSecs > 0) ? ((int)nWaitTimeSecs * 1000) : 1;
                goto restart;
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
                nState = eState::DocumentCompleted;
                break;
              }

              //no redirection or (re)auth
              bFireDocumentCompleted = TRUE;
              if (ShouldLog(1) != FALSE)
              {
                Log(L"HttpClient(DocumentCompleted/0x%p)", this);
              }
              if (nState == eState::ReceivingResponseHeaders)
              {
                bFireResponseHeadersReceivedCallback = TRUE;
                nState = eState::AfterHeaders;
              }
              else
              {
                nState = eState::DocumentCompleted;
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

        case eState::Closed:
          hRes = cConnection->ConsumeBufferedMessage(nMsgSize);
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
      hRes = sWebSocketHandshakeCallbackData.cWebSocket->SetupIpc(&cSocketMgr, cConnection->GetConn(), FALSE);
      if (FAILED(hRes))
        goto on_request_error;

      //the connection handle does not belong to us anymore
      cConnection->Detach();
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

      if (ShouldLog(1) != FALSE)
      {
        Log(L"HttpClient(ResponseHeadersReceived/0x%p)", this);
      }

      //we can free this because not needed anymore
      sRequest.cLocalIpHeader.Release();

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

  if (nRedirectionTimerAction < 0)
  {
    MX::TimedEvent::Clear(&(sRedirection.nTimerId));
    nRedirectionTimerAction = 0;
  }
  else if (nRedirectionTimerAction > 0)
  {
    MX::TimedEvent::Clear(&(sRedirection.nTimerId));
    hRes = MX::TimedEvent::SetTimeout(&(sRedirection.nTimerId), (DWORD)nRedirectionTimerAction,
                                      MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnRedirection, this), NULL);
    if (SUCCEEDED(hRes))
    {
      TAutoRefCounted<CConnection> cTempConnection;

      cTempConnection.Attach(GetConnection());
      if (lpConn != cTempConnection.Get())
      {
        //if the connection is invalidated
        MX::TimedEvent::Clear(&(sRedirection.nTimerId));

        hRes = MX_E_Cancelled;
      }
    }
    if (FAILED(hRes))
    {
      //just a fatal error if we cannot set up the timers
      CCriticalSection::CAutoLock cLock(cMutex);

      SetErrorOnRequestAndClose(hRes);
      return S_OK;
    }
  }

  //have some action to do?
  if (bFireResponseHeadersReceivedCallback != FALSE)
  {
    TAutoRefCounted<CHttpBodyParserBase> cBodyParser;
    BOOL bRestart = FALSE;

    hRes = S_OK;

    LockThreadCall(FALSE);
    if (cHeadersReceivedCallback)
    {
      hRes = cHeadersReceivedCallback(this, (LPCWSTR)(sHeadersCallbackData.cStrFileNameW),
                                      sHeadersCallbackData.lpnContentLength, (LPCSTR)(sHeadersCallbackData.cStrTypeA),
                                      sHeadersCallbackData.bTreatAsAttachment,
                                      sHeadersCallbackData.cStrDownloadFileNameW, &cBodyParser);
    }
    _InterlockedExchange(&nCallInProgressThread, 0);

    {
      CCriticalSection::CAutoLock cLock(cMutex);

      if (lpConn == cConnection.Get() && cConnection->IsClosed() == FALSE && nState == eState::AfterHeaders)
      {
        if (SUCCEEDED(hRes))
        {
          if (bFireDocumentCompleted == FALSE)
          {
            //add a default body if none was added
            if (!cBodyParser)
            {
              if (sResponse.cStrDownloadFileNameW.IsEmpty() == FALSE)
              {
                cBodyParser.Attach(MX_DEBUG_NEW CHttpBodyParserDefault(
                                   MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnDownloadStarted, this), NULL, 0,
                                   ULONGLONG_MAX));
              }
              else if (StrCompareA((LPCSTR)(sHeadersCallbackData.cStrTypeA), "application/json") == 0)
              {
                cBodyParser.Attach(MX_DEBUG_NEW CHttpBodyParserJSON());
              }
              else
              {
                cBodyParser.Attach(MX_DEBUG_NEW CHttpBodyParserDefault(
                                   MX_BIND_MEMBER_CALLBACK(&CHttpClient::OnDownloadStarted, this), NULL,
                                   dwMaxBodySizeInMemory, ullMaxBodySize));
              }
              if (!cBodyParser)
                hRes = E_OUTOFMEMORY;
            }
            if (SUCCEEDED(hRes))
            {
              hRes = sResponse.cParser.SetBodyParser(cBodyParser.Get());
            }
            if (SUCCEEDED(hRes))
            {
              nState = eState::ReceivingResponseBody;
            }
          }
          else
          {
            nState = eState::DocumentCompleted;
          }
        }

        if (FAILED(hRes))
        {
          nRedirectionTimerAction = -1;

          SetErrorOnRequestAndClose(hRes);
          bRestart = TRUE;
        }
      }
    }

    if (bRestart != FALSE || bFireDocumentCompleted == FALSE)
    {
      goto restart;
    }
  }

  if (bFireWebSocketHandshakeCompletedCallback != FALSE)
  {
    LockThreadCall(FALSE);
    if (cWebSocketHandshakeCompletedCallback)
    {
      cWebSocketHandshakeCompletedCallback(this, sWebSocketHandshakeCallbackData.cWebSocket.Get(),
                                           (LPCSTR)(sWebSocketHandshakeCallbackData.cStrProtocolA));
    }
    sWebSocketHandshakeCallbackData.cWebSocket->FireConnectedAndInitialRead();
    _InterlockedExchange(&nCallInProgressThread, 0);
  }
  else if (bFireDocumentCompleted != FALSE)
  {
    MX::TimedEvent::Clear(&(sRequest.nTimeoutTimerId));

    LockThreadCall(FALSE);
    if (cDocumentCompletedCallback)
    {
      cDocumentCompletedCallback(this);
    }
    _InterlockedExchange(&nCallInProgressThread, 0);
  }

  //done
  return S_OK;
}

HRESULT CHttpClient::OnAddSslLayer(_In_ CIpc *lpIpc, _In_ HANDLE h)
{
  TAutoRefCounted<CSslCertificateArray> cCheckCertificates;
  TAutoRefCounted<CSslCertificate> cSelfCert;
  TAutoRefCounted<CEncryptionKey> cPrivKey;
  CStringA cStrHostNameA;
  HRESULT hRes;

  //query for client certificates
  LockThreadCall(FALSE);
  hRes = (cQueryCertificatesCallback) ? cQueryCertificatesCallback(this, &cCheckCertificates, &cSelfCert, &cPrivKey)
                                      : MX_E_NotReady;
  _InterlockedExchange(&nCallInProgressThread, 0);
  if (FAILED(hRes))
    return hRes;

  //get host name
  hRes = sRequest.cUrl.GetHost(cStrHostNameA);
  if (FAILED(hRes))
    return hRes;

  //add ssl layer
  return lpIpc->InitializeSSL(h, (LPCSTR)cStrHostNameA, cCheckCertificates.Get(), cSelfCert.Get(), cPrivKey.Get(),
                              NULL);
}

VOID CHttpClient::OnRedirection(_In_ LONG nTimerId, _In_ LPVOID lpUserData, _In_opt_ LPBOOL lpbCancel)
{
  TAutoRefCounted<CConnection> cConnToClose;
  HRESULT hRes = S_OK;

  UNREFERENCED_PARAMETER(lpbCancel);

  if (_InterlockedCompareExchange(&(sRedirection.nTimerId), 0, nTimerId) == nTimerId)
  {
    CCriticalSection::CAutoLock cLock(cMutex);

    switch (nState)
    {
      case eState::WaitingForRedirection:
        hRes = InternalOpen(sRedirection.cUrl, NULL, TRUE, &cConnToClose);
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

  if (cConnToClose)
    cConnToClose->Close(MX_E_Cancelled);
  return;
}

VOID CHttpClient::OnAfterSendRequestHeaders(_In_ CConnection *lpConn)
{
  TAutoRefCounted<CConnection> cConnection;
  BOOL bFireDymanicRequestBodyStartCallback = FALSE;
  HRESULT hRes = S_OK;

  cConnection.Attach(GetConnection());

  {
    CCriticalSection::CAutoLock cLock(cMutex);

    if (lpConn == cConnection.Get() && nState == eState::SendingRequestHeaders)
    {
      if (StrCompareA((LPCSTR)(sRequest.cStrMethodA), "HEAD") != 0)
      {
        if (sRequest.sPostData.cList.IsEmpty() == FALSE)
        {
          hRes = SendRequestBody(cConnection.Get());
        }
        if (SUCCEEDED(hRes))
        {
          if (sRequest.sPostData.nDynamicFlags == 0 ||
              sRequest.sPostData.nDynamicFlags == (_DYNAMIC_FLAG_IsActive | _DYNAMIC_FLAG_EndMark))
          {
            //no body to send, directly switch to receiving response headers
            nState = eState::ReceivingResponseHeaders;
          }
          else
          {
            //sending dynamic body
            nState = eState::SendingDynamicRequestBody;

            bFireDymanicRequestBodyStartCallback = TRUE;
          }
        }
      }
      else
      {
        nState = eState::ReceivingResponseHeaders;
      }
    }
    if (FAILED(hRes))
      SetErrorOnRequestAndClose(hRes);
  }
  if (SUCCEEDED(hRes) && bFireDymanicRequestBodyStartCallback != FALSE)
  {
    LockThreadCall(FALSE);
    if (cDymanicRequestBodyStartCallback)
    {
      cDymanicRequestBodyStartCallback(this);
    }
    _InterlockedExchange(&nCallInProgressThread, 0);
  }

  //done
  return;
}

VOID CHttpClient::SetErrorOnRequestAndClose(_In_ HRESULT hrErrorCode)
{
  HANDLE hConnToClose = NULL;

  {
    CAutoSlimRWLShared cLock(&(sConnection.sRwMutex));

    nState = eState::Closed;
    if (SUCCEEDED(hLastErrorCode)) //preserve first error
      hLastErrorCode = hrErrorCode;

    if (sConnection.cLink)
    { 
      hConnToClose = sConnection.cLink->GetConn();
    }
  }

  //do close
  if (hConnToClose != NULL)
    cSocketMgr.Close(hConnToClose, hLastErrorCode);
  return;
}

HRESULT CHttpClient::BuildRequestHeaders(_Inout_ CStringA &cStrReqHdrsA)
{
  TAutoRefCounted<CConnection> cConnection;
  CHttpHeaderBase *lpHeader;
  CStringA cStrTempA;
  CHttpCookie *lpCookie;
  SIZE_T nHdrIndex_Accept, nHdrIndex_AcceptLanguage, nHdrIndex_Referer, nHdrIndex_UserAgent;
  SIZE_T nIndex, nCount;
  Http::eBrowser nBrowser = Http::eBrowser::Other;
  HRESULT hRes;

  cConnection.Attach(GetConnection());
  if (!cConnection)
    return MX_E_Cancelled;

  if (cStrReqHdrsA.EnsureBuffer(32768) == FALSE)
    return E_OUTOFMEMORY;

  nHdrIndex_Accept = (SIZE_T)-1;
  nHdrIndex_AcceptLanguage = (SIZE_T)-1;
  nHdrIndex_Referer = (SIZE_T)-1;
  nHdrIndex_UserAgent = (SIZE_T)-1;

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
  if (sRequest.bUsingProxy != FALSE && sRequest.cUrl.GetSchemeCode() != CUrl::eScheme::Https &&
      sRequest.cUrl.GetSchemeCode() != CUrl::eScheme::SecureWebSocket)
  {
    hRes = sRequest.cUrl.ToString(cStrTempA, CUrl::eToStringFlags::AddAll);
  }
  else
  {
    hRes = sRequest.cUrl.ToString(cStrTempA, CUrl::eToStringFlags::AddPath | CUrl::eToStringFlags::AddQueryStrings);
  }
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
  hRes = sRequest.cUrl.ToString(cStrTempA, CUrl::eToStringFlags::AddHostPort);
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
  if (sRequest.bUsingProxy != FALSE && sRequest.cUrl.GetSchemeCode() != CUrl::eScheme::Http &&
      sRequest.cUrl.GetSchemeCode() != CUrl::eScheme::Https)
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

  //update local IP header if present
  if (sRequest.cLocalIpHeader)
  {
    SOCKADDR_INET sLocalAddr;

    hRes = cSocketMgr.GetLocalAddress(cConnection->GetConn(), &sLocalAddr);
    if (FAILED(hRes))
      return hRes;

    switch (sLocalAddr.si_family)
    {
      case AF_INET:
        if (cStrTempA.Format("%lu.%lu.%lu.%lu", sLocalAddr.Ipv4.sin_addr.S_un.S_un_b.s_b1,
                             sLocalAddr.Ipv4.sin_addr.S_un.S_un_b.s_b2, sLocalAddr.Ipv4.sin_addr.S_un.S_un_b.s_b3,
                             sLocalAddr.Ipv4.sin_addr.S_un.S_un_b.s_b4) == FALSE)
        {
          return E_OUTOFMEMORY;
        }
        break;

      case AF_INET6:
        {
        SIZE_T nIdx;

        if (cStrTempA.CopyN("[", 1) == FALSE)
          return E_OUTOFMEMORY;
        for (nIdx = 0; nIdx < 8; nIdx++)
        {
          if (sLocalAddr.Ipv6.sin6_addr.u.Word[nIdx] == 0)
            break;
          if (nIdx > 0)
          {
            if (cStrTempA.ConcatN(":", 1) == FALSE)
              return E_OUTOFMEMORY;
          }
          if (cStrTempA.AppendFormat("%04X", sLocalAddr.Ipv6.sin6_addr.u.Word[nIdx]) == FALSE)
            return E_OUTOFMEMORY;
        }
        if (nIdx < 8)
        {
          if (cStrTempA.ConcatN("::", 2) == FALSE)
            return E_OUTOFMEMORY;
          while (nIdx < 8 && sLocalAddr.Ipv6.sin6_addr.u.Word[nIdx] == 0)
            nIdx++;
          while (nIdx < 7)
          {
            if (cStrTempA.AppendFormat("%04X:", sLocalAddr.Ipv6.sin6_addr.u.Word[nIdx]) == FALSE)
              return E_OUTOFMEMORY;
            nIdx++;
          }
          if (nIdx < 8)
          {
            if (cStrTempA.AppendFormat("%04X", sLocalAddr.Ipv6.sin6_addr.u.Word[nIdx]) == FALSE)
              return E_OUTOFMEMORY;
          }
        }
        if (cStrTempA.ConcatN("]", 1) == FALSE)
          return E_OUTOFMEMORY;
        }
        break;

      default:
        return MX_E_Unsupported;
    }

    hRes = sRequest.cLocalIpHeader->SetValue((LPCSTR)cStrTempA, cStrTempA.GetLength());
    if (FAILED(hRes))
      return hRes;
  }

  //11) the rest of the headers
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

HRESULT CHttpClient::SendTunnelConnect(_In_ CConnection *lpConnection)
{
  MX::CStringA cStrReqHdrsA, cStrTempA;
  CHttpHeaderBase *lpHeader;
  SIZE_T nIndex, nCount;
  HRESULT hRes;

  hRes = sRequest.cUrl.ToString(cStrTempA, CUrl::eToStringFlags::AddHostPort);
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
      hRes = lpHeader->Build(cStrTempA, Http::eBrowser::Other);
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
    hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "User-Agent", szDefaultUserAgentA, Http::eBrowser::Other);
    if (FAILED(hRes))
      return hRes;
  }

  //proxy connection
  hRes = BuildRequestHeaderAdd(cStrReqHdrsA, "Proxy-Connection",
                               ((bKeepConnectionOpen != FALSE) ? "Keep-Alive" : "Close"), Http::eBrowser::Other);
  if (FAILED(hRes))
    return hRes;

  //send CONNECT
  if (cStrReqHdrsA.ConcatN("\r\n", 2) == FALSE)
    return E_OUTOFMEMORY;
  hRes = lpConnection->SendMsg((LPCSTR)cStrReqHdrsA, cStrReqHdrsA.GetLength());
  if (FAILED(hRes))
    return hRes;

  //done
  nState = eState::WaitingProxyTunnelConnectionResponse;
  return S_OK;
}

HRESULT CHttpClient::SendRequestHeaders(_In_ CConnection *lpConnection)
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
    hRes = lpConnection->SendMsg((LPSTR)cStrReqHdrsA, cStrReqHdrsA.GetLength());
  }
  if (SUCCEEDED(hRes))
  {
    hRes = lpConnection->SendAfterSendRequestHeaders();
  }
  //done
  return hRes;
}

HRESULT CHttpClient::SendRequestBody(_In_ CConnection *lpConnection)
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
      hRes = lpConnection->SendStream(lpPostDataItem->cStream.Get());
      if (FAILED(hRes))
        return hRes;
    }

    if ((sRequest.sPostData.nDynamicFlags & _DYNAMIC_FLAG_IsActive) == 0)
    {
      //send end of data stream if not dynamic
      hRes = lpConnection->SendMsg("\r\n", 2);
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
      if (cStrTempA.ConcatN((LPCSTR)cStrValueEncA, cStrValueEncA.GetLength()) == FALSE)
        return E_OUTOFMEMORY;
      //add equal sign
      if (cStrTempA.Concat("=") == FALSE)
        return E_OUTOFMEMORY;
      //add encoded value
      hRes = CUrl::Encode(cStrValueEncA, lpPostDataItem->szValueA);
      if (FAILED(hRes))
        return hRes;
      if (cStrTempA.Concat((LPCSTR)cStrValueEncA) == FALSE)
        return E_OUTOFMEMORY;
    }

    //include end of body '\r\n'
    if (cStrTempA.Concat("\r\n\r\n") == FALSE)
      return E_OUTOFMEMORY;
    hRes = lpConnection->SendMsg((LPCSTR)cStrTempA, cStrTempA.GetLength());
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
        hRes = lpConnection->SendMsg((LPCSTR)cStrTempA, cStrTempA.GetLength());
        if (FAILED(hRes))
          return hRes;

        //send data stream
        hRes = lpConnection->SendStream(lpPostDataItem->cStream.Get());
        if (FAILED(hRes))
          return hRes;

        //send end of data stream
        hRes = lpConnection->SendMsg("\r\n", 2);
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
        hRes = lpConnection->SendMsg((LPCSTR)cStrTempA, cStrTempA.GetLength());
        if (FAILED(hRes))
          return hRes;
      }
    }

    //send boundary end and end of body '\r\n'
    if (cStrTempA.Format("----%s--\r\n\r\n", sRequest.szBoundaryA) == FALSE)
      return E_OUTOFMEMORY;
    hRes = lpConnection->SendMsg((LPCSTR)cStrTempA, cStrTempA.GetLength());
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
  sRequest.cLocalIpHeader.Release();
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


CHttpClient::CConnection* CHttpClient::GetConnection()
{
  CAutoSlimRWLShared cLock(&(sConnection.sRwMutex));

  if (sConnection.cLink)
  {
    sConnection.cLink->AddRef();
    return sConnection.cLink.Get();
  }
  return NULL;
}

BOOL CHttpClient::LockThreadCall(_In_ BOOL bAllowRecursive)
{
  LONG nTid = (LONG)::GetCurrentThreadId();
  LONG nOrigValue;

  for (;;)
  {
    nOrigValue = _InterlockedCompareExchange(&nCallInProgressThread, nTid, 0);
    if (nOrigValue == 0)
      break;
    if (bAllowRecursive != FALSE && nOrigValue == nTid)
      return FALSE;
    //spin (if close is running on other thread while callback is in progress, set a bigger delay)
    ::MxSleep((bAllowRecursive != FALSE) ? 50 : 5);
  }
  return TRUE;
}

VOID CHttpClient::OnRequestTimeout(_In_ LONG nTimerId, _In_ LPVOID lpUserData, _In_opt_ LPBOOL lpbCancel)
{
  UNREFERENCED_PARAMETER(lpbCancel);

  if (_InterlockedCompareExchange(&(sRequest.nTimeoutTimerId), 0, nTimerId) == nTimerId)
  {
    CCriticalSection::CAutoLock cLock(cMutex);

    if (nState != eState::Closed && nState != eState::DocumentCompleted)
    {
      SetErrorOnRequestAndClose(MX_E_Timeout);
    }
  }

  //done
  return;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CHttpClient::CPostDataItem::CPostDataItem(_In_z_ LPCSTR _szNameA, _In_z_ LPCSTR _szValueA) : CBaseMemObj(),
                                                                                             CNonCopyableObj()
{
  SIZE_T nNameLen, nValueLen;

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

//-----------------------------------------------------------

CHttpClient::CConnection::CConnection(_In_ CHttpClient *_lpHttpClient) : CIpc::CUserData()
{
  SlimRWL_Initialize(&sRwMutex);
  lpHttpClient = _lpHttpClient;
  lpIpc = NULL;
  hConn = NULL;
  bUseSSL = FALSE;
  return;
}

CHttpClient::CConnection::~CConnection()
{
  MX_ASSERT(lpHttpClient == NULL);
  return;
}

BOOL CHttpClient::CConnection::IsClosed() const
{
  CAutoSlimRWLShared cLock(const_cast<LPRWLOCK>(&sRwMutex));

  if (lpIpc != NULL && hConn != NULL)
  {
    if (lpIpc->IsClosed(hConn) == S_FALSE)
      return FALSE;
  }
  return TRUE;
}

HRESULT CHttpClient::CConnection::Connect(_In_ CSockets::eFamily nFamily, _In_z_ LPCWSTR szAddressW, _In_ int nPort,
                                          _In_ BOOL _bUseSSL)
{
  TAutoRefCounted<CHttpClient> cHttpClient;

  cHttpClient.Attach(GetHttpClient());
  if (!cHttpClient)
    return MX_E_Cancelled;

  bUseSSL = _bUseSSL;
  return lpHttpClient->cSocketMgr.ConnectToServer(nFamily, szAddressW, nPort,
                                                  MX_BIND_MEMBER_CALLBACK(&CHttpClient::CConnection::OnSocketCreate,
                                                                          this), this);
}

VOID CHttpClient::CConnection::Close(_In_ HRESULT hrErrorCode)
{
  HANDLE hOrigConn;

  {
    CAutoSlimRWLExclusive cLock(&sRwMutex);

    lpHttpClient = NULL;
    hOrigConn = hConn;
    hConn = NULL;
  }

  if (hOrigConn != NULL)
  {
    lpIpc->Close(hOrigConn, hrErrorCode);
  }
  return;
}

VOID CHttpClient::CConnection::Detach()
{
  CAutoSlimRWLExclusive cLock(&sRwMutex);

  lpHttpClient = NULL;
  hConn = NULL;
  return;
}

HRESULT CHttpClient::CConnection::SendMsg(_In_reads_bytes_(nMsgSize) LPCVOID lpMsg, _In_ SIZE_T nMsgSize)
{
  CAutoSlimRWLShared cLock(&sRwMutex);

  if (lpIpc != NULL && hConn != NULL)
    return lpIpc->SendMsg(hConn, lpMsg, nMsgSize);
  return MX_E_NotReady;
}

HRESULT CHttpClient::CConnection::SendStream(_In_ CStream *lpStream)
{
  CAutoSlimRWLShared cLock(&sRwMutex);

  if (lpIpc != NULL && hConn != NULL)
    return lpIpc->SendStream(hConn, lpStream);
  return MX_E_NotReady;
}

HRESULT CHttpClient::CConnection::SendAfterSendRequestHeaders()
{
  CAutoSlimRWLShared cLock(&sRwMutex);

  if (lpIpc != NULL && hConn != NULL)
  {
    return lpIpc->AfterWriteSignal(hConn, MX_BIND_MEMBER_CALLBACK(&CHttpClient::CConnection::OnAfterSendRequestHeaders,
                                                                  this), NULL);
  }
  return MX_E_NotReady;
}

HRESULT CHttpClient::CConnection::GetBufferedMessage(_Out_ LPVOID lpMsg, _Inout_ SIZE_T *lpnMsgSize)
{
  CAutoSlimRWLShared cLock(&sRwMutex);

  if (lpIpc != NULL && hConn != NULL)
    return lpIpc->GetBufferedMessage(hConn, lpMsg, lpnMsgSize);
  return MX_E_NotReady;
}

HRESULT CHttpClient::CConnection::ConsumeBufferedMessage(_In_ SIZE_T nConsumedBytes)
{
  CAutoSlimRWLShared cLock(&sRwMutex);

  if (lpIpc != NULL && hConn != NULL)
    return lpIpc->ConsumeBufferedMessage(hConn, nConsumedBytes);
  return MX_E_NotReady;
}

CHttpClient* CHttpClient::CConnection::GetHttpClient()
{
  CAutoSlimRWLShared cLock(&sRwMutex);

  if (lpHttpClient != NULL)
  {
    if (lpHttpClient->SafeAddRef() > 0)
      return lpHttpClient;
  }
  return NULL;
}

HRESULT CHttpClient::CConnection::OnSocketCreate(_In_ CIpc *_lpIpc, _In_ HANDLE h,
                                                 _Inout_ CIpc::CREATE_CALLBACK_DATA &sData)
{
  TAutoRefCounted<CHttpClient> cHttpClient;

  cHttpClient.Attach(GetHttpClient());
  if (!cHttpClient)
    return MX_E_Cancelled;

  lpIpc = _lpIpc;
  hConn = h;

  //setup socket
  sData.cConnectCallback = MX_BIND_MEMBER_CALLBACK(&CHttpClient::CConnection::OnSocketConnect, this);
  sData.cDataReceivedCallback = MX_BIND_MEMBER_CALLBACK(&CHttpClient::CConnection::OnSocketDataReceived, this);
  sData.cDestroyCallback = MX_BIND_MEMBER_CALLBACK(&CHttpClient::CConnection::OnSocketDestroy, this);

  if (cHttpClient->cConnectionCreatedCallback)
  {
    HRESULT hRes;

    hRes = cHttpClient->cConnectionCreatedCallback(lpIpc, h);
    if (FAILED(hRes))
      return hRes;
  }

  //done
  return S_OK;
}

VOID CHttpClient::CConnection::OnSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                                               _In_ HRESULT hrErrorCode)
{
  TAutoRefCounted<CHttpClient> cHttpClient;

  {
    CAutoSlimRWLExclusive cLock(&sRwMutex);

    if (lpHttpClient != NULL)
    {
      if (lpHttpClient->SafeAddRef() > 0)
        cHttpClient.Attach(lpHttpClient);

      lpHttpClient = NULL;
    }
  }

  if (cHttpClient)
    cHttpClient->OnConnectionClosed(this, hrErrorCode);
  return;
}

HRESULT CHttpClient::CConnection::OnSocketConnect(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_opt_ CIpc::CUserData *lpUserData)
{
  TAutoRefCounted<CConnection> cAutoRef(this);
  TAutoRefCounted<CHttpClient> cHttpClient;

  cHttpClient.Attach(GetHttpClient());
  if (!cHttpClient)
    return MX_E_Cancelled;

  //setup SSL layer
  if (bUseSSL != FALSE)
  {
    HRESULT hRes;

    hRes = cHttpClient->OnAddSslLayer(lpIpc, h);
    if (FAILED(hRes))
      return hRes;
  }

  return cHttpClient->OnConnectionEstablished(this);
}

HRESULT CHttpClient::CConnection::OnSocketDataReceived(_In_ CIpc *lpIpc, _In_ HANDLE h,
                                                       _In_ CIpc::CUserData *lpUserData)
{
  TAutoRefCounted<CConnection> cAutoRef(this);
  TAutoRefCounted<CHttpClient> cHttpClient;

  cHttpClient.Attach(GetHttpClient());
  return (cHttpClient) ? cHttpClient->OnDataReceived(this) : MX_E_Cancelled;
}

VOID CHttpClient::CConnection::OnAfterSendRequestHeaders(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ LPVOID lpCookie,
                                                         _In_ CIpc::CUserData *lpUserData)
{
  TAutoRefCounted<CConnection> cAutoRef(this);
  TAutoRefCounted<CHttpClient> cHttpClient;

  cHttpClient.Attach(GetHttpClient());
  if (cHttpClient)
  {
    cHttpClient->OnAfterSendRequestHeaders(this);
  }
  else
  {
    CAutoSlimRWLShared cLock(&sRwMutex);

    if (lpIpc != NULL && hConn != NULL)
    {
      lpIpc->Close(hConn, MX_E_Cancelled);
    }
  }
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
