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
#ifndef _MX_EVENTNOTIFYBASE_H
#define _MX_EVENTNOTIFYBASE_H

#include "Defines.h"
#include "Callbacks.h"
#include "ArrayList.h"
#include "RefCounted.h"

//-----------------------------------------------------------

namespace MX {

template<class T>
class TEventNotifyBase : public virtual TRefCounted<CBaseMemObj>
{
  MX_DISABLE_COPY_CONSTRUCTOR(TEventNotifyBase<T>);
public:
  typedef TEventNotifyBase<T> CEventBase;

  typedef Callback<VOID (__in T *lpEvent)> OnNotifyCallback;

  typedef enum {
    StateInvalidArg=-1,
    StateDetached=0,
    StateQueued,
    StateProcessing,
    StateProcessed
  } eState;

public:
  TEventNotifyBase(__in OnNotifyCallback _cCallback, __in_opt LPVOID _lpUserData=NULL) : TRefCounted<CBaseMemObj>()
    {
    cCallback = _cCallback;
    lpUserData = _lpUserData;
    _InterlockedExchange(&nState, (LONG)StateDetached);
    return;
    };

  eState GetState() const
    {
    return (eState)(__InterlockedRead(&((const_cast<CEventBase*>(this))->nState)) & 0x0F);
    };

  T* GetEvent() const
    {
    return static_cast<T*>(const_cast<CEventBase*>(this));
    };

  //returns old state
  eState SetState(__in eState nNewState)
    {
    LONG initVal, newVal;

    if (nNewState != StateDetached && nNewState != StateQueued &&
        nNewState != StateProcessing && nNewState != StateProcessed)
      return StateInvalidArg;
    newVal = __InterlockedRead(&nState);
    do
    {
      initVal = newVal;
      newVal = _InterlockedCompareExchange(&nState, (initVal & (~0x0F)) | (LONG)nNewState, initVal);
    }
    while (newVal != initVal);
    return (eState)(initVal & 0x0F);
    };

  //returns old state
  eState SetStateIf(__in eState nNewState, __in eState nIfState)
    {
    LONG initVal, newVal;

    if (nNewState != StateDetached && nNewState != StateQueued &&
        nNewState != StateProcessing && nNewState != StateProcessed)
      return StateInvalidArg;
    if (nIfState != StateDetached && nIfState != StateQueued &&
        nIfState != StateProcessing && nIfState != StateProcessed)
      return StateInvalidArg;
    newVal = __InterlockedRead(&nState);
    do
    {
      initVal = newVal;
      if (nIfState != (eState)(initVal & 0x0F))
        break;
      newVal = _InterlockedCompareExchange(&nState, (initVal & (~0x0F)) | (LONG)nNewState, initVal);
    }
    while (newVal != initVal);
    return (eState)(initVal & 0x0F);
    };

  BOOL IsCanceled() const
    {
    return ((__InterlockedRead(&((const_cast<CEventBase*>(this))->nState)) & 0x10) != 0) ? TRUE : FALSE;
    };

  VOID MarkAsCanceled()
    {
    _InterlockedOr(&nState, 0x10);
    return;
    };

  VOID ResetCancelMark()
    {
    _InterlockedAnd(&nState, ~0x10);
    return;
    };

  VOID WaitUntilProcessed()
    {
    for (;;)
    {
      eState nState = GetState();
      if (nState != StateQueued && GetState() != StateProcessing)
        break;
      _YieldProcessor();
    }
    return;
    };

  LPVOID GetUserData() const
    {
    return lpUserData;
    };

  VOID SetUserData(__in LPVOID _lpUserData)
    {
    lpUserData = _lpUserData;
    return;
    };

  VOID InvokeCallback()
    {
    if (cCallback)
      cCallback(GetEvent());
    return;
    };

private:
  OnNotifyCallback cCallback;
  LPVOID lpUserData;
  LONG volatile nState;
};

//-----------------------------------------------------------

template<class T>
class TPendingListHelper : public virtual CBaseMemObj
{
public:
  typedef Callback<VOID (__in TArrayList<T> &cEventsList)> OnActionCallback;

public:
  TPendingListHelper() : CBaseMemObj()
    {
    _InterlockedExchange(&nMutex, 0);
    return;
    };

  ~TPendingListHelper()
    {
    WaitAll();
    return;
    };

  HRESULT Add(__in T elem)
    {
    CFastLock cLock(&nMutex);

    return cList.SortedInsert(elem, &TPendingListHelper<T>::_Compare, this);
    };

  VOID Remove(__in T elem)
    {
    CFastLock cLock(&nMutex);
    SIZE_T nIndex;

    nIndex = cList.BinarySearch(&elem, &TPendingListHelper<T>::_Compare, this);
    if (nIndex != (SIZE_T)-1)
      cList.RemoveElementAt(nIndex);
    return;
    };

  BOOL HasPending() const
    {
    CFastLock cLock(const_cast<LONG volatile*>(&nMutex));

    return (cList.GetCount() > 0) ? TRUE : FALSE;
    };

  VOID RunAction(__in OnActionCallback cActionCallback)
    {
    CFastLock cLock(&nMutex);

    cActionCallback(cList);
    return;
    };

  VOID WaitAll()
    {
    while (HasPending() != FALSE)
      _YieldProcessor();
    return;
    };

protected:
  virtual int Compare(__in T elem1, __in T elem2) = 0;

private:
  static int _Compare(__in LPVOID lpCtx, __in T *lpElem1, __in T *lpElem2)
    {
    return ((TPendingListHelper<T>*)lpCtx)->Compare(*lpElem1, *lpElem2);
    };

protected:
  LONG volatile nMutex;
  TArrayList<T> cList;
};

//-----------------------------------------------------------

template<class T>
class TPendingListHelperGeneric : public TPendingListHelper<T>
{
protected:
  int Compare(__in T elem1, __in T elem2)
    {
    SSIZE_T v = (SSIZE_T)((SIZE_T)elem1 - (SIZE_T)elem2);
    return (v > 0) - (v < 0);
    };
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_EVENTNOTIFYBASE_H
