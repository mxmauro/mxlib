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

#define __EPSILON                                   0.00001f

//-----------------------------------------------------------

typedef struct {
  MX::CLoggable *lpLogger;;
  LPCWSTR szDescW;
} DEBUGPRINT_DATA, *LPDEBUGPRINT_DATA;

//-----------------------------------------------------------

static int DebugPrintSslError(const char *str, size_t len, void *u);

static BIO_METHOD* BIO_circular_buffer_mem();

static int _X509_STORE_get1_issuer(X509 **issuer, X509_STORE_CTX *ctx, X509 *x);
static STACK_OF(X509)* _X509_STORE_get1_certs(X509_STORE_CTX *ctx, X509_NAME *nm);
static STACK_OF(X509_CRL)* _X509_STORE_get1_crls(X509_STORE_CTX *ctx, X509_NAME *nm);
static X509* _lookup_cert_by_subject(_In_ MX::CSslCertificateArray *lpCertArray, _In_ X509_NAME *name);
static X509_CRL* _lookup_crl_by_subject(_In_ MX::CSslCertificateArray *lpCertArray, _In_ X509_NAME *name);

//-----------------------------------------------------------

namespace MX {

CIpc::CConnectionBase::CConnectionBase(_In_ CIpc *_lpIpc, _In_ CIpc::eConnectionClass _nClass) :
                       TRefCounted<CBaseMemObj>()
{
  _InterlockedIncrement(&(_lpIpc->sConnections.nCount));

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
  FastLock_Initialize(&nMutex);
  FastLock_Initialize(&(sSsl.nMutex));
  sSsl.lpCtx = NULL;
  sSsl.lpSession = NULL;
  sSsl.lpInBio = sSsl.lpOutBio = NULL;
  FastLock_Initialize(&(sReceivedData.nMutex));
  FastLock_Initialize(&(sFreePackets32768.nMutex));
  FastLock_Initialize(&(sFreePackets4096.nMutex));
  FastLock_Initialize(&(sReadPackets.nMutex));
  FastLock_Initialize(&(sInUsePackets.nMutex));
  FastLock_Initialize(&(sPendingWritePackets.nMutex));
  sPendingWritePackets.bHasRequeuedPacket = FALSE;
  sPendingWritePackets.lpRequeuedPacket = NULL;
  return;
}

CIpc::CConnectionBase::~CConnectionBase()
{
  CPacketBase *lpPacket;

  {
    CAutoSlimRWLExclusive cConnListLock(&(lpIpc->sConnections.sRwMutex));

    cTreeNode.Remove();
  }
  lpIpc->FireOnDestroy(this);

  //free packets
  while ((lpPacket = sInUsePackets.cList.DequeueFirst()) != NULL)
    lpIpc->FreePacket(lpPacket);
  //----
  while ((lpPacket = sReadPackets.cList.DequeueFirst()) != NULL)
    lpIpc->FreePacket(lpPacket);
  //----
  if (sPendingWritePackets.lpRequeuedPacket != NULL)
    lpIpc->FreePacket(sPendingWritePackets.lpRequeuedPacket);
  while ((lpPacket = sPendingWritePackets.cList.DequeueFirst()) != NULL)
    lpIpc->FreePacket(lpPacket);
  //----
  while ((lpPacket = sFreePackets32768.cList.DequeueFirst()) != NULL)
    lpIpc->FreePacket(lpPacket);
  while ((lpPacket = sFreePackets4096.cList.DequeueFirst()) != NULL)
    lpIpc->FreePacket(lpPacket);

  //destroy ssl
  if (sSsl.lpSession != NULL)
  {
    SSL_set_session(sSsl.lpSession, NULL);
    SSL_free(sSsl.lpSession);
  }

  //done
  _InterlockedDecrement(&(lpIpc->sConnections.nCount));
  return;
}

VOID CIpc::CConnectionBase::ShutdownLink(_In_ BOOL bAbortive)
{
  return;
}

HRESULT CIpc::CConnectionBase::SendMsg(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize)
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
    lpPacket = GetPacket(CIpc::CPacketBase::TypeWriteRequest, ((nDataSize > 8192) ? 32768 : 4096), FALSE);
    if (lpPacket == NULL)
      return E_OUTOFMEMORY;

    //fill buffer
    dwToSendThisRound = lpPacket->GetBufferSize();
    if (nDataSize < (SIZE_T)dwToSendThisRound)
      dwToSendThisRound = (DWORD)nDataSize;
    lpPacket->SetBytesInUse(dwToSendThisRound);
    ::MxMemCopy(lpPacket->GetBuffer(), s, (SIZE_T)dwToSendThisRound);
    if (lpIpc->ShouldLog(1) != FALSE)
    {
      cLogTimer.Mark();
      lpIpc->Log(L"CIpc::SendMsg) Clock=%lums / Ovr=0x%p / Type=%lu / Bytes=%lu", cLogTimer.GetElapsedTimeMs(),
                 lpPacket->GetOverlapped(), lpPacket->GetType(), lpPacket->GetBytesInUse());
      cLogTimer.ResetToLastMark();
    }

    //send packet
    lpPacket->SetOrder(_InterlockedIncrement(&nNextWriteOrder));
    {
      CFastLock cListLock(&(sInUsePackets.nMutex));

      sInUsePackets.cList.QueueLast(lpPacket);
    }
    AddRef();
    _InterlockedIncrement(&nOutgoingWrites); //NOTE: this generates a full fence
    hRes = lpIpc->cDispatcherPool.Post(lpIpc->cDispatcherPoolPacketCallback, 0, lpPacket->GetOverlapped());
    if (FAILED(hRes))
    {
      MX_ASSERT_ALWAYS(__InterlockedRead(&nOutgoingWrites) >= 1);
      DecrementOutgoingWrites();
      {
        CFastLock cListLock(&(sInUsePackets.nMutex));

        sInUsePackets.cList.Remove(lpPacket);
      }
      FreePacket(lpPacket);
      Release();
      return hRes;
    }

    //next block
    s += (SIZE_T)dwToSendThisRound;
    nDataSize -= (SIZE_T)dwToSendThisRound;
  }

  //done
  return S_OK;
}

HRESULT CIpc::CConnectionBase::SendStream(_In_ CStream *lpStream)
{
  CPacketBase *lpPacket;
  HRESULT hRes;

  MX_ASSERT(lpStream != NULL);

  //get a free buffer
  lpPacket = GetPacket(CIpc::CPacketBase::TypeWriteRequest, 0, FALSE);
  if (lpPacket == NULL)
    return E_OUTOFMEMORY;
  lpPacket->SetStream(lpStream);

  //send packet
  lpPacket->SetOrder(_InterlockedIncrement(&nNextWriteOrder));
  {
    CFastLock cListLock(&(sInUsePackets.nMutex));

    sInUsePackets.cList.QueueLast(lpPacket);
  }
  AddRef();
  _InterlockedIncrement(&nOutgoingWrites); //NOTE: this generates a full fence
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
    DecrementOutgoingWrites();
    {
      CFastLock cListLock(&(sInUsePackets.nMutex));

      sInUsePackets.cList.Remove(lpPacket);
    }
    FreePacket(lpPacket);
    Release();
  }

  //done
  return hRes;
}

