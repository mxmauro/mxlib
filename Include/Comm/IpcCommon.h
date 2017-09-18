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
#include "..\PropertyBag.h"
#include "..\TimedEventQueue.h"
#include "..\Debug.h"

#ifdef _DEBUG
  #define MX_IPC_TIMING
#else //_DEBUG
  //#define MX_IPC_TIMING
#endif //_DEBUG

#ifdef MX_IPC_TIMING
#include "..\HiResTimer.h"
#endif //MX_IPC_TIMING

//-----------------------------------------------------------

#define MX_IPC_PROPERTY_PacketSize                     "IPC_PacketSize"
#define MX_IPC_PROPERTY_PacketSize_DEFVAL              4096
#define MX_IPC_PROPERTY_ReadAhead                      "IPC_ReadAhead"
#define MX_IPC_PROPERTY_ReadAhead_DEFVAL               4
#define MX_IPC_PROPERTY_MaxFreePackets                 "IPC_MaxFreePackets"
#define MX_IPC_PROPERTY_MaxFreePackets_DEFVAL          128
#define MX_IPC_PROPERTY_MaxWriteTimeoutMs              "IPC_MaxWriteTimeoutMs"
#define MX_IPC_PROPERTY_MaxWriteTimeoutMs_DEFVAL       0
#define MX_IPC_PROPERTY_DoZeroReads                    "IPC_DoZeroReads"
#define MX_IPC_PROPERTY_DoZeroReads_DEFVAL             1
#define MX_IPC_PROPERTY_OutgoingPacketsLimit           "IPC_OutgoingPacketsLimit"
#define MX_IPC_PROPERTY_OutgoingPacketsLimit_DEFVAL    16

//-----------------------------------------------------------

namespace MX {

class CIpc : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CIpc);
protected:
  class CConnectionBase;
public:
  class CLayer;

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
      _InterlockedExchange(&nFlags, 0);
      return;
      };

  public:
    virtual ~CLayer()
      { };

  protected:
    friend class CIpc;

    HANDLE GetConn();

    HRESULT SendProcessedDataToNextLayer(__in LPCVOID lpMsg, __in SIZE_T nMsgSize);
    HRESULT SendMsgToNextLayer(__in LPCVOID lpMsg, __in SIZE_T nMsgSize);

    virtual HRESULT OnConnect() = 0;
    virtual HRESULT OnDisconnect() = 0;
    virtual HRESULT OnData(__in LPCVOID lpData, __in SIZE_T nDataSize) = 0;
    virtual HRESULT OnSendMsg(__in LPCVOID lpData, __in SIZE_T nDataSize) = 0;

  private:
    LPVOID lpConn;
    LONG volatile nFlags;
  };

  typedef TLnkLst<CLayer> CLayerList;

  //--------

  typedef Callback<VOID (__in CIpc *lpIpc, __in HRESULT hErrorCode)> OnEngineErrorCallback;

  typedef Callback<HRESULT (__in CIpc *lpIpc, __in HANDLE h, __in CUserData *lpUserData,
                            __inout CLayerList &cLayersList, __in HRESULT hErrorCode)> OnConnectCallback;
  typedef Callback<VOID (__in CIpc *lpIpc, __in HANDLE h, __in CUserData *lpUserData,
                         __in HRESULT hErrorCode)> OnDisconnectCallback;
  typedef Callback<HRESULT (__in CIpc *lpIpc, __in HANDLE h, __in CUserData *lpUserData)> OnDataReceivedCallback;
  typedef Callback<VOID (__in CIpc *lpIpc, __in HANDLE h, __in CUserData *lpUserData,
                         __in HRESULT hErrorCode)> OnDestroyCallback;

  typedef Callback<VOID (__in CIpc *lpIpc, __in HANDLE h, __in LPVOID lpCookie,
                         __in CUserData *lpUserData)> OnAfterWriteSignalCallback;

  typedef struct {
    CLayerList &cLayersList;
    OnConnectCallback &cConnectCallback;
    OnDisconnectCallback &cDisconnectCallback;
    OnDataReceivedCallback &cDataReceivedCallback;
    OnDestroyCallback &cDestroyCallback;
    DWORD &dwWriteTimeoutMs;
    TAutoRefCounted<CUserData> &cUserData;
  } CREATE_CALLBACK_DATA;

  typedef Callback<HRESULT (__in CIpc *lpIpc, __in HANDLE h, __inout CREATE_CALLBACK_DATA &sData)> OnCreateCallback;

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
    CAutoMultiSendLock(__in CMultiSendLock *lpLock);
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
  CIpc(__in CIoCompletionPortThreadPool &cDispatcherPool, __in CPropertyBag &cPropBag);

