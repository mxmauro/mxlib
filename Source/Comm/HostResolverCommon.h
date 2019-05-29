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
#include "..\..\Include\RefCounted.h"
#include "..\..\Include\AutoPtr.h"
#include "..\..\Include\Strings\Strings.h"
#include "..\..\Include\Finalizer.h"
#include "..\..\Include\Http\punycode.h"
#include <ws2def.h>

//-----------------------------------------------------------

#define MAX_SIMULTANEOUS_THREADS                           4

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

class CIPAddressResolver : public TRefCounted<CThread>
{
  MX_DISABLE_COPY_CONSTRUCTOR(CIPAddressResolver);
public:
  CIPAddressResolver();
  ~CIPAddressResolver();

  static CIPAddressResolver* Get();
  static VOID Shutdown();

  HRESULT Resolve(_In_ MX::CHostResolver *lpHostResolver, _In_z_ LPCSTR szHostNameA, _In_ int nDesiredFamily,
                  _In_ DWORD dwTimeoutMs, _In_ CHostResolver::OnResultCallback cCallback, _In_ LPVOID lpUserData);
  VOID Cancel(_In_ MX::CHostResolver *lpHostResolver);

private:
  class CItem : public TRefCounted<CBaseMemObj>, public TLnkLstNode<CItem>
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CItem);
  public:
    CItem(_In_ CHostResolver *_lpHostResolver, CHostResolver::OnResultCallback _cCallback,
          _In_ LPVOID _lpUserData, _In_ int _nDesiredFamily, _In_ DWORD dwTimeoutMs);
    ~CItem();

    VOID CallCallback();

    VOID SetErrorCode(_In_ HRESULT hRes);
    HRESULT GetErrorCode() const
      {
      return __InterlockedRead(&(const_cast<CItem*>(this)->hrErrorCode));
      };

  public:
    LONG volatile nMutex;
    MX::CHostResolver *lpHostResolver;
    CHostResolver::OnResultCallback cCallback;
    LPVOID lpUserData;
    union {
      LPVOID lp;
      LPCSTR szStrA;
      LPCWSTR szStrW;
    } uHostName;
    int nDesiredFamily;
    DWORD dwTimeoutMs;
    SOCKADDR_INET sAddr;
    LONG volatile hrErrorCode;
    HANDLE hCancelAsync;
    LONG volatile nAssignedSlot;
  };

  //----
private:
  BOOL Initialize();

  VOID ThreadProc();
  CItem* GetNextItemToProcess(_In_ LONG nSlot);
  VOID ProcessMulti();
  BOOL ProcessSingle();
  VOID Process(_In_ CItem *lpItemToProcess);
  VOID ResolveItem(_In_ CItem *lpItem);
  VOID ResolverWorkerThread(_In_ SIZE_T nParam);
  VOID CancelPending();
  VOID FlushFinished();

private:
  typedef struct addrinfoexW {
    int                 ai_flags;       // AI_PASSIVE, AI_CANONNAME, AI_NUMERICHOST
    int                 ai_family;      // PF_xxx
    int                 ai_socktype;    // SOCK_xxx
    int                 ai_protocol;    // 0 or IPPROTO_xxx for IPv4 and IPv6
    size_t              ai_addrlen;     // Length of ai_addr
    PWSTR               ai_canonname;   // Canonical name for nodename
    _Field_size_bytes_(ai_addrlen) struct sockaddr *ai_addr;        // Binary address
    _Field_size_(ai_bloblen) void *ai_blob;
    size_t              ai_bloblen;
    LPGUID              ai_provider;
    struct addrinfoexW *ai_next;        // Next structure in linked list
  } ADDRINFOEXW, *PADDRINFOEXW, *LPADDRINFOEXW;

  typedef INT (WSAAPI *lpfnGetAddrInfoExW)(_In_opt_ PCWSTR pName, _In_opt_ PCWSTR pServiceName, _In_ DWORD dwNameSpace,
                                           _In_opt_ LPGUID lpNspId, _In_opt_ const ADDRINFOEXW *hints,
                                           _Outptr_ PADDRINFOEXW *ppResult, _In_opt_ struct timeval *timeout,
                                           _In_opt_ LPOVERLAPPED lpOverlapped, _In_opt_ LPVOID lpCompletionRoutine,
                                           _Out_opt_ LPHANDLE lpNameHandle);
  typedef void (WSAAPI *lpfnFreeAddrInfoExW)(PADDRINFOEXW pAddrInfoEx);
  typedef INT (WSAAPI *lpfnGetAddrInfoExCancel)(_In_ LPHANDLE lpHandle);
  typedef INT (WSAAPI *lpfnGetAddrInfoExOverlappedResult)(_In_ LPOVERLAPPED lpOverlapped);

  LONG volatile nRwMutex;
  HINSTANCE hWs2_32Dll;
  lpfnGetAddrInfoExW fnGetAddrInfoExW;
  lpfnFreeAddrInfoExW fnFreeAddrInfoExW;
  lpfnGetAddrInfoExCancel fnGetAddrInfoExCancel;
  lpfnGetAddrInfoExOverlappedResult fnGetAddrInfoExOverlappedResult;

  TLnkLst<CItem> cItemsList;
  CWindowsEvent cWakeUpWorkerEv;
  struct {
    TClassWorkerThread<CIPAddressResolver> cThread;
    LONG volatile nWorking;
  } sWorkers[MAX_SIMULTANEOUS_THREADS];
};

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HOSTRESOLVER_COMMON_H
