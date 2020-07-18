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
#include "..\Include\TimedEvent.h"
#include "..\Include\WaitableObjects.h"
#include "..\Include\Threads.h"
#include "..\Include\RefCounted.h"
#include "..\Include\AutoPtr.h"
#include "..\Include\ArrayList.h"
#include "..\Include\RedBlackTree.h"
#include "..\Include\LinkedList.h"
#include "..\Include\Finalizer.h"

#define _FLAG_Running                                 0x0001
#define _FLAG_Canceled                                0x0002
#define _FLAG_OneShot                                 0x0004
#define _FLAG_CanceledInCallback                      0x0008

#define MINIMUM_DUE_TIME_CHANGE_THRESHOLD_MS              10

#define TIMERHANDLER_FINALIZER_PRIORITY                10000
#define MAX_TIMERS_IN_FREE_LIST                          128

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CTimerHandler : public TRefCounted<CThread>, public CNonCopyableObj
{
public:
  class CTimer : public virtual CBaseMemObj, public CNonCopyableObj
  {
  public:
    CTimer(_In_ DWORD dwTimeoutMs, _In_ MX::TimedEvent::OnTimeoutCallback cCallback, _In_opt_ LPVOID lpUserData,
           _In_ BOOL bOneShot) : CBaseMemObj(), CNonCopyableObj()
      {
      Setup(dwTimeoutMs, cCallback, lpUserData, bOneShot);
      return;
      };

    VOID Setup(_In_ DWORD _dwTimeoutMs, _In_ MX::TimedEvent::OnTimeoutCallback _cCallback,
               _In_opt_ LPVOID _lpUserData, _In_ BOOL bOneShot)
      {
      nId = 0;
      dwTimeoutMs = _dwTimeoutMs;
      nDueTime = 0ui64;
      cCallback = _cCallback;
      lpUserData = _lpUserData;
      _InterlockedExchange(&nFlags, (bOneShot != FALSE) ? _FLAG_OneShot : 0);
      lpNextInFreeList = NULL;
      return;
      };

    __inline VOID CalculateDueTime(_In_opt_ PULARGE_INTEGER lpuliCurrTime)
      {
      ULARGE_INTEGER uliCurrTime;

      if (lpuliCurrTime == NULL)
      {
        ::MxNtQuerySystemTime(&uliCurrTime);
        lpuliCurrTime = &uliCurrTime;
      }
      nDueTime = MX_100NS_TO_MILLISECONDS(lpuliCurrTime->QuadPart) + (ULONGLONG)dwTimeoutMs;
      return;
      };

    __inline VOID WaitWhileRunning()
      {
      while ((__InterlockedRead(&nFlags) & _FLAG_Running) != 0)
        ::MxSleep(5);
      return;
      };

    __inline BOOL InvokeCallback()
      {
      BOOL bCancel = FALSE;

      cCallback(nId, lpUserData, (((__InterlockedRead(&nFlags) & _FLAG_OneShot) != 0) ? NULL : &bCancel));
      return bCancel;
      };

    __inline BOOL SetAsRunningIfNotCanceled()
      {
      LONG initVal, newVal;

      newVal = __InterlockedRead(&nFlags);
      do
      {
        initVal = newVal;
        if ((initVal & _FLAG_Canceled) != 0)
          return FALSE; //timer was canceled
        newVal = _InterlockedCompareExchange(&nFlags, initVal | _FLAG_Running, initVal);
      }
      while (newVal != initVal);
      return TRUE;
      };

    __inline BOOL SetAsCanceled(_In_ BOOL bInsideCallback)
      {
      LONG initVal;

      initVal = _InterlockedOr(&nFlags, ((bInsideCallback == FALSE) ? _FLAG_Canceled
                                                                    : (_FLAG_Canceled | _FLAG_CanceledInCallback)));
      return ((initVal & _FLAG_Running) != 0) ? TRUE : FALSE;
      };

    static int InsertCompareFunc(_In_ LPVOID lpContext, _In_ CRedBlackTreeNode *lpNode1,
                                 _In_ CRedBlackTreeNode *lpNode2)
      {
      CTimer *lpTimer1 = CONTAINING_RECORD(lpNode1, CTimer, cTreeNode);
      CTimer *lpTimer2 = CONTAINING_RECORD(lpNode2, CTimer, cTreeNode);

      if (lpTimer1->nDueTime < lpTimer2->nDueTime)
        return -1;
      if (lpTimer1->nDueTime > lpTimer2->nDueTime)
        return 1;
      return 0;
      };

  public:
    LONG nId;
    CRedBlackTreeNode cTreeNode;
    DWORD dwTimeoutMs;
    ULONGLONG nDueTime;
    MX::TimedEvent::OnTimeoutCallback cCallback;
    LPVOID lpUserData;
    LONG volatile nFlags;
    CTimer *lpNextInFreeList;
  };

public:
  CTimerHandler();
  ~CTimerHandler();

  static CTimerHandler* Get();
  static VOID Shutdown();

  BOOL Initialize();

  HRESULT AddTimer(_Out_ LONG volatile *lpnTimerId, _In_ MX::TimedEvent::OnTimeoutCallback cCallback,
                   _In_ DWORD dwTimeoutMs, _In_opt_ LPVOID lpUserData, _In_ BOOL bOneShot);
  VOID RemoveTimer(_Inout_ LONG volatile *lpnTimerId);

private:
  VOID ThreadProc();

  DWORD ProcessQueue();

  CTimer* AllocTimer(_In_ MX::TimedEvent::OnTimeoutCallback cCallback, _In_ DWORD dwTimeoutMs,
                     _In_opt_ LPVOID lpUserData, _In_ BOOL bOneShot);
  VOID FreeTimer(_In_ CTimer *lpTimer);

  static int InsertByIdCompareFunc(_In_ LPVOID lpContext, _In_ CTimer **lplpElem1, _In_ CTimer **lplpElem2);
  static int SearchByIdCompareFunc(_In_ LPVOID lpContext, _In_ PLONG lpKey, _In_ CTimer **lplpElem);

private:
  LONG volatile nRundownLock;
  LONG volatile nNextTimerId;
  LONG volatile nThreadMutex;
  struct {
    LONG volatile nMutex;
    CWindowsEvent cChangedEvent;
    TArrayList<CTimer*> cSortedByIdList;
    CRedBlackTree cTree;
  } sQueue;
  struct {
    LONG volatile nMutex;
    CTimer *lpFirst;
    int nListCount;
  } sFreeTimers;
};

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

