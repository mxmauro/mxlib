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
#include "..\..\Include\Comm\Sockets.h"
#include "..\..\Include\Finalizer.h"
#include "..\..\Include\Http\punycode.h"
#include <MSTcpIP.h>
#include <VersionHelpers.h>

#pragma comment(lib, "ws2_32.lib")

//-----------------------------------------------------------

#define SOCKETS_FINALIZER_PRIORITY 11000

#define MAX_TOTAL_ACCEPTS_TO_POST                     0x1000

static __inline HRESULT MX_HRESULT_FROM_LASTSOCKETERROR()
{
  HRESULT hRes = MX_HRESULT_FROM_WIN32(::WSAGetLastError());
  return (hRes != MX_HRESULT_FROM_WIN32(WSAEWOULDBLOCK)) ? hRes : MX_E_IoPending;
}

#define TypeResolvingAddress  (CPacketBase::eType)((int)CPacketBase::TypeMAX + 1)
#define TypeConnect           (CPacketBase::eType)((int)CPacketBase::TypeMAX + 2)
#define TypeConnectEx         (CPacketBase::eType)((int)CPacketBase::TypeMAX + 3)
#define TypeDisconnectEx      (CPacketBase::eType)((int)CPacketBase::TypeMAX + 4)
#define TypeAcceptEx          (CPacketBase::eType)((int)CPacketBase::TypeMAX + 5)

#define SO_UPDATE_ACCEPT_CONTEXT    0x700B
#define SO_UPDATE_CONNECT_CONTEXT   0x7010

#define HANDLE_WITH_DISABLED_IOCP(h) (HANDLE)((SIZE_T)(h) | 1)

#ifndef TF_REUSE_SOCKET
  #define TF_REUSE_SOCKET     0x02
#endif //!TF_REUSE_SOCKET

#define TCP_KEEPALIVE_MS             45000

#ifndef FILE_SKIP_COMPLETION_PORT_ON_SUCCESS
  #define FILE_SKIP_COMPLETION_PORT_ON_SUCCESS    0x1
#endif //!FILE_SKIP_COMPLETION_PORT_ON_SUCCESS
#ifndef FILE_SKIP_SET_EVENT_ON_HANDLE
  #define FILE_SKIP_SET_EVENT_ON_HANDLE           0x2
#endif //!FILE_SKIP_SET_EVENT_ON_HANDLE

#define MGRFLAGS_TcpHasIfsHandles                     0x0001
#define MGRFLAGS_CanCompleteSync                      0x0002

#define IS_COMPLETE_SYNC_AVAILABLE()                                                                                  \
          (((reinterpret_cast<CSockets*>(lpIpc)->nFlags) & (MGRFLAGS_TcpHasIfsHandles | MGRFLAGS_CanCompleteSync)) == \
           (MGRFLAGS_TcpHasIfsHandles | MGRFLAGS_CanCompleteSync))

//-----------------------------------------------------------

typedef BOOL (WINAPI *lpfnSetFileCompletionNotificationModes)(_In_ HANDLE FileHandle, _In_ UCHAR Flags);

typedef BOOL (WINAPI *lpfnAcceptEx)(_In_ SOCKET sListenSocket, _In_ SOCKET sAcceptSocket, _In_ PVOID lpOutputBuffer,
                                    _In_ DWORD dwReceiveDataLength, _In_ DWORD dwLocalAddressLength,
                                    _In_ DWORD dwRemoteAddressLength, _Out_ LPDWORD lpdwBytesReceived,
                                    _Inout_ LPOVERLAPPED lpOverlapped);
typedef BOOL (WINAPI *lpfnConnectEx)(_In_ SOCKET s, _In_ const struct sockaddr FAR *name, _In_ int namelen,
                                     _In_opt_ PVOID lpSendBuffer, _In_ DWORD dwSendDataLength,
                                     _Out_ LPDWORD lpdwBytesSent, _Inout_ LPOVERLAPPED lpOverlapped);
typedef VOID (WINAPI *lpfnGetAcceptExSockaddrs)(_In_ PVOID lpOutputBuffer, _In_ DWORD dwReceiveDataLength,
                                                _In_ DWORD dwLocalAddressLength, _In_ DWORD dwRemoteAddressLength,
                                                _Deref_out_ struct sockaddr **LocalSockaddr,
                                                _Out_ LPINT LocalSockaddrLength,
                                                _Deref_out_ struct sockaddr **RemoteSockaddr,
                                                _Out_ LPINT RemoteSockaddrLength);
typedef BOOL (WINAPI *lpfnDisconnectEx)(_In_ SOCKET hSocket, _In_ LPOVERLAPPED lpOverlapped, _In_ DWORD dwFlags,
                                        _In_ DWORD reserved);

typedef BOOL (WINAPI *lpfnCancelIoEx)(_In_ HANDLE hFile, _In_opt_ LPOVERLAPPED lpOverlapped);

typedef struct {
  WORD wPort;
  SOCKADDR_INET sAddr;
} RESOLVEADDRESS_PACKET_DATA;

//-----------------------------------------------------------

static LONG volatile nSocketInitCount = 0;
static LONG volatile nOSVersion = -1;
static lpfnSetFileCompletionNotificationModes volatile fnSetFileCompletionNotificationModes = NULL;
static lpfnCancelIoEx volatile fnCancelIoEx = NULL;

//-----------------------------------------------------------

static HRESULT Winsock_Init();
static VOID Winsock_Shutdown();
static int FamilyToWinSockFamily(_In_ MX::CSockets::eFamily nFamily);
static int SockAddrSizeFromWinSockFamily(_In_ int nFamily);
static lpfnAcceptEx GetAcceptEx(_In_ SOCKET sck);
static lpfnGetAcceptExSockaddrs GetAcceptExSockaddrs(_In_ SOCKET sck);
static lpfnConnectEx GetConnectEx(_In_ SOCKET sck);
static lpfnDisconnectEx GetDisconnectEx(_In_ SOCKET sck);
static int SocketInsertFunc(_In_ LPVOID lpContext, _In_ SOCKET *lpElem1, _In_ SOCKET *lpElem2);
static int SocketSearchFunc(_In_ LPVOID lpContext, _In_ SOCKET sck, _In_ SOCKET *lpElem);

static inline BOOL __InterlockedIncrementIfLessThan(_In_ LONG volatile *lpnValue, _In_ LONG nMaximumValue)
{
  LONG initVal, newVal;

  newVal = __InterlockedRead(lpnValue);
  do
  {
    initVal = newVal;
    if (initVal >= nMaximumValue)
      return FALSE;
    newVal = _InterlockedCompareExchange(lpnValue, initVal + 1, initVal);
  }
  while (newVal != initVal);
  return TRUE;
}

//-----------------------------------------------------------

