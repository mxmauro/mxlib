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
#include "..\Debug.h"
#include "..\Loggable.h"
#include "..\HiResTimer.h"
#include "..\RedBlackTree.h"

#define MX_IPC_DEBUG_PRINT(level, output) if (MX::CIpc::nDebugLevel >= level) {  \
                                            MX::DebugPrint output;               \
                                          }

//-----------------------------------------------------------

namespace MX {

class CIpc : public virtual CBaseMemObj, public CLoggable
{
  MX_DISABLE_COPY_CONSTRUCTOR(CIpc);
protected:
  class CConnectionBase;
public:
  class CPacketBase;
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

    VOID IncrementOutgoingWrites();
    VOID DecrementOutgoingWrites();

    VOID FreePacket(_In_ CPacketBase *lpPacket);

    HRESULT ReadStream(_In_ CPacketBase *lpStreamPacket, _Out_ CPacketBase **lplpPacket);

    HRESULT SendReadDataToNextLayer(_In_ LPCVOID lpMsg, _In_ SIZE_T nMsgSize);
    HRESULT SendWriteDataToNextLayer(_In_ LPCVOID lpMsg, _In_ SIZE_T nMsgSize);
    //NOTE: The next layer will be responsible for freeing the packet on success or failure
    HRESULT SendAfterWritePacketToNextLayer(_In_ CPacketBase *lpPacket);

    virtual HRESULT OnConnect() = 0;
    virtual HRESULT OnDisconnect() = 0;
    virtual HRESULT OnReceivedData(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize) = 0;
    //NOTE: The OnSendPacket is responsible for freeing the packet on success or failure
    virtual HRESULT OnSendPacket(_In_ CPacketBase *lpPacket) = 0;
    virtual VOID OnShutdown() = 0;

  private:
    LPVOID lpConn;
  };

  typedef TLnkLst<CLayer> CLayerList;

  //--------

  typedef Callback<VOID (_In_ CIpc *lpIpc, _In_ HRESULT hrErrorCode)> OnEngineErrorCallback;

  typedef Callback<HRESULT (_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CUserData *lpUserData,
                            _In_ HRESULT hrErrorCode)> OnConnectCallback;
  typedef Callback<VOID (_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CUserData *lpUserData,
                         _In_ HRESULT hrErrorCode)> OnDisconnectCallback;
  typedef Callback<HRESULT (_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CUserData *lpUserData)> OnDataReceivedCallback;
  typedef Callback<VOID (_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CUserData *lpUserData,
                         _In_ HRESULT hrErrorCode)> OnDestroyCallback;

  typedef Callback<VOID (_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ LPVOID lpCookie,
                         _In_ CUserData *lpUserData)> OnAfterWriteSignalCallback;

