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
#include "..\..\Include\Comm\IpcCommon.h"
#include "..\..\Include\ArrayList.h"
#include "..\..\Include\StackTrace.h"
#include <intrin.h>

#pragma intrinsic(_InterlockedExchangeAdd)

//-----------------------------------------------------------

#define CHECKTAG1                               0x6F7E283CUL
#define CHECKTAG2                               0x3A9B4D8EUL

#define FLAG_Connected                                0x0001
#define FLAG_Closed                                   0x0002
#define FLAG_ShutdownProcessed                        0x0004
#define FLAG_NewReceivedDataAvailable                 0x0008
#define FLAG_GracefulShutdown                         0x0010
#define FLAG_InSendTransaction                        0x0020
#define FLAG_ClosingOnShutdown                        0x0040
#define FLAG_InitialSetupExecuted                     0x0080
#define FLAG_InputProcessingPaused                    0x0100
#define FLAG_OutputProcessingPaused                   0x0200

#define __EPSILON                                   0.00001f

//-----------------------------------------------------------

namespace MX {

CIpc::CIpc(_In_ CIoCompletionPortThreadPool &_cDispatcherPool) : CBaseMemObj(), CLoggable(),
                                                                 cDispatcherPool(_cDispatcherPool)
{
  dwReadAhead = 4;
  bDoZeroReads = TRUE;
  dwMaxOutgoingBytes = 16 * 32768;
  //----
  cDispatcherPoolPacketCallback = MX_BIND_MEMBER_CALLBACK(&CIpc::OnDispatcherPacket, this);
  FastLock_Initialize(&nInitShutdownMutex);
  RundownProt_Initialize(&nRundownProt);
  cEngineErrorCallback = NullCallback();
  SlimRWL_Initialize(&(sConnections.sRwMutex));
  return;
}

CIpc::~CIpc()
{
  MX_ASSERT(cShuttingDownEv.Get() == NULL);
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
    if (dwMaxOutgoingBytes < 1024)
      dwMaxOutgoingBytes = 1024;
    else if (dwMaxOutgoingBytes > 0x01000000)
      dwMaxOutgoingBytes = 0x01000000;
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
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;
  //send real message
  if (cConn->IsClosed() == FALSE)
  {
    CAutoSlimRWLShared cLayersLock(&(cConn->sLayers.sRwMutex));
    CLnkLstNode *lpNode = cConn->sLayers.cList.GetTail();

    return cConn->SendMsg(lpMsg, nMsgSize, ((lpNode != NULL) ? CONTAINING_RECORD(lpNode, CLayer, cListNode) : NULL));
  }
  hRes = cConn->GetErrorCode();
  return (SUCCEEDED(hRes)) ? E_FAIL : hRes;
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
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;
  //send real message
  if (cConn->IsClosed() == FALSE)
    return cConn->SendStream(lpStream);
  hRes = cConn->GetErrorCode();
  return (SUCCEEDED(hRes)) ? E_FAIL : hRes;
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
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;
  //add signal
  if (cConn->IsClosed() == FALSE)
    return cConn->AfterWriteSignal(cCallback, lpCookie);
  hRes = cConn->GetErrorCode();
  return (SUCCEEDED(hRes)) ? E_FAIL : hRes;
}

CIpc::CMultiSendLock* CIpc::StartMultiSendBlock(_In_ HANDLE h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;
  TAutoDeletePtr<CIpc::CMultiSendLock> cMultiSendBlock;

  if (cRundownLock.IsAcquired() == FALSE)
    return NULL;
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return NULL;
  cMultiSendBlock.Attach(MX_DEBUG_NEW CMultiSendLock());
  if (!cMultiSendBlock)
    return NULL;
  //apply lock
  while ((_InterlockedOr(&(cConn->nFlags), FLAG_InSendTransaction) & FLAG_InSendTransaction) != 0)
    _YieldProcessor();
  cMultiSendBlock->lpConn = cConn.Detach();
  //done
  return cMultiSendBlock.Detach();
}

HRESULT CIpc::GetBufferedMessage(_In_ HANDLE h, _Out_ LPVOID lpMsg, _Inout_ SIZE_T *lpnMsgSize)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  if (lpMsg == NULL || lpnMsgSize == NULL)
    return E_POINTER;
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
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;
  //close connection
  cConn->Close(hrErrorCode);
  //done
  return S_OK;
}

