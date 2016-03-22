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

//-----------------------------------------------------------

namespace MX {

CIpc::CIpc(__in CIoCompletionPortThreadPool &_cDispatcherPool, __in CPropertyBag &_cPropBag) : CBaseMemObj(),
      cDispatcherPool(_cDispatcherPool), cPropBag(_cPropBag)
{
  dwPacketSize = 1024;
  dwReadAhead = 1;
  dwMaxFreePackets = MX_IPC_PROPERTY_MaxFreePackets_DEFVAL;
  dwMaxWriteTimeoutMs = MX_IPC_PROPERTY_MaxWriteTimeoutMs_DEFVAL;
  bDoZeroReads = TRUE;
  dwMaxOutgoingPackets = MX_IPC_PROPERTY_OutgoingPacketsLimit_DEFVAL;
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

VOID CIpc::On(__in OnEngineErrorCallback _cEngineErrorCallback)
{
  cEngineErrorCallback = _cEngineErrorCallback;
  return;
}

HRESULT CIpc::Initialize()
{
  CFastLock cInitShutdownLock(&nInitShutdownMutex);
  DWORD dw;
  HRESULT hRes;

  InternalFinalize();

  //read properties from property bag
  cPropBag.GetDWord(MX_IPC_PROPERTY_PacketSize, dwPacketSize, MX_IPC_PROPERTY_PacketSize_DEFVAL);
  if (dwPacketSize < 1024)
    dwPacketSize = 1024;
  else if (dwPacketSize > 32768)
    dwPacketSize = 32768;
  cPropBag.GetDWord(MX_IPC_PROPERTY_ReadAhead, dwReadAhead, MX_IPC_PROPERTY_ReadAhead_DEFVAL);
  if (dwReadAhead < 1)
    dwReadAhead = 1;
  else if (dwReadAhead > 16)
    dwReadAhead = 16;
  cPropBag.GetDWord(MX_IPC_PROPERTY_MaxFreePackets, dwMaxFreePackets, MX_IPC_PROPERTY_MaxFreePackets_DEFVAL);
  cPropBag.GetDWord(MX_IPC_PROPERTY_MaxWriteTimeoutMs, dwMaxWriteTimeoutMs, MX_IPC_PROPERTY_MaxWriteTimeoutMs_DEFVAL);
  if (dwMaxWriteTimeoutMs != 0 && dwMaxWriteTimeoutMs < 100)
    dwMaxWriteTimeoutMs = 100;
  cPropBag.GetDWord(MX_IPC_PROPERTY_DoZeroReads, dw, MX_IPC_PROPERTY_DoZeroReads_DEFVAL);
  bDoZeroReads = (dw != 0) ? TRUE : FALSE;
  cPropBag.GetDWord(MX_IPC_PROPERTY_OutgoingPacketsLimit, dwMaxOutgoingPackets,
                    MX_IPC_PROPERTY_OutgoingPacketsLimit_DEFVAL);
  if (dwMaxOutgoingPackets < 2)
    dwMaxOutgoingPackets = 2;

  //create shutdown event
  hRes = (cShuttingDownEv.Create(TRUE, FALSE) != FALSE) ? S_OK : E_OUTOFMEMORY;
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

HRESULT CIpc::SendMsg(__in HANDLE h, __in LPCVOID lpMsg, __in SIZE_T nMsgSize)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  CConnectionBase *lpConn;
  HRESULT hRes;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  if (lpMsg == NULL)
    return E_POINTER;
  if (nMsgSize == 0)
    return E_INVALIDARG;
  lpConn = CheckAndGetConnection(h);
  if (lpConn == NULL)
    return E_INVALIDARG;
  //send real message
  hRes = (lpConn->IsClosed() == FALSE) ? lpConn->SendMsg(lpMsg, nMsgSize) : E_FAIL;
  //done
  ReleaseAndRemoveConnectionIfClosed(lpConn);
  return hRes;
}

HRESULT CIpc::SendStream(__in HANDLE h, __in CStream *lpStream)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  TAutoRefCounted<CStream> cStream(lpStream);
  CConnectionBase *lpConn;
  HRESULT hRes;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  if (lpStream == NULL)
    return E_POINTER;
  lpConn = CheckAndGetConnection(h);
  if (lpConn == NULL)
    return E_INVALIDARG;
  //send real message
  hRes = (lpConn->IsClosed() == FALSE) ? lpConn->SendStream(lpStream) : E_FAIL;
  //done
  ReleaseAndRemoveConnectionIfClosed(lpConn);
  return hRes;
}

HRESULT CIpc::AfterWriteSignal(__in HANDLE h, __in OnAfterWriteSignalCallback cCallback, __in LPVOID lpCookie)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  CConnectionBase *lpConn;
  HRESULT hRes;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  if (!cCallback)
    return E_INVALIDARG;
  lpConn = CheckAndGetConnection(h);
  if (lpConn == NULL)
    return E_INVALIDARG;
  //add signal
  hRes = (lpConn->IsClosed() == FALSE) ? lpConn->AfterWriteSignal(cCallback, lpCookie) : E_FAIL;
  //done
  ReleaseAndRemoveConnectionIfClosed(lpConn);
  return hRes;
}

