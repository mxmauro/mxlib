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
#include "HostResolverCommon.h"
#include "..\..\Include\Finalizer.h"

//-----------------------------------------------------------

#define IPADDRESSRESOLVER_FINALIZER_PRIORITY 10000

//-----------------------------------------------------------

static MX::CCriticalSection *lpResolverCS = NULL;
static MX::Internals::CIPAddressResolver *lpResolver = NULL;

//-----------------------------------------------------------

static VOID IPAddressResolver_Shutdown();

//-----------------------------------------------------------

namespace MX {

namespace Internals {

CIPAddressResolver::CIPAddressResolver() : CBaseMemObj(), CThread()
{
  InterlockedExchange(&nRefCount, 1);
  lpEventQueue = NULL;
  return;
}

CIPAddressResolver::~CIPAddressResolver()
{
  if (lpEventQueue != NULL)
    lpEventQueue->Release();
  return;
}

CIPAddressResolver* CIPAddressResolver::Get()
{
  static LONG volatile nInitMutex = 0;

  if (lpResolverCS == NULL)
  {
    CFastLock cInitLock(&nInitMutex);

    if (lpResolverCS == NULL)
    {
      CCriticalSection *_lpResolverCS;

      _lpResolverCS = MX_DEBUG_NEW CCriticalSection();
      if (_lpResolverCS == NULL)
        return NULL;
      //register shutdown callback
      if (FAILED(MX::RegisterFinalizer(&IPAddressResolver_Shutdown, IPADDRESSRESOLVER_FINALIZER_PRIORITY)))
      {
        delete _lpResolverCS;
        return NULL;
      }
      lpResolverCS = _lpResolverCS;
    }
  }
  if (lpResolverCS != NULL)
  {
    CCriticalSection::CAutoLock cResolverLock(*lpResolverCS);

    if (lpResolver == NULL)
    {
      lpResolver = MX_DEBUG_NEW CIPAddressResolver();
      if (lpResolver != NULL && lpResolver->Initialize() == FALSE)
      {
        delete lpResolver;
        lpResolver = NULL;
      }
    }
    else
    {
      lpResolver->AddRef();
    }
    return lpResolver;
  }
  return NULL;
}

VOID CIPAddressResolver::Release()
{
  BOOL bStop;

  bStop = FALSE;
  {
    CCriticalSection::CAutoLock cResolverLock(*lpResolverCS);

    if (_InterlockedDecrement(&nRefCount) == 0)
    {
      lpResolver = NULL;
      bStop = TRUE;
    }
  }
  if (bStop != FALSE)
    Stop(); //will autodelete it
  return;
}

HRESULT CIPAddressResolver::Resolve(_In_ MX::CHostResolver *lpHostResolver, _In_ PSOCKADDR_INET lpAddr,
                                    _In_ HRESULT *lphErrorCode, _In_z_ LPCSTR szHostNameA, _In_ int nDesiredFamily,
                                    _In_ DWORD dwTimeoutMs)
{
  TAutoRefCounted<CHostResolver> cAutoRefEvent(lpHostResolver);
  TAutoRefCounted<CItem> cNewItem;
  HRESULT hRes;

  MX_ASSERT(lpHostResolver != NULL);
  MX_ASSERT(lpAddr != NULL);
  MX_ASSERT(lphErrorCode != NULL);
  if (szHostNameA == NULL)
    return E_POINTER;
  if (*szHostNameA == 0)
    return E_INVALIDARG;
  if (nDesiredFamily != AF_UNSPEC && nDesiredFamily != AF_INET && nDesiredFamily != AF_INET6)
    return E_INVALIDARG;
  //create new item, the item will have 2 references, the items list one and the timeout queue
  cNewItem.Attach(MX_DEBUG_NEW CItem(lpHostResolver, MX_BIND_MEMBER_CALLBACK(&CIPAddressResolver::OnTimeout, this)));
  if (!cNewItem)
    return E_OUTOFMEMORY;
  if (cNewItem->cStrHostNameA.Copy(szHostNameA) == FALSE)
    return E_OUTOFMEMORY;
  cNewItem->lpAddr = lpAddr;
  cNewItem->lphErrorCode = lphErrorCode;
  cNewItem->nDesiredFamily = nDesiredFamily;
  //add to lists
  {
    CCriticalSection::CAutoLock cLock(cs);

    lpHostResolver->SetState(CHostResolver::StateQueued);
    lpHostResolver->ResetCancelMark();
    //setup timeout
    hRes = lpEventQueue->Add(cNewItem.Get(), dwTimeoutMs);
    if (FAILED(hRes))
      return hRes;
    //enqueue
    cQueuedItemsList.PushTail(cNewItem.Detach());
    cQueueChangedEv.Set();
  }
  //done
  return S_OK;
}

VOID CIPAddressResolver::Cancel(_In_ MX::CHostResolver *lpHostResolver)
{
  CCriticalSection::CAutoLock cLock(cs);
  TLnkLst<CItem>::Iterator it;
  CItem *lpItem;

  for (lpItem=it.Begin(cQueuedItemsList); lpItem!=NULL; lpItem=it.Next())
  {
    if (lpItem->cHostResolver.Get() == lpHostResolver)
    {
      lpItem->MarkAsCanceled();
      break;
    }
  }
  return;
}

HRESULT CIPAddressResolver::ResolveAddr(_Out_ PSOCKADDR_INET lpAddress, _In_z_ LPCSTR szAddressA,
                                        _In_ int nDesiredFamily)
{
  PADDRINFOA lpCurrAddrInfoA, lpAddrInfoA;

  if (lpAddress == NULL)
    return E_POINTER;
  if (nDesiredFamily == AF_INET || nDesiredFamily == AF_UNSPEC)
  {
    if (MX::CHostResolver::IsValidIPV4(szAddressA, (SIZE_T)-1, lpAddress) != FALSE)
      return S_OK;
  }
  if (nDesiredFamily == AF_INET6 || nDesiredFamily == AF_UNSPEC)
  {
    if (MX::CHostResolver::IsValidIPV6(szAddressA, (SIZE_T)-1, lpAddress) != FALSE)
      return S_OK;
  }
  if (::getaddrinfo(szAddressA, NULL, NULL, &lpAddrInfoA) == SOCKET_ERROR)
  {
    HRESULT hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
    MX::MemSet(lpAddress, 0, sizeof(SOCKADDR_INET));
    return hRes;
  }
  for (lpCurrAddrInfoA=lpAddrInfoA; lpCurrAddrInfoA!=NULL; lpCurrAddrInfoA=lpCurrAddrInfoA->ai_next)
  {
    if ((nDesiredFamily == AF_INET || nDesiredFamily == AF_UNSPEC) &&
        lpCurrAddrInfoA->ai_family == PF_INET &&
        lpCurrAddrInfoA->ai_addrlen >= sizeof(SOCKADDR_IN))
    {
      MX::MemCopy(&(lpAddress->Ipv4), lpCurrAddrInfoA->ai_addr, sizeof(sockaddr_in));
      ::freeaddrinfo(lpAddrInfoA);
      return S_OK;
    }
    if ((nDesiredFamily == AF_INET6 || nDesiredFamily == AF_UNSPEC) &&
        lpCurrAddrInfoA->ai_family == PF_INET6 &&
        lpCurrAddrInfoA->ai_addrlen >= sizeof(SOCKADDR_IN6))
    {
      MX::MemCopy(&(lpAddress->Ipv6), lpCurrAddrInfoA->ai_addr, sizeof(SOCKADDR_IN6));
      ::freeaddrinfo(lpAddrInfoA);
      return S_OK;
    }
  }
  ::freeaddrinfo(lpAddrInfoA);
  MX::MemSet(lpAddress, 0, sizeof(SOCKADDR_INET));
  return E_FAIL;
}

BOOL CIPAddressResolver::Initialize()
{
  SetAutoDelete(TRUE);
  lpEventQueue = CSystemTimedEventQueue::Get();
  if (lpEventQueue == NULL)
    return FALSE;
  if (FAILED(cQueueChangedEv.Create(TRUE, FALSE)) || Start() == FALSE)
    return FALSE;
  return TRUE;
}

VOID CIPAddressResolver::AddRef()
{
  _InterlockedIncrement(&nRefCount);
  return;
}

VOID CIPAddressResolver::ThreadProc()
{
  DWORD dwTimeoutMs, dwHitEv;
  HANDLE hEvent;

  hEvent = cQueueChangedEv.Get();
  do
  {
    //reset event
    cQueueChangedEv.Reset();
    //process next item
    dwTimeoutMs = (ProcessQueued() != FALSE) ? 0 : INFINITE;
    //flush
    FlushCompleted();
  }
  while (CheckForAbort(dwTimeoutMs, 1, &hEvent, &dwHitEv) == FALSE);
  //before leaving the thread, cancel pending
  CancelAll();
  while (FlushCompleted() == FALSE)
  {
    ProcessQueued();
    _YieldProcessor();
  }
  return;
}

VOID CIPAddressResolver::OnTimeout(_In_ CTimedEventQueue::CEvent *lpEvent)
{
  CItem *lpItem = (CItem*)lpEvent;
  CItem *lpProcessItem = NULL;

  {
    CCriticalSection::CAutoLock cLock(cs);

    if (lpItem->GetLinkedList() == &cQueuedItemsList)
    {
      cQueuedItemsList.Remove(lpItem);
      cProcessedItemsList.PushTail(lpItem);
      //mark as processing
      lpItem->cHostResolver->SetState(MX::CHostResolver::StateProcessing);
      lpProcessItem = lpItem;
    }
  }
  //call callback
  if (lpProcessItem != NULL)
  {
    if (lpProcessItem->IsCanceled() == FALSE)
    {
      *(lpProcessItem->lphErrorCode) = MX_E_Timeout;
    }
    else
    {
      *(lpProcessItem->lphErrorCode) = MX_E_Cancelled;
      lpProcessItem->cHostResolver->MarkAsCanceled();
    }
    lpProcessItem->cHostResolver->InvokeCallback();
    lpProcessItem->cHostResolver->SetStateIf(MX::CHostResolver::StateProcessed, MX::CHostResolver::StateProcessing);
  }
  //----
  _InterlockedExchange(&(lpItem->nTimeoutCalled), 1);
  return;
}

BOOL CIPAddressResolver::ProcessQueued()
{
  CItem *lpItem;
  SOCKADDR_INET sAddr;
  HRESULT hRes;

  //now pop the first item from the queue
  {
    CCriticalSection::CAutoLock cLock(cs);

    lpItem = cQueuedItemsList.PopHead();
    if (lpItem == NULL)
      return FALSE;
    cProcessedItemsList.PushTail(lpItem);
    lpEventQueue->Remove(lpItem, FALSE);
    //mark as processing
    lpItem->cHostResolver->SetState(MX::CHostResolver::StateProcessing);
  }
  //do actual resolve
  MemSet(&sAddr, 0, sizeof(sAddr));
  if (lpItem->IsCanceled() == FALSE)
    hRes = ResolveAddr(&sAddr, (LPSTR)(lpItem->cStrHostNameA), lpItem->nDesiredFamily);
  else
    hRes = MX_E_Cancelled;
  if (SUCCEEDED(hRes))
    MemCopy(lpItem->lpAddr, &sAddr, sizeof(sAddr));
  else
    MemSet(lpItem->lpAddr, 0, sizeof(sAddr));
  //raise callback
  if (hRes == MX_E_Cancelled)
    lpItem->cHostResolver->MarkAsCanceled();
  *(lpItem->lphErrorCode) = hRes;
  lpItem->cHostResolver->InvokeCallback();
  lpItem->cHostResolver->SetStateIf(MX::CHostResolver::StateProcessed, MX::CHostResolver::StateProcessing);
  return TRUE;
}

VOID CIPAddressResolver::CancelAll()
{
  CCriticalSection::CAutoLock cLock(cs);
  TLnkLst<CItem>::Iterator it;
  CItem *lpItem;

  for (lpItem=it.Begin(cQueuedItemsList); lpItem!=NULL; lpItem=it.Next())
    lpItem->MarkAsCanceled();
  return;
}

BOOL CIPAddressResolver::FlushCompleted()
{
  CItem *lpItem;
  BOOL bQueueIsEmpty;

  do
  {
    {
      CCriticalSection::CAutoLock cLock(cs);
      TLnkLst<CItem>::Iterator it;

      for (lpItem=it.Begin(cProcessedItemsList); lpItem!=NULL; lpItem=it.Next())
      {
        if (__InterlockedRead(&(lpItem->nTimeoutCalled)) != 0)
        {
          cProcessedItemsList.Remove(lpItem);
          break;
        }
      }
      bQueueIsEmpty = cProcessedItemsList.IsEmpty();
    }
    if (lpItem != NULL)
      lpItem->Release();
  }
  while (lpItem != NULL);
  return bQueueIsEmpty;
}

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

static VOID IPAddressResolver_Shutdown()
{
  delete lpResolverCS;
  lpResolverCS = NULL;
  return;
}
