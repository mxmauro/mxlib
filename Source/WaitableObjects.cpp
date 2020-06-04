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
#include "..\Include\WaitableObjects.h"
#include "..\Include\Debug.h"
#include "Internals\MsVcrt.h"

//#define SHOW_SLIMRWL_DEBUG_INFO

#ifndef OBJ_CASE_INSENSITIVE
  #define OBJ_CASE_INSENSITIVE    0x00000040L
#endif //OBJ_CASE_INSENSITIVE

#ifndef STATUS_PROCEDURE_NOT_FOUND
  #define STATUS_PROCEDURE_NOT_FOUND    0xC000007AL
#endif //STATUS_PROCEDURE_NOT_FOUND

//-----------------------------------------------------------

#pragma pack(8)
typedef struct {
  BYTE Reserved1[24];
  PVOID Reserved2[4];
  CCHAR NumberOfProcessors;
} __SYSTEM_BASIC_INFORMATION;
#pragma pack(8)

typedef int (*_PIFV)(void);

//-----------------------------------------------------------

typedef NTSTATUS (NTAPI *lpfnRtlGetNativeSystemInformation)(_In_ MX_SYSTEM_INFORMATION_CLASS SystemInformationClass,
                                               _Inout_ PVOID SystemInformation, _In_ ULONG SystemInformationLength,
                                               _Out_opt_ PULONG ReturnLength);

typedef HANDLE (WINAPI *lpfnCreateEventW)(_In_opt_ LPSECURITY_ATTRIBUTES lpEventAttributes, _In_ BOOL bManualReset,
                                          _In_ BOOL bInitialState, _In_opt_ LPCWSTR lpName);
typedef HANDLE (WINAPI *lpfnOpenEventW)(_In_ DWORD dwDesiredAccess, _In_ BOOL bInheritHandle, _In_ LPCWSTR lpName);
typedef HANDLE (WINAPI *lpfnCreateMutexW)(_In_opt_ LPSECURITY_ATTRIBUTES lpMutexAttributes, _In_ BOOL bInitialOwner,
                                          _In_opt_ LPCWSTR lpName);
typedef HANDLE (WINAPI *lpfnOpenMutexW)(_In_ DWORD dwDesiredAccess, _In_ BOOL bInheritHandle, _In_opt_ LPCWSTR lpName);

typedef VOID (WINAPI *lpfnInitializeSRWLock)(_Out_ PSRWLOCK SRWLock);
typedef VOID (WINAPI *lpfnReleaseSRWLockExclusive)(_Inout_ PSRWLOCK SRWLock);
typedef VOID (WINAPI *lpfnReleaseSRWLockShared)(_Inout_ PSRWLOCK SRWLock);
typedef VOID (WINAPI *lpfnAcquireSRWLockExclusive)(_Inout_ PSRWLOCK SRWLock);
typedef VOID (WINAPI *lpfnAcquireSRWLockShared)(_Inout_ PSRWLOCK SRWLock);
typedef BOOLEAN (WINAPI *lpfnTryAcquireSRWLockExclusive)(_Inout_ PSRWLOCK SRWLock);
typedef BOOLEAN (WINAPI *lpfnTryAcquireSRWLockShared)(_Inout_ PSRWLOCK SRWLock);

//-----------------------------------------------------------

static LONG volatile nProcessorsCount = 0;

static lpfnCreateEventW fnCreateEventW = NULL;
static lpfnOpenEventW fnOpenEventW = NULL;
static lpfnCreateMutexW fnCreateMutexW = NULL;
static lpfnOpenMutexW fnOpenMutexW = NULL;

static lpfnInitializeSRWLock fnInitializeSRWLock = NULL;
static lpfnReleaseSRWLockExclusive fnReleaseSRWLockExclusive = NULL;
static lpfnReleaseSRWLockShared fnReleaseSRWLockShared = NULL;
static lpfnAcquireSRWLockExclusive fnAcquireSRWLockExclusive = NULL;
static lpfnAcquireSRWLockShared fnAcquireSRWLockShared = NULL;
static lpfnTryAcquireSRWLockExclusive fnTryAcquireSRWLockExclusive = NULL;
static lpfnTryAcquireSRWLockShared fnTryAcquireSRWLockShared = NULL;

//-----------------------------------------------------------

static int __cdecl InitializeKernel32Apis();
static NTSTATUS GetRootDirHandle(_Out_ PHANDLE lphRootDir);

//-----------------------------------------------------------

