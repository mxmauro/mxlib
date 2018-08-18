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
#ifndef _MX_IPCCOMMON_H
#define _MX_IPCCOMMON_H

#include "..\Defines.h"
#include "..\WaitableObjects.h"
#include "..\LinkedList.h"
#include "..\RefCounted.h"
#include "..\AutoPtr.h"
#include "..\Threads.h"
#include "..\Strings\Strings.h"
#include "..\IOCompletionPort.h"
#include "..\CircularBuffer.h"
#include "..\Callbacks.h"
#include "..\Streams.h"
#include "..\TimedEventQueue.h"
#include "..\Debug.h"

#ifdef _DEBUG
  #define MX_IPC_DEBUG_OUTPUT
#else //_DEBUG
  //#define MX_IPC_DEBUG_OUTPUT
#endif //_DEBUG

#ifdef MX_IPC_DEBUG_OUTPUT
  #include "..\HiResTimer.h"

  #define MX_IPC_DEBUG_PRINT(level, output) if (MX::CIpc::nDebugLevel >= level) {  \
                                              MX::DebugPrint output;               \
                                            }
#else //MX_IPC_DEBUG_OUTPUT
  #define MX_IPC_DEBUG_PRINT(level, output)
#endif //MX_IPC_DEBUG_OUTPUT

//-----------------------------------------------------------

namespace MX {

class CIpc : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CIpc);
protected:
  class CConnectionBase;
public:
  class CLayer;

#ifdef MX_IPC_DEBUG_OUTPUT
  static LONG nDebugLevel;
#endif //MX_IPC_DEBUG_OUTPUT

  typedef enum {
    ConnectionClassError=-1,
    ConnectionClassUnknown=0,
    ConnectionClassServer, ConnectionClassClient,
    ConnectionClassListener,
    ConnectionClassMAX=ConnectionClassListener
  } eConnectionClass;

  //--------

  class CUserData : public virtual TRefCounted<CBaseMemObj>
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CUserData);
  public:
    CUserData() : TRefCounted<CBaseMemObj>()
      { };
    virtual ~CUserData()
      { };
  };

  //--------

  class CLayer : public virtual CBaseMemObj, public TLnkLstNode<CLayer>
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CLayer);
  protected:
    CLayer() : CBaseMemObj(), TLnkLstNode<CLayer>()
      {
      lpConn = NULL;
      return;
      };

  public:
    virtual ~CLayer()
      { };

  protected:
    friend class CIpc;

    HANDLE GetConn();

    HRESULT SendProcessedDataToNextLayer(_In_ LPCVOID lpMsg, _In_ SIZE_T nMsgSize);
    HRESULT SendMsgToNextLayer(_In_ LPCVOID lpMsg, _In_ SIZE_T nMsgSize);

    virtual HRESULT OnConnect() = 0;
    virtual HRESULT OnDisconnect() = 0;
    virtual HRESULT OnData(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize) = 0;
    virtual HRESULT OnSendMsg(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize) = 0;

  private:
    LPVOID lpConn;
  };

  typedef TLnkLst<CLayer> CLayerList;

  //--------

  typedef Callback<VOID (_In_ CIpc *lpIpc, _In_ HRESULT hErrorCode)> OnEngineErrorCallback;

  typedef Callback<HRESULT (_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CUserData *lpUserData,
                            _Inout_ CLayerList &cLayersList, _In_ HRESULT hErrorCode)> OnConnectCallback;
  typedef Callback<VOID (_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CUserData *lpUserData,
                         _In_ HRESULT hErrorCode)> OnDisconnectCallback;
  typedef Callback<HRESULT (_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CUserData *lpUserData)> OnDataReceivedCallback;
  typedef Callback<VOID (_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CUserData *lpUserData,
                         _In_ HRESULT hErrorCode)> OnDestroyCallback;

  typedef Callback<VOID (_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ LPVOID lpCookie,
                         _In_ CUserData *lpUserData)> OnAfterWriteSignalCallback;

  typedef struct {
    CLayerList &cLayersList;
    OnConnectCallback &cConnectCallback;
    OnDisconnectCallback &cDisconnectCallback;
    OnDataReceivedCallback &cDataReceivedCallback;
    OnDestroyCallback &cDestroyCallback;
    DWORD &dwWriteTimeoutMs;
    TAutoRefCounted<CUserData> &cUserData;
  } CREATE_CALLBACK_DATA;

  typedef Callback<HRESULT (_In_ CIpc *lpIpc, _In_ HANDLE h, _Inout_ CREATE_CALLBACK_DATA &sData)> OnCreateCallback;

  //--------

  class CMultiSendLock : public virtual CBaseMemObj
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CMultiSendLock);
  private:
    friend class CIpc;

    CMultiSendLock();
  public:
    ~CMultiSendLock();

  private:
    CConnectionBase *lpConn;
  };

  //--------

  class CAutoMultiSendLock : public virtual CBaseMemObj
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CAutoMultiSendLock);
  public:
    CAutoMultiSendLock(_In_ CMultiSendLock *lpLock);
    ~CAutoMultiSendLock();

    BOOL IsLocked() const
      {
      return (lpLock != NULL) ? TRUE : FALSE;
      };

  private:
    CMultiSendLock *lpLock;
  };

  //--------