public:
  virtual ~CIpc();

  VOID On(__in OnEngineErrorCallback cEngineErrorCallback);

  HRESULT Initialize();
  VOID Finalize();

  virtual HRESULT SendMsg(__in HANDLE h, __in LPCVOID lpMsg, __in SIZE_T nMsgSize);
  virtual HRESULT SendStream(__in HANDLE h, __in CStream *lpStream);
  virtual HRESULT AfterWriteSignal(__in HANDLE h, __in OnAfterWriteSignalCallback cCallback, __in LPVOID lpCookie);

  CMultiSendLock* StartMultiSendBlock(__in HANDLE h);

  virtual HRESULT GetBufferedMessage(__in HANDLE h, __out LPVOID lpMsg, __inout SIZE_T *lpnMsgSize);
  virtual HRESULT ConsumeBufferedMessage(__in HANDLE h, __in SIZE_T nConsumedBytes);

  HRESULT Close(__in HANDLE h, __in HRESULT hErrorCode=S_OK);

  HRESULT IsConnected(__in HANDLE h);
  HRESULT IsClosed(__in HANDLE h, __out_opt HRESULT *lphErrorCode=NULL);

  //NOTE: On success, the connection will own the layer
  HRESULT AddLayer(__in HANDLE h, __in CLayer *lpLayer, __in_opt BOOL bFront=TRUE);

  CUserData* GetUserData(__in HANDLE h);

  eConnectionClass GetClass(__in HANDLE h);

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
      TypeMAX=TypeWrite
    } eType;

    explicit CPacket(__in DWORD dwPacketSize) : CBaseMemObj(), TLnkLstNode<CPacket>()
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

    static CPacket* FromOverlapped(__in LPOVERLAPPED lpOvr)
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

    VOID Reset(__in eType nType, __in CConnectionBase *_lpConn)
      {
      SetType(nType);
      nOrder = 0;
      lpConn = _lpConn;
      dwInUseSize = 0;
      MemSet(&sOvr, 0, sizeof(sOvr));
      MX_RELEASE(lpStream);
      cAfterWriteSignalCallback = NullCallback();
      lpUserData = NULL;
      return;
      };

    __inline VOID SetType(__in eType _nType)
      {
      _InterlockedExchange((LONG volatile *)&nType, (LONG)_nType);
      return;
      };

    __inline eType GetType() const
      {
      return (eType)__InterlockedRead((LONG volatile *)&nType);
      };

    __inline VOID SetOrder(__in ULONG _nOrder)
      {
      nOrder = _nOrder;
      return;
      };

    __inline ULONG GetOrder() const
      {
      return nOrder;
      };

    __inline VOID SetStream(__in CStream *_lpStream)
      {
      if (lpStream != NULL)
        lpStream->Release();
      lpStream = _lpStream;
      if (lpStream != NULL)
        lpStream->AddRef();
      nStreamReadOffset = 0ui64;
      return;
      };

    __inline HRESULT ReadStream(__out LPVOID lpDest, __in SIZE_T nBytes, __out SIZE_T &nReaded)
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

    __inline VOID SetBytesInUse(__in DWORD dwBytes)
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

    __inline VOID SetUserData(__in LPVOID _lpUserData)
      {
      lpUserData = _lpUserData;
      return;
      };

    __inline LPVOID GetUserData() const
      {
      return lpUserData;
      };

    __inline VOID SetAfterWriteSignalCallback(__in CIpc::OnAfterWriteSignalCallback cCallback)
      {
      cAfterWriteSignalCallback = cCallback;
      return;
      };

    __inline VOID InvokeAfterWriteSignalCallback(__in CIpc *lpIpc, __in CUserData *_lpUserData)
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

    VOID QueueLast(__in CPacket *lpPacket)
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

    VOID QueueSorted(__in CPacket *lpPacket)
      {
      CFastLock cLock(&nMutex);
      TLnkLst<CPacket>::Iterator it;
      CPacket *lpCurrPacket;

      for (lpCurrPacket=it.Begin(cList); lpCurrPacket!=NULL; lpCurrPacket=it.Next())
      {
        if (lpPacket->GetOrder() < lpCurrPacket->GetOrder())
          break;
      }
      if (lpCurrPacket != NULL)
        cList.PushBefore(lpPacket, lpCurrPacket);
      else
        cList.PushTail(lpPacket);
      nCount++;
      return;
      };

    CPacket* Dequeue(__in ULONG nOrder)
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

    VOID Remove(__in CPacket *lpPacket)
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

  class CConnectionBase : public virtual CBaseMemObj, public TLnkLstNode<CConnectionBase>
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CConnectionBase);
  protected:
    typedef TArrayList<CTimedEventQueue::CEvent*> __TEventArray;

  protected:
    CConnectionBase(__in CIpc *lpIpc, __in CIpc::eConnectionClass nClass);
  public:
    virtual ~CConnectionBase();

    virtual VOID ShutdownLink(__in BOOL bAbortive);

    VOID DoCancelEventsCallback(__in __TEventArray &cEventsList);

    HRESULT SendMsg(__in LPCVOID lpData, __in SIZE_T nDataSize);
    HRESULT SendStream(__in CStream *lpStream);
    HRESULT AfterWriteSignal(__in OnAfterWriteSignalCallback cCallback, __in LPVOID lpCookie);

    HRESULT WriteMsg(__in LPCVOID lpData, __in SIZE_T nDataSize);

    VOID Close(__in HRESULT hRes);

    BOOL IsConnected();
    BOOL IsGracefulShutdown();
    BOOL IsClosed();
    BOOL IsClosedOrGracefulShutdown();

    HRESULT HandleConnected();

    HRESULT DoRead(__in SIZE_T nPacketsCount, __in BOOL bZeroRead, __in_opt CPacket *lpReusePacket=NULL);

    CPacket* GetPacket(__in CPacket::eType nType);
    VOID FreePacket(__in CPacket *lpPacket);

    CIoCompletionPortThreadPool& GetDispatcherPool();
    CIoCompletionPortThreadPool::OnPacketCallback& GetDispatcherPoolPacketCallback();

    virtual HRESULT SendReadPacket(__in CPacket *lpPacket) = 0;
    virtual HRESULT SendWritePacket(__in CPacket *lpPacket) = 0;

    BOOL AddRefIfStillValid();
    VOID ReleaseMySelf();

    HRESULT SendDataToLayer(__in LPCVOID lpMsg, __in SIZE_T nMsgSize, __in CLayer *lpCurrLayer, __in BOOL bIsMsg);

    HRESULT MarkLastWriteActivity(__in BOOL bSet);
    VOID OnWriteTimeout(__in CTimedEventQueue::CEvent *lpEvent);

  protected:
    friend class CIpc;

    ULONG volatile nCheckTag1;
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
    LONG volatile nRefCount, nOutstandingReads, nOutstandingWrites, nOutstandingWritesBeingSent;
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
#ifdef MX_IPC_TIMING
    CHiResTimer cHiResTimer;
