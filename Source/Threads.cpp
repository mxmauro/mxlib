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
#include "..\Include\Threads.h"
#include <process.h>
#include <ObjBase.h>

//-----------------------------------------------------------

#define THREAD_INTERNAL_FLAG_BEINGRELEASED        0x00000001
#ifndef DWORD_MAX
  #define DWORD_MAX 0xFFFFFFFFUL
#endif //!DWORD_MAX

#define X_COWAIT_DISPATCH_CALLS            8
#define X_COWAIT_DISPATCH_WINDOW_MESSAGES  0x10

//-----------------------------------------------------------

#define MS_VC_EXCEPTION                           0x406D1388

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO {
  DWORD dwType;     // Must be 0x1000.
  LPCSTR szName;    // Pointer to name (in user addr space).
  DWORD dwThreadID; // Thread ID (-1=caller thread).
  DWORD dwFlags;    // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

//-----------------------------------------------------------

static int MyExceptionFilter(__out EXCEPTION_POINTERS *lpDest, __in EXCEPTION_POINTERS *lpSrc);

//-----------------------------------------------------------

namespace MX {

CThread::CThread() : CBaseMemObj()
{
  nPriority = THREAD_PRIORITY_NORMAL;
  hThread = NULL;
  dwThreadId = 0;
  dwStackSize = 0;
  bAutoDelete = FALSE;
  return;
}

CThread::~CThread()
{
  //this assertion may hit if app exits because it kills all threads except the main
  //one and IsRunning does only a quick test. just ignore in that case
  //_ASSERT(IsRunning() == FALSE);
  if (hThread != NULL)
  {
    ::CloseHandle(hThread);
    hThread = NULL;
  }
  return;
}

BOOL CThread::Start(__in_opt BOOL bSuspended)
{
  unsigned int tid;

  if (hThread != NULL)
    return TRUE;
  if (FAILED(cKillEvent.Create(TRUE, FALSE)))
    return FALSE;
  hThread = (HANDLE)_beginthreadex(NULL, dwStackSize, &CThread::CommonThreadProc, this,
                                   (bSuspended != FALSE) ? CREATE_SUSPENDED : 0, &tid);
  if (hThread == NULL)
  {
    cKillEvent.Close();
    return FALSE;
  }
  dwThreadId = (DWORD)tid;
  ::SetThreadPriority(hThread, nPriority);
  return TRUE;
}

BOOL CThread::Stop(__in_opt DWORD dwTimeout)
{
  SIZE_T dwRetCode;
  DWORD dwExitCode;

  if (hThread != NULL)
  {
    if (dwThreadId == ::GetCurrentThreadId() || bAutoDelete != FALSE)
      return StopAsync();
    cKillEvent.Set();
    dwRetCode = ::WaitForSingleObject(hThread, dwTimeout);
    if (dwRetCode == WAIT_TIMEOUT)
    {
      dwExitCode = STILL_ACTIVE;
      ::GetExitCodeThread(hThread, &dwExitCode);
      if (dwExitCode != STILL_ACTIVE)
        dwRetCode = WAIT_OBJECT_0;
    }
    if (dwRetCode != WAIT_OBJECT_0)
      ::TerminateThread(hThread, 0);
    ::CloseHandle(hThread);
    hThread = NULL;
  }
  else if (bAutoDelete != FALSE)
  {
    delete this;
    return TRUE;
  }
  dwThreadId = 0;
  cKillEvent.Close();
  return TRUE;
}

BOOL CThread::StopAsync()
{
  if (hThread != NULL)
  {
    cKillEvent.Set();
  }
  return TRUE;
}

BOOL CThread::Pause()
{
  if (hThread == NULL)
    return FALSE;
  if (::SuspendThread(hThread) == (DWORD)-1)
    return FALSE;
  return TRUE;
}

BOOL CThread::Resume()
{
  if (hThread == NULL)
    return FALSE;
  if (::ResumeThread(hThread) == (DWORD)-1)
    return FALSE;
  return TRUE;
}

BOOL CThread::Wait(__in DWORD dwTimeout, __in DWORD dwEventCount, __in_opt LPHANDLE lphEventList,
                   __out_opt LPDWORD lpdwHitEvent)
{
  HANDLE hEvents[50];
  DWORD i, dwRetCode;

  if (dwEventCount > 48)
    return FALSE;
  if (dwEventCount > 0 && lphEventList == NULL)
    return FALSE;
  if (hThread == NULL)
  {
    if (lpdwHitEvent != NULL)
      *lpdwHitEvent = 0;
    return TRUE;
  }
  hEvents[0] = hThread;
  for (i=0; i<dwEventCount; i++)
    hEvents[1+i] = lphEventList[i];
  dwRetCode = ::WaitForMultipleObjects(1+dwEventCount, hEvents, FALSE, dwTimeout);
  if (dwRetCode == WAIT_FAILED)
  {
    if (lpdwHitEvent != NULL)
      *lpdwHitEvent = DWORD_MAX;
    return TRUE;
  }
  if (dwRetCode == WAIT_OBJECT_0)
  {
    if (lpdwHitEvent != NULL)
      *lpdwHitEvent = 0;
    return TRUE;
  }
  if (dwRetCode >= (WAIT_OBJECT_0+1) && dwRetCode <= (WAIT_OBJECT_0+dwEventCount))
  {
    if (lpdwHitEvent != NULL)
      *lpdwHitEvent = dwRetCode - WAIT_OBJECT_0;
    return TRUE;
  }
  if (lpdwHitEvent != NULL)
    *lpdwHitEvent = 0;
  return FALSE;
}

BOOL CThread::IsRunning()
{
  if (hThread == NULL)
    return FALSE;
  if (::WaitForSingleObject(hThread, 0) == WAIT_OBJECT_0)
    return FALSE;
  return TRUE;
}

BOOL CThread::CheckForAbort(__in DWORD dwTimeout, __in DWORD dwEventCount, __out_opt LPHANDLE lphEventList,
                            __out_opt LPDWORD lpdwHitEvent)
{
  HANDLE hEvents[50];
  DWORD i, dwRetCode;

  if (hThread == NULL)
  {
    if (lpdwHitEvent != NULL)
      *lpdwHitEvent = 0;
    return FALSE;
  }
  if (dwEventCount > 48)
    return FALSE;
  if (dwEventCount > 0 && lphEventList == NULL)
    return FALSE;
  hEvents[0] = hThread;
  hEvents[1] = cKillEvent.Get();
  for (i=0; i<dwEventCount; i++)
    hEvents[2+i] = lphEventList[i];
  dwRetCode = ::WaitForMultipleObjects(2+dwEventCount, hEvents, FALSE, dwTimeout);
  if (dwRetCode == WAIT_FAILED)
  {
    if (lpdwHitEvent != NULL)
      *lpdwHitEvent = DWORD_MAX;
    return TRUE;
  }
  if (dwRetCode >= WAIT_OBJECT_0 && dwRetCode <= (WAIT_OBJECT_0+1))
  {
    if (lpdwHitEvent != NULL)
      *lpdwHitEvent = 0;
    return TRUE;
  }
  if (dwRetCode >= (WAIT_OBJECT_0+2) && dwRetCode <= (WAIT_OBJECT_0+1+dwEventCount))
  {
    if (lpdwHitEvent != NULL)
      *lpdwHitEvent = dwRetCode - (WAIT_OBJECT_0+1);
  }
  else
  {
    if (lpdwHitEvent != NULL)
      *lpdwHitEvent = 0;
  }
  return FALSE;
}

VOID CThread::SetThreadName(__in DWORD dwThreadId, __in_z_opt LPCSTR szName)
{
  THREADNAME_INFO sInfo;

  ::Sleep(10);
  sInfo.dwType = 0x1000;
  sInfo.szName = (szName != NULL) ? szName : "";
  sInfo.dwThreadID = (DWORD)dwThreadId;
  sInfo.dwFlags = 0;
  __try
  {
    ::RaiseException(MS_VC_EXCEPTION, 0, sizeof(sInfo)/sizeof(ULONG_PTR), (ULONG_PTR*)&sInfo);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  { }
  return;
}

VOID CThread::SetThreadName(__in_z_opt LPCSTR szName)
{
  if (hThread != NULL)
    SetThreadName(dwThreadId, szName);
  return;
}

BOOL CThread::SetPriority(__in int _nPriority)
{
  if (_nPriority != THREAD_PRIORITY_TIME_CRITICAL &&
      _nPriority != THREAD_PRIORITY_HIGHEST &&
      _nPriority != THREAD_PRIORITY_ABOVE_NORMAL &&
      _nPriority != THREAD_PRIORITY_NORMAL &&
      _nPriority != THREAD_PRIORITY_BELOW_NORMAL &&
      _nPriority != THREAD_PRIORITY_LOWEST &&
      _nPriority != THREAD_PRIORITY_IDLE)
  {
    return FALSE;
  }
  if (hThread != NULL)
  {
    if (::SetThreadPriority(hThread, _nPriority) == FALSE)
      return FALSE;
  }
  nPriority = _nPriority;
  return TRUE;
}

int CThread::GetPriority() const
{
  return nPriority;
}

BOOL CThread::SetStackSize(__in DWORD _dwStackSize)
{
  if (hThread != NULL)
    return FALSE;
  dwStackSize = _dwStackSize;
  return TRUE;
}

DWORD CThread::GetStackSize() const
{
  return dwStackSize;
}

HRESULT CThread::SetAutoDelete(__in BOOL _bAutoDelete)
{
  if (hThread != NULL)
    return MX_E_AlreadyInitialized;
  bAutoDelete = _bAutoDelete;
  return S_OK;
}

unsigned int __stdcall CThread::CommonThreadProc(__in LPVOID lpParameter)
{
  CThread *lpThis = (CThread*)lpParameter;
  BOOL bAutoDelete = lpThis->bAutoDelete;

  lpThis->ThreadProc();
  if (bAutoDelete != FALSE)
    delete lpThis;
  return 0;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CWorkerThread::CWorkerThread(__in lpfnWorkerThread _lpStartRoutine, __in LPVOID _lpParam) : CThread()
{
  lpStartRoutine = _lpStartRoutine;
  lpParam = _lpParam;
  return;
}

CWorkerThread::~CWorkerThread()
{
  return;
}

BOOL CWorkerThread::SetRoutine(__in lpfnWorkerThread _lpStartRoutine, __in LPVOID _lpParam)
{
  lpStartRoutine = _lpStartRoutine;
  lpParam = _lpParam;
  return TRUE;
}

VOID CWorkerThread::ThreadProc()
{
  MX_ASSERT(lpStartRoutine != NULL);
  lpStartRoutine(this, lpParam);
  return;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CThreadPool::CThreadPool()
{
  hIOCP = NULL;
  nMinWorkerThreads = nWorkerThreadsCreateAhead = nThreadShutdownThresholdMs = 0;
  MemSet(&sActiveThreads, 0, sizeof(sActiveThreads));
  MemSet(&sWorkItems, 0, sizeof(sWorkItems));
  sWorkItems.sList.lpNext = sWorkItems.sList.lpPrev = &(sWorkItems.sList);
  _InterlockedExchange(&nInUse, 0);
  return;
}

CThreadPool::~CThreadPool()
{
  Finalize();
  return;
}

BOOL CThreadPool::Initialize(__in ULONG _nMinWorkerThreads, __in ULONG _nWorkerThreadsCreateAhead,
                                __in ULONG _nThreadShutdownThresholdMs)
{
  CCriticalSection::CAutoLock cLock(cMtx);
  DWORD dwOsErr;

  Finalize();
  nMinWorkerThreads = _nMinWorkerThreads;
  if (nMinWorkerThreads > 512)
    nMinWorkerThreads = 512;
  nWorkerThreadsCreateAhead = _nWorkerThreadsCreateAhead;
  if (nWorkerThreadsCreateAhead > 64)
    nWorkerThreadsCreateAhead = 64;
  nThreadShutdownThresholdMs = _nThreadShutdownThresholdMs;
  hIOCP = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
  if (hIOCP == NULL || hIOCP == INVALID_HANDLE_VALUE)
  {
    dwOsErr = ::GetLastError();
    hIOCP = NULL;
    Finalize();
    ::SetLastError(dwOsErr);
    return FALSE;
  }
  ::SetLastError(NOERROR);
  return TRUE;
}

VOID CThreadPool::Finalize()
{
  CCriticalSection::CAutoLock cLock(cMtx);
  WORKITEM *lpWorkItem;
  BOOL b;

  //end worker threads
  if (hIOCP != NULL)
  {
    do
    {
      {
        CFastLock cLock(&(sActiveThreads.nMtx));

        b = (sActiveThreads.nCount > 0) ? TRUE : FALSE;
      }
      if (b != FALSE)
      {
        ::PostQueuedCompletionStatus(hIOCP, 0, 0, NULL);
        ::Sleep(10);
      }
    }
    while (b != FALSE);
  }
  //cancel pending workitems
  do
  {
    {
      CFastLock cLock(&(sWorkItems.nMtx));

      if (sWorkItems.sList.lpNext != &(sWorkItems.sList))
      {
        lpWorkItem = (WORKITEM*)((LPBYTE)(sWorkItems.sList.lpNext) - FIELD_OFFSET(WORKITEM, sLink));
        RemoveTask(lpWorkItem);
      }
      else
      {
        lpWorkItem = NULL;
      }
    }
    if (lpWorkItem != NULL)
    {
      CancelTask(lpWorkItem);
      MX_FREE(lpWorkItem);
    }
  }
  while (lpWorkItem != NULL);
  //close completion port
  if (hIOCP != NULL)
  {
    ::CloseHandle(hIOCP);
    hIOCP = NULL;
  }
  //----
  nMinWorkerThreads = nWorkerThreadsCreateAhead = nThreadShutdownThresholdMs = 0;
  return;
}

BOOL CThreadPool::IsInitialized()
{
  CCriticalSection::CAutoLock cLock(cMtx);

  return (hIOCP != NULL) ? TRUE : FALSE;
}

LPVOID CThreadPool::OnThreadStarted()
{
  return NULL;
}

VOID CThreadPool::OnThreadTerminated(__in LPVOID lpContext)
{
  return;
}

BOOL CThreadPool::QueueTask(__in LPTHREAD_START_ROUTINE lpRoutine, __in LPVOID lpContext)
{
  CCriticalSection::CAutoLock cLock(cMtx);
  WORKITEM *lpNewWorkItem;
  DWORD dwOsErr;

  if (hIOCP == NULL)
  {
    ::SetLastError(ERROR_NOT_READY);
    return FALSE;
  }
  if (lpRoutine == NULL)
  {
    ::SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  //initialize worker threads
  while (1)
  {
    if ((nInUse > 0x7FFFFFFF || (SIZE_T)nInUse+nWorkerThreadsCreateAhead < sActiveThreads.nCount) &&
        sActiveThreads.nCount >= (SIZE_T)nMinWorkerThreads)
      break;
    dwOsErr = InitializeWorker();
    if (dwOsErr != NOERROR)
    {
      ::SetLastError(dwOsErr);
      return FALSE;
    }
  }
  //create new work item
  lpNewWorkItem = (WORKITEM*)MX_MALLOC(sizeof(WORKITEM));
  if (lpNewWorkItem == NULL)
  {
    ::SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return FALSE;
  }
  lpNewWorkItem->sLink.lpNext = lpNewWorkItem->sLink.lpPrev = &(lpNewWorkItem->sLink);
  lpNewWorkItem->lpRoutine = lpRoutine;
  lpNewWorkItem->lpContext = lpContext;
  {
    CFastLock cWorkItemsLock(&(sWorkItems.nMtx));

    lpNewWorkItem->sLink.lpNext = &(sWorkItems.sList);
    lpNewWorkItem->sLink.lpPrev = sWorkItems.sList.lpPrev;
    sWorkItems.sList.lpPrev->lpNext = &(lpNewWorkItem->sLink);
    sWorkItems.sList.lpPrev = &(lpNewWorkItem->sLink);
    if (::PostQueuedCompletionStatus(hIOCP, 0, (ULONG_PTR)lpNewWorkItem, NULL) == FALSE)
    {
      dwOsErr = ::GetLastError();
      RemoveTask(lpNewWorkItem);
      MX_FREE(lpNewWorkItem);
      ::SetLastError(dwOsErr);
      return FALSE;
    }
  }
  ::SetLastError(NOERROR);
  return TRUE;
}

DWORD CThreadPool::InitializeWorker()
{
  CFastLock cLock(&(sActiveThreads.nMtx));
  CWorkerThread **lplpNewList;
  CWorkerThread *lpWrk;

  if (sActiveThreads.nCount >= sActiveThreads.nSize)
  {
    lplpNewList = (CWorkerThread**)MX_MALLOC((sActiveThreads.nSize+32) * sizeof(CWorkerThread*));
    if (lplpNewList == NULL)
      return ERROR_NOT_ENOUGH_MEMORY;
    MemCopy(lplpNewList, sActiveThreads.lplpWorkerThreadsList, sActiveThreads.nCount*sizeof(CWorkerThread*));
    MX_FREE(sActiveThreads.lplpWorkerThreadsList);
    sActiveThreads.lplpWorkerThreadsList = lplpNewList;
    sActiveThreads.nSize += 32;
  }
  //create a new worker thread
  sActiveThreads.lplpWorkerThreadsList[sActiveThreads.nCount] = lpWrk = MX_DEBUG_NEW CWorkerThread();
  if (lpWrk == NULL)
    return ERROR_NOT_ENOUGH_MEMORY;
  if (lpWrk->Start(this, &CThreadPool::WorkerThreadProc, (SIZE_T)lpWrk) == FALSE)
  {
    delete lpWrk;
    sActiveThreads.lplpWorkerThreadsList[sActiveThreads.nCount] = NULL;
    return ERROR_NOT_ENOUGH_MEMORY;
  }
  lpWrk->SetThreadName("ThreadPool/Worker");
  (sActiveThreads.nCount)++;
  return NOERROR;
}

VOID CThreadPool::RemoveWorker(__in CWorkerThread *lpWorker)
{
  CFastLock cLock(&(sActiveThreads.nMtx));
  SIZE_T i;

  for (i=0; i<sActiveThreads.nCount; i++)
  {
    if (sActiveThreads.lplpWorkerThreadsList[i] == lpWorker)
      break;
  }
  MX_ASSERT(i < sActiveThreads.nCount);
  delete sActiveThreads.lplpWorkerThreadsList[i];
  MemMove(sActiveThreads.lplpWorkerThreadsList+i, sActiveThreads.lplpWorkerThreadsList+i+1,
          (sActiveThreads.nCount-i-1)*sizeof(CWorkerThread*));
  (sActiveThreads.nCount)--;
  return;
}

VOID CThreadPool::RemoveTask(__in WORKITEM *lpWorkItem)
{
  lpWorkItem->sLink.lpPrev->lpNext = lpWorkItem->sLink.lpNext;
  lpWorkItem->sLink.lpNext->lpPrev = lpWorkItem->sLink.lpPrev;
  lpWorkItem->sLink.lpNext = lpWorkItem->sLink.lpPrev = &(lpWorkItem->sLink);
  return;
}

VOID CThreadPool::ExecuteTask(__in WORKITEM *lpWorkItem)
{
#ifndef _DEBUG
  EXCEPTION_POINTERS sExcPtr;
#endif //!_DEBUG
  HRESULT hRes;

#ifndef _DEBUG
  __try
  {
#endif //!_DEBUG
    hRes = lpWorkItem->lpRoutine(lpWorkItem->lpContext);
    __try
    {
      OnTaskTerminated(lpWorkItem->lpContext, hRes);
    }
    __finally
    { }
#ifndef _DEBUG
  }
  __except(MyExceptionFilter(&sExcPtr, GetExceptionInformation()))
  {
    __try
    {
      OnTaskExceptionError(lpWorkItem->lpContext, GetExceptionCode(), &sExcPtr);
    }
    __finally
    { }
  }
#endif //!_DEBUG
  return;
}

VOID CThreadPool::CancelTask(__in WORKITEM *lpWorkItem)
{
  __try
  {
    OnTaskCancelled(lpWorkItem->lpContext);
  }
  __finally
  { }
  return;
}

VOID CThreadPool::WorkerThreadProc(__in SIZE_T nParam)
{
  DWORD dwNumberOfBytes;
  WORKITEM *lpWorkItem;
  OVERLAPPED *lpOvr;
  ULONG nTimeout;
  LPVOID lpThreadContext;

  lpThreadContext = OnThreadStarted();
  nTimeout = INFINITE; //at least process one item
  while (1)
  {
    if (::GetQueuedCompletionStatus(hIOCP, &dwNumberOfBytes, (PULONG_PTR)&lpWorkItem, &lpOvr, nTimeout) == FALSE ||
        lpWorkItem == NULL)
      break;
    nTimeout = nThreadShutdownThresholdMs;
    //remove task from queue
    {
      CFastLock cLock(&(sWorkItems.nMtx));

      RemoveTask(lpWorkItem);
    }
    //execute task
    _InterlockedIncrement(&nInUse);
    ExecuteTask(lpWorkItem);
    //delete task
    MX_FREE(lpWorkItem);
    _InterlockedDecrement(&nInUse);
  }
  RemoveWorker((CWorkerThread*)nParam);
  OnThreadTerminated(lpThreadContext);
  return;
}

} //namespace MX

//-----------------------------------------------------------

static int MyExceptionFilter(__out EXCEPTION_POINTERS *lpDest, __in EXCEPTION_POINTERS *lpSrc)
{
  MX::MemCopy(lpDest, lpSrc, sizeof(EXCEPTION_POINTERS));
  return EXCEPTION_EXECUTE_HANDLER;
}
