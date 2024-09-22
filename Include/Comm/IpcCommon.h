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
#include "..\Timer.h"
#include "..\RedBlackTree.h"
#include "SslCertificates.h"
typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct bio_st BIO;

#define MX_IPC_DEBUG_PRINT(level, output) if (MX::CIpc::nDebugLevel >= level) {  \
                                            MX::DebugPrint output;               \
                                          }

//-----------------------------------------------------------

namespace MX {

class MX_NOVTABLE CIpc : public virtual CBaseMemObj, public CLoggable
{
protected:
  class CConnectionBase;
  class CPacketBase;

#ifdef MX_IPC_DEBUG_OUTPUT
  static LONG nDebugLevel;
#endif //MX_IPC_DEBUG_OUTPUT

public:
  enum class eConnectionClass
  {
    Error=-1,
    Unknown=0,
    Server, Client,
    Listener,
    MAX=Listener
  };

  enum class eSslOption
  {
    CheckCertificate = 1,
    AcceptSelfSigned = 2
  };

  //--------

public:
  class MX_NOVTABLE CUserData : public virtual TRefCounted<CBaseMemObj>
  {
  protected:
    CUserData() : TRefCounted<CBaseMemObj>()
      {
      return;
      };

  public:
    virtual ~CUserData()
      {
      return;
      };
  };

  //--------

public:
  typedef Callback<VOID (_In_ CIpc *lpIpc, _In_ HRESULT hrErrorCode)> OnEngineErrorCallback;

  typedef Callback<HRESULT (_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CUserData *lpUserData)> OnConnectCallback;
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

  struct CHANGE_CALLBACKS_DATA {
    CHANGE_CALLBACKS_DATA()
      {
      bConnectCallbackIsValid = bDisconnectCallbackIsValid = bDataReceivedCallbackIsValid = 0;
      bDestroyCallbackIsValid = bUserDataIsValid = 0;
      return;
      };

    ULONG bConnectCallbackIsValid : 1;
    ULONG bDisconnectCallbackIsValid : 1;
    ULONG bDataReceivedCallbackIsValid : 1;
    ULONG bDestroyCallbackIsValid : 1;
    ULONG bUserDataIsValid : 1;
    OnConnectCallback cConnectCallback;
    OnDisconnectCallback cDisconnectCallback;
    OnDataReceivedCallback cDataReceivedCallback;
    OnDestroyCallback cDestroyCallback;
    TAutoRefCounted<CUserData> cUserData;
  };

  //--------

public:
  class CMultiSendLock : public virtual CBaseMemObj, public CNonCopyableObj
  {
  private:
    friend class CIpc;

    CMultiSendLock(_In_ CConnectionBase *lpConn);
  public:
    ~CMultiSendLock();

  private:
    CConnectionBase *lpConn;
  };

  //--------

public:
  class CAutoMultiSendLock : public virtual CBaseMemObj, public CNonCopyableObj
  {
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

  HRESULT InitializeSSL(_In_ HANDLE h, _In_opt_ LPCSTR szHostNameA = NULL,
                        _In_opt_ CSslCertificateArray *lpCheckCertificates = NULL,
                        _In_opt_ CSslCertificate *lpSelfCert = NULL, _In_opt_ CEncryptionKey *lpPrivKey = NULL,
                        _In_opt_ CEncryptionKey *lpDhParam = NULL, _In_opt_ eSslOption nSslOptions = (eSslOption)0);

  HRESULT IsConnected(_In_ HANDLE h);
  HRESULT IsClosed(_In_ HANDLE h, _Out_opt_ HRESULT *lphErrorCode = NULL);

  HRESULT GetReadStats(_In_ HANDLE h, _Out_opt_ PULONGLONG lpullBytesTransferred, _Out_opt_ float *lpnThroughputKbps);
  HRESULT GetWriteStats(_In_ HANDLE h, _Out_opt_ PULONGLONG lpullBytesTransferred, _Out_opt_ float *lpnThroughputKbps);

  HRESULT GetErrorCode(_In_ HANDLE h);

  HRESULT PauseInputProcessing(_In_ HANDLE h);
  HRESULT ResumeInputProcessing(_In_ HANDLE h);
  HRESULT PauseOutputProcessing(_In_ HANDLE h);
  HRESULT ResumeOutputProcessing(_In_ HANDLE h);

  //NOTE: On success, the connection will own the user data if changed
  HRESULT SetCallbacks(_In_ HANDLE h, _In_ CHANGE_CALLBACKS_DATA &cCallbacks);

  CUserData* GetUserData(_In_ HANDLE h);

  eConnectionClass GetClass(_In_ HANDLE h);

  BOOL IsShuttingDown();

protected:
  class CPacketList;
  class CSslChannel;