MX_LINKER_FORCE_INCLUDE(___mx_waitable_init);
#pragma section(".CRT$XIBA", long, read)
extern "C" __declspec(allocate(".CRT$XIBA")) const _PIFV ___mx_waitable_init = &InitializeKernel32Apis;

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
    DllBase = ::MxGetDllHandle(L"ntdll.dll");
    if (DllBase != NULL)
    {
      fnRtlGetNativeSystemInformation = (lpfnRtlGetNativeSystemInformation)
                                        ::MxGetProcedureAddress(DllBase, "RtlGetNativeSystemInformation");
    }
    ::MxMemSet(&sBasicInfo, 0, sizeof(sBasicInfo));
    if (fnRtlGetNativeSystemInformation != NULL)
      nNtStatus = fnRtlGetNativeSystemInformation(MxSystemBasicInformation, &sBasicInfo, sizeof(sBasicInfo), NULL);
    else
      nNtStatus = ::MxNtQuerySystemInformation(MxSystemProcessorInformation, &sBasicInfo, sizeof(sBasicInfo), NULL);
    _InterlockedExchange(&nProcessorsCount, (NT_SUCCESS(nNtStatus) && sBasicInfo.NumberOfProcessors > 1)
                                            ? (LONG)(sBasicInfo.NumberOfProcessors) : 1);
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

CWindowsEvent::CWindowsEvent() : CWindowsHandle()
{
  return;
}

HRESULT CWindowsEvent::Create(_In_ BOOL bManualReset, _In_ BOOL bInitialState, _In_opt_z_ LPCWSTR szNameW,
                              _In_opt_ LPSECURITY_ATTRIBUTES lpSecAttr, _Out_opt_ LPBOOL lpbAlreadyExists)
{
  HANDLE _h;
  HRESULT hRes;

  if (lpbAlreadyExists != NULL)
    *lpbAlreadyExists = FALSE;
  if (fnCreateEventW != NULL)
  {
    _h = fnCreateEventW(lpSecAttr, bManualReset, bInitialState, szNameW);
    hRes = MX_HRESULT_FROM_LASTERROR();
    if (_h == NULL)
      return hRes;
    if (lpbAlreadyExists != NULL)
      *lpbAlreadyExists = (hRes == MX_E_AlreadyExists) ? TRUE : FALSE;
  }
  else
  {
    MX_OBJECT_ATTRIBUTES sObjAttr;
    MX_UNICODE_STRING usName;
    NTSTATUS nNtStatus;

    MxMemSet(&sObjAttr, 0, sizeof(sObjAttr));
    sObjAttr.Length = (ULONG)sizeof(sObjAttr);
    sObjAttr.Attributes = 0x00000080; //OBJ_OPENIF
    if (szNameW != NULL)
    {
      //get root directory handle
      nNtStatus = GetRootDirHandle(&(sObjAttr.RootDirectory));
      if (!NT_SUCCESS(nNtStatus))
        return MX_HRESULT_FROM_WIN32(::MxRtlNtStatusToDosError(nNtStatus));
      //setup object name
      usName.Buffer = (PWSTR)szNameW;
      for (usName.Length=0; szNameW[usName.Length]!=0; usName.Length++);
      usName.Length *= (USHORT)sizeof(WCHAR);
      usName.MaximumLength = usName.Length;
      sObjAttr.ObjectName = &usName;
    }
    //setup security
    if (lpSecAttr != NULL)
    {
      sObjAttr.SecurityDescriptor = lpSecAttr->lpSecurityDescriptor;
      if (lpSecAttr->bInheritHandle != FALSE)
        sObjAttr.Attributes |= 0x00000002; //OBJ_INHERIT
    }
    //create event
    nNtStatus = ::MxNtCreateEvent(&_h, EVENT_ALL_ACCESS, &sObjAttr,
                                  (bManualReset == FALSE) ? MxSynchronizationEvent : MxNotificationEvent,
                                  bInitialState);
    if (!NT_SUCCESS(nNtStatus))
      return MX_HRESULT_FROM_WIN32(::MxRtlNtStatusToDosError(nNtStatus));
    if (nNtStatus == STATUS_OBJECT_NAME_EXISTS && lpbAlreadyExists != NULL)
      *lpbAlreadyExists = TRUE;
  }
  Attach(_h);
  return S_OK;
}

