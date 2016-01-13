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
#include "..\..\Include\Comm\IpcMessageManager.h"

//-----------------------------------------------------------

#define _MESSAGE_ID_MASK     0x7FFFFFFFUL
#define _MESSAGE_IS_REPLY    0x80000000UL

//#define MX_DEBUG_OUTPUT

//-----------------------------------------------------------

#pragma pack(1)
typedef struct {
  DWORD dwMsgId;
  DWORD dwMsgSize;
} HEADER;
#pragma pack()

//-----------------------------------------------------------

namespace MX {

CIpcMessageManager::CIpcMessageManager(__in CIoCompletionPortThreadPool &_cWorkerPool, __in CIpc *_lpIpc,
                                       __in HANDLE _hConn, __in_opt DWORD _dwMaxMessageSize) : CBaseMemObj(),
                                       cWorkerPool(_cWorkerPool)
{
  lpIpc = _lpIpc;
  hConn = _hConn;
  dwMaxMessageSize = _dwMaxMessageSize;
  cMessageReceivedCallbackWP = MX_BIND_MEMBER_CALLBACK(&CIpcMessageManager::OnMessageReceived, this);
  cFlushReceivedRepliesWP = MX_BIND_MEMBER_CALLBACK(&CIpcMessageManager::OnFlushReceivedReplies, this);
  //----
  RundownProt_Initialize(&nRundownLock);
  _InterlockedExchange(&nNextId, 0);
  nState = StateRetrievingId;
  nCurrMsgSize = 0;
  _InterlockedExchange(&nIncomingQueuedMessagesCount, 0);
  cMessageReceivedCallback = NullCallback();
  _InterlockedExchange(&(sReplyMsgWait.nMutex), 0);
  _InterlockedExchange(&(sReceivedReplyMsg.nMutex), 0);
  MemSet(&sFlush, 0, sizeof(sFlush));
  return;
}

CIpcMessageManager::~CIpcMessageManager()
{
  Reset();
  return;
}

VOID CIpcMessageManager::On(__in OnMessageReceivedCallback _cMessageReceivedCallback)
{
  cMessageReceivedCallback = _cMessageReceivedCallback;
  return;
}

VOID CIpcMessageManager::Reset()
{
  REPLYMSG_ITEM sItem;
  CMessage *lpMsg;

  RundownProt_WaitForRelease(&nRundownLock);
  //wait for pending incoming
  while (__InterlockedRead(&nIncomingQueuedMessagesCount) > 0)
    _YieldProcessor();
  //cancel waiters
  do
  {
    MemSet(&sItem, 0, sizeof(sItem));
    {
      CFastLock cLock(&(sReplyMsgWait.nMutex));
      SIZE_T nCount;

      nCount = sReplyMsgWait.cList.GetCount();
      if (nCount > 0)
      {
        sItem = sReplyMsgWait.cList.GetElementAt(nCount-1);
        sReplyMsgWait.cList.RemoveElementAt(nCount-1);
      }
    }
    if (sItem.cCallback)
      sItem.cCallback(lpIpc, hConn, sItem.dwId & _MESSAGE_ID_MASK, NULL, sItem.lpUserData);
  }
  while (sItem.dwId != 0);
  //delete received messages
  lpMsg = NULL;
  do
  {
    {
      CFastLock cLock(&(sReceivedReplyMsg.nMutex));

      lpMsg = sReceivedReplyMsg.cList.PopHead();
    }
    if (lpMsg != NULL)
      lpMsg->Release();
  }
  while (lpMsg != NULL);
  //reset values
  _InterlockedExchange(&nNextId, 0);
  nState = StateRetrievingId;
  nCurrMsgSize = 0;
  cCurrMessage.Release();
  //done
  RundownProt_Initialize(&nRundownLock);
  return;
}

DWORD CIpcMessageManager::GetNextId() const
{
  DWORD dwId;

  do
  {
    dwId = (DWORD)_InterlockedIncrement(const_cast<LONG volatile*>(&nNextId));
  }
  while (dwId == 0);
  return dwId & _MESSAGE_ID_MASK;
}

HRESULT CIpcMessageManager::ProcessIncomingPacket()
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  BYTE aMsgBuf[4096], *s;
  SIZE_T nMsgSize, nToRead, nRemaining;
  HRESULT hRes;

