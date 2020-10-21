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
#include "IpcDefs.h"
#include "..\..\Include\MemoryBarrier.h"

//-----------------------------------------------------------

namespace MX {

CIpc::CIpc(_In_ CIoCompletionPortThreadPool &_cDispatcherPool) : CBaseMemObj(), CLoggable(),
                                                                 cDispatcherPool(_cDispatcherPool)
{
  dwReadAhead = 4;
  bDoZeroReads = TRUE;
  dwMaxOutgoingBytes = 2 * 32768;
  //----
  cDispatcherPoolPacketCallback = MX_BIND_MEMBER_CALLBACK(&CIpc::OnDispatcherPacket, this);
  FastLock_Initialize(&nInitShutdownMutex);
  RundownProt_Initialize(&nRundownProt);
  cEngineErrorCallback = NullCallback();
  SlimRWL_Initialize(&(sConnections.sRwMutex));
  _InterlockedExchange(&(sConnections.nCount), 0);
  FastLock_Initialize(&(sFreePackets32768.nMutex));
  FastLock_Initialize(&(sFreePackets4096.nMutex));
  return;
}

CIpc::~CIpc()
{
#ifdef MX_MEMORY_OBJECTS_HEAP_CHECK
  CHAR szFileNameA[MX_ARRAYLEN(__FILE__) + 24], *sA;
#endif //MX_MEMORY_OBJECTS_HEAP_CHECK

  MX_ASSERT(cShuttingDownEv.Get() == NULL);

#ifdef MX_MEMORY_OBJECTS_HEAP_CHECK
  ::MxMemCopy(szFileNameA, __FILE__, MX_ARRAYLEN(__FILE__));
  szFileNameA[MX_ARRAYLEN(__FILE__)] = 0;
  ::MxDumpLeaks(szFileNameA);

  szFileNameA[MX_ARRAYLEN(__FILE__) - 4] = 'h';
  szFileNameA[MX_ARRAYLEN(__FILE__) - 3] = 0;
  sA = (LPSTR)MX::StrFindA(szFileNameA, "\\Source", TRUE, TRUE);
  ::MxMemMove(sA + 1, sA, sizeof(szFileNameA) - (SIZE_T)(sA - szFileNameA - 1));
  ::MxMemCopy(sA, "\\Include", 8);
  ::MxDumpLeaks(szFileNameA);

  ::MxMemCopy(szFileNameA, __FILE__, MX_ARRAYLEN(__FILE__));
  szFileNameA[MX_ARRAYLEN(__FILE__)] = 0;
  sA = (LPSTR)MX::StrChrA(szFileNameA, '\\', TRUE);
  ::MxMemCopy(sA + 1, "IpcConnection.cpp", 18);
  ::MxDumpLeaks(szFileNameA);

  ::MxMemCopy(sA + 1, "Sockets.cpp", 12);
  ::MxDumpLeaks(szFileNameA);

  ::MxMemCopy(sA + 1, "NamedPipes.cpp", 15);
  ::MxDumpLeaks(szFileNameA);
#endif //MX_MEMORY_OBJECTS_HEAP_CHECK
  return;
}

VOID CIpc::SetOption_ReadAheadCount(_In_ DWORD dwCount)
{
  CFastLock cInitShutdownLock(&nInitShutdownMutex);

  if (cShuttingDownEv.Get() == NULL)
  {
    dwReadAhead = dwCount;
    if (dwReadAhead < 1)
      dwReadAhead = 1;
    else if (dwReadAhead > 16)
      dwReadAhead = 16;
  }
  return;
}

VOID CIpc::SetOption_EnableZeroReads(_In_ BOOL bEnable)
{
  CFastLock cInitShutdownLock(&nInitShutdownMutex);

  if (cShuttingDownEv.Get() == NULL)
  {
    bDoZeroReads = bEnable;
  }
  return;
}

VOID CIpc::SetOption_OutgoingBytesLimitCount(_In_ DWORD dwCount)
{
  CFastLock cInitShutdownLock(&nInitShutdownMutex);

  if (cShuttingDownEv.Get() == NULL)
  {
    dwMaxOutgoingBytes = dwCount;
    if (dwMaxOutgoingBytes < 8192)
      dwMaxOutgoingBytes = 8192;
    else if (dwMaxOutgoingBytes > 262144)
      dwMaxOutgoingBytes = 262144;
  }
  return;
}

VOID CIpc::SetEngineErrorCallback(_In_ OnEngineErrorCallback _cEngineErrorCallback)
{
  cEngineErrorCallback = _cEngineErrorCallback;
  return;
}

HRESULT CIpc::Initialize()
{
  CFastLock cInitShutdownLock(&nInitShutdownMutex);
  HRESULT hRes;

  InternalFinalize();

  //create shutdown event
  hRes = cShuttingDownEv.Create(TRUE, FALSE);
  //check if IO thread pools are running
  if (SUCCEEDED(hRes))
  {
    if (cDispatcherPool.GetActiveThreadsCount() == 0)
      hRes = MX_E_NotReady;
    else if (cDispatcherPool.IsDynamicGrowable() != FALSE)
      hRes = E_INVALIDARG;
  }

  //create internals
  if (SUCCEEDED(hRes))
    hRes = OnInternalInitialize();

  //done
  if (FAILED(hRes))
    InternalFinalize();
  return hRes;
}

VOID CIpc::Finalize()
{
  CFastLock cInitShutdownLock(&nInitShutdownMutex);

  if (cShuttingDownEv.Get() != NULL)
    InternalFinalize();
  return;
}

HRESULT CIpc::SendMsg(_In_ HANDLE h, _In_reads_bytes_(nMsgSize) LPCVOID lpMsg, _In_ SIZE_T nMsgSize)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;
  HRESULT hRes;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  if (lpMsg == NULL)
    return E_POINTER;
  if (nMsgSize == 0)
    return E_INVALIDARG;

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;