HRESULT CWindowsEvent::Open(_In_z_ LPCWSTR szNameW, _In_opt_ BOOL bInherit)
{
  HANDLE _h;

  if (fnOpenEventW != NULL)
  {
    _h = fnOpenEventW(EVENT_ALL_ACCESS, bInherit, szNameW);
    if (_h == NULL)
      return MX_HRESULT_FROM_LASTERROR();
  }
  else
  {
    MX_OBJECT_ATTRIBUTES sObjAttr;
    MX_UNICODE_STRING usName;
    NTSTATUS nNtStatus;

    MxMemSet(&sObjAttr, 0, sizeof(sObjAttr));
    sObjAttr.Length = (ULONG)sizeof(sObjAttr);
    sObjAttr.Attributes = 0x00000080; //OBJ_OPENIF
    if (bInherit != FALSE)
      sObjAttr.Attributes |= 0x00000002; //OBJ_INHERIT

    //get root directory handle
    nNtStatus = GetRootDirHandle(&(sObjAttr.RootDirectory));
    if (!NT_SUCCESS(nNtStatus))
      return MX_HRESULT_FROM_WIN32(::MxRtlNtStatusToDosError(nNtStatus));
    //setup object name
    usName.Buffer = (PWSTR)szNameW;
    for (usName.Length=0; szNameW[usName.Length]!=0; usName.Length++);
    usName.Length *= (USHORT)sizeof(WCHAR);
    usName.MaximumLength = usName.Length;
    sObjAttr.ObjectName = &usName;

    //open event
    nNtStatus = ::MxNtOpenEvent(&_h, EVENT_ALL_ACCESS, &sObjAttr);
    if (!NT_SUCCESS(nNtStatus))
      return MX_HRESULT_FROM_WIN32(::MxRtlNtStatusToDosError(nNtStatus));
  }
  Attach(_h);
  return S_OK;
}

BOOL CWindowsEvent::Wait(_In_ DWORD dwTimeoutMs)
{
  LARGE_INTEGER liTimeout, *lpliTimeout;

  if (h == NULL)
    return TRUE;
  lpliTimeout = NULL;
  if (dwTimeoutMs != INFINITE)
  {
    liTimeout.QuadPart = -(LONGLONG)MX_MILLISECONDS_TO_100NS(dwTimeoutMs);
    lpliTimeout = &liTimeout;
  }
  return (::MxNtWaitForSingleObject(h, FALSE, lpliTimeout) == WAIT_OBJECT_0) ? TRUE : FALSE;
}

//-----------------------------------------------------------

CWindowsMutex::CWindowsMutex() : CWindowsHandle()
{
  return;
}

HRESULT CWindowsMutex::Create(_In_opt_z_ LPCWSTR szNameW, _In_ BOOL bInitialOwner,
                              _In_opt_ LPSECURITY_ATTRIBUTES lpSecAttr, _Out_opt_ LPBOOL lpbAlreadyExists)
{
  HANDLE _h;
  HRESULT hRes;

  if (lpbAlreadyExists != NULL)
    *lpbAlreadyExists = FALSE;
  if (fnCreateMutexW != NULL)
  {
    _h = fnCreateMutexW(lpSecAttr, bInitialOwner, szNameW);
    hRes = MX_HRESULT_FROM_LASTERROR();
    if (_h == NULL)
      return hRes;
    if (lpbAlreadyExists != NULL)
      *lpbAlreadyExists = (hRes == MX_E_AlreadyExists) ? TRUE : FALSE;
  }
  else
  {
    MX_OBJECT_ATTRIBUTES sObjAttr;
    MX_UNICODE_STRING usName;
    NTSTATUS nNtStatus;

    MxMemSet(&sObjAttr, 0, sizeof(sObjAttr));
    sObjAttr.Length = (ULONG)sizeof(sObjAttr);
    sObjAttr.Attributes = 0x00000080; //OBJ_OPENIF
    if (szNameW != NULL)
    {
      //get root directory handle
      nNtStatus = GetRootDirHandle(&(sObjAttr.RootDirectory));
      if (!NT_SUCCESS(nNtStatus))
        return MX_HRESULT_FROM_WIN32(::MxRtlNtStatusToDosError(nNtStatus));
      //setup object name
      usName.Buffer = (PWSTR)szNameW;
      for (usName.Length=0; szNameW[usName.Length]!=0; usName.Length++);
      usName.Length *= (USHORT)sizeof(WCHAR);
      usName.MaximumLength = usName.Length;
      sObjAttr.ObjectName = &usName;
    }
    //setup security
    if (lpSecAttr != NULL)
    {
      sObjAttr.SecurityDescriptor = lpSecAttr->lpSecurityDescriptor;
      if (lpSecAttr->bInheritHandle != FALSE)
        sObjAttr.Attributes |= 0x00000002; //OBJ_INHERIT
    }
    //create mutant
    nNtStatus = ::MxNtCreateMutant(&_h, MUTANT_ALL_ACCESS, &sObjAttr, (BOOLEAN)bInitialOwner);
    if (!NT_SUCCESS(nNtStatus))
      return MX_HRESULT_FROM_WIN32(::MxRtlNtStatusToDosError(nNtStatus));
    if (nNtStatus == STATUS_OBJECT_NAME_EXISTS && lpbAlreadyExists != NULL)
      *lpbAlreadyExists = TRUE;
  }
  Attach(_h);
  return S_OK;
}

