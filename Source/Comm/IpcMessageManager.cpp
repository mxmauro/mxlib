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

#define MAX_SYNCWAIT_ITEMS                                 8

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

typedef struct tagMULTIPLEBLOCKS_VAARGS {
  SIZE_T nBlocksCount;
  va_list args;
} MULTIPLEBLOCKS_VAARGS, *LPMULTIPLEBLOCKS_VAARGS;

typedef struct tagMULTIPLEBLOCKS_ARRAY {
  SIZE_T nBlocksCount;
  MX::CIpcMessageManager::LPMULTIBLOCK lpBlocks;
} MULTIPLEBLOCKS_ARRAY, *LPMULTIPLEBLOCKS_ARRAY;

//-----------------------------------------------------------

static VOID MultiBlockCallback_VaArgs(_In_ SIZE_T nIndex, _Out_ LPVOID *lplpMsg, _Out_ PSIZE_T lpnMsgSize,
                                      _In_ LPVOID lpContext);
static VOID MultiBlockCallback_Array(_In_ SIZE_T nIndex, _Out_ LPVOID *lplpMsg, _Out_ PSIZE_T lpnMsgSize,
                                     _In_ LPVOID lpContext);

//-----------------------------------------------------------

namespace MX {

CIpcMessageManager::CIpcMessageManager(_In_ CIoCompletionPortThreadPool &_cWorkerPool, _In_ CIpc *_lpIpc,
                                       _In_ HANDLE _hConn, _In_ OnMessageReceivedCallback _cMessageReceivedCallback,
                                       _In_opt_ DWORD _dwMaxMessageSize, _In_opt_ DWORD _dwProtocolVersion) :
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
  //----
  RundownProt_Initialize(&nRundownLock);
  _InterlockedExchange(&nNextId, 0);
  _InterlockedExchange(&nPendingCount, 0);
  nState = StateRetrievingId;
  nCurrMsgSize = 0;
  dwLastMessageId = 0;
  cMessageReceivedCallback = _cMessageReceivedCallback;
  _InterlockedExchange(&(sWaitingForReply.nMutex), 0);
  _InterlockedExchange(&(sReceivedMessages.nMutex), 0);
  _InterlockedExchange(&(sReceivedMsgReplies.nMutex), 0);
  _InterlockedExchange(&(sSyncWait.nMutex), 0);
  sSyncWait.nCount = 0;
  return;
}

CIpcMessageManager::~CIpcMessageManager()
{
  CSyncWait *lpSyncWait;

  Shutdown();

  while ((lpSyncWait = sSyncWait.cList.PopHead()) != NULL)
    lpSyncWait->Release();
  return;
}

VOID CIpcMessageManager::Shutdown()
{
  CMessage *lpMsg;

  RundownProt_WaitForRelease(&nRundownLock);

  while (HasPending() != FALSE)
    _YieldProcessor();
  CancelWaitingReplies();

  {
    CFastLock cLock(&(sReceivedMessages.nMutex));

    while ((lpMsg = sReceivedMessages.cList.PopHead()) != NULL)
      lpMsg->Release();
  }
  return;
}

HRESULT CIpcMessageManager::SwitchToProtocol(_In_ DWORD _dwProtocolVersion)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);

  if (_dwProtocolVersion < 1 || _dwProtocolVersion > 2)
    return E_INVALIDARG;
  if (cAutoRundownProt.IsAcquired() == FALSE)
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
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  BYTE aMsgBuf[4096], *s;
  SIZE_T nMsgSize, nToRead, nRemaining;
  HRESULT hRes;

  if (cAutoRundownProt.IsAcquired() == FALSE)
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
        cCurrMessage.Attach(MX_DEBUG_NEW CMessage(this));
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
          cCurrMessage.Release();
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
        MX::DebugPrint("ProcessIncomingPacket: Data=%lu bytes (%lu left)\n", (DWORD)nToRead,
                       (DWORD)(cCurrMessage->nDataLen - nCurrMsgSize));
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

