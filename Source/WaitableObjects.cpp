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

//-----------------------------------------------------------

typedef NTSTATUS (NTAPI *lpfnRtlGetNativeSystemInformation)(__in MX_SYSTEM_INFORMATION_CLASS SystemInformationClass,
                                               __inout PVOID SystemInformation, __in ULONG SystemInformationLength,
                                               __out_opt PULONG ReturnLength);

typedef HANDLE (WINAPI *lpfnCreateEventW)(__in_opt LPSECURITY_ATTRIBUTES lpEventAttributes, __in BOOL bManualReset,
                                          __in BOOL bInitialState, __in_opt LPCWSTR lpName);
typedef HANDLE (WINAPI *lpfnOpenEventW)(__in DWORD dwDesiredAccess, __in BOOL bInheritHandle, __in LPCWSTR lpName);
typedef HANDLE (WINAPI *lpfnCreateMutexW)(__in_opt LPSECURITY_ATTRIBUTES lpMutexAttributes, __in BOOL bInitialOwner,
                                          __in_opt LPCWSTR lpName);
typedef HANDLE (WINAPI *lpfnOpenMutexW)(__in DWORD dwDesiredAccess, __in BOOL bInheritHandle, __in LPCWSTR lpName);

//-----------------------------------------------------------

static LONG volatile nProcessorsCount = 0;

static lpfnCreateEventW fnCreateEventW = NULL;
static lpfnOpenEventW fnOpenEventW = NULL;
static lpfnCreateMutexW fnCreateMutexW = NULL;
static lpfnOpenMutexW fnOpenMutexW = NULL;

//-----------------------------------------------------------

static VOID InitializeKernel32Apis();
static NTSTATUS GetRootDirHandle(__out PHANDLE lphRootDir);

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
      fnRtlGetNativeSystemInformation =
        (lpfnRtlGetNativeSystemInformation)MxGetProcedureAddress(DllBase, "RtlGetNativeSystemInformation");
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

CWindowsEvent::CWindowsEvent() : CWindowsHandle()
{
  InitializeKernel32Apis();
  return;
}

HRESULT CWindowsEvent::Create(__in BOOL bManualReset, __in BOOL bInitialState, __in_z_opt LPCWSTR szNameW,
                              __in_opt LPSECURITY_ATTRIBUTES lpSecAttr, __out_opt LPBOOL lpbAlreadyExists)
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

    MemSet(&sObjAttr, 0, sizeof(sObjAttr));
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

HRESULT CWindowsEvent::Open(__in_z_opt LPCWSTR szNameW, __in_opt BOOL bInherit)
{
  HANDLE _h;
  HRESULT hRes;

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

    MemSet(&sObjAttr, 0, sizeof(sObjAttr));
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
    //open event
    nNtStatus = ::MxNtOpenEvent(&_h, EVENT_ALL_ACCESS, &sObjAttr);
    if (!NT_SUCCESS(nNtStatus))
      return MX_HRESULT_FROM_WIN32(::MxRtlNtStatusToDosError(nNtStatus));
  }
  Attach(_h);
  return S_OK;
}

BOOL CWindowsEvent::Wait(__in DWORD dwTimeoutMs)
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
  InitializeKernel32Apis();
  return;
}

HRESULT CWindowsMutex::Create(__in_z_opt LPCWSTR szNameW, __in BOOL bInitialOwner,
                              __in_opt LPSECURITY_ATTRIBUTES lpSecAttr, __out_opt LPBOOL lpbAlreadyExists)
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

    MemSet(&sObjAttr, 0, sizeof(sObjAttr));
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