HRESULT CWindowsMutex::Open(_In_opt_z_ LPCWSTR szNameW, _In_ BOOL bQueryOnly, _In_opt_ BOOL bInherit)
{
  HANDLE _h = NULL;

  if (fnOpenMutexW != NULL)
  {
    _h = fnOpenMutexW(MUTEX_ALL_ACCESS, bInherit, szNameW);
    if (_h == NULL)
      return MX_HRESULT_FROM_LASTERROR();
  }
  else
  {
    MX_OBJECT_ATTRIBUTES sObjAttr;
    MX_UNICODE_STRING usName;
    NTSTATUS nNtStatus;

    MxMemSet(&sObjAttr, 0, sizeof(sObjAttr));
    sObjAttr.Length = (ULONG)sizeof(sObjAttr);
    sObjAttr.Attributes = 0x00000080; //OBJ_OPENIF
    if (bInherit != FALSE)
      sObjAttr.Attributes |= 0x00000002; //OBJ_INHERIT
    if (szNameW != NULL)
    {
      //get root directory handle
      nNtStatus = GetRootDirHandle(&(sObjAttr.RootDirectory));
      if (!NT_SUCCESS(nNtStatus))
        return MX_HRESULT_FROM_WIN32(::MxRtlNtStatusToDosError(nNtStatus));
      //setup object name
      usName.Buffer = (PWSTR)szNameW;
      for (usName.Length=0; szNameW[usName.Length]!=0; usName.Length++);
      usName.Length *= (USHORT)sizeof(WCHAR);
      usName.MaximumLength = usName.Length;
      sObjAttr.ObjectName = &usName;
    }
    //open mutant
    nNtStatus = ::MxNtOpenMutant(&_h, (bQueryOnly == FALSE) ? MUTANT_ALL_ACCESS : (STANDARD_RIGHTS_READ|SYNCHRONIZE),
                                 &sObjAttr);
    if (!NT_SUCCESS(nNtStatus))
      return MX_HRESULT_FROM_WIN32(::MxRtlNtStatusToDosError(nNtStatus));
  }
  Attach(_h);
  return S_OK;
}

//-----------------------------------------------------------

VOID FastLock_Initialize(_Out_ LONG volatile *lpnLock)
{
  _InterlockedExchange(lpnLock, 0);
  return;
}

VOID FastLock_Enter(_Inout_ LONG volatile *lpnLock)
{
  LONG nTid = (LONG)::MxGetCurrentThreadId();

  while (_InterlockedCompareExchange(lpnLock, nTid, 0) != 0)
    _YieldProcessor();
  return;
}

BOOL FastLock_TryEnter(_Inout_ LONG volatile *lpnLock)
{
  LONG nTid = (LONG)::MxGetCurrentThreadId();

  return (_InterlockedCompareExchange(lpnLock, nTid, 0) == 0) ? TRUE : FALSE;
}

VOID FastLock_Exit(_Inout_ LONG volatile *lpnLock)
{
  _InterlockedExchange(lpnLock, 0);
  return;
}

DWORD FastLock_IsActive(_Inout_ LONG volatile *lpnLock)
{
  return (DWORD)__InterlockedRead(lpnLock);
}

VOID FastLock_Bitmask_Enter(_Inout_ LONG volatile *lpnLock, _In_ int nBitIndex)
{
  LONG nMask = 1L << nBitIndex;

  while ((_InterlockedOr(lpnLock, nMask) & nMask) != 0)
    _YieldProcessor();
  return;
}

BOOL FastLock_Bitmask_TryEnter(_Inout_ LONG volatile *lpnLock, _In_ int nBitIndex)
{
  LONG nMask = 1L << nBitIndex;

  return ((_InterlockedOr(lpnLock, nMask) & nMask) == 0) ? TRUE : FALSE;
}