CIpc::CMultiSendLock* CIpc::StartMultiSendBlock(__in HANDLE h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  CConnectionBase *lpConn;
  TAutoDeletePtr<CIpc::CMultiSendLock> cMultiSendBlock;

  if (cRundownLock.IsAcquired() == FALSE)
    return NULL;
  lpConn = CheckAndGetConnection(h);
  if (lpConn == NULL)
    return NULL;
  cMultiSendBlock.Attach(MX_DEBUG_NEW CMultiSendLock());
  if (!cMultiSendBlock)
  {
    ReleaseAndRemoveConnectionIfClosed(lpConn);
    return NULL;
  }
  //apply lock
  while ((_InterlockedOr(&(lpConn->nFlags), FLAG_InSendTransaction) & FLAG_InSendTransaction) != 0)
    _YieldProcessor();
  cMultiSendBlock->lpConn = lpConn;
  //done
  return cMultiSendBlock.Detach();
}

HRESULT CIpc::GetBufferedMessage(__in HANDLE h, __out LPVOID lpMsg, __inout SIZE_T *lpnMsgSize)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  CConnectionBase *lpConn;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  if (lpMsg == NULL || lpnMsgSize == NULL)
    return E_POINTER;
  lpConn = CheckAndGetConnection(h);
  if (lpConn == NULL)
    return E_INVALIDARG;
  //get received data
  {
    CFastLock cRecBufLock(&(lpConn->sReceivedData.nMutex));

    *lpnMsgSize = lpConn->sReceivedData.cBuffer.Peek(lpMsg, *lpnMsgSize);
  }
  //done
  ReleaseAndRemoveConnectionIfClosed(lpConn);
  return S_OK;
}

HRESULT CIpc::ConsumeBufferedMessage(__in HANDLE h, __in SIZE_T nConsumedBytes)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  CConnectionBase *lpConn;
  HRESULT hRes;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  lpConn = CheckAndGetConnection(h);
  if (lpConn == NULL)
    return E_INVALIDARG;
  //consume received data
  {
    CFastLock cRecBufLock(&(lpConn->sReceivedData.nMutex));

    hRes = lpConn->sReceivedData.cBuffer.AdvanceReadPtr(nConsumedBytes);
  }
  //done
  ReleaseAndRemoveConnectionIfClosed(lpConn);
  return hRes;
}

HRESULT CIpc::Close(__in HANDLE h, __in HRESULT hErrorCode)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  CConnectionBase *lpConn;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  lpConn = CheckAndGetConnection(h);
  if (lpConn == NULL)
    return E_INVALIDARG;
  //close connection
  CloseConnection(lpConn, hErrorCode);
  //done
  ReleaseAndRemoveConnectionIfClosed(lpConn);
  return S_OK;
}

HRESULT CIpc::IsConnected(__in HANDLE h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  CConnectionBase *lpConn;
  HRESULT hRes;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  lpConn = CheckAndGetConnection(h);
  if (lpConn == NULL)
    return E_INVALIDARG;
  //get state
  hRes = (lpConn->IsConnected() != FALSE) ? S_OK : S_FALSE;
  //done
  ReleaseAndRemoveConnectionIfClosed(lpConn);
  return hRes;
}

HRESULT CIpc::IsClosed(__in HANDLE h, __out_opt HRESULT *lphErrorCode)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  CConnectionBase *lpConn;
  HRESULT hRes;

  if (cRundownLock.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  lpConn = CheckAndGetConnection(h);
  if (lpConn == NULL)
    return E_INVALIDARG;
  //get state
  if (lphErrorCode != NULL)
    *lphErrorCode = __InterlockedRead(&(lpConn->hErrorCode));
  hRes = (lpConn->IsClosed() != FALSE) ? S_OK : S_FALSE;
  //done
  ReleaseAndRemoveConnectionIfClosed(lpConn);
  return hRes;
}

CIpc::CUserData* CIpc::GetUserData(__in HANDLE h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  CConnectionBase *lpConn;
  CUserData *lpUserData;

  if (cRundownLock.IsAcquired() == FALSE)
    return NULL;
  lpConn = CheckAndGetConnection(h);
  if (lpConn == NULL)
    return NULL;
  //get user data
  lpUserData = lpConn->cUserData;
  if (lpUserData != NULL)
    lpUserData->AddRef();
  //done
  ReleaseAndRemoveConnectionIfClosed(lpConn);
  return lpUserData;
}

CIpc::eConnectionClass CIpc::GetClass(__in HANDLE h)
{
  CAutoRundownProtection cRundownLock(&nRundownProt);
  CConnectionBase *lpConn;
  eConnectionClass nClass;

  if (cRundownLock.IsAcquired() == FALSE)
    return ConnectionClassError;
  lpConn = CheckAndGetConnection(h);
  if (lpConn == NULL)
    return ConnectionClassError;
  //check class
  switch (lpConn->nClass)
  {
    case ConnectionClassClient:
    case ConnectionClassServer:
    case ConnectionClassListener:
      nClass = lpConn->nClass;
      break;
    default:
      nClass = ConnectionClassUnknown;
      break;
  }
  //done
  ReleaseAndRemoveConnectionIfClosed(lpConn);
  return nClass;
}