HRESULT CWindowsMutex::Open(__in_z_opt LPCWSTR szNameW, __in BOOL bQueryOnly, __in_opt BOOL bInherit)
{
  HANDLE _h;
  HRESULT hRes;

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
    HANDLE _h;

    MemSet(&sObjAttr, 0, sizeof(sObjAttr));
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

//-----------------------------------------------------------

static VOID InitializeKernel32Apis()
{
  static LONG volatile nInitialized = 0;
  static LONG volatile nMutex = 0;

  if (MX::__InterlockedRead(&nInitialized) == 0)
  {
    MX::CFastLock cLock(&nMutex);

    if (MX::__InterlockedRead(&nInitialized) == 0)
    {
      PVOID hDll;

      hDll = ::MxGetDllHandle(L"kernelbase.dll");
      if (hDll != NULL)
      {
        fnCreateEventW = (lpfnCreateEventW)::MxGetProcedureAddress(hDll, "CreateEventW");
        fnOpenEventW = (lpfnOpenEventW)::MxGetProcedureAddress(hDll, "OpenEventW");
        if (fnCreateEventW == NULL || fnOpenEventW == NULL)
        {
          fnCreateEventW = NULL;
          fnOpenEventW = NULL;
        }
        fnCreateMutexW = (lpfnCreateMutexW)::MxGetProcedureAddress(hDll, "CreateMutexW");
        fnOpenMutexW = (lpfnOpenMutexW)::MxGetProcedureAddress(hDll, "OpenMutexW");
        if (fnCreateMutexW == NULL || fnOpenMutexW == NULL)
        {
          fnCreateMutexW = NULL;
          fnOpenMutexW = NULL;
        }
      }

      hDll = ::MxGetDllHandle(L"kernel32.dll");
      if (hDll != NULL)
      {
        if (fnCreateEventW == NULL)
        {
          fnCreateEventW = (lpfnCreateEventW)::MxGetProcedureAddress(hDll, "CreateEventW");
          fnOpenEventW = (lpfnOpenEventW)::MxGetProcedureAddress(hDll, "OpenEventW");
          if (fnCreateEventW == NULL || fnOpenEventW == NULL)
          {
            fnCreateEventW = NULL;
            fnOpenEventW = NULL;
          }
        }
        if (fnCreateMutexW == NULL)
        {
          fnCreateMutexW = (lpfnCreateMutexW)::MxGetProcedureAddress(hDll, "CreateMutexW");
          fnOpenMutexW = (lpfnOpenMutexW)::MxGetProcedureAddress(hDll, "OpenMutexW");
          if (fnCreateMutexW == NULL || fnOpenMutexW == NULL)
          {
            fnCreateMutexW = NULL;
            fnOpenMutexW = NULL;
          }
        }
      }
    }
  }
  return;
}

static NTSTATUS GetRootDirHandle(__out PHANDLE lphRootDir)
{
  static HANDLE volatile hRootDirHandle = NULL;
  RTL_OSVERSIONINFOW sOviW;
  MX_OBJECT_ATTRIBUTES sObjAttr;
  MX_UNICODE_STRING usName;
  HANDLE _h, hProcessToken = NULL, hThreadToken = NULL;
  DWORD dwProcSessionId = 0, dwIsAppContainer = 0, dwUsesPrivateNamespace = 0, dwRetLength;
  WCHAR szBufW[256];
  NTSTATUS nNtStatus;

  *lphRootDir = MX::__InterlockedReadPointer(&hRootDirHandle);
  if ((*lphRootDir) != NULL)
    return STATUS_SUCCESS;

  _h = NULL;

  //get OS version
  MX::MemSet(&sOviW, 0, sizeof(sOviW));
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
  usName.Buffer = L"\\BaseNamedObjects";
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
  MX::MemSet(&sObjAttr, 0, sizeof(sObjAttr));
  sObjAttr.Length = (ULONG)sizeof(sObjAttr);
  sObjAttr.Attributes = OBJ_CASE_INSENSITIVE;
  sObjAttr.ObjectName = &usName;
  usName.MaximumLength = usName.Length;
  nNtStatus = ::MxNtOpenDirectoryObject(&_h, 0x0F, &sObjAttr); //DIRECTORY_CREATE_OBJECT|DIRECTORY_TRAVERSE
  if (NT_SUCCESS(nNtStatus) && dwUsesPrivateNamespace != 0)
  {
    typedef NTSTATUS (NTAPI *lpfnRtlConvertSidToUnicodeString)(__out PMX_UNICODE_STRING UnicodeString,
                                                               __in PSID Sid, __in BOOLEAN AllocateDestinationString);
    typedef VOID (NTAPI *lpfnRtlFreeUnicodeString)(__in PMX_UNICODE_STRING UnicodeString);
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
          MX::MemSet(&sObjAttr, 0, sizeof(sObjAttr));
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
    if (_InterlockedCompareExchangePointer(&hRootDirHandle, _h, NULL) != NULL)
    {
      ::MxNtClose(_h);
      _h = MX::__InterlockedReadPointer(&hRootDirHandle);
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