VOID FastLock_Bitmask_Exit(_Inout_ LONG volatile *lpnLock, _In_ int nBitIndex)
{
  LONG nMask = 1L << nBitIndex;

  _InterlockedAnd(lpnLock, ~nMask);
  return;
}

BOOL FastLock_Bitmask_IsActive(_Inout_ LONG volatile *lpnLock, _In_ int nBitIndex)
{
  LONG nMask = 1L << nBitIndex;

  return ((__InterlockedRead(lpnLock) & nMask) != 0) ? TRUE : FALSE;
}

//-----------------------------------------------------------

VOID SlimRWL_Initialize(_In_ LPRWLOCK lpLock)
{
  if (fnInitializeSRWLock != NULL)
    fnInitializeSRWLock(&(lpLock->sOsLock));
  else
    _InterlockedExchange(&(lpLock->nValue), 0);
  return;
}

BOOL SlimRWL_TryAcquireShared(_In_ LPRWLOCK lpLock)
{
  if (fnInitializeSRWLock != NULL)
  {
    if (fnTryAcquireSRWLockShared(&(lpLock->sOsLock)) == FALSE)
      return FALSE;
  }
  else
  {
    LONG initVal, newVal;

    newVal = __InterlockedRead(&(lpLock->nValue));
    do
    {
      initVal = newVal;
      if ((initVal & 0x80000000L) != 0)
        return FALSE; //a writer is active
      MX_ASSERT(((ULONG)initVal & 0x7FFFFFFFUL) != 0x7FFFFFFFUL); //check overflows
      newVal = _InterlockedCompareExchange(&(lpLock->nValue), initVal + 1, initVal);
    }
    while (newVal != initVal);
  }
  return TRUE;
}

VOID SlimRWL_AcquireShared(_In_ LPRWLOCK lpLock)
{
  if (fnInitializeSRWLock != NULL)
  {
    fnAcquireSRWLockShared(&(lpLock->sOsLock));
  }
  else
  {
    while (SlimRWL_TryAcquireShared(lpLock) == FALSE)
      _YieldProcessor();
  }
#ifdef SHOW_SLIMRWL_DEBUG_INFO
  DebugPrint("SlimRWL_AcquireShared: %lu\n", MxGetCurrentThreadId());
#endif //SHOW_SLIMRWL_DEBUG_INFO
  return;
}

VOID SlimRWL_ReleaseShared(_In_ LPRWLOCK lpLock)
{ 
  if (fnInitializeSRWLock != NULL)
  {
    fnReleaseSRWLockShared(&(lpLock->sOsLock));
  }
  else
  {
    LONG initVal, newVal;

    newVal = __InterlockedRead(&(lpLock->nValue));
    do
    {
      initVal = newVal;
      MX_ASSERT((initVal & 0x7FFFFFFFL) != 0);
      newVal = (initVal & 0x80000000L) | ((initVal & 0x7FFFFFFFL) - 1);
      newVal = _InterlockedCompareExchange(&(lpLock->nValue), newVal, initVal);
    }
    while (newVal != initVal);
  }
#ifdef SHOW_SLIMRWL_DEBUG_INFO
  DebugPrint("SlimRWL_ReleaseShared: %lu\n", MxGetCurrentThreadId());
#endif //SHOW_SLIMRWL_DEBUG_INFO
  return;
}

BOOL SlimRWL_TryAcquireExclusive(_In_ LPRWLOCK lpLock)
{
  if (fnInitializeSRWLock != NULL)
  {
    if (fnTryAcquireSRWLockExclusive(&(lpLock->sOsLock)) == FALSE)
      return FALSE;
  }
  else
  {
    LONG initVal, newVal;

    newVal = __InterlockedRead(&(lpLock->nValue));
    do
    {
      initVal = newVal;
      if ((initVal & 0x80000000L) != 0)
        return FALSE; //another writer is active or waiting
      newVal = _InterlockedCompareExchange(&(lpLock->nValue), initVal | 0x80000000L, initVal);
    }
    while (newVal != initVal);

    //wait until no readers
    while ((__InterlockedRead(&(lpLock->nValue)) & 0x7FFFFFFFL) != 0)
      _YieldProcessor();
  }
  return TRUE;
}

VOID SlimRWL_AcquireExclusive(_In_ LPRWLOCK lpLock)
{
  if (fnInitializeSRWLock != NULL)
  {
    fnAcquireSRWLockExclusive(&(lpLock->sOsLock));
  }
  else
  {
    while (SlimRWL_TryAcquireExclusive(lpLock) == FALSE)
      _YieldProcessor();
  }
#ifdef SHOW_SLIMRWL_DEBUG_INFO
  DebugPrint("SlimRWL_AcquireExclusive: %lu\n", MxGetCurrentThreadId());
#endif //SHOW_SLIMRWL_DEBUG_INFO
  return;
}

