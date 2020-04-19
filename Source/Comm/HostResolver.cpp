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
#include "..\..\Include\Comm\HostResolver.h"
#include "..\..\Include\WaitableObjects.h"
#include "..\..\Include\Threads.h"
#include "..\..\Include\RefCounted.h"
#include "..\..\Include\AutoPtr.h"
#include "..\..\Include\Strings\Strings.h"
#include "..\..\Include\Finalizer.h"
#include "..\..\Include\Http\punycode.h"
#include "..\..\Include\RedBlackTree.h"
#include "..\Internals\SystemDll.h"
#include <ws2def.h>
#include <VersionHelpers.h>

#pragma comment(lib, "ws2_32.lib")

#define _FLAG_Canceled                                0x0001
#define _FLAG_Completed                               0x0002

#define HOSTRESOLVER_FINALIZER_PRIORITY                10020
#define MAX_ITEMS_IN_FREE_LIST                           128

#define S_DONE                               ((HRESULT)100L)

//-----------------------------------------------------------

typedef struct tagSYNC_RESOLVE {
  MX::CWindowsEvent cEvent;
  SOCKADDR_INET *lpAddr;
  HRESULT volatile hRes;
} SYNC_RESOLVE, *LPSYNC_RESOLVE;

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

//-----------------------------------------------------------

typedef INT (WSAAPI *lpfnGetAddrInfoExW)(_In_opt_ PCWSTR pName, _In_opt_ PCWSTR pServiceName, _In_ DWORD dwNameSpace,
                                         _In_opt_ LPGUID lpNspId, _In_opt_ const ADDRINFOEXW *hints,
                                         _Outptr_ PADDRINFOEXW *ppResult, _In_opt_ struct timeval *timeout,
                                         _In_opt_ LPOVERLAPPED lpOverlapped, _In_opt_ LPVOID lpCompletionRoutine,
                                         _Out_opt_ LPHANDLE lpNameHandle);
typedef void (WSAAPI *lpfnFreeAddrInfoExW)(PADDRINFOEXW pAddrInfoEx);
typedef INT (WSAAPI *lpfnGetAddrInfoExCancel)(_In_ LPHANDLE lpHandle);
typedef INT (WSAAPI *lpfnGetAddrInfoExOverlappedResult)(_In_ LPOVERLAPPED lpOverlapped);

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CHostResolver : public TRefCounted<CBaseMemObj>, public CNonCopyableObj
{
public:
  class CAsyncItem : public virtual CBaseMemObj, public CNonCopyableObj
  {
  public:
    CAsyncItem(_In_ CHostResolver *_lpResolver, _In_z_ LPCWSTR szHostNameW, _In_ int nDesiredFamily,
               _In_ DWORD dwTimeoutMs, _In_ PSOCKADDR_INET lpSockAddr, _In_ HostResolver::OnResultCallback cCallback,
               _In_ LPVOID lpUserData, _In_ lpfnFreeAddrInfoExW _fnFreeAddrInfoExW) :
               CBaseMemObj(), CNonCopyableObj()
      {
      lpResolver = _lpResolver;
      fnFreeAddrInfoExW = _fnFreeAddrInfoExW;
      hCancel = NULL;
      lpAddrInfoExW = NULL;
      ::MxMemSet(&(sOvr), 0, sizeof(sOvr));
      Setup(szHostNameW, nDesiredFamily, dwTimeoutMs, lpSockAddr, cCallback, lpUserData);
      return;
      };

    ~CAsyncItem()
      {
      ResetAsync();
      return;
      };

    VOID Setup(_In_z_ LPCWSTR szHostNameW, _In_ int _nDesiredFamily, _In_ DWORD _dwTimeoutMs,
               _In_ PSOCKADDR_INET _lpSockAddr, _In_ HostResolver::OnResultCallback _cCallback, _In_ LPVOID _lpUserData)
      {
      nId = 0;
      cStrHostNameW.Empty();
      cStrHostNameW.Copy(szHostNameW);
      nDesiredFamily = _nDesiredFamily;
      dwTimeoutMs = _dwTimeoutMs;
      cCallback = _cCallback;
      lpUserData = _lpUserData;
      _InterlockedExchange(&nFlags, 0);
      lpSockAddr = _lpSockAddr;
      hr = S_FALSE;
      lpNextInFreeList = NULL;
      return;
      };

    __inline VOID WaitUntilCompleted()
      {
      while ((__InterlockedRead(&nFlags) & _FLAG_Completed) == 0)
        ::MxSleep(1);
      return;
      };

    __inline VOID InvokeCallback()
      {
      cCallback(nId, lpSockAddr, hr, lpUserData);
      return;
      };

    VOID ResetAsync()
      {
      if (lpAddrInfoExW != NULL)
      {
        fnFreeAddrInfoExW(lpAddrInfoExW);
        lpAddrInfoExW = NULL;
      }
      hCancel = NULL;
      MxMemSet(&(sOvr), 0, sizeof(sOvr));
      return;
      };

    static int InsertCompareFunc(_In_opt_ LPVOID lpContext, _In_ CRedBlackTreeNode *lpNode1,
                                 _In_ CRedBlackTreeNode *lpNode2)
      {
      CAsyncItem *lpAsyncItem1 = CONTAINING_RECORD(lpNode1, CAsyncItem, cTreeNode);
      CAsyncItem *lpAsyncItem2 = CONTAINING_RECORD(lpNode2, CAsyncItem, cTreeNode);

      if ((ULONG)(lpAsyncItem1->nId) < (ULONG)(lpAsyncItem2->nId))
        return -1;
      if ((ULONG)(lpAsyncItem1->nId) > (ULONG)(lpAsyncItem2->nId))
        return 1;
      return 0;
      };

    static int SearchCompareFunc(_In_ LPVOID lpContext, _In_ ULONG key, _In_ CRedBlackTreeNode *lpNode)
      {
      CAsyncItem *lpAsyncItem = CONTAINING_RECORD(lpNode, CAsyncItem, cTreeNode);

      if (key < (ULONG)(lpAsyncItem->nId))
        return -1;
      if (key > (ULONG)(lpAsyncItem->nId))
        return 1;
      return 0;
      };

  public:
    CRedBlackTreeNode cTreeNode;
    CHostResolver *lpResolver;
    lpfnFreeAddrInfoExW fnFreeAddrInfoExW;
    LONG nId;
    CStringW cStrHostNameW;
    int nDesiredFamily;
    DWORD dwTimeoutMs;
    HostResolver::OnResultCallback cCallback;
    LPVOID lpUserData;
    LONG volatile nFlags;
    PSOCKADDR_INET lpSockAddr;
    HRESULT hr;
    OVERLAPPED sOvr;
    HANDLE hCancel;
    PADDRINFOEXW lpAddrInfoExW;
    CAsyncItem *lpNextInFreeList;
  };

public:
  CHostResolver();
  ~CHostResolver();

  static CHostResolver* Get();
  static VOID Shutdown();

  HRESULT AddResolver(_In_z_ LPCSTR szHostNameA, _In_ int nDesiredFamily, _Out_ PSOCKADDR_INET lpSockAddr,
                      _In_ DWORD dwTimeoutMs, _In_opt_ HostResolver::OnResultCallback cCallback,
                      _In_opt_ LPVOID lpUserData, _Out_opt_ LONG volatile *lpnResolverId);
  HRESULT AddResolver(_In_z_ LPCWSTR szHostNameW, _In_ int nDesiredFamily, _Out_ PSOCKADDR_INET lpSockAddr,
                      _In_ DWORD dwTimeoutMs, _In_opt_ HostResolver::OnResultCallback cCallback,
                      _In_opt_ LPVOID lpUserData, _Out_opt_ LONG volatile *lpnResolverId);
  HRESULT AddResolverCommon(_Out_ LONG volatile *lpnResolverId, _In_ CAsyncItem *lpNewAsyncItem);
  VOID RemoveResolver(_Out_ LONG volatile *lpnResolverId);

private:
  CAsyncItem* AllocAsyncItem(_In_ LPCWSTR szHostNameW, _In_ int nDesiredFamily, _In_ DWORD dwTimeoutMs,
                             _Out_ PSOCKADDR_INET lpSockAddr, _In_ HostResolver::OnResultCallback cCallback,
                             _In_opt_ LPVOID lpUserData);
  VOID FreeAsyncItem(_In_ CAsyncItem *lpAsyncItem);

  static VOID WINAPI AsyncQueryCompleteCallback(_In_ DWORD dwError, _In_ DWORD dwBytes, _In_ LPOVERLAPPED lpOvr);
  VOID CompleteAsync(_In_ CAsyncItem *lpAsyncItem, _In_ DWORD dwError);

  HRESULT ProcessResultsA(_In_ PSOCKADDR_INET lpSockAddr, _In_ PADDRINFOA lpAddrInfoA, _In_ int nFamily);
  HRESULT ProcessResultsExW(_In_ PSOCKADDR_INET lpSockAddr, _In_ PADDRINFOEXW lpAddrInfoExW, _In_ int nFamily);

private:
  LONG volatile nRundownLock;
  LONG volatile nNextResolverId;
  DWORD dwOsVersion;
  HINSTANCE hWs2_32Dll;
  lpfnGetAddrInfoExW fnGetAddrInfoExW;
  lpfnFreeAddrInfoExW fnFreeAddrInfoExW;
  lpfnGetAddrInfoExCancel fnGetAddrInfoExCancel;
  lpfnGetAddrInfoExOverlappedResult fnGetAddrInfoExOverlappedResult;
  struct {
    LONG volatile nMutex;
    CRedBlackTree cTree;
  } sAsyncTasks;
  struct {
    LONG volatile nMutex;
    CAsyncItem *lpFirst;
    int nListCount;
  } sFreeAsyncItems;
};

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

