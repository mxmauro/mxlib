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
#ifndef _MX_NTDEFS_H
#define _MX_NTDEFS_H

#include <windows.h>

//-----------------------------------------------------------

#ifndef STATUS_SUCCESS
  #define STATUS_SUCCESS                 ((NTSTATUS)0x00000000L)
#endif //!STATUS_SUCCESS
#ifndef STATUS_OBJECT_NAME_EXISTS
  #define STATUS_OBJECT_NAME_EXISTS      ((NTSTATUS)0x40000000L)
#endif //!STATUS_OBJECT_NAME_EXISTS
#ifndef STATUS_BUFFER_OVERFLOW
  #define STATUS_BUFFER_OVERFLOW         ((NTSTATUS)0x80000005L)
#endif //!STATUS_BUFFER_OVERFLOW
#ifndef STATUS_UNSUCCESSFUL
  #define STATUS_UNSUCCESSFUL            ((NTSTATUS)0xC0000001L)
#endif //!STATUS_UNSUCCESSFUL
#ifndef STATUS_NOT_IMPLEMENTED
  #define STATUS_NOT_IMPLEMENTED         ((NTSTATUS)0xC0000002L)
#endif //!STATUS_NOT_IMPLEMENTED
#ifndef STATUS_INFO_LENGTH_MISMATCH
  #define STATUS_INFO_LENGTH_MISMATCH    ((NTSTATUS)0xC0000004L)
#endif //!STATUS_INFO_LENGTH_MISMATCH
#ifndef STATUS_INVALID_PARAMETER
  #define STATUS_INVALID_PARAMETER       ((NTSTATUS)0xC000000DL)
#endif //!STATUS_INVALID_PARAMETER
#ifndef STATUS_PARTIAL_COPY
  #define STATUS_PARTIAL_COPY            ((NTSTATUS)0x8000000DL)
#endif //!STATUS_PARTIAL_COPY
#ifndef STATUS_END_OF_FILE
  #define STATUS_END_OF_FILE             ((NTSTATUS)0xC0000011L)
#endif //!STATUS_END_OF_FILE
#ifndef STATUS_NO_MEMORY
  #define STATUS_NO_MEMORY               ((NTSTATUS)0xC0000017L)
#endif //!STATUS_NO_MEMORY
#ifndef STATUS_ACCESS_DENIED
  #define STATUS_ACCESS_DENIED           ((NTSTATUS)0xC0000022L)
#endif //!STATUS_ACCESS_DENIED
#ifndef STATUS_BUFFER_TOO_SMALL
  #define STATUS_BUFFER_TOO_SMALL        ((NTSTATUS)0xC0000023L)
#endif //!STATUS_BUFFER_TOO_SMALL
#ifndef STATUS_OBJECT_NAME_NOT_FOUND
  #define STATUS_OBJECT_NAME_NOT_FOUND   ((NTSTATUS)0xC0000034L)
#endif //!STATUS_OBJECT_NAME_NOT_FOUND
#ifndef STATUS_OBJECT_NAME_COLLISION
  #define STATUS_OBJECT_NAME_COLLISION   ((NTSTATUS)0xC0000035L)
#endif //!STATUS_OBJECT_NAME_COLLISION
#ifndef STATUS_DATA_ERROR
  #define STATUS_DATA_ERROR              ((NTSTATUS)0xC000003EL)
#endif //!STATUS_DATA_ERROR
#ifndef STATUS_INSUFFICIENT_RESOURCES
  #define STATUS_INSUFFICIENT_RESOURCES  ((NTSTATUS)0xC000009AL)
#endif //STATUS_INSUFFICIENT_RESOURCES
#ifndef STATUS_INSTANCE_NOT_AVAILABLE
  #define STATUS_INSTANCE_NOT_AVAILABLE  ((NTSTATUS)0xC00000ABL)
#endif //!STATUS_INSTANCE_NOT_AVAILABLE
#ifndef STATUS_PIPE_NOT_AVAILABLE
  #define STATUS_PIPE_NOT_AVAILABLE      ((NTSTATUS)0xC00000ACL)
#endif //!STATUS_PIPE_NOT_AVAILABLE
#ifndef STATUS_PIPE_BUSY
  #define STATUS_PIPE_BUSY               ((NTSTATUS)0xC00000AEL)
#endif //!STATUS_PIPE_BUSY
#ifndef STATUS_PIPE_DISCONNECTED
  #define STATUS_PIPE_DISCONNECTED       ((NTSTATUS)0xC00000B0L)
#endif //!STATUS_PIPE_DISCONNECTED
#ifndef STATUS_NOT_SUPPORTED
  #define STATUS_NOT_SUPPORTED           ((NTSTATUS)0xC00000BBL)
#endif //!STATUS_NOT_SUPPORTED
#ifndef STATUS_INTERNAL_ERROR
  #define STATUS_INTERNAL_ERROR          ((NTSTATUS)0xC00000E5L)
#endif //STATUS_INTERNAL_ERROR
#ifndef STATUS_CANCELLED
  #define STATUS_CANCELLED               ((NTSTATUS)0xC0000120L)
#endif //!STATUS_CANCELLED
#ifndef STATUS_LOCAL_DISCONNECT
  #define STATUS_LOCAL_DISCONNECT        ((NTSTATUS)0xC000013BL)
#endif //!STATUS_LOCAL_DISCONNECT
#ifndef STATUS_REMOTE_DISCONNECT
  #define STATUS_REMOTE_DISCONNECT       ((NTSTATUS)0xC000013CL)
#endif //!STATUS_REMOTE_DISCONNECT
#ifndef STATUS_NOT_FOUND
  #define STATUS_NOT_FOUND               ((NTSTATUS)0xC0000225L)
#endif //!STATUS_NOT_FOUND

#ifndef NT_SUCCESS
  #define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif //!NT_SUCCESS

#define MX_DIRECTORY_QUERY                            0x0001
#define MX_SYMBOLIC_LINK_QUERY                        0x0001

#if defined(_M_X64)
#define MX_UNW_FLAG_NHANDLER                               0

#define MX_UNWIND_HISTORY_TABLE_NONE                       0
#define MX_UNWIND_HISTORY_TABLE_GLOBAL                     1
#define MX_UNWIND_HISTORY_TABLE_LOCAL                      2
#endif //_M_X64

#define MX_MILLISECONDS_TO_100NS(n) (10000ui64 * (ULONGLONG)(n))
#define MX_100NS_TO_MILLISECONDS(n) ((ULONGLONG)(n) / 10000ui64)

#define MX_CURRENTPROCESS             ((HANDLE)(LONG_PTR)-1)
#define MX_CURRENTTHREAD              ((HANDLE)(LONG_PTR)-2)

//-----------------------------------------------------------

typedef __success(return >= 0) LONG NTSTATUS;
typedef LONG MX_KPRIORITY;
typedef LONG MX_KWAIT_REASON;

#ifdef __cplusplus
  #define MX_ENUMCLASS enum class
#else //__cplusplus
  #define MX_ENUMCLASS typedef enum
#endif //__cplusplus

#define MxSystemBasicInformation             0
#define MxSystemProcessorInformation         1
#define MxSystemProcessInformation           5
#define MxSystemSessionProcessesInformation 53
#define MxSystemExtendedProcessInformation  57

#define MxProcessBasicInformation            0
#define MxProcessQuotaLimits                 1
#define MxProcessIoCounters                  2
#define MxProcessVmCounters                  3
#define MxProcessTimes                       4
#define MxProcessBasePriority                5
#define MxProcessRaisePriority               6
#define MxProcessDebugPort                   7
#define MxProcessExceptionPort               8
#define MxProcessAccessToken                 9
#define MxProcessLdtInformation             10
#define MxProcessLdtSize                    11
#define MxProcessDefaultHardErrorMode       12
#define MxProcessIoPortHandlers             13
#define MxProcessPooledUsageAndLimits       14
#define MxProcessWorkingSetWatch            15
#define MxProcessUserModeIOPL               16
#define MxProcessEnableAlignmentFaultFixup  17
#define MxProcessPriorityClass              18
#define MxProcessWx86Information            19
#define MxProcessHandleCount                20
#define MxProcessAffinityMask               21
#define MxProcessPriorityBoost              22
#define MxProcessDeviceMap                  23
#define MxProcessSessionInformation         24
#define MxProcessForegroundInformation      25
#define MxProcessWow64Information           26
#define MxProcessImageFileName              27
#define MxProcessLUIDDeviceMapsEnabled      28
#define MxProcessBreakOnTermination         29
#define MxProcessDebugObjectHandle          30
#define MxProcessDebugFlags                 31
#define MxProcessHandleTracing              32
#define MxProcessIoPriority                 33
#define MxProcessExecuteFlags               34
#define MxProcessTlsInformation             35
#define MxProcessCookie                     36
#define MxProcessImageInformation           37
#define MxProcessCycleTime                  38
#define MxProcessPagePriority               39
#define MxProcessInstrumentationCallback    40
#define MxProcessThreadStackAllocation      41
#define MxProcessWorkingSetWatchEx          42
#define MxProcessImageFileNameWin32         43
#define MxProcessImageFileMapping           44
#define MxProcessAffinityUpdateMode         45
#define MxProcessMemoryAllocationMode       46
#define MxProcessGroupInformation           47
#define MxProcessTokenVirtualizationEnabled 48
#define MxProcessConsoleHostMxProcess       49
#define MxProcessWindowInformation          50