HRESULT CIpcMessageManager::SendHeader(_In_ DWORD dwMsgId, _In_ SIZE_T nMsgSize)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  HEADER sHeader;

  if (cAutoRundownProt.IsAcquired() == FALSE)
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

HRESULT CIpcMessageManager::SendData(_In_ LPCVOID lpMsg, _In_ SIZE_T nMsgSize)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);

  if (cAutoRundownProt.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  return lpIpc->SendMsg(hConn, lpMsg, nMsgSize);
}

HRESULT CIpcMessageManager::SendEndOfMessageMark(_In_ DWORD dwMsgId)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);

  if (cAutoRundownProt.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  if (dwProtocolVersion <= 1)
    return S_OK; //protocol version 1 has no end of message mark
  dwMsgId = dwMsgId ^= _MESSAGE_END_XOR_MASK;
  return lpIpc->SendMsg(hConn, &dwMsgId, sizeof(dwMsgId));
}

HRESULT CIpcMessageManager::SendMultipleBlocks(_Out_ LPDWORD lpdwMsgId, _In_ SIZE_T nBlocksCount, ...)
{
  MULTIPLEBLOCKS_VAARGS sData;

  sData.nBlocksCount = nBlocksCount;
  va_start(sData.args, nBlocksCount);
  return SendMultipleBlocks(lpdwMsgId, MX_BIND_CALLBACK(&MultiBlockCallback_VaArgs), &sData);
}

HRESULT CIpcMessageManager::SendMultipleBlocks(_Out_ LPDWORD lpdwMsgId, _In_ SIZE_T nBlocksCount,
                                               _In_ LPMULTIBLOCK lpBlocks)
{
  MULTIPLEBLOCKS_ARRAY sData;

  sData.nBlocksCount = nBlocksCount;
  sData.lpBlocks = lpBlocks;
  return SendMultipleBlocks(lpdwMsgId, MX_BIND_CALLBACK(&MultiBlockCallback_Array), &sData);
}

HRESULT CIpcMessageManager::SendMultipleBlocks(_Out_ LPDWORD lpdwMsgId, _In_ OnMultiBlockCallback cMultiBlockCallback,
                                               _In_opt_ LPVOID lpContext)
{
  MX::CIpc::CAutoMultiSendLock cMultiSendLock(lpIpc->StartMultiSendBlock(hConn));
  LPVOID lpMsg;
  SIZE_T nIndex, nMsgSize, nTotalMsgSize;
  DWORD dwMsgId;
  HRESULT hRes;

  if (lpdwMsgId == NULL)
    return E_POINTER;
  *lpdwMsgId = 0;

  if (!cMultiBlockCallback)
    return E_POINTER;

  if (cMultiSendLock.IsLocked() == FALSE)
    return MX_E_Cancelled;
  nIndex = nTotalMsgSize = 0;
  do
  {
    lpMsg = NULL;
    nMsgSize = 0;
    cMultiBlockCallback(nIndex, &lpMsg, &nMsgSize, lpContext);
    if (lpMsg != NULL)
    {
      if (nMsgSize > dwMaxMessageSize ||
          nTotalMsgSize + nMsgSize < nTotalMsgSize ||
          nTotalMsgSize >(SIZE_T)dwMaxMessageSize)
      {
        MX_ASSERT(FALSE);
        return MX_E_InvalidData;
      }
      nTotalMsgSize += nMsgSize;
      nIndex++;
    }
  }
  while (lpMsg != NULL);
  if (nIndex == 0)
    return E_INVALIDARG;
  //get message id
  dwMsgId = GetNextId();
  //header
  hRes = SendHeader(dwMsgId, nTotalMsgSize);
  //body
  nIndex = 0;
  while (SUCCEEDED(hRes))
  {
    lpMsg = NULL;
    nMsgSize = 0;
    cMultiBlockCallback(nIndex++, &lpMsg, &nMsgSize, lpContext);
    if (lpMsg == NULL)
      break;
    if (nMsgSize > 0)
      hRes = lpIpc->SendMsg(hConn, lpMsg, nMsgSize);
  }
  //end of message
  if (SUCCEEDED(hRes))
    hRes = SendEndOfMessageMark(dwMsgId);
  //done
  if (SUCCEEDED(hRes))
    *lpdwMsgId = dwMsgId;
  return hRes;
}