  class MX_NOVTABLE CPacketBase : public virtual CBaseMemObj, public CNonCopyableObj
  {
  public:
    friend class CPacketList;

  public:
    enum class eType
    {
      Discard = 0,
      InitialSetup,
      ZeroRead, Read,
      WriteRequest, Write,
      ResumeIoProcessing,
      MAX=ResumeIoProcessing
    } ;

  public:
    CPacketBase() : CBaseMemObj(), CNonCopyableObj()
      {
      cAfterWriteSignalCallback = NullCallback();
      return;
      };

    ~CPacketBase()
      {
      MX_RELEASE(lpStream);
      return;
      };

    static CPacketBase* FromOverlapped(_In_ LPOVERLAPPED lpOvr)
      {
      return CONTAINING_RECORD(lpOvr, CPacketBase, sOvr);
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
      lpLinkedPacket = NULL;
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

    __inline VOID LinkPacket(_In_opt_ CPacketBase *lpPacket)
      {
      lpLinkedPacket = lpPacket;
      return;
      };

    __inline CPacketBase* GetLinkedPacket() const
      {
      return lpLinkedPacket;
      };

  private:
    __declspec(align(8)) OVERLAPPED sOvr{};
    eType volatile nType{ eType::Discard };
    CLnkLstNode cListNode;
    ULONG nOrder{ 0 };
    CConnectionBase *lpConn{ NULL };
    CStream *lpStream{ NULL };
    ULONGLONG nStreamReadOffset{ 0 };
    union {
      LPVOID lpPtr;
      DWORD dwValue;
    } uUserData{};
    union {
      DWORD dwInUseSize{ 0 };
      DWORD dwCookie;
    };
    CIpc::OnAfterWriteSignalCallback cAfterWriteSignalCallback;
    CPacketBase *lpLinkedPacket{ NULL };
  };

protected:
  template<SIZE_T Size>
  class TPacket : public CPacketBase
  {
  public:
    TPacket() : CPacketBase()
      {
      aBuffer[0] = 0;
      return;
      };

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

protected:
  class CPacketList : public virtual CBaseMemObj, public CNonCopyableObj
  {
  public:
    CPacketList() : CBaseMemObj(), CNonCopyableObj()
      {
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
        cList.PushHead(&(lpPacket->cListNode));
      }
      return;
      };

    VOID QueueLast(_In_ CPacketBase *lpPacket)
      {
      if (lpPacket != NULL)
      {
        cList.PushTail(&(lpPacket->cListNode));
      }
      return;
      };

    CPacketBase* DequeueFirst()
      {
      CLnkLstNode *lpNode;

      lpNode = cList.PopHead();
      if (lpNode == NULL)
        return NULL;
      return CONTAINING_RECORD(lpNode, CPacketBase, cListNode);
      };

    VOID QueueSorted(_In_ CPacketBase *lpPacket)
      {
      CLnkLst::IteratorRev it;
      CLnkLstNode *lpNode;

      for (lpNode = it.Begin(cList); lpNode != NULL; lpNode = it.Next())
      {
        CPacketBase *lpCurrPacket = CONTAINING_RECORD(lpNode, CPacketBase, cListNode);

        if (lpPacket->GetOrder() > lpCurrPacket->GetOrder())
          break;
      }
      if (lpNode != NULL)
        cList.PushAfter(&(lpPacket->cListNode), lpNode);
      else
        cList.PushHead(&(lpPacket->cListNode));
      return;
      };

    CPacketBase* Dequeue(_In_ ULONG nOrder)
      {
      CLnkLstNode *lpNode;

      lpNode = cList.GetHead();
      if (lpNode != NULL)
      {
        CPacketBase *lpPacket = CONTAINING_RECORD(lpNode, CPacketBase, cListNode);

        if (lpPacket->GetOrder() == nOrder)
        {
          lpNode->Remove();
          return lpPacket;
        }
      }
      return NULL;
      };

    VOID Remove(_In_ CPacketBase *lpPacket)
      {
      MX_ASSERT(lpPacket->cListNode.GetList() == &cList);
      lpPacket->cListNode.Remove();
      return;
      };

    SIZE_T GetCount() const
      {
      return cList.GetCount();
      };

#ifdef _DEBUG
    SIZE_T GetCountOfType(_In_ CPacketBase::eType nType) const
      {
      CPacketBase *lpPacket;
      CLnkLst::Iterator it;
      SIZE_T nCount = 0;

      for (CLnkLstNode *lpNode = it.Begin(cList); lpNode != NULL; lpNode = it.Next())
      {
        lpPacket = CONTAINING_RECORD(lpNode, CPacketBase, cListNode);
        if (lpPacket->nType == nType)
          nCount++;
      }
      return nCount;
      };
#endif //_DEBUG