#define MxThreadBasicInformation             0
#define MxThreadTimes                        1
#define MxThreadPriority                     2
#define MxThreadBasePriority                 3
#define MxThreadAffinityMask                 4
#define MxThreadImpersonationToken           5
#define MxThreadDescriptorTableEntry         6
#define MxThreadEnableAlignmentFaultFixup    7
#define MxThreadEventPair_Reusable           8
#define MxThreadQuerySetWin32StartAddress    9
#define MxThreadZeroTlsCell                 10
#define MxThreadPerformanceCount            11
#define MxThreadAmILastThread               12
#define MxThreadIdealProcessor              13
#define MxThreadPriorityBoost               14
#define MxThreadSetTlsArrayAddress          15
#define MxThreadIsIoPending                 16
#define MxThreadHideFromDebugger            17
#define MxThreadBreakOnTermination          18
#define MxThreadSwitchLegacyState           19
#define MxThreadIsTerminated                20
#define MxThreadLastSystemCall              21
#define MxThreadIoPriority                  22
#define MxThreadCycleTime                   23
#define MxThreadPagePriority                24
#define MxThreadActualBasePriority          25
#define MxThreadTebInformation              26
#define MxThreadCSwitchMon                  27
#define MxThreadCSwitchPmu                  28
#define MxThreadWow64Context                29
#define MxThreadGroupInformation            30
#define MxThreadUmsInformation              31
#define MxThreadCounterProfiling            32
#define MxThreadIdealProcessorEx            33

#define MxMemoryBasicInformation             0
#define MxMemoryWorkingSetList               1
#define MxMemorySectionName                  2
#define MxMemoryBasicVlmInformation          3
#define MxMemoryWorkingSetExList             4

#define MxSectionBasicInformation            0
#define MxSectionImageInformation            1

#define MxKeyBasicInformation                0
#define MxKeyNodeInformation                 1
#define MxKeyFullInformation                 2
#define MxKeyNameInformation                 3
#define MxKeyCachedInformation               4
#define MxKeyFlagsInformation                5
#define MxKeyVirtualizationInformation       6
#define MxKeyHandleTagsInformation           7

#define MxKeyValueBasicInformation           0
#define MxKeyValueFullInformation            1
#define MxKeyValuePartialInformation         2
#define MxKeyValueFullInformationAlign64     3
#define MxKeyValuePartialInformationAlign64  4

#define MxFileDirectoryInformation           1
#define MxFileFullDirectoryInformation       2
#define MxFileBothDirectoryInformation       3
#define MxFileBasicInformation               4
#define MxFileStandardInformation            5
#define MxFileInternalInformation            6
#define MxFileEaInformation                  7
#define MxFileAccessInformation              8
#define MxFileNameInformation                9
#define MxFileRenameInformation             10
#define MxFileLinkInformation               11
#define MxFileNamesInformation              12
#define MxFileDispositionInformation        13
#define MxFilePositionInformation           14
#define MxFileFullEaInformation             15
#define MxFileModeInformation               16
#define MxFileAlignmentInformation          17
#define MxFileAllInformation                18
#define MxFileAllocationInformation         19
#define MxFileEndOfFileInformation          20
#define MxFileAlternateNameInformation      21
#define MxFileStreamInformation             22
#define MxFilePipeInformation               23
#define MxFilePipeLocalInformation          24
#define MxFilePipeRemoteInformation         25
#define MxFileMailslotQueryInformation      26
#define MxFileMailslotSetInformation        27
#define MxFileCompressionInformation        28
#define MxFileObjectIdInformation           29
#define MxFileCompletionInformation         30
#define MxFileMoveClusterInformation        31
#define MxFileQuotaInformation              32
#define MxFileReparsePointInformation       33
#define MxFileNetworkOpenInformation        34
#define MxFileAttributeTagInformation       35
#define MxFileTrackingInformation           36
#define MxFileIdBothDirectoryInformation    37
#define MxFileIdFullDirectoryInformation    38
#define MxFileValidDataLengthInformation    39
#define MxFileShortNameInformation          40

#define MxObjectBasicInformation             0
#define MxObjectNameInformation              1
#define MxObjectTypeInformation              2

#define MxNotificationEvent                  0
#define MxSynchronizationEvent               1

//-----------------------------------------------------------

#pragma pack(8)
typedef struct _MX_ANSI_STRING32 {
  USHORT Length;
  USHORT MaximumLength;
  ULONG Buffer;
} MX_ANSI_STRING32, *PMX_ANSI_STRING32;

typedef struct _MX_ANSI_STRING64 {
  USHORT Length;
  USHORT MaximumLength;
  ULONGLONG Buffer;
} MX_ANSI_STRING64, *PMX_ANSI_STRING64;

typedef struct _MX_ANSI_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PSTR Buffer;
} MX_ANSI_STRING, *PMX_ANSI_STRING;

typedef struct _MX_UNICODE_STRING32 {
  USHORT Length;
  USHORT MaximumLength;
  ULONG Buffer;
} MX_UNICODE_STRING32, *PMX_UNICODE_STRING32;

typedef struct _MX_UNICODE_STRING64 {
  USHORT Length;
  USHORT MaximumLength;
  ULONGLONG Buffer;
} MX_UNICODE_STRING64, *PMX_UNICODE_STRING64;

typedef struct _MX_UNICODE_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PWSTR Buffer;
} MX_UNICODE_STRING, *PMX_UNICODE_STRING;

typedef const MX_ANSI_STRING32 *PCMX_ANSI_STRING32;
typedef const MX_ANSI_STRING64 *PCMX_ANSI_STRING64;
typedef const MX_ANSI_STRING *PCMX_ANSI_STRING;
typedef const MX_UNICODE_STRING32 *PCMX_UNICODE_STRING32;
typedef const MX_UNICODE_STRING64 *PCMX_UNICODE_STRING64;
typedef const MX_UNICODE_STRING *PCMX_UNICODE_STRING;

typedef struct _MX_OBJECT_ATTRIBUTES {
  ULONG Length;
  HANDLE RootDirectory;
  PMX_UNICODE_STRING ObjectName;
  ULONG Attributes;
  PVOID SecurityDescriptor;
  PVOID SecurityQualityOfService;
} MX_OBJECT_ATTRIBUTES;
typedef MX_OBJECT_ATTRIBUTES *PMX_OBJECT_ATTRIBUTES;

typedef struct _MX_IO_STATUS_BLOCK {
  union {
    NTSTATUS Status;
    PVOID Pointer;
  } DUMMYUNIONNAME;
  ULONG_PTR Information;
} MX_IO_STATUS_BLOCK, *PMX_IO_STATUS_BLOCK;

typedef struct _MX_FILE_BASIC_INFORMATION {
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  ULONG FileAttributes;
} MX_FILE_BASIC_INFORMATION, *PMX_FILE_BASIC_INFORMATION;

typedef struct _MX_FILE_STANDARD_INFORMATION {
  LARGE_INTEGER AllocationSize;
  LARGE_INTEGER EndOfFile;
  ULONG NumberOfLinks;
  BOOLEAN DeletePending;
  BOOLEAN Directory;
} MX_FILE_STANDARD_INFORMATION, *PMX_FILE_STANDARD_INFORMATION;

typedef struct _MX_FILE_POSITION_INFORMATION {
  LARGE_INTEGER CurrentByteOffset;
} MX_FILE_POSITION_INFORMATION, *PMX_FILE_POSITION_INFORMATION;

