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
#ifndef _MX_MEMORYBARRIER_H
#define _MX_MEMORYBARRIER_H

#include "Defines.h"
#include <intrin.h>

#pragma intrinsic(_mm_lfence)
#pragma intrinsic(_mm_mfence)
#if defined(_M_X64) || defined(_M_AMD64)
  #pragma intrinsic(__faststorefence)
#else //_M_X64 || _M_AMD64
  #include <mmintrin.h>
  #pragma intrinsic(_mm_sfence)
#endif //_M_X64 || _M_AMD64

//-----------------------------------------------------------

namespace MX {

__forceinline VOID LFence()
{
  _mm_lfence(); //we assume the app will run on Pentium IV+, when SSE2 was introduced
  return;
}

__forceinline VOID SFence()
{
#if defined(_M_X64) || defined(_M_AMD64)
  __faststorefence();
#else //_M_X64 || _M_AMD64
  _mm_sfence(); //we assume the app will run on Pentium IV+, when SSE2 was introduced
#endif //_M_X64 || _M_AMD64
  return;
}

__forceinline VOID MFence()
{
  _mm_mfence(); //we assume the app will run on Pentium IV+, when SSE2 was introduced
  return;
}

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_MEMORYBARRIER_H