BOOL CIpc::IsShuttingDown()
{
  return cShuttingDownEv.Wait(0);
}

//-----------------------------------------------------------

VOID CIpc::InternalFinalize()
{
  TLnkLst<CConnectionBase>::Iterator itConn;
  CConnectionBase *lpConn;

  RundownProt_WaitForRelease(&nRundownProt);
  cShuttingDownEv.Set();
  //close all connections
  do
  {
    {
      CAutoSlimRWLShared cConnListLock(&(sConnections.nSlimMutex));

      for (lpConn=itConn.Begin(sConnections.cList); lpConn!=NULL; lpConn=itConn.Next())
      {
        if (lpConn->IsClosed() == FALSE)
        {
          _InterlockedIncrement(&(lpConn->nRefCount));
          break;
        }
      }
    }
    if (lpConn != NULL)
    {
      CloseConnection(lpConn, MX_E_Cancelled);
      lpConn->ShutdownLink(TRUE);
      ReleaseAndRemoveConnectionIfClosed(lpConn);
    }
  }
  while (lpConn != NULL);
  //wait all connections to be destroyed
  do
  {
    {
      CAutoSlimRWLShared cConnListLock(&(sConnections.nSlimMutex));

      lpConn = sConnections.cList.GetHead();
      if (lpConn != NULL)
        _InterlockedIncrement(&(lpConn->nRefCount));
    }
    if (lpConn != NULL)
    {
      ReleaseAndRemoveConnectionIfClosed(lpConn);
      MX::_YieldProcessor();
    }
  }
  while (lpConn != NULL);
  //remove free packets
  cFreePacketsList.DiscardAll();
  //almost done
  OnInternalFinalize();
  cShuttingDownEv.Close();
  RundownProt_Initialize(&nRundownProt);
  return;
}

VOID CIpc::CloseConnection(__in CConnectionBase *lpConn, __in HRESULT hErrorCode)
{
  lpConn->Close(hErrorCode);
  return;
}

VOID CIpc::ReleaseAndRemoveConnectionIfClosed(__in CConnectionBase *lpConn)
{
  MX_ASSERT(__InterlockedRead(&(lpConn->nRefCount)) > 0);
  if (_InterlockedDecrement(&(lpConn->nRefCount)) == 0 && lpConn->IsClosed() != FALSE)
  {
    BOOL bOnList;

    //this is the last reference to the connection, if closed, remove from the connections list
    lpConn->nCheckTag1 = lpConn->nCheckTag2 = 0;
    {
      CAutoSlimRWLExclusive cConnListLock(&(sConnections.nSlimMutex));

      bOnList = FALSE;
      if (lpConn->GetLinkedList() == &(sConnections.cList))
      {
        bOnList = TRUE;
        lpConn->RemoveNode();
     }
    }
    if (bOnList != FALSE)
    {
      MX_ASSERT(lpConn->IsClosed() != FALSE);
      FireOnDestroy(lpConn);
      delete lpConn;
    }
  }
  return;
}

VOID CIpc::FireOnEngineError(__in HRESULT hErrorCode)
{
  if (cEngineErrorCallback)
    cEngineErrorCallback(this, hErrorCode);
  return;
}

HRESULT CIpc::FireOnCreate(__in CConnectionBase *lpConn)
{
  HRESULT hRes = S_OK;

  if (lpConn->cCreateCallback)
  {
    CREATE_CALLBACK_DATA sCallbackData = {
      lpConn->cLayersList,
      lpConn->cConnectCallback, lpConn->cDisconnectCallback,
      lpConn->cDataReceivedCallback, lpConn->cDestroyCallback,
      lpConn->dwWriteTimeoutMs
    };
    CUserData *lpUserData;

    lpUserData = NULL;
    hRes = lpConn->cCreateCallback(this, (HANDLE)lpConn, &lpUserData, sCallbackData);
    if (SUCCEEDED(hRes))
    {
      TLnkLst<CLayer>::Iterator it;
      CLayer *lpLayer;

      lpConn->cUserData.Attach(lpUserData);
      for (lpLayer=it.Begin(lpConn->cLayersList); lpLayer!=NULL; lpLayer=it.Next())
        lpLayer->lpConn = lpConn;
    }
  }
  return hRes;
}

VOID CIpc::FireOnDestroy(__in CConnectionBase *lpConn)
{
  TAutoRefCounted<CUserData> cUserData(lpConn->cUserData);

  if (lpConn->cDestroyCallback)
    lpConn->cDestroyCallback(this, (HANDLE)lpConn, lpConn->cUserData, lpConn->hErrorCode);
  return;
}

