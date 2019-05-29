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

static __inline HRESULT MX_HRESULT_FROM_LASTSOCKETERROR()
{
  HRESULT hRes = MX_HRESULT_FROM_WIN32(::WSAGetLastError());
  return (hRes != MX_HRESULT_FROM_WIN32(WSAEWOULDBLOCK)) ? hRes : MX_E_IoPending;
}

#define TypeListenRequest     (CPacket::eType)((int)CPacket::TypeMAX + 1)
#define TypeListen            (CPacket::eType)((int)CPacket::TypeMAX + 2)
#define TypeResolvingAddress  (CPacket::eType)((int)CPacket::TypeMAX + 3)
#define TypeAcceptEx          (CPacket::eType)((int)CPacket::TypeMAX + 4)
#define TypeConnect           (CPacket::eType)((int)CPacket::TypeMAX + 5)
#define TypeConnectEx         (CPacket::eType)((int)CPacket::TypeMAX + 6)
#define TypeDisconnectEx      (CPacket::eType)((int)CPacket::TypeMAX + 7)

#define SO_UPDATE_ACCEPT_CONTEXT    0x700B
#define SO_UPDATE_CONNECT_CONTEXT   0x7010

#ifndef TF_REUSE_SOCKET
  #define TF_REUSE_SOCKET     0x02
#endif //!TF_REUSE_SOCKET

#define TCP_KEEPALIVE_MS             45000

#ifndef FILE_SKIP_SET_EVENT_ON_HANDLE
  #define FILE_SKIP_SET_EVENT_ON_HANDLE           0x2
#endif //!FILE_SKIP_SET_EVENT_ON_HANDLE

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

typedef struct {
  HRESULT MX_UNALIGNED hRes;
  WORD wPort;
  SOCKADDR_INET sAddr;
} RESOLVEADDRESS_PACKET_DATA;

//-----------------------------------------------------------

static LONG volatile nSocketInitCount = 0;
static LONG volatile nOSVersion = -1;
static lpfnSetFileCompletionNotificationModes volatile fnSetFileCompletionNotificationModes = NULL;

//-----------------------------------------------------------

static int FamilyToWinSockFamily(_In_ MX::CSockets::eFamily nFamily);
static int SockAddrSizeFromWinSockFamily(_In_ int nFamily);
static HRESULT Winsock_Init();
static VOID Winsock_Shutdown();

//-----------------------------------------------------------

namespace MX {

CSockets::CSockets(_In_ CIoCompletionPortThreadPool &cDispatcherPool) : CIpc(cDispatcherPool)
{
  SIZE_T i;

  dwBackLogSize = 0;
  dwMaxAcceptsToPost = 4;
  dwAddressResolverTimeoutMs = 3000;
  for (i = 0; i < MX_ARRAYLEN(sReusableSockets); i++)
    _InterlockedExchange(&(sReusableSockets[i].nMutex), 0);
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
    CAutoSlimRWLExclusive cConnListLock(&(sConnections.nSlimMutex));

    sConnections.cTree.Insert(cConn.Get());
  }
  hRes = FireOnCreate(cConn.Get());
  if (SUCCEEDED(hRes))
    hRes = cConn->CreateSocket(0);
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
    CAutoSlimRWLExclusive cConnListLock(&(sConnections.nSlimMutex));

    sConnections.cTree.Insert(cConn.Get());
  }
  hRes = FireOnCreate(cConn.Get());
  if (SUCCEEDED(hRes))
    hRes = cConn->CreateSocket(dwPacketSize);
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
  //initialize WinSock
  return Winsock_Init();
}

VOID CSockets::OnInternalFinalize()
{
  SIZE_T i;

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
    CAutoSlimRWLExclusive cConnListLock(&(sConnections.nSlimMutex));

    sConnections.cTree.Insert(cIncomingConn.Get());
  }
  hRes = FireOnCreate(cIncomingConn.Get());
  if (SUCCEEDED(hRes))
    hRes = cIncomingConn->CreateSocket(dwPacketSize);
  if (SUCCEEDED(hRes))
    hRes = lpListenConn->SetupAcceptEx(cIncomingConn.Get());
  //done
  if (FAILED(hRes))
  {
    cIncomingConn->Close(hRes);
  }
  cIncomingConn.Detach();
  return hRes;
}