  //send real message
  if (cConn->IsClosed() != FALSE)
  {
    hRes = cConn->GetErrorCode();
    return (SUCCEEDED(hRes)) ? E_FAIL : hRes;
  }

  return cConn->SendMsg(lpMsg, nMsgSize);
}

HRESULT CIpc::SendStream(_In_ HANDLE h, _In_ CStream *lpStream)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CStream> cStream(lpStream);
  TAutoRefCounted<CConnectionBase> cConn;
  HRESULT hRes;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  if (lpStream == NULL)
    return E_POINTER;

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;

  //send real message
  if (cConn->IsClosed() != FALSE)
  {
    hRes = cConn->GetErrorCode();
    return (SUCCEEDED(hRes)) ? E_FAIL : hRes;
  }

  return cConn->SendStream(lpStream);
}

HRESULT CIpc::AfterWriteSignal(_In_ HANDLE h, _In_ OnAfterWriteSignalCallback cCallback, _In_opt_ LPVOID lpCookie)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;
  HRESULT hRes;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  if (!cCallback)
    return E_INVALIDARG;

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;

  //add signal
  if (cConn->IsClosed() != FALSE)
  {
    hRes = cConn->GetErrorCode();
    return (SUCCEEDED(hRes)) ? E_FAIL : hRes;
  }

  return cConn->AfterWriteSignal(cCallback, lpCookie);
}

CIpc::CMultiSendLock* CIpc::StartMultiSendBlock(_In_ HANDLE h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;

  if (cRundownLock.IsAcquired() == FALSE)
    return NULL;

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return NULL;

  //create lock
  return MX_DEBUG_NEW CMultiSendLock(cConn.Get());
}

HRESULT CIpc::GetBufferedMessage(_In_ HANDLE h, _Out_ LPVOID lpMsg, _Inout_ SIZE_T *lpnMsgSize)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  if (lpMsg == NULL || lpnMsgSize == NULL)
    return E_POINTER;

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;

  //get received data
  {
    CFastLock cRecBufLock(&(cConn->sReceivedData.nMutex));

    *lpnMsgSize = cConn->sReceivedData.cBuffer.Peek(lpMsg, *lpnMsgSize);
  }
  //done
  return S_OK;
}

HRESULT CIpc::ConsumeBufferedMessage(_In_ HANDLE h, _In_ SIZE_T nConsumedBytes)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;
  HRESULT hRes;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;

  //consume received data
  {
    CFastLock cRecBufLock(&(cConn->sReceivedData.nMutex));

    hRes = cConn->sReceivedData.cBuffer.AdvanceReadPtr(nConsumedBytes);
  }
  //done
  return hRes;
}

HRESULT CIpc::Close(_In_opt_ HANDLE h, _In_opt_ HRESULT hrErrorCode)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;

  //close connection
  cConn->Close(hrErrorCode);

  //done
  return S_OK;
}

HRESULT CIpc::InitializeSSL(_In_ HANDLE h, _In_opt_ LPCSTR szHostNameA,
                            _In_opt_ CSslCertificateArray *lpCheckCertificates,
                            _In_opt_ CSslCertificate *lpSelfCert, _In_opt_ CEncryptionKey *lpPrivKey,
                            _In_opt_ CDhParam *lpDhParam, _In_opt_ int nSslOptions)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;
  HRESULT hRes;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;

  hRes = cConn->SetupSsl(szHostNameA, lpCheckCertificates, lpSelfCert, lpPrivKey, lpDhParam, nSslOptions);
  if (FAILED(hRes))
    return hRes;

  if ((__InterlockedRead(&(cConn->nFlags)) & FLAG_InitialSetupExecuted) != 0)
  {
    hRes = cConn->HandleSslStartup();
    if (FAILED(hRes))
    {
      cConn->Close(hRes);
      return hRes;
    }
  }

  //done
  return S_OK;
}