static MX::RWLOCK sHostResolverRwMutex = MX_RWLOCK_INIT;
static MX::Internals::CHostResolver *lpHostResolver = NULL;

//-----------------------------------------------------------

static __inline HRESULT MX_HRESULT_FROM_LASTSOCKETERROR()
{
  HRESULT hRes = MX_HRESULT_FROM_WIN32(::WSAGetLastError());
  return (hRes != MX_HRESULT_FROM_WIN32(WSAEWOULDBLOCK)) ? hRes : MX_E_IoPending;
}

static VOID OnSyncResolution(_In_ LONG nResolverId, _In_ SOCKADDR_INET &sAddr, _In_ HRESULT hrErrorCode,
                             _In_ LPVOID lpUserData);
static SIZE_T Helper_IPv6_Fill(_Out_ LPWORD lpnAddr, _In_z_ LPCSTR szStrA, _In_ SIZE_T nLen);
static SIZE_T Helper_IPv6_Fill(_Out_ LPWORD lpnAddr, _In_z_ LPCWSTR szStrW, _In_ SIZE_T nLen);

//-----------------------------------------------------------

#define __resolver (reinterpret_cast<MX::Internals::CIPAddressResolver*>(lpInternal))

//-----------------------------------------------------------

namespace MX {

namespace HostResolver {

HRESULT Resolve(_In_z_ LPCSTR szHostNameA, _In_ int nDesiredFamily, _Out_ PSOCKADDR_INET lpSockAddr,
                _In_ DWORD dwTimeoutMs, _In_opt_ OnResultCallback cCallback, _In_opt_ LPVOID lpUserData,
                _Out_opt_ LONG volatile *lpnResolverId)
{
  TAutoRefCounted<Internals::CHostResolver> cHandler;

  if (lpSockAddr != NULL)
    MxMemSet(lpSockAddr, 0, sizeof(SOCKADDR_INET));
  if (lpnResolverId != NULL)
    _InterlockedExchange(lpnResolverId, 0);

  cHandler.Attach(Internals::CHostResolver::Get());
  if (!cHandler)
    return E_OUTOFMEMORY;
  return cHandler->AddResolver(szHostNameA, nDesiredFamily, lpSockAddr, dwTimeoutMs, cCallback, lpUserData,
                               lpnResolverId);
}

HRESULT Resolve(_In_z_ LPCWSTR szHostNameW, _In_ int nDesiredFamily, _Out_ PSOCKADDR_INET lpSockAddr,
                _In_ DWORD dwTimeoutMs, _In_opt_ OnResultCallback cCallback, _In_opt_ LPVOID lpUserData,
                _Out_opt_ LONG volatile *lpnResolverId)
{
  TAutoRefCounted<Internals::CHostResolver> cHandler;

  if (lpSockAddr != NULL)
    MxMemSet(lpSockAddr, 0, sizeof(SOCKADDR_INET));
  if (lpnResolverId != NULL)
    _InterlockedExchange(lpnResolverId, 0);

  cHandler.Attach(Internals::CHostResolver::Get());
  if (!cHandler)
    return E_OUTOFMEMORY;
  return cHandler->AddResolver(szHostNameW, nDesiredFamily, lpSockAddr, dwTimeoutMs, cCallback, lpUserData,
                               lpnResolverId);
}

VOID Cancel(_Inout_ LONG volatile *lpnResolverId)
{
  TAutoRefCounted<Internals::CHostResolver> cHandler;

  cHandler.Attach(Internals::CHostResolver::Get());
  if (cHandler)
    cHandler->RemoveResolver(lpnResolverId);
  else if (lpnResolverId != NULL)
    _InterlockedExchange(lpnResolverId, 0);
  return;
}

BOOL IsValidIPV4(_In_z_ LPCSTR szAddressA, _In_opt_ SIZE_T nAddressLen, _Out_opt_ PSOCKADDR_INET lpAddress)
{
  SIZE_T i, j, nLen, nBlocksCount;
  DWORD dw, dwBase, dwValue, dwTempValue;
  LPCSTR sA;
  struct {
    LPCSTR szStartA;
    SIZE_T nLen;
  } sBlocks[4];
  SOCKADDR_INET sTempAddr;

  if (lpAddress == NULL)
    lpAddress = &sTempAddr;
  ::MxMemSet(lpAddress, 0, sizeof(SOCKADDR_INET));
  lpAddress->si_family = AF_INET;
  if (szAddressA == NULL)
    return FALSE;
  if (nAddressLen == (SIZE_T)-1)
    nAddressLen = MX::StrLenA(szAddressA);
  for (nBlocksCount = 0; nBlocksCount < 4; nBlocksCount++)
  {
    sBlocks[nBlocksCount].szStartA = szAddressA;
    for (nLen = 0;
         nLen < nAddressLen && szAddressA[nLen] != '.' && szAddressA[nLen] != '/' && szAddressA[nLen] != '%';
         nLen++);
    {
      sBlocks[nBlocksCount].nLen = nLen;
    }
    if (nLen >= nAddressLen || szAddressA[nLen] != '.')
    {
      nBlocksCount++;
      break;
    }
    if (nBlocksCount == 3)
      return FALSE;
    szAddressA += nLen + 1;
    nAddressLen -= nLen + 1;
  }
  if (nBlocksCount < 2)
    return FALSE;
  for (i = 0; i < nBlocksCount; i++)
  {
    dwValue = 0;
    dwBase = 10;
    sA = sBlocks[i].szStartA;
    if (sBlocks[i].nLen == 0)
      return FALSE;
    if (sA[0] == '0' && sBlocks[i].nLen >= 2)
    {
      if (sA[1] == 'X' || sA[1] == 'x')
      {
        dwBase = 16;
        sA += 2;
        if ((sBlocks[i].nLen -= 2) == 0)
          return FALSE;
      }
      else
      {
        dwBase = 8;
        sA++;
        sBlocks[i].nLen--;
      }
    }
    for (j = 0; j < sBlocks[i].nLen; j++,sA++)
    {
      if (*sA >= '0' && *sA <= '9')
        dw = (DWORD)(*sA - '0');
      else if (*sA >= 'A' && *sA <= 'F')
        dw = (DWORD)(*sA - 'A') + 10;
      else if (*sA >= 'a' && *sA <= 'f')
        dw = (DWORD)(*sA - 'a') + 10;
      else
        return FALSE;
      if (dw >= dwBase)
        return FALSE;
      dwTempValue = dwValue * dwBase;
      if (dwTempValue < dwValue)
        return FALSE; //if overflow, not a valid ipv4 address
      dwValue = dwTempValue + dw;
      if (dwValue < dwTempValue)
        return FALSE; //if overflow, not a valid ipv4 address
    }
    if (i < nBlocksCount - 1)
    {
      if (dwValue > 255)
        return FALSE;
    }
    else
    {
      i = 3;
    }
    if (lpAddress != NULL)
    {
      LPBYTE d = (LPBYTE)&(lpAddress->Ipv4.sin_addr.S_un.S_un_b.s_b1);
      for (j = 0; dwValue > 0; j++,dwValue /= 0x100)
        d[i - j] = (BYTE)(dwValue & 0xFF);
    }
  }
  return TRUE;
}

BOOL IsValidIPV4(_In_z_ LPCWSTR szAddressW, _In_opt_ SIZE_T nAddressLen, _Out_opt_ PSOCKADDR_INET lpAddress)
{
  SIZE_T i, j, nLen, nBlocksCount;
  DWORD dw, dwBase, dwValue, dwTempValue;
  LPCWSTR sW;
  struct {
    LPCWSTR szStartW;
    SIZE_T nLen;
  } sBlocks[4];
  SOCKADDR_INET sTempAddr;

  if (lpAddress == NULL)
    lpAddress = &sTempAddr;
  ::MxMemSet(lpAddress, 0, sizeof(SOCKADDR_INET));
  lpAddress->si_family = AF_INET;
  if (szAddressW == NULL)
    return FALSE;
  if (nAddressLen == (SIZE_T)-1)
    nAddressLen = MX::StrLenW(szAddressW);
  for (nBlocksCount = 0; nBlocksCount < 4; nBlocksCount++)
  {
    sBlocks[nBlocksCount].szStartW = szAddressW;
    for (nLen = 0;
         nLen < nAddressLen && szAddressW[nLen] != L'.' && szAddressW[nLen] != L'/' && szAddressW[nLen] != L'%';
         nLen++);
    {
      sBlocks[nBlocksCount].nLen = nLen;
    }
    if (nLen >= nAddressLen || szAddressW[nLen] != L'.')
    {
      nBlocksCount++;
      break;
    }
    if (nBlocksCount == 3)
      return FALSE;
    szAddressW += nLen + 1;
    nAddressLen -= nLen + 1;
  }
  if (nBlocksCount < 2)
    return FALSE;
  for (i = 0; i < nBlocksCount; i++)
  {
    dwValue = 0;
    dwBase = 10;
    sW = sBlocks[i].szStartW;
    if (sBlocks[i].nLen == 0)
      return FALSE;
    if (sW[0] == L'0' && sBlocks[i].nLen >= 2)
    {
      if (sW[1] == L'X' || sW[1] == L'x')
      {
        dwBase = 16;
        sW += 2;
        if ((sBlocks[i].nLen -= 2) == 0)
          return FALSE;
      }
      else
      {
        dwBase = 8;
        sW++;
        sBlocks[i].nLen--;
      }
    }
    for (j = 0; j < sBlocks[i].nLen; j++,sW++)
    {
      if (*sW >= L'0' && *sW <= L'9')
        dw = (DWORD)(*sW - L'0');
      else if (*sW >= L'A' && *sW <= L'F')
        dw = (DWORD)(*sW - L'A') + 10;
      else if (*sW >= L'a' && *sW <= L'f')
        dw = (DWORD)(*sW - L'a') + 10;
      else
        return FALSE;
      if (dw >= dwBase)
        return FALSE;
      dwTempValue = dwValue * dwBase;
      if (dwTempValue < dwValue)
        return FALSE; //if overflow, not a valid ipv4 address
      dwValue = dwTempValue + dw;
      if (dwValue < dwTempValue)
        return FALSE; //if overflow, not a valid ipv4 address
    }
    if (i < nBlocksCount-1)
    {
      if (dwValue > 255)
        return FALSE;
    }
    else
    {
      i = 3;
    }
    if (lpAddress != NULL)
    {
      LPBYTE d = (LPBYTE)&(lpAddress->Ipv4.sin_addr.S_un.S_un_b.s_b1);
      for (j = 0; dwValue > 0; j++,dwValue /= 0x100)
        d[i - j] = (BYTE)(dwValue & 0xFF);
    }
  }
  return TRUE;
}

BOOL IsValidIPV6(_In_z_ LPCSTR szAddressA, _In_opt_ SIZE_T nAddressLen, _Out_opt_ PSOCKADDR_INET lpAddress)
{
  SIZE_T i, k, nSlots, nLastColonPos;
  BOOL bIPv4, bIPv6;
  LPCSTR sA;
  SOCKADDR_INET sTempAddr;
  LPWORD w;

  if (lpAddress == NULL)
    lpAddress = &sTempAddr;
  ::MxMemSet(lpAddress, 0, sizeof(SOCKADDR_INET));
  lpAddress->si_family = AF_INET6;
  if (szAddressA == NULL)
    return FALSE;
  if (nAddressLen == (SIZE_T)-1)
    nAddressLen = StrLenA(szAddressA);
  w = lpAddress->Ipv6.sin6_addr.u.Word;
  if (nAddressLen >= 2 && szAddressA[0] == '[' && szAddressA[nAddressLen - 1] == ']')
  {
    szAddressA++;
    nAddressLen -= 2;
  }
  for (i = 0; i < nAddressLen; i++)
  {
    if (szAddressA[i] == '/' || szAddressA[i] == '%')
    {
      nAddressLen = i;
      break;
    }
  }
  if (nAddressLen < 2)
    return FALSE;
  //IPv4 inside?
  for (nLastColonPos = nAddressLen; nLastColonPos > 0; nLastColonPos--)
  {
    if (szAddressA[nLastColonPos - 1] == ':')
      break;
  }
  if (nLastColonPos == 0)
    return FALSE;
  nLastColonPos--;
  bIPv4 = FALSE;
  nSlots = 0;
  if (nLastColonPos < nAddressLen - 1)
  {
    SOCKADDR_INET sTempAddrV4;

    if (IsValidIPV4(szAddressA + nLastColonPos + 1, nAddressLen - nLastColonPos - 1, &sTempAddrV4) != FALSE)
    {
      w[6] = sTempAddrV4.Ipv4.sin_addr.S_un.S_un_w.s_w1;
      w[7] = sTempAddrV4.Ipv4.sin_addr.S_un.S_un_w.s_w2;
      nAddressLen = nLastColonPos;
      if (nLastColonPos > 0 && szAddressA[nLastColonPos - 1] == ':')
        nAddressLen++;
      bIPv4 = TRUE;
      nSlots = 2;
    }
  }
  // Only an bIPv6 block remains, either:
  // "hexnumbers::hexnumbers", "hexnumbers::" or "hexnumbers"
  sA = MX::StrFindA(szAddressA, "::");
  if (sA != NULL)
  {
    SIZE_T nPos, nLeftSlots, nRightSlots;

    nPos = (SIZE_T)(sA - szAddressA);
    nRightSlots = Helper_IPv6_Fill(w, szAddressA + nPos + 2, nAddressLen - nPos - 2);
    if (nRightSlots == (SIZE_T)-1 || nRightSlots + nSlots > 8)
      return FALSE;
    k = 8 - nSlots - nRightSlots;
    for (i = nRightSlots; i > 0; i--)
    {
      w[i + k - 1] = w[i - 1];
      w[i - 1] = 0;
    }
    nLeftSlots = Helper_IPv6_Fill(w, szAddressA, nPos);
    if (nLeftSlots == (SIZE_T)-1 || nLeftSlots + nRightSlots + nSlots > 7)
      return FALSE;
  }
  else
  {
    if (Helper_IPv6_Fill(w, szAddressA, nAddressLen) != 8 - nSlots)
      return FALSE;
  }
  //Now check the results in the bIPv6-address range only
  bIPv6 = FALSE;
  for (i = 0; i < nSlots; i++)
  {
    if (w[i] != 0 || (i == 5 && w[i] != 0xFFFF))
      bIPv6 = TRUE;
  }
  //check IPv4 validity
  if (bIPv4 != FALSE && bIPv6 == FALSE)
  {
    for (i = 0; i < 5; i++)
    {
      if (w[i] != 0)
        return FALSE;
    }
    if (w[5] != 0 && w[5] != 0xFFFF)
      return FALSE;
  }
  return TRUE;
}

BOOL IsValidIPV6(_In_z_ LPCWSTR szAddressW, _In_opt_ SIZE_T nAddressLen, _Out_opt_ PSOCKADDR_INET lpAddress)
{
  SIZE_T i, k, nSlots, nLastColonPos;
  BOOL bIPv4, bIPv6;
  LPCWSTR sW;
  SOCKADDR_INET sTempAddr;
  LPWORD w;

  if (lpAddress == NULL)
    lpAddress = &sTempAddr;
  ::MxMemSet(lpAddress, 0, sizeof(SOCKADDR_INET));
  lpAddress->si_family = AF_INET6;
  if (szAddressW == NULL)
    return FALSE;
  if (nAddressLen == (SIZE_T)-1)
    nAddressLen = StrLenW(szAddressW);
  w = lpAddress->Ipv6.sin6_addr.u.Word;
  if (nAddressLen >= 2 && szAddressW[0] == L'[' && szAddressW[nAddressLen - 1] == L']')
  {
    szAddressW++;
    nAddressLen -= 2;
  }
  for (i = 0; i < nAddressLen; i++)
  {
    if (szAddressW[i] == L'/' || szAddressW[i] == L'%')
    {
      nAddressLen = i;
      break;
    }
  }
  if (nAddressLen < 2)
    return FALSE;
  //IPv4 inside?
  for (nLastColonPos = nAddressLen; nLastColonPos > 0; nLastColonPos--)
  {
    if (szAddressW[nLastColonPos - 1] == L':')
      break;
  }
  if (nLastColonPos == 0)
    return FALSE;
  nLastColonPos--;
  bIPv4 = FALSE;
  nSlots = 0;
  if (nLastColonPos < nAddressLen - 1)
  {
    SOCKADDR_INET sTempAddrV4;

    if (IsValidIPV4(szAddressW + nLastColonPos + 1, nAddressLen - nLastColonPos - 1, &sTempAddrV4) != FALSE)
    {
      w[6] = sTempAddrV4.Ipv4.sin_addr.S_un.S_un_w.s_w1;
      w[7] = sTempAddrV4.Ipv4.sin_addr.S_un.S_un_w.s_w2;
      nAddressLen = nLastColonPos;
      if (nLastColonPos > 0 && szAddressW[nLastColonPos - 1] == L':')
        nAddressLen++;
      bIPv4 = TRUE;
      nSlots = 2;
    }
  }
  // Only an bIPv6 block remains, either:
  // "hexnumbers::hexnumbers", "hexnumbers::" or "hexnumbers"
  sW = MX::StrFindW(szAddressW, L"::");
  if (sW != NULL)
  {
    SIZE_T nPos, nLeftSlots, nRightSlots;

    nPos = (SIZE_T)(sW - szAddressW);
    nRightSlots = Helper_IPv6_Fill(w, szAddressW + nPos + 2, nAddressLen - nPos - 2);
    if (nRightSlots == (SIZE_T)-1 || nRightSlots + nSlots > 8)
      return FALSE;
    k = 8 - nSlots - nRightSlots;
    for (i = nRightSlots; i > 0; i--)
    {
      w[i + k - 1] = w[i - 1];
      w[i - 1] = 0;
    }
    nLeftSlots = Helper_IPv6_Fill(w, szAddressW, nPos);
    if (nLeftSlots == (SIZE_T)-1 || nLeftSlots + nRightSlots + nSlots > 7)
      return FALSE;
  }
  else
  {
    if (Helper_IPv6_Fill(w, szAddressW, nAddressLen) != 8 - nSlots)
      return FALSE;
  }
  //Now check the results in the bIPv6-address range only
  bIPv6 = FALSE;
  for (i = 0; i < nSlots; i++)
  {
    if (w[i] != 0 || (i == 5 && w[i] != 0xFFFF))
      bIPv6 = TRUE;
  }
  //check IPv4 validity
  if (bIPv4 != FALSE && bIPv6 == FALSE)
  {
    for (i = 0; i < 5; i++)
    {
      if (w[i] != 0)
        return FALSE;
    }
    if (w[5] != 0 && w[5] != 0xFFFF)
      return FALSE;
  }
  return TRUE;
}

HRESULT FormatAddress(_In_ PSOCKADDR_INET lpAddress, _Out_ CStringA &cStrDestA)
{
  CStringW cStrTempW;
  HRESULT hRes;

  hRes = FormatAddress(lpAddress, cStrTempW);
  if (SUCCEEDED(hRes))
  {
    if (cStrDestA.CopyN((LPCWSTR)cStrTempW, cStrTempW.GetLength()) == FALSE)
      hRes = E_OUTOFMEMORY;
  }
  if (FAILED(hRes))
    cStrDestA.Empty();
  return hRes;
}

HRESULT FormatAddress(_In_ PSOCKADDR_INET lpAddress, _Out_ CStringW &cStrDestW)
{
  cStrDestW.Empty();

  if (lpAddress == NULL)
    return E_POINTER;
  switch (lpAddress->si_family)
  {
    case AF_INET:
      if (cStrDestW.Format(L"%lu.%lu.%lu.%lu", lpAddress->Ipv4.sin_addr.S_un.S_un_b.s_b1,
                           lpAddress->Ipv4.sin_addr.S_un.S_un_b.s_b2, lpAddress->Ipv4.sin_addr.S_un.S_un_b.s_b3,
                           lpAddress->Ipv4.sin_addr.S_un.S_un_b.s_b4) == FALSE)
      {
        return E_OUTOFMEMORY;
      }
      break;

    case AF_INET6:
      {
      LPWORD lpW;
      SIZE_T nOfs, nSeqStart, nSeqLen;

      //lookup for the longest sequence of zeroes
      lpW = lpAddress->Ipv6.sin6_addr.u.Word;
      nSeqStart = nSeqLen = nOfs = 0;
      while (nOfs < 8)
      {
        if (lpW[nOfs] == 0)
        {
          SIZE_T nCurrSeqStart = nOfs;
          SIZE_T nCurrSeqLen = 0;
          while (nOfs < 8 && lpW[nOfs] == 0)
          {
            nCurrSeqLen++;
            nOfs++;
          }
          if (nCurrSeqLen > nSeqLen)
          {
            nSeqStart = nCurrSeqStart;
            nSeqLen = nCurrSeqLen;
          }
        }
        else
        {
          nOfs++;
        }
      }
      //start formatting
      if (nSeqLen < 2)
        nSeqLen = 0;
      //::
      //::1:2:3:4
      //1:2:3:4::
      //1:2::3:4
      if (nSeqStart == 0)
      {
        if (cStrDestW.ConcatN(L":", 1) == FALSE)
          return E_OUTOFMEMORY;
      }
      else
      {
        for (nOfs = 0; nOfs < nSeqStart; nOfs++)
        {
          if (cStrDestW.AppendFormat(L"%x:", lpW[nOfs]) == FALSE)
            return E_OUTOFMEMORY;
        }
      }
      if (cStrDestW.ConcatN(L":", 1) == FALSE)
        return E_OUTOFMEMORY;
      for (nOfs = nSeqStart + nSeqLen; nOfs < 7; nOfs++)
      {
        if (cStrDestW.AppendFormat(L"%x:", lpW[nOfs]) == FALSE)
          return E_OUTOFMEMORY;
      }
      if (nOfs < 8)
      {
        if (cStrDestW.AppendFormat(L"%x", lpW[nOfs]) == FALSE)
          return E_OUTOFMEMORY;
      }
      }
      break;

    default:
      return MX_E_Unsupported;
  }
  //done
  return S_OK;
}

} //namespace HostResolver

} //namespace MX