VOID SlimRWL_ReleaseExclusive(_In_ LPRWLOCK lpLock)
{
  if (fnInitializeSRWLock != NULL)
  {
    fnReleaseSRWLockExclusive(&(lpLock->sOsLock));
  }
  else
  {
    MX_ASSERT(__InterlockedRead(&(lpLock->nValue)) == 0x80000000L);
    _InterlockedExchange(&(lpLock->nValue), 0);
  }
#ifdef SHOW_SLIMRWL_DEBUG_INFO
  DebugPrint("SlimRWL_ReleaseExclusive: %lu\n", MxGetCurrentThreadId());
#endif //SHOW_SLIMRWL_DEBUG_INFO
  return;
}

//-----------------------------------------------------------

VOID RundownProt_Initialize(_In_ LONG volatile *lpnValue)
{
  _InterlockedExchange(lpnValue, 0);
  return;
}

BOOL RundownProt_Acquire(_In_ LONG volatile *lpnValue)
{
  LONG initVal, newVal;

  newVal = __InterlockedRead(lpnValue);
  do
  {
    initVal = newVal;
    if ((initVal & 0x80000000L) != 0)
      return FALSE;
    newVal = _InterlockedCompareExchange(lpnValue, initVal + 1, initVal);
  }
  while (newVal != initVal);
  return TRUE;
}

VOID RundownProt_Release(_In_ LONG volatile *lpnValue)
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

VOID RundownProt_WaitForRelease(_In_ LONG volatile *lpnValue)
{
  //mark rundown protection as shutting down
  _InterlockedOr(lpnValue, 0x80000000L);
  //wait until no locks active
  while (__InterlockedRead(lpnValue) != 0x80000000L)
    ::MxSleep(10);
  return;
}

} //namespace MX

//-----------------------------------------------------------

#define _GETAPI(_api) fn##_api = (lpfn##_api)::MxGetProcedureAddress(hDll, #_api)
static int __cdecl InitializeKernel32Apis()
{
  PVOID hDll;

  hDll = ::MxGetDllHandle(L"kernelbase.dll");
  if (hDll != NULL)
  {
    _GETAPI(CreateEventW);
    _GETAPI(OpenEventW);
    if (fnCreateEventW == NULL || fnOpenEventW == NULL)
    {
      fnCreateEventW = NULL;
      fnOpenEventW = NULL;
    }
    _GETAPI(CreateMutexW);
    _GETAPI(OpenMutexW);
    if (fnCreateMutexW == NULL || fnOpenMutexW == NULL)
    {
      fnCreateMutexW = NULL;
      fnOpenMutexW = NULL;
    }

    _GETAPI(InitializeSRWLock);
    _GETAPI(ReleaseSRWLockExclusive);
    _GETAPI(ReleaseSRWLockShared);
    _GETAPI(AcquireSRWLockExclusive);
    _GETAPI(AcquireSRWLockShared);
    _GETAPI(TryAcquireSRWLockExclusive);
    _GETAPI(TryAcquireSRWLockShared);
    if (fnInitializeSRWLock == NULL ||
        fnReleaseSRWLockExclusive == NULL || fnReleaseSRWLockShared == NULL ||
        fnAcquireSRWLockExclusive == NULL || fnAcquireSRWLockShared == NULL ||
        fnTryAcquireSRWLockExclusive == NULL || fnTryAcquireSRWLockShared == NULL)
    {
      fnInitializeSRWLock = NULL;
      fnReleaseSRWLockExclusive = NULL;    fnReleaseSRWLockShared = NULL;
      fnAcquireSRWLockExclusive = NULL;    fnAcquireSRWLockShared = NULL;
      fnTryAcquireSRWLockExclusive = NULL; fnTryAcquireSRWLockShared = NULL;
    }
  }

  hDll = ::MxGetDllHandle(L"kernel32.dll");
  if (hDll != NULL)
  {
    if (fnCreateEventW == NULL)
    {
      _GETAPI(CreateEventW);
      _GETAPI(OpenEventW);
      if (fnCreateEventW == NULL || fnOpenEventW == NULL)
      {
        fnCreateEventW = NULL;
        fnOpenEventW = NULL;
      }
    }

    if (fnCreateMutexW == NULL)
    {
      _GETAPI(CreateMutexW);
      _GETAPI(OpenMutexW);
      if (fnCreateMutexW == NULL || fnOpenMutexW == NULL)
      {
        fnCreateMutexW = NULL;
        fnOpenMutexW = NULL;
      }
    }

    if (fnInitializeSRWLock == NULL)
    {
      _GETAPI(InitializeSRWLock);
      _GETAPI(ReleaseSRWLockExclusive);
      _GETAPI(ReleaseSRWLockShared);
      _GETAPI(AcquireSRWLockExclusive);
      _GETAPI(AcquireSRWLockShared);
      _GETAPI(TryAcquireSRWLockExclusive);
      _GETAPI(TryAcquireSRWLockShared);
      if (fnInitializeSRWLock == NULL ||
          fnReleaseSRWLockExclusive == NULL || fnReleaseSRWLockShared == NULL ||
          fnAcquireSRWLockExclusive == NULL || fnAcquireSRWLockShared == NULL ||
          fnTryAcquireSRWLockExclusive == NULL || fnTryAcquireSRWLockShared == NULL)
      {
        fnInitializeSRWLock = NULL;
        fnReleaseSRWLockExclusive = NULL;    fnReleaseSRWLockShared = NULL;
        fnAcquireSRWLockExclusive = NULL;    fnAcquireSRWLockShared = NULL;
        fnTryAcquireSRWLockExclusive = NULL; fnTryAcquireSRWLockShared = NULL;
      }
    }
  }

  //done
  return 0;
}
#undef _GETAPI