  if (cAutoRundownProt.IsAcquired() == FALSE)
    return MX_E_NotReady;
  if (nState == StateError)
    return E_FAIL;
  do
  {
    nMsgSize = sizeof(aMsgBuf);
    hRes = lpIpc->GetBufferedMessage(hConn, aMsgBuf, &nMsgSize);
    if (FAILED(hRes) || nMsgSize == 0)
      break;
    s = aMsgBuf;
    nRemaining = nMsgSize;
    while (SUCCEEDED(hRes) && nRemaining > 0)
    {
      if (nState == StateRetrievingId)
      {
        if (nRemaining < sizeof(DWORD))
          break;
        //create a new message
        cCurrMessage.Attach(MX_DEBUG_NEW CMessage(lpIpc, hConn));
        if (!cCurrMessage)
        {
          hRes = E_OUTOFMEMORY;
          break;
        }
        cCurrMessage->dwId = *((DWORD MX_UNALIGNED*)s);
        s += sizeof(DWORD);
        nRemaining -= sizeof(DWORD);
        nState = StateRetrievingSize;
#ifdef MX_DEBUG_OUTPUT
        MX::DebugPrint("ProcessIncomingPacket: Id=%08X\n", cCurrMessage->dwId);
#endif //MX_DEBUG_OUTPUT
      }
      else if (nState == StateRetrievingSize)
      {
        if (nRemaining < sizeof(DWORD))
          break;
        MX_ASSERT(cCurrMessage != NULL);
        cCurrMessage->nDataLen = (SIZE_T)(*((DWORD MX_UNALIGNED*)s));
        s += sizeof(DWORD);
        nRemaining -= sizeof(DWORD);
#ifdef MX_DEBUG_OUTPUT
        MX::DebugPrint("ProcessIncomingPacket: DataLen=%lu\n", (DWORD)(cCurrMessage->nDataLen));
#endif //MX_DEBUG_OUTPUT
        if (cCurrMessage->nDataLen > dwMaxMessageSize)
        {
          hRes = MX_E_InvalidData;
          break;
        }
        if (cCurrMessage->nDataLen > 0)
        {
          cCurrMessage->lpData = (LPBYTE)MX_MALLOC(cCurrMessage->nDataLen);
          if (cCurrMessage->lpData == NULL)
          {
            hRes = E_OUTOFMEMORY;
            break;
          }
          nState = StateRetrievingMessage;
          nCurrMsgSize = 0;
        }
        else
        {
          hRes = OnMessageCompleted();
          if (FAILED(hRes))
            break;
          nState = StateRetrievingId;
        }
      }
      else if (nState == StateRetrievingMessage)
      {
        nToRead = cCurrMessage->nDataLen - nCurrMsgSize;
        if (nToRead > nRemaining)
          nToRead = nRemaining;
        MemCopy(cCurrMessage->lpData+nCurrMsgSize, s, nToRead);
        s += nToRead;
        nRemaining -= nToRead;
        nCurrMsgSize += nToRead;
#ifdef MX_DEBUG_OUTPUT
        MX::DebugPrint("ProcessIncomingPacket: Data=%lu bytes\n", (DWORD)nToRead);
#endif //MX_DEBUG_OUTPUT
        if (nCurrMsgSize >= cCurrMessage->nDataLen)
        {
          hRes = OnMessageCompleted();
          cCurrMessage.Release();
          if (FAILED(hRes))
            break;
          nState = StateRetrievingId;
        }
      }
      else
      {
        MX_ASSERT(FALSE);
        hRes = E_FAIL;
      }
    }
#ifdef MX_DEBUG_OUTPUT
    MX::DebugPrint("ProcessIncomingPacket: Consumed=%lu bytes\n", (DWORD)(nMsgSize-nRemaining));
#endif //MX_DEBUG_OUTPUT
    if (SUCCEEDED(hRes) && nMsgSize != nRemaining)
      hRes = lpIpc->ConsumeBufferedMessage(hConn, nMsgSize-nRemaining);
  }
  while (SUCCEEDED(hRes) && nMsgSize != nRemaining);
  //done
  if (FAILED(hRes))
    nState = StateError;
  return hRes;
}

HRESULT CIpcMessageManager::SendHeader(__in DWORD dwMsgId, __in SIZE_T nMsgSize)
{
  HEADER sHeader;

  if (nMsgSize > dwMaxMessageSize)
    return MX_E_InvalidData;
  sHeader.dwMsgId = dwMsgId;
  sHeader.dwMsgSize = (DWORD)nMsgSize;
  return lpIpc->SendMsg(hConn, &sHeader, sizeof(sHeader));
}

HRESULT CIpcMessageManager::WaitForReply(__in DWORD dwId, __deref_out CMessage **lplpMessage)
{
  SYNC_WAIT sSyncWait;
  HRESULT hRes;

  if (lplpMessage == NULL)
    return E_POINTER;
  *lplpMessage = NULL;
  sSyncWait.lpMsg = NULL;
  if (sSyncWait.cCompletedEvent.Create(TRUE, FALSE) == FALSE)
    return E_OUTOFMEMORY;
  hRes = WaitForReplyAsync(dwId, MX_BIND_MEMBER_CALLBACK(&CIpcMessageManager::SyncWait, this), &sSyncWait);
  if (SUCCEEDED(hRes))
  {
    sSyncWait.cCompletedEvent.Wait(INFINITE);
    *lplpMessage = (CMessage*)MX::__InterlockedReadPointer(&(sSyncWait.lpMsg));
    if ((*lplpMessage) == NULL)
      hRes = MX_E_Cancelled;
  }
  //done
  return hRes;
}

