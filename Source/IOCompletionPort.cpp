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
#include "..\Include\IOCompletionPort.h"
#include "..\Include\Debug.h"

//-----------------------------------------------------------

#define THREAD_FLAGS_CanExit                          0x0001
#define THREAD_FLAGS_IsCleanup                        0x0002

//-----------------------------------------------------------

typedef BOOL (WINAPI *lpfnGetQueuedCompletionStatusEx)(_In_ HANDLE CompletionPort, _Out_ LPOVERLAPPED_ENTRY lpEntries,
                                                       _In_ ULONG ulCount, _Out_ PULONG ulNumEntriesRemoved,
                                                       _In_ DWORD dwMilliseconds, _In_ BOOL fAlertable);

//-----------------------------------------------------------

static lpfnGetQueuedCompletionStatusEx volatile fnGetQueuedCompletionStatusEx = NULL;

//-----------------------------------------------------------

namespace MX {

CIoCompletionPort::CIoCompletionPort() : CBaseMemObj(), CNonCopyableObj()
{
  if (__InterlockedReadPointer(&fnGetQueuedCompletionStatusEx) == NULL)
  {
    LPVOID _fnGetQueuedCompletionStatusEx;
    HINSTANCE hDll;

    _fnGetQueuedCompletionStatusEx = NULL;
    hDll = ::GetModuleHandleW(L"kernelbase.dll");
    if (hDll != NULL)
    {
      _fnGetQueuedCompletionStatusEx = ::GetProcAddress(hDll, "GetQueuedCompletionStatusEx");
    }
    if (_fnGetQueuedCompletionStatusEx == NULL)
    {
      hDll = ::GetModuleHandleW(L"kernel32.dll");
      if (hDll != NULL)
      {
        _fnGetQueuedCompletionStatusEx = ::GetProcAddress(hDll, "GetQueuedCompletionStatusEx");
      }
    }
    _InterlockedExchangePointer((LPVOID volatile*)&fnGetQueuedCompletionStatusEx, _fnGetQueuedCompletionStatusEx);
  }
  hIOCP = NULL;
  return;
}

CIoCompletionPort::~CIoCompletionPort()
{
  Finalize();
  return;
}

HRESULT CIoCompletionPort::Initialize(_In_ DWORD dwMaxConcurrency)
{
  hIOCP = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL,
                                   (dwMaxConcurrency < 0x7FFFFFFFUL) ? dwMaxConcurrency : 0x7FFFFFFFUL);
  if (hIOCP == NULL || hIOCP == INVALID_HANDLE_VALUE)
  {
    hIOCP = NULL;
    return MX_HRESULT_FROM_LASTERROR();
  }
  return S_OK;
}

VOID CIoCompletionPort::Finalize()
{
  if (hIOCP != NULL)
  {
    ::CloseHandle(hIOCP);
    hIOCP = NULL;
  }
  return;
}

HRESULT CIoCompletionPort::Attach(_In_ HANDLE h, _In_ ULONG_PTR nKey)
{
  HANDLE _hiocp;

  if (h == NULL)
    return E_INVALIDARG;
  MX_ASSERT(hIOCP != NULL);
  _hiocp = ::CreateIoCompletionPort(h, hIOCP, nKey, 0);
  if (_hiocp == NULL || _hiocp == INVALID_HANDLE_VALUE)
    return MX_HRESULT_FROM_LASTERROR();
  return S_OK;
}

HRESULT CIoCompletionPort::Post(_In_ ULONG_PTR nKey, _In_ DWORD dwBytes, _In_ OVERLAPPED *lpOvr)
{
  MX_ASSERT(hIOCP != NULL);
  MX_ASSERT(lpOvr != NULL);
  //DebugPrint("PostIoCompletionPort: Ovr=0x%p -> %lu bytes\n", lpOvr, dwBytes);
  if (::PostQueuedCompletionStatus(hIOCP, dwBytes, nKey, lpOvr) == FALSE)
    return MX_HRESULT_FROM_LASTERROR();
  return S_OK;
}

HRESULT CIoCompletionPort::Post(_In_ LPOVERLAPPED_ENTRY lpEntry)
{
  MX_ASSERT(lpEntry != NULL);
  return Post(lpEntry->lpCompletionKey, lpEntry->dwNumberOfBytesTransferred, lpEntry->lpOverlapped);
}

