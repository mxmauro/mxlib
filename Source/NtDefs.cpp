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
#include "..\Include\NtDefs.h"
#include "..\Include\MemoryObjects.h"
#include "..\Include\WaitableObjects.h"
#include <intrin.h>
#include <winternl.h>
#include <stdio.h>

#pragma intrinsic (_InterlockedExchange)

//-----------------------------------------------------------

#ifndef OBJ_INHERIT
  #define OBJ_INHERIT             0x00000002L
#endif //!OBJ_INHERIT

#define __CURRENTPROCESS   ((HANDLE)(LONG_PTR)-1)

//-----------------------------------------------------------

#pragma pack(8)
typedef struct {
  LONG ExitStatus;
  ULONG PebBaseAddress;
  ULONG AffinityMask;
  LONG BasePriority;
  ULONG UniqueProcessId;
  ULONG InheritedFromUniqueProcessId;
} __PROCESS_BASIC_INFORMATION32;

typedef struct {
  LONG ExitStatus;
  ULONG _dummy1;
  ULONGLONG PebBaseAddress;
  ULONGLONG AffinityMask;
  LONG BasePriority;
  ULONG _dummy2;
  ULONGLONG UniqueProcessId;
  ULONGLONG InheritedFromUniqueProcessId;
} __PROCESS_BASIC_INFORMATION64;
#pragma pack()

//-----------------------------------------------------------

typedef NTSTATUS (NTAPI *lpfnRtlGetNativeSystemInformation)(_In_ MX_SYSTEM_INFORMATION_CLASS SystemInformationClass,
                                               _Inout_ PVOID SystemInformation, _In_ ULONG SystemInformationLength,
                                               _Out_opt_ PULONG ReturnLength);

typedef int (__cdecl *lpfn_vsnprintf)(_Out_writes_z_(nMaxCount) char *lpDest, _In_ size_t nMaxCount,
                                      _In_z_ _Printf_format_string_ const char *szFormatA, _In_ va_list lpArgList);
typedef int (__cdecl *lpfn_vsnwprintf)(_Out_writes_z_(nMaxCount) wchar_t *lpDest, _In_ size_t nMaxCount,
                                       _In_z_ _Printf_format_string_ const wchar_t *szFormatW, _In_ va_list lpArgList);

typedef BOOLEAN (NTAPI *lpfnRtlDosPathNameToNtPathName_U)(_In_opt_z_ PCWSTR DosPathName,
                                            _Out_ PMX_UNICODE_STRING NtPathName, _Out_opt_ PCWSTR *NtFileNamePart,
                                            _Out_opt_ PVOID DirectoryInfo);

//-----------------------------------------------------------

static lpfn_vsnprintf fn_vsnprintf = NULL;
static lpfn_vsnwprintf fn_vsnwprintf = NULL;
static lpfnRtlDosPathNameToNtPathName_U fnRtlDosPathNameToNtPathName_U = NULL;

//-----------------------------------------------------------

static BOOL RemoteCompareStringW(_In_ HANDLE hProcess, _In_z_ LPCWSTR szRemoteNameW, _In_z_ LPCWSTR szLocalNameW,
                                 _In_ SIZE_T nNameLen);

//-----------------------------------------------------------

