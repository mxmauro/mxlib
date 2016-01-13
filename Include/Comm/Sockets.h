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
#if (!defined(_WS2DEF_)) && (!defined(_WINSOCKAPI_))
  #include <WS2tcpip.h>
#endif

//-----------------------------------------------------------

#define MX_SOCKETS_PROPERTY_MaxAcceptsToPost             "SOCKETS_MaxAcceptsToPost"
#define MX_SOCKETS_PROPERTY_MaxAcceptsToPost_DEFVAL      4
#define MX_SOCKETS_PROPERTY_MaxResolverTimeoutMs         "SOCKETS_MaxResolverTimeoutMs"
#define MX_SOCKETS_PROPERTY_MaxResolverTimeoutMs_DEFVAL  3000

//-----------------------------------------------------------

namespace MX {

class CHostResolver;

class CSockets : public CIpc
{
public:
  typedef enum {
    FamilyUnknown=0, FamilyIPv4=1, FamilyIPv6
  } eFamily;

  CSockets(__in CIoCompletionPortThreadPool &cDispatcherPool, __in CPropertyBag &cPropBag);
  ~CSockets();

  HRESULT CreateListener(__in eFamily nFamily, __in int nPort, __in OnCreateCallback cCreateCallback,
                         __in_z_opt LPCSTR szBindAddressA=NULL, __in_opt CUserData *lpUserData=NULL,
                         __out_opt HANDLE *h=NULL);
  HRESULT CreateListener(__in eFamily nFamily, __in int nPort, __in OnCreateCallback cCreateCallback,
                         __in_z_opt LPCWSTR szBindAddressW=NULL, __in_opt CUserData *lpUserData=NULL,
                         __out_opt HANDLE *h=NULL);
  HRESULT ConnectToServer(__in eFamily nFamily, __in_z LPCSTR szAddressA, __in int nPort,
                          __in OnCreateCallback cCreateCallback, __in_opt CUserData *lpUserData=NULL,
                          __out_opt HANDLE *h=NULL);
  HRESULT ConnectToServer(__in eFamily nFamily, __in_z LPCWSTR szAddressW, __in int nPort,
                          __in OnCreateCallback cCreateCallback, __in_opt CUserData *lpUserData=NULL,
                          __out_opt HANDLE *h=NULL);

  HRESULT GetPeerAddress(__in HANDLE h, __out sockaddr *lpAddr);

  //--------

private:
  class CConnection : public CConnectionBase
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CConnection);
  public:
    CConnection(__in CIpc *lpIpc, __in CIpc::eConnectionClass nClass);
    ~CConnection();

    HRESULT CreateSocket(__in eFamily nFamily, __in DWORD dwPacketSize);
    VOID ShutdownLink(__in BOOL bAbortive);

    HRESULT SetupListener();
    HRESULT SetupClient();
    HRESULT SetupAcceptEx(__in CConnection *lpListenConn);

    HRESULT ResolveAddress(__in DWORD dwMaxResolverTimeoutMs, __in eFamily nFamily, __in_z LPCSTR szAddressA,
                           __in int nPort);

    HRESULT SendReadPacket(__in CPacket *lpPacket);
    HRESULT SendWritePacket(__in CPacket *lpPacket);

    VOID HostResolveCallback(__in CHostResolver *lpResolver);

    VOID ConnectWaiterThreadProc();

  protected:
    friend class CSockets;

    typedef BOOL (WINAPI *lpfnAcceptEx)(__in SOCKET sListenSocket, __in SOCKET sAcceptSocket, __in PVOID lpOutputBuffer,
                                        __in DWORD dwReceiveDataLength, __in DWORD dwLocalAddressLength,
                                        __in DWORD dwRemoteAddressLength, __out LPDWORD lpdwBytesReceived,
                                        __inout LPOVERLAPPED lpOverlapped);
    typedef BOOL (WINAPI *lpfnConnectEx)(__in SOCKET s, __in const struct sockaddr FAR *name, __in int namelen,
                                         __in_opt PVOID lpSendBuffer, __in DWORD dwSendDataLength,
                                         __out LPDWORD lpdwBytesSent, __inout LPOVERLAPPED lpOverlapped);
    typedef VOID (WINAPI *lpfnGetAcceptExSockaddrs)(__in PVOID lpOutputBuffer, __in DWORD dwReceiveDataLength,
                                                    __in DWORD dwLocalAddressLength, __in DWORD dwRemoteAddressLength,
                                                    __deref_out struct sockaddr **LocalSockaddr,
                                                    __out LPINT LocalSockaddrLength,
                                                    __deref_out struct sockaddr **RemoteSockaddr,
                                                    __out LPINT RemoteSockaddrLength);

  protected:
    sockaddr sAddr;
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

  HRESULT CreateServerConnection(__in CConnection *lpListenConn);

  HRESULT OnPreprocessPacket(__in DWORD dwBytes, __in CPacket *lpPacket, __in HRESULT hRes);
  HRESULT OnCustomPacket(__in DWORD dwBytes, __in CPacket *lpPacket, __in HRESULT hRes);

  BOOL ZeroReadsSupported() const
    {
    return TRUE;
    };

private:
  LONG volatile nSlimMutex;
  DWORD dwMaxAcceptsToPost, dwMaxResolverTimeoutMs;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_SOCKETS_H
