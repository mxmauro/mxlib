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
#include "HttpAuthCache.h"
#include "..\..\Include\AutoPtr.h"
#include "..\..\Include\Finalizer.h"

#define HTTPAUTHCACHE_FINALIZER_PRIORITY              10001

#define MAX_CACHED_ITEMS                                 64

#define _MATCH_No                                         0
#define _MATCH_Yes                                        1
#define _MATCH_Child                                      2
#define _MATCH_Parent                                     3

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CHttpAuthCacheItem : public CBaseMemObj
{
public:
  CHttpAuthCacheItem() : CBaseMemObj()
    {
    nPort = 0;
    return;
    };

public:
  CUrl::eScheme nScheme;
  CStringW cStrHostW;
  int nPort;
  CStringW cStrPathW;
  TAutoRefCounted<CHttpBaseAuth> cHttpAuth;
  DWORD dwLastUsedTickMs;
};

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

static LONG volatile nRwMutex = 0;
static MX::Internals::CHttpAuthCacheItem* aList[MAX_CACHED_ITEMS] = { 0 };
static LONG volatile nShutdownAdded = 0;

//-----------------------------------------------------------

static VOID HttpAuthCache_Shutdown();
static int DoesMatch(_In_ SIZE_T nIdx, _In_ MX::CUrl::eScheme nScheme, _In_z_ LPCWSTR szHostW, _In_ int nPort,
                     _In_z_ LPCWSTR szPathW);

//-----------------------------------------------------------

namespace MX {

namespace HttpAuthCache {

HRESULT Add(_In_ CUrl &cUrl, _In_ CHttpBaseAuth *lpHttpAuth)
{
  return Add(cUrl.GetSchemeCode(), cUrl.GetHost(), cUrl.GetPort(), cUrl.GetPath(), lpHttpAuth);
}

HRESULT Add(_In_ CUrl::eScheme nScheme, _In_z_ LPCWSTR szHostW, _In_ int nPort, _In_z_ LPCWSTR szPathW,
            _In_ CHttpBaseAuth *lpHttpAuth)
{
  MX::CAutoSlimRWLExclusive cLock(&nRwMutex);
  MX::TAutoDeletePtr<MX::Internals::CHttpAuthCacheItem> cNewCachedItem;
  SIZE_T nIdx, nFreeSlotIdx = (SIZE_T)-1;
  int nIsSame = 0;
  HRESULT hRes;

  if (nScheme == MX::CUrl::SchemeUnknown)
    return MX_E_Unsupported;
  if (szHostW == NULL || szPathW == NULL || lpHttpAuth == NULL)
    return E_POINTER;
  if (nPort < 0 || nPort > 65535)
    return E_INVALIDARG;

  if (_InterlockedCompareExchange(&nShutdownAdded, 1, 0) == 0)
  {
    hRes = MX::RegisterFinalizer(&HttpAuthCache_Shutdown, HTTPAUTHCACHE_FINALIZER_PRIORITY);
    if (FAILED(hRes))
    {
      _InterlockedExchange(&nShutdownAdded, 0);
      return hRes;
    }
  }

  //check for same/inner matches
  for (nIdx = 0; nIdx < MAX_CACHED_ITEMS; nIdx++)
  {
    int nMatch;

    switch (nMatch = DoesMatch(nIdx, nScheme, szHostW, nPort, szPathW))
    {
      case _MATCH_Yes:
      case _MATCH_Child:
        if (lpHttpAuth->IsSameThan(aList[nIdx]->cHttpAuth.Get()) == FALSE)
        {
          delete aList[nIdx];
          aList[nIdx] = NULL;

          if (nFreeSlotIdx == (SIZE_T)-1)
            nFreeSlotIdx = nIdx;
        }
        else
        {
          switch (nMatch)
          {
            case _MATCH_Yes:
              nIsSame = 1;
              break;

            case _MATCH_Child:
              if (nIsSame == 0)
                nIsSame = 2;
              break;
          }
        }
        break;
    }
  }
  if (nIsSame == 1)
    return S_OK; //the credential is the same than the cached one

  //find a free slot
  if (nFreeSlotIdx == (SIZE_T)-1)
  {
    for (nIdx = 0; nIdx < MAX_CACHED_ITEMS; nIdx++)
    {
      if (aList[nIdx] == NULL)
      {
        nFreeSlotIdx = nIdx;
        break;
      }
    }
  }
  if (nFreeSlotIdx == (SIZE_T)-1)
  {
    DWORD dwLowerTicksMs = 0xFFFFFFFFUL;

    for (nIdx = 0; nIdx < MAX_CACHED_ITEMS; nIdx++)
    {
      if (aList[nIdx] != NULL)
      {
        if (nFreeSlotIdx == (SIZE_T)-1 || aList[nIdx]->dwLastUsedTickMs < dwLowerTicksMs)
        {
          dwLowerTicksMs = aList[nIdx]->dwLastUsedTickMs;
          nFreeSlotIdx = nIdx;
        }
      }
    }
    MX_ASSERT((nFreeSlotIdx != (SIZE_T)-1));
    delete aList[nFreeSlotIdx];
    aList[nFreeSlotIdx] = NULL;
  }

  //create the new entry
  cNewCachedItem.Attach(MX_DEBUG_NEW MX::Internals::CHttpAuthCacheItem());
  if (!cNewCachedItem)
    return E_OUTOFMEMORY;
  cNewCachedItem->cHttpAuth = lpHttpAuth;
  cNewCachedItem->dwLastUsedTickMs = ::GetTickCount();
  cNewCachedItem->nScheme = nScheme;
  cNewCachedItem->nPort = nPort;
  if (cNewCachedItem->cStrHostW.Copy(szHostW) == FALSE ||
      cNewCachedItem->cStrPathW.Copy(szPathW) == FALSE)
  {
    return E_OUTOFMEMORY;
  }

  //add to the list
  aList[nFreeSlotIdx] = cNewCachedItem.Detach();

  //done
  return S_OK;
}

VOID Remove(_In_ CHttpBaseAuth *lpHttpAuth)
{
  MX::CAutoSlimRWLExclusive cLock(&nRwMutex);
  SIZE_T nIdx;

  for (nIdx = 0; nIdx < MAX_CACHED_ITEMS; nIdx++)
  {
    if (aList[nIdx] != NULL && aList[nIdx]->cHttpAuth.Get() == lpHttpAuth)
    {
      delete aList[nIdx];
      aList[nIdx] = NULL;
      break;
    }
  }
  //done
  return;
}

VOID RemoveAll()
{
  MX::CAutoSlimRWLExclusive cLock(&nRwMutex);
  SIZE_T nIdx;

  for (nIdx = 0; nIdx < MAX_CACHED_ITEMS; nIdx++)
  {
    if (aList[nIdx] != NULL)
    {
      delete aList[nIdx];
      aList[nIdx] = NULL;
    }
  }
  //done
  return;
}

CHttpBaseAuth* Lookup(_In_ CUrl &cUrl)
{
  return Lookup(cUrl.GetSchemeCode(), cUrl.GetHost(), cUrl.GetPort(), cUrl.GetPath());
}

CHttpBaseAuth* Lookup(_In_ CUrl::eScheme nScheme, _In_z_ LPCWSTR szHostW, _In_ int nPort, _In_z_ LPCWSTR szPathW)
{
  MX::CAutoSlimRWLShared cLock(&nRwMutex);
  CHttpBaseAuth *lpHttpAuth;
  SIZE_T nIdx, nBestMatchIdx, nBestMatchPathLen;

  if (szHostW == NULL || szPathW == NULL || nPort < 0 || nPort > 65535)
    return NULL;

  nBestMatchIdx = (SIZE_T)-1;
  nBestMatchPathLen = 0;
  for (nIdx = 0; nIdx < MAX_CACHED_ITEMS; nIdx++)
  {
    switch (DoesMatch(nIdx, nScheme, szHostW, nPort, szPathW))
    {
      case _MATCH_Yes:
        lpHttpAuth = aList[nIdx]->cHttpAuth.Get();
        lpHttpAuth->AddRef();
        aList[nIdx]->dwLastUsedTickMs = ::GetTickCount();
        return lpHttpAuth;

      case _MATCH_Parent:
        if (nBestMatchIdx == (SIZE_T)-1 || aList[nIdx]->cStrPathW.GetLength() > nBestMatchPathLen)
        {
          nBestMatchIdx = nIdx;
          nBestMatchPathLen = aList[nIdx]->cStrPathW.GetLength();
        }
        break;
    }
  }

  if (nBestMatchIdx == (SIZE_T)-1)
    return NULL;

  lpHttpAuth = aList[nBestMatchIdx]->cHttpAuth.Get();
  lpHttpAuth->AddRef();
  aList[nBestMatchIdx]->dwLastUsedTickMs = ::GetTickCount();
  return lpHttpAuth;
}

} //namespace HttpAuthCache

} //namespace MX

