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

  virtual BOOL Start(__in BOOL bSuspended=FALSE);
  virtual BOOL Stop(__in DWORD dwTimeout=INFINITE);
  virtual BOOL StopAsync();
  virtual BOOL Pause();
  virtual BOOL Resume();
  virtual BOOL IsRunning();
  virtual BOOL CheckForAbort(__in DWORD dwTimeout=0, __in DWORD dwEventCount=0,
                             __out_opt LPHANDLE lphEventList=NULL, __out_opt LPDWORD lpdwHitEvent=NULL);
  virtual BOOL MsgCheckForAbort(__in DWORD dwTimeout=INFINITE, __in DWORD dwEventCount=0,
                                __out_opt LPHANDLE lphEventList=NULL,
                                __out_opt LPDWORD lpdwHitEvent=NULL);

  static VOID SetThreadName(__in DWORD dwThreadId, __in_z_opt LPCSTR szName);
  virtual VOID SetThreadName(__in_z_opt LPCSTR szName);

  virtual BOOL SetPriority(__in int nPriority);
  virtual int GetPriority() const;

  virtual BOOL SetStackSize(__in DWORD dwStackSize=0);
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

  HRESULT SetAutoDelete(__in BOOL bAutoDelete);

  virtual BOOL Wait(__in DWORD dwTimeout=0, __in DWORD dwEventCount=0,
                    __in_opt LPHANDLE lphEventList=NULL, __out_opt LPDWORD lpdwHitEvent=NULL);
  virtual BOOL MsgWait(__in DWORD dwTimeout=INFINITE, __in DWORD dwEventCount=0,
                       __in_opt LPHANDLE lphEventList=NULL, __out_opt LPDWORD lpdwHitEvent=NULL);
  virtual BOOL CoWait(__in DWORD dwTimeout=INFINITE, __in DWORD dwEventCount=0,
                      __in_opt LPHANDLE lphEventList=NULL, __out_opt LPDWORD lpdwHitEvent=NULL);

  virtual VOID ThreadProc()=0;

  static DWORD MsgWaitAndDispatchMessages(__in DWORD dwTimeout, __in DWORD dwEventsCount, __in LPHANDLE lpHandles);
  static DWORD CoWaitAndDispatchMessages(__in DWORD dwTimeout, __in DWORD dwEventsCount, __in LPHANDLE lpHandles);

protected:
  int nPriority;
  HANDLE hThread;
  DWORD dwThreadId, dwStackSize;
  CWindowsEvent cKillEvent, cThreadEndedOK;
  BOOL bAutoDelete;
};

//-----------------------------------------------------------

class CWorkerThread : public CThread
{
  MX_DISABLE_COPY_CONSTRUCTOR(CWorkerThread);
public:
  typedef VOID (__cdecl *lpfnWorkerThread)(__in CWorkerThread *lpWrkThread, __in LPVOID lpParam);

  CWorkerThread(__in lpfnWorkerThread lpStartRoutine=NULL, __in LPVOID lpParam=NULL);
  virtual ~CWorkerThread();

  virtual BOOL SetRoutine(__in lpfnWorkerThread lpStartRoutine, __in LPVOID lpParam=NULL);

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
    };

  virtual BOOL Start(__in BOOL bSuspended=FALSE)
    {
    if (lpObject==NULL || (lpStartRoutine==NULL && lpStartRoutineWithParam==NULL))
      return FALSE;
    return CThread::Start(bSuspended);
    };
  
  virtual BOOL Start(__in TClass *_lpObject, __in VOID (TClass::* _lpStartRoutine)(),
                     __in BOOL bSuspended=FALSE)
    {
    lpObject = _lpObject;
    lpStartRoutine = _lpStartRoutine;
    lpStartRoutineWithParam = NULL;
    nParam = 0;
    return Start(bSuspended);
    }

  virtual BOOL Start(__in TClass *_lpObject, __in VOID (TClass::* _lpStartRoutine)(SIZE_T),
                     __in SIZE_T _nParam, __in BOOL bSuspended=FALSE)
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

  BOOL Initialize(__in ULONG nMinWorkerThreads, __in ULONG nWorkerThreadsCreateAhead,
                  __in ULONG nThreadShutdownThresholdMs);
  VOID Finalize();

  BOOL IsInitialized();

  BOOL QueueTask(__in LPTHREAD_START_ROUTINE lpRoutine, __in LPVOID lpContext);

  virtual LPVOID OnThreadStarted();
  virtual VOID OnThreadTerminated(__in LPVOID lpContext);

  virtual VOID OnTaskTerminated(__in LPVOID lpContext, __in HRESULT hReturnValue) = 0;
  virtual VOID OnTaskCancelled(__in LPVOID lpContext) = 0;
  virtual VOID OnTaskExceptionError(__in LPVOID lpContext, __in DWORD dwException,
                                    __in struct _EXCEPTION_POINTERS *excPtr) = 0;

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
  VOID RemoveWorker(__in CWorkerThread *lpWorker);
  VOID RemoveTask(__in WORKITEM *lpWorkItem);
  VOID ExecuteTask(__in WORKITEM *lpWorkItem);
  VOID CancelTask(__in WORKITEM *lpWorkItem);
  VOID WorkerThreadProc(__in SIZE_T nParam);

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
