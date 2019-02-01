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
#include "..\Include\Debug.h"
#include "..\Include\WaitableObjects.h"
#include <stdio.h>

//-----------------------------------------------------------

#define MX_FORCE_OUTPUTDEBUG_ON_XP

//-----------------------------------------------------------

typedef ULONG (__cdecl *lpfnDbgPrint)(_In_z_ PCSTR Format, ...);
typedef VOID (WINAPI *lpfnOutputDebugStringA)(_In_opt_ LPCSTR lpOutputString);

//-----------------------------------------------------------

namespace MX {

VOID DebugPrint(_In_z_ LPCSTR szFormatA, ...)
{
  va_list ap;

  va_start(ap, szFormatA);
  DebugPrintV(szFormatA, ap);
  va_end(ap);
  return;
}

VOID DebugPrintV(_In_z_ LPCSTR szFormatA, va_list ap)
{
  static LONG volatile nAccessMtx = 0;
  static lpfnOutputDebugStringA fnOutputDebugStringA = NULL;
  static lpfnDbgPrint fnDbgPrint = NULL;
#ifndef MX_FORCE_OUTPUTDEBUG_ON_XP
  static LONG volatile nOsVersion = 0;
#endif //MX_FORCE_OUTPUTDEBUG_ON_XP
  CHAR szBufA[2048];
  int i;

#ifndef MX_FORCE_OUTPUTDEBUG_ON_XP
  if (nOsVersion == 0)
  {
    RTL_OSVERSIONINFOW sOviW;

    MemSet(&sOviW, 0, sizeof(sOviW));
    sOviW.dwOSVersionInfoSize = (DWORD) sizeof(sOviW);
    if (!NT_SUCCESS(::RtlGetVersion(&sOviW)))
      return;
    _InterlockedExchange(&nOsVersion, (LONG)(sOviW.dwMajorVersion));
  }
  if (__InterlockedRead(&nOsVersion) < 6)
    return;
#endif //MX_FORCE_OUTPUTDEBUG_ON_XP
  //----
  i = mx_vsnprintf(szBufA, MX_ARRAYLEN(szBufA), szFormatA, ap);
  if (i < 0)
    i = 0;
  else if (i > MX_ARRAYLEN(szBufA)-1)
    i = MX_ARRAYLEN(szBufA)-1;
  szBufA[i] = 0;
  //----
  if (fnOutputDebugStringA == NULL)
  {
    PVOID DllBase = MxGetDllHandle(L"kernel32.dll");
    fnOutputDebugStringA = (lpfnOutputDebugStringA)MxGetProcedureAddress(DllBase, "OutputDebugStringA");
    if (fnOutputDebugStringA == NULL)
      fnOutputDebugStringA = (lpfnOutputDebugStringA)1;
  }
  if (fnDbgPrint == NULL)
  {
    PVOID DllBase = MxGetDllHandle(L"ntdll.dll");
    fnDbgPrint = (lpfnDbgPrint)MxGetProcedureAddress(DllBase, "DbgPrint");
    if (fnDbgPrint == NULL)
      fnDbgPrint = (lpfnDbgPrint)1;
  }
  //----
  if (fnOutputDebugStringA == NULL || fnOutputDebugStringA == (PVOID)1)
  {
    fnDbgPrint("%s", szBufA);
  }
  else
  {
    MX::CFastLock cAccessLock(&nAccessMtx);

    fnOutputDebugStringA(szBufA);
  }
  return;
}

} //namespace MX
