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
#ifndef _MX_HOSTRESOLVER_COMMON_H
#define _MX_HOSTRESOLVER_COMMON_H

#include "..\..\Include\Comm\HostResolver.h"
#include "..\..\Include\WaitableObjects.h"
#include "..\..\Include\Threads.h"
#include "..\..\Include\LinkedList.h"
#include "..\..\Include\AutoPtr.h"
#include "..\..\Include\Strings\Strings.h"
#include "..\..\Include\Finalizer.h"
#include "..\..\Include\TimedEventQueue.h"
#include "..\..\Include\Http\punycode.h"

//-----------------------------------------------------------

#define FLAG_TimedOut                                 0x0002
#define FLAG_Processing                               0x0004

//-----------------------------------------------------------

static __inline HRESULT MX_HRESULT_FROM_LASTSOCKETERROR()
{
  HRESULT hRes = MX_HRESULT_FROM_WIN32(::WSAGetLastError());
  return (hRes != MX_HRESULT_FROM_WIN32(WSAEWOULDBLOCK)) ? hRes : MX_E_IoPending;
}

static SIZE_T Helper_IPv6_Fill(_Out_ LPWORD lpnAddr, _In_z_ LPCSTR szStrA, _In_ SIZE_T nLen);
static SIZE_T Helper_IPv6_Fill(_Out_ LPWORD lpnAddr, _In_z_ LPCWSTR szStrW, _In_ SIZE_T nLen);

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CIPAddressResolver : public virtual CBaseMemObj, protected CThread
{
  MX_DISABLE_COPY_CONSTRUCTOR(CIPAddressResolver);
protected:
  CIPAddressResolver();
public:
  ~CIPAddressResolver();

  static CIPAddressResolver* Get();
  VOID Release();

  HRESULT Resolve(_In_ MX::CHostResolver *lpHostResolver, _In_ PSOCKADDR_INET lpAddr, _In_ HRESULT *lphErrorCode,
                  _In_z_ LPCSTR szHostNameA, _In_ int nDesiredFamily, _In_ DWORD dwTimeoutMs);
  VOID Cancel(_In_ MX::CHostResolver *lpHostResolver);

  HRESULT ResolveAddr(_Out_ PSOCKADDR_INET lpAddress, _In_z_ LPCSTR szAddressA, _In_ int nDesiredFamily=AF_UNSPEC);

private:
  class CItem : public CTimedEventQueue::CEvent, public TLnkLstNode<CItem>
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CItem);
  public:
    CItem(MX::CHostResolver *lpHostResolver, _In_ OnNotifyCallback cCallback) :
          CTimedEventQueue::CEvent(cCallback, NULL), TLnkLstNode<CItem>()
      {
      cHostResolver = lpHostResolver;
      lpAddr = NULL;
      nDesiredFamily = 0;
      _InterlockedExchange(&nTimeoutCalled, 0);
      return;
      };

  public:
    TAutoRefCounted<MX::CHostResolver> cHostResolver;
    PSOCKADDR_INET lpAddr;
    HRESULT *lphErrorCode;
    CStringA cStrHostNameA;
    int nDesiredFamily;
    LONG volatile nTimeoutCalled;
  };

  //----
private:
  BOOL Initialize();
  VOID AddRef();

  VOID ThreadProc();
  VOID OnTimeout(_In_ CTimedEventQueue::CEvent *lpEvent);
  BOOL ProcessQueued();
  VOID CancelAll();
  BOOL FlushCompleted();

private:
  typedef INT(WSAAPI *LPFN_GETADDRINFOW)(_In_opt_ PCWSTR pNodeName, _In_opt_ PCWSTR pServiceName,
                                          _In_opt_ const ADDRINFOW *pHints, _Deref_out_ PADDRINFOW *ppResult);
  typedef VOID (WSAAPI * LPFN_FREEADDRINFOW)(_In_opt_ PADDRINFOW pAddrInfo);

  CCriticalSection cs;
  LONG volatile nRefCount;
  CSystemTimedEventQueue *lpEventQueue;
  TLnkLst<CItem> cQueuedItemsList, cProcessedItemsList;
  CWindowsEvent cQueueChangedEv;
};

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HOSTRESOLVER_COMMON_H
