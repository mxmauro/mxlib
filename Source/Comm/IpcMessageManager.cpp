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

namespace MX {

CIpcMessageManager::CIpcMessageManager(__in CIoCompletionPortThreadPool &_cWorkerPool) : CBaseMemObj(),
                                                                                         cWorkerPool(_cWorkerPool)
{
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
      sItem.cCallback(sItem.dwId & 0x7FFFFFFFUL, NULL, sItem.lpUserData);
  }
  while (sItem.cCallback);
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
  return dwId & 0x7FFFFFFFUL;
}

HRESULT CIpcMessageManager::ProcessIncomingPacket(__in CIpc *lpIpc, __in HANDLE h)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  BYTE aMsgBuf[4096];
  SIZE_T nMsgSize, nToRead;
  BOOL bRestart;
  HRESULT hRes;

  if (cAutoRundownProt.IsAcquired() == FALSE)
    return MX_E_NotReady;
  do
  {
    bRestart = FALSE;
    nMsgSize = sizeof(aMsgBuf);
    hRes = lpIpc->GetBufferedMessage(h, aMsgBuf, &nMsgSize);
    if (SUCCEEDED(hRes))
    {
      switch (nState)
      {
        case StateRetrievingId:
          if (nMsgSize < sizeof(DWORD))
            break;
          //create a new message
          cCurrMessage.Attach(MX_DEBUG_NEW CMessage());
          if (!cCurrMessage)
            return E_OUTOFMEMORY;
          cCurrMessage->dwId = *((LPDWORD)aMsgBuf);
          hRes = lpIpc->ConsumeBufferedMessage(h, sizeof(DWORD));
          if (SUCCEEDED(hRes))
          {
            bRestart = TRUE;
            nState = StateRetrievingSize;
          }
          else
          {
            nState = StateError;
          }
          break;

        case StateRetrievingSize:
          if (nMsgSize < sizeof(DWORD))
            break;
          MX_ASSERT(cCurrMessage != NULL);
          cCurrMessage->nDataLen = (SIZE_T)(*((LPDWORD)aMsgBuf));
          if (cCurrMessage->nDataLen <= 0x0FFFFFFF)
          {
            cCurrMessage->lpData = (LPBYTE)MX_MALLOC(cCurrMessage->nDataLen);
            if (cCurrMessage->lpData != NULL)
              hRes = lpIpc->ConsumeBufferedMessage(h, 4);
            else
              hRes = E_OUTOFMEMORY;
          }
          else
          {
            hRes = MX_E_InvalidData;
          }
          if (SUCCEEDED(hRes))
          {
            if (cCurrMessage->nDataLen == 0)
            {
              hRes = OnMessageCompleted();
              if (SUCCEEDED(hRes))
              {
                nState = StateRetrievingId;
                bRestart = TRUE;
              }
              else
              {
                nState = StateError;
              }
            }
            else
            {
              nState = StateRetrievingMessage;
              nCurrMsgSize = 0;
            }
          }
          else
          {
            nState = StateError;
          }
          break;

        case StateRetrievingMessage:
          if (nMsgSize == 0)
            break;
          nToRead = cCurrMessage->nDataLen - nCurrMsgSize;
          if (nToRead > nMsgSize)
            nToRead = nMsgSize;
          MemCopy(cCurrMessage->lpData+nCurrMsgSize, aMsgBuf, nToRead);
          hRes = lpIpc->ConsumeBufferedMessage(h, nToRead);
          if (SUCCEEDED(hRes))
          {
            nCurrMsgSize += nToRead;
            if (nCurrMsgSize >= cCurrMessage->nDataLen)
            {
              hRes = OnMessageCompleted();
              if (SUCCEEDED(hRes))
              {
                nState = StateRetrievingId;
                cCurrMessage.Release();
                bRestart = TRUE;
              }
              else
              {
                nState = StateError;
              }
            }
            else
            {
              bRestart = TRUE;
            }
          }
          else
          {
            nState = StateError;
          }
          break;

        default:
          hRes = E_FAIL;
          break;
      }
    }
  }
  while (SUCCEEDED(hRes) && bRestart != FALSE);
  //done
  return hRes;
}

