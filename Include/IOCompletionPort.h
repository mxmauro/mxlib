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
#ifndef _MX_IOCOMPLETIONPORT_H
#define _MX_IOCOMPLETIONPORT_H

#include "Defines.h"
#include "LinkedList.h"
#include "Threads.h"
#include "Callbacks.h"

//-----------------------------------------------------------

namespace MX {

class CIoCompletionPort : private virtual CBaseMemObj, public CNonCopyableObj
{
public:
  CIoCompletionPort();
  ~CIoCompletionPort();

  HRESULT Initialize(_In_ DWORD dwMaxConcurrency);
  VOID Finalize();

  HRESULT Attach(_In_ HANDLE h, _In_ ULONG_PTR nKey);

  HRESULT Post(_In_ ULONG_PTR nKey, _In_ DWORD dwBytes, _In_ OVERLAPPED *lpOvr=NULL);
  HRESULT Post(_In_ LPOVERLAPPED_ENTRY lpEntry);

  HRESULT Get(_Out_ LPOVERLAPPED_ENTRY lpEntries, _Inout_ ULONG &nEntriesCount, _In_ DWORD dwTimeoutMs=INFINITE);

  operator HANDLE() const
    {
    return hIOCP;
    };

  bool operator!() const
    {
    return (hIOCP == NULL) ? true : false;
    };

  operator bool() const
    {
    return (hIOCP != NULL) ? true : false;
    };

private:
  HANDLE hIOCP;
};

//-----------------------------------------------------------

class CIoCompletionPortThreadPool : public virtual CBaseMemObj, public CNonCopyableObj
{
public:
  typedef Callback<VOID (_In_ CIoCompletionPortThreadPool *lpPool, _Inout_ LPVOID &lpUserData)> OnThreadStartCallback;
  typedef Callback<VOID (_In_ CIoCompletionPortThreadPool *lpPool, _In_ LPVOID lpUserData)> OnThreadEndCallback;
  typedef Callback<VOID (_In_ CIoCompletionPortThreadPool *lpPool, _In_ HRESULT hRes)> OnThreadStartErrorCallback;
  typedef Callback<VOID (_In_ CIoCompletionPortThreadPool *lpPool, _In_ DWORD dwBytes, _In_ OVERLAPPED *lpOvr,
                         _In_ HRESULT hRes)> OnPacketCallback;

public:
  CIoCompletionPortThreadPool();
  ~CIoCompletionPortThreadPool();

  VOID SetOption_MinThreadsCount(_In_opt_ DWORD dwCount = 0);
  VOID SetOption_MaxThreadsCount(_In_opt_ DWORD dwCount = 0);
  VOID SetOption_WorkerThreadIdleTime(_In_opt_ DWORD dwTimeoutMs = 2000);
  VOID SetOption_ShutdownThreadThreshold(_In_opt_ DWORD dwThreshold = 2);
  VOID SetOption_ThreadStackSize(_In_opt_ DWORD dwStackSize = 0);

  VOID SetThreadStartCallback(_In_opt_ OnThreadStartCallback cThreadStartCallback);
  VOID SetThreadEndCallback(_In_opt_ OnThreadEndCallback cThreadEndCallback);
  VOID SetThreadStartErrorCallback(_In_opt_ OnThreadStartErrorCallback cThreadStartErrorCallback);

  HRESULT Initialize();
  VOID Finalize();

  //NOTE: 'cCallback' SHOULD be available when a packet is dequeued because a pointer is stored in the IOCP queue
  HRESULT Attach(_In_ HANDLE h, _In_ OnPacketCallback &cCallback);
  HRESULT Post(_In_ OnPacketCallback &cCallback, _In_ DWORD dwBytes, _In_ OVERLAPPED *lpOvr);

  DWORD GetActiveThreadsCount() const
    {
    return (DWORD)__InterlockedRead(const_cast<LONG volatile*>(&(sThreads.nActiveCount)));
    };

  DWORD GetBusyThreadsCount() const
    {
    return (DWORD)__InterlockedRead(const_cast<LONG volatile*>(&(sThreads.nBusyCount)));
    };

  BOOL IsDynamicGrowable() const
    {
    return (dwMaxThreadsCount > dwMinThreadsCount && dwWorkerThreadIdleTimeoutMs != INFINITE) ? TRUE : FALSE;
    };

  static DWORD GetNumberOfProcessors();

private:
  class CThread : public virtual CBaseMemObj, public TClassWorkerThread<CIoCompletionPortThreadPool>
  {
  public:
    CThread() : CBaseMemObj(), TClassWorkerThread<CIoCompletionPortThreadPool>()
      {
      nFlags = 0;
      return;
      };

  public:
    LONG nFlags;
    CLnkLstNode cListNode;
  };

  VOID InternalFinalize();

  HRESULT StartThread(_In_ BOOL bInitial);
  VOID ThreadProc(_In_ SIZE_T nParam);

private:
  LONG volatile nSlimMutex;
  OnThreadStartCallback cThreadStartCallback;
  OnThreadEndCallback cThreadEndCallback;
  OnThreadStartErrorCallback cThreadStartErrorCallback;
  DWORD dwMinThreadsCount, dwMaxThreadsCount;
  DWORD dwWorkerThreadIdleTimeoutMs, dwShutdownThreadThreshold;
  DWORD dwThreadStackSize;
  CIoCompletionPort cIOCP;
  struct {
    LONG volatile nMutex;
    LONG volatile nShuttingDown;
    LONG volatile nBusyCount;
    LONG volatile nActiveCount;
    CLnkLst cList;
  } sThreads;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_IOCOMPLETIONPORT_H