HRESULT CIpc::FireOnConnect(__in CConnectionBase *lpConn, __in HRESULT hErrorCode)
{
  HRESULT hRes = S_OK;

  if (lpConn->IsClosed() == FALSE)
  {
    if (lpConn->cConnectCallback)
    {
      hRes = lpConn->cConnectCallback(this, (HANDLE)lpConn, lpConn->cUserData, lpConn->cLayersList, hErrorCode);
      if (SUCCEEDED(hRes))
      {
        TLnkLst<CLayer>::Iterator it;
        CLayer *lpLayer;

        for (lpLayer=it.Begin(lpConn->cLayersList); lpLayer!=NULL; lpLayer=it.Next())
          lpLayer->lpConn = lpConn;
      }
    }
  }
  return hRes;
}

VOID CIpc::FireOnDisconnect(__in CConnectionBase *lpConn)
{
  TAutoRefCounted<CUserData> cUserData(lpConn->cUserData);

  if (lpConn->cDisconnectCallback)
    lpConn->cDisconnectCallback(this, (HANDLE)lpConn, lpConn->cUserData, lpConn->hErrorCode);
  return;
}

HRESULT CIpc::FireOnDataReceived(__in CConnectionBase *lpConn)
{
  CCriticalSection::CAutoLock cOnDataReceivedLock(lpConn->cOnDataReceivedCS);
  TAutoRefCounted<CUserData> cUserData(lpConn->cUserData);

  if (!(lpConn->cDataReceivedCallback))
    return E_NOTIMPL;
  return lpConn->cDataReceivedCallback(this, lpConn, lpConn->cUserData);
}

CIpc::CPacket* CIpc::GetPacket(__in CConnectionBase *lpConn, __in CPacket::eType nType)
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

VOID CIpc::FreePacket(__in CPacket *lpPacket)
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

CIpc::CConnectionBase* CIpc::CheckAndGetConnection(__in HANDLE h)
{
  CAutoSlimRWLShared cConnListLock(&(sConnections.nSlimMutex));
  CConnectionBase *lpConn;

  lpConn = IsValidConnection(h);
  if (lpConn != NULL)
  {
    if (lpConn->GetLinkedList() == &(sConnections.cList))
      _InterlockedIncrement(&(lpConn->nRefCount));
    else
      lpConn = NULL;
  }
  return lpConn;
}

CIpc::CConnectionBase* CIpc::IsValidConnection(__in HANDLE h)
{
  CConnectionBase *lpConn;

  if (h == NULL)
    return NULL;
  lpConn = (CConnectionBase*)h;
  __try
  {
    if (lpConn->nCheckTag1 != CHECKTAG1 || lpConn->nCheckTag2 != CHECKTAG2)
      return NULL;
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    return NULL;
  }
  return lpConn;
}

