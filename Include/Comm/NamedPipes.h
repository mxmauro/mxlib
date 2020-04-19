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
#ifndef _MX_NAMEDPIPES_H
#define _MX_NAMEDPIPES_H

#include "IpcCommon.h"

//-----------------------------------------------------------

namespace MX {

class CNamedPipes : public CIpc, public CNonCopyableObj
{
public:
  CNamedPipes(_In_ CIoCompletionPortThreadPool &cDispatcherPool);
  ~CNamedPipes();

  VOID SetOption_ConnectTimeout(_In_ DWORD dwTimeoutMs);

  HRESULT CreateListener(_In_z_ LPCSTR szServerNameA, _In_ OnCreateCallback cCreateCallback,
                         _In_opt_z_ LPCWSTR szSecutityDescriptorA=NULL);
  HRESULT CreateListener(_In_z_ LPCWSTR szServerNameW, _In_ OnCreateCallback cCreateCallback,
                         _In_opt_z_ LPCWSTR szSecutityDescriptorW=NULL);
  HRESULT ConnectToServer(_In_z_ LPCSTR szServerNameA, _In_ OnCreateCallback cCreateCallback,
                          _In_opt_ CUserData *lpUserData=NULL, _Out_opt_ HANDLE *h=NULL);
  HRESULT ConnectToServer(_In_z_ LPCWSTR szServerNameW, _In_ OnCreateCallback cCreateCallback,
                          _In_opt_ CUserData *lpUserData=NULL, _Out_opt_ HANDLE *h=NULL);
  HRESULT CreateRemoteClientConnection(_In_ HANDLE hProc, _Out_ HANDLE &h, _Out_ HANDLE &hRemotePipe,
                                       _In_ OnCreateCallback cCreateCallback, _In_opt_ CUserData *lpUserData=NULL);

  HRESULT ImpersonateConnectionClient(_In_ HANDLE h);

private:
  class CServerInfo : public virtual TRefCounted<CBaseMemObj>, public CNonCopyableObj
  {
  public:
    CServerInfo();
    ~CServerInfo();

    HRESULT Init(_In_z_ LPCWSTR szServerNameW, _In_ PSECURITY_DESCRIPTOR lpSecDescr);

  public:
    CStringW cStrNameW;
    PSECURITY_DESCRIPTOR lpSecDescr;
  };

  //----

private:
  class CConnection : public CConnectionBase, public CNonCopyableObj
  {
  public:
    CConnection(_In_ CIpc *lpIpc, _In_ CIpc::eConnectionClass nClass);
    ~CConnection();

    HRESULT CreateServer();
    HRESULT CreateClient(_In_z_ LPCWSTR szServerNameW, _In_ DWORD dwMaxWriteTimeoutMs,
                         _In_ PSECURITY_DESCRIPTOR lpSecDescr);
    VOID ShutdownLink(_In_ BOOL bAbortive);

    HRESULT SendReadPacket(_In_ CPacketBase *lpPacket, _Out_ LPDWORD lpdwRead);
    HRESULT SendWritePacket(_In_ CPacketBase *lpPacket, _Out_ LPDWORD lpdwWritten);
    SIZE_T GetMultiWriteMaxCount() const
      {
      return 1;
      };

  protected:
    friend class CNamedPipes;

    RWLOCK sRwHandleInUse;
    HANDLE hPipe;
    TAutoRefCounted<CServerInfo> cServerInfo;
  };

  //----

private:
  HRESULT OnInternalInitialize();
  VOID OnInternalFinalize();

  HRESULT CreateServerConnection(_In_ CServerInfo *lpServerInfo, _In_ OnCreateCallback cCreateCallback);

  HRESULT OnCustomPacket(_In_ DWORD dwBytes, _In_ CPacketBase *lpPacket, _In_ HRESULT hRes);

  BOOL ZeroReadsSupported() const
    {
    return FALSE;
    };

private:
  LONG volatile nRemoteConnCounter;
  DWORD dwConnectTimeoutMs;
  PSECURITY_DESCRIPTOR lpSecDescr;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_NAMEDPIPES_H
