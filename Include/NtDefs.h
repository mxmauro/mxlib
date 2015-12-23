/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2006-2015. All rights reserved.
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

typedef enum {
  MxSystemBasicInformation = 0,
  MxSystemProcessorInformation = 1,
  MxSystemProcessInformation = 5,
  MxSystemSessionProcessesInformation = 53,
  MxSystemExtendedProcessInformation = 57
} MX_SYSTEM_INFORMATION_CLASS;

typedef enum {
  MxProcessBasicInformation = 0,
  MxProcessQuotaLimits,                //1
  MxProcessIoCounters,                 //2
  MxProcessVmCounters,                 //3
  MxProcessTimes,                      //4
  MxProcessBasePriority,               //5
  MxProcessRaisePriority,              //6
  MxProcessDebugPort,                  //7
  MxProcessExceptionPort,              //8
  MxProcessAccessToken,                //9
  MxProcessLdtInformation,             //10
  MxProcessLdtSize,                    //11
  MxProcessDefaultHardErrorMode,       //12
  MxProcessIoPortHandlers,             //13
  MxProcessPooledUsageAndLimits,       //14
  MxProcessWorkingSetWatch,            //15
  MxProcessUserModeIOPL,               //16
  MxProcessEnableAlignmentFaultFixup,  //17
  MxProcessPriorityClass,              //18
  MxProcessWx86Information,            //19
  MxProcessHandleCount,                //20
  MxProcessAffinityMask,               //21
  MxProcessPriorityBoost,              //22
  MxProcessDeviceMap,                  //23
  MxProcessSessionInformation,         //24
  MxProcessForegroundInformation,      //25
  MxProcessWow64Information,           //26
  MxProcessImageFileName,              //27
  MxProcessLUIDDeviceMapsEnabled,      //28
  MxProcessBreakOnTermination,         //29
  MxProcessDebugObjectHandle,          //30
  MxProcessDebugFlags,                 //31
  MxProcessHandleTracing,              //32
  MxProcessIoPriority,                 //33
  MxProcessExecuteFlags,               //34
  MxProcessTlsInformation,             //35
  MxProcessCookie,                     //36
  MxProcessImageInformation,           //37
  MxProcessCycleTime,                  //38
  MxProcessPagePriority,               //39
  MxProcessInstrumentationCallback,    //40
  MxProcessThreadStackAllocation,      //41
  MxProcessWorkingSetWatchEx,          //42
  MxProcessImageFileNameWin32,         //43
  MxProcessImageFileMapping,           //44
  MxProcessAffinityUpdateMode,         //45
  MxProcessMemoryAllocationMode,       //46
  MxProcessGroupInformation,           //47
  MxProcessTokenVirtualizationEnabled, //48
  MxProcessConsoleHostMxProcess,       //49
  MxProcessWindowInformation           //50
} MX_PROCESS_INFORMATION_CLASS;

typedef enum {
  MxThreadBasicInformation = 0,
  MxThreadTimes,                     //1
  MxThreadPriority,                  //2
  MxThreadBasePriority,              //3
  MxThreadAffinityMask,              //4
  MxThreadImpersonationToken,        //5
  MxThreadDescriptorTableEntry,      //6
  MxThreadEnableAlignmentFaultFixup, //7
  MxThreadEventPair_Reusable,        //8
  MxThreadQuerySetWin32StartAddress, //9
  MxThreadZeroTlsCell,               //10
  MxThreadPerformanceCount,          //11
  MxThreadAmILastThread,             //12
  MxThreadIdealProcessor,            //13
  MxThreadPriorityBoost,             //14
  MxThreadSetTlsArrayAddress,        //15
  MxThreadIsIoPending,               //16
  MxThreadHideFromDebugger,          //17
  MxThreadBreakOnTermination,        //18
  MxThreadSwitchLegacyState,         //19
  MxThreadIsTerminated,              //20
  MxThreadLastSystemCall,            //21
  MxThreadIoPriority,                //22
  MxThreadCycleTime,                 //23
  MxThreadPagePriority,              //24
  MxThreadActualBasePriority,        //25
  MxThreadTebInformation,            //26
  MxThreadCSwitchMon,                //27
  MxThreadCSwitchPmu,                //28
  MxThreadWow64Context,              //29
  MxThreadGroupInformation,          //30
  MxThreadUmsInformation,            //31
  MxThreadCounterProfiling,          //32
  MxThreadIdealProcessorEx           //33
} MX_THREAD_INFORMATION_CLASS;

typedef enum {
  MxMemoryBasicInformation = 0,
  MxMemoryWorkingSetList,      //1
  MxMemorySectionName,         //2
  MxMemoryBasicVlmInformation, //3
  MxMemoryWorkingSetExList     //4
} MX_MEMORY_INFORMATION_CLASS;

typedef enum {
  MxSectionBasicInformation = 0,
  MxSectionImageInformation, //1
} MX_SECTION_INFORMATION_CLASS;

typedef enum {
  MxKeyBasicInformation = 0,
  MxKeyNodeInformation,           //1
  MxKeyFullInformation,           //2
  MxKeyNameInformation,           //3
  MxKeyCachedInformation,         //4
  MxKeyFlagsInformation,          //5
  MxKeyVirtualizationInformation, //6
  MxKeyHandleTagsInformation      //7
} MX_KEY_INFORMATION_CLASS;

typedef enum {
  MxKeyValueBasicInformation = 0,
  MxKeyValueFullInformation,          //1
  MxKeyValuePartialInformation,       //2
  MxKeyValueFullInformationAlign64,   //3
  MxKeyValuePartialInformationAlign64 //4
} MX_KEY_VALUE_INFORMATION_CLASS;

