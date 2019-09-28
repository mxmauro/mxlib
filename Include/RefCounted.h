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
#ifndef _MX_REFCOUNTED_H
#define _MX_REFCOUNTED_H

#include "WaitableObjects.h"
#include "Debug.h"

//-----------------------------------------------------------

namespace MX {

template <class T>
class MX_NOVTABLE TRefCounted : public T
{
public:
  TRefCounted() : T()
    {
    _InterlockedExchange(&nRefCount, 1);
    return;
    };

  virtual ~TRefCounted()
    {
    return;
    };

  ULONG AddRef()
    {
    ULONG nNewVal = (ULONG)_InterlockedIncrement(&nRefCount);
    MX_ASSERT((nNewVal & 0xC0000000) == 0);
    return nNewVal;
    };

  ULONG SafeAddRef()
    {
    LONG nInitVal, nOrigVal, nNewVal;

    nOrigVal = __InterlockedRead(&nRefCount);
    do
    {
      nInitVal = nOrigVal;
      nNewVal = (nInitVal > 0) ? (nInitVal + 1) : nInitVal;
      nOrigVal = _InterlockedCompareExchange(&nRefCount, nNewVal, nInitVal);
    }
    while (nOrigVal != nInitVal);
    MX_ASSERT(((ULONG)nNewVal & 0xC0000000) == 0);
    return (nNewVal > 0) ? (ULONG)nNewVal : 0;
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
    MX_ASSERT(!(nNewVal <= -(2147483647L / 2L) || (nNewVal & 0x80000000) || nNewVal > 0x01000000));
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

  TAutoRefCounted(_In_opt_ T* _lpPtr)
    {
    if ((lpPtr = _lpPtr) != NULL)
      lpPtr->AddRef();
    return;
    };

  TAutoRefCounted(_In_ TAutoRefCounted<T> &cPtr)
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

  T* operator=(_In_opt_ T* _lpPtr)
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

  T* operator=(_In_ const TAutoRefCounted<T>& cPtr)
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

  bool operator==(_In_opt_ T* _lpPtr) const
    {
    return (lpPtr == _lpPtr) ? true : false;
    };

  VOID Attach(_In_opt_ T* _lpPtr)
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
