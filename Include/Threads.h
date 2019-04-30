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
#ifndef _MX_THREADS_H
#define _MX_THREADS_H

#include "Defines.h"
#include "WaitableObjects.h"

//-----------------------------------------------------------

namespace MX {

class CThread : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CThread);
public:
  CThread();
  virtual ~CThread();

  virtual BOOL Start(_In_opt_ BOOL bSuspended=FALSE);
  virtual BOOL Stop(_In_opt_ DWORD dwTimeout=INFINITE);
  virtual BOOL StopAsync();
  virtual BOOL Pause();
  virtual BOOL Resume();
  virtual BOOL IsRunning();
  virtual BOOL CheckForAbort(_In_opt_ DWORD dwTimeout = 0, _In_opt_ DWORD dwEventCount = 0,
                             _In_opt_ LPHANDLE lphEventList = NULL, _Out_opt_ LPDWORD lpdwHitEvent = NULL);

  static VOID SetThreadName(_In_ DWORD dwThreadId, _In_opt_z_ LPCSTR szName);
  virtual VOID SetThreadName(_In_opt_z_ LPCSTR szName);

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

class CWorkerThread : public CThread
{
  MX_DISABLE_COPY_CONSTRUCTOR(CWorkerThread);
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
class TClassWorkerThread : public CThread
{
  MX_DISABLE_COPY_CONSTRUCTOR(TClassWorkerThread<TClass>);
public:
  TClassWorkerThread()
    {
    lpObject = NULL;
    lpStartRoutine = NULL;
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
                     _In_ SIZE_T _nParam, _In_ BOOL bSuspended=FALSE)
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