typedef enum {
  MxFileDirectoryInformation = 1,
  MxFileFullDirectoryInformation,
  MxFileBothDirectoryInformation,
  MxFileBasicInformation,
  MxFileStandardInformation,
  MxFileInternalInformation,
  MxFileEaInformation,
  MxFileAccessInformation,
  MxFileNameInformation,
  MxFileRenameInformation,
  MxFileLinkInformation,
  MxFileNamesInformation,
  MxFileDispositionInformation,
  MxFilePositionInformation,
  MxFileFullEaInformation,
  MxFileModeInformation,
  MxFileAlignmentInformation,
  MxFileAllInformation,
  MxFileAllocationInformation,
  MxFileEndOfFileInformation,
  MxFileAlternateNameInformation,
  MxFileStreamInformation,
  MxFilePipeInformation,
  MxFilePipeLocalInformation,
  MxFilePipeRemoteInformation,
  MxFileMailslotQueryInformation,
  MxFileMailslotSetInformation,
  MxFileCompressionInformation,
  MxFileObjectIdInformation,
  MxFileCompletionInformation,
  MxFileMoveClusterInformation,
  MxFileQuotaInformation,
  MxFileReparsePointInformation,
  MxFileNetworkOpenInformation,
  MxFileAttributeTagInformation,
  MxFileTrackingInformation,
  MxFileIdBothDirectoryInformation,
  MxFileIdFullDirectoryInformation,
  MxFileValidDataLengthInformation,
  MxFileShortNameInformation
} MX_FILE_INFORMATION_CLASS;

typedef enum {
  MxObjectNameInformation = 1
} MX_OBJECT_INFORMATION_CLASS;

typedef enum {
  MxNotificationEvent = 0,
  MxSynchronizationEvent
} MX_EVENT_TYPE;

//-----------------------------------------------------------

#pragma pack(8)
typedef struct {
  USHORT Length;
  USHORT MaximumLength;
  ULONG Buffer;
} MX_ANSI_STRING32, MX_UNICODE_STRING32;

typedef struct {
  USHORT Length;
  USHORT MaximumLength;
  ULONGLONG Buffer;
} MX_ANSI_STRING64, MX_UNICODE_STRING64;

typedef struct {
  USHORT Length;
  USHORT MaximumLength;
  PSTR   Buffer;
} MX_ANSI_STRING;

typedef struct {
  USHORT Length;
  USHORT MaximumLength;
  PWSTR  Buffer;
} MX_UNICODE_STRING;

typedef MX_ANSI_STRING32 *PMX_ANSI_STRING32;
typedef MX_ANSI_STRING64 *PMX_ANSI_STRING64;
typedef MX_ANSI_STRING *PMX_ANSI_STRING;
typedef const MX_ANSI_STRING32 *PCMX_ANSI_STRING32;
typedef const MX_ANSI_STRING64 *PCMX_ANSI_STRING64;
typedef const MX_ANSI_STRING *PCMX_ANSI_STRING;

typedef MX_UNICODE_STRING32 *PMX_UNICODE_STRING32;
typedef MX_UNICODE_STRING64 *PMX_UNICODE_STRING64;
typedef MX_UNICODE_STRING *PMX_UNICODE_STRING;
typedef const MX_UNICODE_STRING32 *PCMX_UNICODE_STRING32;
typedef const MX_UNICODE_STRING64 *PCMX_UNICODE_STRING64;
typedef const MX_UNICODE_STRING *PCMX_UNICODE_STRING;

typedef struct {
  ULONG Length;
  HANDLE RootDirectory;
  PMX_UNICODE_STRING ObjectName;
  ULONG Attributes;
  PVOID SecurityDescriptor;
  PVOID SecurityQualityOfService;
} MX_OBJECT_ATTRIBUTES;
typedef MX_OBJECT_ATTRIBUTES *PMX_OBJECT_ATTRIBUTES;

typedef struct {
  union {
    NTSTATUS Status;
    PVOID Pointer;
  } DUMMYUNIONNAME;
  ULONG_PTR Information;
} MX_IO_STATUS_BLOCK, *PMX_IO_STATUS_BLOCK;

typedef struct {
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  ULONG FileAttributes;
} MX_FILE_BASIC_INFORMATION;

typedef struct {
  LARGE_INTEGER AllocationSize;
  LARGE_INTEGER EndOfFile;
  ULONG NumberOfLinks;
  BOOLEAN DeletePending;
  BOOLEAN Directory;
} MX_FILE_STANDARD_INFORMATION;

typedef struct {
  LARGE_INTEGER CurrentByteOffset;
} MX_FILE_POSITION_INFORMATION;

typedef struct {
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
} MX_FILE_DIRECTORY_INFORMATION;

typedef struct {
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
} MX_FILE_FULL_DIR_INFORMATION;

typedef struct {
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
} MX_FILE_BOTH_DIR_INFORMATION;

typedef struct {
  ULONG NextEntryOffset;
  ULONG FileIndex;
  ULONG FileNameLength;
  WCHAR FileName[1];
} MX_FILE_NAMES_INFORMATION;

typedef struct {
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
} MX_FILE_ID_BOTH_DIR_INFORMATION;

typedef struct {
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
} MX_FILE_ID_FULL_DIR_INFORMATION;

//--------

typedef struct {
  LARGE_INTEGER LastWriteTime;
  ULONG TitleIndex;
  ULONG NameLength;
  WCHAR Name[1];
} MX_KEY_BASIC_INFORMATION;

typedef struct {
  LARGE_INTEGER LastWriteTime;
  ULONG TitleIndex;
  ULONG ClassOffset;
  ULONG ClassLength;
  ULONG NameLength;
  WCHAR Name[1];
} MX_KEY_NODE_INFORMATION;

typedef struct {
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
} MX_KEY_FULL_INFORMATION;

typedef struct {
  ULONG NameLength;
  WCHAR Name[1];
} MX_KEY_NAME_INFORMATION;

typedef struct {
  LARGE_INTEGER LastWriteTime;
  ULONG TitleIndex;
  ULONG SubKeys;
  ULONG MaxNameLen;
  ULONG Values;
  ULONG MaxValueNameLen;
  ULONG MaxValueDataLen;
  ULONG NameLength;
} MX_KEY_CACHED_INFORMATION;