HRESULT CIpcMessageManager::WaitForReplyAsync(__in DWORD dwId, __in OnMessageReplyCallback cCallback,
                                              __in LPVOID lpUserData)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  REPLYMSG_ITEM sNewItem;
  HRESULT hRes;

  if (dwId == 0 || (dwId & (~_MESSAGE_ID_MASK)) != 0)
    return E_INVALIDARG;
  if (!cCallback)
    return E_POINTER;
  if (cAutoRundownProt.IsAcquired() == FALSE)
    return MX_E_NotReady;
  //queue waiter
  hRes = S_OK;
  sNewItem.dwId = dwId | _MESSAGE_IS_REPLY;
  sNewItem.cCallback = cCallback;
  sNewItem.lpUserData = lpUserData;
  {
    CFastLock cLock(&(sReplyMsgWait.nMutex));

    if (sReplyMsgWait.cList.SortedInsert(&sNewItem, &CIpcMessageManager::ReplyMsgWaitCompareFunc, NULL) == FALSE)
      hRes = E_OUTOFMEMORY;
  }
  //add post for checking already received message replies
  if (SUCCEEDED(hRes) && __InterlockedRead(&(sFlush.nActive)) == 0)
  {
    CFastLock cLock(&(sFlush.nMutex));

    if (__InterlockedRead(&(sFlush.nActive)) == 0)
    {
      _InterlockedIncrement(&nIncomingQueuedMessagesCount);
      hRes = cWorkerPool.Post(cFlushReceivedRepliesWP, 0, &(sFlush.sOvr));
      if (SUCCEEDED(hRes))
        _InterlockedExchange(&(sFlush.nActive), 1);
      else
        _InterlockedDecrement(&nIncomingQueuedMessagesCount);
    }
  }
  //done
  return hRes;
}

VOID CIpcMessageManager::SyncWait(__in CIpc *lpIpc, __in HANDLE hConn, __in DWORD dwId, __in CMessage *lpMsg,
                                  __in LPVOID lpUserData)
{
  SYNC_WAIT *lpSyncWait = (SYNC_WAIT*)lpUserData;

  _InterlockedExchangePointer(&(lpSyncWait->lpMsg), (LPVOID)lpMsg);
  if (lpMsg != NULL)
    lpMsg->AddRef();
  lpSyncWait->cCompletedEvent.Set();
  return;
}

HRESULT CIpcMessageManager::OnMessageCompleted()
{
  HRESULT hRes;

#ifdef MX_DEBUG_OUTPUT
  MX::DebugPrint("ProcessIncomingPacket: OnMessageCompleted\n");
#endif //MX_DEBUG_OUTPUT
  _InterlockedIncrement(&nIncomingQueuedMessagesCount);
  hRes = cWorkerPool.Post(cMessageReceivedCallbackWP, 0, &(cCurrMessage->sOvr));
  if (SUCCEEDED(hRes))
    cCurrMessage.Detach();
  else
    _InterlockedDecrement(&nIncomingQueuedMessagesCount);
  return hRes;
}

VOID CIpcMessageManager::OnMessageReceived(__in CIoCompletionPortThreadPool *lpPool, __in DWORD dwBytes,
                                           __in OVERLAPPED *lpOvr, __in HRESULT hRes)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  TAutoRefCounted<CMessage> cMessage;

  cMessage.Attach((CMessage*)((char*)lpOvr - (char*)&(((CMessage*)0)->sOvr)));
  //process only if we are not resetting
  if (cAutoRundownProt.IsAcquired() != FALSE)
  {
    //check if it is a reply for another message
    if ((cMessage->GetId() & _MESSAGE_IS_REPLY) == 0)
    {
      //standard message
      if (cMessageReceivedCallback)
        cMessageReceivedCallback(cMessage.Get());
    }
    else
    {
      //check if the message is in the wait list
      REPLYMSG_ITEM sItem;
      SIZE_T nIndex;

      MemSet(&sItem, 0, sizeof(sItem));
      {
        CFastLock cLock(&(sReplyMsgWait.nMutex));

        sItem.dwId = cMessage->GetId();
        nIndex = sReplyMsgWait.cList.BinarySearch(&sItem, &CIpcMessageManager::ReplyMsgWaitCompareFunc, NULL);
        if (nIndex != (SIZE_T)-1)
        {
          sItem = sReplyMsgWait.cList.GetElementAt(nIndex);
          sReplyMsgWait.cList.RemoveElementAt(nIndex);
        }
      }
      if (sItem.cCallback)
      {
        sItem.cCallback(lpIpc, hConn, sItem.dwId & _MESSAGE_ID_MASK, cMessage.Get(), sItem.lpUserData);
      }
      else
      {
        //add to the received replies list
        CFastLock cLock(&(sReceivedReplyMsg.nMutex));

        sReceivedReplyMsg.cList.PushTail(cMessage.Detach());
      }
      //flush pending
      FlushReceivedReplies();
    }
  }
  //done
  _InterlockedDecrement(&nIncomingQueuedMessagesCount);
  return;
}

