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
#include "..\..\Include\Defines.h"
#include <VersionHelpers.h>

//-----------------------------------------------------------

#define IPADDRESSRESOLVER_FINALIZER_PRIORITY 10000

//-----------------------------------------------------------

static LONG volatile nResolverRwMutex = 0;
static MX::Internals::CIPAddressResolver *lpResolver = NULL;

//-----------------------------------------------------------

namespace MX {

namespace Internals {

CIPAddressResolver::CIPAddressResolver() : TRefCounted<CThread>()
{
  WCHAR szDllNameW[4096];
  DWORD dwLen;
  SIZE_T nSlot;

  SlimRWL_Initialize(&nRwMutex);

  for (nSlot = 0; nSlot < MAX_SIMULTANEOUS_THREADS; nSlot++)
    _InterlockedExchange(&(sWorkers[nSlot].nWorking), 0);

  hWs2_32Dll = NULL;
  fnGetAddrInfoExW = NULL;
  fnFreeAddrInfoExW = NULL;
  fnGetAddrInfoExCancel = NULL;
  fnGetAddrInfoExOverlappedResult = NULL;

  dwLen = ::GetSystemDirectoryW(szDllNameW, MX_ARRAYLEN(szDllNameW) - 16);
  if (dwLen > 0)
  {
    if (szDllNameW[dwLen - 1] != L'\\')
      szDllNameW[dwLen++] = L'\\';
    MX::MemCopy(szDllNameW + dwLen, L"ws2_32.dll", (10 + 1) * sizeof(WCHAR));
    //load library
    hWs2_32Dll = ::LoadLibraryW(szDllNameW);
    if (hWs2_32Dll != NULL)
    {
      fnGetAddrInfoExW = (lpfnGetAddrInfoExW)::GetProcAddress(hWs2_32Dll, "GetAddrInfoExW");
      fnFreeAddrInfoExW = (lpfnFreeAddrInfoExW)::GetProcAddress(hWs2_32Dll, "FreeAddrInfoExW");
      if (::IsWindows8OrGreater() != FALSE)
      {
        fnGetAddrInfoExCancel = (lpfnGetAddrInfoExCancel)::GetProcAddress(hWs2_32Dll, "GetAddrInfoExCancel");
        fnGetAddrInfoExOverlappedResult = (lpfnGetAddrInfoExOverlappedResult)
                                          ::GetProcAddress(hWs2_32Dll, "GetAddrInfoExOverlappedResult");
      }
      if (fnGetAddrInfoExW == NULL || fnFreeAddrInfoExW == NULL)
      {
        fnGetAddrInfoExW = NULL;
        fnFreeAddrInfoExW = NULL;
        fnGetAddrInfoExCancel = NULL;
        fnGetAddrInfoExOverlappedResult = NULL;

        ::FreeLibrary(hWs2_32Dll);
        hWs2_32Dll = NULL;
      }
      else if (fnGetAddrInfoExCancel == NULL || fnGetAddrInfoExOverlappedResult == NULL)
      {
        fnGetAddrInfoExCancel = NULL;
        fnGetAddrInfoExOverlappedResult = NULL;
      }
    }
  }
  return;
}

CIPAddressResolver::~CIPAddressResolver()
{
  if (hWs2_32Dll != NULL)
    ::FreeLibrary(hWs2_32Dll);
  return;
}

CIPAddressResolver* CIPAddressResolver::Get()
{
  CAutoSlimRWLShared cLock(&nResolverRwMutex);

  if (lpResolver == NULL)
  {
    cLock.UpgradeToExclusive();

    if (lpResolver == NULL)
    {
      CIPAddressResolver *_lpResolver;

      _lpResolver = MX_DEBUG_NEW CIPAddressResolver();
      if (_lpResolver == NULL)
        return NULL;
      if (_lpResolver->Initialize() == FALSE)
      {
        delete _lpResolver;
        return NULL;
      }

      //register shutdown callback
      if (FAILED(MX::RegisterFinalizer(&CIPAddressResolver::Shutdown, IPADDRESSRESOLVER_FINALIZER_PRIORITY)))
      {
        delete _lpResolver;
        return NULL;
      }
      lpResolver = _lpResolver;
    }
  }
  lpResolver->AddRef();
  return lpResolver;
}

VOID CIPAddressResolver::Shutdown()
{
  CAutoSlimRWLExclusive cLock(&nResolverRwMutex);

  MX_RELEASE(lpResolver);
  return;
}

HRESULT CIPAddressResolver::Resolve(_In_ MX::CHostResolver *lpHostResolver, _In_z_ LPCSTR szHostNameA,
                                    _In_ int nDesiredFamily, _In_ DWORD dwTimeoutMs,
                                    _In_ CHostResolver::OnResultCallback cCallback, _In_ LPVOID lpUserData)
{
  TAutoRefCounted<CItem> cNewItem;

  MX_ASSERT(lpHostResolver != NULL);
  if (szHostNameA == NULL)
    return E_POINTER;
  if (*szHostNameA == 0)
    return E_INVALIDARG;
  if (nDesiredFamily != AF_UNSPEC && nDesiredFamily != AF_INET && nDesiredFamily != AF_INET6)
    return E_INVALIDARG;
  //create new item, the item will have 2 references, the items list one and the timeout queue
  cNewItem.Attach(MX_DEBUG_NEW CItem(lpHostResolver, cCallback, lpUserData, nDesiredFamily, dwTimeoutMs));
  if (!cNewItem)
    return E_OUTOFMEMORY;
  if (fnFreeAddrInfoExW != NULL)
  {
    CStringW cStrHostNameCopyW;

    if (cStrHostNameCopyW.Copy(szHostNameA) == FALSE)
      return E_OUTOFMEMORY;
    cNewItem->uHostName.szStrW = cStrHostNameCopyW.Detach();
  }
  else
  {
    CStringA cStrHostNameCopyA;

    if (cStrHostNameCopyA.Copy(szHostNameA) == FALSE)
      return E_OUTOFMEMORY;
    cNewItem->uHostName.szStrA = cStrHostNameCopyA.Detach();
  }
  //if we can cancel, try to create a cancelable event
  if (fnGetAddrInfoExCancel != NULL)
  {
    cNewItem->hCancelAsync = ::CreateEventW(NULL, TRUE, FALSE, NULL);
  }

  //queue on list
  {
    CAutoSlimRWLExclusive cLock(&nRwMutex);

    cItemsList.PushTail(cNewItem.Detach());
    cWakeUpWorkerEv.Set();
  }
  //done
  return S_OK;
}

VOID CIPAddressResolver::Cancel(_In_ MX::CHostResolver *lpHostResolver)
{
  CAutoSlimRWLShared cLock(&nRwMutex);
  TLnkLst<CItem>::Iterator it;
  CItem *lpItem;

  for (lpItem = it.Begin(cItemsList); lpItem != NULL; lpItem = it.Next())
  {
    if (lpItem->lpHostResolver == lpHostResolver)
    {
      lpItem->SetErrorCode(MX_E_Cancelled);
      cWakeUpWorkerEv.Set();
      break;
    }
  }
  return;
}

BOOL CIPAddressResolver::Initialize()
{
  SetAutoDelete(TRUE);
  if (FAILED(cWakeUpWorkerEv.Create(TRUE, FALSE)) || Start() == FALSE)
    return FALSE;
  return TRUE;
}

VOID CIPAddressResolver::ThreadProc()
{
  DWORD dwTimeoutMs, dwHitEv;
  HANDLE hWakeUpEvent;

  dwTimeoutMs = INFINITE;
  hWakeUpEvent = cWakeUpWorkerEv.Get();
  while (CheckForAbort(dwTimeoutMs, 1, &hWakeUpEvent, &dwHitEv) == FALSE)
  {
    //reset event
    cWakeUpWorkerEv.Reset();

    //process next item
    if (fnFreeAddrInfoExW != NULL)
    {
      ProcessMulti();
    }
    else
    {
      dwTimeoutMs = (ProcessSingle() != FALSE) ? 0 : INFINITE;
    }
  }

  //end all workers
  for (SIZE_T nSlot = 0; nSlot < MAX_SIMULTANEOUS_THREADS; nSlot++)
  {
    sWorkers[nSlot].cThread.Stop();
  }

  //before leaving the thread, cancel pending
  CancelPending();
  FlushFinished();
  return;
}

CIPAddressResolver::CItem* CIPAddressResolver::GetNextItemToProcess(_In_ LONG nSlot)
{
  CAutoSlimRWLShared cLock(&nRwMutex);
  CItem *lpItem;

  lpItem = cItemsList.GetHead();
  while (lpItem != NULL)
  {
    if (_InterlockedCompareExchange(&(lpItem->nAssignedSlot), nSlot, -1) < 0)
    {
      lpItem->AddRef();
      return lpItem;
    }
    lpItem = lpItem->GetNextEntry();
  }
  return NULL;
}

VOID CIPAddressResolver::ProcessMulti()
{
  TAutoRefCounted<CItem> cItem;
  LONG nSlot, nWorking;

  do
  {
    if (CheckForAbort() != FALSE)
      break;

    for (nSlot = nWorking = 0; nSlot < MAX_SIMULTANEOUS_THREADS && CheckForAbort() == FALSE; nSlot++)
    {
      if (__InterlockedRead(&(sWorkers[nSlot].nWorking)) == 0)
      {
        sWorkers[nSlot].cThread.Stop();

        cItem.Attach(GetNextItemToProcess(nSlot));
        if (!cItem)
          return;

        if (sWorkers[nSlot].cThread.Start(this, &CIPAddressResolver::ResolverWorkerThread,
                                          (SIZE_T)(cItem.Get())) != FALSE)
        {
          cItem.Detach();
        }
        else
        {
          cItem->SetErrorCode(E_OUTOFMEMORY);
          Process(cItem.Detach());
        }
      }
      else
      {
        nWorking++;
      }
    }
  }
  while (nWorking < MAX_SIMULTANEOUS_THREADS);
  return;
}

BOOL CIPAddressResolver::ProcessSingle()
{
  TAutoRefCounted<CItem> cItem;

  cItem.Attach(GetNextItemToProcess(0));
  if (!cItem)
    return FALSE;
  Process(cItem);
  return TRUE;
}

VOID CIPAddressResolver::Process(_In_ CItem *lpItemToProcess)
{
  TLnkLst<CItem> cCompletedItemsList;
  CItem *lpItem;

  //resolve this item if not canceled
  if (lpItemToProcess->GetErrorCode() == S_FALSE)
  {
    ResolveItem(lpItemToProcess);
  }

  //check for similar items that can be completed
  {
    CAutoSlimRWLExclusive cLock(&nRwMutex);

    MX_ASSERT(lpItemToProcess->GetLinkedList() == &cItemsList);
    lpItemToProcess->RemoveNode();

    cCompletedItemsList.PushTail(lpItemToProcess);

    //if succeeded, check if other pending item wants to resolve the same host
    if (SUCCEEDED(lpItemToProcess->GetErrorCode()))
    {
      TLnkLst<CItem>::Iterator it;

      for (lpItem = it.Begin(cItemsList); lpItem != NULL; lpItem = it.Next())
      {
        if (lpItem->GetErrorCode() == S_FALSE)
        {
          if (fnFreeAddrInfoExW != NULL)
          {
            if (StrCompareW(lpItemToProcess->uHostName.szStrW, lpItem->uHostName.szStrW) == 0)
            {
item_completed:
              lpItem->SetErrorCode(S_OK);
              MemCopy(&(lpItem->sAddr), &(lpItemToProcess->sAddr), sizeof(lpItem->sAddr));

              lpItem->RemoveNode();
              cCompletedItemsList.PushTail(lpItem);
            }
          }
          else
          {
            if (StrCompareA(lpItemToProcess->uHostName.szStrA, lpItem->uHostName.szStrA) == 0)
              goto item_completed;
          }
        }
      }
    }
  }

  //call callback on completed items and release them
  while ((lpItem = cCompletedItemsList.PopHead()) != NULL)
  {
    lpItem->CallCallback();
    lpItem->Release();
  }

  //done
  return;
}

VOID CIPAddressResolver::ResolverWorkerThread(_In_ SIZE_T nParam)
{
  CItem *lpItemToProcess = (CItem*)nParam;
  LONG nAssignedSlot = __InterlockedRead(&(lpItemToProcess->nAssignedSlot));

  _InterlockedExchange(&(sWorkers[nAssignedSlot].nWorking), 1);

  Process(lpItemToProcess);

  _InterlockedExchange(&(sWorkers[nAssignedSlot].nWorking), 0);
  cWakeUpWorkerEv.Set();
  return;
}

VOID CIPAddressResolver::ResolveItem(_In_ CItem *lpItem)
{
  if (fnFreeAddrInfoExW != NULL)
  {
    PADDRINFOEXW lpCurrAddrInfoExW, lpAddrInfoExW;
    INT res;

    //check for numeric address
    if (lpItem->nDesiredFamily == AF_INET || lpItem->nDesiredFamily == AF_UNSPEC)
    {
      if (MX::CHostResolver::IsValidIPV4(lpItem->uHostName.szStrW, (SIZE_T)-1, &(lpItem->sAddr)) != FALSE)
      {
        lpItem->SetErrorCode(S_OK);
        return;
      }
    }
    if (lpItem->nDesiredFamily == AF_INET6 || lpItem->nDesiredFamily == AF_UNSPEC)
    {
      if (MX::CHostResolver::IsValidIPV6(lpItem->uHostName.szStrW, (SIZE_T)-1, &(lpItem->sAddr)) != FALSE)
      {
        lpItem->SetErrorCode(S_OK);
        return;
      }
    }

    //slow path
    if (lpItem->hCancelAsync == NULL)
    {
      timeval tv;

      tv.tv_sec = (long)(lpItem->dwTimeoutMs / 1000);
      tv.tv_usec = (long)(lpItem->dwTimeoutMs % 1000);

      res = fnGetAddrInfoExW(lpItem->uHostName.szStrW, NULL, NS_ALL, NULL, NULL, &lpAddrInfoExW,
                             ((lpItem->dwTimeoutMs != INFINITE) ? &tv : NULL), NULL, NULL, NULL);
    }
    else
    {
      OVERLAPPED sOvr;
      HANDLE hAsyncHandle;

      MemSet(&sOvr, 0, sizeof(sOvr));
      sOvr.hEvent = ::CreateEventW(NULL, TRUE, FALSE, NULL);
      if (sOvr.hEvent == NULL)
      {
        lpItem->SetErrorCode(E_OUTOFMEMORY);
        return;
      }

      res = fnGetAddrInfoExW(lpItem->uHostName.szStrW, NULL, NS_ALL, NULL, NULL, &lpAddrInfoExW, NULL, &sOvr, NULL,
                             &hAsyncHandle);
      if (res == NO_ERROR || res == WSAEINPROGRESS || res == WSA_IO_PENDING)
      {
        HANDLE hEvents[2] = { sOvr.hEvent, lpItem->hCancelAsync };

        switch (::WaitForMultipleObjects(2, hEvents, FALSE, lpItem->dwTimeoutMs))
        {
          case WAIT_TIMEOUT:
            fnGetAddrInfoExCancel(&hAsyncHandle);
            ::WaitForSingleObject(sOvr.hEvent, INFINITE);
            ::CloseHandle(sOvr.hEvent);

            lpItem->SetErrorCode(MX_E_Timeout);
            return;

          case WAIT_OBJECT_0:
            res = fnGetAddrInfoExOverlappedResult(&sOvr);
            break;

          case WAIT_OBJECT_0 + 1:
            fnGetAddrInfoExCancel(&hAsyncHandle);
            ::WaitForSingleObject(sOvr.hEvent, INFINITE);
            ::CloseHandle(sOvr.hEvent);

            lpItem->SetErrorCode(MX_E_Cancelled);
            return;

          default:
            fnGetAddrInfoExCancel(&hAsyncHandle);
            ::WaitForSingleObject(sOvr.hEvent, INFINITE);
            ::CloseHandle(sOvr.hEvent);

            lpItem->SetErrorCode(E_FAIL);
            return;
        }
      }
      ::CloseHandle(sOvr.hEvent);
    }
    if (res != NO_ERROR)
    {
      lpItem->SetErrorCode(MX_HRESULT_FROM_WIN32(res));
      return;
    }

    //process results
    for (lpCurrAddrInfoExW = lpAddrInfoExW; lpCurrAddrInfoExW != NULL; lpCurrAddrInfoExW = lpCurrAddrInfoExW->ai_next)
    {
      if ((lpItem->nDesiredFamily == AF_INET || lpItem->nDesiredFamily == AF_UNSPEC) &&
          lpCurrAddrInfoExW->ai_family == PF_INET &&
          lpCurrAddrInfoExW->ai_addrlen >= sizeof(SOCKADDR_IN))
      {
        MX::MemCopy(&(lpItem->sAddr.Ipv4), lpCurrAddrInfoExW->ai_addr, sizeof(sockaddr_in));
        fnFreeAddrInfoExW(lpAddrInfoExW);

        lpItem->SetErrorCode(S_OK);
        return;
      }
      if ((lpItem->nDesiredFamily == AF_INET6 || lpItem->nDesiredFamily == AF_UNSPEC) &&
          lpCurrAddrInfoExW->ai_family == PF_INET6 &&
          lpCurrAddrInfoExW->ai_addrlen >= sizeof(SOCKADDR_IN6))
      {
        MX::MemCopy(&(lpItem->sAddr.Ipv6), lpCurrAddrInfoExW->ai_addr, sizeof(SOCKADDR_IN6));
        fnFreeAddrInfoExW(lpAddrInfoExW);

        lpItem->SetErrorCode(S_OK);
        return;
      }
    }

    //free results
    fnFreeAddrInfoExW(lpAddrInfoExW);
  }
  else
  {
    PADDRINFOA lpCurrAddrInfoA, lpAddrInfoA;

    //check for numeric address
    if (lpItem->nDesiredFamily == AF_INET || lpItem->nDesiredFamily == AF_UNSPEC)
    {
      if (MX::CHostResolver::IsValidIPV4(lpItem->uHostName.szStrA, (SIZE_T)-1, &(lpItem->sAddr)) != FALSE)
      {
        lpItem->SetErrorCode(S_OK);
        return;
      }
    }
    if (lpItem->nDesiredFamily == AF_INET6 || lpItem->nDesiredFamily == AF_UNSPEC)
    {
      if (MX::CHostResolver::IsValidIPV6(lpItem->uHostName.szStrA, (SIZE_T)-1, &(lpItem->sAddr)) != FALSE)
      {
        lpItem->SetErrorCode(S_OK);
        return;
      }
    }

    //slow path
    if (::getaddrinfo(lpItem->uHostName.szStrA, NULL, NULL, &lpAddrInfoA) == SOCKET_ERROR)
    {
      lpItem->SetErrorCode(MX_HRESULT_FROM_LASTSOCKETERROR());
      return;
    }

    //process results
    for (lpCurrAddrInfoA = lpAddrInfoA; lpCurrAddrInfoA != NULL; lpCurrAddrInfoA = lpCurrAddrInfoA->ai_next)
    {
      if ((lpItem->nDesiredFamily == AF_INET || lpItem->nDesiredFamily == AF_UNSPEC) &&
          lpCurrAddrInfoA->ai_family == PF_INET &&
          lpCurrAddrInfoA->ai_addrlen >= sizeof(SOCKADDR_IN))
      {
        MX::MemCopy(&(lpItem->sAddr.Ipv4), lpCurrAddrInfoA->ai_addr, sizeof(sockaddr_in));
        ::freeaddrinfo(lpAddrInfoA);

        lpItem->SetErrorCode(S_OK);
        return;
      }
      if ((lpItem->nDesiredFamily == AF_INET6 || lpItem->nDesiredFamily == AF_UNSPEC) &&
          lpCurrAddrInfoA->ai_family == PF_INET6 &&
          lpCurrAddrInfoA->ai_addrlen >= sizeof(SOCKADDR_IN6))
      {
        MX::MemCopy(&(lpItem->sAddr.Ipv6), lpCurrAddrInfoA->ai_addr, sizeof(SOCKADDR_IN6));
        ::freeaddrinfo(lpAddrInfoA);

        lpItem->SetErrorCode(S_OK);
        return;
      }
    }

    //free results
    ::freeaddrinfo(lpAddrInfoA);
  }
  //done
  lpItem->SetErrorCode(E_FAIL);
  return;
}

VOID CIPAddressResolver::CancelPending()
{
  CAutoSlimRWLShared cLock(&nRwMutex);
  TLnkLst<CItem>::Iterator it;
  CItem *lpItem;

  for (lpItem = it.Begin(cItemsList); lpItem != NULL; lpItem = it.Next())
  {
    lpItem->SetErrorCode(MX_E_Cancelled);
  }
  return;
}

VOID CIPAddressResolver::FlushFinished()
{
  CItem *lpItem;

  do
  {
    {
      CAutoSlimRWLExclusive cLock(&nRwMutex);
      TLnkLst<CItem>::Iterator it;

      for (lpItem = it.Begin(cItemsList); lpItem != NULL; lpItem = it.Next())
      {
        if (__InterlockedRead(&(lpItem->hrErrorCode)) != S_FALSE)
        {
          lpItem->AddRef();
          lpItem->RemoveNode();
          break;
        }
      }
    }
    if (lpItem != NULL)
    {
      lpItem->CallCallback();
      lpItem->Release();
    }
  }
  while (lpItem != NULL);
  return;
}

//-----------------------------------------------------------

CIPAddressResolver::CItem::CItem(_In_ CHostResolver *_lpHostResolver, CHostResolver::OnResultCallback _cCallback,
                                 _In_ LPVOID _lpUserData, _In_ int _nDesiredFamily,
                                 _In_ DWORD _dwTimeoutMs) : TRefCounted<CBaseMemObj>(), TLnkLstNode<CItem>()
{
  _InterlockedExchange(&nMutex, 0);
  lpHostResolver = _lpHostResolver;
  cCallback = _cCallback;
  lpUserData = _lpUserData;
  nDesiredFamily = _nDesiredFamily;
  dwTimeoutMs = _dwTimeoutMs;
  uHostName.lp = NULL;
  MemSet(&sAddr, 0, sizeof(sAddr));
  _InterlockedExchange(&hrErrorCode, S_FALSE);
  hCancelAsync = NULL;
  _InterlockedExchange(&nAssignedSlot, -1);
  return;
}

CIPAddressResolver::CItem::~CItem()
{
  MX_FREE(uHostName.lp);
  if (hCancelAsync != NULL)
    ::CloseHandle(hCancelAsync);
  return;
}

VOID CIPAddressResolver::CItem::SetErrorCode(_In_ HRESULT hRes)
{
  _InterlockedCompareExchange(&hrErrorCode, hRes, S_FALSE);
  if (hCancelAsync != NULL)
    ::SetEvent(hCancelAsync);
  return;
}

VOID CIPAddressResolver::CItem::CallCallback()
{
  static SOCKADDR_INET sZeroAddr = { 0 };
  HRESULT hRes = __InterlockedRead(&hrErrorCode);

  cCallback(lpHostResolver, (SUCCEEDED(hRes) ? sAddr : sZeroAddr), hRes, lpUserData);
  return;
}

} //namespace Internals

} //namespace MX
