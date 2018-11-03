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
#include <VersionHelpers.h>

//-----------------------------------------------------------

#define TypeListenRequest (CPacket::eType)((int)CPacket::TypeMAX + 1)
#define TypeListen        (CPacket::eType)((int)CPacket::TypeMAX + 2)

//-----------------------------------------------------------

//D:(A;;0x12019F;;;WD)
static const BYTE aSecDescriptorXP[] = {
  0x01, 0x00, 0x04, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x14, 0x00, 0x00, 0x00, 0x02, 0x00, 0x1C, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00,
  0x9F, 0x01, 0x12, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00
};

//D:(A;;0x12019F;;;WD)S:(ML;;NW;;;LW)
static const BYTE aSecDescriptorVistaOrLater[] = {
  0x01, 0x00, 0x14, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
  0x30, 0x00, 0x00, 0x00, 0x02, 0x00, 0x1C, 0x00, 0x01, 0x00, 0x00, 0x00, 0x11, 0x00, 0x14, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x10, 0x00, 0x00,
  0x02, 0x00, 0x1C, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x9F, 0x01, 0x12, 0x00,
  0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00
};

//-----------------------------------------------------------

static VOID GenerateUniquePipeName(_Out_ LPWSTR szNameW, _In_ SIZE_T nNameSize, _In_ DWORD dwVal1, _In_ DWORD dwVal2);

//-----------------------------------------------------------

namespace MX {

CNamedPipes::CNamedPipes(_In_ CIoCompletionPortThreadPool &cDispatcherPool) : CIpc(cDispatcherPool)
{
  _InterlockedExchange(&nRemoteConnCounter, 0);
  dwMaxWaitTimeoutMs = 1000;
  lpSecDescr = (PSECURITY_DESCRIPTOR)((::IsWindowsVistaOrGreater() != FALSE) ? aSecDescriptorVistaOrLater :
                                                                               aSecDescriptorXP);
  return;
}

CNamedPipes::~CNamedPipes()
{
  Finalize();
  return;
}

VOID CNamedPipes::SetOption_MaxWaitTimeoutMs(_In_ DWORD dwTimeoutMs)
{
  CFastLock cInitShutdownLock(&nInitShutdownMutex);

  if (cShuttingDownEv.Get() == NULL)
  {
    dwMaxWaitTimeoutMs = dwTimeoutMs;
    if (dwMaxWaitTimeoutMs < 100)
      dwMaxWaitTimeoutMs = 100;
    else if (dwMaxWaitTimeoutMs > 180000)
      dwMaxWaitTimeoutMs = 180000;
  }
  return;
}

HRESULT CNamedPipes::CreateListener(_In_z_ LPCSTR szServerNameA, _In_ OnCreateCallback cCreateCallback,
                                    _In_opt_z_ LPCWSTR szSecutityDescriptorA)
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

HRESULT CNamedPipes::CreateListener(_In_z_ LPCWSTR szServerNameW, _In_ OnCreateCallback cCreateCallback,
                                    _In_opt_z_ LPCWSTR szSecutityDescriptorW)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CServerInfo> cServerInfo;
  HRESULT hRes;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  if (szServerNameW == NULL)
    return E_POINTER;
  if (*szServerNameW == 0 || (!cCreateCallback))
    return E_INVALIDARG;
  //initialize server info
  cServerInfo.Attach(MX_DEBUG_NEW CServerInfo());
  if (!cServerInfo)
    return E_OUTOFMEMORY;
  if (szSecutityDescriptorW != NULL && *szSecutityDescriptorW != 0)
  {
    PSECURITY_DESCRIPTOR _lpSecDescr;

    if (::ConvertStringSecurityDescriptorToSecurityDescriptorW(szSecutityDescriptorW, SECURITY_DESCRIPTOR_REVISION,
                                                               &_lpSecDescr, NULL) != FALSE)
    {
      hRes = cServerInfo->Init(szServerNameW, _lpSecDescr);
      if (FAILED(hRes))
        ::LocalFree(_lpSecDescr);
    }
    else
    {
      hRes = MX_HRESULT_FROM_LASTERROR();
    }
  }
  else
  {
    hRes = cServerInfo->Init(szServerNameW, lpSecDescr);
  }
  if (SUCCEEDED(hRes))
  {
    hRes = CreateServerConnection((CServerInfo*)cServerInfo, cCreateCallback);
    if (FAILED(hRes))
      FireOnEngineError(hRes);
  }
  return hRes;
}