HRESULT CIpc::CConnectionBase::AfterWriteSignal(_In_ OnAfterWriteSignalCallback cCallback, _In_opt_ LPVOID lpCookie)
{
  CPacketBase *lpPacket;
  HRESULT hRes;

  MX_ASSERT(cCallback != false);

  //get a free buffer
  lpPacket = GetPacket(CIpc::CPacketBase::TypeWriteRequest, 0, FALSE);
  if (lpPacket == NULL)
    return E_OUTOFMEMORY;
  lpPacket->SetAfterWriteSignalCallback(cCallback);
  lpPacket->SetUserData(lpCookie);
  hRes = AfterWriteSignal(lpPacket);

  //done
  return hRes;
}

HRESULT CIpc::CConnectionBase::AfterWriteSignal(_In_ CPacketBase *lpPacket)
{
  HRESULT hRes;

  //send packet
  lpPacket->SetOrder(_InterlockedIncrement(&nNextWriteOrder));
  {
    CFastLock cListLock(&(sInUsePackets.nMutex));

    sInUsePackets.cList.QueueLast(lpPacket);
  }
  AddRef();
  _InterlockedIncrement(&nOutgoingWrites); //NOTE: this generates a full fence
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
    DecrementOutgoingWrites();
    {
      CFastLock cListLock(&(sInUsePackets.nMutex));

      sInUsePackets.cList.Remove(lpPacket);
    }
    FreePacket(lpPacket);
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
  lpPacket = GetPacket(CIpc::CPacketBase::TypeResumeIoProcessing, 0, FALSE);
  if (lpPacket == NULL)
    return E_OUTOFMEMORY;

  //send packet
  lpPacket->SetOrder((bInput != FALSE) ? 1 : 0);
  {
    CFastLock cListLock(&(sInUsePackets.nMutex));

    sInUsePackets.cList.QueueLast(lpPacket);
  }
  AddRef(); //NOTE: this generates a full fence
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
    {
      CFastLock cListLock(&(sInUsePackets.nMutex));

      sInUsePackets.cList.Remove(lpPacket);
    }
    FreePacket(lpPacket);
    Release();
  }

  //done
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
  {
    ShutdownLink(FAILED(__InterlockedRead(&hrErrorCode)));
  }

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
  return ((__InterlockedRead(&(const_cast<CIpc::CConnectionBase *>(this)->nFlags)) & FLAG_GracefulShutdown) != 0)
    ? TRUE : FALSE;
}

BOOL CIpc::CConnectionBase::IsClosed() const
{
  return ((__InterlockedRead(&(const_cast<CIpc::CConnectionBase *>(this)->nFlags)) & FLAG_Closed) != 0) ? TRUE : FALSE;
}

BOOL CIpc::CConnectionBase::IsClosedOrGracefulShutdown() const
{
  return ((__InterlockedRead(&(const_cast<CIpc::CConnectionBase *>(this)->nFlags)) &
           (FLAG_GracefulShutdown | FLAG_Closed)) != 0) ? TRUE : FALSE;
}

BOOL CIpc::CConnectionBase::IsConnected() const
{
  return ((__InterlockedRead(&(const_cast<CIpc::CConnectionBase *>(this)->nFlags)) & FLAG_Connected) != 0)
    ? TRUE : FALSE;
}

CIpc *CIpc::CConnectionBase::GetIpc() const
{
  return lpIpc;
}

HRESULT CIpc::CConnectionBase::GetErrorCode() const
{
  return (HRESULT)__InterlockedRead(&(const_cast<CIpc::CConnectionBase *>(this)->hrErrorCode));
}

HRESULT CIpc::CConnectionBase::HandleConnected()
{
  CPacketBase *lpPacket;
  HRESULT hRes;

  //mark as connected
#ifdef _DEBUG
  MX_ASSERT((_InterlockedOr(&nFlags, FLAG_Connected) & FLAG_Connected) == 0);
#else //_DEBUG
  _InterlockedOr(&nFlags, FLAG_Connected);
#endif //_DEBUG

  //initialize read/write stats
  cReadStats.HandleConnected();
  cWriteStats.HandleConnected();

  //send initial setup packet
  lpPacket = GetPacket(CPacketBase::TypeInitialSetup, 0, FALSE);
  if (lpPacket == NULL)
    return E_OUTOFMEMORY;
  {
    CFastLock cListLock(&(sInUsePackets.nMutex));

    sInUsePackets.cList.QueueLast(lpPacket);
  }
  AddRef(); //NOTE: this generates a full fence
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
    {
      CFastLock cListLock(&(sInUsePackets.nMutex));

      sInUsePackets.cList.Remove(lpPacket);
    }
    FreePacket(lpPacket);
    Release();
    return hRes;
  }

  //done
  return S_OK;
}

HRESULT CIpc::CConnectionBase::HandleIncomingPackets()
{
  CPacketBase *lpPacket;
  HRESULT hRes = S_OK;

  //get next sequenced block
  while (SUCCEEDED(hRes) && lpIpc->IsShuttingDown() == FALSE)
  {
    {
      CFastLock cListLock(&(sReadPackets.nMutex));

      lpPacket = sReadPackets.cList.Dequeue(__InterlockedRead(&nNextReadOrderToProcess));
    }
    if (lpPacket == NULL)
      break;

    if ((__InterlockedRead(&nFlags) & FLAG_HasSSL) != 0)
    {
      hRes = HandleSslInput(lpPacket);
    }
    else
    {
      {
        CFastLock cRecBufLock(&(sReceivedData.nMutex));

        hRes = sReceivedData.cBuffer.Write(lpPacket->GetBuffer(), (SIZE_T)(lpPacket->GetBytesInUse()));
      }
      if (SUCCEEDED(hRes))
        _InterlockedOr(&nFlags, FLAG_NewReceivedDataAvailable);
    }

    FreePacket(lpPacket);
    _InterlockedIncrement(&nNextReadOrderToProcess);
  }
  //done
  return hRes;
}