typedef struct {
  ULONG VirtualizationCandidate : 1;
  ULONG VirtualizationEnabled   : 1;
  ULONG VirtualTarget           : 1;
  ULONG VirtualStore            : 1;
  ULONG VirtualSource           : 1;
  ULONG Reserved                : 27;
} MX_KEY_VIRTUALIZATION_INFORMATION;

typedef struct {
  ULONG TitleIndex;
  ULONG Type;
  ULONG NameLength;
  WCHAR Name[1];
} MX_KEY_VALUE_BASIC_INFORMATION;

typedef struct {
  ULONG TitleIndex;
  ULONG Type;
  ULONG DataLength;
  UCHAR Data[1];
} MX_KEY_VALUE_PARTIAL_INFORMATION;

typedef struct {
  ULONG TitleIndex;
  ULONG Type;
  ULONG DataOffset;
  ULONG DataLength;
  ULONG NameLength;
  WCHAR Name[1];
} MX_KEY_VALUE_FULL_INFORMATION;

typedef struct {
  PMX_UNICODE_STRING ValueName;
  ULONG DataLength;
  ULONG DataOffset;
  ULONG Type;
} MX_KEY_VALUE_ENTRY;

//--------

typedef struct {
  SIZE_T UniqueProcess;
  SIZE_T UniqueThread;
} MX_CLIENT_ID;

typedef struct {
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
} MX_SYSTEM_THREAD_INFORMATION, *LPMX_SYSTEM_THREAD_INFORMATION;

typedef struct {
  MX_SYSTEM_THREAD_INFORMATION ThreadInfo;
  PVOID StackBase;
  PVOID StackLimit;
  PVOID Win32StartAddress;
  PVOID TebAddress; //Vista or later
  ULONG Reserved1;
  ULONG Reserved2;
  ULONG Reserved3;
} MX_SYSTEM_EXTENDED_THREAD_INFORMATION, *LPMX_SYSTEM_EXTENDED_THREAD_INFORMATION;

typedef struct {
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
} MX_SYSTEM_PROCESS_INFORMATION, *LPMX_SYSTEM_PROCESS_INFORMATION;

typedef struct {
  USHORT ProcessorArchitecture;
  USHORT ProcessorLevel;
  USHORT ProcessorRevision;
  USHORT Reserved;
  ULONG ProcessorFeatureBits;
} MX_SYSTEM_PROCESSOR_INFORMATION;

typedef struct {
  ULONG SessionId;
  ULONG SizeOfBuf;
  PVOID Buffer;
} MX_SYSTEM_SESSION_PROCESS_INFORMATION, *PMX_SYSTEM_SESSION_PROCESS_INFORMATION;

typedef struct {
  ULONG SessionId;
} MX_PROCESS_SESSION_INFORMATION, *LPMX_PROCESS_SESSION_INFORMATION;

typedef struct {
  NTSTATUS ExitStatus;
  PVOID TebBaseAddress;
  MX_CLIENT_ID ClientId;
  ULONG_PTR AffinityMask;
  MX_KPRIORITY Priority;
  LONG BasePriority;
} MX_THREAD_BASIC_INFORMATION, *LPMX_THREAD_BASIC_INFORMATION;

typedef struct {
  ULONG Flink;
  ULONG Blink;
} MX_LIST_ENTRY32;

typedef struct {
  ULONGLONG Flink;
  ULONGLONG Blink;
} MX_LIST_ENTRY64;

typedef struct tagMX_LIST_ENTRY {
  struct tagMX_LIST_ENTRY *Flink;
  struct tagMX_LIST_ENTRY *Blink;
} MX_LIST_ENTRY;

typedef struct {
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
} MX_LDR_DATA_TABLE_ENTRY32;

typedef struct {
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
} MX_LDR_DATA_TABLE_ENTRY64;

typedef struct {
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
} MX_LDR_DATA_TABLE_ENTRY;

typedef struct {
  ULONG Length;
  BOOLEAN Initialized;
  HANDLE SsHandle;
  MX_LIST_ENTRY InLoadOrderModuleList;
  MX_LIST_ENTRY InMemoryOrderModuleList;
  MX_LIST_ENTRY InInitializationOrderModuleList;
  PVOID EntryInProgress;
  BOOLEAN ShutdownInProgress;
  HANDLE ShutdownThreadId;
} MX_PEB_LDR_DATA;

#pragma pack(4)
typedef struct {
  union {
    struct {
      HANDLE DirectoryHandle;
    } Set;
    struct {
      ULONG DriveMap;
      UCHAR DriveType[32];
    } Query;
  };
} MX_PROCESS_DEVICEMAP_INFORMATION;
#pragma pack()

typedef struct {
  MX_UNICODE_STRING Name;
  WCHAR Buffer[1];
} MX_OBJECT_NAME_INFORMATION;

typedef struct {
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
} MX_KNONVOLATILE_CONTEXT_POINTERS;
#pragma pack()

//-----------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

#define __DECLARE(ret, apiName) ret NTAPI Mx##apiName
__DECLARE(NTSTATUS, NtOpenProcess)(__out PHANDLE ProcessHandle, __in ACCESS_MASK DesiredAccess,
                                   __in PMX_OBJECT_ATTRIBUTES ObjectAttributes, __in_opt PVOID ClientId);

__DECLARE(NTSTATUS, NtOpenThread)(__out PHANDLE ThreadHandle, __in ACCESS_MASK DesiredAccess,
                                  __in PMX_OBJECT_ATTRIBUTES ObjectAttributes, __in_opt PVOID ClientId);

__DECLARE(NTSTATUS, NtOpenProcessToken)(__in HANDLE ProcessHandle, __in ACCESS_MASK DesiredAccess,
                                        __out PHANDLE TokenHandle);
__DECLARE(NTSTATUS, NtOpenThreadToken)(__in HANDLE ThreadHandle, __in ACCESS_MASK DesiredAccess,
                                       __in BOOLEAN OpenAsSelf, __out PHANDLE TokenHandle);

