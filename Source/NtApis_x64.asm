;
; Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
;
; Copyright (C) 2006-2015. All rights reserved.
;
; This software is provided 'as-is', without any express or implied warranty.
; In no event will the authors be held liable for any damages arising from
; the use of this software.
;
; 1. The origin of this software must not be misrepresented; you must not
;    claim that you wrote the original software.
;
; 2. This notice may not be removed or altered from any source distribution.
;
; 3. YOU MAY NOT:
;
;    a. Modify, translate, adapt, alter, or create derivative works from
;       this software.
;    b. Copy (other than one back-up copy), distribute, publicly display,
;       transmit, sell, rent, lease or otherwise exploit this software.
;    c. Distribute, sub-license, rent, lease, loan [or grant any third party
;       access to or use of the software to any third party.
;

;---------------------------------------------------------------------------------

SIZEOF_ARGUMENTS_HOME          EQU    20h

STATUS_NOT_IMPLEMENTED         EQU    0C0000002h
VOID                           EQU    0
FALSE                          EQU    0
NULL                           EQU    0
ERROR_NOT_SUPPORTED            EQU    50

IMAGE_DOS_SIGNATURE            EQU    5A4Dh     ;MZ
IMAGE_NT_SIGNATURE             EQU    00004550h ;PE00
IMAGE_NT_OPTIONAL_HDR64_MAGIC  EQU    20Bh
IMAGE_FILE_MACHINE_AMD64       EQU    8664h

;---------------------------------------------------------------------------------

UNICODE_STRING64 STRUCT 8
    _Length       WORD  ?
    MaximumLength WORD  ?
    Buffer        QWORD ?
UNICODE_STRING64 ENDS

LIST_ENTRY64 STRUCT
    Flink QWORD ?
    Blink QWORD ?
LIST_ENTRY64 ENDS

MODULE_ENTRY64 STRUCT
    InLoadOrderLinks           LIST_ENTRY64 <>
    InMemoryOrderLinks         LIST_ENTRY64 <>
    InInitializationOrderLinks LIST_ENTRY64 <>
    DllBase                    QWORD ?
    EntryPoint                 QWORD ?
    SizeOfImage                QWORD ?
    FullDllName                UNICODE_STRING64 <>
    BaseDllName                UNICODE_STRING64 <>
    Flags                      DWORD ?
    LoadCount                  WORD  ?
    ;structure continues but it is not needed
MODULE_ENTRY64 ENDS

IMAGE_DATA_DIRECTORY STRUCT
    VirtualAddress DWORD ?
    _Size          DWORD ?
IMAGE_DATA_DIRECTORY ENDS

IMAGE_DOS_HEADER STRUCT
    e_magic    WORD  ?
    e_cblp     WORD  ?
    e_cp       WORD  ?
    e_crlc     WORD  ?
    e_cparhdr  WORD  ?
    e_minalloc WORD  ?
    e_maxalloc WORD  ?
    e_ss       WORD  ?
    e_sp       WORD  ?
    e_csum     WORD  ?
    e_ip       WORD  ?
    e_cs       WORD  ?
    e_lfarlc   WORD  ?
    e_ovno     WORD  ?
    e_res      DW 4 DUP (<?>)
    e_oemid    WORD  ?
    e_oeminfo  WORD  ?
    e_res2     DW 10 DUP (<?>)
    e_lfanew   DWORD ?
IMAGE_DOS_HEADER ENDS

IMAGE_FILE_HEADER STRUCT
    Machine              WORD  ?
    NumberOfSections     WORD  ?
    TimeDateStamp        DWORD ?
    PointerToSymbolTable DWORD ?
    NumberOfSymbols      DWORD ?
    SizeOfOptionalHeader WORD  ?
    Characteristics      WORD  ?
IMAGE_FILE_HEADER ENDS

IMAGE_OPTIONAL_HEADER64 STRUCT 
    Magic                       WORD  ?
    MajorLinkerVersion          BYTE  ?
    MinorLinkerVersion          BYTE  ?
    SizeOfCode                  DWORD ?
    SizeOfInitializedData       DWORD ?
    SizeOfUninitializedData     DWORD ?
    AddressOfEntryPoint         DWORD ?
    BaseOfCode                  DWORD ?
    ImageBase                   QWORD ?
    SectionAlignment            DWORD ?
    FileAlignment               DWORD ?
    MajorOperatingSystemVersion WORD  ?
    MinorOperatingSystemVersion WORD  ?
    MajorImageVersion           WORD  ?
    MinorImageVersion           WORD  ?
    MajorSubsystemVersion       WORD  ?
    MinorSubsystemVersion       WORD  ?
    Win32VersionValue           DWORD ?
    SizeOfImage                 DWORD ?
    SizeOfHeaders               DWORD ?
    CheckSum                    DWORD ?
    Subsystem                   WORD  ?
    DllCharacteristics          WORD  ?
    SizeOfStackReserve          QWORD ?
    SizeOfStackCommit           QWORD ?
    SizeOfHeapReserve           QWORD ?
    SizeOfHeapCommit            QWORD ?
    LoaderFlags                 DWORD ?
    NumberOfRvaAndSizes         DWORD ?
    DataDirectory               IMAGE_DATA_DIRECTORY 16 DUP (<>)
IMAGE_OPTIONAL_HEADER64 ENDS

IMAGE_NT_HEADERS64 STRUCT 
    Signature      DWORD ?
    FileHeader     IMAGE_FILE_HEADER <>
    OptionalHeader IMAGE_OPTIONAL_HEADER64 <>
IMAGE_NT_HEADERS64 ENDS

IMAGE_EXPORT_DIRECTORY STRUCT
    Characteristics       DWORD ?
    TimeDateStamp         DWORD ?
    MajorVersion          WORD  ?
    MinorVersion          WORD  ?
    _Name                 DWORD ?
    Base                  DWORD ?
    NumberOfFunctions     DWORD ?
    NumberOfNames         DWORD ?
    AddressOfFunctions    DWORD ?
    AddressOfNames        DWORD ?
    AddressOfNameOrdinals DWORD ?
IMAGE_EXPORT_DIRECTORY ENDS

MINIMAL_SEH STRUCT
    SafeOffset         QWORD ?
    PrevRsp            QWORD ?
    PrevRbp            QWORD ?
    __unusedAlignment  QWORD ?
MINIMAL_SEH ENDS

;---------------------------------------------------------------------------------

.data
ALIGN 16
_apisInitialized DD 0

DECLARENTAPI MACRO apiName:REQ, defRetValue:REQ

.data
ALIGN 16
_&apiName&_Proc DQ 0
_&apiName&_Name DB "&apiName&", 0

.code
PUBLIC Mx&apiName&
ALIGN 16
Mx&apiName& PROC FRAME
    mov  QWORD PTR [rsp+8h], rcx
    mov  QWORD PTR [rsp+10h], rdx
    mov  QWORD PTR [rsp+18h], r8
    mov  QWORD PTR [rsp+20h], r9

    sub  rsp, SIZEOF_ARGUMENTS_HOME + 8h ;Mantain 16-byte alignment
.ALLOCSTACK SIZEOF_ARGUMENTS_HOME + 8h
.ENDPROLOG

    xor rax, rax
    lock cmpxchg QWORD PTR [_apisInitialized], rax
    jne  @F
    call InitializeNtApis
@@: add  rsp, SIZEOF_ARGUMENTS_HOME + 8h
    mov  rcx, QWORD PTR [rsp+8h]
    mov  rdx, QWORD PTR [rsp+10h]
    mov  r8, QWORD PTR [rsp+18h]
    mov  r9, QWORD PTR [rsp+20h]
    xor  rax, rax
    lock cmpxchg QWORD PTR [_&apiName&_Proc], rax
    je   @F
    jmp  rax
@@: mov rax, defRetValue
    ret

Mx&apiName& ENDP
             ENDM

DECLARENTAPI NtOpenProcess, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtOpenThread, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtOpenProcessToken, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtOpenThreadToken, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtDuplicateToken, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQueryInformationToken, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtSetInformationToken, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtGetContextThread, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtSetContextThread, STATUS_NOT_IMPLEMENTED

DECLARENTAPI RtlCreateUserThread, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtCreateEvent, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtOpenEvent, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtResetEvent, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtSetEvent, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtCreateMutant, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtOpenMutant, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtReleaseMutant, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtCreateFile, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtOpenFile, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtOpenDirectoryObject, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtOpenSymbolicLinkObject, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQuerySymbolicLinkObject, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtClose, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtReadFile, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtWriteFile, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtCancelIoFile, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQueryInformationFile, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtSetInformationFile, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtCreateKey, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtOpenKey, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtEnumerateKey, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtEnumerateValueKey, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQueryKey, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQueryValueKey, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtSetValueKey, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtDeleteKey, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtDeleteValueKey, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtFlushKey, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtDelayExecution, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtYieldExecution, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQuerySystemTime, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQueryPerformanceCounter, STATUS_NOT_IMPLEMENTED

DECLARENTAPI RtlInitializeCriticalSection, STATUS_NOT_IMPLEMENTED
DECLARENTAPI RtlInitializeCriticalSectionAndSpinCount, STATUS_NOT_IMPLEMENTED
DECLARENTAPI RtlDeleteCriticalSection, STATUS_NOT_IMPLEMENTED
DECLARENTAPI RtlEnterCriticalSection, STATUS_NOT_IMPLEMENTED
DECLARENTAPI RtlLeaveCriticalSection, STATUS_NOT_IMPLEMENTED
DECLARENTAPI RtlTryEnterCriticalSection, FALSE

DECLARENTAPI RtlGetNativeSystemInformation, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQuerySystemInformation, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQueryInformationProcess, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtSetInformationProcess, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQueryInformationThread, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtSetInformationThread, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQueryObject, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtWaitForSingleObject, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtWaitForMultipleObjects, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtAllocateVirtualMemory, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtFreeVirtualMemory, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtFlushVirtualMemory, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtReadVirtualMemory, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtWriteVirtualMemory, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQueryVirtualMemory, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtProtectVirtualMemory, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtCreateSection, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQuerySection, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtMapViewOfSection, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtUnmapViewOfSection, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtSuspendThread, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtResumeThread, STATUS_NOT_IMPLEMENTED

DECLARENTAPI RtlImpersonateSelf, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtAdjustPrivilegesToken, STATUS_NOT_IMPLEMENTED

DECLARENTAPI RtlRaiseException, VOID
DECLARENTAPI RtlNtStatusToDosError, ERROR_NOT_SUPPORTED
DECLARENTAPI NtFlushInstructionCache, STATUS_NOT_IMPLEMENTED

DECLARENTAPI RtlAllocateHeap, NULL
DECLARENTAPI RtlReAllocateHeap, NULL
DECLARENTAPI RtlFreeHeap, FALSE
DECLARENTAPI RtlCreateHeap, NULL
DECLARENTAPI RtlDestroyHeap, NULL
DECLARENTAPI RtlSizeHeap, 0

DECLARENTAPI NtTerminateProcess, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtTerminateThread, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtDuplicateObject, STATUS_NOT_IMPLEMENTED

DECLARENTAPI RtlGetVersion, STATUS_NOT_IMPLEMENTED

DECLARENTAPI LdrLoadDll, STATUS_NOT_IMPLEMENTED
DECLARENTAPI LdrUnloadDll, STATUS_NOT_IMPLEMENTED
DECLARENTAPI LdrFindEntryForAddress, STATUS_NOT_IMPLEMENTED
DECLARENTAPI LdrGetProcedureAddress, STATUS_NOT_IMPLEMENTED

DECLARENTAPI RtlAnsiStringToUnicodeString, STATUS_NOT_IMPLEMENTED
DECLARENTAPI RtlAnsiStringToUnicodeSize, 0
DECLARENTAPI RtlFreeUnicodeString, VOID
DECLARENTAPI RtlUnicodeStringToAnsiString, STATUS_NOT_IMPLEMENTED
DECLARENTAPI RtlUnicodeStringToAnsiSize, 0
DECLARENTAPI RtlFreeAnsiString, VOID
DECLARENTAPI RtlCompareUnicodeString, -1
DECLARENTAPI RtlCompareString, -1
DECLARENTAPI RtlUpcaseUnicodeChar, 0
DECLARENTAPI RtlDowncaseUnicodeChar, 0
DECLARENTAPI RtlUpperChar, 0

DECLARENTAPI RtlAddVectoredExceptionHandler, NULL
DECLARENTAPI RtlRemoveVectoredExceptionHandler, 0

DECLARENTAPI RtlFindClearBitsAndSet, 0FFFFFFFFh
DECLARENTAPI RtlAreBitsSet, FALSE
DECLARENTAPI RtlClearBits, VOID

DECLARENTAPI RtlAcquirePebLock, VOID
DECLARENTAPI RtlReleasePebLock, VOID

DECLARENTAPI RtlUnwind, VOID
DECLARENTAPI RtlUnwindEx, VOID

ifdef DEFINE_RTLUNWIND
.code
PUBLIC RtlUnwind
ALIGN 16
RtlUnwind:
    JMP MxRtlUnwind

PUBLIC RtlUnwindEx
ALIGN 16
RtlUnwindEx:
    JMP MxRtlUnwindEx

.data
___imp_RtlUnwind   DQ OFFSET MxRtlUnwind
___imp_RtlUnwindEx DQ OFFSET MxRtlUnwindEx
endif ;DEFINE_RTLUNWIND

DECLARENTAPI RtlCaptureContext, VOID
DECLARENTAPI RtlLookupFunctionEntry, NULL
DECLARENTAPI RtlVirtualUnwind, NULL

DECLARENTAPI RtlAddFunctionTable, FALSE
DECLARENTAPI RtlDeleteFunctionTable, FALSE
DECLARENTAPI RtlInstallFunctionTableCallback, FALSE

;---------------------------------------------------------------------------------

;BOOL MyStrNICmpW(LPWSTR string1, LPWSTR string2, DWORD dwStr1Len)
ALIGN 16
MyStrNICmpW PROC
    ;get string1 and check for null
    test rcx, rcx
    je   @@mismatch
    ;get string2 and check for null
    test rdx, rdx
    je   @@mismatch
    ;get length and check for zero
    test r8, r8
    je   @@afterloop
    mov  QWORD PTR [rsp+18h], r8 ;save 3rd parameter for later use
    mov  r8, rcx
    mov  r9, rdx
@@loop:
    ;compare letter
    mov  ax, WORD PTR [r8]
    mov  cx, WORD PTR [r9]
    cmp  cx, ax
    je   @F
    ;tolower
    sub  ax, 'A'
    cmp  ax, 'Z'-'A'+1
    sbb  dx, dx
    and  dx, 'a'-'A'
    add  ax, dx
    add  ax, 'A'
    ;tolower
    sub  cx, 'A'
    cmp  cx, 'Z'-'A'+1
    sbb  dx, dx
    and  dx, 'a'-'A'
    add  cx, dx
    add  cx, 'A'
    ;compare letter again
    cmp  cx, ax
    jne  @@mismatch
@@: add  r8, 2
    add  r9, 2
    dec  QWORD PTR [rsp+18h]
    jne  @@loop
@@afterloop:
    cmp  WORD PTR [r9], 0
    jne  @@mismatch
    xor  rax, rax
    inc  rax
    ret
@@mismatch:
    xor  rax, rax
    ret
MyStrNICmpW ENDP

;BOOL MyStrICmpA(LPSTR string1, LPSTR string2)
ALIGN 16
MyStrICmpA PROC
    ;get string1 and check for null
    test rcx, rcx
    je   @@mismatch
    ;get string2 and check for null
    test rdx, rdx
    je   @@mismatch
    mov  r8, rcx
    mov  r9, rdx
@@loop:
    ;compare letter
    mov  al, BYTE PTR [r8]
    mov  cl, BYTE PTR [r9]
    mov  ah, cl
    cmp  ax, 0
    je   @@match
    ;compare letter
    cmp  al, cl
    je   @F
    ;tolower
    sub  al, 'A'
    cmp  al, 'Z'-'A'+1
    sbb  dl, dl
    and  dl, 'a'-'A'
    add  al, dl
    add  al, 'A'
    ;tolower
    sub  cl, 'A'
    cmp  cl, 'Z'-'A'+1
    sbb  dl, dl
    and  dl, 'a'-'A'
    add  cl, dl
    add  cl, 'A'
    ;compare letter again
    cmp  al, cl
    jne  @@mismatch
@@: inc  r8
    inc  r9
    jmp  @@loop
@@match:
    xor  rax, rax
    inc  rax
    ret
@@mismatch:
    xor  rax, rax
    ret
MyStrICmpA ENDP

;DWORD SimpleAtolA(LPSTR string)
ALIGN 16
SimpleAtolA PROC
    xor  rax, rax
    xor  rdx, rdx
@@loop:
    mov  dl, BYTE PTR [rcx]
    inc  rcx
    cmp  dl, 30h
    jb   @@done
    cmp  dl, 39h
    ja   @@done
    sub  dl, 30h
    cmp  rax, 19999999h
    jae  @@overflow
    imul rax, 10
    add  rax, rdx
    jmp  @@loop
@@overflow:
    mov  rax, 0FFFFFFFFh
@@done:
    ret
SimpleAtolA ENDP

;LPVOID GetPEB()
ALIGN 16
OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE
GetPEB PROC
    mov  rax, QWORD PTR gs:[30h]
    mov  rax, QWORD PTR [rax+60h]
    ret
GetPEB ENDP
OPTION PROLOGUE:PrologueDef
OPTION EPILOGUE:EpilogueDef

;LPVOID GetLoaderLockAddr()
ALIGN 16
OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE
GetLoaderLockAddr PROC
    call GetPEB
    mov  rax, QWORD PTR [rax+110h]
    ret
GetLoaderLockAddr ENDP
OPTION PROLOGUE:PrologueDef
OPTION EPILOGUE:EpilogueDef

;BOOL CheckImageType(LPVOID lpBase, LPVOID *lplpNtHdr)
ALIGN 16
CheckImageType PROC
    xor  rax, rax
    ;get lpBase and check for null
    test rcx, rcx
    je   @@end
    ;check dos header magic
    cmp  WORD PTR [rcx].IMAGE_DOS_HEADER.e_magic, IMAGE_DOS_SIGNATURE
    jne  @@end
    ;get header offset
    xor  r8, r8
    mov  r8d, DWORD PTR [rcx].IMAGE_DOS_HEADER.e_lfanew
    add  rcx, r8 ;rcx now points to NtHeader address
    ;check if we are asked to store NtHeader address 
    test rdx, rdx
    je   @F
    mov  QWORD PTR [rdx], rcx ;save it
@@: ;check image type
    cmp  WORD PTR [rcx].IMAGE_NT_HEADERS64.FileHeader.Machine, IMAGE_FILE_MACHINE_AMD64
    jne  @@end
    ;check magic
    cmp  WORD PTR [rcx].IMAGE_NT_HEADERS64.Signature, IMAGE_NT_SIGNATURE
    jne  @@end
    inc  rax
@@end:
    ret
CheckImageType ENDP

;---------------------------------------------------------------------------------

PUBLIC MxGetModuleBaseAddress

;LPVOID MxGetModuleBaseAddress(LPWSTR szDllNameW)
ALIGN 16
MxGetModuleBaseAddress PROC FRAME
szDllNameW$ = (2*8h + SIZEOF_ARGUMENTS_HOME + 8h) + 8h

    push rbx
.PUSHREG RBX
    push r10
.PUSHREG R10
    sub  rsp, SIZEOF_ARGUMENTS_HOME + 8h ;Mantain 16-byte alignment
.ALLOCSTACK SIZEOF_ARGUMENTS_HOME + 8h
.ENDPROLOG

    mov  QWORD PTR szDllNameW$[rsp], rcx ;save 1st parameter for later use
    ;check parameters
    test rcx, rcx
    je   @@not_found
    call GetPEB
    mov  rax, QWORD PTR [rax+18h] ;peb64+24 => pointer to PEB_LDR_DATA64
    test rax, rax
    je   @@not_found
    cmp  DWORD PTR [rax+4], 0h ;check PEB_LDR_DATA64.Initialize flag
    je   @@not_found
    mov  r10, rax
    add  r10, 10h ;r10 has the first link (PEB_LDR_DATA64.InLoadOrderModuleList.Flink)
    mov  rbx, QWORD PTR [r10]
@@loop:
    cmp  rbx, r10
    je   @@not_found
    ;check if this is the entry we are looking for...
    movzx r8, WORD PTR [rbx].MODULE_ENTRY64.BaseDllName._Length
    test r8, r8
    je   @@next
    shr  r8, 1 ;divide by 2 because they are unicode chars
    mov  rcx, QWORD PTR [rbx].MODULE_ENTRY64.BaseDllName.Buffer
    test rcx, rcx
    je   @@next
    mov  rdx, QWORD PTR szDllNameW$[rsp]
    CALL MyStrNICmpW
    test rax, rax
    je   @@next
    ;got it
    mov  rcx, QWORD PTR [rbx].MODULE_ENTRY64.DllBase
    xor  rdx, rdx
    call CheckImageType
    test rax, rax
    je   @@next
    mov  rax, QWORD PTR [rbx].MODULE_ENTRY64.DllBase
    jmp  @@found
@@next:
    mov  rbx, QWORD PTR [rbx].MODULE_ENTRY64.InLoadOrderLinks.Flink ;go to the next entry
    jmp  @@loop
@@not_found:
    xor  rax, rax
@@found:
    add  rsp, SIZEOF_ARGUMENTS_HOME + 8h
    pop  r10
    pop  rbx
    ret
MxGetModuleBaseAddress ENDP

;---------------------------------------------------------------------------------

PUBLIC MxGetProcedureAddress

;LPVOID MxGetProcedureAddress(LPVOID lpDllBase, LPSTR szFuncNameA)
ALIGN 16
MxGetProcedureAddress PROC FRAME
lpDllBase$ = (2*8h + SIZEOF_ARGUMENTS_HOME + 8h) + 8h
szFuncNameA$ = (2*8h + SIZEOF_ARGUMENTS_HOME + 8h) + 10h

_lpNtHdr$ = 32
_nNamesCount$ = 40
_lpAddrOfNames$ = 48
_szTempStrW$ = 56

    push r13
.PUSHREG R13
    push r14
.PUSHREG R14
    push r15
.PUSHREG R15
    sub  rsp, 88h + SIZEOF_ARGUMENTS_HOME + 8h ;Mantain 16-byte alignment
.ALLOCSTACK 88h + SIZEOF_ARGUMENTS_HOME + 8h
.ENDPROLOG

@@restart:
    mov  QWORD PTR lpDllBase$[rsp], rcx   ;save 1st parameter for later use
    mov  QWORD PTR szFuncNameA$[rsp], rdx ;save 2nd parameter for later use
    ;check for non-null
    test rcx, rcx
    je   @@not_found
    test rdx, rdx
    je   @@not_found
    ;get module base address and check for null
    test rcx, rcx
    je   @@not_found
    ;get nt header
    lea  rdx, QWORD PTR _lpNtHdr$[rsp]
    call CheckImageType
    test rax, rax
    je   @@not_found
    ;check export data directory
    mov  rax, QWORD PTR _lpNtHdr$[rsp]
    xor  r14, r14
    mov  r14d, DWORD PTR [rax].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[0].VirtualAddress
    test r14d, r14d
    je   @@not_found
    xor  r15, r15
    mov  r15d, DWORD PTR [rax].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[0]._Size
    test r15d, r15d
    je   @@not_found
    add  r14, QWORD PTR lpDllBase$[rsp]
    add  r15, r14
    ;locating an export by ordinal?
    mov  rcx, QWORD PTR szFuncNameA$[rsp]
    cmp  BYTE PTR [rcx], 23h ; is "#"
    jne  @@findByName
    inc  rcx
    call SimpleAtolA
    cmp  eax, DWORD PTR [r14].IMAGE_EXPORT_DIRECTORY.Base
    jb   @@not_found
    cmp  eax, DWORD PTR [r14].IMAGE_EXPORT_DIRECTORY.NumberOfFunctions
    jae  @@not_found
    ;get the ordinal
    sub  eax, DWORD PTR [r14].IMAGE_EXPORT_DIRECTORY.Base
    xor  rcx, rcx
    mov  ecx, eax
    jmp  @@getAddress
@@findByName:
    ;get the number of names
    xor  rax, rax
    mov  eax, DWORD PTR [r14].IMAGE_EXPORT_DIRECTORY.NumberOfNames
    mov  QWORD PTR _nNamesCount$[rsp], rax
    ;get the AddressOfNames
    xor  rax, rax
    mov  eax, DWORD PTR [r14].IMAGE_EXPORT_DIRECTORY.AddressOfNames
    add  rax, QWORD PTR lpDllBase$[rsp]
    mov  QWORD PTR _lpAddrOfNames$[rsp], rax
    ;main loop
    xor  r13, r13
@@loop:
    cmp  r13, QWORD PTR _nNamesCount$[rsp]
    jae  @@not_found
    ;get exported name
    mov  rdx, QWORD PTR szFuncNameA$[rsp]
    mov  rax, QWORD PTR _lpAddrOfNames$[rsp]
    xor  rcx, rcx
    mov  ecx, DWORD PTR [rax]
    add  rcx, QWORD PTR lpDllBase$[rsp]
    call MyStrICmpA
    test rax, rax
    je   @@next
    ;got the function
    xor  rax, rax
    mov  eax, DWORD PTR [r14].IMAGE_EXPORT_DIRECTORY.AddressOfNameOrdinals
    add  rax, QWORD PTR lpDllBase$[rsp]
    shl  r13, 1
    add  rax, r13
    xor  rcx, rcx
    mov  cx, WORD PTR [rax] ;get the ordinal of this function
@@getAddress:
    xor  rax, rax
    mov  eax, DWORD PTR [r14].IMAGE_EXPORT_DIRECTORY.AddressOfFunctions
    add  rax, QWORD PTR lpDllBase$[rsp]
    shl  rcx, 2
    add  rcx, rax
    ;get the function address
    xor  rax, rax
    mov  eax, DWORD PTR [rcx]
    add  rax, QWORD PTR lpDllBase$[rsp]
    ;check if inside export table zone
    cmp  rax, r14
    jb   @@found
    cmp  rax, r15
    jae  @@found
    ;got a forward declaration
    lea  r8, QWORD PTR _szTempStrW$[rsp]
    xor  rcx, rcx
@@: cmp  rcx, 32
    jae  @@not_found ;dll name is too long
    movzx dx, BYTE PTR [rax+rcx]
    cmp  dl, 2Eh ;a dot?
    je   @F
    mov  WORD PTR [r8+rcx*2], dx
    inc  rcx
    jmp  @B
@@: ;add ".dll" to string
    mov  WORD PTR [r8+rcx*2], 2Eh
    mov  WORD PTR [r8+rcx*2+2], 64h
    mov  WORD PTR [r8+rcx*2+4], 6Ch
    mov  WORD PTR [r8+rcx*2+6], 6Ch
    mov  WORD PTR [r8+rcx*2+8], 0
    add  rcx, rax ;rcx points to the dot
@@: ;skip after the dot (no validity checks are done)
    cmp  BYTE PTR [rcx], 2Eh
    je   @F
    inc  rcx
    jmp  @B
@@: lea  r13, [rcx+1] ;skip the dot
    lea  rcx, QWORD PTR _szTempStrW$[rsp]
    call MxGetModuleBaseAddress
    cmp  rax, 0
    je   @@not_found ;forwarded module was not found
    ;RAX contains the module instance
    ;R13 contains the pointer to the api string (valid until module unload)
    mov  rcx, rax
    mov  rdx, r13
    jmp  @@restart

@@next:
    add  QWORD PTR _lpAddrOfNames$[rsp], 4
    inc  r13
    jmp  @@loop
@@not_found:
    xor  rax, rax
@@found:

    add  rsp, 88h + SIZEOF_ARGUMENTS_HOME + 8h
    pop  r15
    pop  r14
    pop  r13
    ret
MxGetProcedureAddress ENDP

;---------------------------------------------------------------------------------

MxCallWithSEH_SEH PROTO

ALIGN 16
;SIZE_T __stdcall MxCallWithSEH(_In_ LPVOID lpFunc, _Out_opt_ BOOL *lpExceptionRaised, _In_opt_ SIZE_T nParam1=0,
;                               _In_opt_ SIZE_T nParam2=0, _In_opt_ SIZE_T nParam3=0);
MxCallWithSEH PROC FRAME :MxCallWithSEH_SEH
    push rbp
.PUSHREG rbp
    mov  rbp, rsp
.SETFRAME rbp, 0
    sub  rsp, 20h + sizeof MINIMAL_SEH+20h
.ALLOCSTACK 20h + sizeof MINIMAL_SEH+20h
    lea  rbp, [rsp + 20h]
.SETFRAME rbp, 20h
.ENDPROLOG
seh        EQU <[rbp].MINIMAL_SEH>
origRdx    EQU [rbp + sizeof MINIMAL_SEH]
fifthParam EQU  rbp + sizeof MINIMAL_SEH+20h + 28h

    mov  QWORD PTR origRdx, rdx
    test rdx, rdx
    je   @F
    mov  DWORD PTR [rdx], 0
    ;setup SEH
@@: mov  rax, OFFSET @@onException
    mov  seh.SafeOffset, rax
    mov  seh.PrevRsp, rsp
    mov  seh.PrevRbp, rbp

    ;do call
    mov  rax, rcx

    mov  rcx, r8
    mov  rdx, r9
    mov  r8,  QWORD PTR [fifthParam]
    call rax

    ;done
    lea  rsp, [rbp + sizeof MINIMAL_SEH+20h]
    pop  rbp
    ret
@@onException:
    mov  rax, origRdx
    test rax, rax
    je   @F
    inc  QWORD PTR [rax]
@@: xor  rax, rax
    lea  rsp, [rbp + sizeof MINIMAL_SEH+20h]
    pop  rbp
    ret
MxCallWithSEH ENDP

ALIGN 16
MxCallWithSEH_SEH PROC
seh EQU <[rdx - sizeof MINIMAL_SEH].MINIMAL_SEH>

    mov  rax, seh.SafeOffset
    mov  QWORD PTR [r8+0F8h], rax ;r8.CONTEXT._Rip
    mov  rax, seh.PrevRsp
    mov  QWORD PTR [r8+98h], rax ;r8.CONTEXT.Rsp
    mov  rax, seh.PrevRbp
    mov  QWORD PTR [r8+0A0h], rax ;r8.CONTEXT.Rbp
    xor  rax, rax
    mov  QWORD PTR [r8+78h], rax ;r8.CONTEXT.Rax
    ;rax = 0 & ExceptionContinueExecution = 0 so leave rax untouched and return
    ret ;return with ExceptionContinueExecution
MxCallWithSEH_SEH ENDP

;---------------------------------------------------------------------------------

DECLARENTAPI_FORINIT MACRO apiName:REQ

    mov  rcx, QWORD PTR _hNtDll$[rsp]
    lea  rdx, QWORD PTR [_&apiName&_Name]
    call MxGetProcedureAddress
    xchg QWORD PTR [_&apiName&_Proc], rax

                     ENDM

.data
ALIGN 16
_szNtDllW DB 'n', 0, 't', 0, 'd', 0, 'l', 0, 'l', 0, '.', 0, 'd', 0, 'l', 0, 'l', 0, 0, 0

.code
ALIGN 16
InitializeNtApis PROC FRAME
_hNtDll$ = 32
    sub  rsp, 10h + SIZEOF_ARGUMENTS_HOME + 8h ;Mantain 16-byte alignment
.ALLOCSTACK 10h + SIZEOF_ARGUMENTS_HOME + 8h
.ENDPROLOG

    lea  rcx, QWORD PTR [_szNtDllW]
    call MxGetModuleBaseAddress
    cmp  rax, 0
    je   @@afterGetProc
    mov  QWORD PTR _hNtDll$[rsp], rax

    DECLARENTAPI_FORINIT NtOpenProcess
    DECLARENTAPI_FORINIT NtOpenThread

    DECLARENTAPI_FORINIT NtOpenProcessToken
    DECLARENTAPI_FORINIT NtOpenThreadToken
    DECLARENTAPI_FORINIT NtDuplicateToken
    DECLARENTAPI_FORINIT NtQueryInformationToken
    DECLARENTAPI_FORINIT NtSetInformationToken

    DECLARENTAPI_FORINIT NtGetContextThread
    DECLARENTAPI_FORINIT NtSetContextThread

    DECLARENTAPI_FORINIT RtlCreateUserThread

    DECLARENTAPI_FORINIT NtCreateEvent
    DECLARENTAPI_FORINIT NtOpenEvent
    DECLARENTAPI_FORINIT NtResetEvent
    DECLARENTAPI_FORINIT NtSetEvent

    DECLARENTAPI_FORINIT NtCreateMutant
    DECLARENTAPI_FORINIT NtOpenMutant
    DECLARENTAPI_FORINIT NtReleaseMutant

    DECLARENTAPI_FORINIT NtCreateFile
    DECLARENTAPI_FORINIT NtOpenFile
    DECLARENTAPI_FORINIT NtOpenDirectoryObject
    DECLARENTAPI_FORINIT NtOpenSymbolicLinkObject
    DECLARENTAPI_FORINIT NtQuerySymbolicLinkObject
    DECLARENTAPI_FORINIT NtClose
    DECLARENTAPI_FORINIT NtReadFile
    DECLARENTAPI_FORINIT NtWriteFile
    DECLARENTAPI_FORINIT NtCancelIoFile
    DECLARENTAPI_FORINIT NtQueryInformationFile
    DECLARENTAPI_FORINIT NtSetInformationFile

    DECLARENTAPI_FORINIT NtCreateKey
    DECLARENTAPI_FORINIT NtOpenKey
    DECLARENTAPI_FORINIT NtEnumerateKey
    DECLARENTAPI_FORINIT NtEnumerateValueKey
    DECLARENTAPI_FORINIT NtQueryKey
    DECLARENTAPI_FORINIT NtQueryValueKey
    DECLARENTAPI_FORINIT NtSetValueKey
    DECLARENTAPI_FORINIT NtDeleteKey
    DECLARENTAPI_FORINIT NtDeleteValueKey
    DECLARENTAPI_FORINIT NtFlushKey

    DECLARENTAPI_FORINIT NtDelayExecution
    DECLARENTAPI_FORINIT NtYieldExecution
    DECLARENTAPI_FORINIT NtQuerySystemTime
    DECLARENTAPI_FORINIT NtQueryPerformanceCounter

    DECLARENTAPI_FORINIT RtlInitializeCriticalSection
    DECLARENTAPI_FORINIT RtlInitializeCriticalSectionAndSpinCount
    DECLARENTAPI_FORINIT RtlDeleteCriticalSection
    DECLARENTAPI_FORINIT RtlEnterCriticalSection
    DECLARENTAPI_FORINIT RtlLeaveCriticalSection
    DECLARENTAPI_FORINIT RtlTryEnterCriticalSection

    DECLARENTAPI_FORINIT RtlGetNativeSystemInformation
    DECLARENTAPI_FORINIT NtQuerySystemInformation
    DECLARENTAPI_FORINIT NtQueryInformationProcess
    DECLARENTAPI_FORINIT NtSetInformationProcess
    DECLARENTAPI_FORINIT NtQueryInformationThread
    DECLARENTAPI_FORINIT NtSetInformationThread
    DECLARENTAPI_FORINIT NtQueryObject

    DECLARENTAPI_FORINIT NtWaitForSingleObject
    DECLARENTAPI_FORINIT NtWaitForMultipleObjects

    DECLARENTAPI_FORINIT NtAllocateVirtualMemory
    DECLARENTAPI_FORINIT NtFreeVirtualMemory
    DECLARENTAPI_FORINIT NtFlushVirtualMemory
    DECLARENTAPI_FORINIT NtReadVirtualMemory
    DECLARENTAPI_FORINIT NtWriteVirtualMemory
    DECLARENTAPI_FORINIT NtQueryVirtualMemory
    DECLARENTAPI_FORINIT NtProtectVirtualMemory

    DECLARENTAPI_FORINIT NtCreateSection
    DECLARENTAPI_FORINIT NtQuerySection
    DECLARENTAPI_FORINIT NtMapViewOfSection
    DECLARENTAPI_FORINIT NtUnmapViewOfSection

    DECLARENTAPI_FORINIT NtSuspendThread
    DECLARENTAPI_FORINIT NtResumeThread

    DECLARENTAPI_FORINIT RtlImpersonateSelf
    DECLARENTAPI_FORINIT NtAdjustPrivilegesToken

    DECLARENTAPI_FORINIT RtlRaiseException
    DECLARENTAPI_FORINIT RtlNtStatusToDosError
    DECLARENTAPI_FORINIT NtFlushInstructionCache

    DECLARENTAPI_FORINIT RtlAllocateHeap
    DECLARENTAPI_FORINIT RtlReAllocateHeap
    DECLARENTAPI_FORINIT RtlFreeHeap
    DECLARENTAPI_FORINIT RtlCreateHeap
    DECLARENTAPI_FORINIT RtlDestroyHeap
    DECLARENTAPI_FORINIT RtlSizeHeap

    DECLARENTAPI_FORINIT NtTerminateProcess
    DECLARENTAPI_FORINIT NtTerminateThread

    DECLARENTAPI_FORINIT NtDuplicateObject

    DECLARENTAPI_FORINIT RtlGetVersion

    DECLARENTAPI_FORINIT LdrLoadDll
    DECLARENTAPI_FORINIT LdrUnloadDll
    DECLARENTAPI_FORINIT LdrFindEntryForAddress
    DECLARENTAPI_FORINIT LdrGetProcedureAddress

    DECLARENTAPI_FORINIT RtlAnsiStringToUnicodeString
    DECLARENTAPI_FORINIT RtlAnsiStringToUnicodeSize
    DECLARENTAPI_FORINIT RtlFreeUnicodeString
    DECLARENTAPI_FORINIT RtlUnicodeStringToAnsiString
    DECLARENTAPI_FORINIT RtlUnicodeStringToAnsiSize
    DECLARENTAPI_FORINIT RtlFreeAnsiString
    DECLARENTAPI_FORINIT RtlCompareUnicodeString
    DECLARENTAPI_FORINIT RtlCompareString
    DECLARENTAPI_FORINIT RtlUpcaseUnicodeChar
    DECLARENTAPI_FORINIT RtlDowncaseUnicodeChar
    DECLARENTAPI_FORINIT RtlUpperChar

    DECLARENTAPI_FORINIT RtlAddVectoredExceptionHandler
    DECLARENTAPI_FORINIT RtlRemoveVectoredExceptionHandler

    DECLARENTAPI_FORINIT RtlFindClearBitsAndSet
    DECLARENTAPI_FORINIT RtlAreBitsSet
    DECLARENTAPI_FORINIT RtlClearBits

    DECLARENTAPI_FORINIT RtlAcquirePebLock
    DECLARENTAPI_FORINIT RtlReleasePebLock

    DECLARENTAPI_FORINIT RtlUnwind
    DECLARENTAPI_FORINIT RtlUnwindEx

    DECLARENTAPI_FORINIT RtlCaptureContext
    DECLARENTAPI_FORINIT RtlLookupFunctionEntry
    DECLARENTAPI_FORINIT RtlVirtualUnwind

    DECLARENTAPI_FORINIT RtlAddFunctionTable
    DECLARENTAPI_FORINIT RtlDeleteFunctionTable
    DECLARENTAPI_FORINIT RtlInstallFunctionTableCallback

@@afterGetProc:
    mov  rdx, 1
    mov  rax, QWORD PTR [_apisInitialized]
@@: nop
    lock cmpxchg QWORD PTR [_apisInitialized], rdx
    jne  @B
    add  rsp, 10h + SIZEOF_ARGUMENTS_HOME + 8h
    ret
InitializeNtApis ENDP

END