VOID CIpcMessageManager::OnFlushReceivedReplies(__in CIoCompletionPortThreadPool *lpPool, __in DWORD dwBytes,
                                                __in OVERLAPPED *lpOvr, __in HRESULT hRes)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);

  _InterlockedExchange(&(sFlush.nActive), 0);
  //process only if we are not resetting
  if (cAutoRundownProt.IsAcquired() != FALSE)
    FlushReceivedReplies();
  //done
  _InterlockedDecrement(&nIncomingQueuedMessagesCount);
  return;
}

VOID CIpcMessageManager::FlushReceivedReplies()
{
  TLnkLst<CMessage>::Iterator it;
  TAutoRefCounted<CMessage> cMsg;
  REPLYMSG_ITEM sItem;
  SIZE_T nIndex;

  MemSet(&sItem, 0, sizeof(sItem));
  while (1)
  {
    cMsg.Release();
    {
      CFastLock cLock1(&(sReceivedReplyMsg.nMutex));
      CFastLock cLock2(&(sReplyMsgWait.nMutex));
      CMessage *lpMsg;

      for (lpMsg=it.Begin(sReceivedReplyMsg.cList); lpMsg!=NULL; lpMsg=it.Next())
      {
        sItem.dwId = lpMsg->GetId();
        nIndex = sReplyMsgWait.cList.BinarySearch(&sItem, &CIpcMessageManager::ReplyMsgWaitCompareFunc, NULL);
        if (nIndex != (SIZE_T)-1)
        {
          sItem = sReplyMsgWait.cList.GetElementAt(nIndex);
          sReplyMsgWait.cList.RemoveElementAt(nIndex);
          lpMsg->RemoveNode();
          cMsg.Attach(lpMsg);
          break;
        }
      }
    }
    if (!cMsg)
      break;
    sItem.cCallback(lpIpc, hConn, sItem.dwId & _MESSAGE_ID_MASK, cMsg.Get(), sItem.lpUserData);
  }
  return;
}

int CIpcMessageManager::ReplyMsgWaitCompareFunc(__in LPVOID lpContext, __in REPLYMSG_ITEM *lpElem1,
                                                  __in REPLYMSG_ITEM *lpElem2)
{
  if (lpElem1->dwId < lpElem2->dwId)
    return -1;
  if (lpElem1->dwId > lpElem2->dwId)
    return 1;
  return 0;
}

//-----------------------------------------------------------

CIpcMessageManager::CMessage::CMessage(__in CIpc *_lpIpc, __in HANDLE _hConn) : CBaseMemObj(), TLnkLstNode<CMessage>(),
                                                                                TRefCounted<CMessage>()
{
  MemSet(&sOvr, 0, sizeof(sOvr));
  dwId = 0;
  lpData = NULL;
  nDataLen = 0;
  lpIpc = _lpIpc;
  hConn = _hConn;
  return;
}

CIpcMessageManager::CMessage::~CMessage()
{
  MX_FREE(lpData);
  return;
}

CIpc::CMultiSendLock* CIpcMessageManager::CMessage::StartMultiSendBlock()
{
  return lpIpc->StartMultiSendBlock(hConn);
}

HRESULT CIpcMessageManager::CMessage::SendReplyHeader(__in SIZE_T nMsgSize)
{
  HEADER sHeader;

#if defined(_M_X64)
  if (nMsgSize > 0xFFFFFFFFUL)
    return MX_E_InvalidData;
#endif //_M_X64
  sHeader.dwMsgId = dwId | _MESSAGE_IS_REPLY;
  sHeader.dwMsgSize = (DWORD)nMsgSize;
  return lpIpc->SendMsg(hConn, &sHeader, sizeof(sHeader));
}

HRESULT CIpcMessageManager::CMessage::SendReplyData(__in LPCVOID lpMsg, __in SIZE_T nMsgSize)
{
  return lpIpc->SendMsg(hConn, lpMsg, nMsgSize);
}

} //namespace MX
