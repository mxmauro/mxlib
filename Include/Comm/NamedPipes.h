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

namespace MX {

class CNamedPipes : public CIpc
{
  MX_DISABLE_COPY_CONSTRUCTOR(CNamedPipes);
public:
  CNamedPipes(__in CIoCompletionPortThreadPool &cDispatcherPool, __in CPropertyBag &cPropBag);
  ~CNamedPipes();

  HRESULT CreateListener(__in_z LPCSTR szServerNameA, __in OnCreateCallback cCreateCallback,
                         __in_z_opt LPCWSTR szSecutityDescriptorA=NULL);
  HRESULT CreateListener(__in_z LPCWSTR szServerNameW, __in OnCreateCallback cCreateCallback,
                         __in_z_opt LPCWSTR szSecutityDescriptorW=NULL);
  HRESULT ConnectToServer(__in_z LPCSTR szServerNameA, __in OnCreateCallback cCreateCallback,
                          __in_opt CUserData *lpUserData=NULL, __out_opt HANDLE *h=NULL);
  HRESULT ConnectToServer(__in_z LPCWSTR szServerNameW, __in OnCreateCallback cCreateCallback,
                          __in_opt CUserData *lpUserData=NULL, __out_opt HANDLE *h=NULL);
  HRESULT CreateRemoteClientConnection(__in HANDLE hProc, __out HANDLE &h, __out HANDLE &hRemotePipe,
                                       __in OnCreateCallback cCreateCallback, __in_opt CUserData *lpUserData=NULL);

  HRESULT ImpersonateConnectionClient(__in HANDLE h);

private:
  class CServerInfo : public virtual CBaseMemObj, public TRefCounted<CServerInfo>
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CServerInfo);
  public:
    CServerInfo();
    ~CServerInfo();

    HRESULT Init(__in_z LPCWSTR szServerNameW, __in_z LPCWSTR szSecutityDescriptorW);

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
    CConnection(__in CIpc *lpIpc, __in CIpc::eConnectionClass nClass);
    ~CConnection();

    HRESULT CreateServer(__in DWORD dwPacketSize);
    HRESULT CreateClient(__in_z LPCWSTR szServerNameW);
    VOID ShutdownLink(__in BOOL bAbortive);

    HRESULT SendReadPacket(__in CPacket *lpPacket);
    HRESULT SendWritePacket(__in CPacket *lpPacket);

  protected:
    friend class CNamedPipes;

    HANDLE hPipe;
    TAutoRefCounted<CServerInfo> cServerInfo;
  };

  //----

private:
  HRESULT OnInternalInitialize();
  VOID OnInternalFinalize();

  HRESULT CreateServerConnection(__in CServerInfo *lpServerInfo, __in OnCreateCallback cCreateCallback);

  HRESULT OnPreprocessPacket(__in DWORD dwBytes, __in CPacket *lpPacket, __in HRESULT hRes);
  HRESULT OnCustomPacket(__in DWORD dwBytes, __in CPacket *lpPacket, __in HRESULT hRes);

private:
  LONG volatile nRemoteConnCounter;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_NAMEDPIPES_H
