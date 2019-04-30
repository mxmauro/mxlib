/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2019. All rights reserved.
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