HRESULT CIpc::CConnectionBase::HandleOutgoingPackets()
{
  CPacketBase *lpPacket;
  HRESULT hRes = S_OK;

  //increment outstanding writes do keep the connection open while the chain is built
  IncrementOutgoingWrites();

  //process queued packets if output processing is not paused
  while (SUCCEEDED(hRes) && (__InterlockedRead(&nFlags) & FLAG_OutputProcessingPaused) == 0 &&
         (DWORD)__InterlockedRead(&nOutgoingBytes) < lpIpc->dwMaxOutgoingBytes)
  {
    BOOL bFromRequeue;

    //get next sequenced block to write
    {
      CFastLock cListLock(&(sPendingWritePackets.nMutex));

      if (sPendingWritePackets.bHasRequeuedPacket != FALSE)
      {
        //NOTE: 'bHasRequeuedPacket' can be TRUE but `lpRequeuedPacket` is NULL
        //      This means another thread is processing the requeued packet so
        //      we shall exit the queue below
        lpPacket = sPendingWritePackets.lpRequeuedPacket;
        sPendingWritePackets.lpRequeuedPacket = NULL;
        bFromRequeue = TRUE;
      }
      else
      {
        bFromRequeue = FALSE;
        lpPacket = sPendingWritePackets.cList.Dequeue((ULONG)__InterlockedRead(&nNextWriteOrderToProcess));
      }
    }
    if (lpPacket == NULL)
      break;

    if (lpPacket->HasAfterWriteSignalCallback() != FALSE) //process an after-write signal?
    {
      //if some chain must be written, requeue and wait
      if (__InterlockedRead(&nOutgoingBytes) == 0)
      {
        //call callback
        TAutoRefCounted<CUserData> _cUserData(cUserData);

        lpPacket->InvokeAfterWriteSignalCallback(lpIpc, cUserData);

        //free packet
        FreePacket(lpPacket);

        //decrement the outstanding writes incremented by the send
        DecrementOutgoingWrites();

        //go to next packet
        if (bFromRequeue == FALSE)
        {
          _InterlockedIncrement(&nNextWriteOrderToProcess);
        }
        else
        {
          CFastLock cListLock(&(sPendingWritePackets.nMutex));

          MX_ASSERT(sPendingWritePackets.bHasRequeuedPacket != FALSE);
          MX_ASSERT(sPendingWritePackets.lpRequeuedPacket == NULL);
          sPendingWritePackets.bHasRequeuedPacket = FALSE;
        }
      }
      else
      {
        //if some chain must be written or there still exists packets being sent, requeue and wait
        {
          CFastLock cListLock(&(sPendingWritePackets.nMutex));

          if (bFromRequeue != FALSE)
          {
            MX_ASSERT(sPendingWritePackets.bHasRequeuedPacket != FALSE);
            MX_ASSERT(sPendingWritePackets.lpRequeuedPacket == NULL);
            sPendingWritePackets.lpRequeuedPacket = lpPacket;
          }
          else
          {
            sPendingWritePackets.cList.QueueFirst(lpPacket);
          }
        }

        if (__InterlockedRead(&nOutgoingBytes) > 0)
          break;
      }
    }
    else if (lpPacket->HasStream() != FALSE) //process a stream?
    {
      CPacketBase *lpNewPacket = lpPacket->GetLinkedPacket();

      if (lpNewPacket != NULL)
      {
        lpPacket->LinkPacket(NULL); //break link
      }

      hRes = S_OK;
      while ((DWORD)__InterlockedRead(&nOutgoingBytes) < lpIpc->dwMaxOutgoingBytes)
      {
        //check if the stream has a linked packet (which is a packet that couldn't be sent in a previous round)
        hRes = (lpNewPacket == NULL) ? ReadStream(lpPacket, &lpNewPacket) : S_OK;

        //error or end of stream reached?
        if (FAILED(hRes))
        {
          if (hRes != MX_E_EndOfFileReached)
            break;

          //reached the end of the stream so free "stream" packet
          FreePacket(lpPacket);
          lpPacket = lpNewPacket = NULL;

          //decrement the outstanding writes incremented by the send
          DecrementOutgoingWrites();

          //go to next packet
          if (bFromRequeue == FALSE)
          {
            _InterlockedIncrement(&nNextWriteOrderToProcess);
          }
          else
          {
            CFastLock cListLock(&(sPendingWritePackets.nMutex));

            MX_ASSERT(sPendingWritePackets.bHasRequeuedPacket != FALSE);
            MX_ASSERT(sPendingWritePackets.lpRequeuedPacket == NULL);
            sPendingWritePackets.bHasRequeuedPacket = FALSE;
          }

          //we are done
          hRes = S_OK;
          break;
        }

        //process this outgoing packet (handler will take control of it unless S_FALSE is returned)
        if ((__InterlockedRead(&nFlags) & FLAG_HasSSL) != 0)
        {
          hRes = HandleSslOutput(lpNewPacket);
        }
        else
        {
          hRes = DoWrite(lpNewPacket); //NOTE: Currently never returns S_FALSE
        }
        if (FAILED(hRes))
        {
          //on error, readed packet was discarded by DoWrite/HandleSslOutput
          lpNewPacket = NULL;
          break;
        }

        if (hRes == S_FALSE)
        {
          //if we couldn't send the packet due to bandwidth, stop
          hRes = S_OK;
          break;
        }

        //packet is not controlled by us anymore (also clean for loop)
        lpNewPacket = NULL;
      }

      if (SUCCEEDED(hRes))
      {
        //at this point we may (or not) have the stream packet
        //if we don't, just do nothing because the stream has
        //reached to it's end and was handled in the loop

        if (lpPacket != NULL)
        {
          //requeue the stream packet
          CFastLock cListLock(&(sPendingWritePackets.nMutex));

          //if we have a data packet, means it couldn't be sent (due to bandwidth) do link it
          if (lpNewPacket != NULL)
          {
            lpPacket->LinkPacket(lpNewPacket);
          }

          //requeue the stream
          if (bFromRequeue != FALSE)
          {
            MX_ASSERT(sPendingWritePackets.bHasRequeuedPacket != FALSE);
            MX_ASSERT(sPendingWritePackets.lpRequeuedPacket == NULL);
            sPendingWritePackets.lpRequeuedPacket = lpPacket;
          }
          else
          {
            sPendingWritePackets.cList.QueueFirst(lpPacket);
          }
        }
      }
      else
      {
        //we hit an error so no problem to free the stream packet and the readed data (if any)
        if (lpPacket != NULL)
          FreePacket(lpPacket);
        if (lpNewPacket != NULL)
          FreePacket(lpNewPacket);
      }
    }
    else
    {
      //process this outgoing normal packet (handler will take control of it unless S_FALSE is returned)
      if ((__InterlockedRead(&nFlags) & FLAG_HasSSL) != 0)
      {
        hRes = HandleSslOutput(lpPacket);
      }
      else
      {
        hRes = DoWrite(lpPacket); //NOTE: Currently never returns S_FALSE
      }

      if (hRes == S_FALSE)
      {
        //requeue this packet as pending because couldn't be sent
        CFastLock cListLock(&(sPendingWritePackets.nMutex));

        if (bFromRequeue != FALSE)
        {
          MX_ASSERT(sPendingWritePackets.bHasRequeuedPacket != FALSE);
          MX_ASSERT(sPendingWritePackets.lpRequeuedPacket == NULL);
          sPendingWritePackets.lpRequeuedPacket = lpPacket;
        }
        else
        {
          sPendingWritePackets.cList.QueueFirst(lpPacket);
        }
        hRes = S_OK;
        break;
      }

      //decrement the outstanding writes incremented by the send
      DecrementOutgoingWrites();

      //go to next packet
      if (bFromRequeue == FALSE)
      {
        _InterlockedIncrement(&nNextWriteOrderToProcess);
      }
      else
      {
        CFastLock cListLock(&(sPendingWritePackets.nMutex));

        MX_ASSERT(sPendingWritePackets.bHasRequeuedPacket != FALSE);
        MX_ASSERT(sPendingWritePackets.lpRequeuedPacket == NULL);
        sPendingWritePackets.bHasRequeuedPacket = FALSE;
      }
    }
  }

  //decrement the outstanding write incremented at the beginning of this section and close if needed
  DecrementOutgoingWrites();

  //done
  return hRes;
}