HRESULT CIpcMessageManager::WaitForReply(_In_ DWORD dwId, _Deref_out_ CMessage **lplpMessage,
                                         _In_opt_ DWORD dwTimeoutMs)
{
  CSyncWait *lpSyncWait;
  HRESULT hRes;

  if (lplpMessage == NULL)
    return E_POINTER;
  *lplpMessage = NULL;

  lpSyncWait = GetSyncWaitObject();
  if (lpSyncWait == NULL)
    return E_OUTOFMEMORY;

  lpSyncWait->AddRef();
  hRes = WaitForReplyAsync(dwId, MX_BIND_MEMBER_CALLBACK(&CIpcMessageManager::SyncWait, this), lpSyncWait);
  if (SUCCEEDED(hRes))
  {
    lpSyncWait->Wait(dwTimeoutMs);
    *lplpMessage = lpSyncWait->DetachMessage();
    if ((*lplpMessage) == NULL)
      hRes = MX_E_Cancelled;
  }
  else
  {
    lpSyncWait->Release();
  }

  FreeSyncWaitObject(lpSyncWait, FAILED(hRes));
  //done
  return hRes;
}

HRESULT CIpcMessageManager::WaitForReplyAsync(_In_ DWORD dwId, _In_ OnMessageReplyCallback cCallback,
                                              _In_opt_ LPVOID lpUserData)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  REPLYMSG_ITEM sNewItem;
  HRESULT hRes;

  if (dwId == 0 || (dwId & (~_MESSAGE_ID_MASK)) != 0)
    return E_INVALIDARG;
  if (!cCallback)
    return E_POINTER;
  if (cAutoRundownProt.IsAcquired() == FALSE)
    return MX_E_Cancelled;
  //queue waiter
  hRes = S_OK;
  sNewItem.dwId = dwId | _MESSAGE_IS_REPLY;
  sNewItem.cCallback = cCallback;
  sNewItem.lpUserData = lpUserData;
  {
    CFastLock cLock(&(sWaitingForReply.nMutex));

    if (sWaitingForReply.cList.SortedInsert(&sNewItem, &CIpcMessageManager::ReplyMsgWaitCompareFunc, NULL) == FALSE)
      hRes = E_OUTOFMEMORY;
  }
  //add post for checking already received message replies
  if (SUCCEEDED(hRes))
  {
    FlushReceivedReplies();
  }
  //done
  return hRes;
}

VOID CIpcMessageManager::SyncWait(_In_ CIpc *lpIpc, _In_ HANDLE hConn, _In_ DWORD dwId, _In_ CMessage *lpMsg,
                                  _In_ LPVOID lpUserData)
{
  CSyncWait *lpSyncWait = (CSyncWait*)lpUserData;

  lpSyncWait->Complete(lpMsg);
  lpSyncWait->Release();
  return;
}

HRESULT CIpcMessageManager::OnMessageCompleted()
{
  HRESULT hRes;

#ifdef MX_DEBUG_OUTPUT
  MX::DebugPrint("ProcessIncomingPacket: OnMessageCompleted\n");
#endif //MX_DEBUG_OUTPUT
  _InterlockedIncrement(&nPendingCount);

  {
    CFastLock cLock(&(sReceivedMessages.nMutex));

    sReceivedMessages.cList.PushTail(cCurrMessage.Get());
    cCurrMessage->AddRef();
  }

  cCurrMessage->AddRef();
  hRes = cWorkerPool.Post(cMessageReceivedCallbackWP, 0, &(cCurrMessage->sOvr));
  if (FAILED(hRes))
  {
    cCurrMessage->Release();
    _InterlockedDecrement(&nPendingCount);
  }
  return hRes;
}