  typedef struct {
    OnConnectCallback &cConnectCallback;
    OnDisconnectCallback &cDisconnectCallback;
    OnDataReceivedCallback &cDataReceivedCallback;
    OnDestroyCallback &cDestroyCallback;
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

  VOID SetOption_ReadAheadCount(_In_ DWORD dwCount);
  //NOTE: Disabling ZeroReads can lead to receive WSAENOBUFFS on low memory conditions.
  VOID SetOption_EnableZeroReads(_In_ BOOL bEnable);
  VOID SetOption_OutgoingBytesLimitCount(_In_ DWORD dwCount);

  VOID SetEngineErrorCallback(_In_ OnEngineErrorCallback cEngineErrorCallback);

  HRESULT Initialize();
  VOID Finalize();

  virtual HRESULT SendMsg(_In_ HANDLE h, _In_reads_bytes_(nMsgSize) LPCVOID lpMsg, _In_ SIZE_T nMsgSize);
  virtual HRESULT SendStream(_In_ HANDLE h, _In_ CStream *lpStream);
  virtual HRESULT AfterWriteSignal(_In_ HANDLE h, _In_ OnAfterWriteSignalCallback cCallback, _In_opt_ LPVOID lpCookie);

  CMultiSendLock* StartMultiSendBlock(_In_ HANDLE h);

  virtual HRESULT GetBufferedMessage(_In_ HANDLE h, _Out_ LPVOID lpMsg, _Inout_ SIZE_T *lpnMsgSize);
  virtual HRESULT ConsumeBufferedMessage(_In_ HANDLE h, _In_ SIZE_T nConsumedBytes);

  HRESULT Close(_In_opt_ HANDLE h, _In_opt_ HRESULT hrErrorCode = S_OK);

  HRESULT IsConnected(_In_ HANDLE h);
  HRESULT IsClosed(_In_ HANDLE h, _Out_opt_ HRESULT *lphErrorCode = NULL);

  HRESULT GetReadStats(_In_ HANDLE h, _Out_opt_ PULONGLONG lpullBytesTransferred, _Out_opt_ float *lpnThroughputKbps);
  HRESULT GetWriteStats(_In_ HANDLE h, _Out_opt_ PULONGLONG lpullBytesTransferred, _Out_opt_ float *lpnThroughputKbps);

  HRESULT GetErrorCode(_In_ HANDLE h);

  HRESULT PauseInputProcessing(_In_ HANDLE h);
  HRESULT ResumeInputProcessing(_In_ HANDLE h);
  HRESULT PauseOutputProcessing(_In_ HANDLE h);
  HRESULT ResumeOutputProcessing(_In_ HANDLE h);

  //NOTE: On success, the connection will own the layer
  HRESULT AddLayer(_In_ HANDLE h, _In_ CLayer *lpLayer, _In_opt_ BOOL bFront = TRUE);

  HRESULT SetConnectCallback(_In_ HANDLE h, _In_ OnConnectCallback cConnectCallback);
  HRESULT SetDisconnectCallback(_In_ HANDLE h, _In_ OnDisconnectCallback cDisconnectCallback);
  HRESULT SetDataReceivedCallback(_In_ HANDLE h, _In_ OnDataReceivedCallback cDataReceivedCallback);
  HRESULT SetDestroyCallback(_In_ HANDLE h, _In_ OnDestroyCallback cDestroyCallback);

  //NOTE: On success, the connection will own the user data
  HRESULT SetUserData(_In_ HANDLE h, _In_ CUserData *lpUserData);
  CUserData* GetUserData(_In_ HANDLE h);

  eConnectionClass GetClass(_In_ HANDLE h);

  BOOL IsShuttingDown();

public:
  class CPacketBase : public virtual CBaseMemObj, public TLnkLstNode<CPacketBase>
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CPacketBase);
  public:
    typedef enum {
      TypeDiscard = 0,
      TypeInitialSetup,
      TypeZeroRead, TypeRead,
      TypeWriteRequest, TypeWrite,
      TypeResumeIoProcessing,
      TypeMAX=TypeResumeIoProcessing
    } eType;

  public:
    CPacketBase() : CBaseMemObj(), TLnkLstNode<CPacketBase>()
      {
      MemSet(&sOvr, 0, sizeof(sOvr));
      nType = TypeDiscard;
      nOrder = 0;
      lpConn = NULL;
      lpStream = NULL;
      nStreamReadOffset = 0ui64;
      cAfterWriteSignalCallback = NullCallback();
      uUserData.lpPtr = NULL;
      dwInUseSize = 0;
      lpChainedPacket = NULL;
      return;
      };

    ~CPacketBase()
      {
      MX_RELEASE(lpStream);
      return;
      };

    static CPacketBase* FromOverlapped(_In_ LPOVERLAPPED lpOvr)
      {
      return (CPacketBase*)((char*)lpOvr - (char*)&(((CPacketBase*)0)->sOvr));
      };