namespace MX {

CSockets::CSockets(_In_ CIoCompletionPortThreadPool &cDispatcherPool) : CIpc(cDispatcherPool)
{
  SIZE_T i;

  dwBackLogSize = 0;
  dwMaxAcceptsToPost = 4;
  dwAddressResolverTimeoutMs = 3000;
  for (i = 0; i < MX_ARRAYLEN(sDisconnectingSockets); i++)
    _InterlockedExchange(&(sDisconnectingSockets[i].nMutex), 0);
  for (i = 0; i < MX_ARRAYLEN(sReusableSockets); i++)
    _InterlockedExchange(&(sReusableSockets[i].nMutex), 0);

  nFlags = 0;

  //detect OS version
  if (__InterlockedRead(&nOSVersion) < 0)
  {
    LONG _nOSVersion = 0;

    if (::IsWindows8OrGreater() != FALSE)
      _nOSVersion = 0x0602;
    else if (::IsWindowsVistaOrGreater() != FALSE)
      _nOSVersion = 0x0600;
#pragma warning(default : 4996)
    _InterlockedExchange(&nOSVersion, _nOSVersion);
  }

  if (__InterlockedRead(&nOSVersion) >= 0x0600)
  {
    if (fnSetFileCompletionNotificationModes == NULL)
    {
      LPVOID _fnSetFileCompletionNotificationModes;
      HINSTANCE hDll;

      _fnSetFileCompletionNotificationModes = NULL;
      hDll = ::GetModuleHandleW(L"kernelbase.dll");
      if (hDll != NULL)
      {
        _fnSetFileCompletionNotificationModes = ::GetProcAddress(hDll, "SetFileCompletionNotificationModes");
      }
      if (_fnSetFileCompletionNotificationModes == NULL)
      {
        hDll = ::GetModuleHandleW(L"kernel32.dll");
        if (hDll != NULL)
        {
          _fnSetFileCompletionNotificationModes = ::GetProcAddress(hDll, "SetFileCompletionNotificationModes");
        }
      }
      _InterlockedExchangePointer((LPVOID volatile*)&fnSetFileCompletionNotificationModes,
                                  _fnSetFileCompletionNotificationModes);
    }
    if (fnCancelIoEx == NULL)
    {
      LPVOID _fnCancelIoEx;
      HINSTANCE hDll;

      _fnCancelIoEx = NULL;
      hDll = ::GetModuleHandleW(L"kernelbase.dll");
      if (hDll != NULL)
      {
        _fnCancelIoEx = ::GetProcAddress(hDll, "CancelIoEx");
      }
      if (_fnCancelIoEx == NULL)
      {
        hDll = ::GetModuleHandleW(L"kernel32.dll");
        if (hDll != NULL)
        {
          _fnCancelIoEx = ::GetProcAddress(hDll, "CancelIoEx");
        }
      }
      _InterlockedExchangePointer((LPVOID volatile*)&fnCancelIoEx, _fnCancelIoEx);
    }
  }

  if (fnSetFileCompletionNotificationModes != NULL)
  {
    nFlags |= MGRFLAGS_CanCompleteSync;
  }
  return;
}

CSockets::~CSockets()
{
  Finalize();
  return;
}

VOID CSockets::SetOption_BackLogSize(_In_ DWORD dwSize)
{
  CFastLock cInitShutdownLock(&nInitShutdownMutex);

  if (cShuttingDownEv.Get() == NULL)
  {
    dwBackLogSize = dwSize;
    if (dwBackLogSize > 0x7FFFFFFFUL)
      dwBackLogSize = 0x7FFFFFFFUL;
  }
  return;
}

VOID CSockets::SetOption_MaxAcceptsToPost(_In_ DWORD dwCount)
{
  CFastLock cInitShutdownLock(&nInitShutdownMutex);

  if (cShuttingDownEv.Get() == NULL)
  {
    dwMaxAcceptsToPost = dwCount;
    if (dwMaxAcceptsToPost < 1)
      dwMaxAcceptsToPost = 1;
    else if (dwMaxAcceptsToPost > MAX_TOTAL_ACCEPTS_TO_POST)
      dwMaxAcceptsToPost = MAX_TOTAL_ACCEPTS_TO_POST;
  }
  return;
}

VOID CSockets::SetOption_AddressResolverTimeout(_In_ DWORD dwTimeoutMs)
{
  CFastLock cInitShutdownLock(&nInitShutdownMutex);

  if (cShuttingDownEv.Get() == NULL)
  {
    dwAddressResolverTimeoutMs = dwTimeoutMs;
    if (dwAddressResolverTimeoutMs < 100)
      dwAddressResolverTimeoutMs = 100;
    else if (dwAddressResolverTimeoutMs > 180000)
      dwAddressResolverTimeoutMs = 180000;
  }
  return;
}

HRESULT CSockets::CreateListener(_In_ eFamily nFamily, _In_ int nPort, _In_ OnCreateCallback cCreateCallback,
                                 _In_opt_z_ LPCSTR szBindAddressA, _In_opt_ CUserData *lpUserData,
                                 _Out_opt_ HANDLE *h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnection> cConn;
  HRESULT hRes;

  if (h != NULL)
    *h = NULL;
  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  if ((nFamily != FamilyIPv4 && nFamily != FamilyIPv6) || nPort < 1 || nPort > 65535 || (!cCreateCallback))
    return E_INVALIDARG;
  //create connection
  cConn.Attach(MX_DEBUG_NEW CConnection(this, CIpc::ConnectionClassListener, nFamily));
  if (!cConn)
    return E_OUTOFMEMORY;
  if (h != NULL)
    *h = reinterpret_cast<HANDLE>(cConn.Get());
  cConn->cCreateCallback = cCreateCallback;
  cConn->cUserData = lpUserData;
  {
    CAutoSlimRWLExclusive cConnListLock(&(sConnections.nRwMutex));

    sConnections.cTree.Insert(cConn.Get());
  }
  hRes = FireOnCreate(cConn.Get());
  if (SUCCEEDED(hRes))
    hRes = cConn->CreateSocket();
  if (SUCCEEDED(hRes))
    hRes = cConn->ResolveAddress(dwAddressResolverTimeoutMs, szBindAddressA, nPort);
  //done
  if (FAILED(hRes))
  {
    cConn->Close(hRes);
    if (h != NULL)
      *h = NULL;
  }
  cConn.Detach();
  return hRes;
}

HRESULT CSockets::CreateListener(_In_ eFamily nFamily, _In_ int nPort, _In_ OnCreateCallback cCreateCallback,
                                 _In_opt_z_ LPCWSTR szBindAddressW, _In_opt_ CUserData *lpUserData,
                                 _Out_opt_ HANDLE *h)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (h != NULL)
    *h = NULL;
  if (szBindAddressW != NULL && szBindAddressW[0] != 0)
  {
    hRes = Punycode_Encode(cStrTempA, szBindAddressW);
    if (FAILED(hRes))
      return hRes;
  }
  return CreateListener(nFamily, nPort, cCreateCallback, (LPSTR)cStrTempA, lpUserData, h);
}

HRESULT CSockets::ConnectToServer(_In_ eFamily nFamily, _In_z_ LPCSTR szAddressA, _In_ int nPort,
                                  _In_ OnCreateCallback cCreateCallback, _In_opt_ CUserData *lpUserData,
                                  _Out_opt_ HANDLE *h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnection> cConn;
  HRESULT hRes;

  if (h != NULL)
    *h = NULL;
  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  if (szAddressA == NULL)
    return E_POINTER;
  if ((nFamily != FamilyIPv4 && nFamily != FamilyIPv6) || *szAddressA == 0 || nPort < 1 || nPort > 65535)
    return E_INVALIDARG;
  if (!cCreateCallback)
    return E_POINTER;
  //create connection
  cConn.Attach(MX_DEBUG_NEW CConnection(this, CIpc::ConnectionClassClient, nFamily));
  if (!cConn)
    return E_OUTOFMEMORY;
  if (h != NULL)
    *h = reinterpret_cast<HANDLE>(cConn.Get());
  cConn->cCreateCallback = cCreateCallback;
  cConn->cUserData = lpUserData;
  {
    CAutoSlimRWLExclusive cConnListLock(&(sConnections.nRwMutex));

    sConnections.cTree.Insert(cConn.Get());
  }
  hRes = FireOnCreate(cConn.Get());
  if (SUCCEEDED(hRes))
    hRes = cConn->CreateSocket();
  if (SUCCEEDED(hRes))
    hRes = cConn->ResolveAddress(dwAddressResolverTimeoutMs, szAddressA, nPort);
  //done
  if (FAILED(hRes))
  {
    cConn->Close(hRes);
    if (h != NULL)
      *h = NULL;
  }
  cConn.Detach();
  return hRes;
}

HRESULT CSockets::ConnectToServer(_In_ eFamily nFamily, _In_z_ LPCWSTR szAddressW, _In_ int nPort,
                                  _In_ OnCreateCallback cCreateCallback, _In_opt_ CUserData *lpUserData,
                                  _Out_opt_ HANDLE *h)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (h != NULL)
    *h = NULL;
  if (szAddressW == NULL)
    return E_POINTER;
  hRes = Punycode_Encode(cStrTempA, szAddressW);
  if (SUCCEEDED(hRes))
    hRes = ConnectToServer(nFamily, (LPSTR)cStrTempA, nPort, cCreateCallback, lpUserData, h);
  return hRes;
}

HRESULT CSockets::GetLocalAddress(_In_ HANDLE h, _Out_ PSOCKADDR_INET lpAddr)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnection> cConn;
  int namelen;

  if (lpAddr != NULL)
    MemSet(lpAddr, 0, sizeof(SOCKADDR_INET));
  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  if (h == NULL || lpAddr == NULL)
    return E_POINTER;
  cConn.Attach((CConnection*)CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;
  //copy local info
  namelen = (int)sizeof(SOCKADDR_INET);
  if (::getsockname(cConn->sck, (sockaddr*)lpAddr, &namelen) != 0)
  {
    MemSet(lpAddr, 0, sizeof(SOCKADDR_INET));
    return MX_HRESULT_FROM_LASTSOCKETERROR();
  }
  //done
  return S_OK;
}