VOID CIpcMessageManager::OnMessageReceived(_In_ CIoCompletionPortThreadPool *lpPool, _In_ DWORD dwBytes,
                                           _In_ OVERLAPPED *lpOvr, _In_ HRESULT hRes)
{
  TAutoRefCounted<CMessage> cMessage;

  //process only if we are not shutting down
  do
  {
    cMessage.Release();
    {
      CFastLock cLock(&(sReceivedMessages.nMutex));

      cMessage.Attach(sReceivedMessages.cList.PopHead());
    }
    if (cMessage)
    {
      CAutoRundownProtection cAutoRundownProt(&nRundownLock);

      if (cAutoRundownProt.IsAcquired() == FALSE)
        break;

      //check if it is a reply for another message
      if ((cMessage->GetId() & _MESSAGE_IS_REPLY) == 0)
      {
        //call the callback
        if (cMessageReceivedCallback)
          cMessageReceivedCallback(cMessage.Get());
      }
      else
      {
        {
          //add to the received replies list
          CFastLock cLock(&(sReceivedMsgReplies.nMutex));

          sReceivedMsgReplies.cList.PushTail(cMessage.Detach());
        }
        //flush pending
        FlushReceivedReplies();
      }
    }
  }
  while (cMessage);

  //release reference to posted message
  cMessage.Attach((CMessage*)((char*)lpOvr - (char*)&(((CMessage*)0)->sOvr)));
  cMessage.Release();
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
  for (;;)
  {
    MemSet(&sItem, 0, sizeof(sItem));

    {
      CFastLock cLock1(&(sReceivedMsgReplies.nMutex));
      CFastLock cLock2(&(sWaitingForReply.nMutex));
      CMessage *lpMsg;

      for (lpMsg=it.Begin(sReceivedMsgReplies.cList); lpMsg!=NULL; lpMsg=it.Next())
      {
        sItem.dwId = lpMsg->GetId();
        nIndex = sWaitingForReply.cList.BinarySearch(&sItem, &CIpcMessageManager::ReplyMsgWaitCompareFunc, NULL);
        if (nIndex != (SIZE_T)-1)
        {
          sItem = sWaitingForReply.cList.GetElementAt(nIndex);
          sWaitingForReply.cList.RemoveElementAt(nIndex);
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
      CFastLock cLock(&(sWaitingForReply.nMutex));
      SIZE_T nCount;

      nCount = sWaitingForReply.cList.GetCount();
      if (nCount > 0)
      {
        sItem = sWaitingForReply.cList.GetElementAt(nCount-1);
        sWaitingForReply.cList.RemoveElementAt(nCount-1);
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
      CFastLock cLock(&(sReceivedMsgReplies.nMutex));

      lpMsg = sReceivedMsgReplies.cList.PopHead();
    }
    if (lpMsg != NULL)
      lpMsg->Release();
  }
  while (lpMsg != NULL);
  //done
  return;
}

CIpcMessageManager::CSyncWait* CIpcMessageManager::GetSyncWaitObject()
{
  CFastLock cLock(&(sSyncWait.nMutex));
  CSyncWait *lpSyncWait;

  lpSyncWait = sSyncWait.cList.PopHead();
  if (lpSyncWait != NULL)
  {
    (sSyncWait.nCount)--;
  }
  else
  {
    try
    {
      lpSyncWait = MX_DEBUG_NEW CSyncWait();
    }
    catch (LONG hr)
    {
      UNREFERENCED_PARAMETER(hr);
      lpSyncWait = NULL;
    }
  }
  return lpSyncWait;
}

VOID CIpcMessageManager::FreeSyncWaitObject(_In_ CSyncWait *lpSyncWait, _In_ BOOL bForceFree)
{
  CFastLock cLock(&(sSyncWait.nMutex));

  if (bForceFree == FALSE && sSyncWait.nCount < MAX_SYNCWAIT_ITEMS)
  {
    lpSyncWait->Reset();
    sSyncWait.cList.PushTail(lpSyncWait);
    (sSyncWait.nCount)++;
  }
  else
  {
    lpSyncWait->Release();
  }
  return;
}

int CIpcMessageManager::ReplyMsgWaitCompareFunc(_In_ LPVOID lpContext, _In_ REPLYMSG_ITEM *lpElem1,
                                                _In_ REPLYMSG_ITEM *lpElem2)
{
  if (lpElem1->dwId < lpElem2->dwId)
    return -1;
  if (lpElem1->dwId > lpElem2->dwId)
    return 1;
  return 0;
}

//-----------------------------------------------------------

CIpcMessageManager::CMessage::CMessage(_In_ CIpcMessageManager *_lpMgr) : TRefCounted<CBaseMemObj>(),
                                                                                TLnkLstNode<CMessage>()
{
  MemSet(&sOvr, 0, sizeof(sOvr));
  lpMgr = _lpMgr;
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

CIpc::CMultiSendLock* CIpcMessageManager::CMessage::StartMultiSendBlock()
{
  return lpMgr->lpIpc->StartMultiSendBlock(lpMgr->hConn);
}

HRESULT CIpcMessageManager::CMessage::SendReplyHeader(_In_ SIZE_T nMsgSize)
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
  return lpMgr->lpIpc->SendMsg(lpMgr->hConn, &sHeader, sizeof(sHeader));
}

HRESULT CIpcMessageManager::CMessage::SendReplyData(_In_reads_bytes_(nMsgSize) LPCVOID lpMsg, _In_ SIZE_T nMsgSize)
{
  return lpMgr->lpIpc->SendMsg(lpMgr->hConn, lpMsg, nMsgSize);
}

HRESULT CIpcMessageManager::CMessage::SendReplyEndOfMessageMark()
{
  DWORD dwMsgId;

  if (lpMgr->dwProtocolVersion <= 1)
    return S_OK; //protocol version 1 has no end of message mark
  dwMsgId = (dwId | _MESSAGE_IS_REPLY) ^ _MESSAGE_END_XOR_MASK;
  return lpMgr->lpIpc->SendMsg(lpMgr->hConn, &dwMsgId, sizeof(dwMsgId));
}

HRESULT CIpcMessageManager::CMessage::SendReplyMultipleBlocks(_In_ SIZE_T nBlocksCount, ...)
{
  MULTIPLEBLOCKS_VAARGS sData;

  sData.nBlocksCount = nBlocksCount;
  va_start(sData.args, nBlocksCount);
  return SendReplyMultipleBlocks(MX_BIND_CALLBACK(&MultiBlockCallback_VaArgs), &sData);
}

HRESULT CIpcMessageManager::CMessage::SendReplyMultipleBlocks(_In_ SIZE_T nBlocksCount, _In_ LPMULTIBLOCK lpBlocks)
{
  MULTIPLEBLOCKS_ARRAY sData;

  sData.nBlocksCount = nBlocksCount;
  sData.lpBlocks = lpBlocks;
  return SendReplyMultipleBlocks(MX_BIND_CALLBACK(&MultiBlockCallback_Array), &sData);
}

HRESULT CIpcMessageManager::CMessage::SendReplyMultipleBlocks(_In_ OnMultiBlockCallback cMultiBlockCallback,
                                                              _In_opt_ LPVOID lpContext)
{
  MX::CIpc::CAutoMultiSendLock cMultiSendLock(StartMultiSendBlock());
  LPVOID lpMsg;
  SIZE_T nIndex, nMsgSize, nTotalMsgSize;
  HRESULT hRes;

  if (cMultiSendLock.IsLocked() == FALSE)
    return MX_E_Cancelled;

  nIndex = nTotalMsgSize = 0;
  do
  {
    lpMsg = NULL;
    nMsgSize = 0;
    cMultiBlockCallback(nIndex, &lpMsg, &nMsgSize, lpContext);
    if (lpMsg != NULL)
    {
      if (nMsgSize > 0x7FFFFFFF ||
          nTotalMsgSize + nMsgSize < nTotalMsgSize ||
          nTotalMsgSize > 0x7FFFFFFF)
      {
        MX_ASSERT(FALSE);
        return MX_E_InvalidData;
      }
      nTotalMsgSize += nMsgSize;
      nIndex++;
    }
  }
  while (lpMsg != NULL);
  if (nIndex == 0)
    return E_INVALIDARG;
  //header
  hRes = SendReplyHeader(nTotalMsgSize);
  //body
  nIndex = 0;
  while (SUCCEEDED(hRes))
  {
    lpMsg = NULL;
    nMsgSize = 0;
    cMultiBlockCallback(nIndex++, &lpMsg, &nMsgSize, lpContext);
    if (lpMsg == NULL)
      break;
    if (nMsgSize > 0)
      hRes = lpMgr->lpIpc->SendMsg(lpMgr->hConn, lpMsg, nMsgSize);
  }
  //end of message
  if (SUCCEEDED(hRes))
    hRes = SendReplyEndOfMessageMark();
  //done
  return hRes;
}

//-----------------------------------------------------------

CIpcMessageManager::CSyncWait::CSyncWait() throw(...) : TRefCounted<CBaseMemObj>(), TLnkLstNode<CSyncWait>()
{
  HRESULT hRes;

  _InterlockedExchangePointer(&lpMsg, NULL);
  hRes = cCompletedEvent.Create(TRUE, FALSE);
  if (FAILED(hRes))
    throw (LONG)hRes;
  return;
}

CIpcMessageManager::CSyncWait::~CSyncWait()
{
  Reset();
  return;
}

VOID CIpcMessageManager::CSyncWait::Reset()
{
  CMessage *lpMessage;

  cCompletedEvent.Reset();
  lpMessage = DetachMessage();
  if (lpMessage != NULL)
    lpMessage->Release();
  return;
}

VOID CIpcMessageManager::CSyncWait::Complete(_In_opt_ CMessage *lpMessage)
{
  CMessage *lpOldMessage = DetachMessage();
  if (lpOldMessage != NULL)
    lpOldMessage->Release();

  _InterlockedExchangePointer(&lpMsg, (LPVOID)lpMessage);
  if (lpMessage != NULL)
    lpMessage->AddRef();
  cCompletedEvent.Set();
  return;
}

VOID CIpcMessageManager::CSyncWait::Wait(_In_ DWORD dwTimeoutMs)
{
  cCompletedEvent.Wait(dwTimeoutMs);
  return;
}

CIpcMessageManager::CMessage* CIpcMessageManager::CSyncWait::DetachMessage()
{
  return (CMessage*)_InterlockedExchangePointer(&lpMsg, NULL);
}

} //namespace MX

//-----------------------------------------------------------

static VOID MultiBlockCallback_VaArgs(_In_ SIZE_T nIndex, _Out_ LPVOID *lplpMsg, _Out_ PSIZE_T lpnMsgSize,
                                      _In_ LPVOID lpContext)
{
  LPMULTIPLEBLOCKS_VAARGS lpData = (LPMULTIPLEBLOCKS_VAARGS)lpContext;

  if (nIndex < lpData->nBlocksCount)
  {
    va_list args = lpData->args;

    while (nIndex > 0)
    {
#pragma warning(suppress: 6269)
      va_arg(args, LPVOID);
#pragma warning(suppress: 6269)
      va_arg(args, SIZE_T);
      nIndex--;
    }
    *lplpMsg = va_arg(args, LPVOID);
    *lpnMsgSize = va_arg(args, SIZE_T);
  }
  else
  {
    *lplpMsg = NULL;
    *lpnMsgSize = 0;
  }
  return;
}

static VOID MultiBlockCallback_Array(_In_ SIZE_T nIndex, _Out_ LPVOID *lplpMsg, _Out_ PSIZE_T lpnMsgSize,
                                     _In_ LPVOID lpContext)
{
  LPMULTIPLEBLOCKS_ARRAY lpData = (LPMULTIPLEBLOCKS_ARRAY)lpContext;

  if (nIndex < lpData->nBlocksCount)
  {
    *lplpMsg = lpData->lpBlocks[nIndex].lpMsg;
    *lpnMsgSize = lpData->lpBlocks[nIndex].nMsgSize;
  }
  else
  {
    *lplpMsg = NULL;
    *lpnMsgSize = 0;
  }
  return;
}