protected:
  CIpc(_In_ CIoCompletionPortThreadPool &cDispatcherPool);

public:
  virtual ~CIpc();

  VOID SetOption_PacketSize(_In_ DWORD dwSize);
  VOID SetOption_ReadAheadCount(_In_ DWORD dwCount);
  VOID SetOption_MaxFreePacketsCount(_In_ DWORD dwCount);
  VOID SetOption_MaxWriteTimeoutMs(_In_ DWORD dwTimeoutMs);
  //NOTE: Disabling ZeroReads can lead to receive WSAENOBUFFS on low memory conditions.
  VOID SetOption_EnableZeroReads(_In_ BOOL bEnable);
  VOID SetOption_OutgoingPacketsLimitCount(_In_ DWORD dwCount);

  VOID On(_In_ OnEngineErrorCallback cEngineErrorCallback);

  HRESULT Initialize();
  VOID Finalize();

  virtual HRESULT SendMsg(_In_ HANDLE h, _In_reads_bytes_(nMsgSize) LPCVOID lpMsg, _In_ SIZE_T nMsgSize);
  virtual HRESULT SendStream(_In_ HANDLE h, _In_ CStream *lpStream);
  virtual HRESULT AfterWriteSignal(_In_ HANDLE h, _In_ OnAfterWriteSignalCallback cCallback, _In_opt_ LPVOID lpCookie);

  CMultiSendLock* StartMultiSendBlock(_In_ HANDLE h);

  virtual HRESULT GetBufferedMessage(_In_ HANDLE h, _Out_ LPVOID lpMsg, _Inout_ SIZE_T *lpnMsgSize);
  virtual HRESULT ConsumeBufferedMessage(_In_ HANDLE h, _In_ SIZE_T nConsumedBytes);

  HRESULT Close(_In_opt_ HANDLE h, _In_opt_ HRESULT hErrorCode=S_OK);

  HRESULT IsConnected(_In_ HANDLE h);
  HRESULT IsClosed(_In_ HANDLE h, _Out_opt_ HRESULT *lphErrorCode=NULL);

  HRESULT PauseInputProcessing(_In_ HANDLE h);
  HRESULT ResumeInputProcessing(_In_ HANDLE h);
  HRESULT PauseOutputProcessing(_In_ HANDLE h);
  HRESULT ResumeOutputProcessing(_In_ HANDLE h);

  //NOTE: On success, the connection will own the layer
  HRESULT AddLayer(_In_ HANDLE h, _In_ CLayer *lpLayer, _In_opt_ BOOL bFront=TRUE);

  CUserData* GetUserData(_In_ HANDLE h);

  eConnectionClass GetClass(_In_ HANDLE h);

  BOOL IsShuttingDown();