VOID CIpc::OnDispatcherPacket(__in CIoCompletionPortThreadPool *lpPool, __in DWORD dwBytes, __in OVERLAPPED *lpOvr,
                              __in HRESULT hRes)
{
  CConnectionBase *lpConn;
  CPacket *lpPacket, *lpStreamPacket;
  CLayer *lpLayer;
  SIZE_T nReaded;
  HRESULT hRes2;
#ifdef MX_IPC_TIMING
  LPVOID lpOrigOverlapped;
  CPacket::eType nOrigOverlappedType;
#endif //MX_IPC_TIMING

  lpPacket = CPacket::FromOverlapped(lpOvr);
  lpConn = lpPacket->GetConn();
  _InterlockedIncrement(&(lpConn->nRefCount));
#ifdef MX_IPC_TIMING
  lpOrigOverlapped = lpPacket->GetOverlapped();
  nOrigOverlappedType = lpPacket->GetType();
  DebugPrint("%lu MX::CIpc::OnDispatcherPacket) Clock=%lums / Conn=0x%p / Ovr=0x%p / Type=%lu / Bytes=%lu / "
             "Err=0x%08X\n", ::MxGetCurrentThreadId(), lpConn->cHiResTimer.GetElapsedTimeMs(), lpConn,
             lpPacket->GetOverlapped(), lpPacket->GetType(), dwBytes, hRes);
#endif //MX_IPC_TIMING
  hRes2 = OnPreprocessPacket(dwBytes, lpPacket, hRes);
  if (hRes2 == S_FALSE)
  {
    switch (lpPacket->GetType())
    {
      case CPacket::TypeInitialSetup:
        {
        TLnkLst<CLayer>::Iterator it;

        if (SUCCEEDED(hRes))
          hRes =  lpConn->DoRead((SIZE_T)dwReadAhead, bDoZeroReads && ZeroReadsSupported());
        //notify all layers about disconnection
        for (lpLayer=it.Begin(lpConn->cLayersList); SUCCEEDED(hRes) && lpLayer!=NULL; lpLayer=it.Next())
          hRes = lpLayer->OnConnect();
        //fire main connect
        if (SUCCEEDED(hRes))
          hRes = FireOnConnect(lpConn, S_OK);
        //free packet
        lpConn->cRwList.Remove(lpPacket);
        FreePacket(lpPacket);
        }
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
        {
        BOOL bQueueNewRead, bNewReadMode;

        bQueueNewRead = bNewReadMode = FALSE;
        if (SUCCEEDED(hRes))
        {
          lpPacket->SetBytesInUse(dwBytes);
          if (dwBytes > 0)
          {
            bQueueNewRead = TRUE;
            bNewReadMode = (lpConn->IsGracefulShutdown() == FALSE && ZeroReadsSupported() != FALSE) ? bDoZeroReads :
                                                                                                      FALSE;
            //move packet to readed list and process them
            lpConn->cRwList.Remove(lpPacket);
            lpConn->cReadedList.QueueSorted(lpPacket);
            lpLayer = lpConn->cLayersList.GetHead();
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
          else
          {
#ifdef MX_IPC_TIMING
            DebugPrint("%lu MX::CIpc::CConnectionBase::GracefulShutdown A) Clock=%lums / This=0x%p\n",
                        ::MxGetCurrentThreadId(), lpConn->cHiResTimer.GetElapsedTimeMs(), lpConn);
#endif //MX_IPC_TIMING
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
        }
        break;

      case CPacket::TypeWriteRequest:
        lpConn->cRwList.Remove(lpPacket);
        lpConn->cWritePendingList.QueueSorted(lpPacket);
check_pending_req:
        lpLayer = lpConn->cLayersList.GetTail();
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
                lpConn->ShutdownLink(FAILED(lpConn->hErrorCode));
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
              lpConn->cWritePendingList.QueueSorted(lpStreamPacket);
              //set error
              hRes = E_OUTOFMEMORY;
              break;
            }
            //read from stream
            nReaded = 0;
            hRes = lpStreamPacket->GetStream()->Read(lpPacket->GetBuffer(), (SIZE_T)dwPacketSize, nReaded);
            if (SUCCEEDED(hRes) && nReaded > (SIZE_T)dwPacketSize)
            {
              MX_ASSERT(FALSE);
              hRes = MX_E_InvalidData;
            }
            if (hRes == MX_E_EndOfFileReached)
              hRes = S_OK;
            if (FAILED(hRes))
            {
              FreePacket(lpPacket);
              //dealing with an error, requeue
              lpConn->cWritePendingList.QueueSorted(lpStreamPacket);
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
                    lpConn->ShutdownLink(FAILED(lpConn->hErrorCode));
#ifdef MX_IPC_TIMING
                  DebugPrint("%lu MX::CIpc::CConnectionBase::GracefulShutdown B) Clock=%lums / This=0x%p\n",
                             ::MxGetCurrentThreadId(), lpConn->cHiResTimer.GetElapsedTimeMs(), this);
#endif //MX_IPC_TIMING
                  _InterlockedOr(&(lpConn->nFlags), FLAG_GracefulShutdown);
                  //free packet
                  lpConn->cRwList.Remove(lpPacket);
                  FreePacket(lpPacket);
                  hRes = S_OK;
                  break;

                case S_OK:
                case 0x80070000|ERROR_IO_PENDING:
                  _InterlockedIncrement(&(lpConn->nRefCount));
                  hRes = S_OK;
                  break;

                default:
                  if (_InterlockedDecrement(&(lpConn->nOutstandingWritesBeingSent)) == 0)
                    lpConn->MarkLastWriteActivity(FALSE);
                  if (_InterlockedDecrement(&(lpConn->nOutstandingWrites)) == 0 && lpConn->IsClosed() != FALSE)
                    lpConn->ShutdownLink(FAILED(lpConn->hErrorCode));
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
            lpConn->cWritePendingList.QueueSorted(lpStreamPacket);
          }
          else
          {
            //increment next write to process if processed a non-stream packet
            _InterlockedIncrement(&(lpConn->nNextWriteOrderToProcess));
            //decrement the outstanding writes incremented by the send and close if needed
            MX_ASSERT_ALWAYS(__InterlockedRead(&(lpConn->nOutstandingWrites)) >= 1);
            if (_InterlockedDecrement(&(lpConn->nOutstandingWrites)) == 0 && lpConn->IsClosed() != FALSE)
              lpConn->ShutdownLink(FAILED(lpConn->hErrorCode));
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
          lpConn->ShutdownLink(FAILED(lpConn->hErrorCode));
        //free packet
        lpConn->cRwList.Remove(lpPacket);
        FreePacket(lpPacket);
        //check for pending write packets
        if (SUCCEEDED(hRes))
          goto check_pending_req;
        break;

      case CPacket::TypeDiscard:
        //ignore
        MX_ASSERT(FALSE);
        break;

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
  else if (__InterlockedRead(&(lpConn->nOutstandingReads)) == 0 && __InterlockedRead(&(lpConn->nOutstandingWrites)) == 0)
  {
    if ((_InterlockedOr(&(lpConn->nFlags), FLAG_DisconnectedSent) & FLAG_DisconnectedSent) == 0)
    {
      TLnkLst<CLayer>::Iterator it;
      CLayer *lpLayer;
      BOOL bDataAvailable;

      if (lpConn->IsClosed() == FALSE)
        CloseConnection(lpConn, S_OK);
      //wait until another message is processed
      lpConn->cOnDataReceivedCS.Lock();
      lpConn->cOnDataReceivedCS.Unlock();
      //notify all layers about disconnection
      for (lpLayer=it.Begin(lpConn->cLayersList); lpLayer!=NULL; lpLayer=it.Next())
      {
        hRes2 = lpLayer->OnDisconnect();
        if (FAILED(hRes2) && SUCCEEDED((HRESULT)__InterlockedRead(&(lpConn->hErrorCode))))
          _InterlockedExchange(&(lpConn->hErrorCode), (LONG)hRes2);
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
#ifdef MX_IPC_TIMING
      DebugPrint("%lu MX::CIpc::CConnectionBase::GracefulShutdown C) Clock=%lums / This=0x%p / Res=0x%08X\n",
                 ::MxGetCurrentThreadId(), lpConn->cHiResTimer.GetElapsedTimeMs(), this, hRes);
#endif //MX_IPC_TIMING
      hRes = S_OK;
      _InterlockedOr(&(lpConn->nFlags), FLAG_GracefulShutdown);
    }
    CloseConnection(lpConn, hRes);
  }
  //release connection reference
  ReleaseAndRemoveConnectionIfClosed(lpConn);
#ifdef MX_IPC_TIMING
  DebugPrint("%lu MX::CIpc::OnDispatcherPacket) Clock=%lums / Conn=0x%p / Ovr=0x%p / Type=%lu / Err=0x%08X [EXIT]\n",
             ::MxGetCurrentThreadId(), lpConn->cHiResTimer.GetElapsedTimeMs(), lpConn, lpOrigOverlapped,
             nOrigOverlappedType, hRes);
#endif //MX_IPC_TIMING
  ReleaseAndRemoveConnectionIfClosed(lpConn);
  //done
  return;
}

HRESULT CIpc::OnPreprocessPacket(__in DWORD dwBytes, __in CPacket *lpPacket, __in HRESULT hRes)
{
  return S_FALSE;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CIpc::CConnectionBase::CConnectionBase(__in CIpc *_lpIpc, __in CIpc::eConnectionClass _nClass) : CBaseMemObj(),
                                                                                 TLnkLstNode<CIpc::CConnectionBase>()
{
  nCheckTag1 = CHECKTAG1;
  _InterlockedExchange(&nMutex, 0);
  lpIpc = _lpIpc;
  nClass = _nClass;
  _InterlockedExchange(&hErrorCode, S_OK);
  _InterlockedExchange(&nFlags, 0);
  _InterlockedExchange(&nNextReadOrder, 0);
  _InterlockedExchange(&nNextReadOrderToProcess, 1);
  _InterlockedExchange(&nNextWriteOrder, 0);
  _InterlockedExchange(&nNextWriteOrderToProcess, 1);
  _InterlockedExchange(&nRefCount, 1);
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
  _InterlockedExchange(&(sReceivedData.nMutex), 0);
  nCheckTag2 = CHECKTAG2;
  return;
}

CIpc::CConnectionBase::~CConnectionBase()
{
  CPacket *lpPacket;
  CLayer *lpLayer;

  nCheckTag1 = nCheckTag2 = 0;
  MX_ASSERT(sWriteTimeout.cActiveList.HasPending() == FALSE);
  //free packets
  while ((lpPacket = cRwList.DequeueFirst()) != NULL)
    lpIpc->FreePacket(lpPacket);
  while ((lpPacket = cReadedList.DequeueFirst()) != NULL)
    lpIpc->FreePacket(lpPacket);
  while ((lpPacket = cWritePendingList.DequeueFirst()) != NULL)
    lpIpc->FreePacket(lpPacket);
  //destroy layers
  while ((lpLayer=cLayersList.PopHead()) != NULL)
    delete lpLayer;
  //release queue
  if (lpTimedEventQueue != NULL)
    lpTimedEventQueue->Release();
  return;
}

VOID CIpc::CConnectionBase::ShutdownLink(__in BOOL bAbortive)
{
  CFastLock cWriteTimeoutLock(&(sWriteTimeout.nMutex));

  //cancel all queued events
  if (lpTimedEventQueue != NULL)
    sWriteTimeout.cActiveList.RunAction(MX_BIND_MEMBER_CALLBACK(&CConnectionBase::DoCancelEventsCallback, this));
  return;
}

VOID CIpc::CConnectionBase::DoCancelEventsCallback(__in __TEventArray &cEventsList)
{
  SIZE_T i, nCount;

  nCount = cEventsList.GetCount();
  for (i=0; i<nCount; i++)
    lpTimedEventQueue->Remove(cEventsList.GetElementAt(i));
  return;
}

HRESULT CIpc::CConnectionBase::SendMsg(__in LPCVOID lpData, __in SIZE_T nDataSize)
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
    _InterlockedIncrement(&nRefCount);
    _InterlockedIncrement(&nOutstandingWrites);
#ifdef MX_IPC_TIMING
    DebugPrint("%lu MX::CIpc::CConnectionBase::SendMsg) Clock=%lums / Ovr=0x%p / Type=%lu / Bytes=%lu\n",
               ::MxGetCurrentThreadId(), cHiResTimer.GetElapsedTimeMs(), lpPacket->GetOverlapped(), lpPacket->GetType(),
               lpPacket->GetBytesInUse());
#endif //MX_IPC_TIMING
    hRes = lpIpc->cDispatcherPool.Post(lpIpc->cDispatcherPoolPacketCallback, 0, lpPacket->GetOverlapped());
    if (FAILED(hRes))
    {
      MX_ASSERT_ALWAYS(__InterlockedRead(&nOutstandingWrites) >= 1);
      _InterlockedDecrement(&nOutstandingWrites);
      _InterlockedDecrement(&nRefCount);
      return hRes;
    }
    //next block
    s += (SIZE_T)dwToSendThisRound;
    nDataSize -= (SIZE_T)dwToSendThisRound;
  }
  return S_OK;
}

HRESULT CIpc::CConnectionBase::SendStream(__in CStream *lpStream)
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
  _InterlockedIncrement(&nRefCount);
  _InterlockedIncrement(&nOutstandingWrites);
#ifdef MX_IPC_TIMING
  DebugPrint("%lu MX::CIpc::CConnectionBase::SendStream) Clock=%lums / Ovr=0x%p / Type=%lu\n",
             ::MxGetCurrentThreadId(), cHiResTimer.GetElapsedTimeMs(), lpPacket->GetOverlapped(), lpPacket->GetType());
#endif //MX_IPC_TIMING
  hRes = lpIpc->cDispatcherPool.Post(lpIpc->cDispatcherPoolPacketCallback, 0, lpPacket->GetOverlapped());
  if (FAILED(hRes))
  {
    _InterlockedDecrement(&nOutstandingWrites);
    _InterlockedDecrement(&nRefCount);
  }
  return hRes;
}

HRESULT CIpc::CConnectionBase::AfterWriteSignal(__in OnAfterWriteSignalCallback cCallback, __in LPVOID lpCookie)
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
  _InterlockedIncrement(&nRefCount);
  _InterlockedIncrement(&nOutstandingWrites);
#ifdef MX_IPC_TIMING
  DebugPrint("%lu MX::CIpc::CConnectionBase::AfterWriteSignal) Clock=%lums / Ovr=0x%p / Type=%lu\n",
             ::MxGetCurrentThreadId(), cHiResTimer.GetElapsedTimeMs(), lpPacket->GetOverlapped(), lpPacket->GetType());
#endif //MX_IPC_TIMING
  hRes = lpIpc->cDispatcherPool.Post(lpIpc->cDispatcherPoolPacketCallback, 0, lpPacket->GetOverlapped());
  if (FAILED(hRes))
  {
    _InterlockedDecrement(&nOutstandingWrites);
    _InterlockedDecrement(&nRefCount);
  }
  return hRes;
}

HRESULT CIpc::CConnectionBase::WriteMsg(__in LPCVOID lpData, __in SIZE_T nDataSize)
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
          ShutdownLink(FAILED(hErrorCode));
#ifdef MX_IPC_TIMING
        DebugPrint("%lu MX::CIpc::CConnectionBase::GracefulShutdown D) Clock=%lums / This=0x%p\n",
                   ::MxGetCurrentThreadId(), cHiResTimer.GetElapsedTimeMs(), this);
#endif //MX_IPC_TIMING
        _InterlockedOr(&nFlags, FLAG_GracefulShutdown);
        //free packet
        cRwList.Remove(lpPacket);
        FreePacket(lpPacket);
        return S_OK;

      case S_OK:
      case 0x80070000|ERROR_IO_PENDING:
        _InterlockedIncrement(&nRefCount);
        break;

      default:
        if (_InterlockedDecrement(&nOutstandingWritesBeingSent) == 0)
          MarkLastWriteActivity(FALSE);
        if (_InterlockedDecrement(&nOutstandingWrites) == 0 && IsClosed() != FALSE)
          ShutdownLink(FAILED(hErrorCode));
        return hRes;
    }
  }
  return S_OK;
}