static MX::RWLOCK sTimerHandlerRwMutex = MX_RWLOCK_INIT;
static MX::Internals::CTimerHandler *lpTimerHandler = NULL;

//-----------------------------------------------------------

namespace MX {

namespace TimedEvent {

HRESULT SetTimeout(_Out_ LONG volatile *lpnTimerId, _In_ DWORD dwTimeoutMs, _In_ OnTimeoutCallback cCallback,
                   _In_opt_ LPVOID lpUserData)
{
  TAutoRefCounted<Internals::CTimerHandler> cHandler;

  cHandler.Attach(Internals::CTimerHandler::Get());
  if (!cHandler)
  {
    if (lpnTimerId != NULL)
      _InterlockedExchange(lpnTimerId, 0);
    return E_OUTOFMEMORY;
  }
  return cHandler->AddTimer(lpnTimerId, cCallback, dwTimeoutMs, lpUserData, TRUE);
}

HRESULT SetInterval(_Out_ LONG volatile *lpnTimerId, _In_ DWORD dwTimeoutMs, _In_ OnTimeoutCallback cCallback,
                    _In_opt_ LPVOID lpUserData)
{
  TAutoRefCounted<Internals::CTimerHandler> cHandler;

  cHandler.Attach(Internals::CTimerHandler::Get());
  if (!cHandler)
  {
    if (lpnTimerId != NULL)
      _InterlockedExchange(lpnTimerId, 0);
    return E_OUTOFMEMORY;
  }
  return cHandler->AddTimer(lpnTimerId, cCallback, dwTimeoutMs, lpUserData, FALSE);
}

VOID Clear(_Inout_ LONG volatile *lpnTimerId)
{
  TAutoRefCounted<Internals::CTimerHandler> cHandler;

  cHandler.Attach(Internals::CTimerHandler::Get());
  if (cHandler)
  {
    cHandler->RemoveTimer(lpnTimerId);
  }
  else if (lpnTimerId != NULL)
  {
    _InterlockedExchange(lpnTimerId, 0);
  }
  return;
}

} //namespace TimedEvent

} //namespace MX