typedef struct _MX_FILE_DIRECTORY_INFORMATION {
  ULONG NextEntryOffset;
  ULONG FileIndex;
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  LARGE_INTEGER EndOfFile;
  LARGE_INTEGER AllocationSize;
  ULONG FileAttributes;
  ULONG FileNameLength;
  WCHAR FileName[1];
} MX_FILE_DIRECTORY_INFORMATION, *PMX_FILE_DIRECTORY_INFORMATION;

typedef struct _MX_FILE_FULL_DIR_INFORMATION {
  ULONG NextEntryOffset;
  ULONG FileIndex;
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  LARGE_INTEGER EndOfFile;
  LARGE_INTEGER AllocationSize;
  ULONG FileAttributes;
  ULONG FileNameLength;
  ULONG EaSize;
  WCHAR FileName[1];
} MX_FILE_FULL_DIR_INFORMATION, *PMX_FILE_FULL_DIR_INFORMATION;

typedef struct _MX_FILE_BOTH_DIR_INFORMATION {
  ULONG NextEntryOffset;
  ULONG FileIndex;
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  LARGE_INTEGER EndOfFile;
  LARGE_INTEGER AllocationSize;
  ULONG FileAttributes;
  ULONG FileNameLength;
  ULONG EaSize;
  CCHAR ShortNameLength;
  WCHAR ShortName[12];
  WCHAR FileName[1];
} MX_FILE_BOTH_DIR_INFORMATION, *PMX_FILE_BOTH_DIR_INFORMATION;

typedef struct _MX_FILE_NAMES_INFORMATION {
  ULONG NextEntryOffset;
  ULONG FileIndex;
  ULONG FileNameLength;
  WCHAR FileName[1];
} MX_FILE_NAMES_INFORMATION, *PMX_FILE_NAMES_INFORMATION;

typedef struct _MX_FILE_ID_BOTH_DIR_INFORMATION {
  ULONG NextEntryOffset;
  ULONG FileIndex;
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  LARGE_INTEGER EndOfFile;
  LARGE_INTEGER AllocationSize;
  ULONG FileAttributes;
  ULONG FileNameLength;
  ULONG EaSize;
  CCHAR ShortNameLength;
  WCHAR ShortName[12];
  LARGE_INTEGER FileId;
  WCHAR FileName[1];
} MX_FILE_ID_BOTH_DIR_INFORMATION, *PMX_FILE_ID_BOTH_DIR_INFORMATION;

typedef struct _MX_FILE_ID_FULL_DIR_INFORMATION {
  ULONG NextEntryOffset;
  ULONG FileIndex;
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  LARGE_INTEGER EndOfFile;
  LARGE_INTEGER AllocationSize;
  ULONG FileAttributes;
  ULONG FileNameLength;
  ULONG EaSize;
  LARGE_INTEGER FileId;
  WCHAR FileName[1];
} MX_FILE_ID_FULL_DIR_INFORMATION, *PMX_FILE_ID_FULL_DIR_INFORMATION;

//--------

typedef struct _MX_SECTION_BASIC_INFORMATION {
  PVOID BaseAddress;
  ULONG AllocationAttributes;
  LARGE_INTEGER MaximumSize;
} MX_SECTION_BASIC_INFORMATION, *PMX_SECTION_BASIC_INFORMATION;

typedef struct _MX_SECTION_IMAGE_INFORMATION {
  PVOID TransferAddress;
  ULONG ZeroBits;
  SIZE_T MaximumStackSize;
  SIZE_T CommittedStackSize;
  ULONG SubSystemType;
  union {
    struct {
      USHORT SubSystemMinorVersion;
      USHORT SubSystemMajorVersion;
    };
    ULONG SubSystemVersion;
  };
  ULONG GpValue;
  USHORT ImageCharacteristics;
  USHORT DllCharacteristics;
  USHORT Machine;
  BOOLEAN ImageContainsCode;
  union {
    UCHAR ImageFlags;
    struct {
      UCHAR ComPlusNativeReady : 1;
      UCHAR ComPlusILOnly : 1;
      UCHAR ImageDynamicallyRelocated : 1;
      UCHAR ImageMappedFlat : 1;
      UCHAR BaseBelow4gb : 1;
      UCHAR Reserved : 3;
    };
  };
  ULONG LoaderFlags;
  ULONG ImageFileSize;
  ULONG CheckSum;
} MX_SECTION_IMAGE_INFORMATION, *PMX_SECTION_IMAGE_INFORMATION;

//--------

typedef struct _MX_KEY_BASIC_INFORMATION {
  LARGE_INTEGER LastWriteTime;
  ULONG TitleIndex;
  ULONG NameLength;
  WCHAR Name[1];
} MX_KEY_BASIC_INFORMATION, *PMX_KEY_BASIC_INFORMATION;

typedef struct _MX_KEY_NODE_INFORMATION {
  LARGE_INTEGER LastWriteTime;
  ULONG TitleIndex;
  ULONG ClassOffset;
  ULONG ClassLength;
  ULONG NameLength;
  WCHAR Name[1];
} MX_KEY_NODE_INFORMATION, *PMX_KEY_NODE_INFORMATION;

typedef struct _MX_KEY_FULL_INFORMATION {
  LARGE_INTEGER LastWriteTime;
  ULONG TitleIndex;
  ULONG ClassOffset;
  ULONG ClassLength;
  ULONG SubKeys;
  ULONG MaxNameLen;
  ULONG MaxClassLen;
  ULONG Values;
  ULONG MaxValueNameLen;
  ULONG MaxValueDataLen;
  WCHAR Class[1];
} MX_KEY_FULL_INFORMATION, *PMX_KEY_FULL_INFORMATION;

typedef struct _MX_KEY_NAME_INFORMATION {
  ULONG NameLength;
  WCHAR Name[1];
} MX_KEY_NAME_INFORMATION, *PMX_KEY_NAME_INFORMATION;

typedef struct _MX_KEY_CACHED_INFORMATION {
  LARGE_INTEGER LastWriteTime;
  ULONG TitleIndex;
  ULONG SubKeys;
  ULONG MaxNameLen;
  ULONG Values;
  ULONG MaxValueNameLen;
  ULONG MaxValueDataLen;
  ULONG NameLength;
} MX_KEY_CACHED_INFORMATION, *PMX_KEY_CACHED_INFORMATION;

typedef struct _MX_KEY_VIRTUALIZATION_INFORMATION {
  ULONG VirtualizationCandidate : 1;
  ULONG VirtualizationEnabled   : 1;
  ULONG VirtualTarget           : 1;
  ULONG VirtualStore            : 1;
  ULONG VirtualSource           : 1;
  ULONG Reserved                : 27;
} MX_KEY_VIRTUALIZATION_INFORMATION, *PMX_KEY_VIRTUALIZATION_INFORMATION;

typedef struct _MX_KEY_VALUE_BASIC_INFORMATION {
  ULONG TitleIndex;
  ULONG Type;
  ULONG NameLength;
  WCHAR Name[1];
} MX_KEY_VALUE_BASIC_INFORMATION, *PMX_KEY_VALUE_BASIC_INFORMATION;

typedef struct _MX_KEY_VALUE_PARTIAL_INFORMATION {
  ULONG TitleIndex;
  ULONG Type;
  ULONG DataLength;
  UCHAR Data[1];
} MX_KEY_VALUE_PARTIAL_INFORMATION, *PMX_KEY_VALUE_PARTIAL_INFORMATION;

typedef struct _MX_KEY_VALUE_FULL_INFORMATION {
  ULONG TitleIndex;
  ULONG Type;
  ULONG DataOffset;
  ULONG DataLength;
  ULONG NameLength;
  WCHAR Name[1];
} MX_KEY_VALUE_FULL_INFORMATION, *PMX_KEY_VALUE_FULL_INFORMATION;

typedef struct _MX_KEY_VALUE_ENTRY {
  PMX_UNICODE_STRING ValueName;
  ULONG DataLength;
  ULONG DataOffset;
  ULONG Type;
} MX_KEY_VALUE_ENTRY, *PMX_KEY_VALUE_ENTRY;

//--------

typedef struct _MX_CLIENT_ID {
  SIZE_T UniqueProcess;
  SIZE_T UniqueThread;
} MX_CLIENT_ID, *PMX_CLIENT_ID;

typedef struct _MX_SYSTEM_THREAD_INFORMATION {
  LARGE_INTEGER KernelTime;
  LARGE_INTEGER UserTime;
  LARGE_INTEGER CreateTime;
  ULONG WaitTime;
  PVOID StartAddress;
  MX_CLIENT_ID ClientId;
  MX_KPRIORITY Priority;
  LONG BasePriority;
  ULONG ContextSwitches;
  ULONG ThreadState;
  MX_KWAIT_REASON WaitReason;
} MX_SYSTEM_THREAD_INFORMATION, *PMX_SYSTEM_THREAD_INFORMATION;

