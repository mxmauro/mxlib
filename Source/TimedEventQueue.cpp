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
#include "..\Include\TimedEventQueue.h"
#include "..\Include\WaitableObjects.h"

//-----------------------------------------------------------

#define STATE_Processed                               0x0001
#define STATE_Canceled                                0x0002

#define MINIMUM_DUE_TIME_CHANGE_THRESHOLD_MS              10

//-----------------------------------------------------------

static struct {
  LONG volatile nMutex;
  LONG volatile nRefCount;
  MX::CSystemTimedEventQueue *lpQueue;
} sSystemQueue = { 0, 0, NULL };

//-----------------------------------------------------------

namespace MX {

CTimedEventQueue::CTimedEventQueue() : TRefCounted<CBaseMemObj>()
{
  _InterlockedExchange(&nQueuedEventsTreeMutex, 0);
  _InterlockedExchange(&nRemovedTreeMutex, 0);
  _InterlockedExchange(&nThreadMutex, 0);
  RundownProt_Initialize(&nRundownLock);
#ifdef _DEBUG
  nQueuedEventsCounter = nRemovedTreeCounter = 0;
#endif //_DEBUG
  return;
}

CTimedEventQueue::~CTimedEventQueue()
{
  RundownProt_WaitForRelease(&nRundownLock);
  cWorkerThread.Stop();
  return;
}

HRESULT CTimedEventQueue::Add(_In_ CEvent *lpEvent, _In_ DWORD dwTimeoutMs)
{
  CAutoRundownProtection cAutoRundownProt(&nRundownLock);
  TAutoRefCounted<CEvent> cAutoRefEvent(lpEvent);
  ULARGE_INTEGER liCurrTime;

  if (cAutoRundownProt.IsAcquired() == FALSE)
    return MX_E_NotReady;
  if (lpEvent == NULL)
    return E_POINTER;
  //start worker thread
  if (cWorkerThread.IsRunning() == FALSE)
  {
    CFastLock cLock(&nThreadMutex);
    HRESULT hRes;

    if (cQueueChangedEv.Get() == NULL)
    {
      hRes = cQueueChangedEv.Create(TRUE, FALSE);
      if (FAILED(hRes))
        return hRes;
    }
    if (cWorkerThread.IsRunning() == FALSE)
    {
      if (cWorkerThread.Start(this, &CTimedEventQueue::ThreadProc, 0) == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  //get current time
  MxNtQuerySystemTime(&liCurrTime);
  //(re)insert into tree
  {
    CFastLock cLock1(&nQueuedEventsTreeMutex);
    CFastLock cLock2(&nRemovedTreeMutex);
    ULONGLONG nDueTime, nDiff;

    //calculate due time
    nDueTime = MX_100NS_TO_MILLISECONDS(liCurrTime.QuadPart) + (ULONGLONG)dwTimeoutMs;
    //add to queue
    if (lpEvent->GetTree() == &cQueuedEventsTree)
    {
      nDiff = (nDueTime > lpEvent->nDueTime) ? (nDueTime - lpEvent->nDueTime) : (lpEvent->nDueTime - nDueTime);
      if (nDueTime < MINIMUM_DUE_TIME_CHANGE_THRESHOLD_MS)
        return S_OK; //don't requeue if due time difference is minimal
    }
    lpEvent->nDueTime = nDueTime;
    lpEvent->SetState(CEvent::StateQueued);
    lpEvent->ResetCancelMark();
    if (lpEvent->GetTree() == &cQueuedEventsTree || lpEvent->GetTree() == &cRemovedTree)
    {
#ifdef _DEBUG
      if (lpEvent->GetTree() == &cQueuedEventsTree)
      {
        MX_ASSERT(nQueuedEventsCounter > 0);
        nQueuedEventsCounter--;
      }
      else
      {
        MX_ASSERT(nRemovedTreeCounter > 0);
        nRemovedTreeCounter--;
      }
#endif //_DEBUG
      //requeue
      lpEvent->RemoveNode();
      cQueuedEventsTree.Insert(lpEvent, TRUE);
#ifdef _DEBUG
      nQueuedEventsCounter++;
      MX_ASSERT(nQueuedEventsCounter == cQueuedEventsTree.GetCount());
#endif //_DEBUG
    }
    else
    {
      cQueuedEventsTree.Insert(lpEvent, TRUE);
      lpEvent->AddRef();
#ifdef _DEBUG
      nQueuedEventsCounter++;
      MX_ASSERT(nQueuedEventsCounter == cQueuedEventsTree.GetCount());
#endif //_DEBUG
    }
  }
  //done
  cQueueChangedEv.Set();
  return S_OK;
}

VOID CTimedEventQueue::Remove(_In_ CEvent *lpEvent, _In_opt_ BOOL bMarkAsCanceled)
{
  if (lpEvent != NULL)
  {
    CFastLock cLock1(&nQueuedEventsTreeMutex);
    CFastLock cLock2(&nRemovedTreeMutex);

    if (lpEvent->GetTree() == &cQueuedEventsTree)
    {
#ifdef _DEBUG
      MX_ASSERT(nQueuedEventsCounter > 0);
      nQueuedEventsCounter--;
#endif //_DEBUG
      if (bMarkAsCanceled != FALSE)
        lpEvent->MarkAsCanceled();
      lpEvent->RemoveNode();

#ifdef _DEBUG
      nRemovedTreeCounter++;
#endif //_DEBUG
      cRemovedTree.Insert(lpEvent, TRUE);
#ifdef _DEBUG
      MX_ASSERT(nRemovedTreeCounter == cRemovedTree.GetCount());
#endif //_DEBUG
      cQueueChangedEv.Set();
    }
  }
  return;
}

VOID CTimedEventQueue::RemoveAll()
{
  CFastLock cLock1(&nQueuedEventsTreeMutex);
  CFastLock cLock2(&nRemovedTreeMutex);
  CEvent *lpEvent;

  while ((lpEvent = cQueuedEventsTree.GetFirst()) != NULL)
  {
#ifdef _DEBUG
    MX_ASSERT(nQueuedEventsCounter > 0);
    nQueuedEventsCounter--;
#endif //_DEBUG
    lpEvent->MarkAsCanceled();
    lpEvent->RemoveNode();
#ifdef _DEBUG
    MX_ASSERT(nQueuedEventsCounter == cQueuedEventsTree.GetCount());
#endif //_DEBUG

#ifdef _DEBUG
    nRemovedTreeCounter++;
#endif //_DEBUG
    cRemovedTree.Insert(lpEvent, TRUE);
#ifdef _DEBUG
    MX_ASSERT(nRemovedTreeCounter == cRemovedTree.GetCount());
#endif //_DEBUG
  }
#ifdef _DEBUG
  MX_ASSERT(nQueuedEventsCounter == 0);
#endif //_DEBUG
  cQueueChangedEv.Set();
  return;
}

BOOL CTimedEventQueue::ChangeTimeout(_In_ CEvent *lpEvent, _In_ DWORD dwTimeoutMs)
{
  CFastLock cLock1(&nQueuedEventsTreeMutex);
  CFastLock cLock2(&nRemovedTreeMutex);
  ULARGE_INTEGER liCurrTime;
  ULONGLONG nDueTime, nDiff;

  if (lpEvent == NULL)
    return FALSE;
  if (lpEvent->GetTree() != &cQueuedEventsTree)
    return FALSE;
  //get current time
  MxNtQuerySystemTime(&liCurrTime);
  //calculate due time
  nDueTime = MX_100NS_TO_MILLISECONDS(liCurrTime.QuadPart) + (ULONGLONG)dwTimeoutMs;
  //re-add to queue
  nDiff = (nDueTime > lpEvent->nDueTime) ? (nDueTime - lpEvent->nDueTime) : (lpEvent->nDueTime - nDueTime);
  if (nDueTime < MINIMUM_DUE_TIME_CHANGE_THRESHOLD_MS)
    return TRUE; //don't requeue if due time difference is minimal
  lpEvent->nDueTime = nDueTime;
  //requeue
  lpEvent->RemoveNode();
  cQueuedEventsTree.Insert(lpEvent, TRUE);
  cQueueChangedEv.Set();
  return TRUE;
}

VOID CTimedEventQueue::ThreadProc(_In_ SIZE_T nParam)
{
  DWORD dwTimeoutMs, dwHitEv;
  HANDLE hEvent;

  UNREFERENCED_PARAMETER(nParam);

  hEvent = cQueueChangedEv.Get();
  do
  {
    //reset event
    cQueueChangedEv.Reset();
    //process timed out
    dwTimeoutMs = ProcessTimedOut();
    //process canceled events
    ProcessCanceled();
  }
  while (cWorkerThread.CheckForAbort(dwTimeoutMs, 1, &hEvent, &dwHitEv) == FALSE);
  //before leaving the thread, cancel pending
  RemoveAll();
  ProcessCanceled();
  return;
}

DWORD CTimedEventQueue::ProcessTimedOut()
{
  ULARGE_INTEGER liCurrTime;
  DWORD dwTimeoutMs;
  CEvent *lpEvent;
  ULONGLONG nDiff;

  //signal expired events
  MxNtQuerySystemTime(&liCurrTime);
  liCurrTime.QuadPart = MX_100NS_TO_MILLISECONDS(liCurrTime.QuadPart);
  dwTimeoutMs = INFINITE;
  do
  {
    {
      CFastLock cLock(&nQueuedEventsTreeMutex);

      lpEvent = cQueuedEventsTree.GetFirst();
      if (lpEvent != NULL)
      {
        if (lpEvent->nDueTime <= liCurrTime.QuadPart)
        {
#ifdef _DEBUG
          MX_ASSERT(nQueuedEventsCounter > 0);
          nQueuedEventsCounter--;
#endif //_DEBUG

          //got a timed-out item
          lpEvent->RemoveNode();
#ifdef _DEBUG
          MX_ASSERT(nQueuedEventsCounter == cQueuedEventsTree.GetCount());
#endif //_DEBUG
          lpEvent->SetState(CEvent::StateProcessing);
        }
        else
        {
          nDiff = lpEvent->nDueTime - liCurrTime.QuadPart;
          dwTimeoutMs = (nDiff > 0xFFFFFFFFui64) ? 0xFFFFFFFFUL : (DWORD)nDiff;
          lpEvent = NULL;
        }
      }
#ifdef _DEBUG
      else
      {
        MX_ASSERT(nQueuedEventsCounter == 0);
      }
#endif //_DEBUG
    }
    //fire event
    if (lpEvent != NULL)
    {
      lpEvent->InvokeCallback();
      lpEvent->SetStateIf(CEvent::StateProcessed, CEvent::StateProcessing);
      lpEvent->Release();
    }
  }
  while (lpEvent != NULL);
  return dwTimeoutMs;
}

VOID CTimedEventQueue::ProcessCanceled()
{
  CEvent *lpEvent;

  do
  {
    {
      CFastLock cLock(&nRemovedTreeMutex);

      lpEvent = cRemovedTree.GetFirst();
      if (lpEvent != NULL)
      {
#ifdef _DEBUG
        MX_ASSERT(nRemovedTreeCounter > 0);
        nRemovedTreeCounter--;
#endif //_DEBUG

        lpEvent->SetState(CEvent::StateProcessing);
        lpEvent->RemoveNode();
      }
#ifdef _DEBUG
      else
      {
        MX_ASSERT(nRemovedTreeCounter == 0);
      }
#endif //_DEBUG
    }
    if (lpEvent != NULL)
    {
      lpEvent->InvokeCallback();
      lpEvent->SetStateIf(CEvent::StateProcessed, CEvent::StateProcessing);
      lpEvent->Release();
    }
  }
  while (lpEvent != NULL);
  return;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CTimedEventQueue::CEvent::CEvent(_In_ OnNotifyCallback cCallback, _In_opt_ LPVOID lpUserData) :
                          TEventNotifyBase<CEvent>(cCallback, lpUserData), TRedBlackTreeNode<CEvent, ULONGLONG>()
{
  nDueTime = 0ui64;
  return;
}

CTimedEventQueue::CEvent::~CEvent()
{
  return;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CSystemTimedEventQueue::CSystemTimedEventQueue() : CTimedEventQueue()
{
  return;
}

CSystemTimedEventQueue::~CSystemTimedEventQueue()
{
  return;
}

CSystemTimedEventQueue* CSystemTimedEventQueue::Get()
{
  CFastLock cLock(&(sSystemQueue.nMutex));

  if (sSystemQueue.lpQueue == NULL)
  {
    sSystemQueue.lpQueue = MX_DEBUG_NEW CSystemTimedEventQueue();
    if (sSystemQueue.lpQueue != NULL)
    {
      MX_ASSERT(sSystemQueue.nRefCount == 0);
      _InterlockedExchange(&(sSystemQueue.nRefCount), 1);
    }
  }
  else
  {
    MX_ASSERT(sSystemQueue.nRefCount > 0);
    _InterlockedIncrement(&(sSystemQueue.nRefCount));
  }
  return sSystemQueue.lpQueue;
}

VOID CSystemTimedEventQueue::AddRef()
{
  CFastLock cLock(&(sSystemQueue.nMutex));

  MX_ASSERT(sSystemQueue.lpQueue == this);
  MX_ASSERT(sSystemQueue.nRefCount > 0);
  _InterlockedIncrement(&(sSystemQueue.nRefCount));
  return;
}

VOID CSystemTimedEventQueue::Release()
{
  CSystemTimedEventQueue *lpToDelete = NULL;

  {
    CFastLock cLock(&(sSystemQueue.nMutex));

    MX_ASSERT(sSystemQueue.lpQueue == this);
    MX_ASSERT(sSystemQueue.nRefCount > 0);
    if (_InterlockedDecrement(&(sSystemQueue.nRefCount)) == 0)
    {
      MX_ASSERT(sSystemQueue.lpQueue != NULL);
      lpToDelete = sSystemQueue.lpQueue;
      sSystemQueue.lpQueue = NULL;
    }
  }
  if (lpToDelete != NULL)
    lpToDelete->CTimedEventQueue::Release();
  return;
}

} //namespace MX