VOID CIpc::CConnectionBase::Close(__in HRESULT hRes)
{
  //mark as closed
#ifdef MX_IPC_TIMING
  if ((_InterlockedOr(&nFlags, FLAG_Closed) & FLAG_Closed) == 0)
  {
    DebugPrint("%lu MX::CIpc::CConnectionBase::Close) Clock=%lums / This=0x%p / Res=0x%08X\n",
               ::MxGetCurrentThreadId(), cHiResTimer.GetElapsedTimeMs(), this, hRes);
  }
#else //MX_IPC_TIMING
  _InterlockedOr(&nFlags, FLAG_Closed);
#endif //MX_IPC_TIMING
  _InterlockedCompareExchange(&hErrorCode, hRes, S_OK);
  if (hRes != S_OK || __InterlockedRead(&nOutstandingWrites) == 0)
    ShutdownLink(FAILED(__InterlockedRead(&hErrorCode)));
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
  return ((__InterlockedRead(&nFlags) & (FLAG_GracefulShutdown|FLAG_Closed)) != 0) ? TRUE : FALSE;
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
  _InterlockedIncrement(&nRefCount);
#ifdef MX_IPC_TIMING
  DebugPrint("%lu MX::CIpc::CConnectionBase::HandleConnected) Clock=%lums / Ovr=0x%p / Type=%lu\n",
             ::MxGetCurrentThreadId(), cHiResTimer.GetElapsedTimeMs(), lpPacket->GetOverlapped(),
             lpPacket->GetType());
#endif //MX_IPC_TIMING
  hRes = lpIpc->cDispatcherPool.Post(lpIpc->cDispatcherPoolPacketCallback, 0, lpPacket->GetOverlapped());
  if (FAILED(hRes))
  {
    cRwList.Remove(lpPacket);
    FreePacket(lpPacket);
    _InterlockedDecrement(&nRefCount);
    return hRes;
  }
  //done
  return S_OK;
}