__DECLARE(NTSTATUS, NtDuplicateToken)(__in HANDLE ExistingTokenHandle, __in ACCESS_MASK DesiredAccess,
                                      __in PMX_OBJECT_ATTRIBUTES ObjectAttributes, __in BOOLEAN EffectiveOnly,
                                      __in TOKEN_TYPE TokenType, __out PHANDLE NewTokenHandle);

__DECLARE(NTSTATUS, NtQueryInformationToken)(__in HANDLE TokenHandle, __in TOKEN_INFORMATION_CLASS TokenInfoClass,
                                             __out PVOID TokenInfo, __in ULONG TokenInfoLength,
                                             __out PULONG ReturnLength);
__DECLARE(NTSTATUS, NtSetInformationToken)(__in HANDLE TokenHandle, __in TOKEN_INFORMATION_CLASS TokenInfoClass,
                                           __in PVOID TokenInfo, __in ULONG TokenInfoLength);

//--------

__DECLARE(NTSTATUS, NtGetContextThread)(__in HANDLE ThreadHandle, __out PCONTEXT Context);
__DECLARE(NTSTATUS, NtSetContextThread)(__in HANDLE ThreadHandle, __in PCONTEXT Context);

__DECLARE(NTSTATUS, RtlCreateUserThread)(__in HANDLE ProcessHandle, __in_opt PSECURITY_DESCRIPTOR SecurityDescriptor,
                                         __in BOOLEAN CreateSuspended, __in ULONG StackZeroBits,
                                         __inout PULONG StackReserved, __inout PULONG StackCommit,
                                         __in PVOID StartAddress, __in_opt PVOID StartParameter,
                                         __out PHANDLE ThreadHandle, __out PVOID ClientID);

//--------

__DECLARE(NTSTATUS, NtCreateEvent)(__out PHANDLE EventHandle, __in ACCESS_MASK DesiredAccess,
                                   __in_opt PMX_OBJECT_ATTRIBUTES ObjectAttributes, __in MX_EVENT_TYPE EventType,
                                   __in BOOLEAN InitialState);
__DECLARE(NTSTATUS, NtOpenEvent)(__out PHANDLE EventHandle, __in ACCESS_MASK DesiredAccess,
                                 __in PMX_OBJECT_ATTRIBUTES ObjectAttributes);
__DECLARE(NTSTATUS, NtResetEvent)(__in HANDLE EventHandle, __out_opt PLONG NumberOfWaitingThreads);
__DECLARE(NTSTATUS, NtSetEvent)(__in HANDLE EventHandle, __out_opt PLONG NumberOfWaitingThreads);

//--------

__DECLARE(NTSTATUS, NtCreateMutant)(__out PHANDLE MutantHandle, __in ACCESS_MASK DesiredAccess,
                                    __in PMX_OBJECT_ATTRIBUTES ObjectAttributes, __in BOOLEAN InitialOwner);
__DECLARE(NTSTATUS, NtOpenMutant)(__out PHANDLE MutantHandle, __in ACCESS_MASK DesiredAccess,
                                  __in PMX_OBJECT_ATTRIBUTES ObjectAttributes);
__DECLARE(NTSTATUS, NtReleaseMutant)(__in HANDLE MutantHandle, __out_opt PLONG PreviousCount);

//--------

__DECLARE(NTSTATUS, NtCreateFile)(__out PHANDLE FileHandle, __in ACCESS_MASK DesiredAccess,
                                  __in PMX_OBJECT_ATTRIBUTES ObjectAttributes, __out PMX_IO_STATUS_BLOCK IoStatusBlock,
                                  __in_opt PLARGE_INTEGER AllocationSize, __in ULONG FileAttributes,
                                  __in ULONG ShareAccess, __in ULONG CreateDisposition, __in ULONG CreateOptions,
                                  __in PVOID EaBuffer, __in ULONG EaLength);
__DECLARE(NTSTATUS, NtOpenFile)(__out PHANDLE FileHandle, __in ACCESS_MASK DesiredAccess,
                                __in PMX_OBJECT_ATTRIBUTES ObjectAttributes, __out PMX_IO_STATUS_BLOCK IoStatusBlock,
                                __in ULONG ShareAccess, __in ULONG OpenOptions);
__DECLARE(NTSTATUS, NtOpenDirectoryObject)(__out PHANDLE FileHandle, __in ACCESS_MASK DesiredAccess,
                                          __in PMX_OBJECT_ATTRIBUTES ObjectAttributes);
__DECLARE(NTSTATUS, NtOpenSymbolicLinkObject)(__out PHANDLE SymbolicLinkHandle, __in ACCESS_MASK DesiredAccess,
                                             __in PMX_OBJECT_ATTRIBUTES ObjectAttributes);
__DECLARE(NTSTATUS, NtQuerySymbolicLinkObject)(__in HANDLE SymLinkObjHandle, __out PMX_UNICODE_STRING LinkTarget,
                                               __out_opt PULONG DataWritten);
__DECLARE(NTSTATUS, NtClose)(__in HANDLE Handle);
__DECLARE(NTSTATUS, NtReadFile)(__in HANDLE FileHandle, __in_opt HANDLE Event, __in_opt PVOID ApcRoutine,
                                __in_opt PVOID ApcContext, __out PMX_IO_STATUS_BLOCK IoStatusBlock, __out PVOID Buffer,
                                __in ULONG Length, __in_opt PLARGE_INTEGER ByteOffset, __in_opt PULONG Key);
__DECLARE(NTSTATUS, NtWriteFile)(__in HANDLE FileHandle, __in_opt HANDLE Event, __in_opt PVOID ApcRoutine,
                                 __in_opt PVOID ApcContext, __out PMX_IO_STATUS_BLOCK IoStatusBlock, __out PVOID Buffer,
                                 __in ULONG Length, __in_opt PLARGE_INTEGER ByteOffset, __in_opt PULONG Key);