HRESULT CIoCompletionPort::Get(_Out_ LPOVERLAPPED_ENTRY lpEntries, _Inout_ ULONG &nEntriesCount, _In_ DWORD dwTimeoutMs)
{
  ULONG i, nRemoved;

  MX_ASSERT(nEntriesCount > 0);
  MX_ASSERT(hIOCP != NULL);
  if (fnGetQueuedCompletionStatusEx != NULL)
  {
    nRemoved = 0;
    if (fnGetQueuedCompletionStatusEx(hIOCP, lpEntries, nEntriesCount, &nRemoved, dwTimeoutMs, FALSE) == FALSE)
    {
      nEntriesCount = 0;
      return MX_HRESULT_FROM_LASTERROR();
    }
    for (i = 0; i < nRemoved; i++)
    {
#pragma warning(suppress : 6386)
      lpEntries[i].Internal = lpEntries[i].lpOverlapped->Internal =
          ::MxRtlNtStatusToDosError((NTSTATUS)(ULONG)(lpEntries[i].lpOverlapped->Internal));
    }
    nEntriesCount = nRemoved;
  }
  else
  {
    if (::GetQueuedCompletionStatus(hIOCP, &(lpEntries->dwNumberOfBytesTransferred), &(lpEntries->lpCompletionKey),
                                    &(lpEntries->lpOverlapped), dwTimeoutMs) == FALSE)
    {
      if (lpEntries->lpOverlapped == NULL)
      {
        nEntriesCount = 0;
        return MX_HRESULT_FROM_LASTERROR();
      }
      lpEntries->Internal = lpEntries->lpOverlapped->Internal = (ULONG_PTR)::GetLastError();
    }
    else
    {
      lpEntries->Internal = lpEntries->lpOverlapped->Internal = ERROR_SUCCESS;
    }
    nEntriesCount = 1;
  }
  return S_OK;
}

//-----------------------------------------------------------

CIoCompletionPortThreadPool::CIoCompletionPortThreadPool() : CBaseMemObj(), CNonCopyableObj()
{
  cThreadStartCallback = NullCallback();
  cThreadEndCallback = NullCallback();
  cThreadStartErrorCallback = NullCallback();
  dwMinThreadsCount = dwMaxThreadsCount = GetNumberOfProcessors() * 2;
  dwWorkerThreadIdleTimeoutMs = 2000;
  dwShutdownThreadThreshold = 2;
  dwThreadStackSize = 0;
  nThreadPriority = THREAD_PRIORITY_NORMAL;
  FastLock_Initialize(&(sThreads.nMutex));
  _InterlockedExchange(&(sThreads.nActiveCount), 0);
  _InterlockedExchange(&(sThreads.nBusyCount), 0);
  _InterlockedExchange(&(sThreads.nShuttingDown), 0);
  return;
}

CIoCompletionPortThreadPool::~CIoCompletionPortThreadPool()
{
  Finalize();
  return;
}

VOID CIoCompletionPortThreadPool::SetOption_MinThreadsCount(_In_opt_ DWORD dwCount)
{
  if (!cIOCP)
  {
    dwMinThreadsCount = (dwCount == 0) ? GetNumberOfProcessors() * 2 : dwCount;
    if (dwMinThreadsCount > 8192)
      dwMinThreadsCount = 8192;
    if (dwMaxThreadsCount < dwMinThreadsCount)
      dwMaxThreadsCount = dwMinThreadsCount;
    if (dwShutdownThreadThreshold > dwMaxThreadsCount)
      dwShutdownThreadThreshold = dwMaxThreadsCount;
  }
  return;
}

VOID CIoCompletionPortThreadPool::SetOption_MaxThreadsCount(_In_opt_ DWORD dwCount)
{
  if (!cIOCP)
  {
    dwMaxThreadsCount = (dwCount == 0) ? GetNumberOfProcessors() * 2 : dwCount;
    if (dwMaxThreadsCount > 8192)
      dwMaxThreadsCount = 8192;
    if (dwMinThreadsCount > dwMaxThreadsCount)
      dwMinThreadsCount = dwMaxThreadsCount;
    if (dwShutdownThreadThreshold > dwMaxThreadsCount)
      dwShutdownThreadThreshold = dwMaxThreadsCount;
  }
  return;
}

VOID CIoCompletionPortThreadPool::SetOption_WorkerThreadIdleTime(_In_opt_ DWORD dwTimeoutMs)
{
  if (!cIOCP)
  {
    dwWorkerThreadIdleTimeoutMs = dwTimeoutMs;
  }
  return;
}

VOID CIoCompletionPortThreadPool::SetOption_ShutdownThreadThreshold(_In_opt_ DWORD dwThreshold)
{
  if (!cIOCP)
  {
    dwShutdownThreadThreshold = dwThreshold;
    if (dwShutdownThreadThreshold > dwMaxThreadsCount)
      dwShutdownThreadThreshold = dwMaxThreadsCount;
  }
  return;
}