VOID CIpc::CConnectionBase::HandleSslShutdown()
{
  if ((__InterlockedRead(&nFlags) & FLAG_HasSSL) != 0)
  {
    CFastLock cSslLock(&(sSsl.nMutex));

    //shutdown
    ERR_clear_error();
    SSL_shutdown(sSsl.lpSession);

    //process ssl
    ProcessSsl(FALSE);
  }
  return;
}

HRESULT CIpc::CConnectionBase::HandleSslInput(_In_ CPacketBase *lpPacket)
{
  CFastLock cSslLock(&(sSsl.nMutex));
  int size = (int)(lpPacket->GetBytesInUse());

  if (BIO_write(sSsl.lpInBio, lpPacket->GetBuffer(), size) != size)
    return E_OUTOFMEMORY;
  return ProcessSsl(TRUE);
}

HRESULT CIpc::CConnectionBase::HandleSslOutput(_In_ CPacketBase *lpPacket)
{
  CFastLock cSslLock(&(sSsl.nMutex));
  int err;

  ERR_clear_error();
  err = SSL_write(sSsl.lpSession, lpPacket->GetBuffer(), (int)(lpPacket->GetBytesInUse()));
  if (err <= 0)
  {
    switch (SSL_get_error(sSsl.lpSession, err))
    {
      case SSL_ERROR_WANT_READ:
        _InterlockedOr(&nFlags, FLAG_SslWantRead);
        //fall into...

      case SSL_ERROR_WANT_WRITE:
      case SSL_ERROR_NONE:
        break;

      default:
        FreePacket(lpPacket);

        if (lpIpc->ShouldLog(1) != FALSE)
        {
          DEBUGPRINT_DATA sData = { lpIpc, L"HandleSslOutput" };
          ERR_print_errors_cb(&DebugPrintSslError, &sData);
        }
        return MX_E_InvalidData;
    }

    return S_FALSE;
  }

  FreePacket(lpPacket);

  //done
  return ProcessSsl(TRUE);
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
  {
    ShutdownLink(FAILED(__InterlockedRead(&hrErrorCode)));
  }
  return;
}

CIpc::CPacketBase* CIpc::CConnectionBase::GetPacket(_In_ CPacketBase::eType nType, _In_ SIZE_T nDesiredSize,
                                                    _In_ BOOL bRealSize)
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
    lpPacket = lpIpc->GetPacket(this, nType, nDesiredSize, FALSE);
  }
  else
  {
    lpPacket->Reset(nType, this);
    //DebugPrint("GetPacket %lums (%lu)\n", ::GetTickCount(), nType);
  }
  return lpPacket;
}

VOID CIpc::CConnectionBase::FreePacket(_In_ CPacketBase *lpPacket)
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

CIoCompletionPortThreadPool &CIpc::CConnectionBase::GetDispatcherPool()
{
  return lpIpc->cDispatcherPool;
}

CIoCompletionPortThreadPool::OnPacketCallback &CIpc::CConnectionBase::GetDispatcherPoolPacketCallback()
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
    lpPacket = GetPacket(CPacketBase::TypeZeroRead, 4096, FALSE);
    if (lpPacket == NULL)
      return E_OUTOFMEMORY;
    {
      CFastLock cListLock(&(sInUsePackets.nMutex));

      sInUsePackets.cList.QueueLast(lpPacket);
    }
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
          {
            CFastLock cListLock(&(sInUsePackets.nMutex));

            sInUsePackets.cList.Remove(lpPacket);
          }
          FreePacket(lpPacket);
          Release();
        }
        return S_OK;

      case S_OK:
        lpPacket->SetBytesInUse(dwRead);
        {
          CFastLock cListLock(&(sInUsePackets.nMutex));

          sInUsePackets.cList.Remove(lpPacket);
        }
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
    //check if closed
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
      lpPacket = GetPacket(CPacketBase::TypeRead, 4096, FALSE);
      if (lpPacket == NULL)
        return E_OUTOFMEMORY;
    }
    lpPacket->SetBytesInUse(lpPacket->GetBufferSize());
    _InterlockedIncrement(&nIncomingReads);
    AddRef();
    {
      {
        CFastLock cListLock(&(sInUsePackets.nMutex));

        sInUsePackets.cList.QueueLast(lpPacket);
      }
      {
        CFastLock cReadLock(&nMutex);

        lpPacket->SetOrder(_InterlockedIncrement(&nNextReadOrder));
        hRes = SendReadPacket(lpPacket, &dwRead); //NOTE: Entering the fastlock generates a full fence
      }
    }
    switch (hRes)
    {
      case S_FALSE:
        MX_ASSERT_ALWAYS(_InterlockedDecrement(&nIncomingReads) >= 0);
        if (((_InterlockedOr(&nFlags, FLAG_GracefulShutdown) & FLAG_GracefulShutdown) == 0) &&
            lpIpc->ShouldLog(1) != FALSE)
        {
          cLogTimer.Mark();
          lpIpc->Log(L"CIpc::GracefulShutdown F) Clock=%lums / This=0x%p", cLogTimer.GetElapsedTimeMs(), this);
          cLogTimer.ResetToLastMark();
        }

        //free packet
        {
          CFastLock cListLock(&(sInUsePackets.nMutex));

          sInUsePackets.cList.Remove(lpPacket);
        }
        FreePacket(lpPacket);
        Release();
        return S_OK;

      case S_OK:
        lpPacket->SetBytesInUse(dwRead);
        {
          CFastLock cListLock(&(sInUsePackets.nMutex));

          sInUsePackets.cList.Remove(lpPacket);
        }
        cQueuedPacketsList.QueueLast(lpPacket);
        break;

      case 0x80070000 | ERROR_IO_PENDING:
        break;

      default: //error
        MX_ASSERT_ALWAYS(_InterlockedDecrement(&nIncomingReads) >= 0);
        Release();
        return hRes;
    }
    nPacketsCount--;
  }

  //done
  return S_OK;
}

