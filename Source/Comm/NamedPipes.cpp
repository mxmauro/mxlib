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
#include "..\..\Include\Comm\NamedPipes.h"
#include "..\..\Include\FnvHash.h"
#include "..\..\Include\AutoHandle.h"
#include "..\..\Include\MemoryBarrier.h"
#include <Sddl.h>
#include <VersionHelpers.h>

//-----------------------------------------------------------

#define TypeListenRequest (CPacketBase::eType)((int)CPacketBase::TypeMAX + 1)
#define TypeListen        (CPacketBase::eType)((int)CPacketBase::TypeMAX + 2)

#ifndef FILE_SKIP_COMPLETION_PORT_ON_SUCCESS
  #define FILE_SKIP_COMPLETION_PORT_ON_SUCCESS    0x1
#endif //!FILE_SKIP_COMPLETION_PORT_ON_SUCCESS
#ifndef FILE_SKIP_SET_EVENT_ON_HANDLE
  #define FILE_SKIP_SET_EVENT_ON_HANDLE           0x2
#endif //!FILE_SKIP_SET_EVENT_ON_HANDLE

//-----------------------------------------------------------

typedef BOOL (WINAPI *lpfnSetFileCompletionNotificationModes)(_In_ HANDLE FileHandle, _In_ UCHAR Flags);

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

static LONG volatile nOSVersion = -1;
static lpfnSetFileCompletionNotificationModes volatile fnSetFileCompletionNotificationModes = NULL;

//-----------------------------------------------------------

static VOID GenerateUniquePipeName(_Out_ LPWSTR szNameW, _In_ SIZE_T nNameSize, _In_ DWORD dwVal1, _In_ DWORD dwVal2);

//-----------------------------------------------------------