typedef struct _MX_SYSTEM_EXTENDED_THREAD_INFORMATION {
  MX_SYSTEM_THREAD_INFORMATION ThreadInfo;
  PVOID StackBase;
  PVOID StackLimit;
  PVOID Win32StartAddress;
  PVOID TebAddress; //Vista or later
  ULONG Reserved1;
  ULONG Reserved2;
  ULONG Reserved3;
} MX_SYSTEM_EXTENDED_THREAD_INFORMATION, *PMX_SYSTEM_EXTENDED_THREAD_INFORMATION;

typedef struct _MX_SYSTEM_PROCESS_INFORMATION {
  ULONG NextEntryOffset;
  ULONG NumberOfThreads;
  LARGE_INTEGER WorkingSetPrivateSize; //VISTA
  ULONG HardFaultCount; //WIN7
  ULONG NumberOfThreadsHighWatermark; //WIN7
  ULONGLONG CycleTime; //WIN7
  LARGE_INTEGER CreateTime;
  LARGE_INTEGER UserTime;
  LARGE_INTEGER KernelTime;
  MX_UNICODE_STRING ImageName;
  MX_KPRIORITY BasePriority;
  HANDLE UniqueProcessId;
  HANDLE InheritedFromUniqueProcessId;
  ULONG HandleCount;
  ULONG SessionId;
  ULONG_PTR PageDirectoryBase;
  //----
  SIZE_T PeakVirtualSize;
  SIZE_T VirtualSize;
  ULONG PageFaultCount;
  SIZE_T PeakWorkingSetSize;
  SIZE_T WorkingSetSize;
  SIZE_T QuotaPeakPagedPoolUsage;
  SIZE_T QuotaPagedPoolUsage;
  SIZE_T QuotaPeakNonPagedPoolUsage;
  SIZE_T QuotaNonPagedPoolUsage;
  SIZE_T PagefileUsage;
  SIZE_T PeakPagefileUsage;
  SIZE_T PrivatePageCount;
  //IO_COUNTERS
  LARGE_INTEGER ReadOperationCount;
  LARGE_INTEGER WriteOperationCount;
  LARGE_INTEGER OtherOperationCount;
  LARGE_INTEGER ReadTransferCount;
  LARGE_INTEGER WriteTransferCount;
  LARGE_INTEGER OtherTransferCount;
  union {
    MX_SYSTEM_THREAD_INFORMATION Threads[1];
    MX_SYSTEM_EXTENDED_THREAD_INFORMATION ExtThreads[1];
  };
} MX_SYSTEM_PROCESS_INFORMATION, *PMX_SYSTEM_PROCESS_INFORMATION;

typedef struct _MX_SYSTEM_PROCESSOR_INFORMATION {
  USHORT ProcessorArchitecture;
  USHORT ProcessorLevel;
  USHORT ProcessorRevision;
  USHORT Reserved;
  ULONG ProcessorFeatureBits;
} MX_SYSTEM_PROCESSOR_INFORMATION, *PMX_SYSTEM_PROCESSOR_INFORMATION;

typedef struct _MX_SYSTEM_SESSION_PROCESS_INFORMATION {
  ULONG SessionId;
  ULONG SizeOfBuf;
  PVOID Buffer;
} MX_SYSTEM_SESSION_PROCESS_INFORMATION, *PMX_SYSTEM_SESSION_PROCESS_INFORMATION;

typedef struct _MX_PROCESS_SESSION_INFORMATION {
  ULONG SessionId;
} MX_PROCESS_SESSION_INFORMATION, *PMX_PROCESS_SESSION_INFORMATION;

typedef struct _MX_THREAD_BASIC_INFORMATION {
  NTSTATUS ExitStatus;
  PVOID TebBaseAddress;
  MX_CLIENT_ID ClientId;
  ULONG_PTR AffinityMask;
  MX_KPRIORITY Priority;
  LONG BasePriority;
} MX_THREAD_BASIC_INFORMATION, *PMX_THREAD_BASIC_INFORMATION;

typedef struct _MX_LIST_ENTRY32 {
  ULONG Flink;
  ULONG Blink;
} MX_LIST_ENTRY32, *PMX_LIST_ENTRY32;

typedef struct _MX_LIST_ENTRY64 {
  ULONGLONG Flink;
  ULONGLONG Blink;
} MX_LIST_ENTRY64, *PMX_LIST_ENTRY64;

typedef struct _MX_LIST_ENTRY {
  struct _MX_LIST_ENTRY *Flink;
  struct _MX_LIST_ENTRY *Blink;
} MX_LIST_ENTRY, *PMX_LIST_ENTRY;

typedef struct _MX_LDR_DATA_TABLE_ENTRY32 {
  MX_LIST_ENTRY32 InLoadOrderLinks;
  MX_LIST_ENTRY32 InMemoryOrderLinks;
  MX_LIST_ENTRY32 InInitializationOrderLinks;
  ULONG DllBase;
  ULONG EntryPoint;
  ULONG SizeOfImage;
  MX_UNICODE_STRING32 FullDllName;
  MX_UNICODE_STRING32 BaseDllName;
  ULONG  Flags;
  USHORT LoadCount;
  //... the rest is unused
} MX_LDR_DATA_TABLE_ENTRY32, *PMX_LDR_DATA_TABLE_ENTRY32;

typedef struct _MX_LDR_DATA_TABLE_ENTRY64 {
  MX_LIST_ENTRY64 InLoadOrderLinks;
  MX_LIST_ENTRY64 InMemoryOrderLinks;
  MX_LIST_ENTRY64 InInitializationOrderLinks;
  ULONGLONG DllBase;
  ULONGLONG EntryPoint;
  ULONGLONG SizeOfImage;
  MX_UNICODE_STRING64 FullDllName;
  MX_UNICODE_STRING64 BaseDllName;
  ULONG  Flags;
  USHORT LoadCount;
  //... the rest is unused
} MX_LDR_DATA_TABLE_ENTRY64, *PMX_LDR_DATA_TABLE_ENTRY64;

typedef struct _MX_LDR_DATA_TABLE_ENTRY {
  MX_LIST_ENTRY InLoadOrderLinks;
  MX_LIST_ENTRY InMemoryOrderLinks;
  MX_LIST_ENTRY InInitializationOrderLinks;
  PVOID DllBase;
  PVOID EntryPoint;
  SIZE_T SizeOfImage;
  MX_UNICODE_STRING FullDllName;
  MX_UNICODE_STRING BaseDllName;
  ULONG  Flags;
  USHORT LoadCount;
  //... the rest is unused
} MX_LDR_DATA_TABLE_ENTRY, *PMX_LDR_DATA_TABLE_ENTRY;

typedef struct _MX_PEB_LDR_DATA {
  ULONG Length;
  BOOLEAN Initialized;
  HANDLE SsHandle;
  MX_LIST_ENTRY InLoadOrderModuleList;
  MX_LIST_ENTRY InMemoryOrderModuleList;
  MX_LIST_ENTRY InInitializationOrderModuleList;
  PVOID EntryInProgress;
  BOOLEAN ShutdownInProgress;
  HANDLE ShutdownThreadId;
} MX_PEB_LDR_DATA, *PMX_PEB_LDR_DATA;

#pragma pack(4)
typedef struct _MX_PROCESS_DEVICEMAP_INFORMATION {
  union {
    struct {
      HANDLE DirectoryHandle;
    } Set;
    struct {
      ULONG DriveMap;
      UCHAR DriveType[32];
    } Query;
  };
} MX_PROCESS_DEVICEMAP_INFORMATION, *PMX_PROCESS_DEVICEMAP_INFORMATION;
#pragma pack()

typedef struct _MX_OBJECT_NAME_INFORMATION {
  MX_UNICODE_STRING Name;
  WCHAR Buffer[1];
} MX_OBJECT_NAME_INFORMATION, *PMX_OBJECT_NAME_INFORMATION;