CIpc::CPacket* CIpc::CConnectionBase::GetPacket(__in CPacket::eType nType)
{
  return lpIpc->GetPacket(this, nType);
}

VOID CIpc::CConnectionBase::FreePacket(__in CPacket *lpPacket)
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

HRESULT CIpc::CConnectionBase::DoRead(__in SIZE_T nPacketsCount, __in BOOL bZeroRead, __in_opt CPacket *lpReusePacket)
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
#ifdef MX_IPC_TIMING
        DebugPrint("%lu MX::CIpc::CConnectionBase::GracefulShutdown E) Clock=%lums / This=0x%p\n",
                   ::MxGetCurrentThreadId(), cHiResTimer.GetElapsedTimeMs(), this);
#endif //MX_IPC_TIMING
        _InterlockedOr(&nFlags, FLAG_GracefulShutdown);
        //free packet
        cRwList.Remove(lpPacket);
        FreePacket(lpPacket);
        hRes = S_OK;
        break;

      case S_OK:
      case 0x80070000|ERROR_IO_PENDING:
        _InterlockedIncrement(&nRefCount);
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

BOOL CIpc::CConnectionBase::AddRefIfStillValid()
{
  return (lpIpc->CheckAndGetConnection((HANDLE)this) != NULL) ? TRUE : FALSE;
}