HRESULT CSockets::OnPreprocessPacket(_In_ DWORD dwBytes, _In_ CPacket *lpPacket, _In_ HRESULT hRes)
{
  return S_FALSE;
}

HRESULT CSockets::OnCustomPacket(_In_ DWORD dwBytes, _In_ CPacket *lpPacket, _In_ HRESULT hRes)
{
  CConnection *lpConn;

  lpConn = (CConnection*)(lpPacket->GetConn());
  switch (lpPacket->GetType())
  {
    case TypeResolvingAddress:
      {
      RESOLVEADDRESS_PACKET_DATA *lpData = (RESOLVEADDRESS_PACKET_DATA*)(lpPacket->GetBuffer());

      hRes = lpData->hRes;
      if (SUCCEEDED(hRes))
      {
        //copy address
        MemCopy(&(lpConn->sAddr), &(lpData->sAddr), sizeof(lpData->sAddr));
        //connect/listen
        switch (lpConn->nClass)
        {
          case CIpc::ConnectionClassListener:
            hRes = lpConn->SetupListener((dwBackLogSize != 0) ? (int)dwBackLogSize : SOMAXCONN);
            if (SUCCEEDED(hRes))
            {
              for (DWORD i = 0; i < dwMaxAcceptsToPost; i++)
              {
                hRes = CreateServerConnection(lpConn);
                if (FAILED(hRes))
                  break;
              }
            }
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
      }
      break;

    case TypeAcceptEx:
      {
      CConnection *lpIncomingConn = (CConnection*)(lpPacket->GetUserData());

      if (SUCCEEDED(hRes) && IsShuttingDown() != FALSE)
        hRes = MX_E_Cancelled;
      if (SUCCEEDED(hRes))
      {
        if (::setsockopt(lpIncomingConn->sck, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&(lpConn->sck),
                          sizeof(lpConn->sck)) == SOCKET_ERROR)
        {
          hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
        }
      }
      if (SUCCEEDED(hRes))
      {
        static const GUID sGuid_GetAcceptExSockAddrs = {
          0xB5367DF2, 0xCBAC, 0x11CF, { 0x95, 0xCA, 0x00, 0x80, 0x5F, 0x48, 0xA1, 0x92 }
        };
        sockaddr *lpLocalAddr, *lpPeerAddr;
        INT nLocalAddrLen, nPeerAddrLen;
        lpfnGetAcceptExSockaddrs fnGetAcceptExSockaddrs;
        DWORD dw;

        lpLocalAddr = lpPeerAddr = NULL;
        nLocalAddrLen = nPeerAddrLen = 0;
        //get extension function addresses and peer address
        fnGetAcceptExSockaddrs = NULL;
        dw = 0;
        if (::WSAIoctl(lpIncomingConn->sck, SIO_GET_EXTENSION_FUNCTION_POINTER, (LPVOID)&sGuid_GetAcceptExSockAddrs,
                       (DWORD)sizeof(sGuid_GetAcceptExSockAddrs), &fnGetAcceptExSockaddrs,
                       (DWORD)sizeof(fnGetAcceptExSockaddrs), &dw, NULL, NULL) != SOCKET_ERROR &&
            fnGetAcceptExSockaddrs != NULL)
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
              hRes = S_OK;
            }
            else if (sPeerAddr.si_family == AF_INET6 && len >= sizeof(SOCKADDR_IN6))
            {
              MemCopy(&(lpIncomingConn->sAddr.Ipv6), (PSOCKADDR_IN6)&sPeerAddr, sizeof(SOCKADDR_IN6));
              hRes = S_OK;
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
      //instatiate a new accept socket
      hRes = S_OK;
      if (lpConn->IsClosed() == FALSE && IsShuttingDown() == FALSE)
      {
        hRes = CreateServerConnection(lpConn);
        if (FAILED(hRes))
          FireOnEngineError(hRes);
      }
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

    case TypeDisconnectEx:
      {
      SOCKET sck = (SOCKET)(lpPacket->GetUserData());

      if (SUCCEEDED(hRes))
      {
        SIZE_T nIndex = (lpConn->nFamily == FamilyIPv4) ? 0 : 1;

        {
          CFastLock cLock(&(sReusableSockets[nIndex].nMutex));

          if (sReusableSockets[nIndex].aList.AddElement(sck) == FALSE)
            ::closesocket(sck);
        }
      }
      else
      {
        ::closesocket(sck);
      }
      FreePacket(lpPacket);
      }
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
  MemSet(&sHostResolver, 0, sizeof(sConnectWaiter));
  MemSet(&sConnectWaiter, 0, sizeof(sConnectWaiter));
  _InterlockedExchange(&nReadThrottle, 128);
  nFamily = _nFamily;
  return;
}

CSockets::CConnection::~CConnection()
{
  MX_ASSERT(sck == NULL);
  MX_ASSERT(sConnectWaiter.lpWorkerThread == NULL);
  MX_ASSERT(sHostResolver.lpResolver == NULL);
  return;
}

HRESULT CSockets::CConnection::CreateSocket(_In_ DWORD dwPacketSize)
{
  u_long nYes;
  struct tcp_keepalive sKeepAliveData;
  DWORD dw;

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
  if (::ioctlsocket(sck, FIONBIO, &nYes) == SOCKET_ERROR)
    return MX_HRESULT_FROM_LASTSOCKETERROR();
  //tcp no delay
  nYes = 1;
  if (::setsockopt(sck, IPPROTO_TCP, TCP_NODELAY, (char*)&nYes, (int)sizeof(nYes)) == SOCKET_ERROR)
    return MX_HRESULT_FROM_LASTSOCKETERROR();

  //detect OS version
  if (__InterlockedRead(&nOSVersion) < 0)
  {
    HINSTANCE hKernel32Dll;
    LONG _nOSVersion = 0;

    if (::IsWindows8OrGreater() != FALSE)
      _nOSVersion = 0x0602;
    else if (::IsWindowsVistaOrGreater() != FALSE)
      _nOSVersion = 0x0600;

    hKernel32Dll = ::GetModuleHandleW(L"kernel32.dll");
    if (hKernel32Dll != NULL)
    {
      fnSetFileCompletionNotificationModes = (lpfnSetFileCompletionNotificationModes)::GetProcAddress(hKernel32Dll,
                                                                "SetFileCompletionNotificationModes");
    }

#pragma warning(default : 4996)
    _InterlockedExchange(&nOSVersion, _nOSVersion);
  }

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
  //loopback fast path on Win8+
  if (__InterlockedRead(&nOSVersion) >= 0x0602)
  {
    nYes = 1;
    if (::WSAIoctl(sck, SIO_LOOPBACK_FAST_PATH, &nYes, sizeof(nYes), NULL, 0, &dw, NULL, NULL) == SOCKET_ERROR)
      return MX_HRESULT_FROM_LASTSOCKETERROR();
  }
  //miscellaneous options
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
      break;

    case CIpc::ConnectionClassListener:
      //reuse address
      if (::setsockopt(sck, SOL_SOCKET, SO_REUSEADDR, (char*)&nYes, (int)sizeof(nYes)) == SOCKET_ERROR)
        return MX_HRESULT_FROM_LASTSOCKETERROR();
      break;
  }
  if (fnSetFileCompletionNotificationModes != NULL)
  {
    if (fnSetFileCompletionNotificationModes((HANDLE)sck, FILE_SKIP_SET_EVENT_ON_HANDLE) == FALSE)
      return MX_HRESULT_FROM_LASTERROR();
  }
  //attach to completion port
  return GetDispatcherPool().Attach((HANDLE)sck, GetDispatcherPoolPacketCallback());
}

VOID CSockets::CConnection::ShutdownLink(_In_ BOOL bAbortive)
{
  {
    CAutoSlimRWLExclusive cHandleInUseLock(&nRwHandleInUse);

    if (sck != NULL)
    {
      static const GUID sGuid_DisconnectEx = {
        0x7FDA2E11, 0x8630, 0x436F, { 0xA0, 0x31, 0xF5, 0x36, 0xA6, 0xEE, 0xC1, 0x57 }
      };
      lpfnDisconnectEx fnDisconnectEx;
      LINGER sLinger;

      //get extension function addresses
      fnDisconnectEx = NULL;
      if (nClass == ConnectionClassServer && lpIpc->IsShuttingDown() == FALSE && IsConnected() != FALSE)
      {
        DWORD dw = 0;

        if (::WSAIoctl(sck, SIO_GET_EXTENSION_FUNCTION_POINTER, (LPVOID)&sGuid_DisconnectEx,
                       (DWORD)sizeof(sGuid_DisconnectEx), &fnDisconnectEx, (DWORD)sizeof(fnDisconnectEx), &dw, NULL,
                       NULL) == SOCKET_ERROR)
        {
          fnDisconnectEx = NULL;
        }
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
      if (fnDisconnectEx != NULL)
      {
        CIpc::CPacket *lpPacket;

        lpPacket = GetPacket(TypeDisconnectEx);
        if (lpPacket != NULL)
        {
          lpPacket->SetUserData((LPVOID)sck);
          AddRef();
          if (fnDisconnectEx(sck, lpPacket->GetOverlapped(), TF_REUSE_SOCKET, 0) == FALSE)
          {
            HRESULT hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
            if (FAILED(hRes) && hRes != MX_E_IoPending)
            {
              Release();
              ::closesocket(sck);
            }
          }
        }
        else
        {
          ::closesocket(sck);
        }
      }
      else
      {
        ::closesocket(sck);
      }
      sck = NULL;
    }
  }
  //cancel connect worker thread
  {
    CFastLock cConnectWaiterLock(&(sConnectWaiter.nMutex));

    if (sConnectWaiter.lpWorkerThread != NULL)
      sConnectWaiter.lpWorkerThread->StopAsync();
  }
  //cancel host resolver
  {
    CFastLock cHostResolverLock(&(sHostResolver.nMutex));

    if (sHostResolver.lpResolver != NULL)
      sHostResolver.lpResolver->Cancel();
  }
  //call base
  CConnectionBase::ShutdownLink(bAbortive);
  return;
}

HRESULT CSockets::CConnection::SetupListener(_In_ int nBackLogSize)
{
  return (::bind(sck, (sockaddr*)&sAddr, SockAddrSizeFromWinSockFamily(sAddr.si_family)) == SOCKET_ERROR ||
          ::listen(sck, nBackLogSize) == SOCKET_ERROR) ? MX_HRESULT_FROM_LASTSOCKETERROR() : S_OK;
}

HRESULT CSockets::CConnection::SetupClient()
{
  static const GUID sGuid_ConnectEx = {
    0x25A207B9, 0xDDF3, 0x4660, { 0x8E, 0xE9, 0x76, 0xE5, 0x8C, 0x74, 0x06, 0x3E }
  };
  CPacket *lpPacket;
  SOCKADDR_INET sBindAddr;
  lpfnConnectEx fnConnectEx;
  DWORD dw;
  HRESULT hRes;

  //get extension function addresses
  fnConnectEx = NULL;
  dw = 0;
  if (::WSAIoctl(sck, SIO_GET_EXTENSION_FUNCTION_POINTER, (LPVOID)&sGuid_ConnectEx, (DWORD)sizeof(sGuid_ConnectEx),
                 &fnConnectEx, (DWORD)sizeof(fnConnectEx), &dw, NULL, NULL) == SOCKET_ERROR)
  {
    fnConnectEx = NULL;
  }
  //----
  MemSet(&sBindAddr, 0, sizeof(sBindAddr));
  sBindAddr.si_family = sAddr.si_family;
  if (::bind(sck, (sockaddr*)&sBindAddr, SockAddrSizeFromWinSockFamily(sAddr.si_family)) == SOCKET_ERROR)
    return MX_HRESULT_FROM_LASTSOCKETERROR();
  lpPacket = GetPacket((fnConnectEx != NULL) ? (TypeConnectEx) : (TypeConnect));
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
    //do connect
    AddRef();
    if (fnConnectEx(sck, (sockaddr*)&sAddr, SockAddrSizeFromWinSockFamily(sAddr.si_family), NULL, 0, &dw,
                    lpPacket->GetOverlapped()) == FALSE)
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
        CFastLock cConnectWaiterLock(&(sConnectWaiter.nMutex));

        //start connect waiter
        sConnectWaiter.lpWorkerThread = MX_DEBUG_NEW TClassWorkerThread<CConnection>();
        if (sConnectWaiter.lpWorkerThread != NULL)
        {
          sConnectWaiter.lpWorkerThread->SetAutoDelete(TRUE);
          AddRef();
          if (sConnectWaiter.lpWorkerThread->Start(this, &CConnection::ConnectWaiterThreadProc) != FALSE)
          {
            sConnectWaiter.lpPacket = lpPacket;
            hRes = S_OK;
          }
          else
          {
            delete sConnectWaiter.lpWorkerThread;
            sConnectWaiter.lpWorkerThread = NULL;
            Release();
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
  static const GUID sGuid_AcceptEx = {
    0xB5367DF1, 0xCBAC, 0x11CF, { 0x95, 0xCA, 0x00, 0x80, 0x5F, 0x48, 0xA1, 0x92 }
  };
  CPacket *lpPacket;
  lpfnAcceptEx fnAcceptEx;
  DWORD dw;
  HRESULT hRes;

  //get extension function addresses
  fnAcceptEx = NULL;
  dw = 0;
  if (::WSAIoctl(sck, SIO_GET_EXTENSION_FUNCTION_POINTER, (LPVOID)&sGuid_AcceptEx, (DWORD)sizeof(sGuid_AcceptEx),
                 &fnAcceptEx, (DWORD)sizeof(fnAcceptEx), &dw, NULL, NULL) == SOCKET_ERROR ||
      fnAcceptEx == NULL)
  {
    return MX_E_ProcNotFound;
  }
  lpPacket = GetPacket(TypeAcceptEx);
  if (lpPacket == NULL)
    return E_OUTOFMEMORY;
  cRwList.QueueLast(lpPacket);
  lpPacket->SetUserData(lpIncomingConn);
  lpPacket->SetBytesInUse((DWORD)SockAddrSizeFromWinSockFamily(sAddr.si_family) + 16);
  AddRef();
  lpIncomingConn->AddRef();
  if (fnAcceptEx(sck, lpIncomingConn->sck, lpPacket->GetBuffer(), 0, lpPacket->GetBytesInUse(),
                 lpPacket->GetBytesInUse(), &dw, lpPacket->GetOverlapped()) == FALSE)
  {
    hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
    if (hRes != MX_E_IoPending)
    {
      lpIncomingConn->Release();
      Release();
      return hRes;
    }
  }
  return S_OK;
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
  sHostResolver.lpPacket = GetPacket(TypeResolvingAddress);
  if (sHostResolver.lpPacket == NULL)
    return E_OUTOFMEMORY;
  lpData = (RESOLVEADDRESS_PACKET_DATA*)(sHostResolver.lpPacket->GetBuffer());
  MemSet(lpData, 0, sizeof(RESOLVEADDRESS_PACKET_DATA));
  lpData->wPort = (WORD)nPort;
  //queue
  cRwList.QueueLast(sHostResolver.lpPacket);
  //is a numeric IP address?
  switch (nFamily)
  {
    case FamilyIPv4:
      if (CHostResolver::IsValidIPV4(szAddressA, nAddrLen, &(lpData->sAddr)) != FALSE)
      {
        lpData->sAddr.Ipv4.sin_port = htons(lpData->wPort);

        AddRef();
        hRes = GetDispatcherPool().Post(GetDispatcherPoolPacketCallback(), 0, sHostResolver.lpPacket->GetOverlapped());
        if (FAILED(hRes))
          Release();
        goto done;
      }
      break;

    case FamilyIPv6:
      if (CHostResolver::IsValidIPV6(szAddressA, nAddrLen, &(lpData->sAddr)) != FALSE)
      {
        lpData->sAddr.Ipv6.sin6_port = htons(lpData->wPort);

        AddRef();
        hRes = GetDispatcherPool().Post(GetDispatcherPoolPacketCallback(), 0, sHostResolver.lpPacket->GetOverlapped());
        if (FAILED(hRes))
          Release();
        goto done;
      }
      break;

    default:
      return E_INVALIDARG;
  }
  //try to resolve the address
  if (lpIpc->ShouldLog(2) != FALSE)
  {
    lpIpc->Log(L"CSockets::ResolveAddress) Clock=%lums / This=0x%p / Ovr=0x%p / Type=%lu",
                cHiResTimer.GetElapsedTimeMs(), this, sHostResolver.lpPacket->GetOverlapped(),
                sHostResolver.lpPacket->GetType());
  }
  //create resolver and attach packet. no need to lock because it is called on socket creation
  sHostResolver.lpResolver = MX_DEBUG_NEW CHostResolver();
  if (sHostResolver.lpResolver != NULL)
  {
    //start address resolving with a timeout
    AddRef();
    hRes = sHostResolver.lpResolver->ResolveAsync(szAddressA, FamilyToWinSockFamily(nFamily),
                                                  dwResolverTimeoutMs,
                                                  MX_BIND_MEMBER_CALLBACK(&CConnection::HostResolveCallback, this),
                                                  NULL);
    if (FAILED(hRes))
      Release();
  }
  else
  {
    hRes = E_OUTOFMEMORY;
  }
done:
  //done
  if (FAILED(hRes))
  {
    cRwList.Remove(sHostResolver.lpPacket);
    FreePacket(sHostResolver.lpPacket);
    sHostResolver.lpPacket = NULL;
  }
  return hRes;
}

HRESULT CSockets::CConnection::SendReadPacket(_In_ CPacket *lpPacket)
{
  WSABUF sWsaBuf;
  DWORD dwToRead, dwReaded, dwFlags;
  LONG nCurrThrottle;
  HRESULT hRes;

  if (sck == NULL)
    return S_FALSE;
  dwToRead = lpPacket->GetBytesInUse();
  nCurrThrottle = __InterlockedRead(&nReadThrottle);
  if (dwToRead > (DWORD)nCurrThrottle)
  {
    dwToRead = (DWORD)nCurrThrottle;
    _InterlockedExchange(&nReadThrottle, nCurrThrottle<<1);
  }
  sWsaBuf.buf = (char*)(lpPacket->GetBuffer());
  sWsaBuf.len = (ULONG)dwToRead;
  dwReaded = dwFlags = 0;
  if (lpIpc->ShouldLog(2) != FALSE)
  {
    lpIpc->Log(L"CSockets::SendReadPacket) Clock=%lums / Ovr=0x%p / Type=%lu / Bytes=%lu",
               cHiResTimer.GetElapsedTimeMs(), lpPacket->GetOverlapped(), lpPacket->GetType(),
               lpPacket->GetBytesInUse());
  }
  hRes = S_OK;
  if (::WSARecv(sck, &sWsaBuf, 1, &dwReaded, &dwFlags, lpPacket->GetOverlapped(), NULL) == SOCKET_ERROR)
  {
    hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
    if (hRes == HRESULT_FROM_WIN32(WSAESHUTDOWN) || hRes == HRESULT_FROM_WIN32(WSAEDISCON))
      hRes = S_FALSE;
  }
  return hRes;
}

HRESULT CSockets::CConnection::SendWritePacket(_In_ CPacket *lpPacket)
{
  CAutoSlimRWLShared cHandleInUseLock(&nRwHandleInUse);
  WSABUF sWsaBuf;
  DWORD dwWritten;
  HRESULT hRes;

  if (sck == NULL)
    return S_FALSE;
  sWsaBuf.buf = (char*)(lpPacket->GetBuffer());
  sWsaBuf.len = (ULONG)(lpPacket->GetBytesInUse());
  dwWritten = 0;
  if (lpIpc->ShouldLog(2) != FALSE)
  {
    lpIpc->Log(L"CSockets::SendWritePacket) Clock=%lums / Ovr=0x%p / Type=%lu / Bytes=%lu",
               cHiResTimer.GetElapsedTimeMs(), lpPacket->GetOverlapped(), lpPacket->GetType(),
               lpPacket->GetBytesInUse());
  }
  hRes = S_OK;
  if (::WSASend(sck, &sWsaBuf, 1, &dwWritten, 0, lpPacket->GetOverlapped(), NULL) == SOCKET_ERROR)
  {
    hRes = MX_HRESULT_FROM_LASTSOCKETERROR();
    if (hRes == HRESULT_FROM_WIN32(WSAESHUTDOWN) || hRes == HRESULT_FROM_WIN32(WSAEDISCON))
      hRes = S_FALSE;
  }
  return hRes;
}

VOID CSockets::CConnection::HostResolveCallback(_In_ CHostResolver *lpResolver, _In_ SOCKADDR_INET &sAddr,
                                                _In_ HRESULT hrErrorCode, _In_opt_ LPVOID lpUserData)
{
  HRESULT hRes;

  {
    CFastLock cHostResolverLock(&(sHostResolver.nMutex));

    //get myself and increase reference count if we are not destroying
    if (IsClosed() == FALSE && sHostResolver.lpResolver == lpResolver)
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
        lpData->hRes = hrErrorCode;
        if (SUCCEEDED(lpData->hRes))
        {
          //copy address
          MemCopy(&(lpData->sAddr), &sAddr, sizeof(sAddr));
          switch (lpData->sAddr.si_family)
          {
            case AF_INET:
              lpData->sAddr.Ipv4.sin_port = htons(lpData->wPort);
              break;
            case AF_INET6:
              lpData->sAddr.Ipv6.sin6_port = htons(lpData->wPort);
              break;
          }
        }
        AddRef();
        hRes = GetDispatcherPool().Post(GetDispatcherPoolPacketCallback(), 0, sHostResolver.lpPacket->GetOverlapped());
        if (FAILED(hRes))
          Release();
      }
      else
      {
        hRes = hrErrorCode;
      }

      sHostResolver.lpResolver = NULL;
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
  //done with resolver
  delete lpResolver;
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
  hCancelEvent = sConnectWaiter.lpWorkerThread->GetKillEvent();
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
  {
    CFastLock cConnectWaiterLock(&(sConnectWaiter.nMutex));

    if (IsClosed() == FALSE)
    {
      *((HRESULT MX_UNALIGNED*)(sConnectWaiter.lpPacket->GetBuffer())) = hRes;
      AddRef();
      hRes = GetDispatcherPool().Post(GetDispatcherPoolPacketCallback(), 0, sConnectWaiter.lpPacket->GetOverlapped());
      if (FAILED(hRes))
      {
        Release();
        cRwList.Remove(sConnectWaiter.lpPacket);
        FreePacket(sConnectWaiter.lpPacket);
      }
    }
    else
    {
      hRes = MX_E_Cancelled;
      cRwList.Remove(sConnectWaiter.lpPacket);
      FreePacket(sConnectWaiter.lpPacket);
    }
    sConnectWaiter.lpWorkerThread = NULL;
    sConnectWaiter.lpPacket = NULL;
  }
  //----
  if (FAILED(hRes))
    Close(hRes);
  //release myself
  Release();
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