VOID CIoCompletionPortThreadPool::SetOption_ThreadStackSize(_In_opt_ DWORD dwStackSize)
{
  if (!cIOCP)
  {
    dwThreadStackSize = dwStackSize;
  }
  return;
}

VOID CIoCompletionPortThreadPool::SetOption_ThreadPriority(_In_opt_ int nPriority)
{
  if (!cIOCP)
  {
    if (nPriority < -31)
      nThreadPriority = -31;
    else if (nPriority > 31)
      nThreadPriority = 31;
    else
      nThreadPriority = nPriority;
  }
  return;
}

VOID CIoCompletionPortThreadPool::SetThreadStartCallback(_In_opt_ OnThreadStartCallback _cThreadStartCallback)
{
  if (!cIOCP)
  {
    cThreadStartCallback = _cThreadStartCallback;
  }
  return;
}

VOID CIoCompletionPortThreadPool::SetThreadEndCallback(_In_opt_ OnThreadEndCallback _cThreadEndCallback)
{
  if (!cIOCP)
  {
    cThreadEndCallback = _cThreadEndCallback;
  }
  return;
}

VOID CIoCompletionPortThreadPool::SetThreadStartErrorCallback(_In_opt_ OnThreadStartErrorCallback
                                                              _cThreadStartErrorCallback)
{
  if (!cIOCP)
  {
    cThreadStartErrorCallback = _cThreadStartErrorCallback;
  }
  return;
}

HRESULT CIoCompletionPortThreadPool::Initialize()
{
  DWORD i;
  HRESULT hRes;

  InternalFinalize();
  hRes = cIOCP.Initialize(0x7FFFFFFFUL /*dwMaxThreadsCount*/);
  for (i=0; SUCCEEDED(hRes) && i<dwMinThreadsCount; i++)
    hRes = StartThread(TRUE);
  if (FAILED(hRes))
    InternalFinalize();
  return hRes;
}

VOID CIoCompletionPortThreadPool::Finalize()
{
  InternalFinalize();
  return;
}

HRESULT CIoCompletionPortThreadPool::Attach(_In_ HANDLE h, _In_ OnPacketCallback &cCallback)
{
  if (!cIOCP)
    return E_FAIL;
  if (h == NULL || h == INVALID_HANDLE_VALUE)
    return E_INVALIDARG;
  if (!cCallback)
    return E_POINTER;
  return cIOCP.Attach(h, (ULONG_PTR)&cCallback);
}

HRESULT CIoCompletionPortThreadPool::Post(_In_ OnPacketCallback &cCallback, _In_ DWORD dwBytes, _In_ OVERLAPPED *lpOvr)
{
  if (!cIOCP)
    return E_FAIL;
  if ((!cCallback) || lpOvr == NULL)
    return E_POINTER;
  return cIOCP.Post((ULONG_PTR)&cCallback, dwBytes, lpOvr);
}

DWORD CIoCompletionPortThreadPool::GetNumberOfProcessors()
{
  SYSTEM_INFO sInfo;

  ::GetSystemInfo(&sInfo);
  return (sInfo.dwNumberOfProcessors > 1) ? sInfo.dwNumberOfProcessors : 1;
}

VOID CIoCompletionPortThreadPool::InternalFinalize()
{
  OVERLAPPED sDummyOvr;
  BOOL b;

  _InterlockedExchange(&(sThreads.nShuttingDown), 1);
  do
  {
    b = FALSE;

    {
      CFastLock cLock(&(sThreads.nMutex));

      MxMemSet(&sDummyOvr, 0, sizeof(sDummyOvr));
      for (SIZE_T nCount = sThreads.cList.GetCount(); nCount > 0; nCount--)
      {
        cIOCP.Post(0, 0, &sDummyOvr);
        b = TRUE;
      }
    }
    if (b != FALSE)
      ::MxSleep(5);
  }
  while (b != FALSE);
  cIOCP.Finalize();
  _InterlockedExchange(&(sThreads.nActiveCount), 0);
  _InterlockedExchange(&(sThreads.nBusyCount), 0);
  _InterlockedExchange(&(sThreads.nShuttingDown), 0);
  return;
}

