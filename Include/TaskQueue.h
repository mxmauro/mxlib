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
#ifndef _MX_TASK_QUEUE_H
#define _MX_TASK_QUEUE_H

#include "IOCompletionPort.h"
#include "RefCounted.h"

//-----------------------------------------------------------

namespace MX {

class CTaskQueue : public virtual CBaseMemObj, private MX::CIoCompletionPortThreadPool
{
public:
  class CTask;

  typedef MX::Callback<VOID (_In_ CTaskQueue *lpQueue, _In_ CTask *lpTask)> OnRunTaskCallback;

public:
  class CTask : public virtual TRefCounted<CBaseMemObj>
  {
  public:
    CTask();
    ~CTask();

  private:
    friend class CTaskQueue;

    OVERLAPPED sOvr;
    OnRunTaskCallback cCallback;
  };

public:
  CTaskQueue();
  ~CTaskQueue();

  using CIoCompletionPortThreadPool::SetOption_MinThreadsCount;
  using CIoCompletionPortThreadPool::SetOption_MaxThreadsCount;
  using CIoCompletionPortThreadPool::SetOption_WorkerThreadIdleTime;
  using CIoCompletionPortThreadPool::SetOption_ShutdownThreadThreshold;
  using CIoCompletionPortThreadPool::SetOption_ThreadStackSize;
  using CIoCompletionPortThreadPool::SetOption_ThreadPriority;

  HRESULT Initialize();
  VOID Finalize();

  VOID SetOption_SetCpuThrottling(_In_ DWORD dwMaxCpuUsage);

  BOOL HasPending() const;

  HRESULT QueueTask(_In_ CTask *lpTask, _In_ OnRunTaskCallback cCallback);

private:
  VOID OnQueuedTask(_In_ MX::CIoCompletionPortThreadPool *lpPool, _In_ DWORD dwBytes, _In_ OVERLAPPED *lpOvr,
                    _In_ HRESULT hRes);

private:
  LONG volatile nRundownLock;
  MX::CIoCompletionPortThreadPool::OnPacketCallback cQueuedTaskCallbackWP;
  LONG volatile nQueuedTasksCount;
  DWORD dwMaxCpuUsage;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_TASK_QUEUE_H