    virtual LPBYTE GetBuffer() const = 0;
    virtual DWORD GetBufferSize() const = 0;
    virtual DWORD GetClassSize() const = 0;

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
      uUserData.lpPtr = NULL;
      lpChainedPacket = NULL;
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

    __inline VOID SetUserData(_In_opt_ LPVOID lpUserData)
      {
      uUserData.lpPtr = lpUserData;
      return;
      };

    __inline LPVOID GetUserData() const
      {
      return uUserData.lpPtr;
      };

    __inline VOID SetUserDataDW(_In_ DWORD dwValue)
      {
      uUserData.dwValue = dwValue;
      return;
      };

    __inline DWORD GetUserDataDW() const
      {
      return uUserData.dwValue;
      };

    __inline VOID SetAfterWriteSignalCallback(_In_ CIpc::OnAfterWriteSignalCallback cCallback)
      {
      cAfterWriteSignalCallback = cCallback;
      return;
      };

    __inline VOID InvokeAfterWriteSignalCallback(_In_ CIpc *lpIpc, _In_ CUserData *_lpUserData)
      {
      cAfterWriteSignalCallback(lpIpc, (HANDLE)lpConn, uUserData.lpPtr, _lpUserData);
      return;
      };

    __inline BOOL HasAfterWriteSignalCallback() const
      {
      return (cAfterWriteSignalCallback != false) ? TRUE : FALSE;
      };

    __inline VOID ChainPacket(_In_ CPacketBase *lpPacket)
      {
      lpChainedPacket = lpPacket;
      return;
      };

    __inline CPacketBase* GetChainedPacket() const
      {
      return lpChainedPacket;
      };

  private:
    __declspec(align(8)) OVERLAPPED sOvr;
    eType volatile nType;
    ULONG nOrder;
    CConnectionBase *lpConn;
    CStream *lpStream;
    ULONGLONG nStreamReadOffset;
    union {
      LPVOID lpPtr;
      DWORD dwValue;
    } uUserData;
    union {
      DWORD dwInUseSize;
      DWORD dwCookie;
    };
    CIpc::OnAfterWriteSignalCallback cAfterWriteSignalCallback;
    CPacketBase *lpChainedPacket;
  };

protected:
  template<SIZE_T Size>
  class TPacket : public CPacketBase
  {
  public:
    virtual LPBYTE GetBuffer() const
      {
      return const_cast<LPBYTE>(aBuffer);
      };

    virtual DWORD GetBufferSize() const
      {
      return (DWORD)sizeof(aBuffer);
      };

    virtual DWORD GetClassSize() const
      {
      return (DWORD)Size;
      };

  public:
    BYTE aBuffer[Size - sizeof(CPacketBase)];
  };

  //----

public:
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
      CPacketBase *lpPacket;

      while ((lpPacket = DequeueFirst()) != NULL)
        delete lpPacket;
      return;
      };

    VOID QueueFirst(_In_ CPacketBase *lpPacket)
      {
      if (lpPacket != NULL)
      {
        CFastLock cLock(&nMutex);

        cList.PushHead(lpPacket);
        nCount++;
      }
      return;
      };

    VOID QueueLast(_In_ CPacketBase *lpPacket)
      {
      if (lpPacket != NULL)
      {
        CFastLock cLock(&nMutex);

        cList.PushTail(lpPacket);
        nCount++;
      }
      return;
      };

    CPacketBase* DequeueFirst()
      {
      CFastLock cLock(&nMutex);
      CPacketBase *lpPacket;

      lpPacket = cList.PopHead();
      if (lpPacket != NULL)
        nCount--;
      return lpPacket;
      };

    VOID QueueSorted(_In_ CPacketBase *lpPacket)
      {
      CFastLock cLock(&nMutex);
      TLnkLst<CPacketBase>::IteratorRev it;
      CPacketBase *lpCurrPacket;

      for (lpCurrPacket = it.Begin(cList); lpCurrPacket != NULL; lpCurrPacket = it.Next())
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

    CPacketBase* Dequeue(_In_ ULONG nOrder)
      {
      CFastLock cLock(&nMutex);
      CPacketBase *lpPacket;

      lpPacket = cList.GetHead();
      if (lpPacket != NULL && lpPacket->GetOrder() == nOrder)
      {
        lpPacket->RemoveNode();
        nCount--;
        return lpPacket;
      }
      return NULL;
      };

    VOID Remove(_In_ CPacketBase *lpPacket)
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
    TLnkLst<CPacketBase> cList;
    SIZE_T nCount;
  };

  //----