HRESULT CIpc::IsConnected(_In_ HANDLE h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
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

HRESULT CIpc::AddLayer(_In_ HANDLE h, _In_ CLayer *lpLayer, _In_opt_ BOOL bFront)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;
  HRESULT hRes;

  if (lpLayer == NULL)
    return E_POINTER;
  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;
  //add layer
  hRes = S_OK;
  {
    CAutoSlimRWLExclusive cLayersLock(&(cConn->sLayers.sRwMutex));

    if (bFront != FALSE)
      cConn->sLayers.cList.PushHead(&(lpLayer->cListNode));
    else
      cConn->sLayers.cList.PushTail(&(lpLayer->cListNode));
    lpLayer->lpConn = cConn.Get();

    //call OnConnect if needed
    if ((__InterlockedRead(&(cConn->nFlags)) & FLAG_InitialSetupExecuted) != 0)
    {
      hRes = lpLayer->OnConnect();
      if (FAILED(hRes))
        lpLayer->cListNode.Remove();
    }
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
  BOOL b;

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
  do
  {
    {
      CAutoSlimRWLShared cConnListLock(&(sConnections.sRwMutex));

      b = sConnections.cTree.IsEmpty();
    }
    if (b == FALSE)
      ::MxSleep(10);
  }
  while (b == FALSE);

  //finish internal stuff
  OnInternalFinalize();

  //remove free packets
  cFreePacketsList32768.DiscardAll();
  cFreePacketsList4096.DiscardAll();
  cFreePacketsList512.DiscardAll();

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
    if (SUCCEEDED(hRes))
    {
      CLnkLst::Iterator it;
      CLnkLstNode *lpNode;

      for (lpNode = it.Begin(lpConn->sLayers.cList); lpNode != NULL; lpNode = it.Next())
      {
        CLayer *lpLayer = CONTAINING_RECORD(lpNode, CLayer, cListNode);

        lpLayer->lpConn = lpConn;
      }
    }
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
  CPacketList *lpList;

  if (bRealSize != FALSE)
    nDesiredSize += sizeof(CPacketBase);

  if (nDesiredSize <= 512)
  {
    lpList = &cFreePacketsList512;
  }
  else if (nDesiredSize <= 4096)
  {
    lpList = &cFreePacketsList4096;
  }
  else
  {
    MX_ASSERT(nDesiredSize <= 32768);
    lpList = &cFreePacketsList32768;
  }

  lpPacket = lpList->DequeueFirst();
  if (lpPacket == NULL)
  {
    if (nDesiredSize <= 512)
    {
      lpPacket = MX_DEBUG_NEW TPacket<512>();
    }
    else if (nDesiredSize <= 4096)
    {
      lpPacket = MX_DEBUG_NEW TPacket<4096>();
    }
    else
    {
      lpPacket = MX_DEBUG_NEW TPacket<32768>();
    }
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
  //DebugPrint("FreePacket: Ovr=0x%p\n", lpPacket->GetOverlapped());
  CPacketList *lpList;
  SIZE_T nMaxItemsInList;
  CPacketBase *lpNextPacket;

  do
  {
    lpNextPacket = lpPacket->GetChainedPacket();

    if (lpPacket->GetClassSize() <= 512)
    {
      lpList = &cFreePacketsList512;
      nMaxItemsInList = 4096;
    }
    else if (lpPacket->GetClassSize() <= 4096)
    {
      lpList = &cFreePacketsList4096;
      nMaxItemsInList = 4096;
    }
    else
    {
      MX_ASSERT(lpPacket->GetClassSize() <= 32768);
      lpList = &cFreePacketsList512;
      nMaxItemsInList = 4096;
    }
    if (lpList->GetCount() < nMaxItemsInList)
    {
      //We don't care if count becomes greater than 'nMaxItemsInList' because several threads
      //are freeing packets and list grows beyond the limit
      lpPacket->Reset(CPacketBase::TypeDiscard, NULL);
      lpList->QueueLast(lpPacket);
    }
    else
    {
      delete lpPacket;
    }

    lpPacket = lpNextPacket;
  }
  while (lpPacket != NULL);
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
  BOOL bQueueNewRead, bJumpFromResumeInputProcessing;

  lpPacket = CPacketBase::FromOverlapped(lpOvr);

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
      if (SUCCEEDED(hRes))
      {
        hRes = (bDoZeroReads != FALSE && ZeroReadsSupported() != FALSE)
               ? lpConn->DoZeroRead((SIZE_T)dwReadAhead, cQueuedPacketsList)
               : lpConn->DoRead((SIZE_T)dwReadAhead, NULL, cQueuedPacketsList);
      }

      //notify all layers about connection
      {
        CAutoSlimRWLShared cLayersLock(&(lpConn->sLayers.sRwMutex));
        CLnkLstNode *lpNode;

        lpNode = lpConn->sLayers.cList.GetTail();
        while (SUCCEEDED(hRes) && lpNode != NULL)
        {
          CLayer *lpLayer = CONTAINING_RECORD(lpNode, CLayer, cListNode);

          hRes = lpLayer->OnConnect();
          if (SUCCEEDED(hRes))
            lpNode = lpNode->GetPrev();
        }
      }
      _InterlockedOr(&(lpConn->nFlags), FLAG_InitialSetupExecuted);

      //fire main connect
      if (SUCCEEDED(hRes))
        hRes = FireOnConnect(lpConn);

      //free packet
      lpConn->cRwList.Remove(lpPacket);
      FreePacket(lpPacket);
      break;

    case CPacketBase::TypeZeroRead:
      lpConn->cRwList.Remove(lpPacket);
      if (SUCCEEDED(hRes) || hRes == MX_E_MoreData)
      {
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
      bQueueNewRead = FALSE;
      if (SUCCEEDED(hRes))
      {
        lpPacket->SetBytesInUse(dwBytes);
        if (dwBytes > 0)
        {
          lpConn->cReadStats.Update(dwBytes);

          bQueueNewRead = TRUE;
          //move packet to readed list
          lpConn->cRwList.Remove(lpPacket);
          lpConn->cReadedList.QueueSorted(lpPacket);
          //----
          bJumpFromResumeInputProcessing = FALSE;

          //process queued packets if input processing is not paused
check_pending_read_req:
          if ((__InterlockedRead(&(lpConn->nFlags)) & FLAG_InputProcessingPaused) == 0)
          {
            //get next sequenced block
            while (SUCCEEDED(hRes) && IsShuttingDown() == FALSE)
            {
              lpPacket = lpConn->cReadedList.Dequeue(__InterlockedRead(&(lpConn->nNextReadOrderToProcess)));
              if (lpPacket != NULL)
              {
                {
                  CAutoSlimRWLShared cLayersLock(&(lpConn->sLayers.sRwMutex));
                  CLnkLstNode *lpNode;

                  lpNode = lpConn->sLayers.cList.GetHead();
                  if (lpNode != NULL)
                  {
                    CLayer *lpLayer = CONTAINING_RECORD(lpNode, CLayer, cListNode);

                    hRes = lpLayer->OnReceivedData(lpPacket->GetBuffer(), (SIZE_T)(lpPacket->GetBytesInUse()));
                  }
                  else
                  {
                    CFastLock cRecBufLock(&(lpConn->sReceivedData.nMutex));

                    hRes = lpConn->sReceivedData.cBuffer.Write(lpPacket->GetBuffer(),
                                                               (SIZE_T)(lpPacket->GetBytesInUse()));
                    _InterlockedOr(&(lpConn->nFlags), FLAG_NewReceivedDataAvailable);
                  }
                }
                FreePacket(lpPacket);
                _InterlockedIncrement(&(lpConn->nNextReadOrderToProcess));
              }
              else
              {
                break;
              }
            }
          }
          else
          {
            //if input is paused, do not queue a read request
            bQueueNewRead = FALSE;
          }
          if (bJumpFromResumeInputProcessing != FALSE)
            break;
        }
        else
        {
          BOOL bLog;

          bLog = ((_InterlockedOr(&lpConn->nFlags, FLAG_GracefulShutdown) & FLAG_GracefulShutdown) == 0) ? TRUE : FALSE;
          if (bLog != FALSE && ShouldLog(1) != FALSE)
          {
            lpConn->cLogTimer.Mark();
            Log(L"CIpc::GracefulShutdown A) Clock=%lums / This=0x%p", lpConn->cLogTimer.GetElapsedTimeMs(), lpConn);
            lpConn->cLogTimer.ResetToLastMark();
          }

          //free packet
          lpConn->cRwList.Remove(lpPacket);
          FreePacket(lpPacket);
        }
      }
      else
      {
        //free packet
        lpConn->cRwList.Remove(lpPacket);
        FreePacket(lpPacket);
      }

      //setup a new read-ahead
      if (SUCCEEDED(hRes) && bQueueNewRead != FALSE && lpConn->IsClosedOrGracefulShutdown() == FALSE)
      {
        hRes = (bDoZeroReads != FALSE && ZeroReadsSupported() != FALSE)
               ? lpConn->DoZeroRead(1, cQueuedPacketsList) : lpConn->DoRead(1, NULL, cQueuedPacketsList);
      }

      //done
      MX_ASSERT_ALWAYS(_InterlockedDecrement(&(lpConn->nIncomingReads)) >= 0);
      break;

    case CPacketBase::TypeWriteRequest:
      lpConn->cRwList.Remove(lpPacket);
      lpConn->cWritePendingList.QueueSorted(lpPacket);

check_pending_write_req:
      //increment outstanding writes do keep the connection open while the chain is built
      lpConn->IncrementOutgoingWrites();

      //process queued packets if output processing is not paused
      while (SUCCEEDED(hRes) && (__InterlockedRead(&(lpConn->nFlags)) & FLAG_OutputProcessingPaused) == 0 &&
             (DWORD)__InterlockedRead(&(lpConn->nOutgoingBytes)) < dwMaxOutgoingBytes)
      {
        CPacketBase *lpFirstPacket, *lpLastPacket;
        BOOL bAdvanceToNextPacket;
        ULONG nPacketOrder;
        SIZE_T nChainLength;

        //get next sequenced block to write
        lpPacket = lpConn->cWritePendingList.Dequeue((ULONG)__InterlockedRead(&(lpConn->nNextWriteOrderToProcess)));
        if (lpPacket == NULL)
          break;

        //process an after-write signal
        if (lpPacket->HasAfterWriteSignalCallback() != FALSE)
        {
          //if some chain must be written, requeue and wait
          if (__InterlockedRead(&(lpConn->nOutgoingBytes)) == 0)
          {
            //call callback
            TAutoRefCounted<CUserData> cUserData(lpConn->cUserData);

            lpPacket->InvokeAfterWriteSignalCallback(this, cUserData);
            //free packet
            FreePacket(lpPacket);

            //decrement the outstanding writes incremented by the send
            lpConn->DecrementOutgoingWrites();

            //go to next packet
            _InterlockedIncrement(&(lpConn->nNextWriteOrderToProcess));
            continue; //loop
          }

          //if some chain must be written or there still exists packets being sent, requeue and wait
          lpConn->cWritePendingList.QueueFirst(lpPacket);

          if (__InterlockedRead(&(lpConn->nOutgoingBytes)) == 0)
            continue;
          break;
        }

        //start processing a series of packets
        lpFirstPacket = lpLastPacket = NULL;
        nChainLength = 0;

write_req_process_packet:
        bAdvanceToNextPacket = FALSE;
        nPacketOrder = lpPacket->GetOrder();

        //process a stream?
        if (lpPacket->HasStream() != FALSE)
        {
          hRes = S_OK;
          while (SUCCEEDED(hRes) && nChainLength < lpConn->GetMultiWriteMaxCount() &&
                  (DWORD)__InterlockedRead(&(lpConn->nOutgoingBytes)) < dwMaxOutgoingBytes)
          {
            CPacketBase *lpNewPacket;

            hRes = lpConn->ReadStream(lpPacket, &lpNewPacket);

            //end of stream reached?
            if (hRes == MX_E_EndOfFileReached)
            {
              //reached the end of the stream so free "stream" packet
              FreePacket(lpPacket);
              lpPacket = NULL;

              //go to next packet
              bAdvanceToNextPacket = TRUE;

              hRes = S_OK;
              break;
            }
            if (FAILED(hRes))
              break;

            //add to chain
            if (lpFirstPacket == NULL)
              lpFirstPacket = lpNewPacket;
            else
              lpLastPacket->ChainPacket(lpNewPacket);
            lpLastPacket = lpNewPacket;
            nChainLength++;
          }

          //at this point we reached the end of the stream, fulfilled the max outgoing bytes or an error was raised
        }
        else
        {
          //add to chain
          if (lpFirstPacket == NULL)
            lpFirstPacket = lpPacket;
          else
            lpLastPacket->ChainPacket(lpPacket);
          lpLastPacket = lpPacket;
          nChainLength++;

          lpPacket = NULL;
          hRes = S_OK;

          //go to next packet
          bAdvanceToNextPacket = TRUE;
        }

        //let's try to partially flush the chain of packets
        if (SUCCEEDED(hRes) && nChainLength > 0)
        {
          hRes = lpConn->SendPackets(&lpFirstPacket, &lpLastPacket, &nChainLength, FALSE, cQueuedPacketsList);
        }

        if (bAdvanceToNextPacket != FALSE)
        {
          //decrement the outstanding writes incremented by the send
          lpConn->DecrementOutgoingWrites();

          //try to process the next one if we have room
          if (SUCCEEDED(hRes) && (DWORD)__InterlockedRead(&(lpConn->nOutgoingBytes)) < dwMaxOutgoingBytes &&
              nChainLength < lpConn->GetMultiWriteMaxCount())
          {
            //get next packet
            lpPacket = lpConn->cWritePendingList.Dequeue(nPacketOrder + 1);
            if (lpPacket != NULL)
            {
              //if it is an after write signal, do not use and requeue it
              if (lpPacket->HasAfterWriteSignalCallback() != FALSE)
              {
                lpConn->cWritePendingList.QueueFirst(lpPacket);
                lpPacket = NULL;
              }
            }

            //if we have a valid packet we can process
            if (lpPacket != NULL)
            {
              _InterlockedIncrement(&(lpConn->nNextWriteOrderToProcess));
              goto write_req_process_packet;
            }
          }
        }

        //at this point we cannot process more packets due to a fulfilled chain or an error

        //first flush completely the chain
        if (SUCCEEDED(hRes) && nChainLength > 0)
        {
          hRes = lpConn->SendPackets(&lpFirstPacket, &lpLastPacket, &nChainLength, TRUE, cQueuedPacketsList);
        }

        //free chained packets left. if no error here, then the chain should be empty
        MX_ASSERT(FAILED(hRes) || lpFirstPacket == NULL);
        if (lpFirstPacket != NULL)
          FreePacket(lpFirstPacket);

        //at this point, if we have a packet, requeue it
        if (lpPacket != NULL)
        {
          MX_ASSERT(lpPacket->HasStream() != FALSE || lpPacket->HasAfterWriteSignalCallback() != FALSE);

          lpConn->cWritePendingList.QueueFirst(lpPacket);
          lpPacket = NULL;
        }
        else
        {
          _InterlockedIncrement(&(lpConn->nNextWriteOrderToProcess));
        }
      }

      //decrement the outstanding write incremented at the beginning of this section and close if needed
      lpConn->DecrementOutgoingWrites();
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

      lpConn->DecrementOutgoingWrites();

      //free packet
      lpConn->cRwList.Remove(lpPacket);
      FreePacket(lpPacket);

      //check for pending write packets
      if (SUCCEEDED(hRes))
        goto check_pending_write_req;
      break;

    case CPacketBase::TypeDiscard:
      //ignore
      MX_ASSERT(FALSE);
      break;

    case CPacketBase::TypeResumeIoProcessing:
      lpConn->cRwList.Remove(lpPacket);

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
      if (FAILED(hRes))
      {
        FreePacket(lpPacket);
        break;
      }

      {
        CFastLock cRecBufLock(&(lpConn->sReceivedData.nMutex));

        if (lpConn->sReceivedData.cBuffer.GetAvailableForRead() > 0)
        {
          _InterlockedOr(&(lpConn->nFlags), FLAG_NewReceivedDataAvailable);
        }
      }

      if (lpPacket->GetOrder() == 1)
      {
        //check pending read packets
        FreePacket(lpPacket);
        bJumpFromResumeInputProcessing = TRUE;
        goto check_pending_read_req;
      }

      //else check pending write packets
      FreePacket(lpPacket);
      goto check_pending_write_req;

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
      //notify all layers about disconnection
      {
        CAutoSlimRWLShared cLayersLock(&(lpConn->sLayers.sRwMutex));
        CLnkLstNode *lpNode;

        lpNode = lpConn->sLayers.cList.GetHead();
        while (lpNode != NULL)
        {
          CLayer *lpLayer = CONTAINING_RECORD(lpNode, CLayer, cListNode);

          hRes2 = lpLayer->OnDisconnect();
          if (FAILED(hRes2) && SUCCEEDED((HRESULT)__InterlockedRead(&(lpConn->hrErrorCode))))
            _InterlockedExchange(&(lpConn->hrErrorCode), (LONG)hRes2);

          lpNode = lpNode->GetNext();
        }
      }
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
  //done
process_queued_packets:
  lpPacket = cQueuedPacketsList.DequeueFirst();
  if (lpPacket != NULL)
  {
    lpPacket->GetConn()->cRwList.QueueLast(lpPacket);
    lpOvr = lpPacket->GetOverlapped();
    dwBytes = lpPacket->GetBytesInUse();
    hRes = S_OK;
    goto start;
  }

  return;
}

BOOL CIpc::OnPreprocessPacket(_In_ DWORD dwBytes, _In_ CPacketBase *lpPacket, _In_ HRESULT hRes)
{
  return FALSE;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CIpc::CConnectionBase::CConnectionBase(_In_ CIpc *_lpIpc, _In_ CIpc::eConnectionClass _nClass) :
                       TRefCounted<CBaseMemObj>()
{
  lpIpc = _lpIpc;
  nClass = _nClass;
  _InterlockedExchange(&hrErrorCode, S_OK);
  _InterlockedExchange(&nFlags, 0);
  _InterlockedExchange(&nNextReadOrder, 0);
  _InterlockedExchange(&nNextReadOrderToProcess, 1);
  _InterlockedExchange(&nNextWriteOrder, 0);
  _InterlockedExchange(&nNextWriteOrderToProcess, 1);
  _InterlockedExchange(&nIncomingReads, 0);
  _InterlockedExchange(&nOutgoingWrites, 0);
  _InterlockedExchange(&nOutgoingBytes, 0);
  cCreateCallback = NullCallback();
  cDestroyCallback = NullCallback();
  cConnectCallback = NullCallback();
  cDisconnectCallback = NullCallback();
  cDataReceivedCallback = NullCallback();
  FastLock_Initialize(&nReadMutex);
  SlimRWL_Initialize(&(sLayers.sRwMutex));
  FastLock_Initialize(&(sReceivedData.nMutex));
  return;
}

CIpc::CConnectionBase::~CConnectionBase()
{
  CPacketBase *lpPacket;
  CLnkLstNode *lpNode;

  {
    CAutoSlimRWLExclusive cConnListLock(&(lpIpc->sConnections.sRwMutex));

    cTreeNode.Remove();
  }
  lpIpc->FireOnDestroy(this);

  //free packets
  while ((lpPacket = cRwList.DequeueFirst()) != NULL)
    lpIpc->FreePacket(lpPacket);
  while ((lpPacket = cReadedList.DequeueFirst()) != NULL)
    lpIpc->FreePacket(lpPacket);
  while ((lpPacket = cWritePendingList.DequeueFirst()) != NULL)
    lpIpc->FreePacket(lpPacket);
  //destroy layers
  while ((lpNode = sLayers.cList.PopHead()) != NULL)
  {
    CLayer *lpLayer = CONTAINING_RECORD(lpNode, CLayer, cListNode);

    delete lpLayer;
  }
  return;
}

VOID CIpc::CConnectionBase::ShutdownLink(_In_ BOOL bAbortive)
{
  //call the shutdown for all layers
  CAutoSlimRWLShared cLayersLock(&(sLayers.sRwMutex));
  CLnkLst::Iterator it;
  CLnkLstNode *lpNode;

  for (lpNode = it.Begin(sLayers.cList); lpNode != NULL; lpNode = it.Next())
  {
    CLayer *lpLayer = CONTAINING_RECORD(lpNode, CLayer, cListNode);

    lpLayer->OnShutdown();
  }
  return;
}

HRESULT CIpc::CConnectionBase::SendMsg(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize, _In_opt_ CLayer *lpToLayer)
{
  CPacketBase *lpPacket;
  LPBYTE s;
  DWORD dwToSendThisRound;
  HRESULT hRes;

  if (nDataSize == 0)
    return S_OK;
  if (lpData == NULL)
    return E_OUTOFMEMORY;
  if (nDataSize > 0x7FFFFFFF)
    return E_INVALIDARG;
  //do write
  s = (LPBYTE)lpData;
  while (nDataSize > 0)
  {
    //get a free buffer
    lpPacket = lpIpc->GetPacket(this, CIpc::CPacketBase::TypeWriteRequest, ((nDataSize > 8192) ? 32768 : 4096), FALSE);
    if (lpPacket == NULL)
      return E_OUTOFMEMORY;
    //fill buffer
    dwToSendThisRound = lpPacket->GetBufferSize();
    if (nDataSize < (SIZE_T)dwToSendThisRound)
      dwToSendThisRound = (DWORD)nDataSize;
    lpPacket->SetBytesInUse(dwToSendThisRound);
    MxMemCopy(lpPacket->GetBuffer(), s, (SIZE_T)dwToSendThisRound);
    if (lpIpc->ShouldLog(1) != FALSE)
    {
      cLogTimer.Mark();
      lpIpc->Log(L"CIpc::SendMsg) Clock=%lums / Ovr=0x%p / Type=%lu / Bytes=%lu", cLogTimer.GetElapsedTimeMs(),
                 lpPacket->GetOverlapped(), lpPacket->GetType(), lpPacket->GetBytesInUse());
      cLogTimer.ResetToLastMark();
    }
    //send packet
    if (lpToLayer != NULL)
    {
      hRes = lpToLayer->OnSendPacket(lpPacket);
      if (FAILED(hRes))
        return hRes;
    }
    else
    {
      //prepare
      lpPacket->SetOrder(_InterlockedIncrement(&nNextWriteOrder));
      cRwList.QueueLast(lpPacket);
      AddRef();
      _InterlockedIncrement(&nOutgoingWrites);
      hRes = lpIpc->cDispatcherPool.Post(lpIpc->cDispatcherPoolPacketCallback, 0, lpPacket->GetOverlapped());
      if (FAILED(hRes))
      {
        MX_ASSERT_ALWAYS(__InterlockedRead(&nOutgoingWrites) >= 1);
        _InterlockedDecrement(&nOutgoingWrites);
        Release();
        return hRes;
      }
    }
    //next block
    s += (SIZE_T)dwToSendThisRound;
    nDataSize -= (SIZE_T)dwToSendThisRound;
  }
  return S_OK;
}

HRESULT CIpc::CConnectionBase::SendStream(_In_ CStream *lpStream)
{
  CPacketBase *lpPacket;
  HRESULT hRes;

  MX_ASSERT(lpStream != NULL);
  //get a free buffer
  lpPacket = lpIpc->GetPacket(this, CIpc::CPacketBase::TypeWriteRequest, 0, FALSE);
  if (lpPacket == NULL)
    return E_OUTOFMEMORY;
  lpPacket->SetStream(lpStream);
  //send packet
  {
    CAutoSlimRWLShared cLayersLock(&(sLayers.sRwMutex));
    CLnkLstNode *lpNode;

    lpNode = sLayers.cList.GetTail();
    if (lpNode != NULL)
    {
      CLayer *lpLayer = CONTAINING_RECORD(lpNode, CLayer, cListNode);

      hRes = lpLayer->OnSendPacket(lpPacket);
    }
    else
    {
      //prepare
      lpPacket->SetOrder(_InterlockedIncrement(&nNextWriteOrder));
      cRwList.QueueLast(lpPacket);
      AddRef();
      _InterlockedIncrement(&nOutgoingWrites);
      if (lpIpc->ShouldLog(1) != FALSE)
      {
        cLogTimer.Mark();
        lpIpc->Log(L"CIpc::SendStream) Clock=%lums / Ovr=0x%p / Type=%lu", cLogTimer.GetElapsedTimeMs(),
                   lpPacket->GetOverlapped(), lpPacket->GetType());
        cLogTimer.ResetToLastMark();
      }
      hRes = lpIpc->cDispatcherPool.Post(lpIpc->cDispatcherPoolPacketCallback, 0, lpPacket->GetOverlapped());
      if (FAILED(hRes))
      {
        _InterlockedDecrement(&nOutgoingWrites);
        Release();
      }
    }
  }
  return hRes;
}

HRESULT CIpc::CConnectionBase::AfterWriteSignal(_In_ OnAfterWriteSignalCallback cCallback, _In_opt_ LPVOID lpCookie)
{
  CPacketBase *lpPacket;
  HRESULT hRes;

  MX_ASSERT(cCallback != false);
  //get a free buffer
  lpPacket = lpIpc->GetPacket(this, CIpc::CPacketBase::TypeWriteRequest, 0, FALSE);
  if (lpPacket == NULL)
  return E_OUTOFMEMORY;
  lpPacket->SetAfterWriteSignalCallback(cCallback);
  lpPacket->SetUserData(lpCookie);
  {
    CAutoSlimRWLShared cLayersLock(&(sLayers.sRwMutex));
    CLnkLstNode *lpNode;

    lpNode = sLayers.cList.GetTail();
    if (lpNode != NULL)
    {
      CLayer *lpLayer = CONTAINING_RECORD(lpNode, CLayer, cListNode);

      hRes = lpLayer->OnSendPacket(lpPacket);
    }
    else
    {
      hRes = AfterWriteSignal(lpPacket);
    }
  }
  if (FAILED(hRes))
   FreePacket(lpPacket);
  return hRes;
}

HRESULT CIpc::CConnectionBase::AfterWriteSignal(_In_ CPacketBase *lpPacket)
{
  HRESULT hRes;

  //send packet
  lpPacket->SetOrder(_InterlockedIncrement(&nNextWriteOrder));
  cRwList.QueueLast(lpPacket);
  AddRef();
  _InterlockedIncrement(&nOutgoingWrites);
  if (lpIpc->ShouldLog(1) != FALSE)
  {
    cLogTimer.Mark();
    lpIpc->Log(L"CIpc::AfterWriteSignal) Clock=%lums / Conn=0x%p / Ovr=0x%p / Type=%lu", cLogTimer.GetElapsedTimeMs(),
               this, lpPacket->GetOverlapped(), lpPacket->GetType());
    cLogTimer.ResetToLastMark();
  }
  hRes = lpIpc->cDispatcherPool.Post(lpIpc->cDispatcherPoolPacketCallback, 0, lpPacket->GetOverlapped());
  if (FAILED(hRes))
  {
    _InterlockedDecrement(&nOutgoingWrites);
    Release();
  }
  //done
  return hRes;
}

HRESULT CIpc::CConnectionBase::SendResumeIoProcessingPacket(_In_ BOOL bInput)
{
  CPacketBase *lpPacket;
  HRESULT hRes;

  //get a free buffer
  lpPacket = lpIpc->GetPacket(this, CIpc::CPacketBase::TypeResumeIoProcessing, 0, FALSE);
  if (lpPacket == NULL)
    return E_OUTOFMEMORY;
  //prepare
  lpPacket->SetOrder((bInput != FALSE) ? 1 : 0);
  cRwList.QueueLast(lpPacket);
  AddRef();
  if (lpIpc->ShouldLog(1) != FALSE)
  {
    cLogTimer.Mark();
    lpIpc->Log(L"CIpc::SendResumeIoProcessingPacket[%s]) Clock=%lums / Ovr=0x%p / Type=%lu",
               ((bInput != FALSE) ? L"Input" : L"Output"), cLogTimer.GetElapsedTimeMs(), lpPacket->GetOverlapped(),
               lpPacket->GetType());
    cLogTimer.ResetToLastMark();
  }
  hRes = lpIpc->cDispatcherPool.Post(lpIpc->cDispatcherPoolPacketCallback, 0, lpPacket->GetOverlapped());
  if (FAILED(hRes))
  {
    cRwList.Remove(lpPacket);
    lpIpc->FreePacket(lpPacket);
    Release();
  }
  return hRes;
}

VOID CIpc::CConnectionBase::Close(_In_ HRESULT hRes)
{
  LONG nInitVal, nOrigVal, nNewVal;

  //mark as closed
  nOrigVal = __InterlockedRead(&nFlags);
  do
  {
    nInitVal = nOrigVal;
    nNewVal = nInitVal | FLAG_Closed;
    if (SUCCEEDED(hRes) && (nInitVal & FLAG_Closed) == 0)
      nNewVal |= FLAG_GracefulShutdown;
    nOrigVal = _InterlockedCompareExchange(&nFlags, nNewVal, nInitVal);
  }
  while (nOrigVal != nInitVal);
  if ((nInitVal & FLAG_Closed) == 0)
  {
    _InterlockedCompareExchange(&hrErrorCode, hRes, S_OK);
  }
  if ((hRes & 0x0F000000) != 0)
  {
    SIZE_T nStack[10];

    StackTrace::Get(nStack, 10);
    for (SIZE_T i = 0; i < 10; i++)
    {
      if (nStack[i] == 0)
        break;
      DebugPrint("Close stack #%i: 0x%IX\n", i + 1, nStack[i]);
    }
  }
  if (hRes != S_OK || __InterlockedRead(&nOutgoingWrites) == 0)
    ShutdownLink(FAILED(__InterlockedRead(&hrErrorCode)));
  //when closing call the "final" release to remove from list
  if ((nInitVal & FLAG_Closed) == 0)
  {
    if (lpIpc->ShouldLog(1) != FALSE)
    {
      cLogTimer.Mark();
      lpIpc->Log(L"CIpc::Close) Clock=%lums / This=0x%p / Res=0x%08X", cLogTimer.GetElapsedTimeMs(), this, hRes);
      cLogTimer.ResetToLastMark();
    }
    Release();
  }
  return;
}

BOOL CIpc::CConnectionBase::IsGracefulShutdown() const
{
  return ((__InterlockedRead(&(const_cast<CIpc::CConnectionBase*>(this)->nFlags)) & FLAG_GracefulShutdown) != 0)
         ? TRUE : FALSE;
}

BOOL CIpc::CConnectionBase::IsClosed() const
{
  return ((__InterlockedRead(&(const_cast<CIpc::CConnectionBase*>(this)->nFlags)) & FLAG_Closed) != 0) ? TRUE : FALSE;
}

BOOL CIpc::CConnectionBase::IsClosedOrGracefulShutdown() const
{
  return ((__InterlockedRead(&(const_cast<CIpc::CConnectionBase*>(this)->nFlags)) &
           (FLAG_GracefulShutdown | FLAG_Closed)) != 0) ? TRUE : FALSE;
}

BOOL CIpc::CConnectionBase::IsConnected() const
{
  return ((__InterlockedRead(&(const_cast<CIpc::CConnectionBase*>(this)->nFlags)) & FLAG_Connected) != 0)
         ? TRUE : FALSE;
}

CIpc* CIpc::CConnectionBase::GetIpc() const
{
  return lpIpc;
}

HRESULT CIpc::CConnectionBase::GetErrorCode() const
{
  return (HRESULT)__InterlockedRead(&(const_cast<CIpc::CConnectionBase*>(this)->hrErrorCode));
}

HRESULT CIpc::CConnectionBase::HandleConnected()
{
  CPacketBase *lpPacket;
  HRESULT hRes;

  //mark as connected
  _InterlockedOr(&nFlags, FLAG_Connected);
  //initialize read/write stats
  cReadStats.HandleConnected();
  cWriteStats.HandleConnected();
  //send initial setup packet
  lpPacket = lpIpc->GetPacket(this, CPacketBase::TypeInitialSetup, 0, FALSE);
  if (lpPacket == NULL)
    return E_OUTOFMEMORY;
  cRwList.QueueLast(lpPacket);
  AddRef();
  if (lpIpc->ShouldLog(1) != FALSE)
  {
    cLogTimer.Mark();
    lpIpc->Log(L"CIpc::HandleConnected) Clock=%lums / Ovr=0x%p / Type=%lu", cLogTimer.GetElapsedTimeMs(),
               lpPacket->GetOverlapped(), lpPacket->GetType());
    cLogTimer.ResetToLastMark();
  }
  hRes = lpIpc->cDispatcherPool.Post(lpIpc->cDispatcherPoolPacketCallback, 0, lpPacket->GetOverlapped());
  if (FAILED(hRes))
  {
    cRwList.Remove(lpPacket);
    FreePacket(lpPacket);
    Release();
    return hRes;
  }
  //done
  return S_OK;
}

VOID CIpc::CConnectionBase::IncrementOutgoingWrites()
{
  _InterlockedIncrement(&nOutgoingWrites);
  return;
}

VOID CIpc::CConnectionBase::DecrementOutgoingWrites()
{
  MX_ASSERT_ALWAYS(__InterlockedRead(&nOutgoingWrites) >= 1);
  if (_InterlockedDecrement(&nOutgoingWrites) == 0 && IsClosed() != FALSE)
    ShutdownLink(FAILED(__InterlockedRead(&hrErrorCode)));
  return;
}

CIpc::CPacketBase* CIpc::CConnectionBase::GetPacket(_In_ CPacketBase::eType nType, _In_ SIZE_T nDesiredSize,
                                                    _In_ BOOL bRealSize)
{
  return lpIpc->GetPacket(this, nType, nDesiredSize, bRealSize);
}

VOID CIpc::CConnectionBase::FreePacket(_In_ CPacketBase *lpPacket)
{
  lpIpc->FreePacket(lpPacket);
  return;
}

CIoCompletionPortThreadPool& CIpc::CConnectionBase::GetDispatcherPool()
{
  return lpIpc->cDispatcherPool;
}

CIoCompletionPortThreadPool::OnPacketCallback& CIpc::CConnectionBase::GetDispatcherPoolPacketCallback()
{
  return lpIpc->cDispatcherPoolPacketCallback;
}

HRESULT CIpc::CConnectionBase::DoZeroRead(_In_ SIZE_T nPacketsCount, _Inout_ CPacketList &cQueuedPacketsList)
{
  CPacketBase *lpPacket;
  DWORD dwRead;
  HRESULT hRes;

  while (nPacketsCount > 0)
  {
    if (IsClosed() != FALSE)
      return MX_E_Cancelled;
    lpPacket = lpIpc->GetPacket(this, CPacketBase::TypeZeroRead, 4096, FALSE);
    if (lpPacket == NULL)
      return E_OUTOFMEMORY;
    cRwList.QueueLast(lpPacket);
    _InterlockedIncrement(&nIncomingReads);
    AddRef();
    hRes = SendReadPacket(lpPacket, &dwRead);
    switch (hRes)
    {
      case S_FALSE:
        {
        BOOL bLog;

        MX_ASSERT_ALWAYS(_InterlockedDecrement(&nIncomingReads) >= 0);
        bLog = ((_InterlockedOr(&nFlags, FLAG_GracefulShutdown) & FLAG_GracefulShutdown) == 0) ? TRUE : FALSE;
        if (bLog != FALSE && lpIpc->ShouldLog(1) != FALSE)
        {
          cLogTimer.Mark();
          lpIpc->Log(L"CIpc::GracefulShutdown E) Clock=%lums / This=0x%p", cLogTimer.GetElapsedTimeMs(), this);
          cLogTimer.ResetToLastMark();
        }
        //free packet
        cRwList.Remove(lpPacket);
        FreePacket(lpPacket);
        Release();
        }
        return S_OK;

      case S_OK:
        lpPacket->SetBytesInUse(dwRead);
        cRwList.Remove(lpPacket);
        cQueuedPacketsList.QueueLast(lpPacket);
        break;

      case 0x80070000 | ERROR_IO_PENDING:
        break;

      default:
        MX_ASSERT_ALWAYS(_InterlockedDecrement(&nIncomingReads) >= 0);
        Release();
        return hRes;
    }
    nPacketsCount--;
  }
  //done
  return S_OK;
}

HRESULT CIpc::CConnectionBase::DoRead(_In_ SIZE_T nPacketsCount, _In_opt_ CPacketBase *lpReusePacket,
                                      _Inout_ CPacketList &cQueuedPacketsList)
{
  CPacketBase *lpPacket;
  DWORD dwRead;
  HRESULT hRes;

  while (nPacketsCount > 0)
  {
    if (IsClosed() != FALSE)
    {
      if (lpReusePacket != NULL)
        FreePacket(lpReusePacket);
      return MX_E_Cancelled;
    }
    if (lpReusePacket != NULL)
    {
      lpReusePacket->Reset(CPacketBase::TypeRead, this);
      lpPacket = lpReusePacket;
      lpReusePacket = NULL;
    }
    else
    {
      lpPacket = lpIpc->GetPacket(this, CPacketBase::TypeRead, 4096, FALSE);
      if (lpPacket == NULL)
        return E_OUTOFMEMORY;
    }
    lpPacket->SetBytesInUse(lpPacket->GetBufferSize());
    _InterlockedIncrement(&nIncomingReads);
    AddRef();
    {
      CFastLock cReadLock(&nReadMutex);

      lpPacket->SetOrder(_InterlockedIncrement(&nNextReadOrder));
      cRwList.QueueLast(lpPacket);
      hRes = SendReadPacket(lpPacket, &dwRead);
    }
    switch (hRes)
    {
      case S_FALSE:
        {
        BOOL bLog;

        MX_ASSERT_ALWAYS(_InterlockedDecrement(&nIncomingReads) >= 0);
        bLog = ((_InterlockedOr(&nFlags, FLAG_GracefulShutdown) & FLAG_GracefulShutdown) == 0) ? TRUE : FALSE;
        if (bLog != FALSE && lpIpc->ShouldLog(1) != FALSE)
        {
          cLogTimer.Mark();
          lpIpc->Log(L"CIpc::GracefulShutdown F) Clock=%lums / This=0x%p", cLogTimer.GetElapsedTimeMs(), this);
          cLogTimer.ResetToLastMark();
        }

        //free packet
        cRwList.Remove(lpPacket);
        FreePacket(lpPacket);
        Release();
        }
        return S_OK;

      case S_OK:
        lpPacket->SetBytesInUse(dwRead);
        cRwList.Remove(lpPacket);
        cQueuedPacketsList.QueueLast(lpPacket);
        break;

      case 0x80070000 | ERROR_IO_PENDING:
        break;

      default:
        MX_ASSERT_ALWAYS(_InterlockedDecrement(&nIncomingReads) >= 0);
        Release();
        return hRes;
    }
    nPacketsCount--;
  }
  //done
  return S_OK;
}

HRESULT CIpc::CConnectionBase::ReadStream(_In_ CPacketBase *lpStreamPacket, _Out_ CPacketBase **lplpPacket)
{
  SIZE_T nRead;
  HRESULT hRes;

  *lplpPacket = GetPacket(CPacketBase::TypeWrite, 32768, FALSE);
  if ((*lplpPacket) == NULL)
    return E_OUTOFMEMORY;

  //read from stream
  nRead = 0;
  hRes = lpStreamPacket->ReadStream((*lplpPacket)->GetBuffer(), (SIZE_T)((*lplpPacket)->GetBufferSize()), nRead);
  if (SUCCEEDED(hRes) && nRead > (SIZE_T)((*lplpPacket)->GetBufferSize()))
  {
    MX_ASSERT(FALSE);
    FreePacket(*lplpPacket);
    *lplpPacket = NULL;
    return MX_E_InvalidData;
  }

  //error?
  if (FAILED(hRes) && hRes != MX_E_EndOfFileReached)
  {
    FreePacket(*lplpPacket);
    *lplpPacket = NULL;
    return hRes;
  }

  //end of stream reached?
  if (nRead == 0)
  {
    FreePacket(*lplpPacket);
    *lplpPacket = NULL;
    return MX_E_EndOfFileReached;
  }

  //done
  (*lplpPacket)->SetBytesInUse((DWORD)nRead);
  return S_OK;
}

HRESULT CIpc::CConnectionBase::SendPackets(_Inout_ CPacketBase **lplpFirstPacket, _Inout_ CPacketBase **lplpLastPacket,
                                           _Inout_ SIZE_T *lpnChainLength, _In_ BOOL bFlushAll,
                                           _Inout_ CPacketList &cQueuedPacketsList)
{
  CPacketBase *lpCurrPacket, *lpPrevPacket, *lpChainStart;
  DWORD dwTotalBytes, dwWritten;
  SIZE_T nToExtract, nMultiWriteMaxCount;
  HRESULT hRes;

  nMultiWriteMaxCount = GetMultiWriteMaxCount();
  if (bFlushAll == FALSE && (*lpnChainLength) < nMultiWriteMaxCount)
    return S_OK;

  hRes = S_OK;
  while ((*lpnChainLength) > 0)
  {
    if (IsClosed() != FALSE)
      return MX_E_Cancelled;

    //extract packets for this round
    lpChainStart = lpPrevPacket = (*lplpFirstPacket);
    lpCurrPacket = lpChainStart->GetChainedPacket();

    nToExtract = ((*lpnChainLength) < nMultiWriteMaxCount) ? (*lpnChainLength) : nMultiWriteMaxCount;
    for (SIZE_T i = nToExtract; i > 1; i--)
    {
      lpPrevPacket = lpCurrPacket;
      lpCurrPacket = lpCurrPacket->GetChainedPacket();
    }
    lpPrevPacket->ChainPacket(NULL);

    //set the next round
    *lplpFirstPacket = lpCurrPacket;
    if (lpCurrPacket == NULL)
      *lplpLastPacket = NULL;
    *lpnChainLength -= nToExtract;

    //calculate the number of bytes to send in this round
    dwTotalBytes = 0;
    for (lpCurrPacket = lpChainStart; lpCurrPacket != NULL; lpCurrPacket = lpCurrPacket->GetChainedPacket())
    {
      dwTotalBytes += lpCurrPacket->GetBytesInUse();
    }
    lpChainStart->SetUserDataDW(dwTotalBytes);

    //do real write
    cRwList.QueueLast(lpChainStart);
    lpChainStart->SetType(CPacketBase::TypeWrite);
    _InterlockedIncrement(&nOutgoingWrites);
    _InterlockedExchangeAdd(&nOutgoingBytes, (LONG)dwTotalBytes);
    AddRef();
    hRes = SendWritePacket(lpChainStart, &dwWritten);
    switch (hRes)
    {
      case S_FALSE:
        {
        BOOL bLog;

        _InterlockedExchangeAdd(&nOutgoingBytes, -((LONG)dwTotalBytes));
        _InterlockedDecrement(&nOutgoingWrites);
        bLog = ((_InterlockedOr(&nFlags, FLAG_GracefulShutdown) & FLAG_GracefulShutdown) == 0) ? TRUE : FALSE;
        if (bLog != FALSE && lpIpc->ShouldLog(1) != FALSE)
        {
          cLogTimer.Mark();
          lpIpc->Log(L"CIpc::GracefulShutdown B) Clock=%lums / This=0x%p", cLogTimer.GetElapsedTimeMs(), this);
          cLogTimer.ResetToLastMark();
        }
        //free packet
        cRwList.Remove(lpChainStart);
        FreePacket(lpChainStart);
        //release connection
        Release();
        }
        break;

      case S_OK:
        lpChainStart->SetBytesInUse(dwWritten);
        cRwList.Remove(lpChainStart);
        cQueuedPacketsList.QueueLast(lpChainStart);
        break;

      case 0x80070000 | ERROR_IO_PENDING:
        hRes = S_OK;
        break;

      default:
        _InterlockedExchangeAdd(&nOutgoingBytes, -((LONG)dwTotalBytes));
        _InterlockedDecrement(&nOutgoingWrites);
        //free packet
        cRwList.Remove(lpChainStart);
        FreePacket(lpChainStart);
        //release connection
        Release();
        break;
    }

    if (hRes != S_OK || bFlushAll == FALSE)
      break;
  }
  //done
  return (hRes != S_FALSE) ? hRes : S_OK;
}

HRESULT CIpc::CConnectionBase::SendReadDataToNextLayer(_In_ LPCVOID lpMsg, _In_ SIZE_T nMsgSize,
                                                       _In_ CLayer *lpCurrLayer)
{
  CLnkLstNode *lpNode;
  HRESULT hRes;

  if (nMsgSize == 0)
    return S_OK;
  MX_ASSERT(lpMsg != NULL);

  hRes = S_OK;
  lpNode = lpCurrLayer->cListNode.GetNext();
  if (lpNode != NULL)
  {
    CLayer *lpLayer = CONTAINING_RECORD(lpNode, CLayer, cListNode);

    hRes = lpLayer->OnReceivedData(lpMsg, nMsgSize);
  }
  else
  {
    CFastLock cRecBufLock(&(sReceivedData.nMutex));

    hRes = sReceivedData.cBuffer.Write(lpMsg, nMsgSize);
    _InterlockedOr(&nFlags, FLAG_NewReceivedDataAvailable);
  }
  //done
  return hRes;
}

HRESULT CIpc::CConnectionBase::SendWriteDataToNextLayer(_In_ LPCVOID lpMsg, _In_ SIZE_T nMsgSize,
                                                        _In_ CLayer *lpCurrLayer)
{
  CLnkLstNode *lpNode;
  HRESULT hRes;

  if (nMsgSize == 0)
    return S_OK;
  MX_ASSERT(lpMsg != NULL);

  hRes = S_OK;
  lpNode = lpCurrLayer->cListNode.GetPrev();
  if (lpNode != NULL)
  {
    CLayer *lpLayer = CONTAINING_RECORD(lpNode, CLayer, cListNode);

    while (nMsgSize > 0)
    {
      CPacketBase *lpPacket;
      SIZE_T nToCopy;

      lpPacket = lpIpc->GetPacket(this, CIpc::CPacketBase::TypeWriteRequest, ((nMsgSize > 8192) ? 32768 : 4096),
                                  FALSE);
      if (lpPacket == NULL)
      {
        hRes = E_OUTOFMEMORY;
        break;
      }
      nToCopy = lpPacket->GetBufferSize();
      if (nToCopy > nMsgSize)
        nToCopy = nMsgSize;
      MxMemCopy(lpPacket->GetBuffer(), lpMsg, nToCopy);
      lpPacket->SetBytesInUse((DWORD)nToCopy);

      //send packet
      hRes = lpLayer->OnSendPacket(lpPacket);
      if (FAILED(hRes))
        break;

      //advance
      lpMsg = (LPCVOID)((LPBYTE)lpMsg + nToCopy);
      nMsgSize -= nToCopy;
    }
  }
  else
  {
    hRes = SendMsg(lpMsg, nMsgSize, NULL);
  }
  //done
  return hRes;
}

HRESULT CIpc::CConnectionBase::SendAfterWritePacketToNextLayer(_In_ CPacketBase *lpPacket, _In_ CLayer *lpCurrLayer)
{
  CLnkLstNode *lpNode;
  HRESULT hRes;

  MX_ASSERT(lpPacket != NULL);
  lpNode = lpCurrLayer->cListNode.GetPrev();
  if (lpNode != NULL)
  {
    CLayer *lpLayer = CONTAINING_RECORD(lpNode, CLayer, cListNode);

    hRes = lpLayer->OnSendPacket(lpPacket);
  }
  else
  {
    hRes = AfterWriteSignal(lpPacket);
  }
  //done
  return hRes;
}

//-----------------------------------------------------------

CIpc::CConnectionBase::CReadWriteStats::CReadWriteStats() : CBaseMemObj()
{
  SlimRWL_Initialize(&sRwMutex);
  ullBytesTransferred = ullPrevBytesTransferred = 0;
  nAvgRate = 0.0;
  ::MxMemSet(nTransferRateHistory, 0, sizeof(nTransferRateHistory));
  return;
}

VOID CIpc::CConnectionBase::CReadWriteStats::HandleConnected()
{
  CAutoSlimRWLExclusive cLock(&sRwMutex);

  cTimer.Reset();
  return;
}

VOID CIpc::CConnectionBase::CReadWriteStats::Update(_In_ DWORD dwBytesTransferred)
{
  CAutoSlimRWLExclusive cLock(&sRwMutex);
  DWORD dwElapsedMs;

  ullBytesTransferred += (ULONGLONG)dwBytesTransferred;

  cTimer.Mark();
  dwElapsedMs = cTimer.GetElapsedTimeMs();
  cTimer.ResetToLastMark();

  if (dwElapsedMs >= 500)
  {
    int i, count;
    ULONGLONG ullDiffBytes;

    ullDiffBytes = ullBytesTransferred - ullPrevBytesTransferred;

    nTransferRateHistory[3] = nTransferRateHistory[1];
    nTransferRateHistory[2] = nTransferRateHistory[2];
    nTransferRateHistory[1] = nTransferRateHistory[0];
    nTransferRateHistory[0] = (float)((double)ullDiffBytes / ((double)dwElapsedMs * 1.024));

    count = 0;
    nAvgRate = 0.0f;
    for (i = 0; i < 4; i++)
    {
      if (nTransferRateHistory[i] > __EPSILON)
      {
        nAvgRate += 1.0f / nTransferRateHistory[i];
        count++;
      }
    }
    if (count > 0)
    {
      nAvgRate = (float)count / nAvgRate;
    }

    ullPrevBytesTransferred = ullBytesTransferred;
  }
  return;
}

VOID CIpc::CConnectionBase::CReadWriteStats::Get(_Out_opt_ PULONGLONG lpullBytesTransferred,
                                                 _Out_opt_ float *lpnThroughputKbps, _Out_opt_ LPDWORD lpdwTimeMarkMs)
{
  CAutoSlimRWLShared cLock(&sRwMutex);

  if (lpullBytesTransferred != NULL)
    *lpullBytesTransferred = ullBytesTransferred;
  if (lpnThroughputKbps != NULL)
    *lpnThroughputKbps = nAvgRate;
  if (lpdwTimeMarkMs != NULL)
    *lpdwTimeMarkMs = cTimer.GetStartTimeMs();
  return;
}

//-----------------------------------------------------------

HANDLE CIpc::CLayer::GetConn()
{
  return (HANDLE)lpConn;
}

VOID CIpc::CLayer::IncrementOutgoingWrites()
{
  reinterpret_cast<CIpc::CConnectionBase*>(lpConn)->IncrementOutgoingWrites();
  return;
}

VOID CIpc::CLayer::DecrementOutgoingWrites()
{
  reinterpret_cast<CIpc::CConnectionBase*>(lpConn)->DecrementOutgoingWrites();
  return;
}

VOID CIpc::CLayer::FreePacket(_In_ CPacketBase *lpPacket)
{
  reinterpret_cast<CIpc::CConnectionBase*>(lpConn)->FreePacket(lpPacket);
  return;
}

HRESULT CIpc::CLayer::ReadStream(_In_ CPacketBase *lpStreamPacket, _Out_ CPacketBase **lplpPacket)
{
  return reinterpret_cast<CIpc::CConnectionBase*>(lpConn)->ReadStream(lpStreamPacket, lplpPacket);
}

HRESULT CIpc::CLayer::SendReadDataToNextLayer(_In_ LPCVOID lpMsg, _In_ SIZE_T nMsgSize)
{
  return reinterpret_cast<CIpc::CConnectionBase*>(lpConn)->SendReadDataToNextLayer(lpMsg, nMsgSize, this);
}

HRESULT CIpc::CLayer::SendWriteDataToNextLayer(_In_ LPCVOID lpMsg, _In_ SIZE_T nMsgSize)
{
  return reinterpret_cast<CIpc::CConnectionBase*>(lpConn)->SendWriteDataToNextLayer(lpMsg, nMsgSize, this);
}

HRESULT CIpc::CLayer::SendAfterWritePacketToNextLayer(_In_ CPacketBase *lpPacket)
{
  return reinterpret_cast<CIpc::CConnectionBase*>(lpConn)->SendAfterWritePacketToNextLayer(lpPacket, this);
}

//-----------------------------------------------------------

CIpc::CMultiSendLock::CMultiSendLock() : CBaseMemObj(), CNonCopyableObj()
{
  lpConn = NULL;
  return;
}

CIpc::CMultiSendLock::~CMultiSendLock()
{
  if (lpConn != NULL)
  {
    _InterlockedAnd(&(lpConn->nFlags), ~FLAG_InSendTransaction);
    lpConn->Release();
  }
  return;
}

//-----------------------------------------------------------

CIpc::CAutoMultiSendLock::CAutoMultiSendLock(_In_ CMultiSendLock *_lpLock) : CBaseMemObj(), CNonCopyableObj()
{
  lpLock = _lpLock;
  return;
}

CIpc::CAutoMultiSendLock::~CAutoMultiSendLock()
{
  if (lpLock != NULL)
    delete lpLock;
  return;
}

} //namespace MX