protected:
  class CPacket : public virtual CBaseMemObj, public TLnkLstNode<CPacket>
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CPacket);
  public:
    typedef enum {
      TypeDiscard=0,
      TypeInitialSetup,
      TypeZeroRead, TypeRead,
      TypeWriteRequest, TypeWrite,
      TypeResumeIoProcessing,
      TypeMAX=TypeResumeIoProcessing
    } eType;

    explicit CPacket(_In_ DWORD dwPacketSize) : CBaseMemObj(), TLnkLstNode<CPacket>()
      {
      MemSet(&sOvr, 0, sizeof(sOvr));
      nType = TypeDiscard;
      nOrder = 0;
      lpConn = NULL;
      lpStream = NULL;
      nStreamReadOffset = 0ui64;
      cAfterWriteSignalCallback = NullCallback();
      lpUserData = NULL;
      dwInUseSize = 0;
      lpBuffer = (LPBYTE)MX_MALLOC((SIZE_T)dwPacketSize);
      return;
      };

    ~CPacket()
      {
      MX_FREE(lpBuffer);
      MX_RELEASE(lpStream);
      return;
      };

    static CPacket* FromOverlapped(_In_ LPOVERLAPPED lpOvr)
      {
      return (CPacket*)((char*)lpOvr - (char*)&(((CPacket*)0)->sOvr));
      };

    __inline LPBYTE GetBuffer()
      {
      return lpBuffer;
      };

    __inline CConnectionBase* GetConn()
      {
      return lpConn;
      };

    VOID Reset(_In_ eType nType, _In_opt_ CConnectionBase *_lpConn)
      {
      SetType(nType);
      nOrder = 0;
      lpConn = _lpConn;
      dwInUseSize = 0;
      sOvr.Internal = 0;
      sOvr.InternalHigh = 0;
      sOvr.Pointer = NULL;
      MX_RELEASE(lpStream);
      cAfterWriteSignalCallback = NullCallback();
      lpUserData = NULL;
      return;
      };

    __inline VOID SetType(_In_ eType _nType)
      {
      _InterlockedExchange((LONG volatile *)&nType, (LONG)_nType);
      return;
      };

    __inline eType GetType() const
      {
      return (eType)__InterlockedRead((LONG volatile *)&nType);
      };

    __inline VOID SetOrder(_In_ ULONG _nOrder)
      {
      nOrder = _nOrder;
      return;
      };

    __inline ULONG GetOrder() const
      {
      return nOrder;
      };

    __inline VOID SetStream(_In_ CStream *_lpStream)
      {
      if (lpStream != NULL)
        lpStream->Release();
      lpStream = _lpStream;
      if (lpStream != NULL)
        lpStream->AddRef();
      nStreamReadOffset = 0ui64;
      return;
      };

    __inline HRESULT ReadStream(_Out_ LPVOID lpDest, _In_ SIZE_T nBytes, _Out_ SIZE_T &nReaded)
      {
      HRESULT hRes = lpStream->Read(lpDest, nBytes, nReaded, nStreamReadOffset);
      if (SUCCEEDED(hRes))
        nStreamReadOffset += (ULONGLONG)nReaded;
      return hRes;
      };

    __inline CStream* GetStream() const
      {
      return lpStream;
      };

    __inline BOOL HasStream() const
      {
      return (lpStream != NULL) ? TRUE : FALSE;
      };

    __inline VOID SetBytesInUse(_In_ DWORD dwBytes)
      {
      dwInUseSize = dwBytes;
      return;
      };

    __inline DWORD GetBytesInUse() const
      {
      return dwInUseSize;
      };

    OVERLAPPED* GetOverlapped()
      {
      return &sOvr;
      };

    __inline VOID SetUserData(_In_opt_ LPVOID _lpUserData)
      {
      lpUserData = _lpUserData;
      return;
      };

    __inline LPVOID GetUserData() const
      {
      return lpUserData;
      };

    __inline VOID SetAfterWriteSignalCallback(_In_ CIpc::OnAfterWriteSignalCallback cCallback)
      {
      cAfterWriteSignalCallback = cCallback;
      return;
      };

    __inline VOID InvokeAfterWriteSignalCallback(_In_ CIpc *lpIpc, _In_ CUserData *_lpUserData)
      {
      cAfterWriteSignalCallback(lpIpc, (HANDLE)lpConn, lpUserData, _lpUserData);
      return;
      };

    __inline BOOL HasAfterWriteSignalCallback() const
      {
      return (cAfterWriteSignalCallback != false) ? TRUE : FALSE;
      };

  private:
    __declspec(align(8)) OVERLAPPED sOvr;
    eType volatile nType;
    ULONG nOrder;
    CConnectionBase *lpConn;
    CStream *lpStream;
    ULONGLONG nStreamReadOffset;
    LPVOID lpUserData;
    union {
      DWORD dwInUseSize;
      DWORD dwCookie;
    };
    LPBYTE lpBuffer;
    CIpc::OnAfterWriteSignalCallback cAfterWriteSignalCallback;
  };

  //----