protected:
  friend class CLayer;

  class CConnectionBase : public virtual TRefCounted<CBaseMemObj>, public TRedBlackTreeNode<CConnectionBase,SIZE_T>
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CConnectionBase);
  protected:
    CConnectionBase(_In_ CIpc *lpIpc, _In_ CIpc::eConnectionClass nClass);
  public:
    virtual ~CConnectionBase();

    SIZE_T GetNodeKey() const
      {
      return (SIZE_T)this;
      };

    virtual VOID ShutdownLink(_In_ BOOL bAbortive);

    HRESULT SendMsg(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize, _In_opt_ CLayer *lpToLayer);
    HRESULT SendStream(_In_ CStream *lpStream);
    HRESULT AfterWriteSignal(_In_ OnAfterWriteSignalCallback cCallback, _In_opt_ LPVOID lpCookie);
    HRESULT AfterWriteSignal(_In_ CPacketBase *lpPacket);

    HRESULT SendResumeIoProcessingPacket(_In_ BOOL bInput);

    VOID Close(_In_ HRESULT hRes);

    BOOL IsConnected();
    BOOL IsGracefulShutdown();
    BOOL IsClosed();
    BOOL IsClosedOrGracefulShutdown();

    HRESULT GetErrorCode();

    HRESULT HandleConnected();

    VOID IncrementOutgoingWrites();
    VOID DecrementOutgoingWrites();

    HRESULT DoZeroRead(_In_ SIZE_T nPacketsCount, _Inout_ CPacketList &cQueuedPacketsList);
    HRESULT DoRead(_In_ SIZE_T nPacketsCount, _In_opt_ CPacketBase *lpReusePacket,
                   _Inout_ CPacketList &cQueuedPacketsList);

    HRESULT SendPackets(_Inout_ CPacketBase **lplpFirstPacket, _Inout_ CPacketBase **lplpLastPacket,
                        _Inout_ SIZE_T *lpnChainLength, _In_ BOOL bFlushAll,
                        _Inout_ CPacketList &cQueuedPacketsList);

    CPacketBase* GetPacket(_In_ CPacketBase::eType nType, _In_ SIZE_T nDesiredSize, _In_ BOOL bRealSize);
    VOID FreePacket(_In_ CPacketBase *lpPacket);

    CIoCompletionPortThreadPool& GetDispatcherPool();
    CIoCompletionPortThreadPool::OnPacketCallback& GetDispatcherPoolPacketCallback();

    HRESULT ReadStream(_In_ CPacketBase *lpStreamPacket, _Out_ CPacketBase **lplpPacket);

    virtual HRESULT SendReadPacket(_In_ CPacketBase *lpPacket, _Out_ LPDWORD lpdwRead) = 0;
    virtual HRESULT SendWritePacket(_In_ CPacketBase *lpPacket, _Out_ LPDWORD lpdwWritten) = 0;
    virtual SIZE_T GetMultiWriteMaxCount() const = 0;

    HRESULT SendReadDataToNextLayer(_In_ LPCVOID lpMsg, _In_ SIZE_T nMsgSize, _In_ CLayer *lpCurrLayer);
    HRESULT SendWriteDataToNextLayer(_In_ LPCVOID lpMsg, _In_ SIZE_T nMsgSize, _In_ CLayer *lpCurrLayer);
    HRESULT SendAfterWritePacketToNextLayer(_In_ CPacketBase *lpPacket, _In_ CLayer *lpCurrLayer);

    VOID UpdateStats(_In_ BOOL bRead, _In_ DWORD dwBytesTransferred);
    VOID GetStats(_In_ BOOL bRead, _Out_ PULONGLONG lpullBytesTransferred, _Out_opt_ float *lpnThroughputKbps);

  protected:
    friend class CIpc;

    LONG volatile nReadMutex;
    CIpc *lpIpc;
    CIpc::eConnectionClass nClass;
    LONG volatile hrErrorCode;
    LONG volatile nFlags;
    CPacketList cReadedList;
    LONG volatile nNextReadOrder;
    LONG volatile nNextReadOrderToProcess;
    CPacketList cWritePendingList;
    LONG volatile nNextWriteOrder;
    LONG volatile nNextWriteOrderToProcess;
    LONG volatile nIncomingReads, nOutgoingWrites, nOutgoingBytes;
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
    struct tagStats {
      LONG nRwMutex;
      ULONGLONG ullBytesTransferred, ullPrevBytesTransferred;
      struct {
        ULARGE_INTEGER uliStartCounter, uliFrequency;
      } sWatchdogTimer;
      float nAvgRate;
      float nTransferRateHistory[4];
    } sReadStats, sWriteStats;
    CCriticalSection cOnDataReceivedCS;
    CHiResTimer cHiResTimer;
  };

  //----