  private:
    CLnkLst cList;
  };

protected:
  class MX_NOVTABLE CConnectionBase : public virtual TRefCounted<CBaseMemObj>
  {
  protected:
    CConnectionBase(_In_ CIpc *lpIpc, _In_ CIpc::eConnectionClass nClass);
  public:
    virtual ~CConnectionBase();

    virtual VOID ShutdownLink(_In_ BOOL bAbortive);

    HRESULT SendMsg(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize);
    HRESULT SendStream(_In_ CStream *lpStream);
    HRESULT AfterWriteSignal(_In_ OnAfterWriteSignalCallback cCallback, _In_opt_ LPVOID lpCookie);
    HRESULT AfterWriteSignal(_In_ CPacketBase *lpPacket);

    HRESULT SendResumeIoProcessingPacket(_In_ BOOL bInput);

    VOID Close(_In_ HRESULT hRes);

    BOOL IsConnected() const;
    BOOL IsGracefulShutdown() const;
    BOOL IsClosed() const;
    BOOL IsClosedOrGracefulShutdown() const;

    CIpc* GetIpc() const;

    HRESULT GetErrorCode() const;

  protected:
    HRESULT HandleConnected();
    HRESULT HandleIncomingPackets();
    HRESULT HandleOutgoingPackets();

    VOID IncrementOutgoingWrites();
    VOID DecrementOutgoingWrites();

    HRESULT DoZeroRead(_In_ SIZE_T nPacketsCount, _Inout_ CPacketList &cQueuedPacketsList);
    HRESULT DoRead(_In_ SIZE_T nPacketsCount, _In_opt_ CPacketBase *lpReusePacket,
                   _Inout_ CPacketList &cQueuedPacketsList);

    HRESULT DoWrite(_In_ CPacketBase *lpPacket);

    CPacketBase* GetPacket(_In_ CPacketBase::eType nType, _In_ SIZE_T nDesiredSize, _In_ BOOL bRealSize);
    VOID FreePacket(_In_ CPacketBase *lpPacket);

    CIoCompletionPortThreadPool& GetDispatcherPool();
    CIoCompletionPortThreadPool::OnPacketCallback& GetDispatcherPoolPacketCallback();

    HRESULT ReadStream(_In_ CPacketBase *lpStreamPacket, _Out_ CPacketBase **lplpPacket);

    virtual HRESULT SendReadPacket(_In_ CPacketBase *lpPacket, _Out_ LPDWORD lpdwRead) = 0;
    virtual HRESULT SendWritePacket(_In_ CPacketBase *lpPacket, _Out_ LPDWORD lpdwWritten) = 0;
    virtual SIZE_T GetMultiWriteMaxCount() const = 0;

    HRESULT SetupSsl(_In_opt_ LPCSTR szHostNameA, _In_opt_ CSslCertificateArray *lpCheckCertificates,
                     _In_opt_ CSslCertificate *lpSelfCert, _In_opt_ CEncryptionKey *lpPrivKey,
                     _In_opt_ CEncryptionKey *lpDhParam, _In_ eSslOption nSslOptions);

    HRESULT HandleSslStartup();
    VOID HandleSslShutdown();
    HRESULT HandleSslInput(_In_ CPacketBase *lpPacket);
    HRESULT HandleSslOutput(_In_ CPacketBase *lpPacket);
    HRESULT ProcessSsl(_In_ BOOL bCanWrite);
    HRESULT ProcessSslIncomingData();
    HRESULT ProcessSslEncryptedOutput();
    HRESULT HandleSslEndOfHandshake();

  protected:
    static int InsertCompareFunc(_In_ LPVOID lpContext, _In_ CRedBlackTreeNode *lpNode1,
                                 _In_ CRedBlackTreeNode *lpNode2)
      {
      CConnectionBase *lpConn1 = CONTAINING_RECORD(lpNode1, CConnectionBase, cTreeNode);
      CConnectionBase *lpConn2 = CONTAINING_RECORD(lpNode2, CConnectionBase, cTreeNode);

      if ((SIZE_T)lpConn1 < (SIZE_T)lpConn2)
        return -1;
      if ((SIZE_T)lpConn1 > (SIZE_T)lpConn2)
        return 1;
      return 0;
      };

    static int SearchCompareFunc(_In_ LPVOID lpContext, _In_ SIZE_T key, _In_ CRedBlackTreeNode *lpNode)
      {
      CConnectionBase *lpConn = CONTAINING_RECORD(lpNode, CConnectionBase, cTreeNode);

      if (key < (SIZE_T)lpConn)
        return -1;
      if (key > (SIZE_T)lpConn)
        return 1;
      return 0;
      };

  protected:
    friend class CIpc;
    friend class CMultiSendLock;