__DECLARE(NTSTATUS, NtCancelIoFile)(__in HANDLE FileHandle, __out PMX_IO_STATUS_BLOCK IoStatusBlock);
__DECLARE(NTSTATUS, NtQueryInformationFile)(__in HANDLE hFile, __out PMX_IO_STATUS_BLOCK IoStatusBlock,
                                            __out PVOID FileInformationBuffer, __in ULONG FileInformationBufferLength,
                                            __in MX_FILE_INFORMATION_CLASS FileInfoClass);
__DECLARE(NTSTATUS, NtSetInformationFile)(__in HANDLE hFile, __out PMX_IO_STATUS_BLOCK IoStatusBlock,
                                          __in PVOID FileInformationBuffer, __in ULONG FileInformationBufferLength,
                                          __in MX_FILE_INFORMATION_CLASS FileInfoClass);

//--------

__DECLARE(NTSTATUS, NtCreateKey)(__out PHANDLE KeyHandle, __in ACCESS_MASK DesiredAccess,
                                 __in PMX_OBJECT_ATTRIBUTES ObjectAttributes, __in ULONG TitleIndex,
                                 __in_opt PMX_UNICODE_STRING Class, __in ULONG CreateOptions,
                                 __out_opt PULONG Disposition);
__DECLARE(NTSTATUS, NtOpenKey)(__out PHANDLE KeyHandle, __in ACCESS_MASK DesiredAccess,
                               __in PMX_OBJECT_ATTRIBUTES ObjectAttributes);
__DECLARE(NTSTATUS, NtEnumerateKey)(__in HANDLE KeyHandle, __in ULONG Index, __in MX_KEY_INFORMATION_CLASS KeyInfoClass,
                                    __out PVOID KeyInformation, __in ULONG Length, __out PULONG ResultLength);
__DECLARE(NTSTATUS, NtEnumerateValueKey)(__in HANDLE KeyHandle, __in ULONG Index,
                                         __in MX_KEY_VALUE_INFORMATION_CLASS KeyValueInfoClass,
                                         __out PVOID KeyValueInformation, __in ULONG Length, __out PULONG ResultLength);
__DECLARE(NTSTATUS, NtQueryKey)(__in HANDLE KeyHandle, __in MX_KEY_INFORMATION_CLASS KeyInfoClass,
                                __out PVOID KeyInformation, __in ULONG Length, __out PULONG ResultLength);
__DECLARE(NTSTATUS, NtQueryValueKey)(__in HANDLE KeyHandle, __in PMX_UNICODE_STRING ValueName,
                                     __in MX_KEY_VALUE_INFORMATION_CLASS KeyValueInfoClass,
                                     __out PVOID KeyValueInformation, __in ULONG Length, __out PULONG ResultLength);
__DECLARE(NTSTATUS, NtSetValueKey)(__in HANDLE KeyHandle, __in PMX_UNICODE_STRING ValueName, __in ULONG TitleIndex,
                                   __in ULONG Type, __in PVOID Data, __in ULONG DataSize);
__DECLARE(NTSTATUS, NtDeleteKey)(__in HANDLE KeyHandle);
__DECLARE(NTSTATUS, NtDeleteValueKey)(__in HANDLE KeyHandle, __in PMX_UNICODE_STRING ValueName);
__DECLARE(NTSTATUS, NtFlushKey)(__in HANDLE KeyHandle);

//--------

__DECLARE(NTSTATUS, NtDelayExecution)(__in BOOLEAN Alertable, __in PLARGE_INTEGER DelayInterval);
__DECLARE(NTSTATUS, NtYieldExecution)();
__DECLARE(NTSTATUS, NtQuerySystemTime)(__out PULARGE_INTEGER SystemTime); //defined as unsigned on purpose
__DECLARE(NTSTATUS, NtQueryPerformanceCounter)(__out PLARGE_INTEGER Counter, __out_opt PLARGE_INTEGER Frequency);

//--------

__DECLARE(NTSTATUS, RtlInitializeCriticalSection)(__in RTL_CRITICAL_SECTION* crit);
__DECLARE(NTSTATUS, RtlInitializeCriticalSectionAndSpinCount)(__in RTL_CRITICAL_SECTION* crit, __in ULONG spincount);
__DECLARE(NTSTATUS, RtlDeleteCriticalSection)(__in RTL_CRITICAL_SECTION* crit);
__DECLARE(NTSTATUS, RtlEnterCriticalSection)(__in RTL_CRITICAL_SECTION* crit);
__DECLARE(NTSTATUS, RtlLeaveCriticalSection)(__in RTL_CRITICAL_SECTION* crit);
__DECLARE(BOOLEAN, RtlTryEnterCriticalSection)(__in RTL_CRITICAL_SECTION* crit);

//--------

__DECLARE(NTSTATUS, RtlGetNativeSystemInformation)(__in MX_SYSTEM_INFORMATION_CLASS SystemInformationClass,
                                                   __inout PVOID SystemInformation, __in ULONG SystemInformationLength,
                                                   __out_opt PULONG ReturnLength);
__DECLARE(NTSTATUS, NtQuerySystemInformation)(__in MX_SYSTEM_INFORMATION_CLASS SystemInformationClass,
                                              __inout PVOID SystemInformation, __in ULONG SystemInformationLength,
                                              __out_opt PULONG ReturnLength);

__DECLARE(NTSTATUS, NtQueryInformationProcess)(__in HANDLE ProcessHandle,
                                               __in MX_PROCESS_INFORMATION_CLASS ProcessInfoClass,
                                               __out_opt PVOID ProcessInfo, __in ULONG ProcessInfoLength,
                                               __out_opt PULONG ReturnLength);
__DECLARE(NTSTATUS, NtSetInformationProcess)(__in HANDLE ProcessHandle,
                                             __in MX_PROCESS_INFORMATION_CLASS ProcessInfoClass,
                                             __in PVOID ProcessInformation, __in ULONG ProcessInformationLength);
