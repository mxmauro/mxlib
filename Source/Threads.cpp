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
#include "..\Include\Threads.h"
#include "Internals\MsVcrt.h"
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

typedef HRESULT (WINAPI *lpfnSetThreadDescription)(_In_ HANDLE hThread, _In_z_ PCWSTR lpThreadDescription);

typedef int (*_PIFV)(void);

//-----------------------------------------------------------

static int __cdecl InitializeThreads();

MX_LINKER_FORCE_INCLUDE(___mx_threads_init);
#pragma section(".CRT$XIB", long, read)
extern "C" __declspec(allocate(".CRT$XIB")) const _PIFV ___mx_threads_init = &InitializeThreads;

static lpfnSetThreadDescription fnSetThreadDescription = NULL;

//-----------------------------------------------------------

static int MyExceptionFilter(_Out_ EXCEPTION_POINTERS *lpDest, _In_ EXCEPTION_POINTERS *lpSrc);

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

BOOL CThread::Start(_In_opt_ BOOL bSuspended)
{
  unsigned int tid;

  if (hThread != NULL)
  {
    if (::WaitForSingleObject(hThread, 0) != WAIT_OBJECT_0)
      return TRUE;
    ::CloseHandle(hThread);
    hThread = NULL;
    cKillEvent.Close();
    dwThreadId = 0;
  }
  if (FAILED(cKillEvent.Create(TRUE, FALSE)))
    return FALSE;
  hThread = (HANDLE)_beginthreadex(NULL, dwStackSize, &CThread::CommonThreadProc, this, CREATE_SUSPENDED, &tid);
  if (hThread == NULL)
  {
    cKillEvent.Close();
    return FALSE;
  }
  dwThreadId = (DWORD)tid;
  ::SetThreadPriority(hThread, nPriority);
  if (bSuspended == FALSE)
    ::ResumeThread(hThread);
  return TRUE;
}

BOOL CThread::Stop(_In_opt_ DWORD dwTimeout)
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
    {
#pragma warning(suppress : 6258)
      ::TerminateThread(hThread, 0);
    }
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