extern "C" {

PVOID MxGetProcessHeap()
{
  LPBYTE lpPtr;
  PVOID h;

#if defined _M_IX86
  lpPtr = (LPBYTE)__readfsdword(0x18); //get TEB
  lpPtr = *((LPBYTE*)(lpPtr+0x30));    //TEB.Peb
  h = *((PVOID*)(lpPtr+0x18));         //PEB.ProcessHeap
#elif defined _M_X64
  lpPtr = (LPBYTE)__readgsqword(0x30); //get TEB
  lpPtr = *((LPBYTE*)(lpPtr+0x60));    //TEB.Peb
  h = *((PVOID*)(lpPtr+0x30));         //PEB.ProcessHeap
#endif
  return h;
}

LPBYTE MxGetPeb()
{
#if defined(_M_IX86)
  LPBYTE lpPtr = (LPBYTE)__readfsdword(0x18); //get TEB
  return *((LPBYTE*)(lpPtr+0x30));            //TEB.Peb
#elif defined(_M_X64)
  LPBYTE lpPtr = (LPBYTE)__readgsqword(0x30); //get TEB
  return *((LPBYTE*)(lpPtr+0x60));            //TEB.Peb
#endif
};

LPBYTE MxGetRemotePeb(_In_ HANDLE hProcess)
{
  ULONG k;

  if (hProcess == NULL || hProcess == __CURRENTPROCESS)
    return MxGetPeb();
  switch (MxGetProcessorArchitecture())
  {
    case PROCESSOR_ARCHITECTURE_INTEL:
#if defined(_M_IX86)
      {
      __PROCESS_BASIC_INFORMATION32 sPbi32;

      if (NT_SUCCESS(::MxNtQueryInformationProcess(hProcess, MxProcessBasicInformation, &sPbi32,
                                                   (ULONG)sizeof(sPbi32), &k)))
        return (LPBYTE)(sPbi32.PebBaseAddress);
      }
#endif //_M_IX86
      break;

    case PROCESSOR_ARCHITECTURE_AMD64:
    case PROCESSOR_ARCHITECTURE_IA64:
    case PROCESSOR_ARCHITECTURE_ALPHA64:
      {
      ULONG_PTR nWow64;
#if defined (_M_X64)
      __PROCESS_BASIC_INFORMATION64 sPbi64;
#endif //_M_X64

      if (NT_SUCCESS(::MxNtQueryInformationProcess(hProcess, MxProcessWow64Information, &nWow64, sizeof(nWow64),
                                                   NULL)) && nWow64 != 0)
        return (LPBYTE)nWow64;
#if defined (_M_X64)
      if (NT_SUCCESS(::MxNtQueryInformationProcess(hProcess, MxProcessBasicInformation, &sPbi64,
                                                   (ULONG)sizeof(sPbi64), &k)))
        return (LPBYTE)(sPbi64.PebBaseAddress);
#endif //_M_X64
      }
      break;
  }
  return NULL;
}

LPBYTE MxGetTeb()
{
#if defined(_M_IX86)
  return (LPBYTE)__readfsdword(0x18); //get TEB
#elif defined(_M_X64)
  return (LPBYTE)__readgsqword(0x30); //get TEB
#endif
};

VOID MxSetLastWin32Error(_In_ DWORD dwOsErr)
{
#if defined(_M_IX86)
  *((LPDWORD)(MxGetTeb()+0x34)) = dwOsErr;
#elif defined(_M_X64)
  *((LPDWORD)(MxGetTeb()+0x68)) = dwOsErr;
#endif
  return;
};

DWORD MxGetLastWin32Error()
{
#if defined(_M_IX86)
  return *((LPDWORD)(MxGetTeb()+0x34));
#elif defined(_M_X64)
  return *((LPDWORD)(MxGetTeb()+0x68));
#endif
};

int mx_sprintf_s(_Out_writes_z_(nMaxCount) char *lpDest, _In_ size_t nMaxCount,
                 _In_z_ _Printf_format_string_ const char *szFormatA, ...)
{
  va_list argptr;
  int ret;

  va_start(argptr, szFormatA);
  ret = mx_vsnprintf(lpDest, nMaxCount, szFormatA, argptr);
  va_end(argptr);
  return ret;
}

int mx_vsnprintf(_Out_writes_z_(nMaxCount) char *lpDest, _In_ size_t nMaxCount,
                 _In_z_ _Printf_format_string_ const char *szFormatA, _In_ va_list lpArgList)
{
  if (*((PVOID*)&__stdio_common_vsnprintf_s) != NULL)
  {
    //We use _CRT_INTERNAL_PRINTF_STANDARD_SNPRINTF_BEHAVIOR to get the total count of characters no matter
    //if the buffer is truncated like original snprintf behavior.
    //We also use _CRT_INTERNAL_PRINTF_LEGACY_WIDE_SPECIFIERS in order to favor %s and %S narrow-wide specifiers.
    int ret = __stdio_common_vsnprintf_s(_CRT_INTERNAL_PRINTF_LEGACY_WIDE_SPECIFIERS |
                                         _CRT_INTERNAL_PRINTF_STANDARD_SNPRINTF_BEHAVIOR, lpDest, nMaxCount,
                                         _TRUNCATE, szFormatA, NULL, lpArgList);
    return (ret >= 0) ? ret : (-1);
  }
  if (fn_vsnprintf == NULL)
  {
    PVOID DllBase = ::MxGetDllHandle(L"ntdll.dll");
    fn_vsnprintf = (lpfn_vsnprintf)::MxGetProcedureAddress(DllBase, "_vsnprintf");
    if (fn_vsnprintf == NULL)
      fn_vsnprintf = (lpfn_vsnprintf)1;
  }
  if (fn_vsnprintf == NULL || fn_vsnprintf == (lpfn_vsnprintf)1)
  {
    if (nMaxCount > 0 && lpDest != NULL)
      lpDest[0] = 0;
    return 0;
  }
  return fn_vsnprintf(lpDest, nMaxCount, szFormatA, lpArgList);
}

int mx_swprintf_s(_Out_writes_z_(nMaxCount) wchar_t *lpDest, _In_ size_t nMaxCount,
                  _In_z_ _Printf_format_string_ const wchar_t *szFormatW, ...)
{
  va_list argptr;
  int ret;

  va_start(argptr, szFormatW);
  ret = mx_vsnwprintf(lpDest, nMaxCount, szFormatW, argptr);
  va_end(argptr);
  return ret;
}

int mx_vsnwprintf(_Out_writes_z_(nMaxCount) wchar_t *lpDest, _In_ size_t nMaxCount,
                  _In_z_ _Printf_format_string_ const wchar_t *szFormatW, _In_ va_list lpArgList)
{
  if (*((PVOID*)&__stdio_common_vswprintf_s) != NULL)
  {
    //We use _CRT_INTERNAL_PRINTF_STANDARD_SNPRINTF_BEHAVIOR to get the total count of characters no matter
    //if the buffer is truncated like original snprintf behavior.
    //We also use _CRT_INTERNAL_PRINTF_LEGACY_WIDE_SPECIFIERS in order to favor %s and %S narrow-wide specifiers.
    int ret = __stdio_common_vsnwprintf_s(_CRT_INTERNAL_PRINTF_LEGACY_WIDE_SPECIFIERS |
                                          _CRT_INTERNAL_PRINTF_STANDARD_SNPRINTF_BEHAVIOR, lpDest, nMaxCount,
                                          _TRUNCATE, szFormatW, NULL, lpArgList);
    return (ret >= 0) ? ret : (-1);
  }
  if (fn_vsnwprintf == NULL)
  {
    PVOID DllBase = ::MxGetDllHandle(L"ntdll.dll");
    fn_vsnwprintf = (lpfn_vsnwprintf)::MxGetProcedureAddress(DllBase, "_vsnwprintf");
    if (fn_vsnwprintf == NULL)
      fn_vsnwprintf = (lpfn_vsnwprintf)1;
  }
  if (fn_vsnwprintf == NULL || fn_vsnwprintf == (PVOID)1)
  {
    if (nMaxCount > 0 && lpDest != NULL)
      lpDest[0] = 0;
    return 0;
  }
  return fn_vsnwprintf(lpDest, nMaxCount, szFormatW, lpArgList);
}

PRTL_CRITICAL_SECTION MxGetLoaderLockCS()
{
  static PRTL_CRITICAL_SECTION volatile lpLoaderLockCS = NULL;
  PRTL_CRITICAL_SECTION _lpLoaderLockCS;

  _lpLoaderLockCS = (PRTL_CRITICAL_SECTION)__InterlockedReadPointer(&lpLoaderLockCS);
  if (_lpLoaderLockCS == NULL)
  {
    LPBYTE lpPtr;

#if defined(_M_IX86)
    lpPtr = (LPBYTE)__readfsdword(0x30); //get PEB from the TIB
    lpPtr = *((LPBYTE*)(lpPtr + 0xA0));  //get loader lock pointer
#elif defined(_M_X64)
    lpPtr = (LPBYTE)__readgsqword(0x30); //get TEB
    lpPtr = *((LPBYTE*)(lpPtr + 0x60));
    lpPtr = *((LPBYTE*)(lpPtr + 0x110)); //get loader lock pointer
#endif
    __InterlockedExchangePointer(&lpLoaderLockCS, lpPtr);
    _lpLoaderLockCS = (PRTL_CRITICAL_SECTION)lpPtr;
  }
  return _lpLoaderLockCS;
}

LONG MxGetProcessorArchitecture()
{
  static LONG volatile nProcessorArchitecture = -1;

  if (_InterlockedExchangeAdd(&nProcessorArchitecture, 0L) == -1)
  {
    MX_SYSTEM_PROCESSOR_INFORMATION sProcInfo;
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
    ::MxMemSet(&sProcInfo, 0, sizeof(sProcInfo));
    if (fnRtlGetNativeSystemInformation != NULL)
      nNtStatus = fnRtlGetNativeSystemInformation(MxSystemProcessorInformation, &sProcInfo, sizeof(sProcInfo), NULL);
    else
      nNtStatus = ::MxNtQuerySystemInformation(MxSystemProcessorInformation, &sProcInfo, sizeof(sProcInfo), NULL);
    if (NT_SUCCESS(nNtStatus))
      _InterlockedExchange(&nProcessorArchitecture, (LONG)(sProcInfo.ProcessorArchitecture));
  }
  return _InterlockedExchangeAdd(&nProcessorArchitecture, 0L);
}

HANDLE MxOpenProcess(_In_ DWORD dwDesiredAccess, _In_ BOOL bInheritHandle, _In_ DWORD dwProcessId)
{
  MX_OBJECT_ATTRIBUTES sObjAttr;
  MX_CLIENT_ID sClientId;
  HANDLE hProc;
  NTSTATUS nNtStatus;

  sClientId.UniqueProcess = (SIZE_T)(ULONG_PTR)(dwProcessId);
  sClientId.UniqueThread = 0;
  ::MxMemSet(&sObjAttr, 0, sizeof(sObjAttr));
  sObjAttr.Length = (ULONG)sizeof(sObjAttr);
  sObjAttr.Attributes = (bInheritHandle != FALSE) ? OBJ_INHERIT : 0;
  nNtStatus = ::MxNtOpenProcess(&hProc, dwDesiredAccess, &sObjAttr, &sClientId);
  return (NT_SUCCESS(nNtStatus)) ? hProc : NULL;
}

HANDLE MxOpenThread(_In_ DWORD dwDesiredAccess, _In_ BOOL bInheritHandle, _In_ DWORD dwThreadId)
{
  MX_OBJECT_ATTRIBUTES sObjAttr;
  MX_CLIENT_ID sClientId;
  HANDLE hThread;
  NTSTATUS nNtStatus;

  sClientId.UniqueProcess = 0;
  sClientId.UniqueThread = (SIZE_T)(ULONG_PTR)(dwThreadId);
  ::MxMemSet(&sObjAttr, 0, sizeof(sObjAttr));
  sObjAttr.Length = (ULONG)sizeof(sObjAttr);
  sObjAttr.Attributes = (bInheritHandle != FALSE) ? OBJ_INHERIT : 0;
  nNtStatus = ::MxNtOpenThread(&hThread, dwDesiredAccess, &sObjAttr, &sClientId);
  return (NT_SUCCESS(nNtStatus)) ? hThread : NULL;
}

NTSTATUS MxCreateFile(_Out_ HANDLE *lphFile, _In_ LPCWSTR szFileNameW, _In_opt_ DWORD dwDesiredAccess,
                      _In_opt_ DWORD dwShareMode, _In_opt_ DWORD dwCreationDisposition,
                      _In_opt_ DWORD dwFlagsAndAttributes, _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
  MX_OBJECT_ATTRIBUTES sObjAttr;
  MX_IO_STATUS_BLOCK sIoStatus;
  MX_UNICODE_STRING usNtPath;
  ULONG nFlags;
  HANDLE h;
  NTSTATUS nNtStatus;

  if (lphFile != NULL)
    *lphFile = INVALID_HANDLE_VALUE;
  if (szFileNameW == NULL || lphFile == NULL)
    return STATUS_INVALID_PARAMETER;
  if (fnRtlDosPathNameToNtPathName_U == NULL)
  {
    PVOID DllBase = ::MxGetDllHandle(L"ntdll.dll");
    fnRtlDosPathNameToNtPathName_U = (lpfnRtlDosPathNameToNtPathName_U)::MxGetProcedureAddress(DllBase,
                                                          "RtlDosPathNameToNtPathName_U");
    if (fnRtlDosPathNameToNtPathName_U == NULL)
      fnRtlDosPathNameToNtPathName_U = (lpfnRtlDosPathNameToNtPathName_U)1;
  }
  if (fnRtlDosPathNameToNtPathName_U == NULL || fnRtlDosPathNameToNtPathName_U == (PVOID)1)
    return STATUS_NOT_IMPLEMENTED;
  //----
  nFlags = 0;
  switch (dwCreationDisposition)
  {
    case CREATE_NEW:
      dwCreationDisposition = FILE_CREATE;
      break;
    case CREATE_ALWAYS:
      dwCreationDisposition = FILE_OVERWRITE_IF;
      break;
    case OPEN_EXISTING:
      dwCreationDisposition = FILE_OPEN;
      break;
    case OPEN_ALWAYS:
      dwCreationDisposition = FILE_OPEN_IF;
      break;
    case TRUNCATE_EXISTING:
      dwCreationDisposition = FILE_OVERWRITE;
      break;
    default:
      return STATUS_INVALID_PARAMETER;
  }
  nFlags = 0;
  if ((dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED) == 0)
    nFlags |= FILE_SYNCHRONOUS_IO_NONALERT;
  if ((dwFlagsAndAttributes & FILE_FLAG_WRITE_THROUGH) != 0)
    nFlags |= FILE_WRITE_THROUGH;
  if ((dwFlagsAndAttributes & FILE_FLAG_NO_BUFFERING) != 0)
    nFlags |= FILE_NO_INTERMEDIATE_BUFFERING;
  if ((dwFlagsAndAttributes & FILE_FLAG_RANDOM_ACCESS) != 0)
    nFlags |= FILE_RANDOM_ACCESS;
  if ((dwFlagsAndAttributes & FILE_FLAG_SEQUENTIAL_SCAN) != 0)
    nFlags |= FILE_SEQUENTIAL_ONLY;
  if ((dwFlagsAndAttributes & FILE_FLAG_DELETE_ON_CLOSE) != 0)
  {
    nFlags |= FILE_DELETE_ON_CLOSE;
    dwDesiredAccess |= DELETE;
  }
  if ((dwFlagsAndAttributes & FILE_FLAG_BACKUP_SEMANTICS) != 0)
  {
    if ((dwDesiredAccess & GENERIC_ALL) != 0)
    {
      nFlags |= FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REMOTE_INSTANCE;
    }
    else
    {
      if ((dwDesiredAccess & GENERIC_READ) != 0)
        nFlags |= FILE_OPEN_FOR_BACKUP_INTENT;
      if ((dwDesiredAccess & GENERIC_WRITE) != 0)
        nFlags |= FILE_OPEN_REMOTE_INSTANCE;
    }
  }
  else
  {
    nFlags |= FILE_NON_DIRECTORY_FILE;
  }
  if ((dwFlagsAndAttributes & FILE_FLAG_OPEN_REPARSE_POINT) != 0)
    nFlags |= FILE_OPEN_REPARSE_POINT;
  if ((dwFlagsAndAttributes & FILE_FLAG_OPEN_NO_RECALL) != 0)
    nFlags |= FILE_OPEN_NO_RECALL;
  dwDesiredAccess |= SYNCHRONIZE | FILE_READ_ATTRIBUTES;
  if (fnRtlDosPathNameToNtPathName_U(szFileNameW, &usNtPath, NULL, NULL) == FALSE)
    return STATUS_OBJECT_NAME_NOT_FOUND;
  //--------
  ::MxMemSet(&sObjAttr, 0, sizeof(sObjAttr));
  sObjAttr.Length = (ULONG)sizeof(sObjAttr);
  sObjAttr.ObjectName = &usNtPath;
  if (lpSecurityAttributes != NULL)
  {
    if (lpSecurityAttributes->bInheritHandle != FALSE)
      sObjAttr.Attributes |= OBJ_INHERIT;
    sObjAttr.SecurityDescriptor = lpSecurityAttributes->lpSecurityDescriptor;
  }
  ::MxMemSet(&sIoStatus, 0, sizeof(sIoStatus));
  nNtStatus = ::MxNtCreateFile(&h, dwDesiredAccess, &sObjAttr, &sIoStatus, NULL, dwFlagsAndAttributes & 0x00007FA7,
                               dwShareMode, dwCreationDisposition, nFlags, NULL, 0);
  if (NT_SUCCESS(nNtStatus))
  {
    if (dwCreationDisposition == FILE_OPEN_IF)
    {
      if (sIoStatus.Information == FILE_OPENED)
        nNtStatus = STATUS_OBJECT_NAME_EXISTS;
    }
    else if (dwCreationDisposition == FILE_OVERWRITE_IF)
    {
      if (sIoStatus.Information == FILE_OVERWRITTEN)
        nNtStatus = STATUS_OBJECT_NAME_EXISTS;
    }
    *lphFile = h;
  }
  //else
  //{
  //  if (nNtStatus == STATUS_OBJECT_NAME_COLLISION && dwCreationDisposition == FILE_CREATE)
  //    nNtStatus = STATUS_OBJECT_NAME_EXISTS;
  //}
  ::MxRtlFreeHeap(MxGetProcessHeap(), 0, usNtPath.Buffer);
  //done
  return nNtStatus;
}

NTSTATUS MxIsWow64(_In_ HANDLE hProcess)
{
  NTSTATUS nNtStatus = STATUS_NOT_SUPPORTED;

  if (hProcess == NULL)
    hProcess = __CURRENTPROCESS;
  switch (MxGetProcessorArchitecture())
  {
    case PROCESSOR_ARCHITECTURE_AMD64:
    case PROCESSOR_ARCHITECTURE_IA64:
    case PROCESSOR_ARCHITECTURE_ALPHA64:
      {
      //check on 64-bit platforms
      ULONG_PTR nWow64;

      nNtStatus = ::MxNtQueryInformationProcess(hProcess, MxProcessWow64Information, &nWow64, sizeof(nWow64), NULL);
      if (NT_SUCCESS(nNtStatus))
        nNtStatus = (nWow64 != 0) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
      }
      break;
  }
  return nNtStatus;
}

SIZE_T MxReadMem(_In_ HANDLE hProcess, _Out_writes_bytes_(nBytesCount) LPVOID lpDest, _In_ LPVOID lpSrc,
                 _In_ SIZE_T nBytesCount)
{
  NTSTATUS nStatus;
  SIZE_T nReaded;

  if (nBytesCount == 0)
    return 0;
  if (hProcess == __CURRENTPROCESS)
    return ::MxTryMemCopy(lpDest, lpSrc, nBytesCount);
  nReaded = 0;
  nStatus = ::MxNtReadVirtualMemory(hProcess, lpSrc, lpDest, nBytesCount, &nReaded);
  if (nStatus == STATUS_PARTIAL_COPY)
    return nReaded;
  return (NT_SUCCESS(nStatus)) ? nBytesCount : 0;
}

BOOL MxWriteMem(_In_ HANDLE hProcess, _In_ LPVOID lpDest, _In_ LPVOID lpSrc, _In_ SIZE_T nBytesCount)
{
  NTSTATUS nStatus;
  SIZE_T nWritten;

  if (nBytesCount == 0)
    return TRUE;
  if (hProcess == __CURRENTPROCESS)
    return (::MxTryMemCopy(lpDest, lpSrc, nBytesCount) == nBytesCount) ? TRUE : FALSE;
  nWritten = 0;
  nStatus = ::MxNtWriteVirtualMemory(hProcess, lpDest, lpSrc, nBytesCount, &nWritten);
  return (NT_SUCCESS(nStatus) ||
          (nStatus == STATUS_PARTIAL_COPY && nWritten == nBytesCount)) ? TRUE : FALSE;
}

NTSTATUS MxGetThreadPriority(_In_ HANDLE hThread, _Out_ int *lpnPriority)
{
  MX_THREAD_BASIC_INFORMATION sTbi;
  NTSTATUS nNtStatus;

  nNtStatus = ::MxNtQueryInformationThread(hThread, MxThreadBasicInformation, &sTbi, sizeof(sTbi), NULL);
  if (NT_SUCCESS(nNtStatus))
  {
    if (sTbi.BasePriority == THREAD_BASE_PRIORITY_LOWRT+1)
      *lpnPriority = (int)THREAD_PRIORITY_TIME_CRITICAL;
    else if (sTbi.BasePriority == THREAD_BASE_PRIORITY_IDLE-1)
      *lpnPriority = (int)THREAD_PRIORITY_IDLE;
    else
      *lpnPriority = (int)(sTbi.BasePriority);
  }
  return nNtStatus;
}

NTSTATUS MxSetThreadPriority(_In_ HANDLE hThread, _In_ int _nPriority)
{
  LONG nPriority = _nPriority;

  if (nPriority == THREAD_PRIORITY_TIME_CRITICAL)
    nPriority = THREAD_BASE_PRIORITY_LOWRT + 1;
  else if (nPriority == THREAD_PRIORITY_IDLE)
    nPriority = THREAD_BASE_PRIORITY_IDLE - 1;
  return ::MxNtSetInformationThread(hThread, MxThreadBasePriority, &nPriority, sizeof(nPriority));
}

DWORD MxGetCurrentThreadId()
{
  LPBYTE lpPtr;
  DWORD dw;

#if defined _M_IX86
  lpPtr = (LPBYTE)__readfsdword(0x18); //get TEB
  dw = *((DWORD*)(lpPtr+0x24));        //TEB.ClientId.UniqueThread
#elif defined _M_X64
  lpPtr = (LPBYTE)__readgsqword(0x30); //get TEB
  dw = *((DWORD*)(lpPtr+0x48));        //TEB.ClientId.UniqueThread
#endif
  return dw;
}

DWORD MxGetCurrentProcessId()
{
  LPBYTE lpPtr;
  DWORD dw;

#if defined _M_IX86
  lpPtr = (LPBYTE)__readfsdword(0x18);     //get TEB
  dw = (DWORD)*((ULONGLONG*)(lpPtr+0x20)); //TEB.ClientId.UniqueProcess
#elif defined _M_X64
  lpPtr = (LPBYTE)__readgsqword(0x30);     //get TEB
  dw = (DWORD)*((ULONGLONG*)(lpPtr+0x40)); //TEB.ClientId.UniqueProcess
#endif
  return dw;
}

PVOID MxGetDllHandle(_In_z_ PCWSTR szModuleNameW)
{
  PRTL_CRITICAL_SECTION lpLoaderLockCS;
  MX_PEB_LDR_DATA *lpPebLdr;
  MX_LIST_ENTRY *lpFirst, *lpCurr;
  MX_LDR_DATA_TABLE_ENTRY *lpLdrEntry;
  MX_UNICODE_STRING usDllName;
  PVOID pRet;

  if (szModuleNameW == NULL || *szModuleNameW == 0)
    return NULL;
  usDllName.Buffer = (PWSTR)szModuleNameW;
  for (usDllName.Length=0; szModuleNameW[usDllName.Length]!=0; usDllName.Length++);
  usDllName.Length *= (USHORT)sizeof(WCHAR);
  usDllName.MaximumLength = usDllName.Length;
  //----
#if defined(_M_IX86)
  lpPebLdr = *((MX_PEB_LDR_DATA**)(MxGetPeb()+0x0C));
#elif defined(_M_X64)
  lpPebLdr = *((MX_PEB_LDR_DATA**)(MxGetPeb()+0x18));
#endif
  lpLoaderLockCS = ::MxGetLoaderLockCS();
  if (lpLoaderLockCS == NULL)
    return NULL;
  ::MxRtlEnterCriticalSection(lpLoaderLockCS);
  //loop
  pRet = NULL;
  lpFirst = &(lpPebLdr->InLoadOrderModuleList);
  for (lpCurr=lpFirst->Flink; lpCurr!=lpFirst; lpCurr=lpLdrEntry->InLoadOrderLinks.Flink)
  {
    lpLdrEntry = (MX_LDR_DATA_TABLE_ENTRY*)((LPBYTE)lpCurr - FIELD_OFFSET(MX_LDR_DATA_TABLE_ENTRY, InLoadOrderLinks));
    if (::MxRtlCompareUnicodeString(&(lpLdrEntry->BaseDllName), &usDllName, TRUE) == 0)
    {
      pRet = lpLdrEntry->DllBase;
      break;
    }
  }
  ::MxRtlLeaveCriticalSection(lpLoaderLockCS);
  return pRet;
}

PVOID MxGetRemoteDllHandle(_In_ HANDLE hProcess, _In_z_ PCWSTR szModuleNameW)
{
  BOOL bTargetIsX64;
  SIZE_T k, nModuleNameLen;
  LPBYTE lpPeb, lpFirstLink, lpCurrLink;
  DWORD dw, dw2;
  ULONGLONG ull;
  WORD w;
  BYTE nTemp8;
  union {
    MX_LDR_DATA_TABLE_ENTRY32 sCurrEntry32;
    MX_LDR_DATA_TABLE_ENTRY64 sCurrEntry64;
  };
  MEMORY_BASIC_INFORMATION sMbi;
  LPBYTE lpBaseAddr, lpCurrAddr;
  struct {
    MX_UNICODE_STRING us;
    BYTE aBuf[2048];
  } usDllName;
  MX_UNICODE_STRING usTemp;
  NTSTATUS nNtStatus;

  if (szModuleNameW == NULL || *szModuleNameW == 0)
    return NULL;
  for (nModuleNameLen=0; szModuleNameW[nModuleNameLen]!=0; nModuleNameLen++);
  nModuleNameLen *= sizeof(WCHAR);
  //check if we are dealing with a x86/x64 bit process
  nNtStatus = MxIsWow64(hProcess);
#if defined(_M_IX86)
  switch (nNtStatus)
  {
    case STATUS_SUCCESS:
    case STATUS_NOT_SUPPORTED:
      bTargetIsX64 = FALSE;
      break;
    case STATUS_UNSUCCESSFUL:
      //cannot retrieve a 64-bit process module address
      return NULL;
    default:
      //on error return NULL
      return NULL;
  }
#elif defined(_M_X64)
  switch (nNtStatus)
  {
    case STATUS_UNSUCCESSFUL:
      bTargetIsX64 = TRUE;
      break;
    case STATUS_SUCCESS:
      bTargetIsX64 = FALSE;
      break;
    default:
      //on error return NULL
      return NULL;
  }
#endif
  //get remote peb to start
  lpPeb = MxGetRemotePeb(hProcess);
  if (lpPeb == NULL)
    return NULL;
  //find the loader data table
  lpFirstLink = NULL;
  if (bTargetIsX64 == FALSE)
  {
    //nPeb32+12 => pointer to PEB_LDR_DATA32
    dw = 0;
    if (MxReadMem(hProcess, &dw, lpPeb+0x0C, sizeof(dw)) == sizeof(dw) && dw != 0)
    {
      //check if table is initialized
      if (MxReadMem(hProcess, &nTemp8, (LPBYTE)ULongToPtr(dw) + 0x04, sizeof(nTemp8)) == sizeof(nTemp8) && nTemp8 != 0)
        lpFirstLink = (LPBYTE)ULongToPtr(dw) + 0x0C; //PEB_LDR_DATA32.InLoadOrderModuleList.Flink
    }
  }
  else
  {
    //nPeb64+24 => pointer to PEB_LDR_DATA64
    ull = 0ui64;
    if (MxReadMem(hProcess, &ull, lpPeb+0x18, sizeof(ull)) == sizeof(ull) && ull != 0)
    {
      //check if table is initialized
      nTemp8 = 0;
      if (MxReadMem(hProcess, &nTemp8, (LPBYTE)ull + 0x04, sizeof(nTemp8)) == sizeof(nTemp8) && nTemp8 != 0)
        lpFirstLink = (LPBYTE)ull + 0x10; //PEB_LDR_DATA64.InLoadOrderModuleList.Flink
    }
  }
  //if we could get the loader table, query it
  if (lpFirstLink != NULL)
  {
    if (bTargetIsX64 == FALSE)
    {
      lpCurrLink = (MxReadMem(hProcess, &dw, lpFirstLink, 4) == 4) ? (LPBYTE)ULongToPtr(dw) : lpFirstLink;
      while (lpCurrLink != lpFirstLink)
      {
        if (MxReadMem(hProcess, &sCurrEntry32, lpCurrLink - FIELD_OFFSET(MX_LDR_DATA_TABLE_ENTRY32, InLoadOrderLinks),
                      sizeof(sCurrEntry32)) != sizeof(sCurrEntry32))
            break;
        if (sCurrEntry32.DllBase != 0 &&
            (SIZE_T)(sCurrEntry32.BaseDllName.Length) == nModuleNameLen &&
            RemoteCompareStringW(hProcess, (LPWSTR)ULongToPtr(sCurrEntry32.BaseDllName.Buffer), szModuleNameW,
                                 nModuleNameLen / sizeof(WCHAR)) != FALSE)
        {
          return (LPBYTE)ULongToPtr(sCurrEntry32.DllBase);
        }
        lpCurrLink = (LPBYTE)ULongToPtr(sCurrEntry32.InLoadOrderLinks.Flink);
      }
    }
    else
    {
      lpCurrLink = (MxReadMem(hProcess, &ull, lpFirstLink, 8) == 8) ? (LPBYTE)ull : lpFirstLink;
      while (lpCurrLink != lpFirstLink)
      {
        if (MxReadMem(hProcess, &sCurrEntry64, lpCurrLink - FIELD_OFFSET(MX_LDR_DATA_TABLE_ENTRY64, InLoadOrderLinks),
                      sizeof(sCurrEntry64)) != sizeof(sCurrEntry64))
          break;
        if (sCurrEntry64.DllBase != 0 &&
            (SIZE_T)(sCurrEntry64.BaseDllName.Length) == nModuleNameLen &&
            RemoteCompareStringW(hProcess, (LPWSTR)(sCurrEntry64.BaseDllName.Buffer), szModuleNameW,
                                 nModuleNameLen/sizeof(WCHAR)) != FALSE)
        {
          return (LPBYTE)(sCurrEntry64.DllBase);
        }
        lpCurrLink = (LPBYTE)(sCurrEntry64.InLoadOrderLinks.Flink);
      }
    }
  }
  //try scanning mapped image files
  lpCurrAddr = NULL;
  usTemp.Buffer = (PWSTR)szModuleNameW;
  usTemp.Length = usTemp.MaximumLength = (USHORT)nModuleNameLen;
  while (1)
  {
    if (!NT_SUCCESS(MxNtQueryVirtualMemory(hProcess, lpCurrAddr, MxMemoryBasicInformation, &sMbi,
                                           sizeof(sMbi), &k)))
      break;
    if (sMbi.Type != MEM_MAPPED && sMbi.Type != MEM_IMAGE)
    {
      //skip block
      lpCurrAddr += sMbi.RegionSize;
      continue;
    }
    //get block size
    dw = sMbi.Type;
    lpBaseAddr = (LPBYTE)(sMbi.AllocationBase);
    do
    {
      lpCurrAddr += sMbi.RegionSize;
      nNtStatus = MxNtQueryVirtualMemory(hProcess, lpCurrAddr, MxMemoryBasicInformation, &sMbi,
                                          sizeof(sMbi), &k);
    }
    while (NT_SUCCESS(nNtStatus) && lpBaseAddr == (LPBYTE)(sMbi.AllocationBase));
    if (!NT_SUCCESS(nNtStatus))
      break;
    //only check images
    if (dw != MEM_IMAGE)
      continue;
    //check image platform match
    if (MxReadMem(hProcess, &w, lpBaseAddr + FIELD_OFFSET(IMAGE_DOS_HEADER, e_magic), sizeof(w)) != sizeof(w) ||
        w != IMAGE_DOS_SIGNATURE)
      continue;
    //get header offset
    if (MxReadMem(hProcess, &dw, lpBaseAddr + FIELD_OFFSET(IMAGE_DOS_HEADER, e_lfanew), sizeof(dw)) != sizeof(dw))
      continue;
    //check signature
    if (MxReadMem(hProcess, &dw2, lpBaseAddr + (SIZE_T)dw + (SIZE_T)FIELD_OFFSET(IMAGE_NT_HEADERS32, Signature),
                  sizeof(dw2)) != sizeof(dw2) ||
        dw2 != IMAGE_NT_SIGNATURE)
      continue;
    //check image type
    if (MxReadMem(hProcess, &w, lpBaseAddr + (SIZE_T)dw + (SIZE_T)FIELD_OFFSET(IMAGE_NT_HEADERS32, FileHeader.Machine),
                  sizeof(w)) != sizeof(w))
      continue;
    if (bTargetIsX64 == FALSE)
    {
      if (w != IMAGE_FILE_MACHINE_I386)
        continue;
    }
    else
    {
      if (w != IMAGE_FILE_MACHINE_AMD64)
        continue;
    }
    //retrieve mapped section name
    nNtStatus = MxNtQueryVirtualMemory(hProcess, (PVOID)lpBaseAddr, MxMemorySectionName, &usDllName, sizeof(usDllName),
                                       &k);
    if (!NT_SUCCESS(nNtStatus))
      continue;
    //compare base name
    k = (SIZE_T)(usDllName.us.Length) / sizeof(WCHAR);
    while (k > 0 && usDllName.us.Buffer[k-1] != L'\\')
      k--;
    usDllName.us.Buffer += k;
    usDllName.us.Length -= (USHORT)(k * sizeof(WCHAR));
    if (MxRtlCompareUnicodeString(&(usDllName.us), &usTemp, TRUE) != 0)
      return lpBaseAddr;
  }
  return NULL;
}
/*
PVOID MxGetProcedureAddress(_In_ PVOID DllBase, _In_z_ PCSTR szApiNameA)
{
  MX_ANSI_STRING asApiName, *lpApiName;
  ULONG nOrd;
  PVOID pRet;

  if (DllBase == NULL || szApiNameA == NULL || szApiNameA[0] == 0)
    return NULL;
  if (IS_INTRESOURCE(szApiNameA))
  {
    nOrd = (ULONG)((SIZE_T)szApiNameA & 0xFFFF);
    lpApiName = NULL;
  }
  else if (szApiNameA[0] == '#')
  {
    nOrd = 0;
    szApiNameA++;
    while (szApiNameA[0] >= '0' && szApiNameA[0] <= '9')
    {
      nOrd = nOrd * 10 + (ULONG)(szApiNameA[0] - '0');
      if (nOrd >= 65536)
        return NULL;
    }
    if (nOrd == 0 || szApiNameA[0] != 0)
      return NULL;
    lpApiName = NULL;
  }
  else
  {
    nOrd = 0;
    asApiName.Buffer = (PSTR)szApiNameA;
    for (asApiName.Length=0; szApiNameA[asApiName.Length]!=0; asApiName.Length++);
    asApiName.MaximumLength = asApiName.Length;
    lpApiName = &asApiName;
  }
  if (!NT_SUCCESS(::MxLdrGetProcedureAddress(DllBase, lpApiName, nOrd, &pRet)))
    pRet = NULL;
  return pRet;
}
*/

VOID MxSleep(_In_ DWORD dwTimeMs)
{
  LARGE_INTEGER liTime;

  liTime.QuadPart = -(LONGLONG)MX_MILLISECONDS_TO_100NS(dwTimeMs);
  ::MxNtDelayExecution(FALSE, &liTime);
  return;
}

}; //extern "C"

