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
#include "..\Include\Timer.h"

//-----------------------------------------------------------

namespace MX {

CTimer::CTimer() : CBaseMemObj()
{
  if (!NT_SUCCESS(::MxNtQueryPerformanceCounter((PLARGE_INTEGER)&uliStart, (PLARGE_INTEGER)&uliFrequency)))
  {
    uliStart.HighPart = 0;
    uliStart.LowPart = ::GetTickCount();
    uliFrequency.QuadPart = 0ui64;
  }
  uliMark.QuadPart = uliStart.QuadPart;
  return;
}

CTimer::CTimer(_In_ CTimer const &cSrc)
{
  uliStart.QuadPart = cSrc.uliStart.QuadPart;
  uliMark.QuadPart = cSrc.uliMark.QuadPart;
  uliFrequency.QuadPart = cSrc.uliFrequency.QuadPart;
  return;
}

CTimer& CTimer::operator=(_In_ CTimer const &cSrc)
{
  uliStart.QuadPart = cSrc.uliStart.QuadPart;
  uliMark.QuadPart = cSrc.uliMark.QuadPart;
  uliFrequency.QuadPart = cSrc.uliFrequency.QuadPart;
  return *this;
}

VOID CTimer::Reset()
{
  Mark();
  ResetToLastMark();
  return;
}

VOID CTimer::Mark()
{
  if (uliFrequency.QuadPart != 0ui64)
  {
    ULARGE_INTEGER _liFrequency;

    ::MxNtQueryPerformanceCounter((PLARGE_INTEGER)&uliMark, (PLARGE_INTEGER)&_liFrequency);
  }
  else
  {
    uliMark.LowPart = ::GetTickCount();
  }
  return;
}

VOID CTimer::ResetToLastMark()
{
  uliStart.QuadPart = uliMark.QuadPart;
  return;
}

DWORD CTimer::GetElapsedTimeMs() const
{
  if (uliFrequency.QuadPart != 0ui64)
  {
    return (DWORD)(((uliMark.QuadPart - uliStart.QuadPart) * 1000ui64) / uliFrequency.QuadPart);
  }
  return uliMark.LowPart - uliStart.LowPart;
}

DWORD CTimer::GetStartTimeMs() const
{
  if (uliFrequency.QuadPart != 0ui64)
  {
    return (DWORD)((uliStart.QuadPart * 1000ui64) / uliFrequency.QuadPart);
  }
  return uliStart.LowPart;
}

DWORD CTimer::GetMarkTimeMs() const
{
  if (uliFrequency.QuadPart != 0ui64)
  {
    return (DWORD)((uliMark.QuadPart * 1000ui64) / uliFrequency.QuadPart);
  }
  return uliMark.LowPart;
}

} //namespace MX