BOOL CThread::Wait(_In_opt_ DWORD dwTimeout, _In_opt_ DWORD dwEventCount, _In_opt_ LPHANDLE lphEventList,
                   _Out_opt_ LPDWORD lpdwHitEvent)
{
  HANDLE hEvents[50];
  DWORD i, dwRetCode;

  if (lpdwHitEvent != NULL)
    *lpdwHitEvent = 0;
  if (dwEventCount > 48)
    return FALSE;
  if (dwEventCount > 0 && lphEventList == NULL)
    return FALSE;
  if (hThread == NULL)
    return TRUE;
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

BOOL CThread::CheckForAbort(_In_opt_ DWORD dwTimeout, _In_opt_ DWORD dwEventCount, _In_opt_ LPHANDLE lphEventList,
                            _Out_opt_ LPDWORD lpdwHitEvent)
{
  HANDLE hEvents[50];
  DWORD i, dwRetCode;

  if (lpdwHitEvent != NULL)
    *lpdwHitEvent = 0;
  if (hThread == NULL || dwEventCount > 48)
    return FALSE;
  if (dwEventCount > 0 && lphEventList == NULL)
    return FALSE;
  hEvents[0] = hThread;
  hEvents[1] = cKillEvent.Get();
  for (i=0; i<dwEventCount; i++)
    hEvents[2+i] = lphEventList[i];
  dwRetCode = ::WaitForMultipleObjects(2 + dwEventCount, hEvents, FALSE, dwTimeout);
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

VOID CThread::SetThreadName(_In_ DWORD dwThreadId, _In_opt_z_ LPCSTR szNameA)
{
  THREADNAME_INFO sInfo;

  if (fnSetThreadDescription != NULL)
  {
    HANDLE hThread = ::OpenThread(THREAD_SET_LIMITED_INFORMATION, FALSE, dwThreadId);
    if (hThread != NULL)
    {
      WCHAR szBufW[256];
      SIZE_T i;

      for (i = 0; i < 255 && szNameA[i] != 0; i++)
      {
        szBufW[i] = (WCHAR)(unsigned char)szNameA[i];
      }
      szBufW[i] = 0;
      if (fnSetThreadDescription(hThread, szBufW) >= 0)
      {
        ::CloseHandle(hThread);
        return;
      }
      ::CloseHandle(hThread);
    }
  }

  ::Sleep(10);
  sInfo.dwType = 0x1000;
  sInfo.szName = (szNameA != NULL) ? szNameA : "";
  sInfo.dwThreadID = (DWORD)dwThreadId;
  sInfo.dwFlags = 0;
  __try
  {
    ::RaiseException(MS_VC_EXCEPTION, 0, sizeof(sInfo) / sizeof(ULONG_PTR), (ULONG_PTR *)&sInfo);
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  { }
  return;
}

VOID CThread::SetThreadName(_In_opt_z_ LPCSTR szNameA)
{
  if (hThread != NULL)
  {
    SetThreadName(dwThreadId, szNameA);
  }
  return;
}

BOOL CThread::SetPriority(_In_ int _nPriority)
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

BOOL CThread::SetStackSize(_In_opt_ DWORD _dwStackSize)
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

HRESULT CThread::SetAutoDelete(_In_ BOOL _bAutoDelete)
{
  if (hThread != NULL)
    return MX_E_AlreadyInitialized;
  bAutoDelete = _bAutoDelete;
  return S_OK;
}

unsigned int __stdcall CThread::CommonThreadProc(_In_ LPVOID lpParameter)
{
  CThread *lpThis = (CThread*)lpParameter;
  BOOL bAutoDelete = lpThis->bAutoDelete;

  lpThis->ThreadProc();
  //lpThis->cKillEvent.Set();
  if (bAutoDelete != FALSE)
    delete lpThis;
  return 0;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CWorkerThread::CWorkerThread(_In_opt_ lpfnWorkerThread _lpStartRoutine, _In_opt_ LPVOID _lpParam) : CThread()
{
  lpStartRoutine = _lpStartRoutine;
  lpParam = _lpParam;
  return;
}

CWorkerThread::~CWorkerThread()
{
  return;
}

BOOL CWorkerThread::SetRoutine(_In_ lpfnWorkerThread _lpStartRoutine, _In_opt_ LPVOID _lpParam)
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
  MxMemSet(&sActiveThreads, 0, sizeof(sActiveThreads));
  MxMemSet(&sWorkItems, 0, sizeof(sWorkItems));
  sWorkItems.sList.lpNext = sWorkItems.sList.lpPrev = &(sWorkItems.sList);
  _InterlockedExchange(&nInUse, 0);
  return;
}

CThreadPool::~CThreadPool()
{
  Finalize();
  return;
}

BOOL CThreadPool::Initialize(_In_ ULONG _nMinWorkerThreads, _In_ ULONG _nWorkerThreadsCreateAhead,
                                _In_ ULONG _nThreadShutdownThresholdMs)
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

VOID CThreadPool::OnThreadTerminated(_In_ LPVOID lpContext)
{
  return;
}

BOOL CThreadPool::QueueTask(_In_ LPTHREAD_START_ROUTINE lpRoutine, _In_ LPVOID lpContext)
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
    MxMemCopy(lplpNewList, sActiveThreads.lplpWorkerThreadsList, sActiveThreads.nCount*sizeof(CWorkerThread*));
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

VOID CThreadPool::RemoveWorker(_In_ CWorkerThread *lpWorker)
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
  ::MxMemMove(sActiveThreads.lplpWorkerThreadsList + i, sActiveThreads.lplpWorkerThreadsList + i + 1,
              (sActiveThreads.nCount - i - 1) * sizeof(CWorkerThread*));
  (sActiveThreads.nCount)--;
  return;
}

VOID CThreadPool::RemoveTask(_In_ WORKITEM *lpWorkItem)
{
  lpWorkItem->sLink.lpPrev->lpNext = lpWorkItem->sLink.lpNext;
  lpWorkItem->sLink.lpNext->lpPrev = lpWorkItem->sLink.lpPrev;
  lpWorkItem->sLink.lpNext = lpWorkItem->sLink.lpPrev = &(lpWorkItem->sLink);
  return;
}

VOID CThreadPool::ExecuteTask(_In_ WORKITEM *lpWorkItem)
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

VOID CThreadPool::CancelTask(_In_ WORKITEM *lpWorkItem)
{
  __try
  {
    OnTaskCancelled(lpWorkItem->lpContext);
  }
  __finally
  { }
  return;
}

VOID CThreadPool::WorkerThreadProc(_In_ SIZE_T nParam)
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
    {
      break;
    }
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

static int __cdecl InitializeThreads()
{
  HINSTANCE hDll;

  hDll = ::GetModuleHandleW(L"kernel32.dll");
  if (hDll != NULL)
  {
    fnSetThreadDescription = (lpfnSetThreadDescription)::GetProcAddress(hDll, "SetThreadDescription");
  }
  return 0;
}

static int MyExceptionFilter(_Out_ EXCEPTION_POINTERS *lpDest, _In_ EXCEPTION_POINTERS *lpSrc)
{
  ::MxMemCopy(lpDest, lpSrc, sizeof(EXCEPTION_POINTERS));
  return EXCEPTION_EXECUTE_HANDLER;
}