HRESULT CIpc::IsConnected(_In_ HANDLE h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;

  //get state
  return (cConn->IsConnected() != FALSE) ? S_OK : S_FALSE;
}

HRESULT CIpc::IsClosed(_In_ HANDLE h, _Out_opt_ HRESULT *lphErrorCode)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;

  //get state
  if (lphErrorCode != NULL)
    *lphErrorCode = __InterlockedRead(&(cConn->hrErrorCode));
  return (cConn->IsClosed() != FALSE) ? S_OK : S_FALSE;
}

HRESULT CIpc::GetReadStats(_In_ HANDLE h, _Out_opt_ PULONGLONG lpullBytesTransferred,
                           _Out_opt_ float *lpnThroughputKbps)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;

  if (cRundownLock.IsAcquired() == FALSE)
  {
    if (lpullBytesTransferred != NULL)
      *lpullBytesTransferred = 0;
    if (lpnThroughputKbps != NULL)
      *lpnThroughputKbps = 0.0f;
    return MX_E_Cancelled;
  }

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
  {
    if (lpullBytesTransferred != NULL)
      *lpullBytesTransferred = 0;
    if (lpnThroughputKbps != NULL)
      *lpnThroughputKbps = 0.0f;
    return E_INVALIDARG;
  }

  //done
  cConn->cReadStats.Get(lpullBytesTransferred, lpnThroughputKbps);
  return S_OK;
}

HRESULT CIpc::GetWriteStats(_In_ HANDLE h, _Out_opt_ PULONGLONG lpullBytesTransferred,
                            _Out_opt_ float *lpnThroughputKbps)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;

  if (cRundownLock.IsAcquired() == FALSE)
  {
    if (lpullBytesTransferred != NULL)
      *lpullBytesTransferred = 0;
    if (lpnThroughputKbps != NULL)
      *lpnThroughputKbps = 0.0f;
    return MX_E_Cancelled;
  }

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
  {
    if (lpullBytesTransferred != NULL)
      *lpullBytesTransferred = 0;
    if (lpnThroughputKbps != NULL)
      *lpnThroughputKbps = 0.0f;
    return E_INVALIDARG;
  }

  //done
  cConn->cWriteStats.Get(lpullBytesTransferred, lpnThroughputKbps);
  return S_OK;
}

HRESULT CIpc::GetErrorCode(_In_ HANDLE h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;

  //get error code
  return __InterlockedRead(&(cConn->hrErrorCode));
}

HRESULT CIpc::PauseInputProcessing(_In_ HANDLE h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;

  //set flag
  _InterlockedOr(&(cConn->nFlags), FLAG_InputProcessingPaused);

  //done
  return S_OK;
}

HRESULT CIpc::ResumeInputProcessing(_In_ HANDLE h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;
  HRESULT hRes;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;

  //clear flag
  hRes = S_OK;
  if ((_InterlockedAnd(&(cConn->nFlags), ~FLAG_InputProcessingPaused) & FLAG_InputProcessingPaused) != 0)
  {
    hRes = cConn->SendResumeIoProcessingPacket(TRUE);
  }

  //done
  return hRes;
}

HRESULT CIpc::PauseOutputProcessing(_In_ HANDLE h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;

  //set flag
  _InterlockedOr(&(cConn->nFlags), FLAG_OutputProcessingPaused);

  //done
  return S_OK;
}

HRESULT CIpc::ResumeOutputProcessing(_In_ HANDLE h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;
  HRESULT hRes;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;

  //clear flag
  hRes = S_OK;
  if ((_InterlockedAnd(&(cConn->nFlags), ~FLAG_OutputProcessingPaused) & FLAG_OutputProcessingPaused) != 0)
  {
    hRes = cConn->SendResumeIoProcessingPacket(FALSE);
  }

  //done
  return hRes;
}