//-----------------------------------------------------------

static BOOL RemoteCompareStringW(_In_ HANDLE hProcess, _In_z_ LPCWSTR szRemoteNameW, _In_z_ LPCWSTR szLocalNameW,
                                 _In_ SIZE_T nNameLen)
{
  WCHAR szTempW[256];
  SIZE_T nThisRound;
  MX_UNICODE_STRING us[2];

  if (szRemoteNameW == NULL)
    return FALSE;
  us[0].Buffer = szTempW;
  while (nNameLen > 0)
  {
    nThisRound = (nNameLen > _countof(szTempW)) ? _countof(szTempW) : nNameLen;
    nThisRound *= sizeof(WCHAR);
    if (::MxReadMem(hProcess, szTempW, (LPVOID)szRemoteNameW, nThisRound) != nThisRound)
      return FALSE;
    us[1].Buffer = (PWSTR)szLocalNameW;
    us[0].Length = us[1].Length = us[0].MaximumLength = us[1].MaximumLength = (USHORT)nThisRound;
    if (MxRtlCompareUnicodeString(&us[0], &us[1], TRUE) != 0)
      return FALSE;
    nThisRound /= sizeof(WCHAR);
    szRemoteNameW += nThisRound;
    szLocalNameW += nThisRound;
    nNameLen -= nThisRound;
  }
  return TRUE;
}
