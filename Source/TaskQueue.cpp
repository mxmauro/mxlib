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
#include "..\Include\TaskQueue.h"
#include "..\Include\WaitableObjects.h"
#include "..\Include\Timer.h"

//-----------------------------------------------------------

namespace MX {

CTaskQueue::CTaskQueue() : TRefCounted<CBaseMemObj>(), CIoCompletionPortThreadPool()
{
  RundownProt_Initialize(&nRundownLock);
  cQueuedTaskCallbackWP = MX_BIND_MEMBER_CALLBACK(&CTaskQueue::OnQueuedTask, this);
  _InterlockedExchange(&nQueuedTasksCount, 0);
  dwMaxCpuUsage = 100;
  return;
}

CTaskQueue::~CTaskQueue()
{
  Finalize();
  return;
}

HRESULT CTaskQueue::Initialize()
{
  RundownProt_Initialize(&nRundownLock);
  return CIoCompletionPortThreadPool::Initialize();
}

VOID CTaskQueue::Finalize()
{
  RundownProt_WaitForRelease(&nRundownLock);

  while (HasPending() != FALSE)
    ::MxSleep(10);
  CIoCompletionPortThreadPool::Finalize();
  return;
}

VOID CTaskQueue::SetOption_SetCpuThrottling(_In_ DWORD _dwMaxCpuUsage)
{
  if (_dwMaxCpuUsage < 10)
    dwMaxCpuUsage = 10;
  else if (_dwMaxCpuUsage > 100)
    dwMaxCpuUsage = 100;
  else
    dwMaxCpuUsage = _dwMaxCpuUsage;
  return;
}

BOOL CTaskQueue::HasPending() const
{
  return (__InterlockedRead(const_cast<LONG volatile*>(&nQueuedTasksCount)) > 0) ? TRUE : FALSE;
}

HRESULT CTaskQueue::QueueTask(_In_ CTask *lpTask, _In_ OnRunTaskCallback cCallback)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  HRESULT hRes;

  if (lpTask == NULL || (!cCallback))
    return E_POINTER;
  if (cAutoRundownProt.IsAcquired() == FALSE)
    return MX_E_Cancelled;

  ::MxMemSet(&(lpTask->sOvr), 0, sizeof(lpTask->sOvr));
  lpTask->cCallback = cCallback;

  _InterlockedIncrement(&nQueuedTasksCount);
  AddRef();
  lpTask->AddRef();
  hRes = Post(cQueuedTaskCallbackWP, 0, &(lpTask->sOvr));
  if (FAILED(hRes))
  {
    lpTask->Release();
    Release();
    _InterlockedDecrement(&nQueuedTasksCount);
  }
  return hRes;
}

VOID CTaskQueue::OnQueuedTask(_In_ CIoCompletionPortThreadPool *lpPool, _In_ DWORD dwBytes, _In_ OVERLAPPED *lpOvr,
                              _In_ HRESULT hRes)
{
  CTask *lpTask = CONTAINING_RECORD(lpOvr, CTask, sOvr);
  CTimer cTimer;

  cTimer.GetMarkTimeMs();
  lpTask->cCallback(this, lpTask);
  cTimer.Mark();

  if (dwMaxCpuUsage < 100)
  {
    CAutoRundownProtection cAutoRundownProt(&nRundownLock);

    if (cAutoRundownProt.IsAcquired() != FALSE)
    {
      DWORD dwElapsedMs = cTimer.GetElapsedTimeMs();

      ::MxSleep(dwElapsedMs / dwMaxCpuUsage);
    }
  }

  lpTask->Release();
  _InterlockedDecrement(&nQueuedTasksCount);
  Release();
  return;
}

//-----------------------------------------------------------

CTaskQueue::CTask::CTask() : TRefCounted<CBaseMemObj>()
{
  ::MxMemSet(&sOvr, 0, sizeof(sOvr));
  cCallback = NullCallback();
  return;
}

CTaskQueue::CTask::~CTask()
{
  return;
}

} //namespace MX
