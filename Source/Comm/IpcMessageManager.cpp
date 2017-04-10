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

#define _MESSAGE_ID_MASK                        0x7FFFFFFFUL
#define _MESSAGE_IS_REPLY                       0x80000000UL

#define _MESSAGE_END_XOR_MASK                   0xA53B7F91UL

#ifdef _DEBUG
  #define MX_DEBUG_OUTPUT
#else //_DEBUG
  //#define MX_DEBUG_OUTPUT
#endif //_DEBUG

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
                                       __in HANDLE _hConn, __in OnMessageReceivedCallback _cMessageReceivedCallback,
                                       __in_opt DWORD _dwMaxMessageSize, __in_opt DWORD _dwProtocolVersion) :
                                       CBaseMemObj(), cWorkerPool(_cWorkerPool)
{
  lpIpc = _lpIpc;
  hConn = _hConn;
  dwMaxMessageSize = (_dwMaxMessageSize >= 1) ? _dwMaxMessageSize : 1;
  if (_dwProtocolVersion < 1)
    dwProtocolVersion = 1;
  else if (_dwProtocolVersion > 2)
    dwProtocolVersion = 2;
  else 
    dwProtocolVersion = _dwProtocolVersion;
  cMessageReceivedCallbackWP = MX_BIND_MEMBER_CALLBACK(&CIpcMessageManager::OnMessageReceived, this);
  cFlushReceivedRepliesWP = MX_BIND_MEMBER_CALLBACK(&CIpcMessageManager::OnFlushReceivedReplies, this);
  //----
  _InterlockedExchange(&nShuttingDown, 0);
  _InterlockedExchange(&nNextId, 0);
  _InterlockedExchange(&nPendingCount, 0);
  nState = StateRetrievingId;
  nCurrMsgSize = 0;
  dwLastMessageId = 0;
  cMessageReceivedCallback = _cMessageReceivedCallback;
  _InterlockedExchange(&(sReplyMsgWait.nMutex), 0);
  _InterlockedExchange(&(sReceivedReplyMsg.nMutex), 0);
  MemSet(&sFlush, 0, sizeof(sFlush));
  return;
}

CIpcMessageManager::~CIpcMessageManager()
{
  CancelWaitingReplies();
  return;
}

VOID CIpcMessageManager::Shutdown()
{
  _InterlockedExchange(&nShuttingDown, 1);
  CancelWaitingReplies();
  return;
}

HRESULT CIpcMessageManager::SwitchToProtocol(__in DWORD _dwProtocolVersion)
{
  if (_dwProtocolVersion < 1 || _dwProtocolVersion > 2)
    return E_INVALIDARG;
  if (__InterlockedRead(&nShuttingDown) != 0)
    return MX_E_Cancelled;
  if (nState != StateRetrievingId)
    return E_FAIL;
  dwProtocolVersion = _dwProtocolVersion;
  return S_OK;
}

BOOL CIpcMessageManager::HasPending() const
{
  return (__InterlockedRead(const_cast<LONG volatile*>(&nPendingCount)) > 0) ? TRUE : FALSE;
}

DWORD CIpcMessageManager::GetNextId()
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
  BYTE aMsgBuf[4096], *s;
  SIZE_T nMsgSize, nToRead, nRemaining;
  HRESULT hRes;

  if (__InterlockedRead(&nShuttingDown) != 0)
    return MX_E_Cancelled;
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
        dwLastMessageId = cCurrMessage->dwId = *((DWORD MX_UNALIGNED*)s);
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
          MX_ASSERT(FALSE);
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
          nState = (dwProtocolVersion >= 2) ? StateWaitingMessageEnd : StateRetrievingId;
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
          nState = (dwProtocolVersion >= 2) ? StateWaitingMessageEnd : StateRetrievingId;
        }
      }
      else if (nState == StateWaitingMessageEnd)
      {
        if (nRemaining < sizeof(DWORD))
          break;
        if (*((DWORD MX_UNALIGNED*)s) != (dwLastMessageId ^ _MESSAGE_END_XOR_MASK))
        {
          MX_ASSERT(FALSE);
          hRes = MX_E_InvalidData;
          break;
        }
        s += sizeof(DWORD);
        nRemaining -= sizeof(DWORD);
        nState = StateRetrievingId;
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

  if (__InterlockedRead(&nShuttingDown) != 0)
    return MX_E_Cancelled;
  if (nMsgSize > dwMaxMessageSize)
  {
    MX_ASSERT(FALSE);
    return MX_E_InvalidData;
  }
  sHeader.dwMsgId = dwMsgId;
  sHeader.dwMsgSize = (DWORD)nMsgSize;
  return lpIpc->SendMsg(hConn, &sHeader, sizeof(sHeader));
}

