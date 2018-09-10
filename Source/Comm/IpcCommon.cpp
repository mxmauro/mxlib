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
#include "..\..\Include\Comm\IpcCommon.h"

//-----------------------------------------------------------

#define CHECKTAG1                               0x6F7E283CUL
#define CHECKTAG2                               0x3A9B4D8EUL

#define FLAG_Connected                                0x0001
#define FLAG_Closed                                   0x0002
#define FLAG_DisconnectedSent                         0x0004
#define FLAG_NewReceivedDataAvailable                 0x0008
#define FLAG_GracefulShutdown                         0x0010
#define FLAG_InSendTransaction                        0x0020
#define FLAG_ClosingOnShutDown                        0x0040
#define FLAG_InitialSetupExecuted                     0x0080
#define FLAG_InputProcessingPaused                    0x0100
#define FLAG_OutputProcessingPaused                   0x0200

//-----------------------------------------------------------

namespace MX {

#ifdef MX_IPC_DEBUG_OUTPUT
LONG CIpc::nDebugLevel = 0;
#endif //MX_IPC_DEBUG_OUTPUT

CIpc::CIpc(_In_ CIoCompletionPortThreadPool &_cDispatcherPool) : CBaseMemObj(), cDispatcherPool(_cDispatcherPool)
{
  dwPacketSize = 4096;
  dwReadAhead = 4;
  dwMaxFreePackets = 4096;
  dwMaxWriteTimeoutMs = 0;
  bDoZeroReads = TRUE;
  dwMaxOutgoingPackets = 16;
  //----
  cDispatcherPoolPacketCallback = MX_BIND_MEMBER_CALLBACK(&CIpc::OnDispatcherPacket, this);
  _InterlockedExchange(&nInitShutdownMutex, 0);
  RundownProt_Initialize(&nRundownProt);
  cEngineErrorCallback = NullCallback();
  SlimRWL_Initialize(&(sConnections.nSlimMutex));
  return;
}

CIpc::~CIpc()
{
  Finalize();
  return;
}

VOID CIpc::SetOption_PacketSize(_In_ DWORD dwSize)
{
  CFastLock cInitShutdownLock(&nInitShutdownMutex);

  if (cShuttingDownEv.Get() == NULL)
  {
    dwPacketSize = dwSize;
    if (dwPacketSize < 1024)
      dwPacketSize = 1024;
    else if (dwPacketSize > 32768)
      dwPacketSize = 32768;
  }
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

VOID CIpc::SetOption_MaxFreePacketsCount(_In_ DWORD dwCount)
{
  CFastLock cInitShutdownLock(&nInitShutdownMutex);

  if (cShuttingDownEv.Get() == NULL)
  {
    dwMaxFreePackets = dwCount;
    if (dwMaxFreePackets > 102400)
      dwMaxFreePackets = 102400;
  }
  return;
}

VOID CIpc::SetOption_MaxWriteTimeoutMs(_In_ DWORD dwTimeoutMs)
{
  CFastLock cInitShutdownLock(&nInitShutdownMutex);

  if (cShuttingDownEv.Get() == NULL)
  {
    dwMaxWriteTimeoutMs = dwTimeoutMs;
    if (dwMaxWriteTimeoutMs != 0 && dwMaxWriteTimeoutMs < 100)
      dwMaxWriteTimeoutMs = 100;
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

VOID CIpc::SetOption_OutgoingPacketsLimitCount(_In_ DWORD dwCount)
{
  CFastLock cInitShutdownLock(&nInitShutdownMutex);

  if (cShuttingDownEv.Get() == NULL)
  {
    dwMaxOutgoingPackets = dwCount;
    if (dwMaxOutgoingPackets < 2)
      dwMaxOutgoingPackets = 2;
  }
  return;
}

VOID CIpc::On(_In_ OnEngineErrorCallback _cEngineErrorCallback)
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
  return (cConn->IsClosed() == FALSE) ? cConn->SendMsg(lpMsg, nMsgSize) : E_FAIL;
}

HRESULT CIpc::SendStream(_In_ HANDLE h, _In_ CStream *lpStream)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CStream> cStream(lpStream);
  TAutoRefCounted<CConnectionBase> cConn;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  if (lpStream == NULL)
    return E_POINTER;
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;
  //send real message
  return (cConn->IsClosed() == FALSE) ? cConn->SendStream(lpStream) : E_FAIL;
}

HRESULT CIpc::AfterWriteSignal(_In_ HANDLE h, _In_ OnAfterWriteSignalCallback cCallback, _In_opt_ LPVOID lpCookie)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  if (!cCallback)
    return E_INVALIDARG;
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;
  //add signal
  return (cConn->IsClosed() == FALSE) ? cConn->AfterWriteSignal(cCallback, lpCookie) : E_FAIL;
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

HRESULT CIpc::Close(_In_opt_ HANDLE h, _In_opt_ HRESULT hErrorCode)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CConnectionBase> cConn;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;
  //close connection
  cConn->Close(hErrorCode);
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
    *lphErrorCode = __InterlockedRead(&(cConn->hErrorCode));
  return (cConn->IsClosed() != FALSE) ? S_OK : S_FALSE;
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

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return E_INVALIDARG;
  if (lpLayer == NULL)
    return E_POINTER;
  //add layer
  {
    CAutoSlimRWLExclusive cLayersLock(&(cConn->sLayers.nRwMutex));

    if (bFront != FALSE)
      cConn->sLayers.cList.PushHead(lpLayer);
    else
      cConn->sLayers.cList.PushTail(lpLayer);
    lpLayer->lpConn = cConn.Get();
  }
  //call OnConnect if needed
  if ((__InterlockedRead(&(cConn->nFlags)) & FLAG_InitialSetupExecuted) != 0)
  {
    HRESULT hRes = lpLayer->OnConnect();
    if (FAILED(hRes))
    {
      {
        CAutoSlimRWLExclusive cLayersLock(&(cConn->sLayers.nRwMutex));

        lpLayer->RemoveNode();
      }
      return hRes;
    }
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
    return ConnectionClassError;
  cConn.Attach(CheckAndGetConnection(h));
  if (!cConn)
    return ConnectionClassError;
  //check class
  switch (cConn->nClass)
  {
    case ConnectionClassClient:
    case ConnectionClassServer:
    case ConnectionClassListener:
      return cConn->nClass;
  }
  //done
  return ConnectionClassUnknown;
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
    {
      CAutoSlimRWLShared cConnListLock(&(sConnections.nSlimMutex));
      TRedBlackTree<CConnectionBase, SIZE_T>::Iterator it;

      for (lpConn=it.Begin(sConnections.cTree); lpConn!=NULL; lpConn=it.Next())
      {
        if ((_InterlockedOr(&(lpConn->nFlags), FLAG_ClosingOnShutDown) & FLAG_ClosingOnShutDown) == 0)
        {
          if (lpConn->SafeAddRef() > 0)
            break;
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
      CAutoSlimRWLShared cConnListLock(&(sConnections.nSlimMutex));

      b = sConnections.cTree.IsEmpty();
    }
    if (b == FALSE)
      MX::_YieldProcessor();
  }
  while (b == FALSE);

  //remove free packets
  cFreePacketsList.DiscardAll();
  //almost done
  OnInternalFinalize();
  cShuttingDownEv.Close();
  RundownProt_Initialize(&nRundownProt);
  return;
}

VOID CIpc::FireOnEngineError(_In_ HRESULT hErrorCode)
{
  if (cEngineErrorCallback)
    cEngineErrorCallback(this, hErrorCode);
  return;
}

HRESULT CIpc::FireOnCreate(_In_ CConnectionBase *lpConn)
{
  HRESULT hRes = S_OK;

  if (lpConn->cCreateCallback)
  {
    CREATE_CALLBACK_DATA sCallbackData = {
      lpConn->sLayers.cList,
      lpConn->cConnectCallback, lpConn->cDisconnectCallback,
      lpConn->cDataReceivedCallback, lpConn->cDestroyCallback,
      lpConn->dwWriteTimeoutMs,
      lpConn->cUserData
    };

    hRes = lpConn->cCreateCallback(this, (HANDLE)lpConn, sCallbackData);
    if (SUCCEEDED(hRes))
    {
      TLnkLst<CLayer>::Iterator it;
      CLayer *lpLayer;

      for (lpLayer=it.Begin(lpConn->sLayers.cList); lpLayer!=NULL; lpLayer=it.Next())
        lpLayer->lpConn = lpConn;
    }
  }
  return hRes;
}

VOID CIpc::FireOnDestroy(_In_ CConnectionBase *lpConn)
{
  TAutoRefCounted<CUserData> cUserData(lpConn->cUserData);

  if (lpConn->cDestroyCallback)
    lpConn->cDestroyCallback(this, (HANDLE)lpConn, lpConn->cUserData, __InterlockedRead(&(lpConn->hErrorCode)));
  return;
}

HRESULT CIpc::FireOnConnect(_In_ CConnectionBase *lpConn, _In_ HRESULT hErrorCode)
{
  HRESULT hRes = S_OK;

  if (lpConn->IsClosed() == FALSE)
  {
    if (lpConn->cConnectCallback)
    {
      hRes = lpConn->cConnectCallback(this, (HANDLE)lpConn, lpConn->cUserData, lpConn->sLayers.cList, hErrorCode);
      if (SUCCEEDED(hRes))
      {
        TLnkLst<CLayer>::Iterator it;
        CLayer *lpLayer;

        for (lpLayer=it.Begin(lpConn->sLayers.cList); lpLayer!=NULL; lpLayer=it.Next())
          lpLayer->lpConn = lpConn;
      }
    }
  }
  return hRes;
}

VOID CIpc::FireOnDisconnect(_In_ CConnectionBase *lpConn)
{
  TAutoRefCounted<CUserData> cUserData(lpConn->cUserData);

  if (lpConn->cDisconnectCallback)
    lpConn->cDisconnectCallback(this, (HANDLE)lpConn, lpConn->cUserData, __InterlockedRead(&(lpConn->hErrorCode)));
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

CIpc::CPacket* CIpc::GetPacket(_In_ CConnectionBase *lpConn, _In_ CPacket::eType nType)
{
  CPacket *lpPacket;

  lpPacket = cFreePacketsList.DequeueFirst();
  if (lpPacket == NULL)
  {
    lpPacket = MX_DEBUG_NEW CPacket(dwPacketSize);
    if (lpPacket != NULL && lpPacket->GetBuffer() == NULL)
    {
      delete lpPacket;
      lpPacket = NULL;
    }
  }
  if (lpPacket != NULL)
  {
    lpPacket->Reset(nType, lpConn);
    //DebugPrint("GetPacket %lums (%lu)\n", ::GetTickCount(), nType);
  }
  return lpPacket;
}

VOID CIpc::FreePacket(_In_ CPacket *lpPacket)
{
  //DebugPrint("FreePacket: Ovr=0x%p\n", lpPacket->GetOverlapped());
  if (cFreePacketsList.GetCount() < (SIZE_T)dwMaxFreePackets)
  {
    lpPacket->Reset(CPacket::TypeDiscard, NULL);
    cFreePacketsList.QueueLast(lpPacket);
  }
  else
  {
    delete lpPacket;
  }
  return;
}

CIpc::CConnectionBase* CIpc::CheckAndGetConnection(_In_opt_ HANDLE h)
{
  if (h != NULL)
  {
    CAutoSlimRWLShared cConnListLock(&(sConnections.nSlimMutex));
    CConnectionBase *lpConn;

    lpConn = sConnections.cTree.Find((SIZE_T)h);
    if (lpConn != NULL)
    {
      if (lpConn->SafeAddRef() > 0)
        return lpConn;
    }
  }
  return NULL;
}

VOID CIpc::OnDispatcherPacket(_In_ CIoCompletionPortThreadPool *lpPool, _In_ DWORD dwBytes, _In_ OVERLAPPED *lpOvr,
                              _In_ HRESULT hRes)
{
  CConnectionBase *lpConn;
  CPacket *lpPacket, *lpStreamPacket;
  CLayer *lpLayer;
  SIZE_T nReaded;
  HRESULT hRes2;
#ifdef MX_IPC_DEBUG_OUTPUT
  LPVOID lpOrigOverlapped;
  CPacket::eType nOrigOverlappedType;
#endif //MX_IPC_DEBUG_OUTPUT
  BOOL bQueueNewRead, bNewReadMode, bJumpFromResumeInputProcessing;

  lpPacket = CPacket::FromOverlapped(lpOvr);
  lpConn = lpPacket->GetConn();
  lpConn->AddRef();
#ifdef MX_IPC_DEBUG_OUTPUT
  lpOrigOverlapped = lpPacket->GetOverlapped();
  nOrigOverlappedType = lpPacket->GetType();
  MX_IPC_DEBUG_PRINT(1, ("%lu MX::CIpc::OnDispatcherPacket) Clock=%lums / Conn=0x%p / Ovr=0x%p / Type=%lu / "
                         "Bytes=%lu / Err=0x%08X\n", ::MxGetCurrentThreadId(), lpConn->cHiResTimer.GetElapsedTimeMs(),
                         lpConn, lpPacket->GetOverlapped(), lpPacket->GetType(), dwBytes, hRes));
#endif //MX_IPC_DEBUG_OUTPUT
  hRes2 = OnPreprocessPacket(dwBytes, lpPacket, hRes);
  if (hRes2 == S_FALSE)
  {
    switch (lpPacket->GetType())
    {
      case CPacket::TypeInitialSetup:
        if (SUCCEEDED(hRes))
          hRes =  lpConn->DoRead((SIZE_T)dwReadAhead, bDoZeroReads && ZeroReadsSupported());
        //notify all layers about connection
        {
          CAutoSlimRWLShared cLayersLock(&(lpConn->sLayers.nRwMutex));

          lpLayer = lpConn->sLayers.cList.GetTail();
        }
        while (SUCCEEDED(hRes) && lpLayer != NULL)
        {
          hRes = lpLayer->OnConnect();
          {
            CAutoSlimRWLShared cLayersLock(&(lpConn->sLayers.nRwMutex));

            lpLayer = lpLayer->GetPrevEntry();
          }
        }
        _InterlockedOr(&(lpConn->nFlags), FLAG_InitialSetupExecuted);
        //fire main connect
        if (SUCCEEDED(hRes))
          hRes = FireOnConnect(lpConn, S_OK);
        //free packet
        lpConn->cRwList.Remove(lpPacket);
        FreePacket(lpPacket);
        break;

      case CPacket::TypeZeroRead:
        lpConn->cRwList.Remove(lpPacket);
        if (SUCCEEDED(hRes) || hRes == MX_E_MoreData)
        {
          hRes = lpConn->DoRead(1, FALSE, lpPacket);
        }
        else
        {
          //free packet
          FreePacket(lpPacket);
        }
        MX_ASSERT_ALWAYS(_InterlockedDecrement(&(lpConn->nOutstandingReads)) >= 0);
        break;

      case CPacket::TypeRead:
        bQueueNewRead = bNewReadMode = FALSE;
        if (SUCCEEDED(hRes))
        {
          lpPacket->SetBytesInUse(dwBytes);
          if (dwBytes > 0)
          {
            bQueueNewRead = TRUE;
            bNewReadMode = (lpConn->IsGracefulShutdown() == FALSE && ZeroReadsSupported() != FALSE) ? bDoZeroReads :
                                                                                                      FALSE;
            //move packet to readed list
            lpConn->cRwList.Remove(lpPacket);
            lpConn->cReadedList.QueueSorted(lpPacket);
            //----
            bJumpFromResumeInputProcessing = FALSE;
            //process queued packets if input processing is not paused
check_pending_read_req:
            if ((__InterlockedRead(&(lpConn->nFlags)) & FLAG_InputProcessingPaused) == 0)
            {
              {
                CAutoSlimRWLShared cLayersLock(&(lpConn->sLayers.nRwMutex));

                lpLayer = lpConn->sLayers.cList.GetHead();
              }
              //get next sequenced block
              while (SUCCEEDED(hRes) && IsShuttingDown() == FALSE)
              {
                lpPacket = lpConn->cReadedList.Dequeue(__InterlockedRead(&(lpConn->nNextReadOrderToProcess)));
                if (lpPacket == NULL)
                  break;
                if (lpLayer != NULL)
                {
                  hRes = lpLayer->OnData(lpPacket->GetBuffer(), (SIZE_T)(lpPacket->GetBytesInUse()));
                }
                else
                {
                  CFastLock cRecBufLock(&(lpConn->sReceivedData.nMutex));

                  hRes = lpConn->sReceivedData.cBuffer.Write(lpPacket->GetBuffer(), (SIZE_T)(lpPacket->GetBytesInUse()));
                  _InterlockedOr(&(lpConn->nFlags), FLAG_NewReceivedDataAvailable);
                }
                FreePacket(lpPacket);
                _InterlockedIncrement(&(lpConn->nNextReadOrderToProcess));
              }
            }
            if (bJumpFromResumeInputProcessing != FALSE)
              break;
          }
          else
          {
            MX_IPC_DEBUG_PRINT(1, ("%lu MX::CIpc::CConnectionBase::GracefulShutdown A) Clock=%lums / This=0x%p\n",
                                   ::MxGetCurrentThreadId(), lpConn->cHiResTimer.GetElapsedTimeMs(), lpConn));
            _InterlockedOr(&(lpConn->nFlags), FLAG_GracefulShutdown);
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
          hRes = lpConn->DoRead(1, bNewReadMode);
        //done
        MX_ASSERT_ALWAYS(_InterlockedDecrement(&(lpConn->nOutstandingReads)) >= 0);
        break;

      case CPacket::TypeWriteRequest:
        lpConn->cRwList.Remove(lpPacket);
        lpConn->cWritePendingList.QueueSorted(lpPacket);
check_pending_write_req:
        //process queued packets if output processing is not paused
        if ((__InterlockedRead(&(lpConn->nFlags)) & FLAG_OutputProcessingPaused) == 0)
        {
          {
            CAutoSlimRWLShared cLayersLock(&(lpConn->sLayers.nRwMutex));

            lpLayer = lpConn->sLayers.cList.GetTail();
          }
          //loop
          while (SUCCEEDED(hRes) &&
                 (DWORD)__InterlockedRead(&(lpConn->nOutstandingWritesBeingSent)) < dwMaxOutgoingPackets)
          {
            //get next sequenced block to write
            lpPacket = lpConn->cWritePendingList.Dequeue(lpConn->nNextWriteOrderToProcess);
            if (lpPacket == NULL)
              break;
            //process an after-write signal
            if (lpPacket->HasAfterWriteSignalCallback() != FALSE)
            {
              if (__InterlockedRead(&(lpConn->nOutstandingWritesBeingSent)) == 0)
              {
                //call callback
                TAutoRefCounted<CUserData> cUserData(lpConn->cUserData);

                lpPacket->InvokeAfterWriteSignalCallback(this, cUserData);
                //free packet
                FreePacket(lpPacket);
                //go to next packet
                _InterlockedIncrement(&(lpConn->nNextWriteOrderToProcess));
                //decrement the outstanding writes incremented by the send/recheck and close if needed
                MX_ASSERT_ALWAYS(__InterlockedRead(&(lpConn->nOutstandingWrites)) >= 1);
                if (_InterlockedDecrement(&(lpConn->nOutstandingWrites)) == 0 && lpConn->IsClosed() != FALSE)
                  lpConn->ShutdownLink(FAILED(__InterlockedRead(&(lpConn->hErrorCode))));
                continue; //loop
              }
              //there still exists packets being sent so requeue and wait
              lpConn->cWritePendingList.QueueSorted(lpPacket);
              break;
            }
            //process a stream
            lpStreamPacket = NULL;
            if (lpPacket->HasStream() != FALSE)
            {
              lpStreamPacket = lpPacket;
              lpPacket = GetPacket(lpConn, CPacket::TypeWrite);
              if (lpPacket == NULL)
              {
                //dealing with an error, requeue
                //lpConn->cWritePendingList.QueueSorted(lpStreamPacket);
                lpConn->cWritePendingList.QueueFirst(lpStreamPacket);
                //set error
                hRes = E_OUTOFMEMORY;
                break;
              }
              //read from stream
              nReaded = 0;
              hRes = lpStreamPacket->ReadStream(lpPacket->GetBuffer(), (SIZE_T)dwPacketSize, nReaded);
              if (SUCCEEDED(hRes) && nReaded > (SIZE_T)dwPacketSize)
              {
                MX_ASSERT(FALSE);
                hRes = MX_E_InvalidData;
              }
              if (hRes == MX_E_EndOfFileReached)
              {
                hRes = S_OK;
              }
              else if (FAILED(hRes))
              {
                FreePacket(lpPacket);
                //dealing with an error, requeue
                //lpConn->cWritePendingList.QueueSorted(lpStreamPacket);
                lpConn->cWritePendingList.QueueFirst(lpStreamPacket);
                break;
              }
              lpPacket->SetBytesInUse((DWORD)nReaded);
              //end of stream reached?
              if (nReaded == 0)
              {
                FreePacket(lpPacket);
                //free "stream" packet
                FreePacket(lpStreamPacket);
                //go to next packet
                _InterlockedIncrement(&(lpConn->nNextWriteOrderToProcess));
                //decrement the outstanding writes incremented by the send and close if needed
                MX_ASSERT_ALWAYS(__InterlockedRead(&(lpConn->nOutstandingWrites)) >= 1);
                if (_InterlockedDecrement(&(lpConn->nOutstandingWrites)) == 0 && lpConn->IsClosed() != FALSE)
                  lpConn->ShutdownLink(FAILED(lpConn->hErrorCode));
                continue; //loop
              }
            }
            //does the connection has a layer?
            if (lpLayer != NULL)
            {
              hRes = lpLayer->OnSendMsg(lpPacket->GetBuffer(), (SIZE_T)(lpPacket->GetBytesInUse()));
              FreePacket(lpPacket);
            }
            else
            {
              //do real write
              hRes = lpConn->MarkLastWriteActivity(TRUE);
              if (SUCCEEDED(hRes))
              {
                lpConn->cRwList.QueueLast(lpPacket);
                lpPacket->SetType(CPacket::TypeWrite);
                _InterlockedIncrement(&(lpConn->nOutstandingWrites));
                _InterlockedIncrement(&(lpConn->nOutstandingWritesBeingSent));
                switch (hRes = lpConn->SendWritePacket(lpPacket))
                {
                  case S_FALSE:
                    if (_InterlockedDecrement(&(lpConn->nOutstandingWritesBeingSent)) == 0)
                      lpConn->MarkLastWriteActivity(FALSE);
                    if (_InterlockedDecrement(&(lpConn->nOutstandingWrites)) == 0 && lpConn->IsClosed() != FALSE)
                      lpConn->ShutdownLink(FAILED(__InterlockedRead(&(lpConn->hErrorCode))));
                    MX_IPC_DEBUG_PRINT(1, ("%lu MX::CIpc::CConnectionBase::GracefulShutdown B) Clock=%lums / "
                                           "This=0x%p\n", ::MxGetCurrentThreadId(),
                                           lpConn->cHiResTimer.GetElapsedTimeMs(), this));
                    _InterlockedOr(&(lpConn->nFlags), FLAG_GracefulShutdown);
                    //free packet
                    lpConn->cRwList.Remove(lpPacket);
                    FreePacket(lpPacket);
                    hRes = S_OK;
                    break;

                  case S_OK:
                  case 0x80070000 | ERROR_IO_PENDING:
                    lpConn->AddRef();
                    hRes = S_OK;
                    break;

                  default:
                    if (_InterlockedDecrement(&(lpConn->nOutstandingWritesBeingSent)) == 0)
                      lpConn->MarkLastWriteActivity(FALSE);
                    if (_InterlockedDecrement(&(lpConn->nOutstandingWrites)) == 0 && lpConn->IsClosed() != FALSE)
                      lpConn->ShutdownLink(FAILED(__InterlockedRead(&(lpConn->hErrorCode))));
                    break;
                }
              }
              else
              {
                FreePacket(lpPacket);
              }
            }
            //here, the packet was sent or discarded if an error happened
            //if packet's origin was a stream, then requeue but don't increment next write to process
            if (lpStreamPacket != NULL)
            {
              lpConn->cWritePendingList.QueueFirst(lpStreamPacket);
              //lpConn->cWritePendingList.QueueSorted(lpStreamPacket);
            }
            else
            {
              //increment next write to process if processed a non-stream packet
              _InterlockedIncrement(&(lpConn->nNextWriteOrderToProcess));
              //decrement the outstanding writes incremented by the send and close if needed
              MX_ASSERT_ALWAYS(__InterlockedRead(&(lpConn->nOutstandingWrites)) >= 1);
              if (_InterlockedDecrement(&(lpConn->nOutstandingWrites)) == 0 && lpConn->IsClosed() != FALSE)
                lpConn->ShutdownLink(FAILED(__InterlockedRead(&(lpConn->hErrorCode))));
            }
          }
        }
        break;

      case CPacket::TypeWrite:
        if (SUCCEEDED(hRes))
        {
          MX_ASSERT(dwBytes != 0);
          if (dwBytes != lpPacket->GetBytesInUse())
            hRes = MX_E_WriteFault;
        }
        //----
        MX_ASSERT_ALWAYS(__InterlockedRead(&(lpConn->nOutstandingWrites)) >= 1);
        MX_ASSERT_ALWAYS(__InterlockedRead(&(lpConn->nOutstandingWritesBeingSent)) >= 1);
        //----
        if (_InterlockedDecrement(&(lpConn->nOutstandingWritesBeingSent)) == 0)
          lpConn->MarkLastWriteActivity(FALSE);
        if (_InterlockedDecrement(&(lpConn->nOutstandingWrites)) == 0 && lpConn->IsClosed() != FALSE)
          lpConn->ShutdownLink(FAILED(__InterlockedRead(&(lpConn->hErrorCode))));
        //free packet
        lpConn->cRwList.Remove(lpPacket);
        FreePacket(lpPacket);
        //check for pending write packets
        if (SUCCEEDED(hRes))
          goto check_pending_write_req;
        break;

      case CPacket::TypeDiscard:
        //ignore
        MX_ASSERT(FALSE);
        break;

      case CPacket::TypeResumeIoProcessing:
        lpConn->cRwList.Remove(lpPacket);
        hRes = S_OK;
        if (lpPacket->GetOrder() == 1)
        {
          //check pending write packets
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
  }
  else
  {
    hRes = hRes2;
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
  else if (__InterlockedRead(&(lpConn->nOutstandingReads)) == 0 &&
           __InterlockedRead(&(lpConn->nOutstandingWrites)) == 0)
  {
    if ((_InterlockedOr(&(lpConn->nFlags), FLAG_DisconnectedSent) & FLAG_DisconnectedSent) == 0)
    {
      CLayer *lpLayer;
      BOOL bDataAvailable;

      if (lpConn->IsClosed() == FALSE)
        lpConn->Close(S_OK);
      //wait until another message is processed
      lpConn->cOnDataReceivedCS.Lock();
      lpConn->cOnDataReceivedCS.Unlock();
      //notify all layers about disconnection
      {
        CAutoSlimRWLShared cLayersLock(&(lpConn->sLayers.nRwMutex));

        lpLayer = lpConn->sLayers.cList.GetHead();
      }
      while (lpLayer != NULL)
      {
        hRes2 = lpLayer->OnDisconnect();
        if (FAILED(hRes2) && SUCCEEDED((HRESULT)__InterlockedRead(&(lpConn->hErrorCode))))
          _InterlockedExchange(&(lpConn->hErrorCode), (LONG)hRes2);
        {
          CAutoSlimRWLShared cLayersLock(&(lpConn->sLayers.nRwMutex));

          lpLayer = lpLayer->GetNextEntry();
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
          if (FAILED(hRes2) && SUCCEEDED((HRESULT)__InterlockedRead(&(lpConn->hErrorCode))))
            _InterlockedExchange(&(lpConn->hErrorCode), (LONG)hRes2);
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
      MX_IPC_DEBUG_PRINT(1, ("%lu MX::CIpc::CConnectionBase::GracefulShutdown C) Clock=%lums / This=0x%p / "
                             "Res=0x%08X\n", ::MxGetCurrentThreadId(), lpConn->cHiResTimer.GetElapsedTimeMs(), this,
                             hRes));
      hRes = S_OK;
      _InterlockedOr(&(lpConn->nFlags), FLAG_GracefulShutdown);
    }
    lpConn->Close(hRes);
  }
  //release connection reference
  lpConn->Release();
  MX_IPC_DEBUG_PRINT(1, ("%lu MX::CIpc::OnDispatcherPacket) Clock=%lums / Conn=0x%p / Ovr=0x%p / Type=%lu / "
                         "Err=0x%08X [EXIT]\n", ::MxGetCurrentThreadId(), lpConn->cHiResTimer.GetElapsedTimeMs(),
                         lpConn, lpOrigOverlapped, nOrigOverlappedType, hRes));
  lpConn->Release();
  //done
  return;
}

HRESULT CIpc::OnPreprocessPacket(_In_ DWORD dwBytes, _In_ CPacket *lpPacket, _In_ HRESULT hRes)
{
  return S_FALSE;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CIpc::CConnectionBase::CConnectionBase(_In_ CIpc *_lpIpc, _In_ CIpc::eConnectionClass _nClass) :
                       TRefCounted<CBaseMemObj>(), TRedBlackTreeNode<CIpc::CConnectionBase,SIZE_T>()
{
  _InterlockedExchange(&nMutex, 0);
  lpIpc = _lpIpc;
  nClass = _nClass;
  _InterlockedExchange(&hErrorCode, S_OK);
  _InterlockedExchange(&nFlags, 0);
  _InterlockedExchange(&nNextReadOrder, 0);
  _InterlockedExchange(&nNextReadOrderToProcess, 1);
  _InterlockedExchange(&nNextWriteOrder, 0);
  _InterlockedExchange(&nNextWriteOrderToProcess, 1);
  _InterlockedExchange(&nOutstandingReads, 0);
  _InterlockedExchange(&nOutstandingWrites, 0);
  _InterlockedExchange(&nOutstandingWritesBeingSent, 0);
  cCreateCallback = NullCallback();
  cDestroyCallback = NullCallback();
  cConnectCallback = NullCallback();
  cDisconnectCallback = NullCallback();
  cDataReceivedCallback = NullCallback();
  dwWriteTimeoutMs = lpIpc->dwMaxWriteTimeoutMs;
  lpTimedEventQueue = NULL;
  _InterlockedExchange(&(sWriteTimeout.nMutex), 0);
  SlimRWL_Initialize(&(sLayers.nRwMutex));
  _InterlockedExchange(&(sReceivedData.nMutex), 0);
  return;
}

CIpc::CConnectionBase::~CConnectionBase()
{
  CPacket *lpPacket;
  CLayer *lpLayer;

  {
    CAutoSlimRWLExclusive cConnListLock(&(lpIpc->sConnections.nSlimMutex));

    RemoveNode();
  }
  lpIpc->FireOnDestroy(this);

  MX_ASSERT(sWriteTimeout.cActiveList.HasPending() == FALSE);
  //free packets
  while ((lpPacket = cRwList.DequeueFirst()) != NULL)
    lpIpc->FreePacket(lpPacket);
  while ((lpPacket = cReadedList.DequeueFirst()) != NULL)
    lpIpc->FreePacket(lpPacket);
  while ((lpPacket = cWritePendingList.DequeueFirst()) != NULL)
    lpIpc->FreePacket(lpPacket);
  //destroy layers
  while ((lpLayer=sLayers.cList.PopHead()) != NULL)
    delete lpLayer;
  //release queue
  if (lpTimedEventQueue != NULL)
    lpTimedEventQueue->Release();
  return;
}

VOID CIpc::CConnectionBase::ShutdownLink(_In_ BOOL bAbortive)
{
  CFastLock cWriteTimeoutLock(&(sWriteTimeout.nMutex));

  //cancel all queued events
  if (lpTimedEventQueue != NULL)
    sWriteTimeout.cActiveList.RunAction(MX_BIND_MEMBER_CALLBACK(&CConnectionBase::DoCancelEventsCallback, this));
  return;
}

VOID CIpc::CConnectionBase::DoCancelEventsCallback(_In_ __TEventArray &cEventsList)
{
  SIZE_T i, nCount;

  nCount = cEventsList.GetCount();
  for (i=0; i<nCount; i++)
    lpTimedEventQueue->Remove(cEventsList.GetElementAt(i));
  return;
}

HRESULT CIpc::CConnectionBase::SendMsg(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize)
{
  CPacket *lpPacket;
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
    lpPacket = lpIpc->GetPacket(this, CIpc::CPacket::TypeWriteRequest);
    if (lpPacket == NULL)
      return E_OUTOFMEMORY;
    //fill buffer
    dwToSendThisRound = (nDataSize > (SIZE_T)(lpIpc->dwPacketSize)) ? lpIpc->dwPacketSize : (DWORD)nDataSize;
    lpPacket->SetBytesInUse(dwToSendThisRound);
    MemCopy(lpPacket->GetBuffer(), s, (SIZE_T)(lpPacket->GetBytesInUse()));
    //prepare
    lpPacket->SetOrder(_InterlockedIncrement(&nNextWriteOrder));
    cRwList.QueueLast(lpPacket);
    AddRef();
    _InterlockedIncrement(&nOutstandingWrites);
    MX_IPC_DEBUG_PRINT(1, ("%lu MX::CIpc::CConnectionBase::SendMsg) Clock=%lums / Ovr=0x%p / Type=%lu / Bytes=%lu\n",
                           ::MxGetCurrentThreadId(), cHiResTimer.GetElapsedTimeMs(), lpPacket->GetOverlapped(),
                           lpPacket->GetType(), lpPacket->GetBytesInUse()));
    hRes = lpIpc->cDispatcherPool.Post(lpIpc->cDispatcherPoolPacketCallback, 0, lpPacket->GetOverlapped());
    if (FAILED(hRes))
    {
      MX_ASSERT_ALWAYS(__InterlockedRead(&nOutstandingWrites) >= 1);
      _InterlockedDecrement(&nOutstandingWrites);
      Release();
      return hRes;
    }
    //next block
    s += (SIZE_T)dwToSendThisRound;
    nDataSize -= (SIZE_T)dwToSendThisRound;
  }
  return S_OK;
}

HRESULT CIpc::CConnectionBase::SendStream(_In_ CStream *lpStream)
{
  CPacket *lpPacket;
  HRESULT hRes;

  MX_ASSERT(lpStream != NULL);
  //get a free buffer
  lpPacket = lpIpc->GetPacket(this, CIpc::CPacket::TypeWriteRequest);
  if (lpPacket == NULL)
    return E_OUTOFMEMORY;
  //prepare
  lpPacket->SetStream(lpStream);
  lpPacket->SetOrder(_InterlockedIncrement(&nNextWriteOrder));
  cRwList.QueueLast(lpPacket);
  AddRef();
  _InterlockedIncrement(&nOutstandingWrites);
  MX_IPC_DEBUG_PRINT(1, ("%lu MX::CIpc::CConnectionBase::SendStream) Clock=%lums / Ovr=0x%p / Type=%lu\n",
                         ::MxGetCurrentThreadId(), cHiResTimer.GetElapsedTimeMs(), lpPacket->GetOverlapped(),
                         lpPacket->GetType()));
  hRes = lpIpc->cDispatcherPool.Post(lpIpc->cDispatcherPoolPacketCallback, 0, lpPacket->GetOverlapped());
  if (FAILED(hRes))
  {
    _InterlockedDecrement(&nOutstandingWrites);
    Release();
  }
  return hRes;
}

HRESULT CIpc::CConnectionBase::AfterWriteSignal(_In_ OnAfterWriteSignalCallback cCallback, _In_opt_ LPVOID lpCookie)
{
  CPacket *lpPacket;
  HRESULT hRes;

  MX_ASSERT(cCallback != false);
  //get a free buffer
  lpPacket = lpIpc->GetPacket(this, CIpc::CPacket::TypeWriteRequest);
  if (lpPacket == NULL)
    return E_OUTOFMEMORY;
  //prepare
  lpPacket->SetOrder(_InterlockedIncrement(&nNextWriteOrder));
  lpPacket->SetAfterWriteSignalCallback(cCallback);
  lpPacket->SetUserData(lpCookie);
  cRwList.QueueLast(lpPacket);
  AddRef();
  _InterlockedIncrement(&nOutstandingWrites);
  MX_IPC_DEBUG_PRINT(1, ("%lu MX::CIpc::CConnectionBase::AfterWriteSignal) Clock=%lums / Ovr=0x%p / Type=%lu\n",
                         ::MxGetCurrentThreadId(), cHiResTimer.GetElapsedTimeMs(), lpPacket->GetOverlapped(),
                         lpPacket->GetType()));
  hRes = lpIpc->cDispatcherPool.Post(lpIpc->cDispatcherPoolPacketCallback, 0, lpPacket->GetOverlapped());
  if (FAILED(hRes))
  {
    _InterlockedDecrement(&nOutstandingWrites);
    Release();
  }
  return hRes;
}

HRESULT CIpc::CConnectionBase::SendResumeIoProcessingPacket(_In_ BOOL bInput)
{
  CPacket *lpPacket;
  HRESULT hRes;

  //get a free buffer
  lpPacket = lpIpc->GetPacket(this, CIpc::CPacket::TypeResumeIoProcessing);
  if (lpPacket == NULL)
    return E_OUTOFMEMORY;
  //prepare
  lpPacket->SetOrder((bInput != FALSE) ? 1 : 0);
  cRwList.QueueLast(lpPacket);
  AddRef();
  MX_IPC_DEBUG_PRINT(1, ("%lu MX::CIpc::CConnectionBase::SendResumeIoProcessingPacket[%s]) Clock=%lums / Ovr=0x%p / "
                         "Type=%lu\n", ::MxGetCurrentThreadId(), ((bInput != FALSE) ? "Input" : "Output"),
                         cHiResTimer.GetElapsedTimeMs(), lpPacket->GetOverlapped(), lpPacket->GetType()));
  hRes = lpIpc->cDispatcherPool.Post(lpIpc->cDispatcherPoolPacketCallback, 0, lpPacket->GetOverlapped());
  if (FAILED(hRes))
  {
    cRwList.Remove(lpPacket);
    lpIpc->FreePacket(lpPacket);
    Release();
  }
  return hRes;
}

HRESULT CIpc::CConnectionBase::WriteMsg(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize)
{
  CPacket *lpPacket;
  HRESULT hRes;

  if (nDataSize == 0)
    return S_OK;
  if (lpData == NULL)
    return E_POINTER;
  if (nDataSize > 0x7FFFFFFF)
    return E_INVALIDARG;
  if (IsClosed() != FALSE)
    return E_FAIL;
  //do write
  hRes = MarkLastWriteActivity(TRUE);
  if (FAILED(hRes))
    return hRes;
  while (nDataSize > 0)
  {
    //get a free buffer
    lpPacket = lpIpc->GetPacket(this, CIpc::CPacket::TypeWrite);
    if (lpPacket == NULL)
      return E_OUTOFMEMORY;
    //fill buffer
    lpPacket->SetBytesInUse((nDataSize > (SIZE_T)(lpIpc->dwPacketSize)) ? lpIpc->dwPacketSize : (DWORD)nDataSize);
    MemCopy(lpPacket->GetBuffer(), lpData, (SIZE_T)(lpPacket->GetBytesInUse()));
    //next block
    lpData = (LPBYTE)lpData + (SIZE_T)(lpPacket->GetBytesInUse());
    nDataSize -= (SIZE_T)(lpPacket->GetBytesInUse());
    //prepare
    cRwList.QueueLast(lpPacket);
    _InterlockedIncrement(&nOutstandingWrites);
    _InterlockedIncrement(&nOutstandingWritesBeingSent);
    switch (hRes = SendWritePacket(lpPacket))
    {
      case S_FALSE:
        if (_InterlockedDecrement(&nOutstandingWritesBeingSent) == 0)
          MarkLastWriteActivity(FALSE);
        if (_InterlockedDecrement(&nOutstandingWrites) == 0 && IsClosed() != FALSE)
          ShutdownLink(FAILED(__InterlockedRead(&hErrorCode)));
        MX_IPC_DEBUG_PRINT(1, ("%lu MX::CIpc::CConnectionBase::GracefulShutdown D) Clock=%lums / This=0x%p\n",
                               ::MxGetCurrentThreadId(), cHiResTimer.GetElapsedTimeMs(), this));
        _InterlockedOr(&nFlags, FLAG_GracefulShutdown);
        //free packet
        cRwList.Remove(lpPacket);
        FreePacket(lpPacket);
        return S_OK;

      case S_OK:
      case 0x80070000|ERROR_IO_PENDING:
        AddRef();
        break;

      default:
        if (_InterlockedDecrement(&nOutstandingWritesBeingSent) == 0)
          MarkLastWriteActivity(FALSE);
        if (_InterlockedDecrement(&nOutstandingWrites) == 0 && IsClosed() != FALSE)
          ShutdownLink(FAILED(__InterlockedRead(&hErrorCode)));
        return hRes;
    }
  }
  return S_OK;
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
  _InterlockedCompareExchange(&hErrorCode, hRes, S_OK);
  if (hRes != S_OK || __InterlockedRead(&nOutstandingWrites) == 0)
    ShutdownLink(FAILED(__InterlockedRead(&hErrorCode)));
  //when closing call the "final" release to remove from list
  if ((nInitVal & FLAG_Closed) == 0)
  {
    MX_IPC_DEBUG_PRINT(1, ("%lu MX::CIpc::CConnectionBase::Close) Clock=%lums / This=0x%p / Res=0x%08X\n",
                           ::MxGetCurrentThreadId(), cHiResTimer.GetElapsedTimeMs(), this, hRes));
    Release();
  }
  return;
}

BOOL CIpc::CConnectionBase::IsGracefulShutdown()
{
  return ((__InterlockedRead(&nFlags) & FLAG_GracefulShutdown) != 0) ? TRUE : FALSE;
}

BOOL CIpc::CConnectionBase::IsClosed()
{
  return ((__InterlockedRead(&nFlags) & FLAG_Closed) != 0) ? TRUE : FALSE;
}

BOOL CIpc::CConnectionBase::IsClosedOrGracefulShutdown()
{
  return ((__InterlockedRead(&nFlags) & (FLAG_GracefulShutdown | FLAG_Closed)) != 0) ? TRUE : FALSE;
}

BOOL CIpc::CConnectionBase::IsConnected()
{
  return ((__InterlockedRead(&nFlags) & FLAG_Connected) != 0) ? TRUE : FALSE;
}

HRESULT CIpc::CConnectionBase::HandleConnected()
{
  CPacket *lpPacket;
  HRESULT hRes;

  //mark as connected
  _InterlockedOr(&nFlags, FLAG_Connected);
  //send initial setup packet
  lpPacket = lpIpc->GetPacket(this, CPacket::TypeInitialSetup);
  if (lpPacket == NULL)
    return E_OUTOFMEMORY;
  cRwList.QueueLast(lpPacket);
  AddRef();
  MX_IPC_DEBUG_PRINT(1, ("%lu MX::CIpc::CConnectionBase::HandleConnected) Clock=%lums / Ovr=0x%p / Type=%lu\n",
                         ::MxGetCurrentThreadId(), cHiResTimer.GetElapsedTimeMs(), lpPacket->GetOverlapped(),
                         lpPacket->GetType()));
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

CIpc::CPacket* CIpc::CConnectionBase::GetPacket(_In_ CPacket::eType nType)
{
  return lpIpc->GetPacket(this, nType);
}

VOID CIpc::CConnectionBase::FreePacket(_In_ CPacket *lpPacket)
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

HRESULT CIpc::CConnectionBase::DoRead(_In_ SIZE_T nPacketsCount, _In_ BOOL bZeroRead, _In_opt_ CPacket *lpReusePacket)
{
  CFastLock cReadLock(&nMutex);
  CPacket *lpPacket;
  HRESULT hRes;

  while (nPacketsCount > 0)
  {
    if (lpReusePacket != NULL)
    {
      lpReusePacket->Reset((bZeroRead != FALSE) ? CPacket::TypeZeroRead : CPacket::TypeRead, this);
      lpPacket = lpReusePacket;
      lpReusePacket = NULL;
    }
    else
    {
      lpPacket = lpIpc->GetPacket(this, (bZeroRead != FALSE) ? CPacket::TypeZeroRead : CPacket::TypeRead);
      if (lpPacket == NULL)
        return E_OUTOFMEMORY;
    }
    if (bZeroRead == FALSE)
    {
      lpPacket->SetOrder(_InterlockedIncrement(&nNextReadOrder));
      lpPacket->SetBytesInUse(lpIpc->dwPacketSize);
    }
    else
    {
      lpPacket->SetOrder(0);
      lpPacket->SetBytesInUse(0);
    }
    cRwList.QueueLast(lpPacket);
    _InterlockedIncrement(&nOutstandingReads);
    switch (hRes = SendReadPacket(lpPacket))
    {
      case S_FALSE:
        MX_ASSERT_ALWAYS(_InterlockedDecrement(&nOutstandingReads) >= 0);
        MX_IPC_DEBUG_PRINT(1, ("%lu MX::CIpc::CConnectionBase::GracefulShutdown E) Clock=%lums / This=0x%p\n",
                               ::MxGetCurrentThreadId(), cHiResTimer.GetElapsedTimeMs(), this));
        _InterlockedOr(&nFlags, FLAG_GracefulShutdown);
        //free packet
        cRwList.Remove(lpPacket);
        FreePacket(lpPacket);
        hRes = S_OK;
        break;

      case S_OK:
      case 0x80070000|ERROR_IO_PENDING:
        AddRef();
        break;

      default:
        MX_ASSERT_ALWAYS(_InterlockedDecrement(&nOutstandingReads) >= 0);
        return hRes;
    }
    nPacketsCount--;
  }
  //done
  return S_OK;
}

HRESULT CIpc::CConnectionBase::SendDataToLayer(_In_ LPCVOID lpMsg, _In_ SIZE_T nMsgSize, _In_ CLayer *lpCurrLayer,
                                               _In_ BOOL bIsMsg)
{
  CLayer *lpLayer;
  HRESULT hRes = S_OK;

  if (lpMsg == NULL && nMsgSize > 0)
    return E_POINTER;
  if (nMsgSize == 0)
    return S_OK;
  if (bIsMsg == FALSE)
  {
    {
      CAutoSlimRWLShared cLayersLock(&(sLayers.nRwMutex));

      lpLayer = lpCurrLayer->GetNextEntry();
    }
    if (lpLayer != NULL)
    {
      hRes = lpLayer->OnData(lpMsg, nMsgSize);
    }
    else
    {
      CFastLock cRecBufLock(&(sReceivedData.nMutex));

      hRes = sReceivedData.cBuffer.Write(lpMsg, nMsgSize);
      _InterlockedOr(&nFlags, FLAG_NewReceivedDataAvailable);
    }
  }
  else
  {
    {
      CAutoSlimRWLShared cLayersLock(&(sLayers.nRwMutex));

      lpLayer = lpCurrLayer->GetPrevEntry();
    }
    hRes = (lpLayer != NULL) ? (lpLayer->OnSendMsg(lpMsg, nMsgSize)) : WriteMsg(lpMsg, nMsgSize);
  }
  return hRes;
}

HRESULT CIpc::CConnectionBase::MarkLastWriteActivity(_In_ BOOL bSet)
{
  CFastLock cWriteTimeoutLock(&(sWriteTimeout.nMutex));
  CTimedEventQueue::CEvent *lpEvent;

  if (IsClosed() != FALSE && IsGracefulShutdown() == FALSE)
    return MX_E_Cancelled;
  if (bSet != FALSE)
  {
    HRESULT hRes;

    if (dwWriteTimeoutMs == 0)
      return S_OK;
    //get timed queue
    if (lpTimedEventQueue == NULL)
    {
      lpTimedEventQueue = CSystemTimedEventQueue::Get();
      if (lpTimedEventQueue == NULL)
        return E_OUTOFMEMORY;
    }
    //create event if needed
    lpEvent = MX_DEBUG_NEW CTimedEventQueue::CEvent(MX_BIND_MEMBER_CALLBACK(&CIpc::CConnectionBase::OnWriteTimeout,
                                                                            this), NULL);
    if (lpEvent == NULL)
      return E_OUTOFMEMORY;
    AddRef();
    //add to list
    hRes = sWriteTimeout.cActiveList.Add(lpEvent);
    if (FAILED(hRes))
    {
      lpEvent->Release();
      Release();
      return hRes;
    }
    //add event
    hRes = lpTimedEventQueue->Add(lpEvent, dwWriteTimeoutMs);
    if (FAILED(hRes))
    {
      sWriteTimeout.cActiveList.Remove(lpEvent);
      lpEvent->Release();
      Release();
      return hRes;
    }
  }
  else
  {
    if (lpTimedEventQueue != NULL)
    {
      //cancel all queued events
      sWriteTimeout.cActiveList.RunAction(MX_BIND_MEMBER_CALLBACK(&CConnectionBase::DoCancelEventsCallback, this));
    }
  }
  //done
  return S_OK;
}

VOID CIpc::CConnectionBase::OnWriteTimeout(_In_ CTimedEventQueue::CEvent *lpEvent)
{
  {
    CFastLock cWriteTimeoutLock(&(sWriteTimeout.nMutex));

    sWriteTimeout.cActiveList.Remove(lpEvent);
  }
  //close connection if not canceled
  if (lpEvent->IsCanceled() == FALSE)
    Close(MX_E_Timeout);
  //release event and myself
  lpEvent->Release();
  Release();
  return;
}

//-----------------------------------------------------------

HANDLE CIpc::CLayer::GetConn()
{
  return (HANDLE)lpConn;
}

HRESULT CIpc::CLayer::SendProcessedDataToNextLayer(_In_ LPCVOID lpMsg, _In_ SIZE_T nMsgSize)
{
  return reinterpret_cast<CIpc::CConnectionBase*>(lpConn)->SendDataToLayer(lpMsg, nMsgSize, this, FALSE);
}

HRESULT CIpc::CLayer::SendMsgToNextLayer(_In_ LPCVOID lpMsg, _In_ SIZE_T nMsgSize)
{
  return reinterpret_cast<CIpc::CConnectionBase*>(lpConn)->SendDataToLayer(lpMsg, nMsgSize, this, TRUE);
}

//-----------------------------------------------------------

CIpc::CMultiSendLock::CMultiSendLock() : CBaseMemObj()
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

CIpc::CAutoMultiSendLock::CAutoMultiSendLock(_In_ CMultiSendLock *_lpLock) : CBaseMemObj()
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