static NTSTATUS GetRootDirHandle(_Out_ PHANDLE lphRootDir)
{
  static HANDLE volatile hRootDirHandle = NULL;
  RTL_OSVERSIONINFOW sOviW;
  MX_OBJECT_ATTRIBUTES sObjAttr;
  MX_UNICODE_STRING usName;
  HANDLE _h, hProcessToken = NULL, hThreadToken = NULL;
  DWORD dwProcSessionId = 0, dwIsAppContainer = 0, dwUsesPrivateNamespace = 0, dwRetLength;
  WCHAR szBufW[256];
  NTSTATUS nNtStatus;

  *lphRootDir = __InterlockedReadPointer(&hRootDirHandle);
  if ((*lphRootDir) != NULL)
    return STATUS_SUCCESS;

  _h = NULL;

  //get OS version
  ::MxMemSet(&sOviW, 0, sizeof(sOviW));
  sOviW.dwOSVersionInfoSize = (DWORD)sizeof(sOviW);
  ::MxRtlGetVersion(&sOviW);

  nNtStatus = ::MxNtOpenThreadToken(MX_CURRENTTHREAD, TOKEN_IMPERSONATE, FALSE, &hThreadToken);
  if (NT_SUCCESS(nNtStatus))
  {
    nNtStatus = ::MxNtSetInformationThread(MX_CURRENTTHREAD, MxThreadImpersonationToken, &_h, (ULONG)sizeof(HANDLE));
    if (!NT_SUCCESS(nNtStatus))
      goto done;
  }

  //check name
  usName.Buffer = (PWSTR)L"\\BaseNamedObjects";
  if (sOviW.dwMajorVersion >= 6)
  {
    nNtStatus = ::MxNtOpenProcessToken(MX_CURRENTPROCESS, TOKEN_QUERY, &hProcessToken);
    if (!NT_SUCCESS(nNtStatus))
      goto done;
    nNtStatus =::MxNtQueryInformationToken(hProcessToken, TokenSessionId, &dwProcSessionId,
                                           (ULONG)sizeof(dwProcSessionId), &dwRetLength);
    if (!NT_SUCCESS(nNtStatus))
      goto done;
    if (sOviW.dwMajorVersion > 6 || sOviW.dwMinorVersion >= 2)
    {
      nNtStatus =::MxNtQueryInformationToken(hProcessToken, (TOKEN_INFORMATION_CLASS)29/*TokenIsAppContainer*/,
                                             &dwIsAppContainer, (ULONG)sizeof(dwIsAppContainer), &dwRetLength);
      if (!NT_SUCCESS(nNtStatus))
        goto done;
      nNtStatus =::MxNtQueryInformationToken(hProcessToken, (TOKEN_INFORMATION_CLASS)42/*TokenPrivateNameSpace*/,
                                             &dwUsesPrivateNamespace, (ULONG)sizeof(dwUsesPrivateNamespace),
                                             &dwRetLength);
      if (!NT_SUCCESS(nNtStatus))
        goto done;
    }
    if (dwIsAppContainer != 0)
    {
      ::mx_swprintf_s(szBufW, 256, L"\\Sessions\\%ld\\AppContainerNamedObjects", dwProcSessionId);
      usName.Buffer = szBufW;
    }
  }
  for (usName.Length = 0; usName.Buffer[usName.Length] != 0; usName.Length++);
  usName.Length *= 2;

  //initialize object attributes
  ::MxMemSet(&sObjAttr, 0, sizeof(sObjAttr));
  sObjAttr.Length = (ULONG)sizeof(sObjAttr);
  sObjAttr.Attributes = OBJ_CASE_INSENSITIVE;
  sObjAttr.ObjectName = &usName;
  usName.MaximumLength = usName.Length;
  nNtStatus = ::MxNtOpenDirectoryObject(&_h, 0x0F, &sObjAttr); //DIRECTORY_CREATE_OBJECT|DIRECTORY_TRAVERSE
  if (NT_SUCCESS(nNtStatus) && dwUsesPrivateNamespace != 0)
  {
    typedef NTSTATUS (NTAPI *lpfnRtlConvertSidToUnicodeString)(_Out_ PMX_UNICODE_STRING UnicodeString,
                                                               _In_ PSID Sid, _In_ BOOLEAN AllocateDestinationString);
    typedef VOID (NTAPI *lpfnRtlFreeUnicodeString)(_In_ PMX_UNICODE_STRING UnicodeString);
    PVOID nNtDll;
    lpfnRtlConvertSidToUnicodeString fnRtlConvertSidToUnicodeString = NULL;
    lpfnRtlFreeUnicodeString fnRtlFreeUnicodeString = NULL;
    BYTE aSid[SECURITY_MAX_SID_SIZE];

    nNtDll = ::MxGetDllHandle(L"ntdll.dll");
    if (nNtDll != NULL)
    {
      fnRtlConvertSidToUnicodeString =
        (lpfnRtlConvertSidToUnicodeString)::MxGetProcedureAddress(nNtDll, "RtlConvertSidToUnicodeString");
      fnRtlFreeUnicodeString = (lpfnRtlFreeUnicodeString)::MxGetProcedureAddress(nNtDll, "RtlFreeUnicodeString");
    }
    if (fnRtlConvertSidToUnicodeString != NULL && fnRtlFreeUnicodeString != NULL)
    {
      MX_UNICODE_STRING usName = { 0 };

      nNtStatus = ::MxNtQueryInformationToken(hProcessToken, TokenUser, &aSid, SECURITY_MAX_SID_SIZE, &dwRetLength);
      if (NT_SUCCESS(nNtStatus))
      {
        nNtStatus = fnRtlConvertSidToUnicodeString(&usName, (PSID)aSid, TRUE);
        if (NT_SUCCESS(nNtStatus))
        {
          HANDLE _hChild = NULL;

          //initialize object attributes
          ::MxMemSet(&sObjAttr, 0, sizeof(sObjAttr));
          sObjAttr.Length = (ULONG)sizeof(sObjAttr);
          sObjAttr.Attributes = OBJ_CASE_INSENSITIVE;
          sObjAttr.ObjectName = &usName;
          sObjAttr.RootDirectory = _h;
          nNtStatus = ::MxNtOpenDirectoryObject(&_hChild, 0x0F, &sObjAttr);
          if (NT_SUCCESS(nNtStatus))
          {
            ::MxNtClose(_h);
            _h = _hChild;
          }
          //free string
          fnRtlFreeUnicodeString(&usName);
        }
      }
    }
    else
    {
      nNtStatus = STATUS_PROCEDURE_NOT_FOUND;
    }
  }

done:
  if (hThreadToken != NULL)
  {
    ::MxNtSetInformationThread(MX_CURRENTTHREAD, MxThreadImpersonationToken, &hThreadToken, (ULONG)sizeof(HANDLE));
    ::MxNtClose(hThreadToken);
  }
  if (hProcessToken != NULL)
  {
    ::MxNtClose(hProcessToken);
  }
  if (NT_SUCCESS(nNtStatus))
  {
#pragma warning(suppress: 28112) //NOTE: This suppress is an analyzer bug
    if (_InterlockedCompareExchangePointer(&hRootDirHandle, _h, NULL) != NULL)
    {
      ::MxNtClose(_h);
      _h = __InterlockedReadPointer(&hRootDirHandle);
    }
  }
  else
  {
    if (_h != NULL)
    {
      ::MxNtClose(_h);
      _h = NULL;
    }
  }
  *lphRootDir = _h;
  return nNtStatus;
}