HRESULT CIpcMessageManager::SendEndOfMessageMark(__in DWORD dwMsgId)
{
  if (__InterlockedRead(&nShuttingDown) != 0)
    return MX_E_Cancelled;
  if (dwProtocolVersion == 2)
    return S_OK; //protocol version 1 has no end of message mark
  dwMsgId = dwMsgId ^= _MESSAGE_END_XOR_MASK;
  return lpIpc->SendMsg(hConn, &dwMsgId, sizeof(dwMsgId));
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
  REPLYMSG_ITEM sNewItem;
  HRESULT hRes;

  if (dwId == 0 || (dwId & (~_MESSAGE_ID_MASK)) != 0)
    return E_INVALIDARG;
  if (!cCallback)
    return E_POINTER;
  if (__InterlockedRead(&nShuttingDown) != 0)
    return MX_E_Cancelled;
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
  if (SUCCEEDED(hRes) && sFlush.nActive == 0)
  {
    CFastLock cLock(&(sFlush.nMutex));

    if (sFlush.nActive == 0)
    {
      _InterlockedIncrement(&nPendingCount);
      hRes = cWorkerPool.Post(cFlushReceivedRepliesWP, 0, &(sFlush.sOvr));
      if (SUCCEEDED(hRes))
        sFlush.nActive = 1;
      else
        _InterlockedDecrement(&nPendingCount);
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
  _InterlockedIncrement(&nPendingCount);
  hRes = cWorkerPool.Post(cMessageReceivedCallbackWP, 0, &(cCurrMessage->sOvr));
  if (SUCCEEDED(hRes))
    cCurrMessage.Detach();
  else
    _InterlockedDecrement(&nPendingCount);
  return hRes;
}

VOID CIpcMessageManager::OnMessageReceived(__in CIoCompletionPortThreadPool *lpPool, __in DWORD dwBytes,
                                           __in OVERLAPPED *lpOvr, __in HRESULT hRes)
{
  TAutoRefCounted<CMessage> cMessage;
  BOOL bCallCallback = FALSE;
  REPLYMSG_ITEM sItem;

  cMessage.Attach((CMessage*)((char*)lpOvr - (char*)&(((CMessage*)0)->sOvr)));
  MemSet(&sItem, 0, sizeof(sItem));
  //process only if we are not resetting
  if (__InterlockedRead(&nShuttingDown) == 0)
  {
    //check if it is a reply for another message
    if ((cMessage->GetId() & _MESSAGE_IS_REPLY) == 0)
    {
      //call the callback
      if (cMessageReceivedCallback)
        cMessageReceivedCallback(cMessage.Get());
    }
    else
    {
      //add to the received replies list
      CFastLock cLock(&(sReceivedReplyMsg.nMutex));

      sReceivedReplyMsg.cList.PushTail(cMessage.Detach());
    }
  }
  //flush pending
  FlushReceivedReplies();
  //done
  _InterlockedDecrement(&nPendingCount);
  return;
}

VOID CIpcMessageManager::OnFlushReceivedReplies(__in CIoCompletionPortThreadPool *lpPool, __in DWORD dwBytes,
                                                __in OVERLAPPED *lpOvr, __in HRESULT hRes)
{
  _InterlockedExchange(&(sFlush.nActive), 0);
  //process only if we are not resetting
  FlushReceivedReplies();
  //done
  _InterlockedDecrement(&nPendingCount);
  return;
}

VOID CIpcMessageManager::FlushReceivedReplies()
{
  TLnkLst<CMessage>::Iterator it;
  TAutoRefCounted<CMessage> cMsg;
  REPLYMSG_ITEM sItem;
  SIZE_T nIndex;

  //process only if we are not resetting
  while (__InterlockedRead(&nShuttingDown) == 0)
  {
    MemSet(&sItem, 0, sizeof(sItem));

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
    //call callback
    if (sItem.cCallback)
      sItem.cCallback(lpIpc, hConn, sItem.dwId & _MESSAGE_ID_MASK, cMsg.Get(), sItem.lpUserData);
    //release message
    cMsg.Release();
  }
  //cancel pending if shutting down
  if (__InterlockedRead(&nShuttingDown) != 0)
    CancelWaitingReplies();
  //done
  return;
}

VOID CIpcMessageManager::CancelWaitingReplies()
{
  REPLYMSG_ITEM sItem;
  CMessage *lpMsg;

  //cancel reply waiters
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
  //done
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

CIpcMessageManager::CMessage::CMessage(__in CIpc *_lpIpc, __in HANDLE _hConn) : TRefCounted<CBaseMemObj>(),
                                                                                TLnkLstNode<CMessage>()
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
  {
    MX_ASSERT(FALSE);
    return MX_E_InvalidData;
  }
#endif //_M_X64
  sHeader.dwMsgId = dwId | _MESSAGE_IS_REPLY;
  sHeader.dwMsgSize = (DWORD)nMsgSize;
  return lpIpc->SendMsg(hConn, &sHeader, sizeof(sHeader));
}

HRESULT CIpcMessageManager::CMessage::SendReplyData(__in LPCVOID lpMsg, __in SIZE_T nMsgSize)
{
  return lpIpc->SendMsg(hConn, lpMsg, nMsgSize);
}

HRESULT CIpcMessageManager::CMessage::SendReplyEndOfMessageMark()
{
  DWORD dwMsgId;

  if (dwProtocolVersion <= 1)
    return S_OK; //protocol version 1 has no end of message mark
  dwMsgId = (dwId | _MESSAGE_IS_REPLY) ^ _MESSAGE_END_XOR_MASK;
  return lpIpc->SendMsg(hConn, &dwMsgId, sizeof(dwMsgId));
}

} //namespace MX