HRESULT CIpc::SetCallbacks(_In_ HANDLE h, _In_ CHANGE_CALLBACKS_DATA &cCallbacks)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;

  //set new callback
  if (cCallbacks.bConnectCallbackIsValid != 0)
  {
    cConn->cConnectCallback = cCallbacks.cConnectCallback;
  }
  if (cCallbacks.bDisconnectCallbackIsValid != 0)
  {
    cConn->cDisconnectCallback = cCallbacks.cDisconnectCallback;
  }
  if (cCallbacks.bDataReceivedCallbackIsValid != 0)
  {
    cConn->cDataReceivedCallback = cCallbacks.cDataReceivedCallback;
  }
  if (cCallbacks.bDestroyCallbackIsValid != 0)
  {
    cConn->cDestroyCallback = cCallbacks.cDestroyCallback;
  }
  if (cCallbacks.bUserDataIsValid != 0)
  {
    cConn->cUserData = cCallbacks.cUserData;
  }
  //done
  return S_OK;
}

CIpc::CUserData* CIpc::GetUserData(_In_ HANDLE h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;
  CUserData *lpUserData;

  if (cRundownLock.IsAcquired() == FALSE)
    return NULL;

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return NULL;

  //get user data
  lpUserData = cConn->cUserData;
  if (lpUserData != NULL)
    lpUserData->AddRef();
  //done
  return lpUserData;
}

CIpc::eConnectionClass CIpc::GetClass(_In_ HANDLE h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;

  if (cRundownLock.IsAcquired() == FALSE)
    return CIpc::ConnectionClassError;

  //lookup connection
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return CIpc::ConnectionClassError;

  //check class
  switch (cConn->nClass)
  {
    case CIpc::ConnectionClassClient:
    case CIpc::ConnectionClassServer:
    case CIpc::ConnectionClassListener:
      return cConn->nClass;
  }

  //done
  return CIpc::ConnectionClassUnknown;
}

BOOL CIpc::IsShuttingDown()
{
  return cShuttingDownEv.Wait(0);
}

//-----------------------------------------------------------

VOID CIpc::InternalFinalize()
{
  CConnectionBase *lpConn;

  RundownProt_WaitForRelease(&nRundownProt);
  cShuttingDownEv.Set();

  //close all connections
  do
  {
    lpConn = NULL;

    {
      CAutoSlimRWLShared cConnListLock(&(sConnections.sRwMutex));
      CRedBlackTree::Iterator it;

      for (CRedBlackTreeNode *lpNode = it.Begin(sConnections.cTree); lpNode != NULL; lpNode = it.Next())
      {
        CConnectionBase *_lpConn = CONTAINING_RECORD(lpNode, CConnectionBase, cTreeNode);

        if ((_InterlockedOr(&(_lpConn->nFlags), FLAG_ClosingOnShutdown) & FLAG_ClosingOnShutdown) == 0)
        {
          if (_lpConn->SafeAddRef() > 0)
          {
            lpConn = _lpConn;
            break;
          }
        }
      }
    }
    if (lpConn != NULL)
    {
      lpConn->Close(MX_E_Cancelled);
      lpConn->ShutdownLink(TRUE);
      lpConn->Release();
    }
  }
  while (lpConn != NULL);

  //wait all connections to be destroyed
  while (__InterlockedRead(&(sConnections.nCount)) > 0)
  {
    ::MxSleep(50);
  }
  MX_ASSERT(sConnections.cTree.IsEmpty() != FALSE);

  //finish internal stuff
  OnInternalFinalize();

  //remove free packets
  sFreePackets32768.cList.DiscardAll();
  sFreePackets4096.cList.DiscardAll();

  //done
  cShuttingDownEv.Close();
  RundownProt_Initialize(&nRundownProt);
  return;
}

VOID CIpc::FireOnEngineError(_In_ HRESULT hrErrorCode)
{
  if (cEngineErrorCallback)
    cEngineErrorCallback(this, hrErrorCode);
  return;
}

HRESULT CIpc::FireOnCreate(_In_ CConnectionBase *lpConn)
{
  HRESULT hRes = S_OK;

  if (lpConn->cCreateCallback)
  {
    CREATE_CALLBACK_DATA sCallbackData = {
      lpConn->cConnectCallback, lpConn->cDisconnectCallback,
      lpConn->cDataReceivedCallback, lpConn->cDestroyCallback,
      lpConn->cUserData
    };

    hRes = lpConn->cCreateCallback(this, (HANDLE)lpConn, sCallbackData);
  }
  return hRes;
}

VOID CIpc::FireOnDestroy(_In_ CConnectionBase *lpConn)
{
  TAutoRefCounted<CUserData> cUserData(lpConn->cUserData);

  if (lpConn->cDestroyCallback)
    lpConn->cDestroyCallback(this, (HANDLE)lpConn, lpConn->cUserData, __InterlockedRead(&(lpConn->hrErrorCode)));
  return;
}

