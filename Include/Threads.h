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
#ifndef _MX_THREADS_H
#define _MX_THREADS_H

#include "Defines.h"
#include "WaitableObjects.h"

//-----------------------------------------------------------

namespace MX {

class MX_NOVTABLE CThread : public virtual CBaseMemObj
{
protected:
  CThread();
public:
  virtual ~CThread();

  virtual BOOL Start(_In_opt_ BOOL bSuspended = FALSE);
  virtual BOOL Stop(_In_opt_ DWORD dwTimeout = INFINITE);
  virtual BOOL StopAsync();
  virtual BOOL Pause();
  virtual BOOL Resume();
  virtual BOOL IsRunning();
  virtual BOOL CheckForAbort(_In_opt_ DWORD dwTimeout = 0, _In_opt_ DWORD dwEventCount = 0,
                             _In_opt_ LPHANDLE lphEventList = NULL, _Out_opt_ LPDWORD lpdwHitEvent = NULL);

  static VOID SetThreadName(_In_ DWORD dwThreadId, _In_opt_z_ LPCSTR szNameA);
  virtual VOID SetThreadName(_In_opt_z_ LPCSTR szNameA);

  virtual BOOL SetPriority(_In_ int nPriority);
  virtual int GetPriority() const;

  virtual BOOL SetStackSize(_In_opt_ DWORD dwStackSize = 0);
  virtual DWORD GetStackSize() const;

  virtual HANDLE GetKillEvent()
    {
    return cKillEvent.Get();
    };

  virtual HANDLE GetThreadHandle()
    {
    return hThread;
    };
  virtual DWORD GetThreadId()
    {
    return dwThreadId;
    };

  HRESULT SetAutoDelete(_In_ BOOL bAutoDelete);

  virtual BOOL Wait(_In_opt_ DWORD dwTimeout = 0, _In_opt_ DWORD dwEventCount = 0,
                    _In_opt_ LPHANDLE lphEventList = NULL, _Out_opt_ LPDWORD lpdwHitEvent = NULL);

  virtual VOID ThreadProc() = 0;

private:
  static unsigned int __stdcall CommonThreadProc(_In_ LPVOID _lpParameter);

protected:
  int nPriority;
  HANDLE hThread;
  DWORD dwThreadId, dwStackSize;
  CWindowsEvent cKillEvent;
  BOOL bAutoDelete;
};

//-----------------------------------------------------------

class CWorkerThread : public CThread, public CNonCopyableObj
{
public:
  typedef VOID (__cdecl *lpfnWorkerThread)(_In_ CWorkerThread *lpWrkThread, _In_opt_ LPVOID lpParam);

  CWorkerThread(_In_opt_ lpfnWorkerThread lpStartRoutine=NULL, _In_opt_ LPVOID lpParam=NULL);
  virtual ~CWorkerThread();

  virtual BOOL SetRoutine(_In_ lpfnWorkerThread lpStartRoutine, _In_opt_ LPVOID lpParam=NULL);

private:
  VOID ThreadProc();

private:
  lpfnWorkerThread lpStartRoutine;
  LPVOID lpParam;
};

//-----------------------------------------------------------

template <class TClass>
class TClassWorkerThread : public CThread, public CNonCopyableObj
{
public:
  TClassWorkerThread() : CThread(), CNonCopyableObj()
    {
    lpObject = NULL;
    lpStartRoutine = NULL;
    lpStartRoutineWithParam = NULL;
    return;
    };

  virtual BOOL Start(_In_opt_ BOOL bSuspended=FALSE)
    {
    if (lpObject == NULL || (lpStartRoutine == NULL && lpStartRoutineWithParam == NULL))
      return FALSE;
    return CThread::Start(bSuspended);
    };
  
  virtual BOOL Start(_In_ TClass *_lpObject, _In_ VOID (TClass::* _lpStartRoutine)(),
                     _In_ BOOL bSuspended = FALSE)
    {
    lpObject = _lpObject;
    lpStartRoutine = _lpStartRoutine;
    lpStartRoutineWithParam = NULL;
    nParam = 0;
    return Start(bSuspended);
    }

  virtual BOOL Start(_In_ TClass *_lpObject, _In_ VOID (TClass::* _lpStartRoutine)(SIZE_T),
                     _In_ SIZE_T _nParam, _In_ BOOL bSuspended = FALSE)
    {
    lpObject = _lpObject;
    lpStartRoutine = NULL;
    lpStartRoutineWithParam = _lpStartRoutine;
    nParam = _nParam;
    return Start(bSuspended);
    }

private:
  virtual VOID ThreadProc()
    {
    if (lpStartRoutine != NULL)
      (*lpObject.*lpStartRoutine)();
    else
      (*lpObject.*lpStartRoutineWithParam)(nParam);
    return;
    }

private:
  TClass *lpObject;
  VOID (TClass::* lpStartRoutine)();
  VOID (TClass::* lpStartRoutineWithParam)(SIZE_T);
  SIZE_T nParam;
};

//-----------------------------------------------------------

class CThreadPool : public virtual CBaseMemObj
{
public:
  CThreadPool();
  virtual ~CThreadPool();

  BOOL Initialize(_In_ ULONG nMinWorkerThreads, _In_ ULONG nWorkerThreadsCreateAhead,
                  _In_ ULONG nThreadShutdownThresholdMs);
  VOID Finalize();

  BOOL IsInitialized();

  BOOL QueueTask(_In_ LPTHREAD_START_ROUTINE lpRoutine, _In_ LPVOID lpContext);

  virtual LPVOID OnThreadStarted();
  virtual VOID OnThreadTerminated(_In_ LPVOID lpContext);

  virtual VOID OnTaskTerminated(_In_ LPVOID lpContext, _In_ HRESULT hReturnValue) = 0;
  virtual VOID OnTaskCancelled(_In_ LPVOID lpContext) = 0;
  virtual VOID OnTaskExceptionError(_In_ LPVOID lpContext, _In_ DWORD dwException,
                                    _In_ struct _EXCEPTION_POINTERS *excPtr) = 0;

private:
  typedef TClassWorkerThread<CThreadPool> CWorkerThread;

  typedef struct tagDLLIST_ITEM {
    struct tagDLLIST_ITEM *lpNext, *lpPrev;
  } DLLIST_ITEM;

  typedef struct tagWORKITEM {
    DLLIST_ITEM sLink;
    LPTHREAD_START_ROUTINE lpRoutine;
    LPVOID lpContext;
  } WORKITEM;

private:
  DWORD InitializeWorker();
  VOID RemoveWorker(_In_ CWorkerThread *lpWorker);
  VOID RemoveTask(_In_ WORKITEM *lpWorkItem);
  VOID ExecuteTask(_In_ WORKITEM *lpWorkItem);
  VOID CancelTask(_In_ WORKITEM *lpWorkItem);
  VOID WorkerThreadProc(_In_ SIZE_T nParam);

private:
  CCriticalSection cMtx;
  HANDLE hIOCP;
  ULONG nMinWorkerThreads;
  ULONG nWorkerThreadsCreateAhead;
  ULONG nThreadShutdownThresholdMs;
  struct {
    LONG volatile nMtx;
    DLLIST_ITEM sList;
  } sWorkItems;
  struct {
    LONG volatile nMtx;
    CWorkerThread **lplpWorkerThreadsList;
    SIZE_T nCount, nSize;
  } sActiveThreads;
  LONG volatile nInUse;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_THREADS_H
