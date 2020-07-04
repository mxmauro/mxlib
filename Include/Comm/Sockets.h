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
#ifndef _MX_SOCKETS_H
#define _MX_SOCKETS_H

#include "IpcCommon.h"
#include "HostResolver.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHostResolver;

class CSockets : public CIpc, public CNonCopyableObj
{
public:
  typedef enum {
    FamilyIPv4=1, FamilyIPv6
  } eFamily;

  class CListenerOptions : public virtual CBaseMemObj
  {
  public:
    CListenerOptions()
      {
      dwBackLogSize = 0;
      dwMaxAcceptsToPost = 4;
      dwMaxRequestsPerSecond = 0;
      dwMaxRequestsBurstSize = 0;
      return;
      };

  public:
    DWORD dwBackLogSize;
    DWORD dwMaxAcceptsToPost;
    DWORD dwMaxRequestsPerSecond;
    DWORD dwMaxRequestsBurstSize;
  };

public:
  CSockets(_In_ CIoCompletionPortThreadPool &cDispatcherPool);
  ~CSockets();

  VOID SetOption_AddressResolverTimeout(_In_ DWORD dwTimeoutMs);

  HRESULT CreateListener(_In_ eFamily nFamily, _In_ int nPort, _In_ OnCreateCallback cCreateCallback,
                         _In_opt_z_ LPCSTR szBindAddressA = NULL, _In_opt_ CUserData *lpUserData = NULL,
                         _In_opt_ CListenerOptions *lpOptions = NULL, _Out_opt_ HANDLE *h = NULL);
  HRESULT CreateListener(_In_ eFamily nFamily, _In_ int nPort, _In_ OnCreateCallback cCreateCallback,
                         _In_opt_z_ LPCWSTR szBindAddressW = NULL, _In_opt_ CUserData *lpUserData = NULL,
                         _In_opt_ CListenerOptions *lpOptions = NULL, _Out_opt_ HANDLE *h = NULL);
  HRESULT ConnectToServer(_In_ eFamily nFamily, _In_z_ LPCSTR szAddressA, _In_ int nPort,
                          _In_ OnCreateCallback cCreateCallback, _In_opt_ CUserData *lpUserData = NULL,
                          _Out_opt_ HANDLE *h = NULL);
  HRESULT ConnectToServer(_In_ eFamily nFamily, _In_z_ LPCWSTR szAddressW, _In_ int nPort,
                          _In_ OnCreateCallback cCreateCallback, _In_opt_ CUserData *lpUserData = NULL,
                          _Out_opt_ HANDLE *h = NULL);

  HRESULT GetLocalAddress(_In_ HANDLE h, _Out_ PSOCKADDR_INET lpAddr);
  HRESULT GetPeerAddress(_In_ HANDLE h, _Out_ PSOCKADDR_INET lpAddr);

  HRESULT Disable(_In_ HANDLE h, _In_ BOOL bReceive, _In_ BOOL bSend);

  //--------

private:
  friend class CConnection;

  class CConnection : public CConnectionBase, public CNonCopyableObj
  {
  public:
    CConnection(_In_ CIpc *lpIpc, _In_ CIpc::eConnectionClass nClass, _In_ eFamily nFamily);
    ~CConnection();

    HRESULT CreateSocket();
    VOID ShutdownLink(_In_ BOOL bAbortive);

    HRESULT SetupListener();
    HRESULT SetupClient();
    HRESULT SetupAcceptEx(_In_ CConnection *lpIncomingConn);

    HRESULT ResolveAddress(_In_ DWORD dwResolverTimeoutMs, _In_opt_z_ LPCSTR szAddressA, _In_opt_ int nPort);

    HRESULT SendReadPacket(_In_ CPacketBase *lpPacket, _Out_ LPDWORD lpdwRead);
    HRESULT SendWritePacket(_In_ CPacketBase *lpPacket, _Out_ LPDWORD lpdwWritten);
    SIZE_T GetMultiWriteMaxCount() const
      {
      return 4;
      };

    VOID HostResolveCallback(_In_ LONG nResolverId, _In_ PSOCKADDR_INET lpSockAddr, _In_ HRESULT hrErrorCode,
                             _In_opt_ LPVOID lpUserData);

  protected:
    friend class CSockets;
    friend class CConnectWaiter;
    friend class CListener;

    class CConnectWaiter : public CBaseMemObj
    {
    public:
      CConnectWaiter(_In_ CConnection *lpConn);
      ~CConnectWaiter();

      HRESULT Start(_In_ CPacketBase *lpPacket);
      VOID Stop();

    private:
      VOID ThreadProc();

    private:
      CConnection *lpConn;
      TClassWorkerThread<CConnectWaiter> *lpWorkerThread;
      CPacketBase *lpPacket;
    };

  protected:
    class CListener : public CBaseMemObj
    {
    public:
      CListener(_In_ CConnection *lpConn);
      ~CListener();

      VOID SetOptions(_In_opt_ CListenerOptions *lpOptions);

      HRESULT Start();
      VOID Stop();

      BOOL CheckRateLimit();

    private:
      VOID ThreadProc();

    public:
      CConnection *lpConn;
      CListenerOptions cOptions;
      TClassWorkerThread<CListener> *lpWorkerThread;
      HANDLE hAcceptSelect, hAcceptCompleted;
      LPVOID fnAcceptEx, fnGetAcceptExSockaddrs;
      LONG volatile nAcceptsInProgress;
      struct {
        LONG volatile nMutex;
        CTimer cTimer;
        union {
          DWORD dwRequestCounter;
          DWORD dwCurrentExcess;
        };
      } sLimiter;
    };

  protected:
    RWLOCK sRwHandleInUse;
    SOCKADDR_INET sAddr;
    SOCKET sck;
    eFamily nFamily;
    struct {
      LONG volatile nMutex;
      LONG volatile nResolverId;
      CPacketBase *lpPacket;
    } sHostResolver;
    TAutoDeletePtr<CConnectWaiter> cConnectWaiter;
    TAutoDeletePtr<CListener> cListener;
    LONG volatile nReadThrottle;
    struct {
      DWORD dwCurrentSize;
      DWORD dwLastTickMs;
    } sAutoAdjustSndBuf;
  };

private:
  HRESULT OnInternalInitialize();
  VOID OnInternalFinalize();

  HRESULT CreateServerConnection(_In_ CConnection *lpListenConn);

  BOOL OnPreprocessPacket(_In_ DWORD dwBytes, _In_ CPacketBase *lpPacket, _In_ HRESULT hRes);
  HRESULT OnCustomPacket(_In_ DWORD dwBytes, _In_ CPacketBase *lpPacket, _In_ HRESULT hRes);

  BOOL ZeroReadsSupported() const
    {
    return TRUE;
    };

private:
  DWORD dwAddressResolverTimeoutMs;
  struct tagReusableSockets {
    LONG volatile nMutex;
    TArrayList<SOCKET> aList;
  } sReusableSockets[2];
  struct tagDisconnectingSockets {
    LONG volatile nMutex;
    TArrayList<SOCKET> aList;
  } sDisconnectingSockets[2];
  ULONG nFlags;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_SOCKETS_H