//-----------------------------------------------------------

namespace MX {

namespace Internals {

CHostResolver::CHostResolver() : TRefCounted<CBaseMemObj>(), CNonCopyableObj()
{
  RundownProt_Initialize(&nRundownLock);
  _InterlockedExchange(&nNextResolverId, 0);

  _InterlockedExchange(&(sAsyncTasks.nMutex), 0);
  MxMemSet(&sFreeAsyncItems, 0, sizeof(sFreeAsyncItems));

  if (::IsWindows8OrGreater() != FALSE)
    dwOsVersion = 0x0800;
  else if (::IsWindows7OrGreater() != FALSE)
    dwOsVersion = 0x0700;
  else
    dwOsVersion = 0x0600;

  hWs2_32Dll = NULL;
  fnGetAddrInfoExW = NULL;
  fnFreeAddrInfoExW = NULL;
  fnGetAddrInfoExCancel = NULL;
  fnGetAddrInfoExOverlappedResult = NULL;

  //load library
  if (SUCCEEDED(Internals::LoadSystemDll(L"ws2_32.dll", &hWs2_32Dll)))
  {
    fnGetAddrInfoExW = (lpfnGetAddrInfoExW)::GetProcAddress(hWs2_32Dll, "GetAddrInfoExW");
    fnFreeAddrInfoExW = (lpfnFreeAddrInfoExW)::GetProcAddress(hWs2_32Dll, "FreeAddrInfoExW");
    if (dwOsVersion >= 0x0800)
    {
      fnGetAddrInfoExCancel = (lpfnGetAddrInfoExCancel)::GetProcAddress(hWs2_32Dll, "GetAddrInfoExCancel");
      fnGetAddrInfoExOverlappedResult =
          (lpfnGetAddrInfoExOverlappedResult)::GetProcAddress(hWs2_32Dll, "GetAddrInfoExOverlappedResult");
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
  return;
}

CHostResolver::~CHostResolver()
{
  CRedBlackTreeNode *lpNode;
  CAsyncItem *lpAsyncItem;

  RundownProt_WaitForRelease(&nRundownLock);

  //remove items in list
  while ((lpNode = sAsyncTasks.cTree.GetFirst()) != NULL)
  {
    lpAsyncItem = CONTAINING_RECORD(lpNode, CAsyncItem, cTreeNode);

    LONG volatile nId = lpAsyncItem->nId;
    RemoveResolver(&nId);
  }

  //free list of items
  while ((lpAsyncItem = sFreeAsyncItems.lpFirst) != NULL)
  {
    sFreeAsyncItems.lpFirst = lpAsyncItem->lpNextInFreeList;
    delete lpAsyncItem;
  }

  //free library
  if (hWs2_32Dll != NULL)
    ::FreeLibrary(hWs2_32Dll);
  return;
}

CHostResolver* CHostResolver::Get()
{
  CAutoSlimRWLShared cLock(&sHostResolverRwMutex);

  if (lpHostResolver == NULL)
  {
    cLock.UpgradeToExclusive();

    if (lpHostResolver == NULL)
    {
      CHostResolver *_lpHostResolver;

      _lpHostResolver = MX_DEBUG_NEW CHostResolver();
      if (_lpHostResolver == NULL)
        return NULL;
      //register shutdown callback
      if (FAILED(MX::RegisterFinalizer(&CHostResolver::Shutdown, HOSTRESOLVER_FINALIZER_PRIORITY)))
      {
        _lpHostResolver->Release();
        return NULL;
      }
      lpHostResolver = _lpHostResolver;
    }
  }
  lpHostResolver->AddRef();
  return lpHostResolver;
}

VOID CHostResolver::Shutdown()
{
  CAutoSlimRWLExclusive cLock(&sHostResolverRwMutex);

  MX_RELEASE(lpHostResolver);
  return;
}

HRESULT CHostResolver::AddResolver(_In_z_ LPCSTR szHostNameA, _In_ int nDesiredFamily, _Out_ PSOCKADDR_INET lpSockAddr,
                                   _In_ DWORD dwTimeoutMs, _In_opt_ HostResolver::OnResultCallback cCallback,
                                   _In_opt_ LPVOID lpUserData, _Out_opt_ LONG volatile *lpnResolverId)
{
  TAutoDeletePtr<CAsyncItem> cNewAsyncItem;
  HRESULT hRes;

  if (szHostNameA == NULL || lpSockAddr == NULL)
    return E_POINTER;
  if (*szHostNameA == 0 || (nDesiredFamily != AF_UNSPEC && nDesiredFamily != AF_INET && nDesiredFamily != AF_INET6))
    return E_INVALIDARG;

  {
    CAutoRundownProtection cAutoRundownProt(&nRundownLock);
    CStringW cStrTempW;

    if (cAutoRundownProt.IsAcquired() == FALSE)
      return MX_E_NotReady;

    if (nDesiredFamily == AF_INET || nDesiredFamily == AF_UNSPEC)
    {
      if (HostResolver::IsValidIPV4(szHostNameA, StrLenA(szHostNameA), lpSockAddr) != FALSE)
        return S_OK;
    }
    if (nDesiredFamily == AF_INET6 || nDesiredFamily == AF_UNSPEC)
    {
      if (HostResolver::IsValidIPV6(szHostNameA, StrLenA(szHostNameA), lpSockAddr) != FALSE)
        return S_OK;
    }

    //solve sync if no callback or not cancelable function
    if ((!cCallback) || fnGetAddrInfoExW == NULL || fnGetAddrInfoExCancel == NULL)
    {
      if (fnGetAddrInfoExW != NULL)
      {
        PADDRINFOEXW lpAddrInfoExW;
        ADDRINFOEXW sHintAddrInfoExW;
        timeval tv;
        INT res;

        if (cStrTempW.Copy(szHostNameA) == FALSE)
          return E_OUTOFMEMORY;

        tv.tv_sec = (long)(dwTimeoutMs / 1000);
        tv.tv_usec = (long)(dwTimeoutMs % 1000);

        MxMemSet(&sHintAddrInfoExW, 0, sizeof(sHintAddrInfoExW));
        sHintAddrInfoExW.ai_family = nDesiredFamily;
        lpAddrInfoExW = NULL;
        //NOTE: Windows 7 does NOT support timeout at all despite the documentation says a different thing.
        res = fnGetAddrInfoExW((LPCWSTR)cStrTempW, NULL, NS_DNS, NULL, &sHintAddrInfoExW, &lpAddrInfoExW,
                               ((dwOsVersion >= 0x0800 && dwTimeoutMs != INFINITE) ? &tv : NULL), NULL, NULL, NULL);
        if (res == NO_ERROR)
        {
          //process results
          hRes = ProcessResultsExW(lpSockAddr, lpAddrInfoExW, nDesiredFamily);

          //free results
          fnFreeAddrInfoExW(lpAddrInfoExW);
        }
        else
        {
          hRes = MX_HRESULT_FROM_WIN32((DWORD)res);
        }
      }
      else
      {
        PADDRINFOA lpAddrInfoA;
        ADDRINFOA sHintAddrInfoA;

        MxMemSet(&sHintAddrInfoA, 0, sizeof(sHintAddrInfoA));
        sHintAddrInfoA.ai_family = nDesiredFamily;
        lpAddrInfoA = NULL;
        if (::getaddrinfo(szHostNameA, NULL, &sHintAddrInfoA, &lpAddrInfoA) != SOCKET_ERROR)
        {
          //process results
          hRes = ProcessResultsA(lpSockAddr, lpAddrInfoA, nDesiredFamily);

          //free results
          ::freeaddrinfo(lpAddrInfoA);
        }
        else
        {
          hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
        }
      }
      //done
      return hRes;
    }

    if (lpnResolverId == NULL)
      return E_INVALIDARG;

    //create a new async item
    if (cStrTempW.Copy(szHostNameA) == FALSE)
      return E_OUTOFMEMORY;
    cNewAsyncItem.Attach(AllocAsyncItem((LPCWSTR)cStrTempW, nDesiredFamily, dwTimeoutMs, lpSockAddr, cCallback,
                                        lpUserData));
    if (!cNewAsyncItem)
      return E_OUTOFMEMORY;

    //jump to common code
    return AddResolverCommon(lpnResolverId, cNewAsyncItem.Detach());
  }
}

HRESULT CHostResolver::AddResolver(_In_z_ LPCWSTR szHostNameW, _In_ int nDesiredFamily, _Out_ PSOCKADDR_INET lpSockAddr,
                                   _In_ DWORD dwTimeoutMs, _In_opt_ HostResolver::OnResultCallback cCallback,
                                   _In_opt_ LPVOID lpUserData, _Out_opt_ LONG volatile *lpnResolverId)
{
  TAutoDeletePtr<CAsyncItem> cNewAsyncItem;
  HRESULT hRes;

  if (szHostNameW == NULL || lpSockAddr == NULL)
    return E_POINTER;
  if (*szHostNameW == 0 || (nDesiredFamily != AF_UNSPEC && nDesiredFamily != AF_INET && nDesiredFamily != AF_INET6))
    return E_INVALIDARG;

  {
    CAutoRundownProtection cAutoRundownProt(&nRundownLock);

    if (cAutoRundownProt.IsAcquired() == FALSE)
      return MX_E_NotReady;

    if (nDesiredFamily == AF_INET || nDesiredFamily == AF_UNSPEC)
    {
      if (HostResolver::IsValidIPV4(szHostNameW, StrLenW(szHostNameW), lpSockAddr) != FALSE)
        return S_OK;
    }
    if (nDesiredFamily == AF_INET6 || nDesiredFamily == AF_UNSPEC)
    {
      if (HostResolver::IsValidIPV6(szHostNameW, StrLenW(szHostNameW), lpSockAddr) != FALSE)
        return S_OK;
    }

    //solve sync if no callback or not cancelable function
    if ((!cCallback) || fnGetAddrInfoExW == NULL || fnGetAddrInfoExCancel == NULL)
    {
      if (fnGetAddrInfoExW != NULL)
      {
        PADDRINFOEXW lpAddrInfoExW;
        ADDRINFOEXW sHintAddrInfoExW;
        timeval tv;
        INT res;

        tv.tv_sec = (long)(dwTimeoutMs / 1000);
        tv.tv_usec = (long)(dwTimeoutMs % 1000);

        MxMemSet(&sHintAddrInfoExW, 0, sizeof(sHintAddrInfoExW));
        sHintAddrInfoExW.ai_family = nDesiredFamily;
        lpAddrInfoExW = NULL;
        //NOTE: Windows 7 does NOT support timeout at all despite the documentation says a different thing.
        res = fnGetAddrInfoExW(szHostNameW, NULL, NS_DNS, NULL, &sHintAddrInfoExW, &lpAddrInfoExW,
                               ((dwOsVersion >= 0x0800 && dwTimeoutMs != INFINITE) ? &tv : NULL), NULL, NULL, NULL);
        if (res == NO_ERROR)
        {
          //process results
          hRes = ProcessResultsExW(lpSockAddr, lpAddrInfoExW, nDesiredFamily);

          //free results
          fnFreeAddrInfoExW(lpAddrInfoExW);
        }
        else
        {
          hRes = MX_HRESULT_FROM_WIN32((DWORD)res);
        }
      }
      else
      {
        PADDRINFOA lpAddrInfoA;
        ADDRINFOA sHintAddrInfoA;
        CStringA cStrTempA;

        hRes = Punycode_Encode(cStrTempA, szHostNameW);
        if (FAILED(hRes))
          return hRes;

        MxMemSet(&sHintAddrInfoA, 0, sizeof(sHintAddrInfoA));
        sHintAddrInfoA.ai_family = nDesiredFamily;
        lpAddrInfoA = NULL;
        if (::getaddrinfo((LPCSTR)cStrTempA, NULL, &sHintAddrInfoA, &lpAddrInfoA) != SOCKET_ERROR)
        {
          //process results
          hRes = ProcessResultsA(lpSockAddr, lpAddrInfoA, nDesiredFamily);

          //free results
          ::freeaddrinfo(lpAddrInfoA);
        }
        else
        {
          hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
        }
      }
    }

    if (lpnResolverId == NULL)
      return E_INVALIDARG;

    //create a new async item
    cNewAsyncItem.Attach(AllocAsyncItem(szHostNameW, nDesiredFamily, dwTimeoutMs, lpSockAddr, cCallback, lpUserData));
    if (!cNewAsyncItem)
      return E_OUTOFMEMORY;

    //jump to common code
    return AddResolverCommon(lpnResolverId, cNewAsyncItem.Detach());
  }
}

HRESULT CHostResolver::AddResolverCommon(_Out_ LONG volatile *lpnResolverId, _In_ CAsyncItem *lpNewAsyncItem)
{
  //assign resolver id
  do
  {
    lpNewAsyncItem->nId = _InterlockedIncrement(&nNextResolverId);
  }
  while (lpNewAsyncItem->nId == 0);

  //insert into tree
  {
    CFastLock cQueueLock(&(sAsyncTasks.nMutex));
    ADDRINFOEXW sHintAddrInfoExW;
    timeval tv;
    INT res;

    _InterlockedExchange(lpnResolverId, lpNewAsyncItem->nId);

    sAsyncTasks.cTree.Insert(&(lpNewAsyncItem->cTreeNode), &CAsyncItem::InsertCompareFunc, TRUE);

    tv.tv_sec = (long)(lpNewAsyncItem->dwTimeoutMs / 1000);
    tv.tv_usec = (long)(lpNewAsyncItem->dwTimeoutMs % 1000);

    MxMemSet(&sHintAddrInfoExW, 0, sizeof(sHintAddrInfoExW));
    sHintAddrInfoExW.ai_family = lpNewAsyncItem->nDesiredFamily;
    //NOTE: If we are here, we are using Windows 8+
    res = fnGetAddrInfoExW((LPCWSTR)(lpNewAsyncItem->cStrHostNameW), NULL, NS_DNS, NULL, &sHintAddrInfoExW,
                           &(lpNewAsyncItem->lpAddrInfoExW), ((lpNewAsyncItem->dwTimeoutMs != INFINITE) ? &tv : NULL),
                           &(lpNewAsyncItem->sOvr), &CHostResolver::AsyncQueryCompleteCallback,
                           &(lpNewAsyncItem->hCancel));
    if (res != WSA_IO_PENDING) // WSA_IO_PENDING == ERROR_IO_PENDING
    {
      sAsyncTasks.cTree.Remove(&(lpNewAsyncItem->cTreeNode));
      FreeAsyncItem(lpNewAsyncItem);
      return MX_HRESULT_FROM_WIN32((DWORD)res);
    }
  }

  //done
  return MX_E_IoPending;
}

VOID CHostResolver::RemoveResolver(_Out_ LONG volatile *lpnResolverId)
{
  if (lpnResolverId != NULL)
  {
    ULONG nResolver = (ULONG)_InterlockedExchange(lpnResolverId, 0);
    CAsyncItem *lpAsyncItem = NULL;

    if (nResolver != 0)
    {
      CFastLock cQueueLock(&(sAsyncTasks.nMutex));
      CRedBlackTreeNode *lpNode;

      lpNode = sAsyncTasks.cTree.Find(nResolver, &CAsyncItem::SearchCompareFunc);
      if (lpNode != NULL)
      {
        lpAsyncItem = CONTAINING_RECORD(lpNode, CAsyncItem, cTreeNode);

        //only running async tasks are in the tree and can be canceled
        _InterlockedOr(&(lpAsyncItem->nFlags), _FLAG_Canceled);

        lpNode->Remove();
      }
    }

    if (lpAsyncItem != NULL)
    {
      fnGetAddrInfoExCancel(&(lpAsyncItem->hCancel));

      lpAsyncItem->WaitUntilCompleted();
      FreeAsyncItem(lpAsyncItem);
    }
  }
  return;
}

CHostResolver::CAsyncItem* CHostResolver::AllocAsyncItem(_In_ LPCWSTR szHostNameW, _In_ int nDesiredFamily,
                                                         _In_ DWORD dwTimeoutMs, _Out_ PSOCKADDR_INET lpSockAddr,
                                                         _In_ HostResolver::OnResultCallback cCallback,
                                                         _In_opt_ LPVOID lpUserData)
{
  CFastLock cLock(&(sFreeAsyncItems.nMutex));
  CAsyncItem *lpAsyncItem;

  if (sFreeAsyncItems.lpFirst != NULL)
  {
    MX_ASSERT(sFreeAsyncItems.nListCount > 0);
    (sFreeAsyncItems.nListCount)--;

    lpAsyncItem = sFreeAsyncItems.lpFirst;
    sFreeAsyncItems.lpFirst = lpAsyncItem->lpNextInFreeList;

    lpAsyncItem->Setup(szHostNameW, nDesiredFamily, dwTimeoutMs, lpSockAddr, cCallback, lpUserData);
  }
  else
  {
    lpAsyncItem = MX_DEBUG_NEW CAsyncItem(this, szHostNameW, nDesiredFamily, dwTimeoutMs, lpSockAddr, cCallback,
                                          lpUserData, fnFreeAddrInfoExW);
  }
  if (lpAsyncItem->cStrHostNameW.IsEmpty() != FALSE)
  {
    delete lpAsyncItem;
    lpAsyncItem = NULL;
  }
  return lpAsyncItem;
}

VOID CHostResolver::FreeAsyncItem(_In_ CAsyncItem *lpAsyncItem)
{
  CFastLock cLock(&(sFreeAsyncItems.nMutex));

  if (sFreeAsyncItems.nListCount < MAX_ITEMS_IN_FREE_LIST)
  {
    lpAsyncItem->ResetAsync();

    lpAsyncItem->lpNextInFreeList = sFreeAsyncItems.lpFirst;
    sFreeAsyncItems.lpFirst = lpAsyncItem;
    (sFreeAsyncItems.nListCount)++;
  }
  else
  {
    delete lpAsyncItem;
  }
  return;
}

VOID WINAPI CHostResolver::AsyncQueryCompleteCallback(_In_ DWORD dwError, _In_ DWORD dwBytes, _In_ LPOVERLAPPED lpOvr)
{
  MX::Internals::CHostResolver::CAsyncItem *lpAsyncItem;

  UNREFERENCED_PARAMETER(dwBytes);

  lpAsyncItem = CONTAINING_RECORD(lpOvr, Internals::CHostResolver::CAsyncItem, sOvr);
  lpAsyncItem->lpResolver->CompleteAsync(lpAsyncItem, dwError);
  return;
}

VOID CHostResolver::CompleteAsync(_In_ CAsyncItem *lpAsyncItem, _In_ DWORD dwError)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);

  if (cAutoRundownProt.IsAcquired() == FALSE)
  {
    //if rundown is active, we are just canceling
    _InterlockedOr(&(lpAsyncItem->nFlags), _FLAG_Completed);
    return;
  }

  {
    CFastLock cQueueLock(&(sAsyncTasks.nMutex));

    //if it was canceled BEFORE callback is called, we don't call the callback
    if ((__InterlockedRead(&(lpAsyncItem->nFlags)) & _FLAG_Canceled) != 0)
    {
      _InterlockedOr(&(lpAsyncItem->nFlags), _FLAG_Completed);
      return;
    }
  }

  //process normally
  if (dwError == NO_ERROR)
  {
    lpAsyncItem->hr = ProcessResultsExW(lpAsyncItem->lpSockAddr, lpAsyncItem->lpAddrInfoExW,
                                        lpAsyncItem->nDesiredFamily);
  }
  else if (dwError == ERROR_TIMEOUT || dwError == WSA_OPERATION_ABORTED || dwError == WSA_WAIT_TIMEOUT ||
           dwError == WSAETIMEDOUT)
  {
    lpAsyncItem->hr = MX_E_Timeout;
  }
  else
  {
    lpAsyncItem->hr = MX_HRESULT_FROM_WIN32(dwError);
  }

  lpAsyncItem->InvokeCallback();

  {
    CFastLock cQueueLock(&(sAsyncTasks.nMutex));

    //if it was canceled WHILE/AFTER the callback was called
    if ((__InterlockedRead(&(lpAsyncItem->nFlags)) & _FLAG_Canceled) != 0)
    {
      //the item was removed from the list and let cancel to free the item
      _InterlockedOr(&(lpAsyncItem->nFlags), _FLAG_Completed);
    }
    else
    {
      lpAsyncItem->cTreeNode.Remove();
      FreeAsyncItem(lpAsyncItem);
    }
  }
  return;
}

HRESULT CHostResolver::ProcessResultsA(_In_ PSOCKADDR_INET lpSockAddr, _In_ PADDRINFOA lpAddrInfoA, _In_ int nFamily)
{
  PADDRINFOA lpCurrAddrInfoA;

  for (lpCurrAddrInfoA = lpAddrInfoA; lpCurrAddrInfoA != NULL; lpCurrAddrInfoA = lpCurrAddrInfoA->ai_next)
  {
    if ((nFamily == AF_INET || nFamily == AF_UNSPEC) &&
        lpCurrAddrInfoA->ai_family == PF_INET &&
        lpCurrAddrInfoA->ai_addrlen >= sizeof(SOCKADDR_IN))
    {
      ::MxMemCopy(&(lpSockAddr->Ipv4), lpCurrAddrInfoA->ai_addr, sizeof(sockaddr_in));
      return S_OK;
    }
    if ((nFamily == AF_INET6 || nFamily == AF_UNSPEC) &&
        lpCurrAddrInfoA->ai_family == PF_INET6 &&
        lpCurrAddrInfoA->ai_addrlen >= sizeof(SOCKADDR_IN6))
    {
      ::MxMemCopy(&(lpSockAddr->Ipv6), lpCurrAddrInfoA->ai_addr, sizeof(SOCKADDR_IN6));
      return S_OK;
    }
  }
  return MX_E_NotFound;
}

HRESULT CHostResolver::ProcessResultsExW(_In_ PSOCKADDR_INET lpSockAddr, _In_ PADDRINFOEXW lpAddrInfoExW,
                                         _In_ int nFamily)
{
  PADDRINFOEXW lpCurrAddrInfoExW;

  for (lpCurrAddrInfoExW = lpAddrInfoExW; lpCurrAddrInfoExW != NULL; lpCurrAddrInfoExW = lpCurrAddrInfoExW->ai_next)
  {
    if ((nFamily == AF_INET || nFamily == AF_UNSPEC) &&
        lpCurrAddrInfoExW->ai_family == PF_INET &&
        lpCurrAddrInfoExW->ai_addrlen >= sizeof(SOCKADDR_IN))
    {
      ::MxMemCopy(&(lpSockAddr->Ipv4), lpCurrAddrInfoExW->ai_addr, sizeof(SOCKADDR_IN));
      return S_OK;
    }
    if ((nFamily == AF_INET6 || nFamily == AF_UNSPEC) &&
        lpCurrAddrInfoExW->ai_family == PF_INET6 &&
        lpCurrAddrInfoExW->ai_addrlen >= sizeof(SOCKADDR_IN6))
    {
      ::MxMemCopy(&(lpSockAddr->Ipv6), lpCurrAddrInfoExW->ai_addr, sizeof(SOCKADDR_IN6));
      return S_OK;
    }
  }
  return MX_E_NotFound;
}

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

static VOID OnSyncResolution(_In_ LONG nResolverId, _In_ SOCKADDR_INET &sAddr, _In_ HRESULT hrErrorCode,
                             _In_ LPVOID lpUserData)
{
  LPSYNC_RESOLVE lpSyncData = (LPSYNC_RESOLVE)lpUserData;

  ::MxMemCopy(lpSyncData->lpAddr, &sAddr, sizeof(sAddr));
  _InterlockedExchange(&(lpSyncData->hRes), hrErrorCode);
  lpSyncData->cEvent.Set();
  return;
}

#pragma warning(suppress: 6101)
static SIZE_T Helper_IPv6_Fill(_Out_ LPWORD lpnAddr, _In_z_ LPCSTR szStrA, _In_ SIZE_T nLen)
{
  SIZE_T i, nSlot;
  DWORD dw, dwValue, dwTempValue;

  if (nLen == 0)
    return 0;
  //catch double uses of ::
  if (MX::StrNFindA(szStrA, "::", nLen) != NULL)
    return (SIZE_T)-1;
  nSlot = 0;
  dwValue = 0;
  for (i = 0; i < nLen; i++)
  {
    if (szStrA[i] == ':')
    {
      //trailing : is not allowed
      if (i == nLen - 1 || nSlot == 8)
        return (SIZE_T)-1;
      lpnAddr[nSlot++] = (WORD)dwValue;
      dwValue = 0;
    }
    else
    {
      if (szStrA[i] >= '0' && szStrA[i] <= '9')
        dw = (DWORD)(szStrA[i] - '0');
      else if (szStrA[i] >= 'a' && szStrA[i] <= 'f')
        dw = (DWORD)(szStrA[i] - 'a' + 10);
      else if (szStrA[i] >= 'A' && szStrA[i] <= 'F')
        dw = (DWORD)(szStrA[i] - 'A' + 10);
      else
        return (SIZE_T)-1;
      dwTempValue = dwValue << 4;
      if (dwTempValue < dwValue || dwValue > 0xFFFF - dw)
        return (SIZE_T)-1;
      dwValue = dwTempValue + dw;
    }
  }
  if (nSlot == 8)
    return (SIZE_T)-1;
#pragma warning(suppress: 6386)
  lpnAddr[nSlot++] = (WORD)dwValue;
  return nSlot;
}

#pragma warning(suppress: 6101)
static SIZE_T Helper_IPv6_Fill(_Out_ LPWORD lpnAddr, _In_z_ LPCWSTR szStrW, _In_ SIZE_T nLen)
{
  SIZE_T i, nSlot;
  DWORD dw, dwValue, dwTempValue;

  if (nLen == 0)
    return 0;
  //catch double uses of ::
  if (MX::StrFindW(szStrW, L"::") != NULL)
    return (SIZE_T)-1;
  nSlot = 0;
  dwValue = 0;
  for (i = 0; i < nLen; i++)
  {
    if (szStrW[i] == L':')
    {
      //trailing : is not allowed
      if (i == nLen - 1 || nSlot == 8)
        return (SIZE_T)-1;
      lpnAddr[nSlot++] = (WORD)dwValue;
      dwValue = 0;
    }
    else
    {
      if (szStrW[i] >= L'0' && szStrW[i] <= L'9')
        dw = (DWORD)(szStrW[i] - L'0');
      else if (szStrW[i] >= L'a' && szStrW[i] <= L'f')
        dw = (DWORD)(szStrW[i] - L'a' + 10);
      else if (szStrW[i] >= L'A' && szStrW[i] <= L'F')
        dw = (DWORD)(szStrW[i] - L'A' + 10);
      else
        return (SIZE_T)-1;
      dwTempValue = dwValue << 4;
      if (dwTempValue < dwValue || dwValue > 0xFFFF - dw)
        return (SIZE_T)-1;
      dwValue = dwTempValue + dw;
    }
  }
  if (nSlot == 8)
    return (SIZE_T)-1;
#pragma warning(suppress: 6386)
  lpnAddr[nSlot++] = (WORD)dwValue;
  return nSlot;
}