#endif //MX_IPC_TIMING
    ULONG volatile nCheckTag2;
  };

  //----

protected:
  VOID InternalFinalize();

  virtual HRESULT OnInternalInitialize() = 0;
  virtual VOID OnInternalFinalize() = 0;

  VOID CloseConnection(__in CConnectionBase *lpConn, __in HRESULT hErrorCode);
  VOID ReleaseAndRemoveConnectionIfClosed(__in CConnectionBase *lpConn);

  VOID FireOnEngineError(__in HRESULT hErrorCode);
  HRESULT FireOnCreate(__in CConnectionBase *lpConn);
  VOID FireOnDestroy(__in CConnectionBase *lpConn);
  HRESULT FireOnConnect(__in CConnectionBase *lpConn, __in HRESULT hErrorCode);
  VOID FireOnDisconnect(__in CConnectionBase *lpConn);
  HRESULT FireOnDataReceived(__in CConnectionBase *lpConn);

  CPacket* GetPacket(__in CConnectionBase *lpConn, __in CPacket::eType nType);
  VOID FreePacket(__in CPacket *lpPacket);

  CConnectionBase* CheckAndGetConnection(__in HANDLE h);
  CConnectionBase* IsValidConnection(__in HANDLE h);

  VOID OnDispatcherPacket(__in CIoCompletionPortThreadPool *lpPool, __in DWORD dwBytes, __in OVERLAPPED *lpOvr,
                          __in HRESULT hRes);

  virtual HRESULT OnPreprocessPacket(__in DWORD dwBytes, __in CPacket *lpPacket, __in HRESULT hRes);
  virtual HRESULT OnCustomPacket(__in DWORD dwBytes, __in CPacket *lpPacket, __in HRESULT hRes) = 0;
  virtual BOOL ZeroReadsSupported() const = 0;

  VOID OnReadWriteTimeout(__in CTimedEventQueue::CEvent *lpEvent);

protected:
  LONG volatile nInitShutdownMutex;
  LONG volatile nRundownProt;
  CIoCompletionPortThreadPool &cDispatcherPool;
  CIoCompletionPortThreadPool::OnPacketCallback cDispatcherPoolPacketCallback;
  CPropertyBag &cPropBag;
  DWORD dwPacketSize;
  DWORD dwReadAhead, dwMaxOutgoingPackets;
  BOOL bDoZeroReads;
  DWORD dwMaxFreePackets;
  DWORD dwMaxWriteTimeoutMs;
  CWindowsEvent cShuttingDownEv;
  OnEngineErrorCallback cEngineErrorCallback;
  struct {
    LONG volatile nSlimMutex;
    TLnkLst<CConnectionBase> cList;
  } sConnections;
  CPacketList cFreePacketsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_IPCCOMMON_H
