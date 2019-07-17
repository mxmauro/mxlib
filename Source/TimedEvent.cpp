/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2019. All rights reserved.
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

#define MINIMUM_DUE_TIME_CHANGE_THRESHOLD_MS              10

#define TIMERHANDLER_FINALIZER_PRIORITY                10000
#define MAX_TIMERS_IN_FREE_LIST                          128

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CTimerHandler : public TRefCounted<CThread>
{
  MX_DISABLE_COPY_CONSTRUCTOR(CTimerHandler);
public:
  class CTimer : public TRedBlackTreeNode<CTimer, ULONGLONG>
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CTimer);
  public:
    CTimer(_In_ DWORD dwTimeoutMs, _In_ MX::TimedEvent::OnTimeoutCallback cCallback, _In_ LPVOID lpUserData,
           _In_ BOOL bOneShot) : TRedBlackTreeNode<CTimer, ULONGLONG>()
      {
      Setup(dwTimeoutMs, cCallback, lpUserData, bOneShot);
      return;
      };

    VOID Setup(_In_ DWORD _dwTimeoutMs, _In_ MX::TimedEvent::OnTimeoutCallback _cCallback, _In_ LPVOID _lpUserData,
               _In_ BOOL bOneShot)
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

    __inline ULONGLONG GetNodeKey() const
      {
      return nDueTime;
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
        _YieldProcessor();
      return;
      };

    __inline VOID InvokeCallback()
      {
      cCallback(nId, lpUserData);
      return;
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

  public:
    LONG nId;
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

  HRESULT AddTimer(_Out_ LONG volatile *lpnTimerId, _In_ DWORD dwTimeoutMs,
                   _In_ MX::TimedEvent::OnTimeoutCallback cCallback, _In_ LPVOID lpUserData, _In_ BOOL bOneShot);
  VOID RemoveTimer(_Out_ LONG volatile *lpnTimerId);

private:
  VOID ThreadProc();

  DWORD ProcessQueue();

  CTimer* AllocTimer(_In_ DWORD dwTimeoutMs, _In_ MX::TimedEvent::OnTimeoutCallback cCallback, _In_ LPVOID lpUserData,
                     _In_ BOOL bOneShot);
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
    TRedBlackTree<CTimer, ULONGLONG> cTree;
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

static LONG volatile nTimerHandlerRwMutex = 0;
static MX::Internals::CTimerHandler *lpTimerHandler = NULL;

//-----------------------------------------------------------

namespace MX {

namespace TimedEvent {

HRESULT SetTimeout(_Out_ LONG volatile *lpnTimerId, _In_ DWORD dwTimeoutMs, _In_ OnTimeoutCallback cCallback,
                   _In_ LPVOID lpUserData)
{
  TAutoRefCounted<Internals::CTimerHandler> cHandler;

  cHandler.Attach(Internals::CTimerHandler::Get());
  if (!cHandler)
  {
    if (lpnTimerId != NULL)
      _InterlockedExchange(lpnTimerId, 0);
    return E_OUTOFMEMORY;
  }
  return cHandler->AddTimer(lpnTimerId, dwTimeoutMs, cCallback, lpUserData, TRUE);
}

HRESULT SetInterval(_Out_ LONG volatile *lpnTimerId, _In_ DWORD dwTimeoutMs, _In_ OnTimeoutCallback cCallback,
                    _In_ LPVOID lpUserData)
{
  TAutoRefCounted<Internals::CTimerHandler> cHandler;

  cHandler.Attach(Internals::CTimerHandler::Get());
  if (!cHandler)
  {
    if (lpnTimerId != NULL)
      _InterlockedExchange(lpnTimerId, 0);
    return E_OUTOFMEMORY;
  }
  return cHandler->AddTimer(lpnTimerId, dwTimeoutMs, cCallback, lpUserData, FALSE);
}

VOID Clear(_Inout_ LONG volatile *lpnTimerId)
{
  TAutoRefCounted<Internals::CTimerHandler> cHandler;

  cHandler.Attach(Internals::CTimerHandler::Get());
  if (cHandler)
    cHandler->RemoveTimer(lpnTimerId);
  else if (lpnTimerId != NULL)
    _InterlockedExchange(lpnTimerId, 0);
  return;
}

} //namespace TimedEvent

} //namespace MX

//-----------------------------------------------------------

namespace MX {

namespace Internals {

CTimerHandler::CTimerHandler() : TRefCounted<CThread>()
{
  RundownProt_Initialize(&nRundownLock);
  _InterlockedExchange(&nNextTimerId, 0);
  _InterlockedExchange(&nThreadMutex, 0);
  _InterlockedExchange(&(sQueue.nMutex), 0);
  MemSet(&sFreeTimers, 0, sizeof(sFreeTimers));
  return;
}

CTimerHandler::~CTimerHandler()
{
  CTimer *lpTimer;

  RundownProt_WaitForRelease(&nRundownLock);

  Stop();

  //remove items in list
  sQueue.cSortedByIdList.RemoveAllElements();
  while ((lpTimer = sQueue.cTree.GetFirst()) != NULL)
  {
    lpTimer->RemoveNode();
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
  CAutoSlimRWLShared cLock(&nTimerHandlerRwMutex);

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
  CAutoSlimRWLExclusive cLock(&nTimerHandlerRwMutex);

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

HRESULT CTimerHandler::AddTimer(_Out_ LONG volatile *lpnTimerId, _In_ DWORD dwTimeoutMs,
                                _In_ MX::TimedEvent::OnTimeoutCallback cCallback, _In_ LPVOID lpUserData,
                                _In_ BOOL bOneShot)
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
  cNewTimer.Attach(AllocTimer(dwTimeoutMs, cCallback, lpUserData, bOneShot));
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
    sQueue.cTree.Insert(cNewTimer.Detach(), TRUE);
  }
  sQueue.cChangedEvent.Set();

  //done
  return S_OK;
}

VOID CTimerHandler::RemoveTimer(_Out_ LONG volatile *lpnTimerId)
{
  if (lpnTimerId != NULL)
  {
    CTimer *lpTimer = NULL;

    LONG nTimerId = _InterlockedExchange(lpnTimerId, 0);
    if (nTimerId != 0)
    {
      CFastLock cQueueLock(&(sQueue.nMutex));
      SIZE_T nIndex;

      nIndex = sQueue.cSortedByIdList.BinarySearch(&nTimerId, &CTimerHandler::SearchByIdCompareFunc, NULL);
      if (nIndex != (SIZE_T)-1)
      {
        lpTimer = sQueue.cSortedByIdList.GetElementAt(nIndex);

        _InterlockedOr(&(lpTimer->nFlags), _FLAG_Canceled);

        sQueue.cSortedByIdList.RemoveElementAt(nIndex);
        lpTimer->RemoveNode();
      }
    }

    if (lpTimer != NULL)
    {
      sQueue.cChangedEvent.Set();

      lpTimer->WaitWhileRunning();
      FreeTimer(lpTimer);
    }
  }
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
  ULARGE_INTEGER uliCurrTime;
  DWORD dwTimeoutMs;
  CTimer *lpTimer;
  ULONGLONG nDiff;

  //signal expired events
  MxNtQuerySystemTime(&uliCurrTime);
  uliCurrTime.QuadPart = MX_100NS_TO_MILLISECONDS(uliCurrTime.QuadPart);

  dwTimeoutMs = INFINITE;
  do
  {
    {
      CFastLock cLock(&(sQueue.nMutex));

      lpTimer = sQueue.cTree.GetFirst();
check_timer:
      if (lpTimer != NULL)
      {
        if (lpTimer->nDueTime <= uliCurrTime.QuadPart)
        {
          //got a timed-out item
          if (lpTimer->SetAsRunningIfNotCanceled() == FALSE)
          {
            //the timer was canceled so jump to next
            lpTimer = lpTimer->GetNextEntry();
            goto check_timer;
          }
        }
        else
        {
          nDiff = lpTimer->nDueTime - uliCurrTime.QuadPart;
          dwTimeoutMs = (nDiff > 0xFFFFFFFFui64) ? 0xFFFFFFFEUL : (DWORD)nDiff;
          lpTimer = NULL;
        }
      }
    }

    //fire event
    if (lpTimer != NULL)
    {
      lpTimer->InvokeCallback();

      {
        CFastLock cLock(&(sQueue.nMutex));
        LONG nFlags = __InterlockedRead(&(lpTimer->nFlags));

        if ((nFlags & _FLAG_Canceled) != 0)
        {
          //timer was canceled so just let's keep the cancel code to continue it's task
          _InterlockedAnd(&(lpTimer->nFlags), ~_FLAG_Running);
        }
        else
        {
          lpTimer->RemoveNode();
          if ((nFlags & _FLAG_OneShot) != 0)
          {
            FreeTimer(lpTimer);
          }
          else
          {
            lpTimer->CalculateDueTime(&uliCurrTime);
            sQueue.cTree.Insert(lpTimer, TRUE);

            _InterlockedAnd(&(lpTimer->nFlags), ~_FLAG_Running);
          }
        }
      }
    }
  }
  while (lpTimer != NULL);
  return dwTimeoutMs;
}

CTimerHandler::CTimer* CTimerHandler::AllocTimer(_In_ DWORD dwTimeoutMs,
                                                 _In_ MX::TimedEvent::OnTimeoutCallback cCallback,
                                                 _In_ LPVOID lpUserData, _In_ BOOL bOneShot)
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
