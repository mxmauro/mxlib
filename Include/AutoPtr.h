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
#ifndef _MX_AUTOPTR_H
#define _MX_AUTOPTR_H

#include "Defines.h"
#include "WaitableObjects.h"

//-----------------------------------------------------------

namespace MX {

template <class T>
class MX_NOVTABLE TAutoPtrBase : public virtual CBaseMemObj
{
public:
  TAutoPtrBase() : CBaseMemObj()
    {
    lpPtr = NULL;
    return;
    };

  TAutoPtrBase(_In_ T* _lpPtr)
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

  T* operator=(_In_ T* _lpPtr)
    {
    if (_lpPtr != lpPtr)
      Attach(_lpPtr);
    return _lpPtr;
    };

  bool operator!() const
    {
    return (lpPtr == NULL) ? true : false;
    };

  operator bool() const
    {
    return (lpPtr != NULL) ? true : false;
    };

  bool operator==(_In_ T* _lpPtr) const
    {
    return (lpPtr == _lpPtr) ? true : false;
    };

  VOID Attach(_In_ T* _lpPtr)
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
  virtual VOID OnDeleteItem(_Inout_ T *lpObj) = 0;

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
  virtual VOID OnDeleteItem(_Inout_ T *lpObj)
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
  virtual VOID OnDeleteItem(_Inout_ T *lpObj)
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
  virtual VOID OnDeleteItem(_Inout_ T *lpObj)
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

  TAtomicPtr(_In_ T* _lpPtr)
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

  T* operator=(_In_ T* _lpPtr)
    {
    _InterlockedExchangePointer((PVOID volatile*)&lpPtr, _lpPtr);
    return _lpPtr;
    };

  T* operator=(_In_ const TAtomicPtr<T>& cPtr)
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

  bool operator==(_In_ T* _lpPtr) const
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
