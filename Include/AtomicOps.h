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
#ifndef _MX_ATOMICOPS_H
#define _MX_ATOMICOPS_H

#include "Defines.h"
#include <intrin.h>

#pragma intrinsic (_InterlockedExchange)
#pragma intrinsic (_InterlockedCompareExchange)
#pragma intrinsic (_InterlockedExchangeAdd)
#if defined(_M_X64)
  #pragma intrinsic (_InterlockedExchangeAdd64)
  #pragma intrinsic (_InterlockedExchange64)
  #pragma intrinsic (_InterlockedCompareExchange64)
#endif //_M_X64

//-----------------------------------------------------------

namespace MX {

#define __InterlockedRead(lpnValue) _InterlockedExchangeAdd(lpnValue, 0L)
#define __InterlockedRead64(lpnValue) _InterlockedExchangeAdd64(lpnValue, 0i64)

#if defined(_M_IX86)

#define __InterlockedReadPointer(lpValue)                                                          \
           ((LPVOID)_InterlockedExchangeAdd((LONG volatile*)lpValue, 0L))
#define __InterlockedExchangePointer(lpValue, lpPtr)                                               \
           ((LPVOID)_InterlockedExchange((LONG volatile*)lpValue, PtrToLong(lpPtr)))
#define __InterlockedCompareExchangePointer(lpValue, lpExchange, lpComperand)                      \
           ((LPVOID)_InterlockedCompareExchange((LONG volatile*)lpValue, (LONG)lpExchange,         \
                                                PtrToLong(lpComperand)))

#elif defined(_M_X64)

#define __InterlockedReadPointer(lpValue)                                                          \
           ((LPVOID)_InterlockedExchangeAdd64((__int64 volatile*)lpValue, 0i64))
#define __InterlockedExchangePointer(lpValue, lpPtr)                                               \
           ((LPVOID)_InterlockedExchange64((__int64 volatile*)lpValue, (__int64)lpPtr))
#define __InterlockedCompareExchangePointer(lpValue, lpExchange, lpComperand)                      \
           ((LPVOID)_InterlockedCompareExchange64((__int64 volatile*)lpValue, (__int64)lpExchange, \
                                                  (__int64)lpComperand))

#endif

#define __InterlockedReadSizeT(lpnValue)                                                           \
           ((SIZE_T)__InterlockedReadPointer((LPVOID volatile*)lpnValue))
#define __InterlockedExchangeSizeT(lpnValue, nNewValue)                                            \
           ((SIZE_T)__InterlockedExchangePointer((LPVOID volatile*)lpnValue, (LPVOID)nNewValue))
#define __InterlockedCompareExchangeSizeT(lpnValue, nExchange, nComperand)                         \
           ((SIZE_T)__InterlockedCompareExchangePointer((LPVOID volatile*)lpnValue,                \
                                                        (LPVOID)nExchange, (LPVOID)nComperand))

inline LONG __InterlockedClamp(_In_ LONG volatile *lpnValue, _In_ LONG nMinimumValue, _In_ LONG nMaximumValue)
{
  LONG initVal, newVal;

  newVal = __InterlockedRead(lpnValue);
  do
  {
    initVal = newVal;
    if (newVal < nMinimumValue)
      newVal = nMinimumValue;
    else if (newVal > nMaximumValue)
      newVal = nMaximumValue;
    else
      break;
    newVal = _InterlockedCompareExchange(lpnValue, newVal, initVal);
  }
  while (newVal != initVal);
  return newVal;
}

inline DWORD __InterlockedClampU(_In_ LONG volatile *lpnValue, _In_ DWORD dwMinimumValue, _In_ DWORD dwMaximumValue)
{
  DWORD initVal, newVal;

  newVal = (DWORD)__InterlockedRead(lpnValue);
  do
  {
    initVal = newVal;
    if (newVal < dwMinimumValue)
      newVal = dwMinimumValue;
    else if (newVal > dwMaximumValue)
      newVal = dwMaximumValue;
    else
      break;
    newVal = _InterlockedCompareExchange(lpnValue, (LONG)newVal, (LONG)initVal);
  }
  while (newVal != initVal);
  return newVal;
}

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_ATOMICOPS_H