protected:
  class CPacketList : public virtual CBaseMemObj
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CPacketList);
  public:
    CPacketList() : CBaseMemObj()
      {
      _InterlockedExchange(&nMutex, 0);
      nCount = 0;
      return;
      };

    ~CPacketList()
      {
      DiscardAll();
      return;
      };

    VOID DiscardAll()
      {
      CPacket *lpPacket;

      while ((lpPacket=DequeueFirst()) != NULL)
        delete lpPacket;
      return;
      };

    VOID QueueFirst(_In_ CPacket *lpPacket)
      {
      if (lpPacket != NULL)
      {
        CFastLock cLock(&nMutex);

        cList.PushHead(lpPacket);
        nCount++;
      }
      return;
      };

    VOID QueueLast(_In_ CPacket *lpPacket)
      {
      if (lpPacket != NULL)
      {
        CFastLock cLock(&nMutex);

        cList.PushTail(lpPacket);
        nCount++;
      }
      return;
      };

    CPacket* DequeueFirst()
      {
      CFastLock cLock(&nMutex);

      CPacket *lpPacket = cList.PopHead();
      if (lpPacket != NULL)
        nCount--;
      return lpPacket;
      };

    VOID QueueSorted(_In_ CPacket *lpPacket)
      {
      CFastLock cLock(&nMutex);
      TLnkLst<CPacket>::IteratorRev it;
      CPacket *lpCurrPacket;

      for (lpCurrPacket=it.Begin(cList); lpCurrPacket!=NULL; lpCurrPacket=it.Next())
      {
        if (lpPacket->GetOrder() > lpCurrPacket->GetOrder())
          break;
      }
      if (lpCurrPacket != NULL)
        cList.PushAfter(lpPacket, lpCurrPacket);
      else
        cList.PushHead(lpPacket);
      nCount++;
      return;
      };

    CPacket* Dequeue(_In_ ULONG nOrder)
      {
      CFastLock cLock(&nMutex);
      CPacket *lpPacket;

      lpPacket = cList.GetHead();
      if (lpPacket != NULL && lpPacket->GetOrder() == nOrder)
      {
        lpPacket->RemoveNode();
        nCount--;
        return lpPacket;
      }
      return NULL;
      };

    VOID Remove(_In_ CPacket *lpPacket)
      {
      CFastLock cLock(&nMutex);

      MX_ASSERT(lpPacket->GetLinkedList() == &cList);
      lpPacket->RemoveNode();
      nCount--;
      return;
      };

    SIZE_T GetCount() const
      {
      CFastLock cLock(const_cast<LONG volatile *>(&nMutex));

      return nCount;
      };

  private:
    LONG volatile nMutex;
    TLnkLst<CPacket> cList;
    SIZE_T nCount;
  };

  //----

protected:
  friend class CLayer;

  class CConnectionBase : public virtual TRefCounted<CBaseMemObj>, public TRedBlackTreeNode<CConnectionBase,SIZE_T>
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CConnectionBase);
  protected:
    typedef TArrayList<CTimedEventQueue::CEvent*> __TEventArray;

  protected:
    CConnectionBase(_In_ CIpc *lpIpc, _In_ CIpc::eConnectionClass nClass);
  public:
    virtual ~CConnectionBase();

    SIZE_T GetNodeKey() const
      {
      return (SIZE_T)this;
      };

    virtual VOID ShutdownLink(_In_ BOOL bAbortive);

    VOID DoCancelEventsCallback(_In_ __TEventArray &cEventsList);

    HRESULT SendMsg(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize);
    HRESULT SendStream(_In_ CStream *lpStream);
    HRESULT AfterWriteSignal(_In_ OnAfterWriteSignalCallback cCallback, _In_opt_ LPVOID lpCookie);

    HRESULT SendResumeIoProcessingPacket(_In_ BOOL bInput);

    HRESULT WriteMsg(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize);

    VOID Close(_In_ HRESULT hRes);

    BOOL IsConnected();
    BOOL IsGracefulShutdown();
    BOOL IsClosed();
    BOOL IsClosedOrGracefulShutdown();

    HRESULT HandleConnected();

    HRESULT DoRead(_In_ SIZE_T nPacketsCount, _In_ BOOL bZeroRead, _In_opt_ CPacket *lpReusePacket=NULL);

    CPacket* GetPacket(_In_ CPacket::eType nType);
    VOID FreePacket(_In_ CPacket *lpPacket);

    CIoCompletionPortThreadPool& GetDispatcherPool();
    CIoCompletionPortThreadPool::OnPacketCallback& GetDispatcherPoolPacketCallback();

    virtual HRESULT SendReadPacket(_In_ CPacket *lpPacket) = 0;
    virtual HRESULT SendWritePacket(_In_ CPacket *lpPacket) = 0;

    HRESULT SendDataToLayer(_In_ LPCVOID lpMsg, _In_ SIZE_T nMsgSize, _In_ CLayer *lpCurrLayer, _In_ BOOL bIsMsg);

    HRESULT MarkLastWriteActivity(_In_ BOOL bSet);
    VOID OnWriteTimeout(_In_ CTimedEventQueue::CEvent *lpEvent);

  protected:
    friend class CIpc;

    LONG volatile nMutex;
    CIpc *lpIpc;
    CIpc::eConnectionClass nClass;
    LONG volatile hErrorCode;
    LONG volatile nFlags;
    CPacketList cReadedList;
    LONG volatile nNextReadOrder;
    LONG volatile nNextReadOrderToProcess;
    CPacketList cWritePendingList;
    LONG volatile nNextWriteOrder;
    LONG volatile nNextWriteOrderToProcess;
    LONG volatile nOutstandingReads, nOutstandingWrites, nOutstandingWritesBeingSent;
    DWORD dwWriteTimeoutMs;
    CPacketList cRwList;
    struct {
      LONG volatile nMutex;
      CCircularBuffer cBuffer;
    } sReceivedData;
    struct {
      LONG volatile nRwMutex;
      CLayerList cList;
    } sLayers;
    TAutoRefCounted<CUserData> cUserData;
    OnCreateCallback cCreateCallback;
    OnDestroyCallback cDestroyCallback;
    OnConnectCallback cConnectCallback;
    OnDisconnectCallback cDisconnectCallback;
    OnDataReceivedCallback cDataReceivedCallback;
    CSystemTimedEventQueue *lpTimedEventQueue;
    struct {
      LONG volatile nMutex;
      TPendingListHelperGeneric<CTimedEventQueue::CEvent*> cActiveList;
    } sWriteTimeout;
    CCriticalSection cOnDataReceivedCS;