__DECLARE(NTSTATUS, NtQueryInformationThread)(__in HANDLE ThreadHandle,
                                              __in MX_THREAD_INFORMATION_CLASS ThreadInfoClass,
                                              __out_opt PVOID ThreadInfo, __in ULONG ThreadInfoLength,
                                              __out_opt PULONG ReturnLength);
__DECLARE(NTSTATUS, NtSetInformationThread)(__in HANDLE ThreadHandle,
                                            __in MX_THREAD_INFORMATION_CLASS ThreadInformationClass,
                                            __in PVOID ThreadInformation, __in ULONG ThreadInformationLength);

__DECLARE(NTSTATUS, NtQueryObject)(__in_opt HANDLE Handle, __in MX_OBJECT_INFORMATION_CLASS ObjectInformationClass,
                                   __out_opt PVOID ObjectInformation, __in ULONG ObjectInformationLength,
                                   __out_opt PULONG ReturnLength);

//--------

__DECLARE(NTSTATUS, NtWaitForSingleObject)(__in HANDLE Handle, __in BOOLEAN Alertable, __in_opt PLARGE_INTEGER Timeout);
__DECLARE(NTSTATUS, NtWaitForMultipleObjects)(__in ULONG Count, __in HANDLE Object[], __in LONG WaitType,
                                              __in BOOLEAN Alertable, __in PLARGE_INTEGER Time);

//--------

__DECLARE(NTSTATUS, NtAllocateVirtualMemory)(__in HANDLE ProcessHandle, __inout PVOID *BaseAddress,
                                             __in ULONG_PTR ZeroBits, __inout PSIZE_T RegionSize,
                                             __in ULONG AllocationType, __in ULONG Protect);
__DECLARE(NTSTATUS, NtFreeVirtualMemory)(__in HANDLE ProcessHandle, __inout PVOID *BaseAddress,
                                         __inout PSIZE_T RegionSize, __in ULONG FreeType);
__DECLARE(NTSTATUS, NtFlushVirtualMemory)(__in HANDLE ProcessHandle, __inout PVOID *BaseAddress,
                                          __inout PSIZE_T RegionSize, __out PMX_IO_STATUS_BLOCK IoStatus);
__DECLARE(NTSTATUS, NtReadVirtualMemory)(__in HANDLE ProcessHandle, __in PVOID BaseAddress, __out PVOID Buffer,
                                         __in SIZE_T NumberOfBytesToRead, __out_opt PSIZE_T NumberOfBytesReaded);
__DECLARE(NTSTATUS, NtWriteVirtualMemory)(__in HANDLE ProcessHandle, __in PVOID BaseAddress, __out PVOID Buffer,
                                          __in SIZE_T NumberOfBytesToWrite, __out_opt PSIZE_T NumberOfBytesWritten);
__DECLARE(NTSTATUS, NtQueryVirtualMemory)(__in HANDLE ProcessHandle, __in PVOID Address,
                                          __in ULONG VirtualMemoryInformationClass,
                                          __out PVOID VirtualMemoryInformation, __in SIZE_T Length,
                                          __out_opt PSIZE_T ResultLength);
__DECLARE(NTSTATUS, NtProtectVirtualMemory)(__in HANDLE ProcessHandle, __inout PVOID *UnsafeBaseAddress,
                                            __inout SIZE_T *UnsafeNumberOfBytesToProtect,
                                            __in ULONG NewAccessProtection, __out PULONG UnsafeOldAccessProtection);

//--------

__DECLARE(NTSTATUS, NtCreateSection)(__out PHANDLE SectionHandle, __in ACCESS_MASK DesiredAccess,
                                     __in_opt PMX_OBJECT_ATTRIBUTES ObjectAttributes,
                                     __in_opt PLARGE_INTEGER MaximumSize, __in ULONG SectionPageProtection,
                                     __in ULONG AllocationAttributes, __in_opt HANDLE FileHandle);
__DECLARE(NTSTATUS, NtMapViewOfSection)(__in HANDLE SectionHandle, __in HANDLE ProcessHandle,
                                        __inout PVOID *BaseAddress, __in ULONG_PTR ZeroBits, __in SIZE_T CommitSize,
                                        __inout PLARGE_INTEGER SectionOffset, __inout PSIZE_T ViewSize,
                                        __in ULONG InheritDisposition, __in ULONG AllocationType,
                                        __in ULONG Win32Protect);
__DECLARE(NTSTATUS, NtUnmapViewOfSection)(__in HANDLE ProcessHandle, __in PVOID BaseAddress);

//--------

__DECLARE(NTSTATUS, NtSuspendThread)(__in HANDLE ThreadHandle, __out_opt PULONG PreviousSuspendCount);
__DECLARE(NTSTATUS, NtResumeThread)(__in HANDLE ThreadHandle, __out_opt PULONG SuspendCount);

//--------

__DECLARE(NTSTATUS, RtlImpersonateSelf)(__in SECURITY_IMPERSONATION_LEVEL ImpersonationLevel);

__DECLARE(NTSTATUS, NtAdjustPrivilegesToken)(__in HANDLE TokenHandle, __in BOOLEAN DisableAllPrivileges,
                                             __in_opt PTOKEN_PRIVILEGES NewState, __in ULONG BufferLength,
                                             __out PTOKEN_PRIVILEGES PreviousState, __out_opt PULONG ReturnLength);

__DECLARE(VOID, RtlRaiseException)(__in PEXCEPTION_RECORD ExceptionRecord);

__DECLARE(ULONG, RtlNtStatusToDosError)(__in NTSTATUS Status);

__DECLARE(NTSTATUS, NtFlushInstructionCache)(__in HANDLE ProcessHandle, __in PVOID BaseAddress,
                                             __in ULONG NumberOfBytesToFlush);

//--------