typedef struct _MX_KNONVOLATILE_CONTEXT_POINTERS {
  union {
    PM128A FloatingContext[16];
    struct {
      PM128A Xmm0;
      PM128A Xmm1;
      PM128A Xmm2;
      PM128A Xmm3;
      PM128A Xmm4;
      PM128A Xmm5;
      PM128A Xmm6;
      PM128A Xmm7;
      PM128A Xmm8;
      PM128A Xmm9;
      PM128A Xmm10;
      PM128A Xmm11;
      PM128A Xmm12;
      PM128A Xmm13;
      PM128A Xmm14;
      PM128A Xmm15;
    };
  };
  union {
    PULONG64 IntegerContext[16];
    struct {
      PULONG64 Rax;
      PULONG64 Rcx;
      PULONG64 Rdx;
      PULONG64 Rbx;
      PULONG64 Rsp;
      PULONG64 Rbp;
      PULONG64 Rsi;
      PULONG64 Rdi;
      PULONG64 R8;
      PULONG64 R9;
      PULONG64 R10;
      PULONG64 R11;
      PULONG64 R12;
      PULONG64 R13;
      PULONG64 R14;
      PULONG64 R15;
    };
  };
} MX_KNONVOLATILE_CONTEXT_POINTERS, *PMX_KNONVOLATILE_CONTEXT_POINTERS;
#pragma pack()

//-----------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#define __DECLARE(ret, apiName) ret NTAPI Mx##apiName
__DECLARE(NTSTATUS, NtOpenProcess)(_Out_ PHANDLE ProcessHandle, _In_ ACCESS_MASK DesiredAccess,
                                   _In_ PMX_OBJECT_ATTRIBUTES ObjectAttributes, _In_opt_ PVOID ClientId);

__DECLARE(NTSTATUS, NtOpenThread)(_Out_ PHANDLE ThreadHandle, _In_ ACCESS_MASK DesiredAccess,
                                  _In_ PMX_OBJECT_ATTRIBUTES ObjectAttributes, _In_opt_ PVOID ClientId);

__DECLARE(NTSTATUS, NtOpenProcessToken)(_In_ HANDLE ProcessHandle, _In_ ACCESS_MASK DesiredAccess,
                                        _Out_ PHANDLE TokenHandle);
__DECLARE(NTSTATUS, NtOpenThreadToken)(_In_ HANDLE ThreadHandle, _In_ ACCESS_MASK DesiredAccess,
                                       _In_ BOOLEAN OpenAsSelf, _Out_ PHANDLE TokenHandle);

__DECLARE(NTSTATUS, NtDuplicateToken)(_In_ HANDLE ExistingTokenHandle, _In_ ACCESS_MASK DesiredAccess,
                                      _In_ PMX_OBJECT_ATTRIBUTES ObjectAttributes, _In_ BOOLEAN EffectiveOnly,
                                      _In_ TOKEN_TYPE TokenType, _Out_ PHANDLE NewTokenHandle);

__DECLARE(NTSTATUS, NtQueryInformationToken)(_In_ HANDLE TokenHandle, _In_ TOKEN_INFORMATION_CLASS TokenInfoClass,
                                             _Out_ PVOID TokenInfo, _In_ ULONG TokenInfoLength,
                                             _Out_ PULONG ReturnLength);
__DECLARE(NTSTATUS, NtSetInformationToken)(_In_ HANDLE TokenHandle, _In_ TOKEN_INFORMATION_CLASS TokenInfoClass,
                                           _In_ PVOID TokenInfo, _In_ ULONG TokenInfoLength);

//--------

__DECLARE(NTSTATUS, NtGetContextThread)(_In_ HANDLE ThreadHandle, _Out_ PCONTEXT Context);
__DECLARE(NTSTATUS, NtSetContextThread)(_In_ HANDLE ThreadHandle, _In_ PCONTEXT Context);

__DECLARE(NTSTATUS, RtlCreateUserThread)(_In_ HANDLE ProcessHandle, _In_opt_ PSECURITY_DESCRIPTOR SecurityDescriptor,
                                         _In_ BOOLEAN CreateSuspended, _In_ ULONG StackZeroBits,
                                         _Inout_ PULONG StackReserved, _Inout_ PULONG StackCommit,
                                         _In_ PVOID StartAddress, _In_opt_ PVOID StartParameter,
                                         _Out_ PHANDLE ThreadHandle, _Out_ PVOID ClientID);

//--------

__DECLARE(NTSTATUS, NtCreateEvent)(_Out_ PHANDLE EventHandle, _In_ ACCESS_MASK DesiredAccess,
                                   _In_opt_ PMX_OBJECT_ATTRIBUTES ObjectAttributes, _In_ ULONG EventType,
                                   _In_ BOOLEAN InitialState);
__DECLARE(NTSTATUS, NtOpenEvent)(_Out_ PHANDLE EventHandle, _In_ ACCESS_MASK DesiredAccess,
                                 _In_ PMX_OBJECT_ATTRIBUTES ObjectAttributes);
__DECLARE(NTSTATUS, NtResetEvent)(_In_ HANDLE EventHandle, _Out_opt_ PLONG NumberOfWaitingThreads);
__DECLARE(NTSTATUS, NtSetEvent)(_In_ HANDLE EventHandle, _Out_opt_ PLONG NumberOfWaitingThreads);

//--------

__DECLARE(NTSTATUS, NtCreateMutant)(_Out_ PHANDLE MutantHandle, _In_ ACCESS_MASK DesiredAccess,
                                    _In_ PMX_OBJECT_ATTRIBUTES ObjectAttributes, _In_ BOOLEAN InitialOwner);
__DECLARE(NTSTATUS, NtOpenMutant)(_Out_ PHANDLE MutantHandle, _In_ ACCESS_MASK DesiredAccess,
                                  _In_ PMX_OBJECT_ATTRIBUTES ObjectAttributes);
__DECLARE(NTSTATUS, NtReleaseMutant)(_In_ HANDLE MutantHandle, _Out_opt_ PLONG PreviousCount);

//--------

__DECLARE(NTSTATUS, NtCreateFile)(_Out_ PHANDLE FileHandle, _In_ ACCESS_MASK DesiredAccess,
                                  _In_ PMX_OBJECT_ATTRIBUTES ObjectAttributes, _Out_ PMX_IO_STATUS_BLOCK IoStatusBlock,
                                  _In_opt_ PLARGE_INTEGER AllocationSize, _In_ ULONG FileAttributes,
                                  _In_ ULONG ShareAccess, _In_ ULONG CreateDisposition, _In_ ULONG CreateOptions,
                                  _In_opt_ PVOID EaBuffer, _In_ ULONG EaLength);
__DECLARE(NTSTATUS, NtOpenFile)(_Out_ PHANDLE FileHandle, _In_ ACCESS_MASK DesiredAccess,
                                _In_ PMX_OBJECT_ATTRIBUTES ObjectAttributes, _Out_ PMX_IO_STATUS_BLOCK IoStatusBlock,
                                _In_ ULONG ShareAccess, _In_ ULONG OpenOptions);
__DECLARE(NTSTATUS, NtOpenDirectoryObject)(_Out_ PHANDLE FileHandle, _In_ ACCESS_MASK DesiredAccess,
                                          _In_ PMX_OBJECT_ATTRIBUTES ObjectAttributes);
__DECLARE(NTSTATUS, NtOpenSymbolicLinkObject)(_Out_ PHANDLE SymbolicLinkHandle, _In_ ACCESS_MASK DesiredAccess,
                                             _In_ PMX_OBJECT_ATTRIBUTES ObjectAttributes);
__DECLARE(NTSTATUS, NtQuerySymbolicLinkObject)(_In_ HANDLE SymLinkObjHandle, _Out_ PMX_UNICODE_STRING LinkTarget,
                                               _Out_opt_ PULONG DataWritten);
__DECLARE(NTSTATUS, NtClose)(_In_ HANDLE Handle);
__DECLARE(NTSTATUS, NtReadFile)(_In_ HANDLE FileHandle, _In_opt_ HANDLE Event, _In_opt_ PVOID ApcRoutine,
                                _In_opt_ PVOID ApcContext, _Out_ PMX_IO_STATUS_BLOCK IoStatusBlock, _Out_ PVOID Buffer,
                                _In_ ULONG Length, _In_opt_ PLARGE_INTEGER ByteOffset, _In_opt_ PULONG Key);
__DECLARE(NTSTATUS, NtWriteFile)(_In_ HANDLE FileHandle, _In_opt_ HANDLE Event, _In_opt_ PVOID ApcRoutine,
                                 _In_opt_ PVOID ApcContext, _Out_ PMX_IO_STATUS_BLOCK IoStatusBlock, _Out_ PVOID Buffer,
                                 _In_ ULONG Length, _In_opt_ PLARGE_INTEGER ByteOffset, _In_opt_ PULONG Key);