HRESULT CSockets::GetPeerAddress(_In_ HANDLE h, _Out_ PSOCKADDR_INET lpAddr)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnection> cConn;

  if (lpAddr != NULL)
    MemSet(lpAddr, 0, sizeof(SOCKADDR_INET));
  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  if (h == NULL || lpAddr == NULL)
    return E_POINTER;
  cConn.Attach((CConnection*)CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;
  //copy peer info
  MemCopy(lpAddr, &(cConn->sAddr), sizeof(cConn->sAddr));
  //done
  return S_OK;
}

HRESULT CSockets::OnInternalInitialize()
{
  HRESULT hRes;

  //initialize WinSock
  hRes = Winsock_Init();
  if (SUCCEEDED(hRes))
  {
    INT aProtocols[2] = { IPPROTO_TCP, 0 };
    TAutoFreePtr<WSAPROTOCOL_INFOW> cProtInfoW;
    DWORD dw, dwCount, dwProfBufLen;

    dwCount = 0;
    dwProfBufLen = 32 * (DWORD)sizeof(WSAPROTOCOL_INFOW);
    cProtInfoW.Attach((LPWSAPROTOCOL_INFOW)MX_MALLOC((SIZE_T)dwProfBufLen));
    if (cProtInfoW)
    {
      int err;

      err = ::WSAEnumProtocolsW(aProtocols, cProtInfoW.Get(), &dwProfBufLen);
      if (err >= 0)
      {
        hRes = S_OK;
        dwCount = (int)err;
      }
      else
      {
        hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
        if (hRes == MX_HRESULT_FROM_WIN32(WSAENOBUFS) || hRes == MX_E_BufferOverflow)
        {
          cProtInfoW.Attach((LPWSAPROTOCOL_INFOW)MX_MALLOC((SIZE_T)dwProfBufLen));
          if (cProtInfoW)
          {
            err = ::WSAEnumProtocolsW(aProtocols, cProtInfoW.Get(), &dwProfBufLen);
            if (err >= 0)
            {
              hRes = S_OK;
              dwCount = (int)err;
            }
            else
            {
              hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
            }
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
    if (SUCCEEDED(hRes))
    {
      for (dw = 0; dw < dwCount; dw++)
      {
        if (((cProtInfoW.Get())[dw].dwServiceFlags1 & XP1_IFS_HANDLES) == 0)
          break;
      }
      if (dw >= dwCount)
      {
        nFlags |= MGRFLAGS_TcpHasIfsHandles;
      }
    }
    else if (hRes == E_OUTOFMEMORY)
    {
      Winsock_Shutdown();
    }
    else
    {
      hRes = S_OK; //ignore other errors
    }
  }
  //done
  return hRes;
}

VOID CSockets::OnInternalFinalize()
{
  SIZE_T i;

  for (i = 0; i < MX_ARRAYLEN(sDisconnectingSockets); i++)
  {
    CFastLock cLock(&(sDisconnectingSockets[i].nMutex));
    SIZE_T nCount;
    SOCKET *lpSck;

    nCount = sDisconnectingSockets[i].aList.GetCount();
    lpSck = sDisconnectingSockets[i].aList.GetBuffer();
    while (nCount > 0)
    {
      fnCancelIoEx((HANDLE)(*lpSck), NULL);

      lpSck++;
      nCount--;
    }
  }

  for (i = 0; i < MX_ARRAYLEN(sReusableSockets); i++)
  {
    CFastLock cLock(&(sReusableSockets[i].nMutex));
    SIZE_T nCount;
    SOCKET *lpSck;

    nCount = sReusableSockets[i].aList.GetCount();
    lpSck = sReusableSockets[i].aList.GetBuffer();
    while (nCount > 0)
    {
      ::closesocket(*lpSck);
      lpSck++;
      nCount--;
    }
  }

  nFlags &= ~MGRFLAGS_TcpHasIfsHandles;
  return;
}

HRESULT CSockets::CreateServerConnection(_In_ CConnection *lpListenConn)
{
  TAutoDeletePtr<CConnection> cIncomingConn;
  HRESULT hRes;

  //create connection
  cIncomingConn.Attach(MX_DEBUG_NEW CConnection(this, CIpc::ConnectionClassServer, lpListenConn->nFamily));
  if (!cIncomingConn)
    return E_OUTOFMEMORY;
  cIncomingConn->cCreateCallback = lpListenConn->cCreateCallback;
  MemCopy(&(cIncomingConn->sAddr), &(lpListenConn->sAddr), sizeof(lpListenConn->sAddr));
  {
    CAutoSlimRWLExclusive cConnListLock(&(sConnections.nRwMutex));

    sConnections.cTree.Insert(cIncomingConn.Get());
  }
  hRes = FireOnCreate(cIncomingConn.Get());
  if (SUCCEEDED(hRes))
    hRes = cIncomingConn->CreateSocket();
  if (SUCCEEDED(hRes))
    hRes = lpListenConn->SetupAcceptEx(cIncomingConn.Get());
  //done
  if (FAILED(hRes))
    cIncomingConn->Close(hRes);
  cIncomingConn.Detach();
  return hRes;
}

BOOL CSockets::OnPreprocessPacket(_In_ DWORD dwBytes, _In_ CPacketBase *lpPacket, _In_ HRESULT hRes)
{
  if (lpPacket->GetType() == TypeDisconnectEx)
  {
    SIZE_T nArrIdx;
    eFamily nFamily;
    SOCKET sck;

    MemCopy(&nFamily, lpPacket->GetBuffer(), sizeof(eFamily));
    sck = (SOCKET)(lpPacket->GetUserData());
    nArrIdx = (nFamily == FamilyIPv4) ? 0 : 1;

    FreePacket(lpPacket);

    if (SUCCEEDED(hRes))
    {
      {
        CFastLock cLock(&(sDisconnectingSockets[nArrIdx].nMutex));
        SIZE_T nIndex;

        nIndex = sDisconnectingSockets[nArrIdx].aList.BinarySearch(sck, &SocketSearchFunc);
        if (nIndex != (SIZE_T)-1)
          sDisconnectingSockets[nArrIdx].aList.RemoveElementAt(nIndex);
      }

      if (IsShuttingDown() == FALSE)
      {
        CFastLock cLock(&(sReusableSockets[nArrIdx].nMutex));

        if (sReusableSockets[nArrIdx].aList.AddElement(sck) == FALSE)
          hRes = E_OUTOFMEMORY;
      }
      else
      {
        hRes = MX_E_Cancelled;
      }
    }
    if (FAILED(hRes))
    {
      ::closesocket(sck);
    }

    //done
    return TRUE;
  }
  return FALSE;
}

HRESULT CSockets::OnCustomPacket(_In_ DWORD dwBytes, _In_ CPacketBase *lpPacket, _In_ HRESULT hRes)
{
  CConnection *lpConn;

  lpConn = (CConnection*)(lpPacket->GetConn());
  switch (lpPacket->GetType())
  {
    case TypeResolvingAddress:
      {
      RESOLVEADDRESS_PACKET_DATA *lpData = (RESOLVEADDRESS_PACKET_DATA*)(lpPacket->GetBuffer());

      //copy address
      MemCopy(&(lpConn->sAddr), &(lpData->sAddr), sizeof(lpData->sAddr));
      //connect/listen
      switch (lpConn->nClass)
      {
        case CIpc::ConnectionClassListener:
          hRes = lpConn->SetupListener(((dwBackLogSize != 0) ? (int)dwBackLogSize : SOMAXCONN), dwMaxAcceptsToPost);
          if (FAILED(hRes))
            FireOnEngineError(hRes);
          break;

        case CIpc::ConnectionClassClient:
          hRes = lpConn->SetupClient();
          break;

        default:
          MX_ASSERT(FALSE);
      }
      }
      break;

    case TypeAcceptEx:
      {
      CConnection *lpIncomingConn = (CConnection*)(lpPacket->GetUserData());

      _InterlockedDecrement(&(lpConn->lpListener->nAcceptsInProgress));
      ::SetEvent(lpConn->lpListener->hAcceptCompleted);

      if (SUCCEEDED(hRes) && IsShuttingDown() != FALSE)
        hRes = MX_E_Cancelled;
      if (SUCCEEDED(hRes))
      {
        if (::setsockopt(lpIncomingConn->sck, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&(lpConn->sck),
                         (int)sizeof(lpConn->sck)) == SOCKET_ERROR)
        {
          hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
        }
      }
      if (SUCCEEDED(hRes))
      {
        sockaddr *lpLocalAddr, *lpPeerAddr;
        INT nLocalAddrLen, nPeerAddrLen;
        lpfnGetAcceptExSockaddrs fnGetAcceptExSockaddrs;

        lpLocalAddr = lpPeerAddr = NULL;
        nLocalAddrLen = nPeerAddrLen = 0;
        //get extension function addresses and peer address
        fnGetAcceptExSockaddrs = GetAcceptExSockaddrs(lpConn->sck);
        if (fnGetAcceptExSockaddrs != NULL)
        {
          fnGetAcceptExSockaddrs(lpPacket->GetBuffer(), 0, lpPacket->GetBytesInUse(), lpPacket->GetBytesInUse(),
                                 &lpLocalAddr, &nLocalAddrLen, &lpPeerAddr, &nPeerAddrLen);
        }
        if (lpPeerAddr != NULL && nPeerAddrLen >= (INT)SockAddrSizeFromWinSockFamily(lpConn->sAddr.si_family))
        {
          MemCopy(&(lpIncomingConn->sAddr.Ipv4), (PSOCKADDR_IN)lpPeerAddr, sizeof(SOCKADDR_IN));
        }
        else
        {
          SOCKADDR_INET sPeerAddr;

          int len = SockAddrSizeFromWinSockFamily(lpIncomingConn->sAddr.si_family);
          if (::getpeername(lpIncomingConn->sck, (sockaddr*)&sPeerAddr, &len) != SOCKET_ERROR)
          {
            if (sPeerAddr.si_family == AF_INET && len >= sizeof(SOCKADDR_IN))
            {
              MemCopy(&(lpIncomingConn->sAddr.Ipv4), (PSOCKADDR_IN)&sPeerAddr, sizeof(SOCKADDR_IN));
            }
            else if (sPeerAddr.si_family == AF_INET6 && len >= sizeof(SOCKADDR_IN6))
            {
              MemCopy(&(lpIncomingConn->sAddr.Ipv6), (PSOCKADDR_IN6)&sPeerAddr, sizeof(SOCKADDR_IN6));
            }
            else
            {
              MemCopy(&(lpIncomingConn->sAddr), (sockaddr*)&sPeerAddr, sizeof((lpIncomingConn->sAddr)));
              hRes = E_FAIL;
            }
          }
          else
          {
            hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
          }
        }
      }
      if (SUCCEEDED(hRes))
        hRes = lpIncomingConn->HandleConnected();
      if (FAILED(hRes))
        lpIncomingConn->Close(hRes);
      //free packet
      lpIncomingConn->Release();
      lpConn->cRwList.Remove(lpPacket);
      FreePacket(lpPacket);
      //done
      hRes = S_OK;
      }
      break;


    case TypeConnect:
      //get result from packet
      hRes = *((HRESULT UNALIGNED*)(lpPacket->GetBuffer()));
      //fall into connectex

    case TypeConnectEx:
      if (SUCCEEDED(hRes) && lpPacket->GetType() == TypeConnectEx)
      {
        if (::setsockopt(lpConn->sck, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0) == SOCKET_ERROR)
          hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
      }
      if (SUCCEEDED(hRes))
        hRes = lpConn->HandleConnected();
      if (FAILED(hRes))
        FireOnConnect(lpConn, hRes);
      //free packet
      lpConn->cRwList.Remove(lpPacket);
      FreePacket(lpPacket);
      break;
  }
  //done
  return hRes;
}

//-----------------------------------------------------------

CSockets::CConnection::CConnection(_In_ CIpc *lpIpc, _In_ CIpc::eConnectionClass nClass,
                                   _In_ eFamily _nFamily) : CConnectionBase(lpIpc, nClass)
{
  SlimRWL_Initialize(&nRwHandleInUse);
  MemSet(&sAddr, 0, sizeof(sAddr));
  sck = NULL;
  MemSet(&sHostResolver, 0, sizeof(sHostResolver));
  lpConnectWaiter = NULL;
  lpListener = NULL;
  _InterlockedExchange(&nReadThrottle, 1024);
  nFamily = _nFamily;
  return;
}

CSockets::CConnection::~CConnection()
{
  MX_ASSERT(sck == NULL);
  MX_ASSERT(lpConnectWaiter == NULL || lpConnectWaiter->lpWorkerThread == NULL);
  MX_FREE(lpConnectWaiter);
  if (lpListener != NULL)
  {
    MX_ASSERT(lpListener->lpWorkerThread == NULL);
    if (lpListener->hAcceptSelect != NULL)
      ::CloseHandle(lpListener->hAcceptSelect);
    if (lpListener->hAcceptCompleted != NULL)
      ::CloseHandle(lpListener->hAcceptCompleted);
    MX_FREE(lpListener);
  }
  MX_ASSERT(__InterlockedRead(&(sHostResolver.nResolverId)) == 0);
  return;
}

HRESULT CSockets::CConnection::CreateSocket()
{
  u_long nYes;
  struct tcp_keepalive sKeepAliveData;
  DWORD dw;
  HRESULT hRes;

  //create socket
  if (nClass == ConnectionClassServer)
  {
    SIZE_T nIndex = (nFamily == eFamily::FamilyIPv4) ? 0 : 1;
    struct tagReusableSockets *lpReusableSockets = &(reinterpret_cast<CSockets*>(lpIpc)->sReusableSockets[nIndex]);

    {
      CFastLock cLock(&(lpReusableSockets->nMutex));
      SIZE_T nCount;

      nCount = lpReusableSockets->aList.GetCount();
      if (nCount > 0)
      {
        nCount--;
        sck = lpReusableSockets->aList.GetElementAt(nCount);
        lpReusableSockets->aList.RemoveElementAt(nCount);

        //done
        return S_OK;
      }
    }
  }
  if (sck == NULL)
  {
    sck = ::WSASocketW(FamilyToWinSockFamily(nFamily), SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (sck == NULL || sck == INVALID_SOCKET)
    {
      sck = NULL;
      return MX_HRESULT_FROM_LASTSOCKETERROR();
    }
  }
  ::SetHandleInformation((HANDLE)sck, HANDLE_FLAG_INHERIT, 0);
  //non-blocking
  nYes = 1;
  if (::WSAIoctl(sck, FIONBIO, &nYes, sizeof(nYes), NULL, 0, &dw, NULL, NULL) == SOCKET_ERROR)
    return MX_HRESULT_FROM_LASTSOCKETERROR();
  //tcp no delay
  nYes = 1;
  if (::setsockopt(sck, IPPROTO_TCP, TCP_NODELAY, (char*)&nYes, (int)sizeof(nYes)) == SOCKET_ERROR)
    return MX_HRESULT_FROM_LASTSOCKETERROR();

  //packet size on pre-vista
  if (__InterlockedRead(&nOSVersion) < 0x0600)
  {
    //Vista and later uses an auto tune algorithm better than a static one
    unsigned int nBufSize;

    nBufSize = 65536;
    if (::setsockopt(sck, SOL_SOCKET, SO_RCVBUF, (char*)&nBufSize, (int)sizeof(nBufSize)) == SOCKET_ERROR)
      return MX_HRESULT_FROM_LASTSOCKETERROR();
    nBufSize = 65536;
    if (::setsockopt(sck, SOL_SOCKET, SO_SNDBUF, (char*)&nBufSize, (int)sizeof(nBufSize)) == SOCKET_ERROR)
      return MX_HRESULT_FROM_LASTSOCKETERROR();
  }
  //IOCP options
  if (fnSetFileCompletionNotificationModes != NULL)
  {
    UCHAR flags = FILE_SKIP_SET_EVENT_ON_HANDLE;

    if (IS_COMPLETE_SYNC_AVAILABLE())
      flags |= FILE_SKIP_COMPLETION_PORT_ON_SUCCESS;
    if (fnSetFileCompletionNotificationModes((HANDLE)sck, flags) == FALSE)
      return MX_HRESULT_FROM_LASTERROR();
  }
  switch (nClass)
  {
    case CIpc::ConnectionClassClient:
    case CIpc::ConnectionClassServer:
      //keep alive
      sKeepAliveData.onoff = 1;
      sKeepAliveData.keepalivetime = sKeepAliveData.keepaliveinterval = TCP_KEEPALIVE_MS;
      dw = 0;
      if (::WSAIoctl(sck, SIO_KEEPALIVE_VALS, &sKeepAliveData, sizeof(sKeepAliveData), NULL, 0, &dw,
                     NULL, NULL) == SOCKET_ERROR)
      {
        return MX_HRESULT_FROM_LASTSOCKETERROR();
      }
      //attach to completion port
      hRes = GetDispatcherPool().Attach((HANDLE)sck, GetDispatcherPoolPacketCallback());
      if (FAILED(hRes))
        return hRes;
      break;

    case CIpc::ConnectionClassListener:
      //use loopback fast path on Win8+
      if (__InterlockedRead(&nOSVersion) >= 0x0602)
      {
        nYes = 1;
        if (::WSAIoctl(sck, SIO_LOOPBACK_FAST_PATH, &nYes, sizeof(nYes), NULL, 0, &dw, NULL, NULL) == SOCKET_ERROR)
          return MX_HRESULT_FROM_LASTSOCKETERROR();
      }
      //reuse address
      if (::setsockopt(sck, SOL_SOCKET, SO_REUSEADDR, (char*)&nYes, (int)sizeof(nYes)) == SOCKET_ERROR)
        return MX_HRESULT_FROM_LASTSOCKETERROR();
      //attach to completion port
      hRes = GetDispatcherPool().Attach((HANDLE)sck, GetDispatcherPoolPacketCallback());
      if (FAILED(hRes))
        return hRes;
      break;
  }
  //done
  return S_OK;
}

VOID CSockets::CConnection::ShutdownLink(_In_ BOOL bAbortive)
{
  {
    CAutoSlimRWLExclusive cHandleInUseLock(&nRwHandleInUse);

    if (sck != NULL)
    {
      lpfnDisconnectEx fnDisconnectEx = NULL;
      CIpc::CPacketBase *lpPacket;
      CSockets *lpSckMgr;
      SIZE_T nArrIdx;
      LINGER sLinger;

      //get extension function addresses
      if (nClass == ConnectionClassServer && lpIpc->IsShuttingDown() == FALSE && IsConnected() != FALSE &&
          fnCancelIoEx != NULL)
      {
        fnDisconnectEx = GetDisconnectEx(sck);
      }
      if (bAbortive != FALSE)
      {
        //abortive close on error
        sLinger.l_onoff = 1;
        sLinger.l_linger = 0;
        ::setsockopt(sck, SOL_SOCKET, SO_LINGER, (char*)&sLinger, (int)sizeof(sLinger));
      }
      else
      {
        if (fnDisconnectEx == NULL)
          ::shutdown(sck, SD_SEND);
        sLinger.l_onoff = 0;
        sLinger.l_linger = 0;
        ::setsockopt(sck, SOL_SOCKET, SO_LINGER, (char*)&sLinger, (int)sizeof(sLinger));
      }
      if (fnDisconnectEx == NULL)
      {
use_default_close_method:
        ::closesocket(sck);
        goto after_close;
      }

      //cancel pending IO
      fnCancelIoEx((HANDLE)sck, NULL);

      lpPacket = GetPacket(TypeDisconnectEx, sizeof(eFamily), TRUE);
      if (lpPacket == NULL)
        goto use_default_close_method;

      lpSckMgr = reinterpret_cast<CSockets*>(lpIpc);
      nArrIdx = (nFamily == FamilyIPv4) ? 0 : 1;

      MemCopy(lpPacket->GetBuffer(), &nFamily, sizeof(eFamily));
      lpPacket->SetUserData((LPVOID)sck);

      {
        CFastLock cLock(&(lpSckMgr->sDisconnectingSockets[nArrIdx].nMutex));

        if (lpSckMgr->sDisconnectingSockets[nArrIdx].aList.SortedInsert(sck, &SocketInsertFunc, NULL, TRUE) == FALSE)
        {
          FreePacket(lpPacket);
          goto use_default_close_method;
        }
      }

      if (fnDisconnectEx(sck, lpPacket->GetOverlapped(), TF_REUSE_SOCKET, 0) != FALSE)
      {
        if (IS_COMPLETE_SYNC_AVAILABLE())
        {
          reinterpret_cast<CSockets*>(lpIpc)->OnDispatcherPacket(&(GetDispatcherPool()), 0,
                                                                  lpPacket->GetOverlapped(), S_OK);
        }
      }
      else
      {
        HRESULT hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
        if (FAILED(hRes) && hRes != MX_E_IoPending)
        {
          CFastLock cLock(&(lpSckMgr->sDisconnectingSockets[nArrIdx].nMutex));
          SIZE_T nIndex;

          nIndex = lpSckMgr->sDisconnectingSockets[nArrIdx].aList.BinarySearch(sck, &SocketSearchFunc);
          if (nIndex != (SIZE_T)-1)
            lpSckMgr->sDisconnectingSockets[nArrIdx].aList.RemoveElementAt(nIndex);
          goto use_default_close_method;
        }
      }

after_close:
      sck = NULL;

      if (lpListener != NULL)
      {
        if (lpListener->lpWorkerThread != NULL)
        {
          lpListener->lpWorkerThread->Stop();
          delete lpListener->lpWorkerThread;
          lpListener->lpWorkerThread = NULL;
        }
      }
    }
  }
  //cancel connect worker thread
  if (lpConnectWaiter != NULL)
  {
    if (lpConnectWaiter->lpWorkerThread != NULL)
    {
      lpConnectWaiter->lpWorkerThread->Stop();
    }
  }
  //cancel host resolver
  HostResolver::Cancel(&(sHostResolver.nResolverId));
  //call base
  CConnectionBase::ShutdownLink(bAbortive);
  return;
}

HRESULT CSockets::CConnection::SetupListener(_In_ int nBackLogSize, _In_ DWORD dwMaxAcceptsToPost)
{
  if (sAddr.si_family == AF_INET6 &&
      sAddr.Ipv6.sin6_addr.u.Word[0] == 0 && sAddr.Ipv6.sin6_addr.u.Word[1] == 0 &&
      sAddr.Ipv6.sin6_addr.u.Word[2] == 0 && sAddr.Ipv6.sin6_addr.u.Word[3] == 0 &&
      sAddr.Ipv6.sin6_addr.u.Word[4] == 0 && sAddr.Ipv6.sin6_addr.u.Word[5] == 0 &&
      sAddr.Ipv6.sin6_addr.u.Word[6] == 0 && sAddr.Ipv6.sin6_addr.u.Word[7] == 0)
  {
    int nNo = 0;
    if (::setsockopt(sck, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&nNo, (int)sizeof(nNo)) == SOCKET_ERROR)
      return MX_HRESULT_FROM_LASTSOCKETERROR();
  }
  if (::bind(sck, (sockaddr*)&sAddr, SockAddrSizeFromWinSockFamily(sAddr.si_family)) == SOCKET_ERROR ||
      ::listen(sck, nBackLogSize) == SOCKET_ERROR)
  {
    return MX_HRESULT_FROM_LASTSOCKETERROR();
  }
  lpListener = (struct tagListener*)MX_MALLOC(sizeof(struct tagListener));
  if (lpListener == NULL)
    return E_OUTOFMEMORY;
  MemSet(lpListener, 0, sizeof(struct tagListener));
  lpListener->dwMaxAcceptsToPost = dwMaxAcceptsToPost;
  lpListener->fnAcceptEx = GetAcceptEx(sck);
  if (lpListener->fnAcceptEx == NULL || GetAcceptExSockaddrs(sck) == NULL)
    return MX_E_Unsupported;
  lpListener->hAcceptSelect = ::CreateEventW(NULL, TRUE, FALSE, NULL);
  if (lpListener->hAcceptSelect == NULL)
    return E_OUTOFMEMORY;
  lpListener->hAcceptCompleted = ::CreateEventW(NULL, TRUE, FALSE, NULL);
  if (lpListener->hAcceptCompleted == NULL)
    return E_OUTOFMEMORY;
  //associate accept event with socket
  if (::WSAEventSelect(sck, lpListener->hAcceptSelect, FD_ACCEPT) == SOCKET_ERROR)
    return MX_HRESULT_FROM_LASTSOCKETERROR();
  //create listener thread
  lpListener->lpWorkerThread = MX_DEBUG_NEW TClassWorkerThread<CConnection>();
  if (lpListener->lpWorkerThread == NULL)
    return E_OUTOFMEMORY;
  lpListener->lpWorkerThread->SetPriority(THREAD_PRIORITY_HIGHEST);
  if (lpListener->lpWorkerThread->Start(this, &CConnection::ListenerThreadProc) == FALSE)
  {
    delete lpListener->lpWorkerThread;
    lpListener->lpWorkerThread = NULL;
    return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CSockets::CConnection::SetupClient()
{
  CPacketBase *lpPacket;
  SOCKADDR_INET sBindAddr;
  lpfnConnectEx fnConnectEx;
  HRESULT hRes;

  fnConnectEx = GetConnectEx(sck);
  //----
  MemSet(&sBindAddr, 0, sizeof(sBindAddr));
  sBindAddr.si_family = sAddr.si_family;
  if (::bind(sck, (sockaddr*)&sBindAddr, SockAddrSizeFromWinSockFamily(sAddr.si_family)) == SOCKET_ERROR)
    return MX_HRESULT_FROM_LASTSOCKETERROR();
  lpPacket = GetPacket(((fnConnectEx != NULL) ? (TypeConnectEx) : (TypeConnect)), 0, FALSE);
  if (lpPacket == NULL)
    return E_OUTOFMEMORY;
  cRwList.QueueLast(lpPacket);
  hRes = S_OK;
  if (lpIpc->ShouldLog(2) != FALSE)
  {
    lpIpc->Log(L"CSockets::SetupClient) Clock=%lums / Ovr=0x%p / Type=%lu", cHiResTimer.GetElapsedTimeMs(),
               lpPacket->GetOverlapped(), lpPacket->GetType());
  }
  if (fnConnectEx != NULL)
  {
    DWORD dw;

    //do connect
    AddRef();
    if (fnConnectEx(sck, (sockaddr*)&sAddr, SockAddrSizeFromWinSockFamily(sAddr.si_family), NULL, 0, &dw,
                    lpPacket->GetOverlapped()) != FALSE)
    {
      if (IS_COMPLETE_SYNC_AVAILABLE())
      {
        reinterpret_cast<CSockets*>(lpIpc)->OnDispatcherPacket(&(GetDispatcherPool()), 0,
                                                               lpPacket->GetOverlapped(), S_OK);
      }
    }
    else
    {
      hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
      if (hRes == MX_E_IoPending)
        hRes = S_OK;
      else if (FAILED(hRes))
        Release();
    }
  }
  else
  {
    if (::connect(sck, (sockaddr*)&sAddr, SockAddrSizeFromWinSockFamily(sAddr.si_family)) != SOCKET_ERROR)
    {
      *((HRESULT MX_UNALIGNED*)(lpPacket->GetBuffer())) = hRes;
      AddRef();
      hRes = GetDispatcherPool().Post(GetDispatcherPoolPacketCallback(), 0, lpPacket->GetOverlapped());
      if (FAILED(hRes))
        Release();
    }
    else
    {
      hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
      if (hRes == MX_E_IoPending)
      {
        lpConnectWaiter = (struct tagConnectWaiter*)MX_MALLOC(sizeof(struct tagConnectWaiter));
        if (lpConnectWaiter != NULL)
        {
          MemSet(lpConnectWaiter, 0, sizeof(struct tagConnectWaiter));

          //start connect waiter
          lpConnectWaiter->lpWorkerThread = MX_DEBUG_NEW TClassWorkerThread<CConnection>();
          if (lpConnectWaiter->lpWorkerThread != NULL)
          {
            lpConnectWaiter->lpWorkerThread->SetAutoDelete(TRUE);
            AddRef();
            lpConnectWaiter->lpPacket = lpPacket;
            if (lpConnectWaiter->lpWorkerThread->Start(this, &CConnection::ConnectWaiterThreadProc) != FALSE)
            {
              hRes = S_OK;
            }
            else
            {
              lpConnectWaiter->lpPacket = NULL;
              delete lpConnectWaiter->lpWorkerThread;
              lpConnectWaiter->lpWorkerThread = NULL;
              Release();
              hRes = E_OUTOFMEMORY;
            }
          }
          else
          {
            hRes = E_OUTOFMEMORY;
          }
        }
        else
        {
          hRes = E_OUTOFMEMORY;
        }
      }
    }
  }
  if (FAILED(hRes))
  {
    cRwList.Remove(lpPacket);
    FreePacket(lpPacket);
  }
  return hRes;
}

HRESULT CSockets::CConnection::SetupAcceptEx(_In_ CConnection *lpIncomingConn)
{
  CPacketBase *lpPacket;
  DWORD dw;
  SIZE_T nReq;
  HRESULT hRes;

  nReq = (SIZE_T)SockAddrSizeFromWinSockFamily(sAddr.si_family) + 16;
  lpPacket = GetPacket(TypeAcceptEx, nReq * 2, FALSE);
  if (lpPacket == NULL)
    return E_OUTOFMEMORY;
  cRwList.QueueLast(lpPacket);
  lpPacket->SetUserData(lpIncomingConn);
  lpPacket->SetBytesInUse((DWORD)nReq);
  AddRef();
  lpIncomingConn->AddRef();
  if (((lpfnAcceptEx)(lpListener->fnAcceptEx))(sck, lpIncomingConn->sck, lpPacket->GetBuffer(), 0,
                      lpPacket->GetBytesInUse(), lpPacket->GetBytesInUse(), &dw, lpPacket->GetOverlapped()) != FALSE)
  {
    if (IS_COMPLETE_SYNC_AVAILABLE())
    {
      reinterpret_cast<CSockets*>(lpIpc)->OnDispatcherPacket(&(GetDispatcherPool()), 0, lpPacket->GetOverlapped(),
                                                             S_OK);
      hRes = S_OK;
    }
    else
    {
      hRes = S_FALSE;
    }
  }
  else
  {
    hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
    if (hRes == MX_E_IoPending)
    {
      hRes = S_FALSE;
    }
    else
    {
      lpIncomingConn->Release();
      Release();
    }
  }
  return hRes;
}

HRESULT CSockets::CConnection::ResolveAddress(_In_ DWORD dwResolverTimeoutMs, _In_opt_z_ LPCSTR szAddressA,
                                              _In_opt_ int nPort)
{
  CFastLock cHostResolverLock(&(sHostResolver.nMutex));
  RESOLVEADDRESS_PACKET_DATA *lpData;
  SIZE_T nAddrLen;
  HRESULT hRes;

  if (szAddressA == NULL || *szAddressA == 0)
    szAddressA = (nFamily != FamilyIPv6) ? "0.0.0.0" : "::";
  nAddrLen = MX::StrLenA(szAddressA);
  //create packet
  sHostResolver.lpPacket = GetPacket(TypeResolvingAddress, sizeof(RESOLVEADDRESS_PACKET_DATA), TRUE);
  if (sHostResolver.lpPacket == NULL)
    return E_OUTOFMEMORY;
  lpData = (RESOLVEADDRESS_PACKET_DATA*)(sHostResolver.lpPacket->GetBuffer());
  MemSet(lpData, 0, sizeof(RESOLVEADDRESS_PACKET_DATA));
  lpData->wPort = (WORD)nPort;
  //queue
  cRwList.QueueLast(sHostResolver.lpPacket);
  //try to resolve the address
  if (lpIpc->ShouldLog(2) != FALSE)
  {
    lpIpc->Log(L"CSockets::ResolveAddress) Clock=%lums / This=0x%p / Ovr=0x%p / Type=%lu",
                cHiResTimer.GetElapsedTimeMs(), this, sHostResolver.lpPacket->GetOverlapped(),
                sHostResolver.lpPacket->GetType());
  }
  //start address resolving with a timeout
  AddRef();
  hRes = HostResolver::Resolve(szAddressA, FamilyToWinSockFamily(nFamily), &(lpData->sAddr),
                               dwResolverTimeoutMs, MX_BIND_MEMBER_CALLBACK(&CConnection::HostResolveCallback, this),
                               NULL, &(sHostResolver.nResolverId));
  if (SUCCEEDED(hRes))
  {
    switch (lpData->sAddr.si_family)
    {
      case AF_INET:
        lpData->sAddr.Ipv4.sin_port = htons(lpData->wPort);
        break;
      case AF_INET6:
        lpData->sAddr.Ipv6.sin6_port = htons(lpData->wPort);
        break;
    }
    hRes = GetDispatcherPool().Post(GetDispatcherPoolPacketCallback(), 0, sHostResolver.lpPacket->GetOverlapped());
    if (FAILED(hRes))
      goto err_cannot_resolve;

  }
  else if (hRes != MX_E_IoPending)
  {
err_cannot_resolve:
    Release();
    cRwList.Remove(sHostResolver.lpPacket);
    FreePacket(sHostResolver.lpPacket);
    sHostResolver.lpPacket = NULL;
  }
  else
  {
    hRes = S_OK;
  }
  //done
  return hRes;
}

HRESULT CSockets::CConnection::SendReadPacket(_In_ CPacketBase *lpPacket, _Out_ LPDWORD lpdwRead)
{
  WSABUF sWsaBuf;
  DWORD dwToRead, dwRead, dwFlags;
  LONG nCurrThrottle;
  HRESULT hRes;

  if (sck == NULL)
  {
    *lpdwRead = 0;
    return S_FALSE;
  }
  dwToRead = lpPacket->GetBytesInUse();
  nCurrThrottle = __InterlockedRead(&nReadThrottle);
  if (dwToRead > (DWORD)nCurrThrottle)
  {
    dwToRead = (DWORD)nCurrThrottle;
    _InterlockedExchange(&nReadThrottle, nCurrThrottle << 1);
  }
  sWsaBuf.buf = (char*)(lpPacket->GetBuffer());
  sWsaBuf.len = (ULONG)dwToRead;
  dwRead = dwFlags = 0;
  if (lpIpc->ShouldLog(2) != FALSE)
  {
    lpIpc->Log(L"CSockets::SendReadPacket) Clock=%lums / Ovr=0x%p / Type=%lu / Bytes=%lu",
               cHiResTimer.GetElapsedTimeMs(), lpPacket->GetOverlapped(), lpPacket->GetType(),
               lpPacket->GetBytesInUse());
  }
  if (::WSARecv(sck, &sWsaBuf, 1, &dwRead, &dwFlags, lpPacket->GetOverlapped(), NULL) != SOCKET_ERROR)
  {
    hRes = (IS_COMPLETE_SYNC_AVAILABLE()) ? S_OK : MX_E_IoPending;
  }
  else
  {
    hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
    if (hRes == HRESULT_FROM_WIN32(WSAESHUTDOWN) || hRes == HRESULT_FROM_WIN32(WSAEDISCON))
      hRes = S_FALSE;
  }
  *lpdwRead = dwRead;
  return hRes;
}

HRESULT CSockets::CConnection::SendWritePacket(_In_ CPacketBase *lpPacket, _Out_ LPDWORD lpdwWritten)
{
  CAutoSlimRWLShared cHandleInUseLock(&nRwHandleInUse);
  CPacketBase *lpCurrPacket;
  WSABUF aWsaBuf[16];
  DWORD dwBuffersCount, dwWritten;
  HRESULT hRes;

  if (sck == NULL)
  {
    *lpdwWritten = 0;
    return S_FALSE;
  }
  dwBuffersCount = 0;
  for (lpCurrPacket = lpPacket; lpCurrPacket != NULL; lpCurrPacket = lpCurrPacket->GetChainedPacket())
  {
    aWsaBuf[dwBuffersCount].buf = (char*)(lpCurrPacket->GetBuffer());
    aWsaBuf[dwBuffersCount].len = (ULONG)(lpCurrPacket->GetBytesInUse());
    dwBuffersCount++;
    MX_ASSERT((SIZE_T)dwBuffersCount <= MX_ARRAYLEN(aWsaBuf));
    if (lpIpc->ShouldLog(2) != FALSE)
    {
      lpIpc->Log(L"CSockets::SendWritePacket) Clock=%lums / Ovr=0x%p / Type=%lu / Bytes=%lu",
                 cHiResTimer.GetElapsedTimeMs(), lpCurrPacket->GetOverlapped(), lpCurrPacket->GetType(),
                 lpCurrPacket->GetBytesInUse());
    }
  }
  dwWritten = 0;
  if (::WSASend(sck, aWsaBuf, dwBuffersCount, &dwWritten, 0, lpPacket->GetOverlapped(), NULL) != SOCKET_ERROR)
  {
    hRes = (IS_COMPLETE_SYNC_AVAILABLE()) ? S_OK : MX_E_IoPending;
  }
  else
  {
    hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
    if (hRes == HRESULT_FROM_WIN32(WSAESHUTDOWN) || hRes == HRESULT_FROM_WIN32(WSAEDISCON))
      hRes = S_FALSE;
  }
  *lpdwWritten = dwWritten;
  return hRes;
}

VOID CSockets::CConnection::HostResolveCallback(_In_ LONG nResolverId, _In_ PSOCKADDR_INET lpSockAddr,
                                                _In_ HRESULT hrErrorCode, _In_opt_ LPVOID lpUserData)
{
  HRESULT hRes;

  if (_InterlockedCompareExchange(&(sHostResolver.nResolverId), 0, nResolverId) == nResolverId)
  {
    CFastLock cHostResolverLock(&(sHostResolver.nMutex));

    //get myself and increase reference count if we are not destroying
    if (IsClosed() == FALSE)
    {
      //process resolver result
      if (SUCCEEDED(hrErrorCode))
      {
        RESOLVEADDRESS_PACKET_DATA *lpData = (RESOLVEADDRESS_PACKET_DATA*)(sHostResolver.lpPacket->GetBuffer());

        if (lpIpc->ShouldLog(2) != FALSE)
        {
          lpIpc->Log(L"CSockets::HostResolveCallback) Clock = %lums / Ovr = 0x%p / Type=%lu",
                     cHiResTimer.GetElapsedTimeMs(), sHostResolver.lpPacket->GetOverlapped(),
                     sHostResolver.lpPacket->GetType());
        }
        //set port
        switch (lpData->sAddr.si_family)
        {
          case AF_INET:
            lpData->sAddr.Ipv4.sin_port = htons(lpData->wPort);
            break;
          case AF_INET6:
            lpData->sAddr.Ipv6.sin6_port = htons(lpData->wPort);
            break;
        }
        //dispatch
        AddRef();
        hRes = GetDispatcherPool().Post(GetDispatcherPoolPacketCallback(), 0, sHostResolver.lpPacket->GetOverlapped());
        if (FAILED(hRes))
          Release();
      }
      else
      {
        hRes = hrErrorCode;
      }
    }
    else
    {
      hRes = MX_E_Cancelled;
    }
    if (FAILED(hRes))
    {
      cRwList.Remove(sHostResolver.lpPacket);
      FreePacket(sHostResolver.lpPacket);
      sHostResolver.lpPacket = NULL;
    }
  }
  //----
  if (FAILED(hRes))
    Close(hRes);
  //release myself
  Release();
  return;
}

VOID CSockets::CConnection::ConnectWaiterThreadProc()
{
  fd_set sWriteFds, sExceptFds;
  timeval sTv;
  int res;
  HANDLE hCancelEvent;
  HRESULT hRes;

  //wait for connection established or timeout
  hCancelEvent = lpConnectWaiter->lpWorkerThread->GetKillEvent();
  res = 0;
  while (res == 0)
  {
    if (::WaitForSingleObject(hCancelEvent, 0) == WAIT_OBJECT_0)
    {
      hRes = MX_E_Cancelled;
      break;
    }
    //check 4 socket write/except ready
    FD_ZERO(&sWriteFds);
    FD_SET(sck, &sWriteFds);
    FD_ZERO(&sExceptFds);
    FD_SET(sck, &sExceptFds);
    sTv.tv_sec = 0;
    sTv.tv_usec = 10000; //10ms
    res = ::select(0, NULL, &sWriteFds, &sExceptFds, &sTv);
  }
  if (res != 0)
  {
    if (res != SOCKET_ERROR)
    {
      //check if socket is writable
      hRes = (FD_ISSET(sck, &sWriteFds)) ? S_OK : MX_HRESULT_FROM_WIN32(WSAECONNRESET);
    }
    else
    {
      hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
    }
  }
  //process data
  if (SUCCEEDED(hRes))
  {
    if (IsClosed() == FALSE)
    {
      *((HRESULT MX_UNALIGNED*)(lpConnectWaiter->lpPacket->GetBuffer())) = hRes;
      AddRef();
      hRes = GetDispatcherPool().Post(GetDispatcherPoolPacketCallback(), 0, lpConnectWaiter->lpPacket->GetOverlapped());
      if (FAILED(hRes))
        Release();
    }
    else
    {
      hRes = MX_E_Cancelled;
    }
  }
  if (FAILED(hRes))
  {
    cRwList.Remove(lpConnectWaiter->lpPacket);
    FreePacket(lpConnectWaiter->lpPacket);
  }
  lpConnectWaiter->lpWorkerThread = NULL;
  lpConnectWaiter->lpPacket = NULL;
  //----
  if (FAILED(hRes))
    Close(hRes);
  //release myself
  Release();
  return;
}

VOID CSockets::CConnection::ListenerThreadProc()
{
  HANDLE hEvents[3];
  DWORD dw, dwErrorsCount;

  hEvents[0] = lpListener->lpWorkerThread->GetKillEvent();
  hEvents[1] = lpListener->hAcceptSelect;
  hEvents[2] = lpListener->hAcceptCompleted;

  while (lpIpc->IsShuttingDown() == FALSE)
  {
    ::ResetEvent(lpListener->hAcceptSelect);
    ::ResetEvent(lpListener->hAcceptCompleted);

    dwErrorsCount = 0;
    while (dwErrorsCount < 64 &&
           ::WaitForSingleObject(hEvents[0], 0) != WAIT_OBJECT_0 &&
           __InterlockedIncrementIfLessThan(&(lpListener->nAcceptsInProgress),
                                            (LONG)(lpListener->dwMaxAcceptsToPost)) != FALSE)
    {
      HRESULT hRes;

      hRes = ((CSockets*)lpIpc)->CreateServerConnection(this);
      if (SUCCEEDED(hRes))
      {
        if (hRes == S_OK)
          _InterlockedDecrement(&(lpListener->nAcceptsInProgress));
        dwErrorsCount = 0;
      }
      else
      {
        _InterlockedDecrement(&(lpListener->nAcceptsInProgress));

        if (hRes != MX_HRESULT_FROM_WIN32(ERROR_NETNAME_DELETED) && hRes != MX_HRESULT_FROM_WIN32(WSAECONNRESET) &&
            hRes != HRESULT_FROM_NT(STATUS_LOCAL_DISCONNECT) && hRes != HRESULT_FROM_NT(STATUS_REMOTE_DISCONNECT))
        {
          ((CSockets*)lpIpc)->FireOnEngineError(hRes);
        }
        dwErrorsCount++;
      }
    }

    dw = ::WaitForMultipleObjects(3, hEvents, FALSE, INFINITE);
    if (dw == WAIT_OBJECT_0)
      break;

    if (dw == WAIT_OBJECT_0 + 2)
    {
      if (lpListener->dwMaxAcceptsToPost < MAX_TOTAL_ACCEPTS_TO_POST - 10)
      {
        lpListener->dwMaxAcceptsToPost += 10;
      }
      else
      {
        lpListener->dwMaxAcceptsToPost = MAX_TOTAL_ACCEPTS_TO_POST;
      }
    }
  }
  //done
  return;
}

} //namespace MX

//-----------------------------------------------------------

static HRESULT Winsock_Init()
{
  static LONG volatile nMutex = 0;
  static LONG volatile nInitialized = 0;

  if (__InterlockedRead(&nInitialized) == 0)
  {
    MX::CFastLock cLock(&nMutex);
    WSADATA sWsaData;
    HRESULT hRes;

    if (__InterlockedRead(&nInitialized) == 0)
    {
      MX::MemSet(&sWsaData, 0, sizeof(sWsaData));
      hRes = MX_HRESULT_FROM_WIN32(::WSAStartup(MAKEWORD(2, 2), &sWsaData));
      if (FAILED(hRes))
        return hRes;
      //register shutdown callback
      hRes = MX::RegisterFinalizer(&Winsock_Shutdown, SOCKETS_FINALIZER_PRIORITY);
      if (FAILED(hRes))
      {
        Winsock_Shutdown();
        return hRes;
      }
      //done
      _InterlockedExchange(&nInitialized, 1);
    }
  }
  return S_OK;
}

static VOID Winsock_Shutdown()
{
  ::WSACleanup();
  return;
}

static int FamilyToWinSockFamily(_In_ MX::CSockets::eFamily nFamily)
{
  switch (nFamily)
  {
    case MX::CSockets::FamilyIPv4:
      return AF_INET;
    case MX::CSockets::FamilyIPv6:
      return AF_INET6;
  }
  return AF_UNSPEC;
}

static int SockAddrSizeFromWinSockFamily(_In_ int nFamily)
{
  switch (nFamily)
  {
    case AF_INET:
      return (int)sizeof(SOCKADDR_IN);
    case AF_INET6:
      return (int)sizeof(SOCKADDR_IN6);
  }
  return 0;
}

static lpfnAcceptEx GetAcceptEx(_In_ SOCKET sck)
{
  static const GUID sGuid_AcceptEx = {
    0xB5367DF1, 0xCBAC, 0x11CF, { 0x95, 0xCA, 0x00, 0x80, 0x5F, 0x48, 0xA1, 0x92 }
  };
  DWORD dw = 0;
  lpfnAcceptEx fnAcceptEx = NULL;

  if (::WSAIoctl(sck, SIO_GET_EXTENSION_FUNCTION_POINTER, (LPVOID)&sGuid_AcceptEx,
                 (DWORD)sizeof(sGuid_AcceptEx), &fnAcceptEx, (DWORD)sizeof(fnAcceptEx), &dw, NULL,
                 NULL) == SOCKET_ERROR)
  {
    return NULL;
  }
  return fnAcceptEx;
}

static lpfnGetAcceptExSockaddrs GetAcceptExSockaddrs(_In_ SOCKET sck)
{
  static const GUID sGuid_GetAcceptExSockAddrs = {
    0xB5367DF2, 0xCBAC, 0x11CF, { 0x95, 0xCA, 0x00, 0x80, 0x5F, 0x48, 0xA1, 0x92 }
  };
  DWORD dw = 0;
  lpfnGetAcceptExSockaddrs fnGetAcceptExSockaddrs = NULL;

  if (::WSAIoctl(sck, SIO_GET_EXTENSION_FUNCTION_POINTER, (LPVOID)&sGuid_GetAcceptExSockAddrs,
                 (DWORD)sizeof(sGuid_GetAcceptExSockAddrs), &fnGetAcceptExSockaddrs,
                 (DWORD)sizeof(fnGetAcceptExSockaddrs), &dw, NULL, NULL) == SOCKET_ERROR)
  {
    return NULL;
  }
  return fnGetAcceptExSockaddrs;
}

static lpfnConnectEx GetConnectEx(_In_ SOCKET sck)
{
  static const GUID sGuid_ConnectEx = {
    0x25A207B9, 0xDDF3, 0x4660, { 0x8E, 0xE9, 0x76, 0xE5, 0x8C, 0x74, 0x06, 0x3E }
  };
  DWORD dw = 0;
  lpfnConnectEx fnConnectEx = NULL;

  if (::WSAIoctl(sck, SIO_GET_EXTENSION_FUNCTION_POINTER, (LPVOID)&sGuid_ConnectEx, (DWORD)sizeof(sGuid_ConnectEx),
                 &fnConnectEx, (DWORD)sizeof(fnConnectEx), &dw, NULL, NULL) == SOCKET_ERROR)
  {
    return NULL;
  }
  return fnConnectEx;
}

static lpfnDisconnectEx GetDisconnectEx(_In_ SOCKET sck)
{
  static const GUID sGuid_DisconnectEx = {
    0x7FDA2E11, 0x8630, 0x436F, { 0xA0, 0x31, 0xF5, 0x36, 0xA6, 0xEE, 0xC1, 0x57 }
  };
  DWORD dw = 0;
  lpfnDisconnectEx fnDisconnectEx = NULL;

  if (::WSAIoctl(sck, SIO_GET_EXTENSION_FUNCTION_POINTER, (LPVOID)&sGuid_DisconnectEx,
                 (DWORD)sizeof(sGuid_DisconnectEx), &fnDisconnectEx, (DWORD)sizeof(fnDisconnectEx), &dw, NULL,
                 NULL) == SOCKET_ERROR)
  {
    return NULL;
  }
  return fnDisconnectEx;
}

static int SocketInsertFunc(_In_ LPVOID lpContext, _In_ SOCKET *lpElem1, _In_ SOCKET *lpElem2)
{
  if ((SIZE_T)(*lpElem1) < (SIZE_T)(*lpElem2))
    return -1;
  if ((SIZE_T)(*lpElem1) > (SIZE_T)(*lpElem2))
    return 1;
  return 0;
}

static int SocketSearchFunc(_In_ LPVOID lpContext, _In_ SOCKET sck, _In_ SOCKET *lpElem)
{
  if ((SIZE_T)sck < (SIZE_T)(*lpElem))
    return -1;
  if ((SIZE_T)sck > (SIZE_T)(*lpElem))
    return 1;
  return 0;
}
