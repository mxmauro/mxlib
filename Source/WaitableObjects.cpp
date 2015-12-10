/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2015. All rights reserved.
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
#include "..\Include\WaitableObjects.h"
#include "..\Include\Debug.h"

//#define SHOW_SLIMRWL_DEBUG_INFO

//-----------------------------------------------------------

#pragma pack(8)
typedef struct {
  BYTE Reserved1[24];
  PVOID Reserved2[4];
  CCHAR NumberOfProcessors;
} __SYSTEM_BASIC_INFORMATION;
#pragma pack(8)

//-----------------------------------------------------------

typedef NTSTATUS (NTAPI *lpfnRtlGetNativeSystemInformation)(__in MX_SYSTEM_INFORMATION_CLASS SystemInformationClass,
                                               __inout PVOID SystemInformation, __in ULONG SystemInformationLength,
                                               __out_opt PULONG ReturnLength);

//-----------------------------------------------------------

static LONG volatile nProcessorsCount = 0;

//-----------------------------------------------------------

namespace MX {

BOOL IsMultiProcessor()
{
  if (__InterlockedRead(&nProcessorsCount) == 0)
  {
    __SYSTEM_BASIC_INFORMATION sBasicInfo;
    PVOID DllBase;
    lpfnRtlGetNativeSystemInformation fnRtlGetNativeSystemInformation;
    NTSTATUS nNtStatus;

    fnRtlGetNativeSystemInformation = NULL;
    DllBase = MxGetDllHandle(L"ntdll.dll");
    if (DllBase != NULL)
    {
      fnRtlGetNativeSystemInformation = (lpfnRtlGetNativeSystemInformation)MxGetProcedureAddress(
        DllBase, "RtlGetNativeSystemInformation");
    }
    if (fnRtlGetNativeSystemInformation != NULL)
      nNtStatus = fnRtlGetNativeSystemInformation(MxSystemBasicInformation, &sBasicInfo, sizeof(sBasicInfo), NULL);
    else
      nNtStatus = ::MxNtQuerySystemInformation(MxSystemProcessorInformation, &sBasicInfo, sizeof(sBasicInfo), NULL);
    _InterlockedExchange(&nProcessorsCount, (NT_SUCCESS(nNtStatus) && sBasicInfo.NumberOfProcessors > 1) ?
                                             (LONG)(sBasicInfo.NumberOfProcessors) : 1);
  }
  return (__InterlockedRead(&nProcessorsCount) > 1) ? TRUE : FALSE;
}

VOID _YieldProcessor()
{
  if (IsMultiProcessor() == FALSE)
    ::MxNtYieldExecution();
  else
    ::MxSleep(1);
  return;
}

//-----------------------------------------------------------

VOID SlimRWL_Initialize(__in LONG volatile *lpnValue)
{
  _InterlockedExchange(lpnValue, 0);
  return;
}

BOOL SlimRWL_TryAcquireShared(__in LONG volatile *lpnValue)
{
  LONG initVal, newVal;

  newVal = __InterlockedRead(lpnValue);
  do
  {
    initVal = newVal;
    if ((initVal & 0x80000000L) != 0)
      return FALSE; //a writer is active
    MX_ASSERT(((ULONG)initVal & 0x7FFFFFFFUL) != 0x7FFFFFFFUL); //check overflows
    newVal = _InterlockedCompareExchange(lpnValue, initVal+1, initVal);
  }
  while (newVal != initVal);
  return TRUE;
}

VOID SlimRWL_AcquireShared(__in LONG volatile *lpnValue)
{
  while (SlimRWL_TryAcquireShared(lpnValue) == FALSE)
    _YieldProcessor();
#ifdef SHOW_SLIMRWL_DEBUG_INFO
  DebugPrint("SlimRWL_AcquireShared: %lu\n", MxGetCurrentThreadId());
#endif //SHOW_SLIMRWL_DEBUG_INFO
  return;
}

VOID SlimRWL_ReleaseShared(__in LONG volatile *lpnValue)
{ 
  LONG initVal, newVal;

  newVal = __InterlockedRead(lpnValue);
  do
  {
    initVal = newVal;
    MX_ASSERT((initVal & 0x7FFFFFFFL) != 0);
    newVal = (initVal & 0x80000000L) | ((initVal & 0x7FFFFFFFL) - 1);
    newVal = _InterlockedCompareExchange(lpnValue, newVal, initVal);
  }
  while (newVal != initVal);
#ifdef SHOW_SLIMRWL_DEBUG_INFO
  DebugPrint("SlimRWL_ReleaseShared: %lu\n", MxGetCurrentThreadId());
#endif //SHOW_SLIMRWL_DEBUG_INFO
  return;
}

BOOL SlimRWL_TryAcquireExclusive(__in LONG volatile *lpnValue)
{
  LONG initVal, newVal;

  newVal = __InterlockedRead(lpnValue);
  do
  {
    initVal = newVal;
    if ((initVal & 0x80000000L) != 0)
      return FALSE; //another writer is active or waiting
    newVal = _InterlockedCompareExchange(lpnValue, initVal | 0x80000000L, initVal);
  }
  while (newVal != initVal);
  //wait until no readers
  while ((__InterlockedRead(lpnValue) & 0x7FFFFFFFL) != 0)
    _YieldProcessor();
  return TRUE;
}

VOID SlimRWL_AcquireExclusive(__in LONG volatile *lpnValue)
{
  while (SlimRWL_TryAcquireExclusive(lpnValue) == FALSE)
    _YieldProcessor();
#ifdef SHOW_SLIMRWL_DEBUG_INFO
  DebugPrint("SlimRWL_AcquireExclusive: %lu\n", MxGetCurrentThreadId());
#endif //SHOW_SLIMRWL_DEBUG_INFO
  return;
}

VOID SlimRWL_ReleaseExclusive(__in LONG volatile *lpnValue)
{
  MX_ASSERT(__InterlockedRead(lpnValue) == 0x80000000L);
  _InterlockedExchange(lpnValue, 0);
#ifdef SHOW_SLIMRWL_DEBUG_INFO
  DebugPrint("SlimRWL_ReleaseExclusive: %lu\n", MxGetCurrentThreadId());
#endif //SHOW_SLIMRWL_DEBUG_INFO
  return;
}

//-----------------------------------------------------------

VOID RundownProt_Initialize(__in LONG volatile *lpnValue)
{
  _InterlockedExchange(lpnValue, 0);
  return;
}

BOOL RundownProt_Acquire(__in LONG volatile *lpnValue)
{
  LONG initVal, newVal;

  newVal = __InterlockedRead(lpnValue);
  do
  {
    initVal = newVal;
    if ((initVal & 0x80000000L) != 0)
      return FALSE;
    newVal = _InterlockedCompareExchange(lpnValue, initVal+1, initVal);
  }
  while (newVal != initVal);
  return TRUE;
}

VOID RundownProt_Release(__in LONG volatile *lpnValue)
{
  LONG initVal, newVal;

  newVal = __InterlockedRead(lpnValue);
  do
  {
    initVal = newVal;
    MX_ASSERT((initVal & 0x7FFFFFFFL) != 0);
    newVal = (initVal & 0x80000000L) | ((initVal & 0x7FFFFFFFL) - 1);
    newVal = _InterlockedCompareExchange(lpnValue, newVal, initVal);
  }
  while (newVal != initVal);
  return;
}

VOID RundownProt_WaitForRelease(__in LONG volatile *lpnValue)
{
  //mark rundown protection as shutting down
  _InterlockedOr(lpnValue, 0x80000000L);
  //wait until no locks active
  while (__InterlockedRead(lpnValue) != 0x80000000L)
    _YieldProcessor();
  return;
}

} //namespace MX