__DECLARE(NTSTATUS, NtCancelIoFile)(_In_ HANDLE FileHandle, _Out_ PMX_IO_STATUS_BLOCK IoStatusBlock);
__DECLARE(NTSTATUS, NtQueryInformationFile)(_In_ HANDLE hFile, _Out_ PMX_IO_STATUS_BLOCK IoStatusBlock,
                                            _Out_ PVOID FileInformationBuffer, _In_ ULONG FileInformationBufferLength,
                                            _In_ ULONG FileInfoClass);
__DECLARE(NTSTATUS, NtSetInformationFile)(_In_ HANDLE hFile, _Out_ PMX_IO_STATUS_BLOCK IoStatusBlock,
                                          _In_ PVOID FileInformationBuffer, _In_ ULONG FileInformationBufferLength,
                                          _In_ ULONG FileInfoClass);

//--------

__DECLARE(NTSTATUS, NtCreateKey)(_Out_ PHANDLE KeyHandle, _In_ ACCESS_MASK DesiredAccess,
                                 _In_ PMX_OBJECT_ATTRIBUTES ObjectAttributes, _In_ ULONG TitleIndex,
                                 _In_opt_ PMX_UNICODE_STRING Class, _In_ ULONG CreateOptions,
                                 _Out_opt_ PULONG Disposition);
__DECLARE(NTSTATUS, NtOpenKey)(_Out_ PHANDLE KeyHandle, _In_ ACCESS_MASK DesiredAccess,
                               _In_ PMX_OBJECT_ATTRIBUTES ObjectAttributes);
__DECLARE(NTSTATUS, NtEnumerateKey)(_In_ HANDLE KeyHandle, _In_ ULONG Index, _In_ ULONG KeyInfoClass,
                                    _Out_ PVOID KeyInformation, _In_ ULONG Length, _Out_ PULONG ResultLength);
__DECLARE(NTSTATUS, NtEnumerateValueKey)(_In_ HANDLE KeyHandle, _In_ ULONG Index,
                                         _In_ ULONG KeyValueInfoClass,
                                         _Out_ PVOID KeyValueInformation, _In_ ULONG Length, _Out_ PULONG ResultLength);
__DECLARE(NTSTATUS, NtQueryKey)(_In_ HANDLE KeyHandle, _In_ ULONG KeyInfoClass,
                                _Out_ PVOID KeyInformation, _In_ ULONG Length, _Out_ PULONG ResultLength);
__DECLARE(NTSTATUS, NtQueryValueKey)(_In_ HANDLE KeyHandle, _In_ PMX_UNICODE_STRING ValueName, _In_ ULONG KeyValueInfoClass,
                                     _Out_ PVOID KeyValueInformation, _In_ ULONG Length, _Out_ PULONG ResultLength);
__DECLARE(NTSTATUS, NtSetValueKey)(_In_ HANDLE KeyHandle, _In_ PMX_UNICODE_STRING ValueName, _In_ ULONG TitleIndex,
                                   _In_ ULONG Type, _In_ PVOID Data, _In_ ULONG DataSize);
__DECLARE(NTSTATUS, NtDeleteKey)(_In_ HANDLE KeyHandle);
__DECLARE(NTSTATUS, NtDeleteValueKey)(_In_ HANDLE KeyHandle, _In_ PMX_UNICODE_STRING ValueName);
__DECLARE(NTSTATUS, NtFlushKey)(_In_ HANDLE KeyHandle);

//--------

__DECLARE(NTSTATUS, NtDelayExecution)(_In_ BOOLEAN Alertable, _In_ PLARGE_INTEGER DelayInterval);
__DECLARE(NTSTATUS, NtYieldExecution)();
__DECLARE(NTSTATUS, NtQuerySystemTime)(_Out_ PULARGE_INTEGER SystemTime); //defined as unsigned on purpose
__DECLARE(NTSTATUS, NtQueryPerformanceCounter)(_Out_ PLARGE_INTEGER Counter, _Out_opt_ PLARGE_INTEGER Frequency);

//--------

__DECLARE(NTSTATUS, RtlInitializeCriticalSection)(_In_ RTL_CRITICAL_SECTION* crit);
__DECLARE(NTSTATUS, RtlInitializeCriticalSectionAndSpinCount)(_In_ RTL_CRITICAL_SECTION* crit, _In_ ULONG spincount);
__DECLARE(NTSTATUS, RtlDeleteCriticalSection)(_In_ RTL_CRITICAL_SECTION* crit);
__DECLARE(NTSTATUS, RtlEnterCriticalSection)(_In_ RTL_CRITICAL_SECTION* crit);
__DECLARE(NTSTATUS, RtlLeaveCriticalSection)(_In_ RTL_CRITICAL_SECTION* crit);
__DECLARE(BOOLEAN, RtlTryEnterCriticalSection)(_In_ RTL_CRITICAL_SECTION* crit);

//--------

__DECLARE(NTSTATUS, RtlGetNativeSystemInformation)(_In_ ULONG SystemInformationClass, _Inout_ PVOID SystemInformation,
                                                   _In_ ULONG SystemInformationLength, _Out_opt_ PULONG ReturnLength);
__DECLARE(NTSTATUS, NtQuerySystemInformation)(_In_ ULONG SystemInformationClass, _Inout_ PVOID SystemInformation,
                                              _In_ ULONG SystemInformationLength, _Out_opt_ PULONG ReturnLength);

__DECLARE(NTSTATUS, NtQueryInformationProcess)(_In_ HANDLE ProcessHandle, _In_ ULONG ProcessInfoClass,
                                               _Out_opt_ PVOID ProcessInfo, _In_ ULONG ProcessInfoLength,
                                               _Out_opt_ PULONG ReturnLength);
__DECLARE(NTSTATUS, NtSetInformationProcess)(_In_ HANDLE ProcessHandle, _In_ ULONG ProcessInfoClass,
                                             _In_ PVOID ProcessInformation, _In_ ULONG ProcessInformationLength);
__DECLARE(NTSTATUS, NtQueryInformationThread)(_In_ HANDLE ThreadHandle, _In_ ULONG ThreadInfoClass,
                                              _Out_opt_ PVOID ThreadInfo, _In_ ULONG ThreadInfoLength,
                                              _Out_opt_ PULONG ReturnLength);
__DECLARE(NTSTATUS, NtSetInformationThread)(_In_ HANDLE ThreadHandle, _In_ ULONG ThreadInformationClass,
                                            _In_ PVOID ThreadInformation, _In_ ULONG ThreadInformationLength);

__DECLARE(NTSTATUS, NtQueryObject)(_In_opt_ HANDLE Handle, _In_ ULONG ObjectInformationClass,
                                   _Out_opt_ PVOID ObjectInformation, _In_ ULONG ObjectInformationLength,
                                   _Out_opt_ PULONG ReturnLength);

//--------

__DECLARE(NTSTATUS, NtWaitForSingleObject)(_In_ HANDLE Handle, _In_ BOOLEAN Alertable, _In_opt_ PLARGE_INTEGER Timeout);
__DECLARE(NTSTATUS, NtWaitForMultipleObjects)(_In_ ULONG Count, _In_ HANDLE Object[], _In_ LONG WaitType,
                                              _In_ BOOLEAN Alertable, _In_ PLARGE_INTEGER Time);

//--------

__DECLARE(NTSTATUS, NtAllocateVirtualMemory)(_In_ HANDLE ProcessHandle, _Inout_ PVOID *BaseAddress,
                                             _In_ ULONG_PTR ZeroBits, _Inout_ PSIZE_T RegionSize,
                                             _In_ ULONG AllocationType, _In_ ULONG Protect);
__DECLARE(NTSTATUS, NtFreeVirtualMemory)(_In_ HANDLE ProcessHandle, _Inout_ PVOID *BaseAddress,
                                         _Inout_ PSIZE_T RegionSize, _In_ ULONG FreeType);
__DECLARE(NTSTATUS, NtFlushVirtualMemory)(_In_ HANDLE ProcessHandle, _Inout_ PVOID *BaseAddress,
                                          _Inout_ PSIZE_T RegionSize, _Out_ PMX_IO_STATUS_BLOCK IoStatus);
__DECLARE(NTSTATUS, NtReadVirtualMemory)(_In_ HANDLE ProcessHandle, _In_ PVOID BaseAddress, _Out_ PVOID Buffer,
                                         _In_ SIZE_T NumberOfBytesToRead, _Out_opt_ PSIZE_T NumberOfBytesReaded);
