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
#ifndef _MX_TIMEDEVENTQUEUE_H
#define _MX_TIMEDEVENTQUEUE_H

#include "Defines.h"
#include "Callbacks.h"
#include "Threads.h"
#include "EventNotifyBase.h"
#include "RedBlackTree.h"
#include "LinkedList.h"

//-----------------------------------------------------------

namespace MX {

class CTimedEventQueue : public TRefCounted<CBaseMemObj>
{
  MX_DISABLE_COPY_CONSTRUCTOR(CTimedEventQueue);
public:
  class CEvent : public TEventNotifyBase<CEvent>, private TRedBlackTreeNode<CEvent, ULONGLONG>
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CEvent);
  public:
    CEvent(_In_ OnNotifyCallback cCallback, _In_opt_ LPVOID lpUserData=NULL);
    ~CEvent();

  private:
    friend class CTimedEventQueue;

    ULONGLONG GetNodeKey()
      {
      return nDueTime;
      };

  private:
    ULONGLONG nDueTime;
  };

public:
  CTimedEventQueue();
  ~CTimedEventQueue();

  HRESULT Add(_In_ CEvent *lpEvent, _In_ DWORD dwTimeoutMs);
  VOID Remove(_In_ CEvent *lpEvent, _In_opt_ BOOL bMarkAsCanceled=TRUE);
  VOID RemoveAll();

  BOOL ChangeTimeout(_In_ CEvent *lpEvent, _In_ DWORD dwTimeoutMs);

private:
  TClassWorkerThread<CTimedEventQueue> cWorkerThread;

  VOID ThreadProc(_In_ SIZE_T nParam);

  DWORD ProcessTimedOut();
  VOID ProcessCanceled();

private:
  LONG volatile nRundownLock;
  LONG volatile nThreadMutex;
  CWindowsEvent cQueueChangedEv;
  //----
  LONG volatile nQueuedEventsTreeMutex;
  TRedBlackTree<CEvent, ULONGLONG> cQueuedEventsTree;
  LONG volatile nRemovedTreeMutex;
  TRedBlackTree<CEvent, ULONGLONG> cRemovedTree;
};

//-----------------------------------------------------------

class CSystemTimedEventQueue : public CTimedEventQueue
{
  MX_DISABLE_COPY_CONSTRUCTOR(CSystemTimedEventQueue);
protected:
  CSystemTimedEventQueue();
  ~CSystemTimedEventQueue();

public:
  static CSystemTimedEventQueue* Get();

  VOID AddRef();
  VOID Release();
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_TIMEDEVENTQUEUE_H