HRESULT CIpc::FireOnConnect(_In_ CConnectionBase *lpConn)
{
  HRESULT hRes = S_OK;

  if (lpConn->IsClosed() == FALSE)
  {
    if (lpConn->cConnectCallback)
      hRes = lpConn->cConnectCallback(this, (HANDLE)lpConn, lpConn->cUserData);
  }
  return hRes;
}

VOID CIpc::FireOnDisconnect(_In_ CConnectionBase *lpConn)
{
  TAutoRefCounted<CUserData> cUserData(lpConn->cUserData);

  if (lpConn->cDisconnectCallback)
    lpConn->cDisconnectCallback(this, (HANDLE)lpConn, lpConn->cUserData, __InterlockedRead(&(lpConn->hrErrorCode)));
  return;
}

HRESULT CIpc::FireOnDataReceived(_In_ CConnectionBase *lpConn)
{
  CCriticalSection::CAutoLock cOnDataReceivedLock(lpConn->cOnDataReceivedCS);
  TAutoRefCounted<CUserData> cUserData(lpConn->cUserData);

  if (!(lpConn->cDataReceivedCallback))
    return E_NOTIMPL;
  return lpConn->cDataReceivedCallback(this, lpConn, lpConn->cUserData);
}

CIpc::CPacketBase* CIpc::GetPacket(_In_ CConnectionBase *lpConn, _In_ CPacketBase::eType nType,
                                   _In_ SIZE_T nDesiredSize, _In_ BOOL bRealSize)
{
  CPacketBase *lpPacket;

  if (bRealSize != FALSE)
    nDesiredSize += sizeof(CPacketBase);

  if (nDesiredSize <= 4096)
  {
    CFastLock cLock(&(sFreePackets4096.nMutex));

    lpPacket = sFreePackets4096.cList.DequeueFirst();
  }
  else
  {
    CFastLock cLock(&(sFreePackets32768.nMutex));

    MX_ASSERT(nDesiredSize <= 32768);
    lpPacket = sFreePackets32768.cList.DequeueFirst();
  }
  if (lpPacket == NULL)
  {
    lpPacket = (nDesiredSize <= 4096) ? (CPacketBase*)(MX_DEBUG_NEW TPacket<4096>())
                                      : (CPacketBase *)(MX_DEBUG_NEW TPacket<32768>());
  }
  if (lpPacket != NULL)
  {
    lpPacket->Reset(nType, lpConn);
    //DebugPrint("GetPacket %lums (%lu)\n", ::GetTickCount(), nType);
  }
  return lpPacket;
}

VOID CIpc::FreePacket(_In_ CPacketBase *lpPacket)
{
  CPacketBase *lpLinkedPacket;

  while (lpPacket != NULL)
  {
    //DebugPrint("FreePacket: Ovr=0x%p\n", lpPacket->GetOverlapped());

    lpLinkedPacket = lpPacket->GetLinkedPacket();

    if (lpPacket->GetClassSize() <= 4096)
    {
      CFastLock cLock(&(sFreePackets4096.nMutex));

      if (sFreePackets4096.cList.GetCount() < MAXIMUM_FREE_PACKETS_IN_LIST)
      {
        //We don't care if count becomes greater than 'nMaxItemsInList' because several threads
        //are freeing packets and list grows beyond the limit
        lpPacket->Reset(CPacketBase::TypeDiscard, NULL);
        sFreePackets4096.cList.QueueLast(lpPacket);

        //free linked packets if they belong to the same class
        while (lpLinkedPacket != NULL && lpLinkedPacket->GetClassSize() <= 4096 &&
               sFreePackets4096.cList.GetCount() < MAXIMUM_FREE_PACKETS_IN_LIST)
        {
          lpPacket = lpLinkedPacket;
          lpLinkedPacket = lpLinkedPacket->GetLinkedPacket();

          lpPacket->Reset(CPacketBase::TypeDiscard, NULL);
          sFreePackets4096.cList.QueueLast(lpPacket);
        }

        lpPacket = NULL;
      }
    }
    else
    {
      CFastLock cLock(&(sFreePackets32768.nMutex));

      if (sFreePackets32768.cList.GetCount() < MAXIMUM_FREE_PACKETS_IN_LIST)
      {
        //We don't care if count becomes greater than 'nMaxItemsInList' because several threads
        //are freeing packets and list grows beyond the limit
        lpPacket->Reset(CPacketBase::TypeDiscard, NULL);
        sFreePackets32768.cList.QueueLast(lpPacket);

        //free linked packets if they belong to the same class
        while (lpLinkedPacket != NULL && lpLinkedPacket->GetClassSize() > 4096 &&
               sFreePackets32768.cList.GetCount() < MAXIMUM_FREE_PACKETS_IN_LIST)
        {
          lpPacket = lpLinkedPacket;
          lpLinkedPacket = lpLinkedPacket->GetLinkedPacket();

          lpPacket->Reset(CPacketBase::TypeDiscard, NULL);
          sFreePackets32768.cList.QueueLast(lpPacket);
        }

        lpPacket = NULL;
      }
    }

    //if 'lpPacket' is not NULL, delete it because list is full
    if (lpPacket != NULL)
    {
      delete lpPacket;
    }

    lpPacket = lpLinkedPacket;
  }

  //done
  return;
}

