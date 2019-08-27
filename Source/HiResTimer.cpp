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
#include "..\Include\HiResTimer.h"

//-----------------------------------------------------------

namespace MX {

CHiResTimer::CHiResTimer() : CBaseMemObj()
{
  Reset();
  return;
}

VOID CHiResTimer::Reset()
{
  if (!NT_SUCCESS(::MxNtQueryPerformanceCounter((PLARGE_INTEGER)&uliStartCounter, (PLARGE_INTEGER)&uliFrequency)))
  {
    uliStartCounter.HighPart = 0;
    uliStartCounter.LowPart = ::GetTickCount();
    uliFrequency.QuadPart = 0ui64;
  }
  return;
}

DWORD CHiResTimer::GetElapsedTimeMs(_In_opt_ BOOL bReset)
{
  DWORD dwElapsedMs;

  if (uliFrequency.QuadPart != 0ui64)
  {
    ULARGE_INTEGER liCurrCounter, _liFrequency;

    ::MxNtQueryPerformanceCounter((PLARGE_INTEGER)&liCurrCounter, (PLARGE_INTEGER)&_liFrequency);

    dwElapsedMs = (DWORD)(((liCurrCounter.QuadPart - uliStartCounter.QuadPart) * 1000ui64) / uliFrequency.QuadPart);
    if (bReset != FALSE)
      uliStartCounter.QuadPart = liCurrCounter.QuadPart;
  }
  else
  {
    DWORD dwCurrCounter = ::GetTickCount();

    dwElapsedMs = dwCurrCounter - uliStartCounter.LowPart;
    if (bReset != FALSE)
      uliStartCounter.LowPart = dwCurrCounter;
  }
  return dwElapsedMs;
}

} //namespace MX
