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
#include "..\..\Include\Comm\NamedPipes.h"
#include "..\..\Include\FnvHash.h"
#include "..\..\Include\AutoHandle.h"
#include <Sddl.h>

//-----------------------------------------------------------

#define CLIENTCONNECT_RETRY_COUNT                         10
#define CLIENTCONNECT_RETRY_DELAY                        100

#define TypeListenRequest (CPacket::eType)((int)CPacket::TypeMAX + 1)
#define TypeListen        (CPacket::eType)((int)CPacket::TypeMAX + 2)

//-----------------------------------------------------------

static VOID GenerateUniquePipeName(__out LPWSTR szNameW, __in SIZE_T nNameSize, __in DWORD dwVal1, __in DWORD dwVal2);

//-----------------------------------------------------------

namespace MX {

CNamedPipes::CNamedPipes(__in CIoCompletionPortThreadPool &cDispatcherPool, __in CPropertyBag &cPropBag) :
             CIpc(cDispatcherPool, cPropBag)
{
  _InterlockedExchange(&nRemoteConnCounter, 0);
  return;
}

CNamedPipes::~CNamedPipes()
{
  Finalize();
  return;
}

HRESULT CNamedPipes::CreateListener(__in_z LPCSTR szServerNameA, __in OnCreateCallback cCreateCallback,
                                    __in_z_opt LPCWSTR szSecutityDescriptorA)
{
  CStringW cStrTempW[2];

  if (szServerNameA == NULL)
    return E_POINTER;
  if (cStrTempW[0].Copy(szServerNameA) == FALSE)
    return E_OUTOFMEMORY;
  if (szSecutityDescriptorA != NULL)
  {
    if (cStrTempW[1].Copy(szSecutityDescriptorA) == FALSE)
      return E_OUTOFMEMORY;
  }
  return CreateListener((LPWSTR)cStrTempW[0], cCreateCallback, (LPWSTR)cStrTempW[1]);
}

HRESULT CNamedPipes::CreateListener(__in_z LPCWSTR szServerNameW, __in OnCreateCallback cCreateCallback,
                                    __in_z_opt LPCWSTR szSecutityDescriptorW)
{
  CAutoSlimRWLShared cLock(&nSlimMutex);
  TAutoRefCounted<CServerInfo> cServerInfo;
  HRESULT hRes;

  if ((!cEngineErrorCallback) || IsShuttingDown() != FALSE)
    return E_FAIL;
  if (szServerNameW == NULL)
    return E_POINTER;
  if (*szServerNameW == 0 || (!cCreateCallback))
    return E_INVALIDARG;
  //initialize server info
  cServerInfo.Attach(MX_DEBUG_NEW CServerInfo());
  if (!cServerInfo)
    return E_OUTOFMEMORY;
  hRes = cServerInfo->Init(szServerNameW, szSecutityDescriptorW);
  if (FAILED(hRes))
    return hRes;
  return CreateServerConnection((CServerInfo*)cServerInfo, cCreateCallback);
}

HRESULT CNamedPipes::ConnectToServer(__in_z LPCSTR szServerNameA, __in OnCreateCallback cCreateCallback,
                                     __in_opt CUserData *lpUserData, __out_opt HANDLE *h)
{
  CStringW cStrTempW;

  if (szServerNameA == NULL)
    return E_POINTER;
  if (cStrTempW.Copy(szServerNameA) == FALSE)
    return E_OUTOFMEMORY;
  return ConnectToServer((LPWSTR)cStrTempW, cCreateCallback, lpUserData, h);
}

HRESULT CNamedPipes::ConnectToServer(__in_z LPCWSTR szServerNameW, __in OnCreateCallback cCreateCallback,
                                     __in_opt CUserData *lpUserData, __out_opt HANDLE *h)
{
  CAutoSlimRWLShared cLock(&nSlimMutex);
  TAutoDeletePtr<CConnection> cConn;
  CStringW cStrTempW;
  HRESULT hRes;

  if (h != NULL)
    *h = NULL;
  if ((!cEngineErrorCallback) || IsShuttingDown() != FALSE)
    return E_FAIL;
  if (szServerNameW == NULL)
    return E_POINTER;
  if (*szServerNameW == 0 || (!cCreateCallback))
    return E_INVALIDARG;
  if (cStrTempW.Format(L"\\\\.\\pipe\\%s", szServerNameW) == FALSE)
    return E_OUTOFMEMORY;
  //create connection
  cConn.Attach(MX_DEBUG_NEW CConnection(this, CIpc::ConnectionClassClient));
  if (!cConn)
    return E_OUTOFMEMORY;
  if (h != NULL)
    *h = reinterpret_cast<HANDLE>(cConn.Get());
  //setup connection
  cConn->cCreateCallback = cCreateCallback;
  cConn->cUserData = lpUserData;
  {
    CAutoSlimRWLExclusive cConnListLock(&(sConnections.nSlimMutex));

    sConnections.cList.PushTail(cConn.Get());
  }
  hRes = FireOnCreate(cConn.Get());
  if (SUCCEEDED(hRes))
    hRes = cConn->CreateClient((LPWSTR)cStrTempW);
  if (SUCCEEDED(hRes))
    hRes = cConn->HandleConnected();
  if (FAILED(hRes))
    FireOnConnect(cConn.Get(), hRes);
  //done
  ReleaseAndRemoveConnectionIfClosed(cConn.Detach());
  if (FAILED(hRes) && h != NULL)
    *h = NULL;
  return hRes;
}

HRESULT CNamedPipes::CreateRemoteClientConnection(__in HANDLE hProc, __out HANDLE &h, __out HANDLE &hRemotePipe,
                                                  __in OnCreateCallback cCreateCallback, __in_opt CUserData *lpUserData)
{
  CAutoSlimRWLShared cLock(&nSlimMutex);
  TAutoDeletePtr<CConnection> cConn;
  SECURITY_ATTRIBUTES sSecAttrib;
  SECURITY_DESCRIPTOR sSecDesc;
  OVERLAPPED sConnOvr;
  CWindowsEvent cConnEv;
  BOOL bConnected;
  WCHAR szBufW[128];
  CWindowsHandle cLocalPipe, cRemotePipe;
  DWORD dw;
  HRESULT hRes;

  h = NULL;
  hRemotePipe = NULL;
  if ((!cEngineErrorCallback) == NULL || IsShuttingDown() != FALSE)
    return E_FAIL;
  if (!cCreateCallback)
    return E_INVALIDARG;
  //create named pipe
  ::InitializeSecurityDescriptor(&sSecDesc, SECURITY_DESCRIPTOR_REVISION);
  ::SetSecurityDescriptorDacl(&sSecDesc, TRUE, NULL, FALSE);
  sSecAttrib.nLength = sizeof(SECURITY_ATTRIBUTES);
  sSecAttrib.bInheritHandle = FALSE;
  sSecAttrib.lpSecurityDescriptor = &sSecDesc;
  GenerateUniquePipeName(szBufW, MX_ARRAYLEN(szBufW), (DWORD)this, (DWORD)_InterlockedIncrement(&nRemoteConnCounter));
  cLocalPipe.Attach(::CreateNamedPipeW(szBufW, PIPE_ACCESS_DUPLEX|FILE_FLAG_WRITE_THROUGH|FILE_FLAG_OVERLAPPED,
                                       PIPE_READMODE_MESSAGE|PIPE_TYPE_MESSAGE|PIPE_WAIT, 1,
                                       dwPacketSize, dwPacketSize, 10000, &sSecAttrib));
  if (cLocalPipe == NULL || cLocalPipe == INVALID_HANDLE_VALUE)
    return MX_HRESULT_FROM_LASTERROR();
  //initialize for connection wait
  if (cConnEv.Create(TRUE, FALSE) == FALSE)
    return E_OUTOFMEMORY;
  MemSet(&sConnOvr, 0, sizeof(sConnOvr));
  sConnOvr.hEvent = cConnEv;
  bConnected = ::ConnectNamedPipe(cLocalPipe, &sConnOvr);
  //create other party pipe
  cRemotePipe.Attach(::CreateFileW(szBufW, GENERIC_READ|GENERIC_WRITE, 0, &sSecAttrib, OPEN_EXISTING,
                                   FILE_FLAG_WRITE_THROUGH|FILE_FLAG_OVERLAPPED, NULL));
  if (cRemotePipe == NULL || cRemotePipe == INVALID_HANDLE_VALUE)
    return MX_HRESULT_FROM_LASTERROR();
  //now wait until connected (may be not necessary)
  if (bConnected == FALSE)
  {
    if (::WaitForSingleObject(sConnOvr.hEvent, 1000) != WAIT_OBJECT_0)
      return MX_E_BrokenPipe;
    if (::GetOverlappedResult(cLocalPipe, &sConnOvr, &dw, FALSE) == FALSE)
    {
      hRes = MX_HRESULT_FROM_LASTERROR();
      if (hRes != MX_HRESULT_FROM_WIN32(ERROR_PIPE_CONNECTED))
        return hRes;
    }
  }
  //pipe connected; change to message-read mode
  dw = PIPE_READMODE_MESSAGE;
  if (::SetNamedPipeHandleState(cRemotePipe, &dw, NULL, NULL) == FALSE)
    return MX_HRESULT_FROM_LASTERROR();
  //duplicate handle on target process
  if (::DuplicateHandle(::GetCurrentProcess(), cRemotePipe.Get(), hProc, &hRemotePipe, 0, FALSE,
                        DUPLICATE_SAME_ACCESS) == FALSE)
    return MX_HRESULT_FROM_LASTERROR();
  //associate the local pipe with a connection and the given (optional) custom data
  cConn.Attach(MX_DEBUG_NEW CConnection(this, CIpc::ConnectionClassServer));
  if (!cConn)
  {
    if (hRemotePipe != NULL)
    {
      CWindowsRemoteHandle::CloseRemoteHandleSEH(hProc, hRemotePipe);
      hRemotePipe = NULL;
    }
    return E_OUTOFMEMORY;
  }
  //setup connection
  cConn->hPipe = cLocalPipe.Detach();
  cConn->cCreateCallback = cCreateCallback;
  cConn->cUserData = lpUserData;
  {
    CAutoSlimRWLExclusive cConnListLock(&(sConnections.nSlimMutex));

    sConnections.cList.PushTail(cConn.Get());
  }
  hRes = FireOnCreate(cConn.Get());
  if (SUCCEEDED(hRes))
    hRes = cDispatcherPool.Attach(cConn->hPipe, cDispatcherPoolPacketCallback);
  if (SUCCEEDED(hRes))
    hRes = cConn->HandleConnected();
  if (FAILED(hRes))
    FireOnConnect(cConn.Get(), hRes);
  //done
  if (FAILED(hRes) && hRemotePipe != NULL)
  {
    CWindowsRemoteHandle::CloseRemoteHandleSEH(hProc, hRemotePipe);
    hRemotePipe = NULL;
  }
  ReleaseAndRemoveConnectionIfClosed(cConn.Detach());
  return hRes;
}

HRESULT CNamedPipes::ImpersonateConnectionClient(__in HANDLE h)
{
  CAutoSlimRWLShared cLock(&nSlimMutex);
  CConnection *lpConn;
  HRESULT hRes;

  if (!cEngineErrorCallback)
    return E_FAIL;
  if (h == NULL)
    return E_POINTER;
  lpConn = reinterpret_cast<CConnection*>(CheckAndGetConnection(h));
  if (lpConn == NULL)
    return E_INVALIDARG;
  //do impersonation
  if (lpConn->hPipe != NULL)
    hRes = (::ImpersonateNamedPipeClient(lpConn->hPipe) != FALSE) ? S_OK : MX_HRESULT_FROM_LASTERROR();
  else
    hRes = E_FAIL;
  //done
  ReleaseAndRemoveConnectionIfClosed(lpConn);
  return hRes;
}

//-----------------------------------------------------------

HRESULT CNamedPipes::OnInternalInitialize()
{
  return S_OK;
}

VOID CNamedPipes::OnInternalFinalize()
{
  _InterlockedExchange(&nRemoteConnCounter, 0);
  return;
}

HRESULT CNamedPipes::CreateServerConnection(__in CServerInfo *lpServerInfo, __in OnCreateCallback _cCreateCallback)
{
  TAutoDeletePtr<CConnection> cConn;
  HRESULT hRes;

  //create connection
  cConn.Attach(MX_DEBUG_NEW CConnection(this, CIpc::ConnectionClassServer));
  if (!cConn)
    return E_OUTOFMEMORY;
  //setup connection
  cConn->cCreateCallback = _cCreateCallback;
  cConn->cServerInfo = lpServerInfo;
  {
    CAutoSlimRWLExclusive cConnListLock(&(sConnections.nSlimMutex));

    sConnections.cList.PushTail(cConn.Get());
  }
  hRes = FireOnCreate(cConn.Get());
  if (SUCCEEDED(hRes))
    hRes = cConn->CreateServer(dwPacketSize);
  //done
  ReleaseAndRemoveConnectionIfClosed(cConn.Detach());
  return hRes;
}

HRESULT CNamedPipes::OnPreprocessPacket(__in DWORD dwBytes, __in CPacket *lpPacket, __in HRESULT hRes)
{
  return S_FALSE;
}

HRESULT CNamedPipes::OnCustomPacket(__in DWORD dwBytes, __in CPacket *lpPacket, __in HRESULT hRes)
{
  CConnection *lpConn;

  lpConn = (CConnection*)(lpPacket->GetConn());
  switch (lpPacket->GetType())
  {
    case TypeListenRequest:
      if (SUCCEEDED(hRes))
      {
        lpPacket->SetType(TypeListen);
        if (::ConnectNamedPipe(lpConn->hPipe, lpPacket->GetOverlapped()) == FALSE)
        {
          hRes = MX_HRESULT_FROM_LASTERROR();
          if (hRes == MX_E_IoPending)
            hRes = S_OK;
        }
        if (SUCCEEDED(hRes))
        {
          _InterlockedIncrement(&(lpConn->nRefCount));
        }
        else
        {
          lpConn->cRwList.Remove(lpPacket);
          FreePacket(lpPacket);
        }
      }
      else
      {
        lpConn->cRwList.Remove(lpPacket);
        FreePacket(lpPacket);
      }
      break;

    case TypeListen:
      if (SUCCEEDED(hRes))
        hRes = lpConn->HandleConnected();
      //free packet
      lpConn->cRwList.Remove(lpPacket);
      FreePacket(lpPacket);
      //process connection
      if (lpConn->nClass == CIpc::ConnectionClassServer)
      {
        //on server mode, create a new listener
        HRESULT hRes2 = CreateServerConnection(lpConn->cServerInfo, lpConn->cCreateCallback);
        if (FAILED(hRes2))
          FireOnEngineError(hRes2);
      }
      break;

    default:
      MX_ASSERT(FALSE);
      hRes = MX_E_InvalidData;
      break;
  }
  //done
  return hRes;
}

//-----------------------------------------------------------

CNamedPipes::CServerInfo::CServerInfo() : CBaseMemObj(), TRefCounted<CNamedPipes::CServerInfo>()
{
  lpSecDescr = NULL;
  return;
}

CNamedPipes::CServerInfo::~CServerInfo()
{
  if (lpSecDescr != NULL)
    ::LocalFree(lpSecDescr);
  return;
}

HRESULT CNamedPipes::CServerInfo::Init(__in_z LPCWSTR szServerNameW, __in_z LPCWSTR szSecutityDescriptorW)
{
  if (cStrNameW.Format(L"\\\\.\\pipe\\%s", szServerNameW) == FALSE)
    return E_OUTOFMEMORY;
  if (szSecutityDescriptorW != NULL && szSecutityDescriptorW[0] != 0 &&
      ::ConvertStringSecurityDescriptorToSecurityDescriptorW(szSecutityDescriptorW, SECURITY_DESCRIPTOR_REVISION,
                                                             &lpSecDescr, NULL) == FALSE)
    return MX_HRESULT_FROM_LASTERROR();
  return S_OK;
}

//-----------------------------------------------------------

CNamedPipes::CConnection::CConnection(__in CIpc *lpIpc, __in CIpc::eConnectionClass nClass) : CConnectionBase(lpIpc,
                                                                                                              nClass)
{
  hPipe = NULL;
  return;
}

CNamedPipes::CConnection::~CConnection()
{
  if (hPipe != NULL)
  {
    ::DisconnectNamedPipe(hPipe);
    ::CloseHandle(hPipe);
  }
  return;
}

HRESULT CNamedPipes::CConnection::CreateServer(__in DWORD dwPacketSize)
{
  SECURITY_ATTRIBUTES sSecAttrib;
  SECURITY_DESCRIPTOR sSecDesc;
  CPacket *lpPacket;
  HRESULT hRes;

  //create server pipe endpoint
  sSecAttrib.nLength = (DWORD)sizeof(sSecAttrib);
  sSecAttrib.bInheritHandle = FALSE;
  if (cServerInfo->lpSecDescr != NULL)
  {
    sSecAttrib.lpSecurityDescriptor = cServerInfo->lpSecDescr;
  }
  else
  {
    ::InitializeSecurityDescriptor(&sSecDesc, SECURITY_DESCRIPTOR_REVISION);
    ::SetSecurityDescriptorDacl(&sSecDesc, TRUE, NULL, FALSE);
    sSecAttrib.lpSecurityDescriptor = &sSecDesc;
  }
  hPipe = ::CreateNamedPipeW((LPWSTR)(cServerInfo->cStrNameW), PIPE_ACCESS_DUPLEX|FILE_FLAG_WRITE_THROUGH|
                             FILE_FLAG_OVERLAPPED, PIPE_READMODE_MESSAGE|PIPE_TYPE_MESSAGE|PIPE_WAIT,
                             PIPE_UNLIMITED_INSTANCES, dwPacketSize, dwPacketSize, 10000, &sSecAttrib);
  if (hPipe == NULL || hPipe == INVALID_HANDLE_VALUE)
  {
    hPipe = NULL;
    return MX_HRESULT_FROM_LASTERROR();
  }
  ::SetHandleInformation(hPipe, HANDLE_FLAG_INHERIT, 0);
  //attach to completion port
  hRes = GetDispatcherPool().Attach(hPipe, GetDispatcherPoolPacketCallback());
  if (FAILED(hRes))
    return hRes;
  //wait for connection
  lpPacket = GetPacket(TypeListenRequest);
  if (lpPacket == NULL)
    return E_OUTOFMEMORY;
  cRwList.QueueLast(lpPacket);
  _InterlockedIncrement(&nRefCount);
  hRes = GetDispatcherPool().Post(GetDispatcherPoolPacketCallback(), 0, lpPacket->GetOverlapped());
  if (FAILED(hRes))
  {
    cRwList.Remove(lpPacket);
    FreePacket(lpPacket);
    _InterlockedDecrement(&nRefCount);
  }
  //done
  return hRes;
}

HRESULT CNamedPipes::CConnection::CreateClient(__in_z LPCWSTR szServerNameW)
{
  SECURITY_ATTRIBUTES sSecAttrib;
  SECURITY_DESCRIPTOR sSecDesc;
  HRESULT hRes;
  DWORD dwMode, dwRetry;

  //create client pipe endpoint
  ::InitializeSecurityDescriptor(&sSecDesc, SECURITY_DESCRIPTOR_REVISION);
  ::SetSecurityDescriptorDacl(&sSecDesc, TRUE, NULL, FALSE);
  sSecAttrib.nLength = (DWORD)sizeof(sSecAttrib);
  sSecAttrib.bInheritHandle = FALSE;
  sSecAttrib.lpSecurityDescriptor = &sSecDesc;
  dwRetry = CLIENTCONNECT_RETRY_COUNT;
  while (1)
  {
    hPipe = ::CreateFileW(szServerNameW, GENERIC_READ|GENERIC_WRITE, 0, &sSecAttrib, OPEN_EXISTING,
                          FILE_FLAG_WRITE_THROUGH|FILE_FLAG_OVERLAPPED, NULL);
    if (hPipe != NULL && hPipe != INVALID_HANDLE_VALUE)
      break;
    hPipe = NULL;
    hRes = MX_HRESULT_FROM_LASTERROR();
    if ((--dwRetry) == 0 || hRes != MX_HRESULT_FROM_WIN32(ERROR_PIPE_BUSY))
      return hRes;
    ::WaitNamedPipeW(szServerNameW, CLIENTCONNECT_RETRY_DELAY);
  }
  //attach to completion port
  hRes = GetDispatcherPool().Attach(hPipe, GetDispatcherPoolPacketCallback());
  if (FAILED(hRes))
    return hRes;
  //pipe connected; change to message-read mode
  dwMode = PIPE_READMODE_MESSAGE;
  if (::SetNamedPipeHandleState(hPipe, &dwMode, NULL, NULL) == FALSE)
    return MX_HRESULT_FROM_LASTERROR();
  //done
  return S_OK;
}

VOID CNamedPipes::CConnection::ShutdownLink(__in BOOL bAbortive)
{
  {
    CFastLock cLock(&nMutex);

    if (hPipe != NULL)
    {
      ::DisconnectNamedPipe(hPipe);
      ::CloseHandle(hPipe);
      hPipe = NULL;
    }
  }
  //call base
  CConnectionBase::ShutdownLink(bAbortive);
  return;
}

HRESULT CNamedPipes::CConnection::SendReadPacket(__in CPacket *lpPacket)
{
  DWORD dwReaded;
  HRESULT hRes;

  if (::ReadFile(hPipe, lpPacket->GetBuffer(), lpPacket->GetBytesInUse(), &dwReaded,
                 lpPacket->GetOverlapped()) == FALSE)
  {
    hRes = MX_HRESULT_FROM_LASTERROR();
    return (hRes == MX_E_BrokenPipe || hRes == HRESULT_FROM_WIN32(ERROR_NO_DATA) ||
            hRes == MX_E_PipeNotConnected && hRes == HRESULT_FROM_WIN32(ERROR_GEN_FAILURE)) ? S_FALSE : hRes;
  }
  return S_OK;
}

HRESULT CNamedPipes::CConnection::SendWritePacket(__in CPacket *lpPacket)
{
  DWORD dwWritten;
  HRESULT hRes;

  if (::WriteFile(hPipe, lpPacket->GetBuffer(), lpPacket->GetBytesInUse(), &dwWritten,
                  lpPacket->GetOverlapped()) == FALSE)
  {
    hRes = MX_HRESULT_FROM_LASTERROR();
    return (hRes == MX_E_BrokenPipe || hRes == HRESULT_FROM_WIN32(ERROR_NO_DATA) ||
            hRes == MX_E_PipeNotConnected && hRes == HRESULT_FROM_WIN32(ERROR_GEN_FAILURE)) ? S_FALSE : hRes;
  }
  return S_OK;
}

} //namespace MX

//-----------------------------------------------------------

static VOID GenerateUniquePipeName(__out LPWSTR szNameW, __in SIZE_T nNameSize, __in DWORD dwVal1, __in DWORD dwVal2)
{
  DWORD dwVal[4];

  dwVal[0] = ::GetTickCount();
  dwVal[1] = ::GetCurrentProcessId();
  dwVal[2] = dwVal1;
  dwVal[3] = dwVal2;
  mx_swprintf_s(szNameW, nNameSize, L"\\\\.\\pipe\\MxNamedPipe%016I64x",
                (ULONGLONG)fnv_64a_buf(dwVal, sizeof(dwVal), FNV1A_64_INIT));
  return;
}