HRESULT CNamedPipes::ConnectToServer(_In_z_ LPCSTR szServerNameA, _In_ OnCreateCallback cCreateCallback,
                                     _In_opt_ CUserData *lpUserData, _Out_opt_ HANDLE *h)
{
  CStringW cStrTempW;

  if (szServerNameA == NULL)
    return E_POINTER;
  if (cStrTempW.Copy(szServerNameA) == FALSE)
    return E_OUTOFMEMORY;
  return ConnectToServer((LPWSTR)cStrTempW, cCreateCallback, lpUserData, h);
}

HRESULT CNamedPipes::ConnectToServer(_In_z_ LPCWSTR szServerNameW, _In_ OnCreateCallback cCreateCallback,
                                     _In_opt_ CUserData *lpUserData, _Out_opt_ HANDLE *h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoDeletePtr<CConnection> cConn;
  CStringW cStrTempW;
  HRESULT hRes;

  if (h != NULL)
    *h = NULL;
  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
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

    sConnections.cTree.Insert(cConn.Get());
  }
  hRes = FireOnCreate(cConn.Get());
  if (SUCCEEDED(hRes))
    hRes = cConn->CreateClient((LPCWSTR)cStrTempW, dwMaxWaitTimeoutMs, lpSecDescr);
  if (SUCCEEDED(hRes))
    hRes = cConn->HandleConnected();
  if (SUCCEEDED(hRes))
    FireOnConnect(cConn.Get(), hRes);
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

HRESULT CNamedPipes::CreateRemoteClientConnection(_In_ HANDLE hProc, _Out_ HANDLE &h, _Out_ HANDLE &hRemotePipe,
                                                  _In_ OnCreateCallback cCreateCallback,
                                                  _In_opt_ CUserData *lpUserData)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoDeletePtr<CConnection> cConn;
  SECURITY_ATTRIBUTES sSecAttrib;
  OVERLAPPED sConnOvr;
  CWindowsEvent cConnEv;
  BOOL bConnected;
  WCHAR szBufW[128];
  CWindowsHandle cLocalPipe, cRemotePipe;
  DWORD dw, dwMode;
  HRESULT hRes;

  h = NULL;
  hRemotePipe = NULL;
  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  if (!cCreateCallback)
    return E_INVALIDARG;
  //create named pipe
  sSecAttrib.nLength = sizeof(SECURITY_ATTRIBUTES);
  sSecAttrib.bInheritHandle = FALSE;
  sSecAttrib.lpSecurityDescriptor = lpSecDescr;
  GenerateUniquePipeName(szBufW, MX_ARRAYLEN(szBufW), (DWORD)((ULONG_PTR)this),
                         (DWORD)_InterlockedIncrement(&nRemoteConnCounter));
  cLocalPipe.Attach(::CreateNamedPipeW(szBufW, PIPE_ACCESS_DUPLEX | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED,
                                       PIPE_READMODE_BYTE | PIPE_TYPE_BYTE | PIPE_WAIT, 1, dwPacketSize, dwPacketSize,
                                       10000, &sSecAttrib));
  if (cLocalPipe == NULL || cLocalPipe == INVALID_HANDLE_VALUE)
    return MX_HRESULT_FROM_LASTERROR();
  //initialize for connection wait
  hRes = cConnEv.Create(TRUE, FALSE);
  if (FAILED(hRes))
    return hRes;
  MemSet(&sConnOvr, 0, sizeof(sConnOvr));
  sConnOvr.hEvent = cConnEv.Get();
  bConnected = ::ConnectNamedPipe(cLocalPipe, &sConnOvr);
  //create other party pipe
  cRemotePipe.Attach(::CreateFileW(szBufW, GENERIC_READ|GENERIC_WRITE, 0, &sSecAttrib, OPEN_EXISTING,
                                   FILE_FLAG_WRITE_THROUGH|FILE_FLAG_OVERLAPPED, NULL));
  if (cRemotePipe == NULL || cRemotePipe == INVALID_HANDLE_VALUE)
    return MX_HRESULT_FROM_LASTERROR();
  //now wait until connected (may be not necessary)
  if (bConnected == FALSE)
  {
    if (::WaitForSingleObject(sConnOvr.hEvent, dwMaxWaitTimeoutMs) != WAIT_OBJECT_0)
      return MX_E_BrokenPipe;
    if (::GetOverlappedResult(cLocalPipe, &sConnOvr, &dw, FALSE) == FALSE)
    {
      hRes = MX_HRESULT_FROM_LASTERROR();
      if (hRes != MX_HRESULT_FROM_WIN32(ERROR_PIPE_CONNECTED))
        return hRes;
    }
  }
  //change wait mode
  dwMode = PIPE_WAIT;
  if (::SetNamedPipeHandleState(cLocalPipe, &dwMode, NULL, NULL) == FALSE)
    return MX_HRESULT_FROM_LASTERROR();
  //duplicate handle on target process
  if (::DuplicateHandle(::GetCurrentProcess(), cRemotePipe.Get(), hProc, &hRemotePipe, 0, FALSE,
                        DUPLICATE_SAME_ACCESS) == FALSE)
  {
    return MX_HRESULT_FROM_LASTERROR();
  }
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

    sConnections.cTree.Insert(cConn.Get());
  }
  hRes = FireOnCreate(cConn.Get());
  if (SUCCEEDED(hRes))
    hRes = cDispatcherPool.Attach(cConn->hPipe, cDispatcherPoolPacketCallback);
  if (SUCCEEDED(hRes))
    hRes = cConn->HandleConnected();
  if (SUCCEEDED(hRes))
    FireOnConnect(cConn.Get(), hRes);
  //done
  if (FAILED(hRes))
  {
    cConn->Close(hRes);
    if (hRemotePipe != NULL)
    {
      CWindowsRemoteHandle::CloseRemoteHandleSEH(hProc, hRemotePipe);
      hRemotePipe = NULL;
    }
  }
  cConn.Detach();
  return hRes;
}