__DECLARE(NTSTATUS, NtWriteVirtualMemory)(_In_ HANDLE ProcessHandle, _In_ PVOID BaseAddress, _Out_ PVOID Buffer,
                                          _In_ SIZE_T NumberOfBytesToWrite, _Out_opt_ PSIZE_T NumberOfBytesWritten);
__DECLARE(NTSTATUS, NtQueryVirtualMemory)(_In_ HANDLE ProcessHandle, _In_opt_ PVOID Address,
                                          _In_ ULONG VirtualMemoryInformationClass,
                                          _Out_ PVOID VirtualMemoryInformation, _In_ SIZE_T Length,
                                          _Out_opt_ PSIZE_T ResultLength);
__DECLARE(NTSTATUS, NtProtectVirtualMemory)(_In_ HANDLE ProcessHandle, _Inout_ PVOID *UnsafeBaseAddress,
                                            _Inout_ SIZE_T *UnsafeNumberOfBytesToProtect,
                                            _In_ ULONG NewAccessProtection, _Out_ PULONG UnsafeOldAccessProtection);

//--------

__DECLARE(NTSTATUS, NtCreateSection)(_Out_ PHANDLE SectionHandle, _In_ ACCESS_MASK DesiredAccess,
                                     _In_opt_ PMX_OBJECT_ATTRIBUTES ObjectAttributes,
                                     _In_opt_ PLARGE_INTEGER MaximumSize, _In_ ULONG SectionPageProtection,
                                     _In_ ULONG AllocationAttributes, _In_opt_ HANDLE FileHandle);
__DECLARE(NTSTATUS, NtQuerySection)(_In_ HANDLE SectionHandle, _In_ ULONG InformationClass,
                                    _Out_ PVOID InformationBuffer, _In_ ULONG InformationBufferSize,
                                    _Out_opt_ PULONG ResultLength);
__DECLARE(NTSTATUS, NtMapViewOfSection)(_In_ HANDLE SectionHandle, _In_ HANDLE ProcessHandle,
                                        _Inout_ PVOID *BaseAddress, _In_ ULONG_PTR ZeroBits, _In_ SIZE_T CommitSize,
                                        _Inout_ PLARGE_INTEGER SectionOffset, _Inout_ PSIZE_T ViewSize,
                                        _In_ ULONG InheritDisposition, _In_ ULONG AllocationType,
                                        _In_ ULONG Win32Protect);
__DECLARE(NTSTATUS, NtUnmapViewOfSection)(_In_ HANDLE ProcessHandle, _In_ PVOID BaseAddress);

//--------

__DECLARE(NTSTATUS, NtSuspendThread)(_In_ HANDLE ThreadHandle, _Out_opt_ PULONG PreviousSuspendCount);
__DECLARE(NTSTATUS, NtResumeThread)(_In_ HANDLE ThreadHandle, _Out_opt_ PULONG SuspendCount);

//--------

__DECLARE(NTSTATUS, RtlImpersonateSelf)(_In_ SECURITY_IMPERSONATION_LEVEL ImpersonationLevel);

__DECLARE(NTSTATUS, NtAdjustPrivilegesToken)(_In_ HANDLE TokenHandle, _In_ BOOLEAN DisableAllPrivileges,
                                             _In_opt_ PTOKEN_PRIVILEGES NewState, _In_ ULONG BufferLength,
                                             _Out_ PTOKEN_PRIVILEGES PreviousState, _Out_opt_ PULONG ReturnLength);

__DECLARE(VOID, RtlRaiseException)(_In_ PEXCEPTION_RECORD ExceptionRecord);

__DECLARE(ULONG, RtlNtStatusToDosError)(_In_ NTSTATUS Status);

__DECLARE(NTSTATUS, NtFlushInstructionCache)(_In_ HANDLE ProcessHandle, _In_ PVOID BaseAddress,
                                             _In_ ULONG NumberOfBytesToFlush);

//--------

__DECLARE(PVOID, RtlAllocateHeap)(_In_ PVOID HeapHandle, _In_opt_ ULONG Flags, _In_ SIZE_T Size);
__DECLARE(PVOID, RtlReAllocateHeap)(_In_ PVOID HeapHandle, _In_opt_ ULONG Flags, _In_ PVOID Ptr, _In_ SIZE_T Size);
__DECLARE(BOOLEAN, RtlFreeHeap)(_In_ PVOID HeapHandle, _In_opt_ ULONG Flags, _Frees_ptr_opt_ PVOID HeapBase);
__DECLARE(PVOID, RtlCreateHeap)(_In_ ULONG Flags, _In_opt_ PVOID HeapBase, _In_opt_ SIZE_T ReserveSize,
                                _In_opt_ SIZE_T CommitSize, _In_opt_ PVOID Lock, _In_opt_ PVOID Parameters);
__DECLARE(PVOID, RtlDestroyHeap)(_In_ PVOID HeapHandle);
__DECLARE(SIZE_T, RtlSizeHeap)(_In_ PVOID HeapHandle, _In_opt_ ULONG Flags, _In_ PVOID Ptr);

//--------

__DECLARE(VOID, RtlUnwind)(_In_opt_ PVOID TargetFrame, _In_opt_ PVOID TargetIp,
                           _In_opt_ PEXCEPTION_RECORD ExceptionRecord, _In_ PVOID ReturnValue);
#if defined(_M_X64) || defined(_M_AMD64)
__DECLARE(VOID, RtlUnwindEx)(_In_ ULONGLONG TargetFrame, _In_ ULONGLONG TargetIp,
                             _In_opt_ PEXCEPTION_RECORD ExceptionRecord, _In_ PVOID ReturnValue,
                             _In_ PCONTEXT OriginalContext, _In_opt_ PVOID HistoryTable);
#endif //_M_X64 || _M_AMD64

//--------

__DECLARE(NTSTATUS, NtTerminateProcess)(_In_opt_ HANDLE ProcessHandle, _In_ NTSTATUS ExitStatus);
__DECLARE(NTSTATUS, NtTerminateThread)(_In_ HANDLE ThreadHandle, _In_ NTSTATUS ExitStatus);

__DECLARE(NTSTATUS, NtDuplicateObject)(_In_ HANDLE SourceProcessHandle, _In_ HANDLE SourceHandle,
                                       _In_opt_ HANDLE TargetProcessHandle, _Out_opt_ PHANDLE TargetHandle,
                                       _In_ ACCESS_MASK DesiredAccess, _In_ ULONG HandleAttr, _In_ ULONG Options);

//--------

__DECLARE(NTSTATUS, RtlGetVersion)(_Inout_ PRTL_OSVERSIONINFOW lpVersionInformation);

//--------

__DECLARE(NTSTATUS, LdrLoadDll)(_In_opt_ PWSTR SearchPath, _In_opt_ PULONG DllCharacteristics,
                                _In_ PMX_UNICODE_STRING DllName, _Out_ PVOID *BaseAddress);
__DECLARE(NTSTATUS, LdrUnloadDll)(_In_ PVOID BaseAddress);

__DECLARE(NTSTATUS, LdrFindEntryForAddress)(_In_ PVOID Address, _Out_ PVOID *lplpEntry);

__DECLARE(NTSTATUS, LdrGetProcedureAddress)(_In_ PVOID BaseAddress, _In_ PMX_ANSI_STRING Name, _In_ ULONG Ordinal,
                                            _Out_ PVOID *ProcedureAddress);

#if defined(_M_X64)
__DECLARE(VOID, RtlCaptureContext)(_Out_ PCONTEXT ContextRecord);

__DECLARE(PVOID, RtlLookupFunctionEntry)(_In_ ULONG64 ControlPc, _Out_ PULONG64 ImageBase,
                                         _Inout_opt_ PUNWIND_HISTORY_TABLE HistoryTable);

__DECLARE(PVOID, RtlVirtualUnwind)(_In_ DWORD HandlerType, _In_ DWORD64 ImageBase, _In_ DWORD64 ControlPc,
                                   _In_ PRUNTIME_FUNCTION FunctionEntry, _Inout_ PCONTEXT ContextRecord,
                                   _Out_ PVOID *HandlerData, _Out_ PDWORD64 EstablisherFrame,
                                   _Inout_opt_ MX_KNONVOLATILE_CONTEXT_POINTERS *ContextPointers);