  protected:
    class CReadWriteStats : public CBaseMemObj
    {
    public:
      CReadWriteStats();

      VOID HandleConnected();
      VOID Update(_In_ DWORD dwBytesTransferred);

      VOID Get(_Out_opt_ PULONGLONG lpullBytesTransferred, _Out_opt_ float *lpnThroughputKbps = NULL,
               _Out_opt_ LPDWORD lpdwTimeMarkMs = NULL);

    private:
      RWLOCK sRwMutex{};
      ULONGLONG ullBytesTransferred{ 0 }, ullPrevBytesTransferred{ 0 };
      CTimer cTimer;
      float nAvgRate{ 0.0 }, nTransferRateHistory[4]{};
    };

  protected:
    LONG volatile nMutex{ MX_FASTLOCK_INIT };
    CRedBlackTreeNode cTreeNode;
    CIpc *lpIpc{ NULL };
    CIpc::eConnectionClass nClass;
    LONG volatile hrErrorCode{ S_OK };
    LONG volatile nFlags{ 0 };
    LONG volatile nNextReadOrder{ 0 };
    LONG volatile nNextReadOrderToProcess{ 1 };
    LONG volatile nNextWriteOrder{ 0 };
    LONG volatile nNextWriteOrderToProcess{ 1 };
    LONG volatile nIncomingReads{ 0 }, nOutgoingWrites{ 0 }, nOutgoingBytes{ 0 };
    struct {
      LONG volatile nMutex{ MX_FASTLOCK_INIT };
      CPacketList cList;
    } sReadPackets, sInUsePackets;
    struct {
      LONG volatile nMutex{ MX_FASTLOCK_INIT };
      CPacketList cList;
      BOOL bHasRequeuedPacket{ FALSE };
      CPacketBase *lpRequeuedPacket{ NULL };
    } sPendingWritePackets;
    struct {
      LONG volatile nMutex{ MX_FASTLOCK_INIT };
      CCircularBuffer cBuffer;
    } sReceivedData;
    TAutoRefCounted<CUserData> cUserData;
    struct {
      LONG volatile nMutex{ MX_FASTLOCK_INIT };
      SSL_CTX *lpCtx{ NULL };
      SSL *lpSession{ NULL };
      BIO *lpInBio{ NULL };
      BIO *lpOutBio{ NULL };
      TAutoRefCounted<MX::CSslCertificateArray> cCertArray;
    } sSsl;
    OnCreateCallback cCreateCallback;
    OnDestroyCallback cDestroyCallback;
    OnConnectCallback cConnectCallback;
    OnDisconnectCallback cDisconnectCallback;
    OnDataReceivedCallback cDataReceivedCallback;
    CReadWriteStats cReadStats, cWriteStats;
    CCriticalSection cOnDataReceivedCS;
    struct {
      LONG volatile nMutex{ MX_FASTLOCK_INIT };
      CPacketList cList;
    } sFreePackets32768, sFreePackets4096;
    CTimer cLogTimer;
  };

  //----

protected:
  VOID InternalFinalize();

  virtual HRESULT OnInternalInitialize() = 0;
  virtual VOID OnInternalFinalize() = 0;

  VOID FireOnEngineError(_In_ HRESULT hrErrorCode);
  HRESULT FireOnCreate(_In_ CConnectionBase *lpConn);
  VOID FireOnDestroy(_In_ CConnectionBase *lpConn);
  HRESULT FireOnConnect(_In_ CConnectionBase *lpConn);
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
  LONG volatile nInitShutdownMutex{ MX_FASTLOCK_INIT };
  LONG volatile nRundownProt{ MX_RUNDOWNPROT_INIT };
  CIoCompletionPortThreadPool &cDispatcherPool;
  CIoCompletionPortThreadPool::OnPacketCallback cDispatcherPoolPacketCallback;
  DWORD dwReadAhead{ 4 }, dwMaxOutgoingBytes{ 2 * 32768 };
  BOOL bDoZeroReads{ TRUE };
  CWindowsEvent cShuttingDownEv;
  OnEngineErrorCallback cEngineErrorCallback;
  struct {
    RWLOCK sRwMutex{};
    CRedBlackTree cTree;
    LONG volatile nCount{ 0 };
  } sConnections;
  struct {
    LONG volatile nMutex{ MX_FASTLOCK_INIT };
    CPacketList cList;
  } sFreePackets32768, sFreePackets4096;
};

inline CIpc::eSslOption operator|(CIpc::eSslOption lhs, CIpc::eSslOption rhs)
{
  return static_cast<CIpc::eSslOption>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

inline CIpc::eSslOption operator&(CIpc::eSslOption lhs, CIpc::eSslOption rhs)
{
  return static_cast<CIpc::eSslOption>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_IPCCOMMON_H
