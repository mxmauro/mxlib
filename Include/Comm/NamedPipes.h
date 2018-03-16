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
#ifndef _MX_NAMEDPIPES_H
#define _MX_NAMEDPIPES_H

#include "IpcCommon.h"

//-----------------------------------------------------------

#define MX_NAMEDPIPES_PROPERTY_MaxWaitTimeoutMs         "NAMEDPIPES_MaxWaitTimeoutMs"
#define MX_NAMEDPIPES_PROPERTY_MaxWaitTimeoutMs_DEFVAL  1000

//-----------------------------------------------------------

namespace MX {

class CNamedPipes : public CIpc
{
  MX_DISABLE_COPY_CONSTRUCTOR(CNamedPipes);
public:
  CNamedPipes(_In_ CIoCompletionPortThreadPool &cDispatcherPool, _In_ CPropertyBag &cPropBag);
  ~CNamedPipes();

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
  class CServerInfo : public virtual TRefCounted<CBaseMemObj>
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CServerInfo);
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
  class CConnection : public CConnectionBase
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CConnection);
  public:
    CConnection(_In_ CIpc *lpIpc, _In_ CIpc::eConnectionClass nClass);
    ~CConnection();

    HRESULT CreateServer(_In_ DWORD dwPacketSize);
    HRESULT CreateClient(_In_z_ LPCWSTR szServerNameW, _In_ DWORD dwMaxWriteTimeoutMs,
                         _In_ PSECURITY_DESCRIPTOR lpSecDescr);
    VOID ShutdownLink(_In_ BOOL bAbortive);

    HRESULT SendReadPacket(_In_ CPacket *lpPacket);
    HRESULT SendWritePacket(_In_ CPacket *lpPacket);

  protected:
    friend class CNamedPipes;

    HANDLE hPipe;
    TAutoRefCounted<CServerInfo> cServerInfo;
  };

  //----

private:
  HRESULT OnInternalInitialize();
  VOID OnInternalFinalize();

  HRESULT CreateServerConnection(_In_ CServerInfo *lpServerInfo, _In_ OnCreateCallback cCreateCallback);

  HRESULT OnPreprocessPacket(_In_ DWORD dwBytes, _In_ CPacket *lpPacket, _In_ HRESULT hRes);
  HRESULT OnCustomPacket(_In_ DWORD dwBytes, _In_ CPacket *lpPacket, _In_ HRESULT hRes);

  BOOL ZeroReadsSupported() const
    {
    return FALSE;
    };

private:
  LONG volatile nRemoteConnCounter;
  DWORD dwMaxWaitTimeoutMs;
  PSECURITY_DESCRIPTOR lpSecDescr;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_NAMEDPIPES_H