__DECLARE(BOOLEAN, RtlAddFunctionTable)(_In_ PVOID FunctionTable, _In_ DWORD EntryCount, _In_ DWORD64 BaseAddress);
__DECLARE(BOOLEAN, RtlDeleteFunctionTable)(_In_ PVOID FunctionTable);
__DECLARE(BOOLEAN, RtlInstallFunctionTableCallback)(_In_ DWORD64 TableIdentifier, _In_ DWORD64 BaseAddress,
                                                    _In_ DWORD Length, _In_ PGET_RUNTIME_FUNCTION_CALLBACK Callback,
                                                    _In_ PVOID Context, _In_ PCWSTR OutOfProcessCallbackDll);
#endif //_M_X64

//--------

__DECLARE(NTSTATUS, RtlAnsiStringToUnicodeString)(_Inout_ PMX_UNICODE_STRING DestinationString,
                                                  _In_ PCMX_ANSI_STRING SourceString, _In_ BOOLEAN AllocDestString);
__DECLARE(ULONG, RtlAnsiStringToUnicodeSize)(_In_ PMX_ANSI_STRING AnsiString);
__DECLARE(VOID, RtlFreeUnicodeString)(_Inout_ PMX_UNICODE_STRING UnicodeString);

__DECLARE(NTSTATUS, RtlUnicodeStringToAnsiString)(_Inout_ PMX_ANSI_STRING DestinationString,
                                                  _In_ PCMX_UNICODE_STRING SourceString, _In_ BOOLEAN AllocDestString);
__DECLARE(ULONG, RtlUnicodeStringToAnsiSize)(_In_ PMX_UNICODE_STRING UnicodeString);
__DECLARE(VOID, RtlFreeAnsiString)(_Inout_ PMX_ANSI_STRING AnsiString);

__DECLARE(LONG, RtlCompareUnicodeString)(_In_ PCMX_UNICODE_STRING String1, _In_ PCMX_UNICODE_STRING String2,
                                         _In_ BOOLEAN CaseInsensitive);
__DECLARE(LONG, RtlCompareString)(_In_ PCMX_ANSI_STRING String1, _In_ PCMX_ANSI_STRING String2,
                                  _In_ BOOLEAN CaseInsensitive);

__DECLARE(WCHAR, RtlUpcaseUnicodeChar)(_In_ WCHAR SourceCharacter);
__DECLARE(WCHAR, RtlDowncaseUnicodeChar)(_In_ WCHAR SourceCharacter);
__DECLARE(CHAR, RtlUpperChar)(_In_ CHAR Character);

//--------

__DECLARE(PVOID, RtlAddVectoredExceptionHandler)(_In_ ULONG FirstHandler,
                                                 _In_ PVECTORED_EXCEPTION_HANDLER VectoredHandler);
__DECLARE(ULONG, RtlRemoveVectoredExceptionHandler)(_In_ PVOID VectoredHandlerHandle);

//--------

__DECLARE(ULONG, RtlFindClearBitsAndSet)(_In_ PVOID BitMapHeader, _In_ ULONG NumberToFind, _In_ ULONG HintIndex);
__DECLARE(BOOLEAN, RtlAreBitsSet)(_In_ PVOID BitMapHeader, _In_ ULONG StartingIndex, _In_ ULONG Length);
__DECLARE(VOID, RtlClearBits)(_In_ PVOID BitMapHeader, _In_ ULONG StartingIndex, _In_ ULONG NumberToClear);

__DECLARE(VOID, RtlAcquirePebLock)();
__DECLARE(VOID, RtlReleasePebLock)();
#undef __DECLARE

PVOID MxGetProcessHeap();

LPBYTE MxGetPeb();
LPBYTE MxGetRemotePeb(_In_ HANDLE hProcess);
LPBYTE MxGetTeb();

VOID MxSetLastWin32Error(_In_ DWORD dwOsErr);
DWORD MxGetLastWin32Error();

PRTL_CRITICAL_SECTION MxGetLoaderLockCS();

PVOID MxGetDllHandle(_In_z_ PCWSTR szModuleNameW);
PVOID __stdcall MxGetProcedureAddress(_In_ PVOID DllBase, _In_z_ PCSTR szApiNameA);

LONG MxGetProcessorArchitecture();

HANDLE MxOpenProcess(_In_ DWORD dwDesiredAccess, _In_ BOOL bInheritHandle, _In_ DWORD dwProcessId);
HANDLE MxOpenThread(_In_ DWORD dwDesiredAccess, _In_ BOOL bInheritHandle, _In_ DWORD dwThreadId);

NTSTATUS MxCreateFile(_Out_ HANDLE *lphFile, _In_z_ LPCWSTR szFileNameW, _In_ DWORD dwDesiredAccess,
                      _In_ DWORD dwShareMode, _In_ DWORD dwCreationDisposition, _In_ DWORD dwFlagsAndAttributes,
                      _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes);

//1 or 0 on success, STATUS_NOT_SUPPORTED if o.s. bitness is < 64 or STATUS_### on error
NTSTATUS MxIsWow64(_In_ HANDLE hProcess);

SIZE_T MxReadMem(_In_ HANDLE hProcess, _Out_writes_bytes_(nBytesCount) LPVOID lpDest, _In_ LPVOID lpSrc,
                 _In_ SIZE_T nBytesCount);
BOOL MxWriteMem(_In_ HANDLE hProcess, _In_ LPVOID lpDest, _In_ LPVOID lpSrc, _In_ SIZE_T nBytesCount);

NTSTATUS MxGetThreadPriority(_In_ HANDLE hThread, _Out_ int *lpnPriority);
NTSTATUS MxSetThreadPriority(_In_ HANDLE hThread, _In_ int _nPriority);

DWORD MxGetCurrentThreadId();
DWORD MxGetCurrentProcessId();

int mx_sprintf_s(_Out_writes_z_(nMaxCount) char *lpDest, _In_ size_t nMaxCount,
                 _In_z_ _Printf_format_string_ const char *szFormatA, ...);
int mx_vsnprintf(_Out_writes_z_(nMaxCount) char *lpDest, _In_ size_t nMaxCount,
                 _In_z_ _Printf_format_string_ const char *szFormatA, _In_ va_list lpArgList);
int mx_swprintf_s(_Out_writes_z_(nMaxCount) wchar_t *lpDest, _In_ size_t nMaxCount,
                  _In_z_ _Printf_format_string_ const wchar_t *szFormatW, ...);
int mx_vsnwprintf(_Out_writes_z_(nMaxCount) wchar_t *lpDest, _In_ size_t nMaxCount,
                  _In_z_ _Printf_format_string_ const wchar_t *szFormatW, _In_ va_list lpArgList);

#if defined(_M_IX86)
SIZE_T __stdcall MxCallWithSEH0(_In_ LPVOID lpFunc, _Out_opt_ BOOL *lpExceptionRaised);
SIZE_T __stdcall MxCallStdCallWithSEH1(_In_ LPVOID lpFunc, _Out_opt_ BOOL *lpExceptionRaised, _In_ SIZE_T nParam1);
SIZE_T __stdcall MxCallStdCallWithSEH2(_In_ LPVOID lpFunc, _Out_opt_ BOOL *lpExceptionRaised, _In_ SIZE_T nParam1,
                                       _In_ SIZE_T nParam2);
SIZE_T __stdcall MxCallStdCallWithSEH3(_In_ LPVOID lpFunc, _Out_opt_ BOOL *lpExceptionRaised, _In_ SIZE_T nParam1,
                                       _In_ SIZE_T nParam2, _In_ SIZE_T nParam3);
SIZE_T __stdcall MxCallCDeclWithSEH1(_In_ LPVOID lpFunc, _Out_opt_ BOOL *lpExceptionRaised, _In_ SIZE_T nParam1);
SIZE_T __stdcall MxCallCDeclWithSEH2(_In_ LPVOID lpFunc, _Out_opt_ BOOL *lpExceptionRaised, _In_ SIZE_T nParam1,
                                     _In_ SIZE_T nParam2);
SIZE_T __stdcall MxCallCDeclWithSEH3(_In_ LPVOID lpFunc, _Out_opt_ BOOL *lpExceptionRaised, _In_ SIZE_T nParam1,
                                     _In_ SIZE_T nParam2, _In_ SIZE_T nParam3);
#elif defined(_M_X64)
SIZE_T __stdcall MxCallWithSEH(_In_ LPVOID lpFunc, _Out_opt_ BOOL *lpExceptionRaised, _In_opt_ SIZE_T nParam1,
                               _In_opt_ SIZE_T nParam2, _In_opt_ SIZE_T nParam3);
#endif

VOID MxSleep(_In_ DWORD dwTimeMs);

#ifdef __cplusplus
}; //extern "C"
#endif //__cplusplus

//-----------------------------------------------------------

#endif //_MX_NTDEFS_H