//-----------------------------------------------------------

namespace MX {

namespace Internals {

CTimerHandler::CTimerHandler() : TRefCounted<CThread>(), CNonCopyableObj()
{
  RundownProt_Initialize(&nRundownLock);
  _InterlockedExchange(&nNextTimerId, 0);
  _InterlockedExchange(&nThreadMutex, 0);
  _InterlockedExchange(&(sQueue.nMutex), 0);
  MxMemSet(&sFreeTimers, 0, sizeof(sFreeTimers));
  return;
}

CTimerHandler::~CTimerHandler()
{
  CRedBlackTreeNode *lpNode;
  CTimer *lpTimer;

  RundownProt_WaitForRelease(&nRundownLock);

  Stop();

  //remove items in list
  sQueue.cSortedByIdList.RemoveAllElements();
  while ((lpNode = sQueue.cTree.GetFirst()) != NULL)
  {
    lpTimer = CONTAINING_RECORD(lpNode, CTimer, cTreeNode);
    lpNode->Remove();
    delete lpTimer;
  }

  //free list of timers
  while ((lpTimer = sFreeTimers.lpFirst) != NULL)
  {
    sFreeTimers.lpFirst = lpTimer->lpNextInFreeList;
    delete lpTimer;
  }
  return;
}

CTimerHandler* CTimerHandler::Get()
{
  CAutoSlimRWLShared cLock(&sTimerHandlerRwMutex);

  if (lpTimerHandler == NULL)
  {
    cLock.UpgradeToExclusive();

    if (lpTimerHandler == NULL)
    {
      CTimerHandler *_lpTimerHandler;

      _lpTimerHandler = MX_DEBUG_NEW CTimerHandler();
      if (_lpTimerHandler == NULL)
        return NULL;
      if (_lpTimerHandler->Initialize() == FALSE)
      {
        _lpTimerHandler->Release();
        return NULL;
      }
      //register shutdown callback
      if (FAILED(MX::RegisterFinalizer(&CTimerHandler::Shutdown, TIMERHANDLER_FINALIZER_PRIORITY)))
      {
        _lpTimerHandler->Release();
        return NULL;
      }
      lpTimerHandler = _lpTimerHandler;
    }
  }
  lpTimerHandler->AddRef();
  return lpTimerHandler;
}

VOID CTimerHandler::Shutdown()
{
  CAutoSlimRWLExclusive cLock(&sTimerHandlerRwMutex);

  MX_RELEASE(lpTimerHandler);
  return;
}

BOOL CTimerHandler::Initialize()
{
  HRESULT hRes;

  hRes = sQueue.cChangedEvent.Create(TRUE, FALSE);
  if (FAILED(hRes))
    return FALSE;
  if (Start() == FALSE)
  {
    sQueue.cChangedEvent.Close();
    return FALSE;
  }
  return TRUE;
}

HRESULT CTimerHandler::AddTimer(_Out_ LONG volatile *lpnTimerId, _In_ MX::TimedEvent::OnTimeoutCallback cCallback,
                                _In_ DWORD dwTimeoutMs, _In_opt_ LPVOID lpUserData, _In_ BOOL bOneShot)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  TAutoDeletePtr<CTimer> cNewTimer;

  if (lpnTimerId == NULL)
    return E_POINTER;
  if (cAutoRundownProt.IsAcquired() == FALSE)
  {
    _InterlockedExchange(lpnTimerId, 0);
    return MX_E_NotReady;
  }
  if (!cCallback)
  {
    _InterlockedExchange(lpnTimerId, 0);
    return E_POINTER;
  }

  //create new timer
  cNewTimer.Attach(AllocTimer(cCallback, dwTimeoutMs, lpUserData, bOneShot));
  if (!cNewTimer)
  {
    _InterlockedExchange(lpnTimerId, 0);
    return E_OUTOFMEMORY;
  }