HRESULT CIpcMessageManager::SendHeader(__in CIpc *lpIpc, __in HANDLE h, __in DWORD dwMsgId, __in SIZE_T nMsgSize)
{
  #pragma pack(1)
  typedef struct {
    DWORD dwMsgId;
    DWORD dwMsgSize;
  } HEADER;
  #pragma pack()
  HEADER sHeader;

  if (nMsgSize > 0x0FFFFFFF)
    return MX_E_InvalidData;
  sHeader.dwMsgId = dwMsgId;
  sHeader.dwMsgSize = (DWORD)nMsgSize;
  return lpIpc->SendMsg(h, &sHeader, sizeof(sHeader));
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
    *lplpMessage = sSyncWait.lpMsg;
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

  if (dwId == 0 || dwId >= 0x80000000UL)
    return E_INVALIDARG;
  if (!cCallback)
    return E_POINTER;
  if (cAutoRundownProt.IsAcquired() == FALSE)
    return MX_E_NotReady;
  //queue waiter
  hRes = S_OK;
  sNewItem.dwId = dwId | 0x80000000UL;
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
      hRes = cWorkerPool.Post(MX_BIND_MEMBER_CALLBACK(&CIpcMessageManager::OnFlushReceivedReplies, this), 0,
                              &(sFlush.sOvr));
      if (SUCCEEDED(hRes))
        _InterlockedExchange(&(sFlush.nActive), 1);
      else
        _InterlockedDecrement(&nIncomingQueuedMessagesCount);
    }
  }
  //done
  return hRes;
}

VOID CIpcMessageManager::SyncWait(__in DWORD dwId, __in CMessage *lpMsg, __in LPVOID lpUserData)
{
  SYNC_WAIT *lpSyncWait = (SYNC_WAIT*)lpUserData;

  lpSyncWait->cCompletedEvent.Set();
  return;
}

HRESULT CIpcMessageManager::OnMessageCompleted()
{
  HRESULT hRes;

  _InterlockedIncrement(&nIncomingQueuedMessagesCount);
  hRes = cWorkerPool.Post(MX_BIND_MEMBER_CALLBACK(&CIpcMessageManager::OnMessageReceived, this), 0,
                          &(cCurrMessage->sOvr));
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
    if ((cMessage->GetId() & 0x80000000) == 0)
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
        sItem.cCallback(sItem.dwId & 0x7FFFFFFFUL, cMessage.Detach(), sItem.lpUserData);
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
  CMessage *lpMsg;
  REPLYMSG_ITEM sItem;
  SIZE_T nIndex;

  MemSet(&sItem, 0, sizeof(sItem));
  do
  {
    lpMsg = NULL;
    {
      CFastLock cLock1(&(sReceivedReplyMsg.nMutex));
      CFastLock cLock2(&(sReplyMsgWait.nMutex));

      for (lpMsg=it.Begin(sReceivedReplyMsg.cList); lpMsg!=NULL; lpMsg=it.Next())
      {
        sItem.dwId = lpMsg->GetId();
        nIndex = sReplyMsgWait.cList.BinarySearch(&sItem, &CIpcMessageManager::ReplyMsgWaitCompareFunc, NULL);
        if (nIndex != (SIZE_T)-1)
        {
          sItem = sReplyMsgWait.cList.GetElementAt(nIndex);
          sReplyMsgWait.cList.RemoveElementAt(nIndex);
          lpMsg->RemoveNode();
          break;
        }
      }
    }
    if (lpMsg != NULL)
    {
      sItem.cCallback(sItem.dwId & 0x7FFFFFFFUL, lpMsg, sItem.lpUserData);
    }
  }
  while (lpMsg != NULL);
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

CIpcMessageManager::CMessage::CMessage() : CBaseMemObj(), TLnkLstNode<CMessage>(), TRefCounted<CMessage>()
{
  MemSet(&sOvr, 0, sizeof(sOvr));
  dwId = 0;
  lpData = NULL;
  nDataLen = 0;
  return;
}

CIpcMessageManager::CMessage::~CMessage()
{
  MX_FREE(lpData);
  return;
}

} //namespace MX