__DECLARE(PVOID, RtlAllocateHeap)(__in PVOID HeapHandle, __in_opt ULONG Flags, __in SIZE_T Size);
__DECLARE(PVOID, RtlReAllocateHeap)(__in PVOID HeapHandle, __in_opt ULONG Flags, __in PVOID Ptr, __in SIZE_T Size);
__DECLARE(BOOLEAN, RtlFreeHeap)(__in PVOID HeapHandle, __in_opt ULONG Flags, __in PVOID HeapBase);
__DECLARE(PVOID, RtlCreateHeap)(__in ULONG Flags, __in_opt PVOID HeapBase, __in_opt SIZE_T ReserveSize,
                                __in_opt SIZE_T CommitSize, __in_opt PVOID Lock, __in_opt PVOID Parameters);
__DECLARE(PVOID, RtlDestroyHeap)(__in PVOID HeapHandle);
__DECLARE(SIZE_T, RtlSizeHeap)(__in PVOID HeapHandle, __in_opt ULONG Flags, __in PVOID Ptr);

//--------

__DECLARE(VOID, RtlUnwind)(__in_opt PVOID TargetFrame, __in_opt PVOID TargetIp,
                           __in_opt PEXCEPTION_RECORD ExceptionRecord, __in PVOID ReturnValue);
#if defined(_M_X64) || defined(_M_AMD64)
__DECLARE(VOID, RtlUnwindEx)(__in ULONGLONG TargetFrame, __in ULONGLONG TargetIp,
                             __in_opt PEXCEPTION_RECORD ExceptionRecord, __in PVOID ReturnValue,
                             __in PCONTEXT OriginalContext, __in_opt PVOID HistoryTable);
#endif //_M_X64 || _M_AMD64

//--------

__DECLARE(NTSTATUS, NtTerminateProcess)(__in_opt HANDLE ProcessHandle, __in NTSTATUS ExitStatus);
__DECLARE(NTSTATUS, NtTerminateThread)(__in HANDLE ThreadHandle, __in NTSTATUS ExitStatus);

__DECLARE(NTSTATUS, NtDuplicateObject)(__in HANDLE SourceProcessHandle, __in HANDLE SourceHandle,
                                       __in_opt HANDLE TargetProcessHandle, __out_opt PHANDLE TargetHandle,
                                       __in ACCESS_MASK DesiredAccess, __in ULONG HandleAttr, __in ULONG Options);

//--------

__DECLARE(NTSTATUS, RtlGetVersion)(__inout PRTL_OSVERSIONINFOW lpVersionInformation);

//--------

__DECLARE(NTSTATUS, LdrLoadDll)(__in_opt PWSTR SearchPath, __in_opt PULONG DllCharacteristics,
                                __in PMX_UNICODE_STRING DllName, __out PVOID *BaseAddress);
__DECLARE(NTSTATUS, LdrUnloadDll)(__in PVOID BaseAddress);

__DECLARE(NTSTATUS, LdrFindEntryForAddress)(__in PVOID Address, __out PVOID *lplpEntry);

__DECLARE(NTSTATUS, LdrGetProcedureAddress)(__in PVOID BaseAddress, __in PMX_ANSI_STRING Name, __in ULONG Ordinal,
                                            __out PVOID *ProcedureAddress);

#if defined(_M_X64)
__DECLARE(VOID, RtlCaptureContext)(__out PCONTEXT ContextRecord);

__DECLARE(PVOID, RtlLookupFunctionEntry)(__in ULONG64 ControlPc, __out PULONG64 ImageBase,
                                         __inout_opt PUNWIND_HISTORY_TABLE HistoryTable);

__DECLARE(PVOID, RtlVirtualUnwind)(__in DWORD HandlerType, __in DWORD64 ImageBase, __in DWORD64 ControlPc,
                                   __in PRUNTIME_FUNCTION FunctionEntry, __inout PCONTEXT ContextRecord,
                                   __out PVOID *HandlerData, __out PDWORD64 EstablisherFrame,
                                   __inout_opt MX_KNONVOLATILE_CONTEXT_POINTERS *ContextPointers);

__DECLARE(BOOLEAN, RtlAddFunctionTable)(__in PVOID FunctionTable, __in DWORD EntryCount, __in DWORD64 BaseAddress);
__DECLARE(BOOLEAN, RtlDeleteFunctionTable)(__in PVOID FunctionTable);
__DECLARE(BOOLEAN, RtlInstallFunctionTableCallback)(__in DWORD64 TableIdentifier, __in DWORD64 BaseAddress,
                                                    __in DWORD Length, __in PGET_RUNTIME_FUNCTION_CALLBACK Callback,
                                                    __in PVOID Context, __in PCWSTR OutOfProcessCallbackDll);
#endif //_M_X64

//--------

__DECLARE(NTSTATUS, RtlAnsiStringToUnicodeString)(__inout PMX_UNICODE_STRING DestinationString,
                                                  __in PCMX_ANSI_STRING SourceString, __in BOOLEAN AllocDestString);
__DECLARE(ULONG, RtlAnsiStringToUnicodeSize)(__in PMX_ANSI_STRING AnsiString);
__DECLARE(VOID, RtlFreeUnicodeString)(__inout PMX_UNICODE_STRING UnicodeString);

__DECLARE(NTSTATUS, RtlUnicodeStringToAnsiString)(__inout PMX_ANSI_STRING DestinationString,
                                                  __in PCMX_UNICODE_STRING SourceString, __in BOOLEAN AllocDestString);
__DECLARE(ULONG, RtlUnicodeStringToAnsiSize)(__in PMX_UNICODE_STRING UnicodeString);
__DECLARE(VOID, RtlFreeAnsiString)(__inout PMX_ANSI_STRING AnsiString);

__DECLARE(LONG, RtlCompareUnicodeString)(__in PCMX_UNICODE_STRING String1, __in PCMX_UNICODE_STRING String2,
                                         __in BOOLEAN CaseInsensitive);
__DECLARE(LONG, RtlCompareString)(__in PCMX_ANSI_STRING String1, __in PCMX_ANSI_STRING String2,
                                  __in BOOLEAN CaseInsensitive);

