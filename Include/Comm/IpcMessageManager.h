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
#ifndef _MX_IPC_MESSAGE_MANAGER_H
#define _MX_IPC_MESSAGE_MANAGER_H

#include "IpcCommon.h"

//-----------------------------------------------------------

namespace MX {

class CIpcMessageManager : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CIpcMessageManager);
public:
  class CMessage;

  typedef Callback<VOID (__in CMessage *lpMsg)> OnMessageReceivedCallback;

  typedef Callback<VOID (__in CIpc *lpIpc, __in HANDLE hConn, __in DWORD dwId, __in CMessage *lpMsg,
                         __in LPVOID lpUserData)> OnMessageReplyCallback;

public:
  CIpcMessageManager(__in CIoCompletionPortThreadPool &cWorkerPool, __in CIpc *lpIpc, __in HANDLE hConn,
                     __in_opt DWORD dwMaxMessageSize=0x0FFFFFFFUL);
  ~CIpcMessageManager();

  VOID On(__in OnMessageReceivedCallback cMessageReceivedCallback);

  VOID Reset();

  DWORD GetNextId() const;

  HRESULT ProcessIncomingPacket();

  HRESULT SendHeader(__in DWORD dwMsgId, __in SIZE_T nMsgSize);

  HRESULT WaitForReply(__in DWORD dwId, __deref_out CMessage **lplpMessage);
  HRESULT WaitForReplyAsync(__in DWORD dwId, __in OnMessageReplyCallback cCallback, __in LPVOID lpUserData);

public:
  class CMessage : public virtual CBaseMemObj, public TLnkLstNode<CMessage>, public TRefCounted<CMessage>
  {
  private:
    CMessage(__in CIpc *lpIpc, __in HANDLE hConn);
  public:
    ~CMessage();

    DWORD GetId() const
      {
      return dwId;
      };

    operator LPBYTE() const
      {
      return GetData();
      };
    LPBYTE GetData() const
      {
      return lpData;
      };
    SIZE_T GetLength() const
      {
      return nDataLen;
      };

    HRESULT SendReplyHeader(__in SIZE_T nMsgSize);

    CIpc* GetIpc() const
      {
      return lpIpc;
      };
    HANDLE GetConn() const
      {
      return hConn;
      };

  private:
    friend class CIpcMessageManager;

    OVERLAPPED sOvr;
    DWORD dwId;
    LPBYTE lpData;
    DWORD nDataLen;
    CIpc *lpIpc;
    HANDLE hConn;
  };

private:
  typedef enum {
    StateRetrievingId,
    StateRetrievingSize,
    StateRetrievingMessage,
    StateError
  } eState;

  typedef struct {
    DWORD dwId;
    OnMessageReplyCallback cCallback;
    LPVOID lpUserData;
  } REPLYMSG_ITEM;

  VOID SyncWait(__in CIpc *lpIpc, __in HANDLE hConn, __in DWORD dwId, __in CMessage *lpMsg, __in LPVOID lpUserData);

  HRESULT OnMessageCompleted();

  VOID OnMessageReceived(__in CIoCompletionPortThreadPool *lpPool, __in DWORD dwBytes, __in OVERLAPPED *lpOvr,
                         __in HRESULT hRes);
  VOID OnFlushReceivedReplies(__in CIoCompletionPortThreadPool *lpPool, __in DWORD dwBytes, __in OVERLAPPED *lpOvr,
                              __in HRESULT hRes);
  static int ReplyMsgWaitCompareFunc(__in LPVOID lpContext, __in REPLYMSG_ITEM *lpElem1, __in REPLYMSG_ITEM *lpElem2);

  VOID FlushReceivedReplies();

private:
  typedef struct {
    CWindowsEvent cCompletedEvent;
    LPVOID volatile lpMsg;
  } SYNC_WAIT;

  CIoCompletionPortThreadPool &cWorkerPool;
  CIoCompletionPortThreadPool::OnPacketCallback cMessageReceivedCallbackWP, cFlushReceivedRepliesWP;
  CIpc *lpIpc;
  HANDLE hConn;
  DWORD dwMaxMessageSize;

  LONG volatile nRundownLock;
  LONG volatile nNextId;
  OnMessageReceivedCallback cMessageReceivedCallback;

  eState nState;
  TAutoRefCounted<CMessage> cCurrMessage;
  SIZE_T nCurrMsgSize;

  LONG volatile nIncomingQueuedMessagesCount;
  struct {
    LONG volatile nMutex;
    TArrayList4Structs<REPLYMSG_ITEM> cList;
  } sReplyMsgWait;
  struct {
    LONG volatile nMutex;
    TLnkLst<CMessage> cList;
  } sReceivedReplyMsg;
  struct {
    LONG volatile nMutex;
    LONG volatile nActive;
    OVERLAPPED sOvr;
  } sFlush;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_IPC_MESSAGE_MANAGER_H
