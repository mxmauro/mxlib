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

  VOID Shutdown();

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
                             _In_opt_ LPVOID lpContext=NULL);

  HRESULT WaitForReply(_In_ DWORD dwId, _Deref_out_ CMessage **lplpMessage);
  HRESULT WaitForReplyAsync(_In_ DWORD dwId, _In_ OnMessageReplyCallback cCallback, _In_ LPVOID lpUserData);

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
    HRESULT SendReplyData(_In_ LPCVOID lpMsg, _In_ SIZE_T nMsgSize);
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

  VOID SyncWait(_In_ CIpc *lpIpc, _In_ HANDLE hConn, _In_ DWORD dwId, _In_ CMessage *lpMsg, _In_ LPVOID lpUserData);

  HRESULT OnMessageCompleted();

  VOID OnMessageReceived(_In_ CIoCompletionPortThreadPool *lpPool, _In_ DWORD dwBytes, _In_ OVERLAPPED *lpOvr,
                         _In_ HRESULT hRes);
  VOID OnFlushReceivedReplies(_In_ CIoCompletionPortThreadPool *lpPool, _In_ DWORD dwBytes, _In_ OVERLAPPED *lpOvr,
                              _In_ HRESULT hRes);
  static int ReplyMsgWaitCompareFunc(_In_ LPVOID lpContext, _In_ REPLYMSG_ITEM *lpElem1, _In_ REPLYMSG_ITEM *lpElem2);

  VOID FlushReceivedReplies();
  VOID CancelWaitingReplies();

private:
  typedef struct {
    CWindowsEvent cCompletedEvent;
    LPVOID volatile lpMsg;
  } SYNC_WAIT;

  CIoCompletionPortThreadPool &cWorkerPool;
  CIoCompletionPortThreadPool::OnPacketCallback cMessageReceivedCallbackWP, cFlushReceivedRepliesWP;
  CIpc *lpIpc;
  HANDLE hConn;
  DWORD dwMaxMessageSize, dwProtocolVersion;

  LONG volatile nShuttingDown;
  LONG volatile nNextId;
  LONG volatile nPendingCount;
  OnMessageReceivedCallback cMessageReceivedCallback;

  eState nState;
  TAutoRefCounted<CMessage> cCurrMessage;
  SIZE_T nCurrMsgSize;
  DWORD dwLastMessageId;

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
    LONG nActive;
    OVERLAPPED sOvr;
  } sFlush;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_IPC_MESSAGE_MANAGER_H