namespace MX {

CNamedPipes::CNamedPipes(_In_ CIoCompletionPortThreadPool &cDispatcherPool) : CIpc(cDispatcherPool), CNonCopyableObj()
{
  _InterlockedExchange(&nRemoteConnCounter, 0);
  dwConnectTimeoutMs = 1000;
  lpSecDescr = (PSECURITY_DESCRIPTOR)((::IsWindowsVistaOrGreater() != FALSE) ? aSecDescriptorVistaOrLater :
                                                                               aSecDescriptorXP);
  //detect OS version
  if (__InterlockedRead(&nOSVersion) < 0)
  {
    LONG _nOSVersion = 0;

    if (::IsWindowsVistaOrGreater() != FALSE)
      _nOSVersion = 0x0600;
#pragma warning(default : 4996)
    _InterlockedExchange(&nOSVersion, _nOSVersion);
  }

  if (__InterlockedRead(&nOSVersion) >= 0x0600)
  {
    if (__InterlockedReadPointer(&fnSetFileCompletionNotificationModes) == NULL)
    {
      LPVOID _fnSetFileCompletionNotificationModes;
      HINSTANCE hDll;

      _fnSetFileCompletionNotificationModes = NULL;
      hDll = ::GetModuleHandleW(L"kernelbase.dll");
      if (hDll != NULL)
      {
        _fnSetFileCompletionNotificationModes = ::GetProcAddress(hDll, "SetFileCompletionNotificationModes");
      }
      if (_fnSetFileCompletionNotificationModes == NULL)
      {
        hDll = ::GetModuleHandleW(L"kernel32.dll");
        if (hDll != NULL)
        {
          _fnSetFileCompletionNotificationModes = ::GetProcAddress(hDll, "SetFileCompletionNotificationModes");
        }
      }
      _InterlockedExchangePointer((LPVOID volatile*)&fnSetFileCompletionNotificationModes,
                                  _fnSetFileCompletionNotificationModes);
    }
  }
  return;
}

CNamedPipes::~CNamedPipes()
{
  Finalize();
  return;
}

VOID CNamedPipes::SetOption_ConnectTimeout(_In_ DWORD dwTimeoutMs)
{
  CFastLock cInitShutdownLock(&nInitShutdownMutex);

  if (cShuttingDownEv.Get() == NULL)
  {
    dwConnectTimeoutMs = dwTimeoutMs;
    if (dwConnectTimeoutMs < 100)
      dwConnectTimeoutMs = 100;
    else if (dwConnectTimeoutMs > 180000)
      dwConnectTimeoutMs = 180000;
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
  TAutoRefCounted<CConnection> cNewConn;
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
  cNewConn.Attach(MX_DEBUG_NEW CConnection(this, CIpc::ConnectionClassClient));
  if (!cNewConn)
    return E_OUTOFMEMORY;
  //setup connection
  cNewConn->cCreateCallback = cCreateCallback;
  cNewConn->cUserData = lpUserData;
  {
    CAutoSlimRWLExclusive cConnListLock(&(sConnections.sRwMutex));

    sConnections.cTree.Insert(&(cNewConn->cTreeNode), &CConnectionBase::InsertCompareFunc);
  }
  hRes = FireOnCreate(cNewConn.Get());
  if (SUCCEEDED(hRes))
    hRes = cNewConn->CreateClient((LPCWSTR)cStrTempW, dwConnectTimeoutMs, lpSecDescr);
  if (SUCCEEDED(hRes))
    hRes = cNewConn->HandleConnected();
  //done
  if (SUCCEEDED(hRes))
  {
    if (h != NULL)
      *h = reinterpret_cast<HANDLE>(cNewConn.Get());
  }
  else
  {
    cNewConn->Close(hRes);
  }
  cNewConn.Detach();
  return hRes;
}

HRESULT CNamedPipes::CreateRemoteClientConnection(_In_ HANDLE hProc, _Out_ HANDLE &h, _Out_ HANDLE &hRemotePipe,
                                                  _In_ OnCreateCallback cCreateCallback,
                                                  _In_opt_ CUserData *lpUserData)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnection> cNewConn;
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
                                       PIPE_READMODE_BYTE | PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096,
                                       10000, &sSecAttrib));
  if (cLocalPipe == NULL || cLocalPipe == INVALID_HANDLE_VALUE)
    return MX_HRESULT_FROM_LASTERROR();
  //initialize for connection wait
  hRes = cConnEv.Create(TRUE, FALSE);
  if (FAILED(hRes))
    return hRes;
  MxMemSet(&sConnOvr, 0, sizeof(sConnOvr));
  sConnOvr.hEvent = cConnEv.Get();
  bConnected = ::ConnectNamedPipe(cLocalPipe, &sConnOvr);
  //create other party pipe
  cRemotePipe.Attach(::CreateFileW(szBufW, GENERIC_READ | GENERIC_WRITE, 0, &sSecAttrib, OPEN_EXISTING,
                                   FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED, NULL));
  if (cRemotePipe == NULL || cRemotePipe == INVALID_HANDLE_VALUE)
    return MX_HRESULT_FROM_LASTERROR();
  //now wait until connected (may be not necessary)
  if (bConnected == FALSE)
  {
    if (::WaitForSingleObject(sConnOvr.hEvent, dwConnectTimeoutMs) != WAIT_OBJECT_0)
      return MX_E_BrokenPipe;
    if (::GetOverlappedResult(cLocalPipe, &sConnOvr, &dw, FALSE) == FALSE)
    {
      hRes = MX_HRESULT_FROM_LASTERROR();
      if (hRes != MX_HRESULT_FROM_WIN32(ERROR_PIPE_CONNECTED))
        return hRes;
    }
  }
  //IOCP options
  if (fnSetFileCompletionNotificationModes != NULL)
  {
    if (fnSetFileCompletionNotificationModes(cLocalPipe, FILE_SKIP_SET_EVENT_ON_HANDLE |
                                                         FILE_SKIP_COMPLETION_PORT_ON_SUCCESS) == FALSE)
    {
      return MX_HRESULT_FROM_LASTERROR();
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
  cNewConn.Attach(MX_DEBUG_NEW CConnection(this, CIpc::ConnectionClassServer));
  if (!cNewConn)
  {
    if (hRemotePipe != NULL)
    {
      CWindowsRemoteHandle::CloseRemoteHandleSEH(hProc, hRemotePipe);
      hRemotePipe = NULL;
    }
    return E_OUTOFMEMORY;
  }
  //setup connection
  cNewConn->hPipe = cLocalPipe.Detach();
  cNewConn->cCreateCallback = cCreateCallback;
  cNewConn->cUserData = lpUserData;
  {
    CAutoSlimRWLExclusive cConnListLock(&(sConnections.sRwMutex));

    sConnections.cTree.Insert(&(cNewConn->cTreeNode), &CConnectionBase::InsertCompareFunc);
  }
  hRes = FireOnCreate(cNewConn.Get());
  if (SUCCEEDED(hRes))
    hRes = cDispatcherPool.Attach(cNewConn->hPipe, cDispatcherPoolPacketCallback);
  if (SUCCEEDED(hRes))
    hRes = cNewConn->HandleConnected();
  //done
  if (FAILED(hRes))
  {
    cNewConn->Close(hRes);
    if (hRemotePipe != NULL)
    {
      CWindowsRemoteHandle::CloseRemoteHandleSEH(hProc, hRemotePipe);
      hRemotePipe = NULL;
    }
  }
  cNewConn.Detach();
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
  TAutoRefCounted<CConnection> cNewConn;
  HRESULT hRes;

  //create connection
  cNewConn.Attach(MX_DEBUG_NEW CConnection(this, CIpc::ConnectionClassServer));
  if (!cNewConn)
    return E_OUTOFMEMORY;
  //setup connection
  cNewConn->cCreateCallback = _cCreateCallback;
  cNewConn->cServerInfo = lpServerInfo;
  {
    CAutoSlimRWLExclusive cConnListLock(&(sConnections.sRwMutex));

    sConnections.cTree.Insert(&(cNewConn->cTreeNode), &CConnectionBase::InsertCompareFunc);
  }
  hRes = FireOnCreate(cNewConn.Get());
  if (SUCCEEDED(hRes))
    hRes = cNewConn->CreateServer();
  //done
  if (FAILED(hRes))
  {
    cNewConn->Close(hRes);
  }
  cNewConn.Detach();
  return hRes;
}

HRESULT CNamedPipes::OnCustomPacket(_In_ DWORD dwBytes, _In_ CPacketBase *lpPacket, _In_ HRESULT hRes)
{
  CConnection *lpConn;
  HRESULT hOrigRes;

  lpConn = (CConnection*)(lpPacket->GetConn());
  switch (lpPacket->GetType())
  {
    case TypeListenRequest:
      if (SUCCEEDED(hRes))
      {
        {
          CFastLock cListLock(&(lpConn->sInUsePackets.nMutex));

          lpConn->sInUsePackets.cList.QueueLast(lpPacket);
        }
        lpPacket->SetType(TypeListen);
        lpConn->AddRef();
        if (::ConnectNamedPipe(lpConn->hPipe, lpPacket->GetOverlapped()) == FALSE)
        {
          hRes = MX_HRESULT_FROM_LASTERROR();
          if (hRes == HRESULT_FROM_WIN32(ERROR_PIPE_CONNECTED))
          {
            //valid connection, the client connected between CreateNamedPipe and ConnectNamedPipe calls
            {
              CFastLock cListLock(&(lpConn->sInUsePackets.nMutex));

              lpConn->sInUsePackets.cList.Remove(lpPacket);
            }

            //release extra reference added above
            lpConn->Release();

            hRes = S_OK;
            goto pipe_connected;
          }

          if (hRes == MX_E_IoPending)
          {
            hRes = S_OK;
          }
          else
          {
            //free packet
            CFastLock cListLock(&(lpConn->sInUsePackets.nMutex));

            lpConn->sInUsePackets.cList.Remove(lpPacket);
          }
        }
      }
      if (FAILED(hRes))
      {
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

CNamedPipes::CServerInfo::CServerInfo() : TRefCounted<CBaseMemObj>(), CNonCopyableObj()
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
                          CConnectionBase(lpIpc, nClass), CNonCopyableObj()
{
  SlimRWL_Initialize(&sRwHandleInUse);
  hPipe = NULL;
  return;
}

CNamedPipes::CConnection::~CConnection()
{
  //NOTE: The pipe can be still open if some write requests were queued while a graceful shutdown was in progress
  MX_ASSERT((SIZE_T)(ULONG)__InterlockedRead(&nOutgoingWrites) ==
            sPendingWritePackets.cList.GetCount() +
            sInUsePackets.cList.GetCountOfType(CIpc::CPacketBase::TypeWriteRequest));

  if (hPipe != NULL)
    ::CloseHandle(hPipe);
  return;
}

HRESULT CNamedPipes::CConnection::CreateServer()
{
  SECURITY_ATTRIBUTES sSecAttrib;
  CPacketBase *lpPacket;
  HRESULT hRes;

  //create server pipe endpoint
  sSecAttrib.nLength = (DWORD)sizeof(sSecAttrib);
  sSecAttrib.bInheritHandle = FALSE;
  sSecAttrib.lpSecurityDescriptor = cServerInfo->lpSecDescr;
  hPipe = ::CreateNamedPipeW((LPWSTR)(cServerInfo->cStrNameW), PIPE_ACCESS_DUPLEX | FILE_FLAG_WRITE_THROUGH |
                             FILE_FLAG_OVERLAPPED, PIPE_READMODE_BYTE | PIPE_TYPE_BYTE | PIPE_WAIT,
                             PIPE_UNLIMITED_INSTANCES, 4096, 4096, 10000, &sSecAttrib);
  if (hPipe == NULL || hPipe == INVALID_HANDLE_VALUE)
  {
    hPipe = NULL;
    return MX_HRESULT_FROM_LASTERROR();
  }
  ::SetHandleInformation(hPipe, HANDLE_FLAG_INHERIT, 0);

  //IOCP options
  if (fnSetFileCompletionNotificationModes != NULL)
  {
    if (fnSetFileCompletionNotificationModes(hPipe, FILE_SKIP_SET_EVENT_ON_HANDLE |
                                             FILE_SKIP_COMPLETION_PORT_ON_SUCCESS) == FALSE)
    {
      return MX_HRESULT_FROM_LASTERROR();
    }
  }

  //attach to completion port
  hRes = GetDispatcherPool().Attach(hPipe, GetDispatcherPoolPacketCallback());
  if (FAILED(hRes))
    return hRes;

  //wait for connection
  lpPacket = GetPacket(TypeListenRequest, 0, FALSE);
  if (lpPacket == NULL)
    return E_OUTOFMEMORY;
  {
    CFastLock cListLock(&(sInUsePackets.nMutex));

    sInUsePackets.cList.QueueLast(lpPacket);
  }
  AddRef(); //NOTE: this generates a full fence
  hRes = GetDispatcherPool().Post(GetDispatcherPoolPacketCallback(), 0, lpPacket->GetOverlapped());
  if (FAILED(hRes))
  {
    //free packet
    {
      CFastLock cListLock(&(sInUsePackets.nMutex));

      sInUsePackets.cList.Remove(lpPacket);
    }
    FreePacket(lpPacket);

    //release connection
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

  //IOCP options
  if (fnSetFileCompletionNotificationModes != NULL)
  {
    if (fnSetFileCompletionNotificationModes(hPipe, FILE_SKIP_SET_EVENT_ON_HANDLE |
                                             FILE_SKIP_COMPLETION_PORT_ON_SUCCESS) == FALSE)
    {
      return MX_HRESULT_FROM_LASTERROR();
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
  HANDLE hPipeToClose = NULL;

  {
    CAutoSlimRWLExclusive cHandleInUseLock(&sRwHandleInUse);

    if (hPipe != NULL)
    {
      hPipeToClose = hPipe;
      hPipe = NULL;
    }
  }

  if (hPipeToClose != NULL)
  {
    ::DisconnectNamedPipe(hPipeToClose);

    ::CloseHandle(hPipeToClose);
  }

  //call base
  CConnectionBase::ShutdownLink(bAbortive);
  return;
}

HRESULT CNamedPipes::CConnection::SendReadPacket(_In_ CPacketBase *lpPacket, _Out_ LPDWORD lpdwRead)
{
  CAutoSlimRWLShared cHandleInUseLock(&sRwHandleInUse);
  DWORD dwRead;
  HRESULT hRes;

  if (hPipe == NULL)
  {
    *lpdwRead = 0;
    return S_FALSE;
  }
  dwRead = 0;
  if (::ReadFile(hPipe, lpPacket->GetBuffer(), lpPacket->GetBytesInUse(), &dwRead, lpPacket->GetOverlapped()) != FALSE)
  {
    hRes = (fnSetFileCompletionNotificationModes != NULL) ? S_OK : MX_E_IoPending;
  }
  else
  {
    hRes = MX_HRESULT_FROM_LASTERROR();
    if (hRes == MX_E_BrokenPipe || hRes == MX_E_NoData || hRes == MX_E_PipeNotConnected ||
        hRes == HRESULT_FROM_WIN32(ERROR_GEN_FAILURE))
    {
      hRes = S_FALSE;
    }
  }
  *lpdwRead = dwRead;
  return hRes;
}

HRESULT CNamedPipes::CConnection::SendWritePacket(_In_ CPacketBase *lpPacket, _Out_ LPDWORD lpdwWritten)
{
  CAutoSlimRWLShared cHandleInUseLock(&sRwHandleInUse);
  DWORD dwWritten;
  HRESULT hRes;

  if (hPipe == NULL)
  {
    *lpdwWritten = 0;
    return S_FALSE;
  }
  if (lpIpc->ShouldLog(2) != FALSE)
  {
    cLogTimer.Mark();
    lpIpc->Log(L"CNamedPipes::SendWritePacket) Clock=%lums / Conn=0x%p / Ovr=0x%p / Type=%lu / Bytes=%lu",
               cLogTimer.GetElapsedTimeMs(), this, lpPacket->GetOverlapped(), lpPacket->GetType(),
               lpPacket->GetBytesInUse());
    cLogTimer.ResetToLastMark();
  }
  dwWritten = 0;
  if (::WriteFile(hPipe, lpPacket->GetBuffer(), lpPacket->GetBytesInUse(), &dwWritten,
                  lpPacket->GetOverlapped()) != FALSE)
  {
    hRes = (fnSetFileCompletionNotificationModes != NULL) ? S_OK : MX_E_IoPending;
  }
  else
  {
    hRes = MX_HRESULT_FROM_LASTERROR();
    if (hRes == MX_E_BrokenPipe || hRes == MX_E_NoData || hRes == MX_E_PipeNotConnected ||
        hRes == HRESULT_FROM_WIN32(ERROR_GEN_FAILURE))
    {
      hRes = S_FALSE;
    }
  }
  *lpdwWritten = dwWritten;
  return hRes;
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
