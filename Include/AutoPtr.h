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
#ifndef _MX_AUTOPTR_H
#define _MX_AUTOPTR_H

#include "Defines.h"
#include "WaitableObjects.h"

//-----------------------------------------------------------

namespace MX {

template <class T>
class TAutoPtrBase : public virtual CBaseMemObj
{
public:
  TAutoPtrBase() : CBaseMemObj()
    {
    lpPtr = NULL;
    return;
    };

  TAutoPtrBase(__in T* _lpPtr)
    {
    lpPtr = _lpPtr;
    return;
    };

  virtual ~TAutoPtrBase()
    {
    return;
    };

  T* Get()
    {
    return lpPtr;
    };

  VOID Reset()
    {
    if (lpPtr != NULL)
    {
      OnDeleteItem(lpPtr);
      lpPtr = NULL;
    }
    return;
    };

  operator T*() const
    {
    return lpPtr;
    };

  T& operator*() const
    {
    MX_ASSERT(lpPtr != NULL);
    return *lpPtr;
    };

  //The assert on operator& usually indicates a bug.  If this is really
  //what is needed, however, take the address of the lpPtr member explicitly.
  T** operator&()
    {
    MX_ASSERT(lpPtr == NULL);
    return &lpPtr;
    };

  T* operator->() const
    {
    MX_ASSERT(lpPtr != NULL);
    return lpPtr;
    };

  T* operator=(__in T* _lpPtr)
    {
    if (_lpPtr != lpPtr)
      Attach(_lpPtr);
    return _lpPtr;
    };

  T* operator=(__in const TAutoPtrBase<T>& cPtr)
    {
    if (this != &cPtr)
    {
      if (cPtr.lpPtr != NULL)
        cPtr.lpPtr->AddRef();
      if (lpPtr != NULL)
        lpPtr->Release();
      lpPtr = cPtr.lpPtr;
    }
    return lpPtr;
    };

  bool operator!() const
    {
    return (lpPtr == NULL) ? true : false;
    };

  operator bool() const
    {
    return (lpPtr != NULL) ? true : false;
    };

  bool operator==(__in T* _lpPtr) const
    {
    return (lpPtr == _lpPtr) ? true : false;
    };

  VOID Attach(__in T* _lpPtr)
    {
    if (lpPtr != NULL)
      OnDeleteItem(lpPtr);
    lpPtr = _lpPtr;
    return;
  };

  T* Detach()
    {
    T* _lpPtr = lpPtr;
    lpPtr = NULL;
    return _lpPtr;
    };

protected:
  virtual VOID OnDeleteItem(__inout T *lpObj) = 0;

protected:
  T *lpPtr;
};

//-----------------------------------------------------------

template <class T>
class TAutoFreePtr : public TAutoPtrBase<T>
{
public:
  virtual ~TAutoFreePtr()
    {
    if (lpPtr != NULL)
    {
      OnDeleteItem(lpPtr);
      lpPtr = NULL;
    }
    return;
    };

protected:
  virtual VOID OnDeleteItem(__inout T *lpObj)
    {
    MX_FREE(lpObj);
    return;
    };
};

//-----------------------------------------------------------

template <class T>
class TAutoDeletePtr : public TAutoPtrBase<T>
{
public:
  virtual ~TAutoDeletePtr()
    {
    if (lpPtr != NULL)
    {
      OnDeleteItem(lpPtr);
      lpPtr = NULL;
    }
    return;
    };

protected:
  virtual VOID OnDeleteItem(__inout T *lpObj)
    {
    if (lpObj != NULL)
      delete lpObj;
    return;
    };
};

//-----------------------------------------------------------

template <class T>
class TAutoDeleteArrayPtr : public TAutoPtrBase<T>
{
public:
  virtual ~TAutoDeleteArrayPtr()
    {
    if (lpPtr != NULL)
    {
      OnDeleteItem(lpPtr);
      lpPtr = NULL;
    }
    return;
    };

protected:
  virtual VOID OnDeleteItem(__inout T *lpObj)
    {
    delete [] lpObj;
    return;
    }
};

//-----------------------------------------------------------

template <class T>
class TAtomicPtr : public virtual CBaseMemObj
{
public:
  TAtomicPtr() : CBaseMemObj()
    {
    _InterlockedExchangePointer((LPVOID volatile*)&lpPtr, NULL);
    return;
    };

  TAtomicPtr(__in T* _lpPtr)
    {
    _InterlockedExchangePointer((LPVOID volatile*)&lpPtr, _lpPtr);
    return;
    };

  T* Get()
    {
    return (T*)__InterlockedReadPointer((LPVOID volatile*)&lpPtr);
    };

  operator T*() const
    {
    return Get();
    };

  T* operator->() const
    {
    MX_ASSERT(const_cast<TAtomicPtr<T>*>(this)->Get() != NULL);
    return const_cast<TAtomicPtr<T>*>(this)->Get();
    };

  T* operator=(__in T* _lpPtr)
    {
    _InterlockedExchangePointer((PVOID volatile*)&lpPtr, _lpPtr);
    return _lpPtr;
    };

  T* operator=(__in const TAtomicPtr<T>& cPtr)
    {
    if (this != &cPtr)
      _InterlockedExchangePointer((PVOID volatile*)&lpPtr, cPtr.Get());
    return Get();
    };

  bool operator!() const
    {
    return (const_cast<TAtomicPtr<T>*>(this)->Get() == NULL) ? true : false;
    };

  operator bool() const
    {
    return (const_cast<TAtomicPtr<T>*>(this)->Get() != NULL) ? true : false;
    };

  bool operator==(__in T* _lpPtr) const
    {
    return (const_cast<TAtomicPtr<T>*>(this)->Get() == _lpPtr) ? true : false;
    };

  T* Detach()
    {
    T* _lpPtr = Get();
    __InterlockedExchangePointer((PVOID volatile*)&lpPtr, NULL);
    return _lpPtr;
    };

protected:
  T volatile *lpPtr;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_AUTOPTR_H
