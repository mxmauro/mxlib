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