#ifdef MX_IPC_DEBUG_OUTPUT
    CHiResTimer cHiResTimer;
#endif //MX_IPC_DEBUG_OUTPUT
  };

  //----

protected:
  VOID InternalFinalize();

  virtual HRESULT OnInternalInitialize() = 0;
  virtual VOID OnInternalFinalize() = 0;

  /*
  VOID CloseConnection(_In_ CConnectionBase *lpConn, _In_ HRESULT hErrorCode);
  VOID ReleaseAndRemoveConnectionIfClosed(_In_ CConnectionBase *lpConn);
  */

  VOID FireOnEngineError(_In_ HRESULT hErrorCode);
  HRESULT FireOnCreate(_In_ CConnectionBase *lpConn);
  VOID FireOnDestroy(_In_ CConnectionBase *lpConn);
  HRESULT FireOnConnect(_In_ CConnectionBase *lpConn, _In_ HRESULT hErrorCode);
  VOID FireOnDisconnect(_In_ CConnectionBase *lpConn);
  HRESULT FireOnDataReceived(_In_ CConnectionBase *lpConn);

  CPacket* GetPacket(_In_ CConnectionBase *lpConn, _In_ CPacket::eType nType);
  VOID FreePacket(_In_ CPacket *lpPacket);

  CConnectionBase* CheckAndGetConnection(_In_opt_ HANDLE h);

  VOID OnDispatcherPacket(_In_ CIoCompletionPortThreadPool *lpPool, _In_ DWORD dwBytes, _In_ OVERLAPPED *lpOvr,
                          _In_ HRESULT hRes);

  virtual HRESULT OnPreprocessPacket(_In_ DWORD dwBytes, _In_ CPacket *lpPacket, _In_ HRESULT hRes);
  virtual HRESULT OnCustomPacket(_In_ DWORD dwBytes, _In_ CPacket *lpPacket, _In_ HRESULT hRes) = 0;
  virtual BOOL ZeroReadsSupported() const = 0;

protected:
  LONG volatile nInitShutdownMutex;
  LONG volatile nRundownProt;
  CIoCompletionPortThreadPool &cDispatcherPool;
  CIoCompletionPortThreadPool::OnPacketCallback cDispatcherPoolPacketCallback;
  DWORD dwPacketSize;
  DWORD dwReadAhead, dwMaxOutgoingPackets;
  BOOL bDoZeroReads;
  DWORD dwMaxFreePackets;
  DWORD dwMaxWriteTimeoutMs;
  CWindowsEvent cShuttingDownEv;
  OnEngineErrorCallback cEngineErrorCallback;
  struct {
    LONG volatile nSlimMutex;
    TRedBlackTree<CConnectionBase,SIZE_T> cTree;
  } sConnections;
  CPacketList cFreePacketsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_IPCCOMMON_H