HRESULT CNamedPipes::ImpersonateConnectionClient(_In_ HANDLE h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnection> cConn;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  cConn.Attach(reinterpret_cast<CConnection*>(CheckAndGetConnection(h)));
  if (!cConn)
    return E_INVALIDARG;
  //do impersonation
  if (cConn->hPipe == NULL)
    return MX_E_NotReady;
  return (::ImpersonateNamedPipeClient(cConn->hPipe) != FALSE) ? S_OK : MX_HRESULT_FROM_LASTERROR();
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

HRESULT CNamedPipes::CreateServerConnection(_In_ CServerInfo *lpServerInfo, _In_ OnCreateCallback _cCreateCallback)
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

    sConnections.cTree.Insert(cConn.Get());
  }
  hRes = FireOnCreate(cConn.Get());
  if (SUCCEEDED(hRes))
    hRes = cConn->CreateServer(dwPacketSize);
  //done
  if (FAILED(hRes))
  {
    cConn->Close(hRes);
  }
  cConn.Detach();
  return hRes;
}

HRESULT CNamedPipes::OnPreprocessPacket(_In_ DWORD dwBytes, _In_ CPacket *lpPacket, _In_ HRESULT hRes)
{
  return S_FALSE;
}

HRESULT CNamedPipes::OnCustomPacket(_In_ DWORD dwBytes, _In_ CPacket *lpPacket, _In_ HRESULT hRes)
{
  CConnection *lpConn;
  HRESULT hOrigRes;

  lpConn = (CConnection*)(lpPacket->GetConn());
  switch (lpPacket->GetType())
  {
    case TypeListenRequest:
      if (SUCCEEDED(hRes))
      {
        lpPacket->SetType(TypeListen);
        lpConn->AddRef();
        if (::ConnectNamedPipe(lpConn->hPipe, lpPacket->GetOverlapped()) == FALSE)
        {
          hRes = MX_HRESULT_FROM_LASTERROR();
          if (hRes == HRESULT_FROM_WIN32(ERROR_PIPE_CONNECTED))
          {
            lpConn->Release();
            hRes = S_OK;
            goto pipe_connected;
          }
          if (hRes == MX_E_IoPending)
            hRes = S_OK;
        }
      }
      if (FAILED(hRes))
      {
        lpConn->cRwList.Remove(lpPacket);
        FreePacket(lpPacket);
        //process connection
        if (lpConn->nClass == CIpc::ConnectionClassServer && lpConn->IsClosed() == FALSE && IsShuttingDown() == FALSE)
        {
          //on server mode, create a new listener
          HRESULT hRes2 = CreateServerConnection(lpConn->cServerInfo, lpConn->cCreateCallback);
          if (FAILED(hRes2))
            FireOnEngineError(hRes2);
        }
      }
      break;

    case TypeListen:
pipe_connected:
      hOrigRes = hRes;
      if (SUCCEEDED(hRes))
        hRes = lpConn->HandleConnected();
      //free packet
      lpConn->cRwList.Remove(lpPacket);
      FreePacket(lpPacket);
      //process connection
      if (lpConn->nClass == CIpc::ConnectionClassServer && lpConn->IsClosed() == FALSE && IsShuttingDown() == FALSE)
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

CNamedPipes::CServerInfo::CServerInfo() : TRefCounted<CBaseMemObj>()
{
  lpSecDescr = NULL;
  return;
}

CNamedPipes::CServerInfo::~CServerInfo()
{
  if (lpSecDescr != NULL && lpSecDescr != (PSECURITY_DESCRIPTOR)&aSecDescriptorVistaOrLater &&
      lpSecDescr != (PSECURITY_DESCRIPTOR)&aSecDescriptorXP)
  {
    ::LocalFree(lpSecDescr);
  }
  return;
}

HRESULT CNamedPipes::CServerInfo::Init(_In_z_ LPCWSTR szServerNameW, _In_ PSECURITY_DESCRIPTOR _lpSecDescr)
{
  if (cStrNameW.Format(L"\\\\.\\pipe\\%s", szServerNameW) == FALSE)
    return E_OUTOFMEMORY;
  lpSecDescr = _lpSecDescr;
  return S_OK;
}

//-----------------------------------------------------------

CNamedPipes::CConnection::CConnection(_In_ CIpc *lpIpc, _In_ CIpc::eConnectionClass nClass) :
                          CConnectionBase(lpIpc, nClass)
{
  SlimRWL_Initialize(&nRwHandleInUse);
  hPipe = NULL;
  return;
}

CNamedPipes::CConnection::~CConnection()
{
  MX_ASSERT(hPipe == NULL);
  return;
}

HRESULT CNamedPipes::CConnection::CreateServer(_In_ DWORD dwPacketSize)
{
  SECURITY_ATTRIBUTES sSecAttrib;
  CPacket *lpPacket;
  HRESULT hRes;

  //create server pipe endpoint
  sSecAttrib.nLength = (DWORD)sizeof(sSecAttrib);
  sSecAttrib.bInheritHandle = FALSE;
  sSecAttrib.lpSecurityDescriptor = cServerInfo->lpSecDescr;
  hPipe = ::CreateNamedPipeW((LPWSTR)(cServerInfo->cStrNameW), PIPE_ACCESS_DUPLEX|FILE_FLAG_WRITE_THROUGH|
                             FILE_FLAG_OVERLAPPED, PIPE_READMODE_BYTE|PIPE_TYPE_BYTE|PIPE_WAIT,
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
  AddRef();
  hRes = GetDispatcherPool().Post(GetDispatcherPoolPacketCallback(), 0, lpPacket->GetOverlapped());
  if (FAILED(hRes))
  {
    cRwList.Remove(lpPacket);
    FreePacket(lpPacket);
    Release();
  }
  //done
  return hRes;
}

HRESULT CNamedPipes::CConnection::CreateClient(_In_z_ LPCWSTR szServerNameW, _In_ DWORD dwMaxWriteTimeoutMs,
                                               _In_ PSECURITY_DESCRIPTOR lpSecDescr)
{
  SECURITY_ATTRIBUTES sSecAttrib;
  DWORD dwMode;
  HRESULT hRes;

  //create client pipe endpoint
  sSecAttrib.nLength = (DWORD)sizeof(sSecAttrib);
  sSecAttrib.bInheritHandle = FALSE;
  sSecAttrib.lpSecurityDescriptor = lpSecDescr;
  while (1)
  {
    hPipe = ::CreateFileW(szServerNameW, GENERIC_READ|GENERIC_WRITE, 0, &sSecAttrib, OPEN_EXISTING,
                          FILE_FLAG_WRITE_THROUGH|FILE_FLAG_OVERLAPPED, NULL);
    if (hPipe != NULL && hPipe != INVALID_HANDLE_VALUE)
      break;
    hPipe = NULL;
    hRes = MX_HRESULT_FROM_LASTERROR();
    if (dwMaxWriteTimeoutMs == 0 || hRes != MX_HRESULT_FROM_WIN32(ERROR_PIPE_BUSY))
      return hRes;
    if (dwMaxWriteTimeoutMs >= 100)
    {
      ::WaitNamedPipeW(szServerNameW, 100);
      dwMaxWriteTimeoutMs -= 100;
    }
    else
    {
      ::WaitNamedPipeW(szServerNameW, dwMaxWriteTimeoutMs);
      dwMaxWriteTimeoutMs = 0;
    }
  }
  //attach to completion port
  hRes = GetDispatcherPool().Attach(hPipe, GetDispatcherPoolPacketCallback());
  if (FAILED(hRes))
    return hRes;
  //change wait mode
  dwMode = PIPE_WAIT;
  if (::SetNamedPipeHandleState(hPipe, &dwMode, NULL, NULL) == FALSE)
    return MX_HRESULT_FROM_LASTERROR();
  //done
  return S_OK;
}

VOID CNamedPipes::CConnection::ShutdownLink(_In_ BOOL bAbortive)
{
  {
    CAutoSlimRWLExclusive cHandleInUseLock(&nRwHandleInUse);

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

HRESULT CNamedPipes::CConnection::SendReadPacket(_In_ CPacket *lpPacket)
{
  CAutoSlimRWLShared cHandleInUseLock(&nRwHandleInUse);
  DWORD dwReaded;
  HRESULT hRes;

  if (hPipe == NULL)
    return S_FALSE;
  if (::ReadFile(hPipe, lpPacket->GetBuffer(), lpPacket->GetBytesInUse(), &dwReaded,
                 lpPacket->GetOverlapped()) == FALSE)
  {
    hRes = MX_HRESULT_FROM_LASTERROR();
    return (hRes == MX_E_BrokenPipe || hRes == MX_E_NoData || hRes == MX_E_PipeNotConnected &&
            hRes == HRESULT_FROM_WIN32(ERROR_GEN_FAILURE)) ? S_FALSE : hRes;
  }
  return S_OK;
}

HRESULT CNamedPipes::CConnection::SendWritePacket(_In_ CPacket *lpPacket)
{
  CAutoSlimRWLShared cHandleInUseLock(&nRwHandleInUse);
  DWORD dwWritten;
  HRESULT hRes;

  if (hPipe == NULL)
    return S_FALSE;
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

static VOID GenerateUniquePipeName(_Out_ LPWSTR szNameW, _In_ SIZE_T nNameSize, _In_ DWORD dwVal1, _In_ DWORD dwVal2)
{
  DWORD dwVal[4];

#pragma warning(suppress: 28159)
  dwVal[0] = ::GetTickCount();
  dwVal[1] = ::GetCurrentProcessId();
  dwVal[2] = dwVal1;
  dwVal[3] = dwVal2;
  mx_swprintf_s(szNameW, nNameSize, L"\\\\.\\pipe\\MxNamedPipe%016I64x",
                (ULONGLONG)fnv_64a_buf(dwVal, sizeof(dwVal), FNV1A_64_INIT));
  return;
}
