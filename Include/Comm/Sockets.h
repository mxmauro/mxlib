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
#ifndef _MX_SOCKETS_H
#define _MX_SOCKETS_H

#include "IpcCommon.h"
#include "HostResolver.h"
#include "..\TimedEventQueue.h"

//-----------------------------------------------------------

namespace MX {

class CHostResolver;

class CSockets : public CIpc
{
public:
  typedef enum {
    FamilyUnknown=0, FamilyIPv4=1, FamilyIPv6
  } eFamily;

  CSockets(_In_ CIoCompletionPortThreadPool &cDispatcherPool);
  ~CSockets();

  VOID SetOption_MaxAcceptsToPost(_In_ DWORD dwCount);
  VOID SetOption_MaxResolverTimeoutMs(_In_ DWORD dwTimeoutMs);

  HRESULT CreateListener(_In_ eFamily nFamily, _In_ int nPort, _In_ OnCreateCallback cCreateCallback,
                         _In_opt_z_ LPCSTR szBindAddressA=NULL, _In_opt_ CUserData *lpUserData=NULL,
                         _Out_opt_ HANDLE *h=NULL);
  HRESULT CreateListener(_In_ eFamily nFamily, _In_ int nPort, _In_ OnCreateCallback cCreateCallback,
                         _In_opt_z_ LPCWSTR szBindAddressW=NULL, _In_opt_ CUserData *lpUserData=NULL,
                         _Out_opt_ HANDLE *h=NULL);
  HRESULT ConnectToServer(_In_ eFamily nFamily, _In_z_ LPCSTR szAddressA, _In_ int nPort,
                          _In_ OnCreateCallback cCreateCallback, _In_opt_ CUserData *lpUserData=NULL,
                          _Out_opt_ HANDLE *h=NULL);
  HRESULT ConnectToServer(_In_ eFamily nFamily, _In_z_ LPCWSTR szAddressW, _In_ int nPort,
                          _In_ OnCreateCallback cCreateCallback, _In_opt_ CUserData *lpUserData=NULL,
                          _Out_opt_ HANDLE *h=NULL);

  HRESULT GetPeerAddress(_In_ HANDLE h, _Out_ PSOCKADDR_INET lpAddr);

  //--------

private:
  class CConnection : public CConnectionBase
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CConnection);
  public:
    CConnection(_In_ CIpc *lpIpc, _In_ CIpc::eConnectionClass nClass);
    ~CConnection();

    HRESULT CreateSocket(_In_ eFamily nFamily, _In_ DWORD dwPacketSize);
    VOID ShutdownLink(_In_ BOOL bAbortive);

    HRESULT SetupListener();
    HRESULT SetupClient();
    HRESULT SetupAcceptEx(_In_ CConnection *lpListenConn);

    HRESULT ResolveAddress(_In_ DWORD dwMaxResolverTimeoutMs, _In_ eFamily nFamily, _In_opt_z_ LPCSTR szAddressA,
                           _In_opt_ int nPort);

    HRESULT SendReadPacket(_In_ CPacket *lpPacket);
    HRESULT SendWritePacket(_In_ CPacket *lpPacket);

    VOID HostResolveCallback(_In_ CHostResolver *lpResolver);

    VOID ConnectWaiterThreadProc();

  protected:
    friend class CSockets;

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

  protected:
    SOCKADDR_INET sAddr;
    union {
      lpfnAcceptEx fnAcceptEx;
      lpfnConnectEx fnConnectEx;
    };
    lpfnGetAcceptExSockaddrs fnGetAcceptExSockaddrs;
    SOCKET sck;
    struct {
      LONG volatile nMutex;
      CHostResolver *lpResolver;
      CPacket *lpPacket;
    } sHostResolver;
    struct {
      LONG volatile nMutex;
      TClassWorkerThread<CConnection> *lpWorkerThread;
      CPacket *lpPacket;
    } sConnectWaiter;
    LONG volatile nReadThrottle;
  };

private:
  HRESULT OnInternalInitialize();
  VOID OnInternalFinalize();

  HRESULT CreateServerConnection(_In_ CConnection *lpListenConn);

  HRESULT OnPreprocessPacket(_In_ DWORD dwBytes, _In_ CPacket *lpPacket, _In_ HRESULT hRes);
  HRESULT OnCustomPacket(_In_ DWORD dwBytes, _In_ CPacket *lpPacket, _In_ HRESULT hRes);

  BOOL ZeroReadsSupported() const
    {
    return TRUE;
    };

private:
  DWORD dwMaxAcceptsToPost, dwMaxResolverTimeoutMs;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_SOCKETS_H
