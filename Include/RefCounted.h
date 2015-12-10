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
#ifndef _MX_REFCOUNTED_H
#define _MX_REFCOUNTED_H

#include "WaitableObjects.h"
#include "Debug.h"

//-----------------------------------------------------------

namespace MX {

template <class T>
class TRefCounted
{
protected:
  TRefCounted()
    {
    _InterlockedExchange(&nRefCount, 1);
    return;
    };

public:
  virtual ~TRefCounted()
    {
    return;
    };

  ULONG AddRef()
    {
    return (ULONG)_InterlockedIncrement(&nRefCount);
    };
  ULONG Release()
    {
    LONG nInitVal, nOrigVal, nNewVal;

    nOrigVal = __InterlockedRead(&nRefCount);
    do
    {
      nInitVal = nOrigVal;
      nNewVal = (nInitVal != 0) ? (nInitVal-1) : (-(2147483647L/2L));
      nOrigVal = _InterlockedCompareExchange(&nRefCount, nNewVal, nInitVal);
    }
    while (nOrigVal != nInitVal);
#ifdef _DEBUG
    if (nNewVal < -(2147483647L/2L) || (nNewVal & 0x80000000) || nNewVal > 500)
    {
      MX_ASSERT(FALSE);
    }
#endif //_DEBUG
    if (nNewVal == 0)
    {
      delete this;
    }
    return (ULONG)nNewVal;
    };

private:
  LONG volatile nRefCount;
};

//-----------------------------------------------------------

template <class T>
class TAutoRefCounted
{
public:
  TAutoRefCounted()
    {
    lpPtr = NULL;
    return;
    };

  TAutoRefCounted(__in_opt T* _lpPtr)
    {
    if ((lpPtr = _lpPtr) != NULL)
      lpPtr->AddRef();
    return;
    };

  TAutoRefCounted(__in TAutoRefCounted<T> &cPtr)
    {
    if ((lpPtr = cPtr.lpPtr) != NULL)
      lpPtr->AddRef();
    return;
    };

  ~TAutoRefCounted()
    {
    if (lpPtr != NULL)
      lpPtr->Release();
    return;
    };

  VOID Release()
    {
    T *lpTemp;

    if ((lpTemp = lpPtr) != NULL)
    {
      lpPtr = NULL;
      lpTemp->Release();
    }
    return;
    };

  operator T*() const
    {
    return lpPtr;
    };

  T* Get() const
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

  T** GetAddressOfPointer()
    {
    return &lpPtr;
    };

  T* operator->() const
    {
    MX_ASSERT(lpPtr != NULL);
    return lpPtr;
    };

  T* operator=(__in_opt T* _lpPtr)
    {
    if (_lpPtr != lpPtr)
    {
      if (_lpPtr != NULL)
        _lpPtr->AddRef();
      if (lpPtr != NULL)
        lpPtr->Release();
      lpPtr = _lpPtr;
    }
    return _lpPtr;
    };

  T* operator=(__in const TAutoRefCounted<T>& cPtr)
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

  BOOL operator!() const
    {
    return (lpPtr == NULL) ? true : false;
    };

  operator bool() const
    {
    return (lpPtr != NULL) ? true : false;
    };

  bool operator==(__in_opt T* _lpPtr) const
    {
    return (lpPtr == _lpPtr) ? true : false;
    };

  VOID Attach(__in_opt T* _lpPtr)
    {
    if (lpPtr != NULL)
      lpPtr->Release();
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
  T* lpPtr;
};

//-----------------------------------------------------------

} //namespace MX

#endif //_MX_REFCOUNTED_H