//-----------------------------------------------------------

static VOID HttpAuthCache_Shutdown()
{
  MX::HttpAuthCache::RemoveAll();
  return;
}

static int DoesMatch(_In_ SIZE_T nIdx, _In_ MX::CUrl::eScheme nScheme, _In_z_ LPCWSTR szHostW, _In_ int nPort,
                     _In_z_ LPCWSTR szPathW)
{
  MX::Internals::CHttpAuthCacheItem *lpItem = aList[nIdx];
  LPCWSTR szItemPathW;
  SIZE_T nPathLen, nItemPathLen, nMinPathLen;

  if (lpItem == NULL)
    return _MATCH_No;

  if (lpItem->nScheme != lpItem->nScheme)
    return _MATCH_No;

  if (lpItem->nPort != lpItem->nPort)
    return _MATCH_No;

  if (MX::StrCompareW((LPCWSTR)(lpItem->cStrHostW), szHostW) != 0)
    return _MATCH_No;

  szItemPathW = (LPCWSTR)(lpItem->cStrPathW);
  nItemPathLen = lpItem->cStrPathW.GetLength();

  nMinPathLen = nPathLen = MX::StrLenW(szPathW);
  if (nMinPathLen > nItemPathLen)
    nMinPathLen = nItemPathLen;

  if (MX::StrNCompareW(szItemPathW, szPathW, nMinPathLen) != 0)
    return _MATCH_No;

  if (nPathLen > nItemPathLen)
  {
    return (szPathW[nMinPathLen] == L'/') ? _MATCH_Child : _MATCH_No;
  }
  else if (nPathLen < nItemPathLen)
  {
    return (szItemPathW[nMinPathLen] == L'/') ? _MATCH_Parent : _MATCH_No;
  }
  return _MATCH_Yes;
}