HRESULT CIpc::CConnectionBase::DoWrite(_In_ CPacketBase *lpPacket)
{
  DWORD dwTotalBytes, dwWritten;
  HRESULT hRes;

  /*
  if (IsClosed() != FALSE)
  {
    FreePacket(lpPacket);
    return MX_E_Cancelled;
  }
  */

  dwTotalBytes = lpPacket->GetBytesInUse();

  //do real write
  {
    CFastLock cListLock(&(sInUsePackets.nMutex));

    sInUsePackets.cList.QueueLast(lpPacket);
  }
  lpPacket->SetType(CPacketBase::TypeWrite);
  lpPacket->SetUserDataDW(dwTotalBytes);
  _InterlockedIncrement(&nOutgoingWrites);
  _InterlockedExchangeAdd(&nOutgoingBytes, (LONG)dwTotalBytes);
  AddRef(); //NOTE: this generates a full fence
  hRes = SendWritePacket(lpPacket, &dwWritten);
  switch (hRes)
  {
    case S_FALSE:
      _InterlockedExchangeAdd(&nOutgoingBytes, -((LONG)dwTotalBytes));
      DecrementOutgoingWrites();

      if ((_InterlockedOr(&nFlags, FLAG_GracefulShutdown) & FLAG_GracefulShutdown) == 0 &&
          lpIpc->ShouldLog(1) != FALSE)
      {
        cLogTimer.Mark();
        lpIpc->Log(L"CIpc::GracefulShutdown B) Clock=%lums / This=0x%p", cLogTimer.GetElapsedTimeMs(), this);
        cLogTimer.ResetToLastMark();
      }

      //free packet
      {
        CFastLock cListLock(&(sInUsePackets.nMutex));

        sInUsePackets.cList.Remove(lpPacket);
      }
      FreePacket(lpPacket);

      //release connection
      Release();
      hRes = S_OK;
      break;

    case S_OK:
      _InterlockedExchangeAdd(&nOutgoingBytes, -((LONG)dwTotalBytes));
      DecrementOutgoingWrites();

      //free packet
      {
        CFastLock cListLock(&(sInUsePackets.nMutex));

        sInUsePackets.cList.Remove(lpPacket);
      }
      FreePacket(lpPacket);

      //release connection
      Release();

      cWriteStats.Update(dwTotalBytes);

      //HandleOutgoingPackets();
      break;

    case 0x80070000 | ERROR_IO_PENDING:
      hRes = S_OK;
      break;

    default:
      _InterlockedExchangeAdd(&nOutgoingBytes, -((LONG)dwTotalBytes));
      DecrementOutgoingWrites();

      //free packet
      {
        CFastLock cListLock(&(sInUsePackets.nMutex));

        sInUsePackets.cList.Remove(lpPacket);
      }
      FreePacket(lpPacket);

      //release connection
      Release();
      break;
  }

  //done
  return hRes;
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

HRESULT CIpc::CConnectionBase::SetupSsl(_In_opt_ LPCSTR szHostNameA, _In_opt_ CSslCertificateArray *lpCheckCertificates,
                                        _In_opt_ CSslCertificate *lpSelfCert, _In_opt_ CEncryptionKey *lpPrivKey,
                                        _In_opt_ CDhParam *lpDhParam, _In_ int nSslOptions)
{
  HRESULT hRes = MX_E_AlreadyExists;

  if (nClass == CIpc::ConnectionClassServer && lpSelfCert == NULL)
    return E_POINTER;

  if ((__InterlockedRead(&nFlags) & FLAG_HasSSL) == 0)
  {
    CFastLock cLock(&nMutex);

    if ((__InterlockedRead(&nFlags) & FLAG_HasSSL) == 0)
    {
      SSL_CTX *lpCtx;
      SSL *lpSession = NULL;
      BIO *lpInBio = NULL, *lpOutBio = NULL;
      X509_STORE *lpStore;
      X509 *lpX509;
      EVP_PKEY *lpKey;
      DH *lpDH;

      hRes = Internals::OpenSSL::Init();
      if (FAILED(hRes))
        return hRes;

      //create SSL context
      ERR_clear_error();
      lpCtx = Internals::OpenSSL::GetSslContext((nClass == CIpc::ConnectionClassServer) ? TRUE : FALSE);
      if (lpCtx == NULL)
        return E_OUTOFMEMORY;

      //create session
      lpSession = SSL_new(lpCtx);
      if (lpSession == NULL)
        return E_OUTOFMEMORY;

      //init some stuff
      if ((nSslOptions & (int)CIpc::SslOptionCheckCertificate) != 0)
      {
        _InterlockedOr(&nFlags, FLAG_SslCheckCertificate);
      }
      if ((nSslOptions & (int)CIpc::SslOptionAcceptSelfSigned) != 0)
      {
        _InterlockedOr(&nFlags, FLAG_SslAcceptSelfSigned);
      }
      SSL_set_verify(lpSession, (((nSslOptions & (int)CIpc::SslOptionCheckCertificate) != 0)
                                 ? (SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE) : SSL_VERIFY_NONE), NULL);
      SSL_set_verify_depth(lpSession, 4);
      SSL_set_options(lpSession, SSL_OP_NO_COMPRESSION | SSL_OP_LEGACY_SERVER_CONNECT | SSL_OP_NO_RENEGOTIATION |
                                 SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
      SSL_set_mode(lpSession, SSL_MODE_RELEASE_BUFFERS | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER | SSL_MODE_NO_AUTO_CHAIN);
      if (nClass == CIpc::ConnectionClassServer)
      {
        SSL_set_accept_state(lpSession);
      }
      else
      {
        SSL_set_connect_state(lpSession);
        if (szHostNameA != NULL && *szHostNameA != 0)
        {
          ERR_clear_error();
          if (SSL_set_tlsext_host_name(lpSession, szHostNameA) <= 0)
          {
            int err = ERR_get_error();

            SSL_free(lpSession);
            return (err == ((ERR_LIB_SSL << 24) | (SSL_F_SSL3_CTRL << 12) | SSL_R_SSL3_EXT_INVALID_SERVERNAME))
                   ? MX_E_InvalidData : E_OUTOFMEMORY;
          }
        }
      }

      //create i/o objects
      lpInBio = BIO_new(BIO_circular_buffer_mem());
      if (lpInBio == NULL)
      {
err_nomem:
        hRes = E_OUTOFMEMORY;
on_error:
        SSL_free(lpSession);
        return hRes;
      }
      lpOutBio = BIO_new(BIO_circular_buffer_mem());
      if (lpOutBio == NULL)
      {
        BIO_free(lpInBio);
        goto err_nomem;
      }
      SSL_set_bio(lpSession, lpInBio, lpOutBio);
      if (SSL_set_ex_data(lpSession, 0, (void *)this) <= 0)
        goto err_nomem;

      lpStore = X509_STORE_new();
      if (lpStore == NULL)
        goto err_nomem;
      SSL_ctrl(lpSession, SSL_CTRL_SET_CHAIN_CERT_STORE, 0, lpStore); //don't add reference
      SSL_ctrl(lpSession, SSL_CTRL_SET_VERIFY_CERT_STORE, 1, lpStore); //do add reference
      if (X509_STORE_set_ex_data(lpStore, 0, lpCheckCertificates) <= 0)
        goto err_nomem;
      X509_STORE_set_get_issuer(lpStore, &_X509_STORE_get1_issuer);
      X509_STORE_set_lookup_certs(lpStore, &_X509_STORE_get1_certs);
      X509_STORE_set_lookup_crls(lpStore, &_X509_STORE_get1_crls);

      //setup server/client certificate if provided
      if (lpSelfCert != NULL)
      {
        lpX509 = lpSelfCert->GetX509();
        if (lpX509 == NULL)
        {
          hRes = MX_E_NotReady;
          goto on_error;
        }

        ERR_clear_error();
        if (SSL_use_certificate(lpSession, lpX509) <= 0)
        {
          hRes = Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
          goto on_error;
        }

        //setup private key if provided
        if (lpPrivKey != NULL)
        {
          lpKey = lpPrivKey->GetPKey();
          if (lpKey == NULL)
          {
            hRes = MX_E_NotReady;
            goto on_error;
          }

          ERR_clear_error();
          if (SSL_use_PrivateKey(lpSession, lpKey) <= 0)
          {
            hRes = Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
            goto on_error;
          }

          if (SSL_check_private_key(lpSession) == 0)
          {
            hRes = MX_E_InvalidData;
            goto on_error;
          }
        }
      }

      if (lpDhParam != NULL)
      {
        lpDH = lpDhParam->GetDH();
        if (lpDH == NULL)
        {
          hRes = MX_E_NotReady;
          goto on_error;
        }

        ERR_clear_error();
        if (SSL_set_tmp_dh(lpSession, lpDH) == 0)
        {
          hRes = Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
          goto on_error;
        }
      }

      sSsl.cCertArray = lpCheckCertificates;
      sSsl.lpCtx = lpCtx;
      sSsl.lpSession = lpSession;
      sSsl.lpInBio = lpInBio;
      sSsl.lpOutBio = lpOutBio;

      _InterlockedOr(&nFlags, FLAG_HasSSL);

      hRes = S_OK;
    }
  }

  //done
  return hRes;
}

HRESULT CIpc::CConnectionBase::HandleSslStartup()
{
  if ((__InterlockedRead(&nFlags) & FLAG_HasSSL) != 0)
  {
    CFastLock cSslLock(&(sSsl.nMutex));

    return ProcessSsl(TRUE);
  }

  //done
  return S_OK;
}

HRESULT CIpc::CConnectionBase::ProcessSsl(_In_ BOOL bCanWrite)
{
  LONG nCurrFlags;
  BOOL bLoop;
  HRESULT hRes;

  do
  {
    hRes = S_OK;
    bLoop = FALSE;
    nCurrFlags = __InterlockedRead(&nFlags) & (FLAG_SslHandshakeCompleted | FLAG_SslWantRead);

    //process encrypted input
    hRes = ProcessSslIncomingData();
    if (FAILED(hRes))
      return hRes;
    if (hRes == S_OK)
    {
      bLoop = TRUE;
      nCurrFlags &= ~FLAG_SslWantRead;
      _InterlockedAnd(&nFlags, ~FLAG_SslWantRead);
    }

    //send encrypted output
    if (bCanWrite != FALSE)
    {
      hRes = ProcessSslEncryptedOutput();
      if (FAILED(hRes))
        return hRes;
      if (hRes == S_OK)
      {
        bLoop = TRUE;
      }
    }

    //process handshake
    if ((nCurrFlags & FLAG_SslHandshakeCompleted) == 0)
    {
      if (SSL_in_init(sSsl.lpSession) == 0)
      {
        _InterlockedOr(&nFlags, FLAG_SslHandshakeCompleted);
        nCurrFlags |= FLAG_SslHandshakeCompleted;

        hRes = HandleSslEndOfHandshake();
        if (FAILED(hRes))
          return hRes;

        //NOTE: Should we raise a callback?
        bLoop = TRUE;
      }
    }
  }
  while (bLoop != FALSE);

  //done
  return S_OK;
}

HRESULT CIpc::CConnectionBase::ProcessSslIncomingData()
{
  BYTE aBuffer[4096];
  int r;
  BOOL bSomethingProcessed;
  HRESULT hRes;

  bSomethingProcessed = FALSE;
  do
  {
    ERR_clear_error();
    r = SSL_read(sSsl.lpSession, aBuffer, (int)sizeof(aBuffer));
    if (r > 0)
    {
      {
        CFastLock cRecBufLock(&(sReceivedData.nMutex));

        hRes = sReceivedData.cBuffer.Write(aBuffer, (SIZE_T)r);
      }
      if (SUCCEEDED(hRes))
        _InterlockedOr(&nFlags, FLAG_NewReceivedDataAvailable);

      bSomethingProcessed = TRUE;
    }
  }
  while (r > 0);
  switch (SSL_get_error(sSsl.lpSession, r))
  {
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
    case SSL_ERROR_NONE:
    case SSL_ERROR_ZERO_RETURN:
      break;

    default:
      if (lpIpc->ShouldLog(1) != FALSE)
      {
        DEBUGPRINT_DATA sData = { lpIpc, L"ExecSslRead" };
        ERR_print_errors_cb(&DebugPrintSslError, &sData);
      }
      return MX_E_InvalidData;
  }
  //done
  return (bSomethingProcessed != FALSE) ? S_OK : S_FALSE;
}

HRESULT CIpc::CConnectionBase::ProcessSslEncryptedOutput()
{
  CPacketBase *lpPacket;
  BOOL bSomethingProcessed = FALSE;
  SIZE_T nAvailable;
  int nRead;
  HRESULT hRes = S_OK;

  bSomethingProcessed = FALSE;
  //send processed output from ssl engine
  while ((nAvailable = BIO_ctrl_pending(sSsl.lpOutBio)) > 0)
  {
    lpPacket = GetPacket(CIpc::CPacketBase::TypeWriteRequest, ((nAvailable > 8192) ? 32768 : 4096), FALSE);
    if (lpPacket == NULL)
      return E_OUTOFMEMORY;

    nRead = BIO_read(sSsl.lpOutBio, lpPacket->GetBuffer(), (int)(lpPacket->GetBufferSize()));
    if (nRead <= 0)
    {
      FreePacket(lpPacket);
      break;
    }
    lpPacket->SetBytesInUse((DWORD)nRead);

    //process this outgoing packet (handler will take control of it unless S_FALSE is returned)
    hRes = DoWrite(lpPacket);
    if (FAILED(hRes))
      return hRes;
    bSomethingProcessed = TRUE;
  }
  //done
  return (bSomethingProcessed != FALSE) ? S_OK : S_FALSE;
}

HRESULT CIpc::CConnectionBase::HandleSslEndOfHandshake()
{
  LONG _nFlags = __InterlockedRead(&nFlags);

  if ((_nFlags & FLAG_SslCheckCertificate) == 0)
    return S_OK;
  ERR_clear_error();
  switch (SSL_get_verify_result(sSsl.lpSession))
  {
    case X509_V_OK:
      return S_OK;

    case X509_V_ERR_OUT_OF_MEM:
      return E_OUTOFMEMORY;

    case X509_V_ERR_CERT_HAS_EXPIRED:
      return SEC_E_CERT_EXPIRED;

    case X509_V_ERR_CERT_REVOKED:
      return CRYPT_E_REVOKED;

    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
      return CRYPT_E_ISSUER_SERIALNUMBER;

    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
      return ((_nFlags & FLAG_SslAcceptSelfSigned) != 0) ? S_OK : CRYPT_E_SELF_SIGNED;

    case X509_V_ERR_INVALID_PURPOSE:
      return CERT_E_PURPOSE;

    case X509_V_ERR_INVALID_CA:
      return SEC_E_ISSUING_CA_UNTRUSTED;

    case X509_V_ERR_CERT_UNTRUSTED:
      return CERT_E_CHAINING;
  }
  return CRYPT_E_NO_TRUSTED_SIGNER;
  /*
  unsigned char *lpTempBuf[2];
  int r, nLen;
  X509 *lpPeer;
  BOOL bPeerCertIsValid;
  TAutoRefCounted<CSslCertificate> cPeerCert;
  HRESULT hRes;


  hRes = MX_E_NotReady;
  bPeerCertIsValid = (r == X509_V_OK) ? TRUE : FALSE;
  lpPeer = SSL_get_peer_certificate(sSsl.lpSession);
  if (lpPeer != NULL)
  {
    nLen = i2d_X509(lpPeer, NULL);
    lpTempBuf[0] = lpTempBuf[1] = (unsigned char *)OPENSSL_malloc(nLen);
    if (lpTempBuf[0] == NULL)
    {
      X509_free(lpPeer);
      return E_OUTOFMEMORY;
    }
    nLen = i2d_X509(lpPeer, &lpTempBuf[1]);
    X509_free(lpPeer);
    if (nLen < 0)
    {
      OPENSSL_free(lpTempBuf[0]);
      return MX_E_InvalidData;
    }

    cPeerCert.Attach(MX_DEBUG_NEW CSslCertificate());
    if (!cPeerCert)
    {
      OPENSSL_free(lpTempBuf[0]);
      return E_OUTOFMEMORY;
    }
    hRes = cPeerCert->InitializeFromDER(lpTempBuf[0], (SIZE_T)nLen);
    OPENSSL_free(lpTempBuf[0]);
    if (FAILED(hRes))
      return hRes;
  }

  //done
  return S_OK;
  */
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
  if (dwElapsedMs >= 500)
  {
    int i, count;
    ULONGLONG ullDiffBytes;

    cTimer.ResetToLastMark();

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

} //namespace MX

//-----------------------------------------------------------

static int DebugPrintSslError(const char *str, size_t len, void *u)
{
  while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r'))
    len--;

  ((LPDEBUGPRINT_DATA)u)->lpLogger->Log(L"IpcSslLayer/%s: Error: %.*S\n", ((LPDEBUGPRINT_DATA)u)->szDescW,
                                       (unsigned int)len, str);
  return 1;
}

//-----------------------------------------------------------

static int circular_buffer_create(BIO *bi)
{
  MX::CCircularBuffer *lpBuf = MX_DEBUG_NEW MX::CCircularBuffer();
  if (lpBuf == NULL)
    return 0;
  BIO_set_data(bi, lpBuf);
  BIO_set_init(bi, 1);
  BIO_set_shutdown(bi, 1);
  return 1;
}

static int circular_buffer_destroy(BIO *bi)
{
  MX::CCircularBuffer *lpBuf;

  if (bi == NULL)
    return 0;
  lpBuf = (MX::CCircularBuffer*)BIO_get_data(bi);
  if (lpBuf != NULL && BIO_get_shutdown(bi) != 0 && BIO_get_init(bi) != 0)
  {
    delete lpBuf;
    BIO_set_data(bi, NULL);
  }
  return 1;
}

static int circular_buffer_read_old(BIO *bi, char *out, int outl)
{
  MX::CCircularBuffer *lpBuf = (MX::CCircularBuffer *)BIO_get_data(bi);
  SIZE_T nAvailable;
  int ret;

  BIO_clear_retry_flags(bi);
  nAvailable = lpBuf->GetAvailableForRead();
  ret = (outl > 0 && (SIZE_T)outl > nAvailable) ? (int)nAvailable : outl;
  if (out != NULL && ret > 0)
  {
    ret = (int)(lpBuf->Read(out, (SIZE_T)ret));
  }
  else if (nAvailable == 0)
  {
    ret = -1;
    BIO_set_retry_read(bi);
  }
  return ret;
}

static int circular_buffer_read(BIO *bio, char *data, size_t datal, size_t *readbytes)
{
  int ret;

  if (datal > INT_MAX)
    datal = INT_MAX;
  ret = circular_buffer_read_old(bio, data, (int)datal);
  if (ret <= 0)
  {
    *readbytes = 0;
    return ret;
  }
  *readbytes = (size_t)ret;
  return 1;
}

static int circular_buffer_write_old(BIO *bi, const char *in, int inl)
{
  MX::CCircularBuffer *lpBuf;

  if (in == NULL)
  {
    BIOerr(BIO_F_MEM_WRITE, BIO_R_NULL_PARAMETER);
    return -1;
  }
  if (BIO_test_flags(bi, BIO_FLAGS_MEM_RDONLY))
  {
    BIOerr(BIO_F_MEM_WRITE, BIO_R_WRITE_TO_READ_ONLY_BIO);
    return -1;
  }

  BIO_clear_retry_flags(bi);
  lpBuf = (MX::CCircularBuffer*)BIO_get_data(bi);
  if (inl > 0)
  {
    if (FAILED(lpBuf->Write(in, (SIZE_T)inl)))
    {
      BIOerr(BIO_F_MEM_WRITE, ERR_R_MALLOC_FAILURE);
      return -1;
    }
  }
  return inl;
}

static int circular_buffer_write(BIO *bio, const char *data, size_t datal, size_t *written)
{
  int ret;

  if (datal > INT_MAX)
    datal = INT_MAX;
  ret = circular_buffer_write_old(bio, data, (int)datal);
  if (ret <= 0)
  {
    *written = 0;
    return ret;
  }
  *written = (size_t)ret;
  return 1;
}

static long circular_buffer_ctrl(BIO *bi, int cmd, long num, void *ptr)
{
  MX::CCircularBuffer *lpBuf = (MX::CCircularBuffer*)BIO_get_data(bi);
  switch (cmd)
  {
    case BIO_CTRL_RESET:
      lpBuf->SetBufferSize(0);
      return 1;

    case BIO_CTRL_GET_CLOSE:
      return (long)BIO_get_shutdown(bi);

    case BIO_CTRL_SET_CLOSE:
      BIO_set_shutdown(bi, (int)num);
      return 1;

    case BIO_CTRL_PENDING:
      {
      SIZE_T nAvail = lpBuf->GetAvailableForRead();

      if (nAvail > (SIZE_T)LONG_MAX)
        nAvail = (SIZE_T)LONG_MAX;
      return (long)nAvail;
      }
      break;

    case BIO_CTRL_DUP:
    case BIO_CTRL_FLUSH:
      return 1;
  }
  return 0;
}

static int circular_buffer_gets(BIO *bi, char *buf, int size)
{
  if (buf == NULL)
  {
    BIOerr(BIO_F_BIO_READ, BIO_R_NULL_PARAMETER);
    return -1;
  }
  BIO_clear_retry_flags(bi);
  if (size > 0)
  {
    MX::CCircularBuffer *lpBuf = (MX::CCircularBuffer *)BIO_get_data(bi);
    SIZE_T i, nToRead, nReaded;

    nToRead = (SIZE_T)size;
    nReaded = lpBuf->Peek(buf, nToRead);
    if (nReaded == 0)
    {
      *buf = 0;
      return 0;
    }
    for (i = 0; i < nReaded; i++)
    {
      if (buf[i] == '\n')
      {
        i++;
        break;
      }
    }
    lpBuf->AdvanceReadPtr(i);
    buf[i] = 0;
    return (int)i;
  }
  if (buf != NULL)
    *buf = '\0';
  return 0;
}

static int circular_buffer_puts(BIO *bi, const char *str)
{
  return circular_buffer_write_old(bi, str, (int)MX::StrLenA(str));
}

static BIO_METHOD *BIO_circular_buffer_mem()
{
  static const BIO_METHOD circular_buffer_mem = {
    BIO_TYPE_MEM,
    (char*)"circular memory buffer",
    &circular_buffer_write,
    &circular_buffer_write_old,
    &circular_buffer_read,
    &circular_buffer_read_old,
    &circular_buffer_puts,
    &circular_buffer_gets,
    &circular_buffer_ctrl,
    &circular_buffer_create,
    &circular_buffer_destroy,
    NULL
  };
  return (BIO_METHOD*)&circular_buffer_mem;
}

//-----------------------------------------------------------

static int _X509_STORE_get1_issuer(X509 **issuer, X509_STORE_CTX *ctx, X509 *x)
{
  MX::CSslCertificateArray *lpCertArray;
  X509 *cert;

  lpCertArray = (MX::CSslCertificateArray*)(::X509_STORE_get_ex_data(X509_STORE_CTX_get0_store(ctx), 0));
  cert = _lookup_cert_by_subject(lpCertArray, X509_get_issuer_name(x));
  if (cert != NULL)
  {
    X509_STORE_CTX_check_issued_fn fnCheckIssued = X509_STORE_CTX_get_check_issued(ctx);

    if (fnCheckIssued(ctx, x, cert) != 0)
    {
      X509_up_ref(cert);
      *issuer = cert;
      return 1;
    }
  }
  return 0;
}

static STACK_OF(X509)* _X509_STORE_get1_certs(X509_STORE_CTX *ctx, X509_NAME *nm)
{
  MX::CSslCertificateArray *lpCertArray;
  X509 *cert;

  lpCertArray = (MX::CSslCertificateArray*)(::X509_STORE_get_ex_data(X509_STORE_CTX_get0_store(ctx), 0));
  cert = _lookup_cert_by_subject(lpCertArray, nm);
  if (cert != NULL)
  {
    STACK_OF(X509) *sk = sk_X509_new_null();
    if (sk != NULL)
    {
      if (sk_X509_push(sk, cert) > 0)
      {
        X509_up_ref(cert);
        return sk;
      }
      sk_X509_free(sk);
    }
  }
  return NULL;
}

static STACK_OF(X509_CRL)* _X509_STORE_get1_crls(X509_STORE_CTX *ctx, X509_NAME *nm)
{
  MX::CSslCertificateArray *lpCertArray;
  X509_CRL *cert;

  lpCertArray = (MX::CSslCertificateArray*)(::X509_STORE_get_ex_data(X509_STORE_CTX_get0_store(ctx), 0));
  cert = _lookup_crl_by_subject(lpCertArray, nm);
  if (cert != NULL)
  {
    STACK_OF(X509_CRL) *sk = sk_X509_CRL_new_null();
    if (sk != NULL)
    {
      if (sk_X509_CRL_push(sk, cert) > 0)
      {
        X509_CRL_up_ref(cert);
        return sk;
      }
      sk_X509_CRL_free(sk);
    }
  }
  return NULL;
}

static X509* _lookup_cert_by_subject(_In_ MX::CSslCertificateArray *lpCertArray, _In_ X509_NAME *name)
{
  if (lpCertArray != NULL && name != NULL)
  {
    SIZE_T i, nCount;

    nCount = lpCertArray->cCertsList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      MX::CSslCertificate *lpCert = lpCertArray->cCertsList.GetElementAt(i);
      X509 *lpX509 = lpCert->GetX509();

      if (lpX509 != NULL && X509_NAME_cmp(X509_get_subject_name(lpX509), name) == 0 &&
          X509_cmp_time(X509_get_notBefore(lpX509), NULL) < 0 &&
          X509_cmp_time(X509_get_notAfter(lpX509), NULL) > 0)
      {
        return lpX509;
      }
    }
  }
  return NULL;
}

static X509_CRL* _lookup_crl_by_subject(_In_ MX::CSslCertificateArray *lpCertArray, _In_ X509_NAME *name)
{
  if (lpCertArray != NULL && name != NULL)
  {
    SIZE_T i, nCount;

    nCount = lpCertArray->cCertCrlsList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      MX::CSslCertificateCrl *lpCertCrl = lpCertArray->cCertCrlsList.GetElementAt(i);
      X509_CRL *lpX509Crl = lpCertCrl->GetX509Crl();

      if (lpX509Crl != NULL && X509_NAME_cmp(X509_CRL_get_issuer(lpX509Crl), name) == 0)
        return lpX509Crl;
    }
  }
  return NULL;
}