protected:
  VOID InternalFinalize();

  virtual HRESULT OnInternalInitialize() = 0;
  virtual VOID OnInternalFinalize() = 0;

  VOID FireOnEngineError(_In_ HRESULT hrErrorCode);
  HRESULT FireOnCreate(_In_ CConnectionBase *lpConn);
  VOID FireOnDestroy(_In_ CConnectionBase *lpConn);
  HRESULT FireOnConnect(_In_ CConnectionBase *lpConn, _In_ HRESULT hrErrorCode);
  VOID FireOnDisconnect(_In_ CConnectionBase *lpConn);
  HRESULT FireOnDataReceived(_In_ CConnectionBase *lpConn);

  CPacketBase* GetPacket(_In_ CConnectionBase *lpConn, _In_ CPacketBase::eType nType, _In_ SIZE_T nDesiredSize,
                         _In_ BOOL bRealSize);
  VOID FreePacket(_In_ CPacketBase *lpPacket);

  CConnectionBase* CheckAndGetConnection(_In_opt_ HANDLE h);

  VOID OnDispatcherPacket(_In_ CIoCompletionPortThreadPool *lpPool, _In_ DWORD dwBytes, _In_ OVERLAPPED *lpOvr,
                          _In_ HRESULT hRes);

  virtual BOOL OnPreprocessPacket(_In_ DWORD dwBytes, _In_ CPacketBase *lpPacket, _In_ HRESULT hRes);
  virtual HRESULT OnCustomPacket(_In_ DWORD dwBytes, _In_ CPacketBase *lpPacket, _In_ HRESULT hRes) = 0;
  virtual BOOL ZeroReadsSupported() const = 0;

protected:
  LONG volatile nInitShutdownMutex;
  LONG volatile nRundownProt;
  CIoCompletionPortThreadPool &cDispatcherPool;
  CIoCompletionPortThreadPool::OnPacketCallback cDispatcherPoolPacketCallback;
  DWORD dwReadAhead, dwMaxOutgoingBytes;
  BOOL bDoZeroReads;
  CWindowsEvent cShuttingDownEv;
  OnEngineErrorCallback cEngineErrorCallback;
  struct {
    LONG volatile nRwMutex;
    TRedBlackTree<CConnectionBase,SIZE_T> cTree;
  } sConnections;
  CPacketList cFreePacketsList32768;
  CPacketList cFreePacketsList4096;
  CPacketList cFreePacketsList256;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_IPCCOMMON_H