  //assign timer id
  do
  {
    cNewTimer->nId = _InterlockedIncrement(&nNextTimerId);
  }
  while (cNewTimer->nId == 0);

  //calculate due time
  cNewTimer->CalculateDueTime(NULL);

  //insert into tree
  {
    CFastLock cQueueLock(&(sQueue.nMutex));

    if (sQueue.cSortedByIdList.SortedInsert(cNewTimer.Get(), &CTimerHandler::InsertByIdCompareFunc, NULL,
                                            TRUE) == FALSE)
    {
      _InterlockedExchange(lpnTimerId, 0);
      return E_OUTOFMEMORY;
    }

    _InterlockedExchange(lpnTimerId, cNewTimer->nId);
    sQueue.cTree.Insert(&(cNewTimer->cTreeNode), &CTimer::InsertCompareFunc, TRUE);
    cNewTimer.Detach();
  }
  sQueue.cChangedEvent.Set();

  //done
  return S_OK;
}

VOID CTimerHandler::RemoveTimer(_Inout_ LONG volatile *lpnTimerId)
{
  if (lpnTimerId != NULL)
  {
    CTimer *lpTimer = NULL;
    BOOL bInsideCallback = (::GetCurrentThreadId() == GetThreadId()) ? TRUE : FALSE;
    BOOL bIsRunning = FALSE;

    LONG nTimerId = _InterlockedExchange(lpnTimerId, 0);
    if (nTimerId != 0)
    {
      CFastLock cQueueLock(&(sQueue.nMutex));
      SIZE_T nIndex;

      nIndex = sQueue.cSortedByIdList.BinarySearch(&nTimerId, &CTimerHandler::SearchByIdCompareFunc, NULL);
      if (nIndex != (SIZE_T)-1)
      {
        lpTimer = sQueue.cSortedByIdList.GetElementAt(nIndex);

        bIsRunning = lpTimer->SetAsCanceled(bInsideCallback);

        sQueue.cSortedByIdList.RemoveElementAt(nIndex);
        lpTimer->cTreeNode.Remove();
      }
    }

    if (lpTimer != NULL)
    {
      sQueue.cChangedEvent.Set();
      if (bIsRunning == FALSE)
      {
        FreeTimer(lpTimer);
      }
      else if (bInsideCallback == FALSE)
      {
        lpTimer->WaitWhileRunning();
        FreeTimer(lpTimer);
      }
    }
  }

  //done
  return;
}

VOID CTimerHandler::ThreadProc()
{
  DWORD dwTimeoutMs, dwHitEv;
  HANDLE hEvent;

  hEvent = sQueue.cChangedEvent.Get();

  dwTimeoutMs = 0;
  while (CheckForAbort(dwTimeoutMs, 1, &hEvent, &dwHitEv) == FALSE)
  {
    //reset event
    sQueue.cChangedEvent.Reset();

    //process queue
    dwTimeoutMs = ProcessQueue();
  }
  //done
  return;
}

DWORD CTimerHandler::ProcessQueue()
{
  DWORD dwTimeoutMs;
  CTimer *lpTimer;

  //signal expired events
  dwTimeoutMs = INFINITE;
  do
  {
    ULARGE_INTEGER uliCurrTime;

    ::MxNtQuerySystemTime(&uliCurrTime);

    {
      CFastLock cLock(&(sQueue.nMutex));
      CRedBlackTreeNode *lpNode;

      lpNode = sQueue.cTree.GetFirst();
check_timer:
      lpTimer = (lpNode != NULL) ? CONTAINING_RECORD(lpNode, CTimer, cTreeNode) : NULL;

      if (lpTimer != NULL)
      {
        ULONGLONG nNow = MX_100NS_TO_MILLISECONDS(uliCurrTime.QuadPart);

        if (lpTimer->nDueTime <= nNow)
        {
          //got a timed-out item
          if (lpTimer->SetAsRunningIfNotCanceled() == FALSE)
          {
            //the timer was canceled so jump to next
            lpNode = lpTimer->cTreeNode.GetNext();
            goto check_timer;
          }
        }
        else
        {
          ULONGLONG nDiff = lpTimer->nDueTime - nNow;
          dwTimeoutMs = (nDiff > 0xFFFFFFFFui64) ? 0xFFFFFFFEUL : (DWORD)nDiff;
          lpTimer = NULL;
        }
      }
    }

    //fire event
    if (lpTimer != NULL)
    {
      BOOL bCancel = lpTimer->InvokeCallback();

      {
        CFastLock cLock(&(sQueue.nMutex));
        LONG nFlags = __InterlockedRead(&(lpTimer->nFlags));

        if ((nFlags & _FLAG_Canceled) != 0)
        {
          //timer was canceled so just let's keep the cancel code to continue it's task unless it was removed
          //from the callback
          if ((_InterlockedAnd(&(lpTimer->nFlags), ~_FLAG_Running) & _FLAG_CanceledInCallback) != 0)
          {
            FreeTimer(lpTimer);
          }
        }
        else
        {
          lpTimer->cTreeNode.Remove();
          if (bCancel != FALSE || (nFlags & _FLAG_OneShot) != 0)
          {
            SIZE_T nIndex;

            nIndex = sQueue.cSortedByIdList.BinarySearch(&(lpTimer->nId), &CTimerHandler::SearchByIdCompareFunc, NULL);
            MX_ASSERT(nIndex != (SIZE_T)-1);
            if (nIndex != (SIZE_T)-1)
            {
              sQueue.cSortedByIdList.RemoveElementAt(nIndex);
            }

            FreeTimer(lpTimer);
          }
          else
          {
            lpTimer->CalculateDueTime(&uliCurrTime);
            sQueue.cTree.Insert(&(lpTimer->cTreeNode), &CTimer::InsertCompareFunc, TRUE);

            _InterlockedAnd(&(lpTimer->nFlags), ~_FLAG_Running);
          }
        }
      }
    }
  }
  while (lpTimer != NULL);

  //done
  return dwTimeoutMs;
}

CTimerHandler::CTimer* CTimerHandler::AllocTimer(_In_ MX::TimedEvent::OnTimeoutCallback cCallback,
                                                 _In_ DWORD dwTimeoutMs, _In_opt_ LPVOID lpUserData, _In_ BOOL bOneShot)
{
  CFastLock cLock(&(sFreeTimers.nMutex));

  if (sFreeTimers.lpFirst != NULL)
  {
    CTimer *lpTimer;

    MX_ASSERT(sFreeTimers.nListCount > 0);
    (sFreeTimers.nListCount)--;

    lpTimer = sFreeTimers.lpFirst;
    sFreeTimers.lpFirst = lpTimer->lpNextInFreeList;

    lpTimer->Setup(dwTimeoutMs, cCallback, lpUserData, bOneShot);
    return lpTimer;
  }
  return MX_DEBUG_NEW CTimer(dwTimeoutMs, cCallback, lpUserData, bOneShot);
}

VOID CTimerHandler::FreeTimer(_In_ CTimer *lpTimer)
{
  CFastLock cLock(&(sFreeTimers.nMutex));

  if (sFreeTimers.nListCount < MAX_TIMERS_IN_FREE_LIST)
  {
    lpTimer->lpNextInFreeList = sFreeTimers.lpFirst;
    sFreeTimers.lpFirst = lpTimer;
    (sFreeTimers.nListCount)++;
  }
  else
  {
    delete lpTimer;
  }
  return;
}

int CTimerHandler::InsertByIdCompareFunc(_In_ LPVOID lpContext, _In_ CTimer **lplpElem1, _In_ CTimer **lplpElem2)
{
  if ((*lplpElem1)->nId < (*lplpElem2)->nId)
    return -1;
  if ((*lplpElem1)->nId > (*lplpElem2)->nId)
    return 1;
  return 0;
}

int CTimerHandler::SearchByIdCompareFunc(_In_ LPVOID lpContext, _In_ PLONG lpKey, _In_ CTimer **lplpElem)
{
  if ((*lpKey) < (*lplpElem)->nId)
    return -1;
  if ((*lpKey) > (*lplpElem)->nId)
    return 1;
  return 0;
}

} //namespace Internals

} //namespace MX