HRESULT CIoCompletionPortThreadPool::StartThread(_In_ BOOL bInitial)
{
  CThread *lpThread;
  HRESULT hRes;

  if (bInitial == FALSE)
  {
    if (__InterlockedRead(&(sThreads.nBusyCount)) < __InterlockedRead(&(sThreads.nActiveCount)))
      return S_OK;
  }
  //create worker thread
  lpThread = MX_DEBUG_NEW CThread();
  if (lpThread != NULL)
  {
    lpThread->SetStackSize(dwThreadStackSize);
    lpThread->nFlags = (bInitial != FALSE || dwWorkerThreadIdleTimeoutMs == INFINITE) ? 0 : THREAD_FLAGS_CanExit;
    if (lpThread->Start(this, &CIoCompletionPortThreadPool::ThreadProc, (SIZE_T)lpThread, TRUE) != FALSE)
    {
      {
        CFastLock cLock(&(sThreads.nMutex));

        sThreads.cList.PushTail(&(lpThread->cListNode));
      }
      _InterlockedIncrement(&(sThreads.nActiveCount));
      lpThread->Resume();
      hRes = S_OK;
    }
    else
    {
      delete lpThread;
      hRes = E_OUTOFMEMORY;
    }
  }
  else
  {
    hRes = E_OUTOFMEMORY;
  }
  //inform error if not initial worker
  if (bInitial == FALSE && FAILED(hRes))
  {
    if (cThreadStartErrorCallback)
      cThreadStartErrorCallback(this, hRes);
  }
  //done
  return hRes;
}

VOID CIoCompletionPortThreadPool::ThreadProc(_In_ SIZE_T nParam)
{
  CThread *lpThread = (CThread*)nParam;
  OVERLAPPED_ENTRY sOvrEntries[1], *lpOvrEntry, *lpOvrEntryEnd;
  ULONG nOvrEntriesCount;
  LPVOID lpThreadCustomParam;
  DWORD dwTimeout;
  LONG nActive, nBusy;
  BOOL bShutdown;
  HRESULT hRes;

  //notify
  lpThreadCustomParam = NULL;
  if (cThreadStartCallback)
    cThreadStartCallback(this, lpThreadCustomParam);

  ::SetThreadPriority(::GetCurrentThread(), nThreadPriority);

  //main loop
  bShutdown = FALSE;
  while (bShutdown == FALSE)
  {
    dwTimeout = INFINITE;
    if ((lpThread->nFlags & THREAD_FLAGS_CanExit) != 0)
    {
      nActive = __InterlockedRead(&(sThreads.nActiveCount));
      nBusy = __InterlockedRead(&(sThreads.nBusyCount));
      if (nBusy + (LONG)dwShutdownThreadThreshold < nActive)
        dwTimeout = dwWorkerThreadIdleTimeoutMs;
    }

    nOvrEntriesCount = (ULONG)MX_ARRAYLEN(sOvrEntries);
    hRes = cIOCP.Get(sOvrEntries, nOvrEntriesCount, dwTimeout);
    if (hRes == MX_E_Timeout)
    {
      nActive = __InterlockedRead(&(sThreads.nActiveCount));
      nBusy = __InterlockedRead(&(sThreads.nBusyCount));
      if (nBusy + (LONG)dwShutdownThreadThreshold < nActive)
        break; //shutdown worker thread
      continue;
    }

    //increment busy thread count
    nBusy = _InterlockedIncrement(&(sThreads.nBusyCount));

    //start a new worker thread if all of them are busy
    if (__InterlockedRead(&(sThreads.nShuttingDown)) == 0)
    {
      nActive = __InterlockedRead(&(sThreads.nActiveCount));
      if (nBusy >= nActive && (DWORD)nActive < dwMaxThreadsCount)
      {
        HRESULT hRes2 = StartThread(FALSE);
        if (FAILED(hRes2) && cThreadStartErrorCallback)
          cThreadStartErrorCallback(this, hRes2);
      }
    }

    //process entries
    lpOvrEntryEnd = (lpOvrEntry = sOvrEntries) + nOvrEntriesCount;
    while (lpOvrEntry < lpOvrEntryEnd)
    {
      //a completion key of 0 is posted to the iocp to request us to shut down...
      if (lpOvrEntry->lpCompletionKey == NULL)
      {
        bShutdown = TRUE;
        break;
      }
      if (lpOvrEntry->lpOverlapped != NULL)
      {
        //process packet
        hRes = MX_HRESULT_FROM_WIN32((DWORD)(lpOvrEntry->lpOverlapped->Internal));
        (*reinterpret_cast<OnPacketCallback*>((LPVOID)(lpOvrEntry->lpCompletionKey)))(this,
                                                       lpOvrEntry->dwNumberOfBytesTransferred,
                                                       lpOvrEntry->lpOverlapped, hRes);
      }
      lpOvrEntry++;
    }

    //decrement busy thread count
    _InterlockedDecrement(&(sThreads.nBusyCount));
  }

  //notify
  if (cThreadEndCallback)
    cThreadEndCallback(this, lpThreadCustomParam);

  //remove worker thread from list
  {
    CFastLock cLock(&(sThreads.nMutex));

    lpThread->cListNode.Remove();
    _InterlockedDecrement(&(sThreads.nActiveCount));
  }
  delete lpThread;

  //done
  return;
}

} //namespace MX
