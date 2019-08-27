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
#ifndef _MX_IPC_MESSAGE_MANAGER_H
#define _MX_IPC_MESSAGE_MANAGER_H

#include "IpcCommon.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CIpcMessageManager : public virtual TRefCounted<CBaseMemObj>, public CLoggable
{
  MX_DISABLE_COPY_CONSTRUCTOR(CIpcMessageManager);
public:
  typedef struct tagMULTIBLOCK {
    LPVOID lpMsg;
    SIZE_T nMsgSize;
  } MULTIBLOCK, *LPMULTIBLOCK;

  class CMessage;

  typedef Callback<VOID (_In_ CMessage *lpMsg)> OnMessageReceivedCallback;

  typedef Callback<VOID (_In_ CIpc *lpIpc, _In_ HANDLE hConn, _In_ DWORD dwId, _In_ CMessage *lpMsg,
                         _In_ LPVOID lpUserData)> OnMessageReplyCallback;

  typedef Callback<VOID (_In_ SIZE_T nIndex, _Out_ LPVOID *lplpMsg, _Out_ PSIZE_T lpnMsgSize,
                         _In_ LPVOID lpContext)> OnMultiBlockCallback;

public:
  CIpcMessageManager(_In_ CIoCompletionPortThreadPool &cWorkerPool, _In_ CIpc *lpIpc, _In_ HANDLE hConn,
                     _In_ OnMessageReceivedCallback cMessageReceivedCallback,
                     _In_opt_ DWORD dwMaxMessageSize=0x0FFFFFFFUL, _In_opt_ DWORD dwProtocolVersion=1);
  ~CIpcMessageManager();

  BOOL HasPending() const;

  HRESULT SwitchToProtocol(_In_ DWORD dwProtocolVersion);

  DWORD GetNextId();

  HRESULT ProcessIncomingPacket();

  HRESULT SendHeader(_In_ DWORD dwMsgId, _In_ SIZE_T nMsgSize);
  HRESULT SendData(_In_ LPCVOID lpMsg, _In_ SIZE_T nMsgSize);
  HRESULT SendEndOfMessageMark(_In_ DWORD dwMsgId);
  HRESULT SendMultipleBlocks(_Out_ LPDWORD lpdwMsgId, _In_ SIZE_T nBlocksCount, ...);
  HRESULT SendMultipleBlocks(_Out_ LPDWORD lpdwMsgId, _In_ SIZE_T nBlocksCount, _In_ LPMULTIBLOCK lpBlocks);
  HRESULT SendMultipleBlocks(_Out_ LPDWORD lpdwMsgId, _In_ OnMultiBlockCallback cMultiBlockCallback,
                             _In_opt_ LPVOID lpContext = NULL);

  HRESULT WaitForReply(_In_ DWORD dwId, _Deref_out_ CMessage **lplpMessage, _In_opt_ DWORD dwTimeoutMs = INFINITE);
  HRESULT WaitForReplyAsync(_In_ DWORD dwId, _In_ OnMessageReplyCallback cCallback, _In_opt_ LPVOID lpUserData);

  CIpc* GetIpc() const
    {
    return lpIpc;
    };
  HANDLE GetConn() const
    {
    return hConn;
    };

public:
  class CMessage : public virtual TRefCounted<CBaseMemObj>, public TLnkLstNode<CMessage>
  {
  private:
    CMessage(_In_ CIpcMessageManager *lpMgr);
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

    CIpc::CMultiSendLock* StartMultiSendBlock();

    HRESULT SendReplyHeader(_In_ SIZE_T nMsgSize);
    HRESULT SendReplyData(_In_reads_bytes_(nMsgSize) LPCVOID lpMsg, _In_ SIZE_T nMsgSize);
    HRESULT SendReplyEndOfMessageMark();
    HRESULT SendReplyMultipleBlocks(_In_ SIZE_T nBlocksCount, ...);
    HRESULT SendReplyMultipleBlocks(_In_ SIZE_T nBlocksCount, _In_ LPMULTIBLOCK lpBlocks);
    HRESULT SendReplyMultipleBlocks(_In_ OnMultiBlockCallback cMultiBlockCallback, _In_opt_ LPVOID lpContext=NULL);

    CIpcMessageManager* GetManager() const
      {
      return lpMgr;
      };
    CIpc* GetIpc() const
      {
      return lpMgr->lpIpc;
      };
    HANDLE GetConn() const
      {
      return lpMgr->hConn;
      };

  private:
    friend class CIpcMessageManager;

    CIpcMessageManager *lpMgr;

    OVERLAPPED sOvr;
    DWORD dwId;
    DWORD dwOrder;
    LPBYTE lpData;
    DWORD nDataLen;
  };

private:
  typedef enum {
    StateRetrievingId,
    StateRetrievingSize,
    StateRetrievingMessage,
    StateWaitingMessageEnd,
    StateError
  } eState;

  typedef struct {
    DWORD dwId;
    OnMessageReplyCallback cCallback;
    LPVOID lpUserData;
  } REPLYMSG_ITEM;

  class CSyncWait : public TRefCounted<CBaseMemObj>, public TLnkLstNode<CSyncWait>
  {
  public:
    CSyncWait() throw(...);
    ~CSyncWait();

    VOID Reset();

    VOID Complete(_In_opt_ CMessage *lpMessage);
    VOID Wait(_In_ DWORD dwTimeoutMs);

    CMessage* DetachMessage();

  private:
    CWindowsEvent cCompletedEvent;
    LPVOID volatile lpMsg;
  };

private:
  VOID SyncWait(_In_ CIpc *lpIpc, _In_ HANDLE hConn, _In_ DWORD dwId, _In_ CMessage *lpMsg, _In_ LPVOID lpUserData);

  HRESULT OnMessageCompleted();

  VOID OnMessageReceived(_In_ CIoCompletionPortThreadPool *lpPool, _In_ DWORD dwBytes, _In_ OVERLAPPED *lpOvr,
                         _In_ HRESULT hRes);
  static int ReplyMsgWaitCompareFunc(_In_ LPVOID lpContext, _In_ REPLYMSG_ITEM *lpElem1,
                                     _In_ REPLYMSG_ITEM *lpElem2);

  VOID FlushReceivedReplies();
  VOID CancelWaitingReplies();

  CSyncWait* GetSyncWaitObject();
  VOID FreeSyncWaitObject(_In_ CSyncWait *lpSyncWait, _In_ BOOL bForceFree);

private:
  CIoCompletionPortThreadPool &cWorkerPool;
  //NOTE: CIoCompletionPortThreadPool::Post needs a non-dynamic variable
  CIoCompletionPortThreadPool::OnPacketCallback cMessageReceivedCallbackWP;
  CIpc *lpIpc;
  HANDLE hConn;
  DWORD dwMaxMessageSize, dwProtocolVersion;

  LONG volatile nRundownLock;
  LONG volatile nNextId;
  LONG volatile nOutgoingMessageReceivedCallback;
  OnMessageReceivedCallback cMessageReceivedCallback;

  eState nState;
  TAutoRefCounted<CMessage> cCurrMessage;
  SIZE_T nCurrMsgSize;
  DWORD dwLastMessageId;

  struct {
    LONG volatile nMutex;
    TArrayList4Structs<REPLYMSG_ITEM> cList;
  } sWaitingForReply;
  struct {
    LONG volatile nMutex;
    TLnkLst<CMessage> cList;
    LONG volatile nNextOrderId;
    LONG volatile nNextOrderIdToProcess;
  } sReceivedMessages;
  struct {
    LONG volatile nMutex;
    TLnkLst<CMessage> cList;
  } sReceivedMsgReplies;
  struct {
    LONG volatile nMutex;
    SIZE_T nCount;
    TLnkLst<CSyncWait> cList;
  } sSyncWait;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_IPC_MESSAGE_MANAGER_H