VOID CIpc::CConnectionBase::ReleaseMySelf()
{
  lpIpc->ReleaseAndRemoveConnectionIfClosed(this);
  return;
}

HRESULT CIpc::CConnectionBase::SendDataToLayer(__in LPCVOID lpMsg, __in SIZE_T nMsgSize, __in CLayer *lpCurrLayer,
                                               __in BOOL bIsMsg)
{
  CLayer *lpLayer;
  HRESULT hRes = S_OK;

  if (lpMsg == NULL && nMsgSize > 0)
    return E_POINTER;
  if (nMsgSize == 0)
    return S_OK;
  if (bIsMsg == FALSE)
  {
    lpLayer = lpCurrLayer->GetNextEntry();
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
    lpLayer = lpCurrLayer->GetPrevEntry();
    hRes = (lpLayer != NULL) ? (lpLayer->OnSendMsg(lpMsg, nMsgSize)) : WriteMsg(lpMsg, nMsgSize);
  }
  return hRes;
}

HRESULT CIpc::CConnectionBase::MarkLastWriteActivity(__in BOOL bSet)
{
  CFastLock cWriteTimeoutLock(&(sWriteTimeout.nMutex));
  CTimedEventQueue::CEvent *lpEvent;

  if (IsClosed() != FALSE)
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
    //add to list
    if (sWriteTimeout.cActiveList.Add(lpEvent) == FALSE)
    {
      lpEvent->Release();
      return E_OUTOFMEMORY;
    }
    //add event
    hRes = lpTimedEventQueue->Add(lpEvent, dwWriteTimeoutMs);
    if (FAILED(hRes))
    {
      sWriteTimeout.cActiveList.Remove(lpEvent);
      lpEvent->Release();
      return hRes;
    }
    _InterlockedIncrement(&nRefCount);
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

VOID CIpc::CConnectionBase::OnWriteTimeout(__in CTimedEventQueue::CEvent *lpEvent)
{
  {
    CFastLock cWriteTimeoutLock(&(sWriteTimeout.nMutex));

    sWriteTimeout.cActiveList.Remove(lpEvent);
  }
  //close connection if not cancelled
  if (lpEvent->IsCanceled() == FALSE)
    Close(MX_E_Timeout);
  //release event and myself
  lpEvent->Release();
  ReleaseMySelf();
  return;
}

//-----------------------------------------------------------

HANDLE CIpc::CLayer::GetConn()
{
  return (HANDLE)lpConn;
}

HRESULT CIpc::CLayer::SendProcessedDataToNextLayer(__in LPCVOID lpMsg, __in SIZE_T nMsgSize)
{
  return reinterpret_cast<CIpc::CConnectionBase*>(lpConn)->SendDataToLayer(lpMsg, nMsgSize, this, FALSE);
}

HRESULT CIpc::CLayer::SendMsgToNextLayer(__in LPCVOID lpMsg, __in SIZE_T nMsgSize)
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
    lpConn->ReleaseMySelf();
  }
  return;
}

//-----------------------------------------------------------

CIpc::CAutoMultiSendLock::CAutoMultiSendLock(__in CMultiSendLock *_lpLock) : CBaseMemObj()
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