CIpc::CConnectionBase* CIpc::CheckAndGetConnection(_In_opt_ HANDLE h)
{
  if (h != NULL)
  {
    CAutoSlimRWLShared cConnListLock(&(sConnections.sRwMutex));
    CRedBlackTreeNode *lpNode;

    lpNode = sConnections.cTree.Find((SIZE_T)h, &CConnectionBase::SearchCompareFunc);
    if (lpNode != NULL)
    {
      CConnectionBase *lpConn = CONTAINING_RECORD(lpNode, CConnectionBase, cTreeNode);

      if (lpConn->SafeAddRef() > 0)
        return lpConn;
    }
  }
  return NULL;
}

VOID CIpc::OnDispatcherPacket(_In_ CIoCompletionPortThreadPool *lpPool, _In_ DWORD dwBytes, _In_ OVERLAPPED *lpOvr,
                              _In_ HRESULT hRes)
{
  CPacketList cQueuedPacketsList;
  CConnectionBase *lpConn;
  CPacketBase *lpPacket;
  HRESULT hRes2;
  LPVOID lpOrigOverlapped;
  CPacketBase::eType nOrigOverlappedType;

  lpPacket = CPacketBase::FromOverlapped(lpOvr);

  MX::LFence();
  lpConn = lpPacket->GetConn();

  {
    CFastLock cListLock(&(lpConn->sInUsePackets.nMutex));

    lpConn->sInUsePackets.cList.Remove(lpPacket);
  }

start:
  if (OnPreprocessPacket(dwBytes, lpPacket, hRes) != FALSE)
    goto process_queued_packets;

  lpConn = lpPacket->GetConn();
  lpConn->AddRef();

  lpOrigOverlapped = lpPacket->GetOverlapped();
  nOrigOverlappedType = lpPacket->GetType();
  if (ShouldLog(1) != FALSE)
  {
    lpConn->cLogTimer.Mark();
    Log(L"CIpc::OnDispatcherPacket) Clock=%lums / Conn=0x%p / Ovr=0x%p / Type=%lu / Bytes=%lu / Err=0x%08X",
        lpConn->cLogTimer.GetElapsedTimeMs(), lpConn, lpPacket->GetOverlapped(), lpPacket->GetType(), dwBytes,
        ((hRes == MX_E_OperationAborted && lpConn->IsClosed() != FALSE) ? S_OK : hRes));
    lpConn->cLogTimer.ResetToLastMark();
  }

  switch (lpPacket->GetType())
  {
    case CPacketBase::TypeInitialSetup:
      //free packet
      FreePacket(lpPacket);

      _InterlockedOr(&(lpConn->nFlags), FLAG_InitialSetupExecuted);

      //fire main connect
      if (SUCCEEDED(hRes))
      {
        hRes = FireOnConnect(lpConn);

        //execute ssl startup if enabled
        if (SUCCEEDED(hRes))
        {
          hRes = lpConn->HandleSslStartup();
        }

        //send read a-head
        if (SUCCEEDED(hRes))
        {
          hRes = (bDoZeroReads != FALSE && ZeroReadsSupported() != FALSE)
                 ? lpConn->DoZeroRead((SIZE_T)dwReadAhead, cQueuedPacketsList)
                 : lpConn->DoRead((SIZE_T)dwReadAhead, NULL, cQueuedPacketsList);
        }
      }
      break;

    case CPacketBase::TypeZeroRead:
      if (SUCCEEDED(hRes) || hRes == MX_E_MoreData)
      {
        //NOTE: if packet is not reused, DoRead will free it
        hRes = lpConn->DoRead(1, lpPacket, cQueuedPacketsList);
      }
      else
      {
        //free packet
        FreePacket(lpPacket);
      }
      MX_ASSERT_ALWAYS(_InterlockedDecrement(&(lpConn->nIncomingReads)) >= 0);
      break;

    case CPacketBase::TypeRead:
      if (SUCCEEDED(hRes))
      {
        lpPacket->SetBytesInUse(dwBytes);
        if (dwBytes > 0)
        {
          lpConn->cReadStats.Update(dwBytes);

          //move packet to read list
          {
            CFastLock cListLock(&(lpConn->sReadPackets.nMutex));

            lpConn->sReadPackets.cList.QueueSorted(lpPacket);
          }

          //process queued packets if input processing is not paused
          if ((__InterlockedRead(&(lpConn->nFlags)) & FLAG_InputProcessingPaused) == 0)
          {
            //get next sequenced block
            hRes = lpConn->HandleIncomingPackets();
            if (SUCCEEDED(hRes))
              hRes = lpConn->HandleOutgoingPackets();
            if (SUCCEEDED(hRes))
            {
              //setup a new read-ahead
              if (lpConn->IsClosedOrGracefulShutdown() == FALSE)
              {
                hRes = (bDoZeroReads != FALSE && ZeroReadsSupported() != FALSE)
                       ? lpConn->DoZeroRead(1, cQueuedPacketsList) : lpConn->DoRead(1, NULL, cQueuedPacketsList);
              }
            }
          }
        }
        else
        {
          //free packet
          FreePacket(lpPacket);

          //empty packet?, discard
          if ((_InterlockedOr(&lpConn->nFlags, FLAG_GracefulShutdown) & FLAG_GracefulShutdown) == 0 &&
              ShouldLog(1) != FALSE)
          {
            lpConn->cLogTimer.Mark();
            Log(L"CIpc::GracefulShutdown A) Clock=%lums / This=0x%p", lpConn->cLogTimer.GetElapsedTimeMs(), lpConn);
            lpConn->cLogTimer.ResetToLastMark();
          }

          //do a graceful shutdown if not initiated yet
          lpConn->Close(S_OK);
        }
      }
      else
      {
        //free packet
        FreePacket(lpPacket);
      }

      //done
      MX_ASSERT_ALWAYS(_InterlockedDecrement(&(lpConn->nIncomingReads)) >= 0);
      break;

    case CPacketBase::TypeWriteRequest:
      if (SUCCEEDED(hRes))
      {
        {
          CFastLock cListLock(&(lpConn->sPendingWritePackets.nMutex));

          lpConn->sPendingWritePackets.cList.QueueSorted(lpPacket);
        }

        //NOTE: we don't decrement outgoing writes because packet is pending
        hRes = lpConn->HandleOutgoingPackets();
      }
      else
      {
        //free packet
        FreePacket(lpPacket);

        lpConn->DecrementOutgoingWrites();
      }
      break;

    case CPacketBase::TypeWrite:
      if (SUCCEEDED(hRes))
      {
        MX_ASSERT(dwBytes != 0);
        if (dwBytes == lpPacket->GetUserDataDW())
        {
          lpConn->cWriteStats.Update(dwBytes);
        }
        else
        {
          hRes = MX_E_WriteFault;
        }
      }

      MX_ASSERT_ALWAYS((DWORD)__InterlockedRead(&(lpConn->nOutgoingBytes)) >= lpPacket->GetUserDataDW());
      _InterlockedExchangeAdd(&(lpConn->nOutgoingBytes), -((LONG)(lpPacket->GetUserDataDW())));

      //free packet
      FreePacket(lpPacket);

      //check for pending write packets
      if (SUCCEEDED(hRes))
      {
        hRes = lpConn->HandleOutgoingPackets();
      }

      lpConn->DecrementOutgoingWrites();
      break;

    case CPacketBase::TypeDiscard:
      //ignore
      MX_ASSERT(FALSE);
      break;

    case CPacketBase::TypeResumeIoProcessing:
      {
      BOOL bIsRead = (lpPacket->GetOrder() == 1) ? TRUE : FALSE;

      FreePacket(lpPacket);

      hRes = S_OK;
      while (SUCCEEDED(hRes) &&
             (__InterlockedRead(&(lpConn->nFlags)) & FLAG_InputProcessingPaused) == 0 &&
             lpConn->IsClosedOrGracefulShutdown() == FALSE &&
             (ULONG)__InterlockedRead(&(lpConn->nIncomingReads)) < dwReadAhead)
      {
        hRes = (bDoZeroReads != FALSE && ZeroReadsSupported() != FALSE)
               ? lpConn->DoZeroRead((SIZE_T)dwReadAhead, cQueuedPacketsList)
               : lpConn->DoRead((SIZE_T)dwReadAhead, NULL, cQueuedPacketsList);
      }
      if (SUCCEEDED(hRes))
      {
        {
          CFastLock cRecBufLock(&(lpConn->sReceivedData.nMutex));

          if (lpConn->sReceivedData.cBuffer.GetAvailableForRead() > 0)
          {
            _InterlockedOr(&(lpConn->nFlags), FLAG_NewReceivedDataAvailable);
          }
        }

        if (bIsRead != FALSE)
        {
          //check pending read packets
          hRes = lpConn->HandleIncomingPackets();
          if (SUCCEEDED(hRes))
            hRes = lpConn->HandleOutgoingPackets();
        }
        else
        {
          //else check pending write packets
          hRes = lpConn->HandleOutgoingPackets();
        }
      }
      }
      break;

    default:
      hRes = OnCustomPacket(dwBytes, lpPacket, hRes);
      break;
  }

  //new message queued? graceful closing?
  if (lpConn->IsClosedOrGracefulShutdown() == FALSE)
  {
    if ((_InterlockedAnd(&(lpConn->nFlags), ~FLAG_NewReceivedDataAvailable) & FLAG_NewReceivedDataAvailable) != 0)
    {
      hRes2 = FireOnDataReceived(lpConn);
      if (SUCCEEDED(hRes))
        hRes = hRes2;
    }
  }
  else if (__InterlockedRead(&(lpConn->nIncomingReads)) == 0 && __InterlockedRead(&(lpConn->nOutgoingWrites)) == 0)
  {
    if ((_InterlockedOr(&(lpConn->nFlags), FLAG_ShutdownProcessed) & FLAG_ShutdownProcessed) == 0)
    {
      BOOL bDataAvailable;

      if (lpConn->IsClosed() == FALSE)
        lpConn->Close(S_OK);

      //wait until another message is processed
      lpConn->cOnDataReceivedCS.Lock();
      lpConn->cOnDataReceivedCS.Unlock();

      //shutdown SSL if active
      lpConn->HandleSslShutdown();

      //check if some received data left
      {
        CFastLock cRecBufLock(&(lpConn->sReceivedData.nMutex));

        bDataAvailable = (lpConn->sReceivedData.cBuffer.GetAvailableForRead() > 0) ? TRUE : FALSE;
      }
      //send notifications is connection was previously 'connected'
      if (lpConn->IsConnected() != FALSE)
      {
        if (SUCCEEDED(hRes) && bDataAvailable != FALSE)
        {
          hRes2 = FireOnDataReceived(lpConn);
          if (FAILED(hRes2) && SUCCEEDED((HRESULT)__InterlockedRead(&(lpConn->hrErrorCode))))
            _InterlockedExchange(&(lpConn->hrErrorCode), (LONG)hRes2);
        }
        //----
        FireOnDisconnect(lpConn);
      }
    }
  }

  //on error terminate connection
  if (FAILED(hRes))
  {
    if (hRes == MX_HRESULT_FROM_WIN32(ERROR_NETNAME_DELETED) ||
        hRes == HRESULT_FROM_NT(STATUS_LOCAL_DISCONNECT) ||
        hRes == HRESULT_FROM_NT(STATUS_REMOTE_DISCONNECT))
    {
      BOOL bLog;

      bLog = ((_InterlockedOr(&lpConn->nFlags, FLAG_GracefulShutdown) & FLAG_GracefulShutdown) == 0) ? TRUE : FALSE;
      if (bLog != FALSE && ShouldLog(1) != FALSE)
      {
        lpConn->cLogTimer.Mark();
        Log(L"CIpc::GracefulShutdown C) Clock=%lums / This=0x%p / Res=0x%08X", lpConn->cLogTimer.GetElapsedTimeMs(),
            this, hRes);
        lpConn->cLogTimer.ResetToLastMark();
      }
      hRes = S_OK;
    }
    lpConn->Close(hRes);
  }

  //release connection reference
  lpConn->Release();
  if (ShouldLog(1) != FALSE)
  {
    lpConn->cLogTimer.Mark();
    Log(L"CIpc::OnDispatcherPacket) Clock=%lums / Conn=0x%p / Ovr=0x%p / Type=%lu / Err=0x%08X [EXIT]",
        lpConn->cLogTimer.GetElapsedTimeMs(), lpConn, lpOrigOverlapped, nOrigOverlappedType,
        ((hRes == MX_E_OperationAborted && lpConn->IsClosed() != FALSE) ? S_OK : hRes));
    lpConn->cLogTimer.ResetToLastMark();
  }
  lpConn->Release();

process_queued_packets:
  lpPacket = cQueuedPacketsList.DequeueFirst();
  if (lpPacket != NULL)
  {
    lpOvr = lpPacket->GetOverlapped();
    dwBytes = lpPacket->GetBytesInUse();
    hRes = S_OK;
    goto start;
  }

  //done
  return;
}

BOOL CIpc::OnPreprocessPacket(_In_ DWORD dwBytes, _In_ CPacketBase *lpPacket, _In_ HRESULT hRes)
{
  return FALSE;
}

} //namespace MX
