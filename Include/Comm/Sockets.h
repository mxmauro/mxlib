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
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHostResolver;

class CSockets : public CIpc
{
public:
  typedef enum {
    FamilyIPv4=1, FamilyIPv6
  } eFamily;

  CSockets(_In_ CIoCompletionPortThreadPool &cDispatcherPool);
  ~CSockets();

  VOID SetOption_BackLogSize(_In_ DWORD dwSize);
  VOID SetOption_MaxAcceptsToPost(_In_ DWORD dwCount);
  VOID SetOption_AddressResolverTimeout(_In_ DWORD dwTimeoutMs);

  HRESULT CreateListener(_In_ eFamily nFamily, _In_ int nPort, _In_ OnCreateCallback cCreateCallback,
                         _In_opt_z_ LPCSTR szBindAddressA = NULL, _In_opt_ CUserData *lpUserData = NULL,
                         _Out_opt_ HANDLE *h = NULL);
  HRESULT CreateListener(_In_ eFamily nFamily, _In_ int nPort, _In_ OnCreateCallback cCreateCallback,
                         _In_opt_z_ LPCWSTR szBindAddressW = NULL, _In_opt_ CUserData *lpUserData = NULL,
                         _Out_opt_ HANDLE *h = NULL);
  HRESULT ConnectToServer(_In_ eFamily nFamily, _In_z_ LPCSTR szAddressA, _In_ int nPort,
                          _In_ OnCreateCallback cCreateCallback, _In_opt_ CUserData *lpUserData = NULL,
                          _Out_opt_ HANDLE *h = NULL);
  HRESULT ConnectToServer(_In_ eFamily nFamily, _In_z_ LPCWSTR szAddressW, _In_ int nPort,
                          _In_ OnCreateCallback cCreateCallback, _In_opt_ CUserData *lpUserData = NULL,
                          _Out_opt_ HANDLE *h = NULL);

  HRESULT GetLocalAddress(_In_ HANDLE h, _Out_ PSOCKADDR_INET lpAddr);
  HRESULT GetPeerAddress(_In_ HANDLE h, _Out_ PSOCKADDR_INET lpAddr);

  //--------

private:
  class CConnection : public CConnectionBase
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CConnection);
  public:
    CConnection(_In_ CIpc *lpIpc, _In_ CIpc::eConnectionClass nClass, _In_ eFamily nFamily);
    ~CConnection();

    HRESULT CreateSocket(_In_ DWORD dwPacketSize);
    VOID ShutdownLink(_In_ BOOL bAbortive);

    HRESULT SetupListener(_In_ int nBackLogSize);
    HRESULT SetupClient();
    HRESULT SetupAcceptEx(_In_ CConnection *lpListenConn);

    HRESULT ResolveAddress(_In_ DWORD dwResolverTimeoutMs, _In_opt_z_ LPCSTR szAddressA, _In_opt_ int nPort);

    HRESULT SendReadPacket(_In_ CPacket *lpPacket);
    HRESULT SendWritePacket(_In_ CPacket *lpPacket);

    VOID HostResolveCallback(_In_ CHostResolver *lpResolver, _In_ SOCKADDR_INET &sAddr, _In_ HRESULT hrErrorCode,
                             _In_opt_ LPVOID lpUserData);

    VOID ConnectWaiterThreadProc();

  protected:
    friend class CSockets;

    LONG volatile nRwHandleInUse;
    SOCKADDR_INET sAddr;
    SOCKET sck;
    eFamily nFamily;
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
  DWORD dwBackLogSize, dwMaxAcceptsToPost, dwAddressResolverTimeoutMs;
  struct tagReusableSockets {
    LONG volatile nMutex;
    TArrayList<SOCKET> aList;
  } sReusableSockets[2];
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_SOCKETS_H