__DECLARE(WCHAR, RtlUpcaseUnicodeChar)(__in WCHAR SourceCharacter);
__DECLARE(WCHAR, RtlDowncaseUnicodeChar)(__in WCHAR SourceCharacter);
__DECLARE(CHAR, RtlUpperChar)(__in CHAR Character);

//--------

__DECLARE(PVOID, RtlAddVectoredExceptionHandler)(__in ULONG FirstHandler,
                                                 __in PVECTORED_EXCEPTION_HANDLER VectoredHandler);
__DECLARE(ULONG, RtlRemoveVectoredExceptionHandler)(__in PVOID VectoredHandlerHandle);

//--------

__DECLARE(ULONG, RtlFindClearBitsAndSet)(__in PVOID BitMapHeader, __in ULONG NumberToFind, __in ULONG HintIndex);
__DECLARE(BOOLEAN, RtlAreBitsSet)(__in PVOID BitMapHeader, __in ULONG StartingIndex, __in ULONG Length);
__DECLARE(VOID, RtlClearBits)(__in PVOID BitMapHeader, __in ULONG StartingIndex, __in ULONG NumberToClear);

__DECLARE(VOID, RtlAcquirePebLock)();
__DECLARE(VOID, RtlReleasePebLock)();
#undef __DECLARE

PVOID MxGetProcessHeap();

LPBYTE MxGetPeb();
LPBYTE MxGetRemotePeb(__in HANDLE hProcess);
LPBYTE MxGetTeb();

VOID MxSetLastWin32Error(__in DWORD dwOsErr);
DWORD MxGetLastWin32Error();

PRTL_CRITICAL_SECTION MxGetLoaderLockCS();

PVOID MxGetDllHandle(__in_z PCWSTR szModuleNameW);
PVOID __stdcall MxGetProcedureAddress(__in PVOID DllBase, __in_z PCSTR szApiNameA);

LONG MxGetProcessorArchitecture();

HANDLE MxOpenProcess(__in DWORD dwDesiredAccess, __in BOOL bInheritHandle, __in DWORD dwProcessId);
HANDLE MxOpenThread(__in DWORD dwDesiredAccess, __in BOOL bInheritHandle, __in DWORD dwThreadId);

NTSTATUS MxCreateFile(__out HANDLE *lphFile, __in LPCWSTR szFileNameW, __in_opt DWORD dwDesiredAccess=GENERIC_READ,
                      __in_opt DWORD dwShareMode=FILE_SHARE_READ, __in_opt DWORD dwCreationDisposition=OPEN_EXISTING,
                      __in_opt DWORD dwFlagsAndAttributes=FILE_ATTRIBUTE_NORMAL,
                      __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes=NULL);

//1 or 0 on success, STATUS_NOT_SUPPORTED if o.s. bitness is < 64 or STATUS_### on error
NTSTATUS MxIsWow64(__in HANDLE hProcess);

SIZE_T MxReadMem(__in HANDLE hProcess, __in LPVOID lpDest, __in LPVOID lpSrc, __in SIZE_T nBytesCount);
BOOL MxWriteMem(__in HANDLE hProcess, __in LPVOID lpDest, __in LPVOID lpSrc, __in SIZE_T nBytesCount);

NTSTATUS MxGetThreadPriority(__in HANDLE hThread, __out int *lpnPriority);
NTSTATUS MxSetThreadPriority(__in HANDLE hThread, __in int _nPriority);

DWORD MxGetCurrentThreadId();
DWORD MxGetCurrentProcessId();

int mx_sprintf_s(__out_z char *lpDest, __in size_t nMaxCount, __in_z const char *szFormatA, ...);
int mx_vsnprintf(__out_z char *lpDest, __in size_t nMaxCount, __in_z const char *szFormatA, __in va_list lpArgList);
int mx_swprintf_s(__out_z wchar_t *lpDest, __in size_t nMaxCount, __in_z const wchar_t *szFormatW, ...);
int mx_vsnwprintf(__out_z wchar_t *lpDest, __in size_t nMaxCount, __in_z const wchar_t *szFormatW,
                  __in va_list lpArgList);

#if defined(_M_IX86)
SIZE_T __stdcall MxCallWithSEH0(__in LPVOID lpFunc, __out BOOL *lpExceptionRaised);
SIZE_T __stdcall MxCallStdCallWithSEH1(__in LPVOID lpFunc, __out BOOL *lpExceptionRaised, __in SIZE_T nParam1);
SIZE_T __stdcall MxCallStdCallWithSEH2(__in LPVOID lpFunc, __out BOOL *lpExceptionRaised, __in SIZE_T nParam1,
                                       __in SIZE_T nParam2);
SIZE_T __stdcall MxCallStdCallWithSEH3(__in LPVOID lpFunc, __out BOOL *lpExceptionRaised, __in SIZE_T nParam1,
                                       __in SIZE_T nParam2, __in SIZE_T nParam3);
SIZE_T __stdcall MxCallCDeclWithSEH1(__in LPVOID lpFunc, __out BOOL *lpExceptionRaised, __in SIZE_T nParam1);
SIZE_T __stdcall MxCallCDeclWithSEH2(__in LPVOID lpFunc, __out BOOL *lpExceptionRaised, __in SIZE_T nParam1,
                                     __in SIZE_T nParam2);
SIZE_T __stdcall MxCallCDeclWithSEH3(__in LPVOID lpFunc, __out BOOL *lpExceptionRaised, __in SIZE_T nParam1,
                                     __in SIZE_T nParam2, __in SIZE_T nParam3);
#elif defined(_M_X64)
SIZE_T __stdcall MxCallWithSEH(__in LPVOID lpFunc, __out BOOL *lpExceptionRaised, __in_opt SIZE_T nParam1=0,
                               __in_opt SIZE_T nParam2=0, __in_opt SIZE_T nParam3=0);
#endif

VOID MxSleep(__in DWORD dwTimeMs);

#ifdef __cplusplus
}; //extern "C"
#endif

//-----------------------------------------------------------

#endif //_MX_NTDEFS_H
