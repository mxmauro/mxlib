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
.486
.model flat, stdcall

;---------------------------------------------------------------------------------

STATUS_NOT_IMPLEMENTED         EQU    0C0000002h
VOID                           EQU    0
FALSE                          EQU    0
NULL                           EQU    0
ERROR_NOT_SUPPORTED            EQU    50

IMAGE_DOS_SIGNATURE            EQU    5A4Dh     ;MZ
IMAGE_NT_SIGNATURE             EQU    00004550h ;PE00
IMAGE_FILE_MACHINE_I386        EQU    14ch
IMAGE_NT_OPTIONAL_HDR32_MAGIC  EQU    10Bh

;---------------------------------------------------------------------------------

UNICODE_STRING32 STRUCT 8
    _Length       WORD  ?
    MaximumLength WORD  ?
    Buffer        DWORD ?
UNICODE_STRING32 ENDS
   
LIST_ENTRY32 STRUCT
    Flink DWORD ?
    Blink DWORD ?
LIST_ENTRY32 ENDS

MODULE_ENTRY32 STRUCT
    InLoadOrderLinks           LIST_ENTRY32 <>
    InMemoryOrderLinks         LIST_ENTRY32 <>
    InInitializationOrderLinks LIST_ENTRY32 <>
    DllBase                    DWORD ?
    EntryPoint                 DWORD ?
    SizeOfImage                DWORD ?
    FullDllName                UNICODE_STRING32 <>
    BaseDllName                UNICODE_STRING32 <>
    Flags                      DWORD ?
    LoadCount                  WORD  ?
    ;structure continues but it is not needed
MODULE_ENTRY32 ENDS

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
    e_res      WORD  4 DUP (?)
    e_oemid    WORD  ?
    e_oeminfo  WORD  ?
    e_res2     WORD 10 DUP (?)
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

IMAGE_OPTIONAL_HEADER32 STRUCT
    Magic                       WORD  ?
    MajorLinkerVersion          BYTE  ?
    MinorLinkerVersion          BYTE  ?
    SizeOfCode                  DWORD ?
    SizeOfInitializedData       DWORD ?
    SizeOfUninitializedData     DWORD ?
    AddressOfEntryPoint         DWORD ?
    BaseOfCode                  DWORD ?
    BaseOfData                  DWORD ?
    ;NT additional fields
    ImageBase                   DWORD ?
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
    SizeOfStackReserve          DWORD ?
    SizeOfStackCommit           DWORD ?
    SizeOfHeapReserve           DWORD ?
    SizeOfHeapCommit            DWORD ?
    LoaderFlags                 DWORD ?
    NumberOfRvaAndSizes         DWORD ?
    DataDirectory               IMAGE_DATA_DIRECTORY 16 DUP (<>)
IMAGE_OPTIONAL_HEADER32 ENDS

IMAGE_NT_HEADERS32 STRUCT 
    Signature      DWORD ?
    FileHeader     IMAGE_FILE_HEADER <>
    OptionalHeader IMAGE_OPTIONAL_HEADER32 <>
IMAGE_NT_HEADERS32 ENDS

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
    PrevLink        DWORD ?
    CurrentHandler  DWORD ?
    SafeOffset      DWORD ?
    PrevEsp         DWORD ?
    PrevEbp         DWORD ?
MINIMAL_SEH ENDS

;---------------------------------------------------------------------------------

.data
ALIGN 4
_apisInitialized DD 0

DECLARENTAPI MACRO apiName:REQ, paramsBytesCount:REQ, defRetValue:REQ

.data
ALIGN 4
_&apiName&_Proc DD 0
_&apiName&_Name DB "&apiName&", 0

.code
PUBLIC Mx&apiName&@&paramsBytesCount&
ALIGN 4
Mx&apiName&@&paramsBytesCount&:
    xor  eax, eax
    lock cmpxchg DWORD PTR [_apisInitialized], eax
    jne  @F
    call InitializeNtApis
@@: xor  eax, eax
    lock cmpxchg DWORD PTR [_&apiName&_Proc], eax
    je   @F
    jmp  eax
@@: mov eax, defRetValue
    ret paramsBytesCount

             ENDM

DECLARENTAPI NtOpenProcess, 16, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtOpenThread, 16, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtOpenProcessToken, 12, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtOpenThreadToken, 16, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtDuplicateToken, 24, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQueryInformationToken, 20, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtSetInformationToken, 16, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtGetContextThread, 8, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtSetContextThread, 8, STATUS_NOT_IMPLEMENTED

DECLARENTAPI RtlCreateUserThread, 40, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtCreateEvent, 20, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtOpenEvent, 12, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtResetEvent, 8, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtSetEvent, 8, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtCreateMutant, 16, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtOpenMutant, 12, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtReleaseMutant, 8, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtCreateFile, 44, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtOpenFile, 24, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtOpenDirectoryObject, 12, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtOpenSymbolicLinkObject, 12, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQuerySymbolicLinkObject, 12, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtClose, 4, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtReadFile, 36, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtWriteFile, 36, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtCancelIoFile, 8, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQueryInformationFile, 20, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtSetInformationFile, 20, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtCreateKey, 28, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtOpenKey, 12, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtEnumerateKey, 24, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtEnumerateValueKey, 24, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQueryKey, 20, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQueryValueKey, 24, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtSetValueKey, 24, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtDeleteKey, 4, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtDeleteValueKey, 8, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtFlushKey, 4, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtDelayExecution, 8, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtYieldExecution, 0, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQuerySystemTime, 4, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQueryPerformanceCounter, 8, STATUS_NOT_IMPLEMENTED

DECLARENTAPI RtlInitializeCriticalSection, 4, STATUS_NOT_IMPLEMENTED
DECLARENTAPI RtlInitializeCriticalSectionAndSpinCount, 8, STATUS_NOT_IMPLEMENTED
DECLARENTAPI RtlDeleteCriticalSection, 4, STATUS_NOT_IMPLEMENTED
DECLARENTAPI RtlEnterCriticalSection, 4, STATUS_NOT_IMPLEMENTED
DECLARENTAPI RtlLeaveCriticalSection, 4, STATUS_NOT_IMPLEMENTED
DECLARENTAPI RtlTryEnterCriticalSection, 4, FALSE

DECLARENTAPI RtlGetNativeSystemInformation, 16, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQuerySystemInformation, 16, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQueryInformationProcess, 20, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtSetInformationProcess, 16, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQueryInformationThread, 20, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtSetInformationThread, 16, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQueryObject, 20, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtWaitForSingleObject, 12, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtWaitForMultipleObjects, 20, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtAllocateVirtualMemory, 24, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtFreeVirtualMemory, 16, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtFlushVirtualMemory, 16, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtReadVirtualMemory, 20, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtWriteVirtualMemory, 20, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtQueryVirtualMemory, 24, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtProtectVirtualMemory, 20, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtCreateSection, 28, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtMapViewOfSection, 40, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtUnmapViewOfSection, 8, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtSuspendThread, 8, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtResumeThread, 8, STATUS_NOT_IMPLEMENTED

DECLARENTAPI RtlImpersonateSelf, 4, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtAdjustPrivilegesToken, 24, STATUS_NOT_IMPLEMENTED

DECLARENTAPI RtlRaiseException, 4, VOID
DECLARENTAPI RtlNtStatusToDosError, 4, ERROR_NOT_SUPPORTED
DECLARENTAPI NtFlushInstructionCache, 12, STATUS_NOT_IMPLEMENTED

DECLARENTAPI RtlAllocateHeap, 12, NULL
DECLARENTAPI RtlReAllocateHeap, 16, NULL
DECLARENTAPI RtlFreeHeap, 12, FALSE
DECLARENTAPI RtlCreateHeap, 24, NULL
DECLARENTAPI RtlDestroyHeap, 4, NULL
DECLARENTAPI RtlSizeHeap, 12, 0

DECLARENTAPI RtlUnwind, 16, VOID

ifdef DEFINE_RTLUNWIND
.code
PUBLIC RtlUnwind@16
ALIGN 4
RtlUnwind@16:
    JMP MxRtlUnwind@16

.data
___imp_RtlUnwind   DQ OFFSET MxRtlUnwind@16
endif ;DEFINE_RTLUNWIND

DECLARENTAPI NtTerminateProcess, 8, STATUS_NOT_IMPLEMENTED
DECLARENTAPI NtTerminateThread, 8, STATUS_NOT_IMPLEMENTED

DECLARENTAPI NtDuplicateObject, 28, STATUS_NOT_IMPLEMENTED

DECLARENTAPI RtlGetVersion, 4, STATUS_NOT_IMPLEMENTED

DECLARENTAPI LdrLoadDll, 16, STATUS_NOT_IMPLEMENTED
DECLARENTAPI LdrUnloadDll, 4, STATUS_NOT_IMPLEMENTED
DECLARENTAPI LdrFindEntryForAddress, 8, STATUS_NOT_IMPLEMENTED
DECLARENTAPI LdrGetProcedureAddress, 16, STATUS_NOT_IMPLEMENTED

DECLARENTAPI RtlAnsiStringToUnicodeString, 12, STATUS_NOT_IMPLEMENTED
DECLARENTAPI RtlAnsiStringToUnicodeSize, 4, 0
DECLARENTAPI RtlFreeUnicodeString, 4, VOID
DECLARENTAPI RtlUnicodeStringToAnsiString, 12, STATUS_NOT_IMPLEMENTED
DECLARENTAPI RtlUnicodeStringToAnsiSize, 4, 0
DECLARENTAPI RtlFreeAnsiString, 4, VOID
DECLARENTAPI RtlCompareUnicodeString, 12, -1
DECLARENTAPI RtlCompareString, 12, -1
DECLARENTAPI RtlUpcaseUnicodeChar, 4, 0
DECLARENTAPI RtlDowncaseUnicodeChar, 4, 0
DECLARENTAPI RtlUpperChar, 4, 0

DECLARENTAPI RtlAddVectoredExceptionHandler, 8, NULL
DECLARENTAPI RtlRemoveVectoredExceptionHandler, 4, 0

DECLARENTAPI RtlFindClearBitsAndSet, 12, 0FFFFFFFFh
DECLARENTAPI RtlAreBitsSet, 12, FALSE
DECLARENTAPI RtlClearBits, 12, VOID

DECLARENTAPI RtlAcquirePebLock, 0, VOID
DECLARENTAPI RtlReleasePebLock, 0, VOID

;---------------------------------------------------------------------------------

;BOOL MyStrNICmpW(LPWSTR string1, LPWSTR string2, DWORD dwStr1Len)
ALIGN 4
MyStrNICmpW PROC STDCALL USES ebx ecx edx esi edi, string1:DWORD, string2:DWORD, dwStr1Len:DWORD
    ;get string1 and check for null
    mov  esi, DWORD PTR string1
    test esi, esi
    je   @@mismatch
    ;get string2 and check for null
    mov  edi, DWORD PTR string2
    test edi, edi
    je   @@mismatch
    ;get length and check for zero
    mov  ecx, DWORD PTR dwStr1Len
    test ecx, ecx
    je   @@afterloop
@@loop:
    mov  ax, WORD PTR [esi]
    mov  bx, WORD PTR [edi]
    ;compare letter
    cmp  bx, ax
    je   @F
    ;tolower
    sub  ax, 'A'
    cmp  ax, 'Z'-'A'+1
    sbb  dx, dx
    and  dx, 'a'-'A'
    add  ax, dx
    add  ax, 'A'
    ;tolower
    sub  bx, 'A'
    cmp  bx, 'Z'-'A'+1
    sbb  dx, dx
    and  dx, 'a'-'A'
    add  bx, dx
    add  bx, 'A'
    ;compare letter again
    cmp  bx, ax
    jne  @@mismatch
@@: add  esi, 2
    add  edi, 2
    dec  ecx
    jne  @@loop
@@afterloop:
    cmp  WORD PTR [edi], 0
    jne  @@mismatch
    xor  eax, eax
    inc  eax
    jmp  @@end
@@mismatch:
    xor  eax, eax
@@end:
    ret  ;pop parameters
MyStrNICmpW ENDP

;BOOL MyStrICmpA(LPSTR string1, LPSTR string2)
ALIGN 4
MyStrICmpA PROC STDCALL USES ecx edx esi edi, string1:DWORD, string2:DWORD
    ;get string1 and check for null
    mov  esi, DWORD PTR string1
    test esi, esi
    je   @@mismatch
    ;get string2 and check for null
    mov  edi, DWORD PTR string2
    test edi, edi
    je   @@mismatch
@@loop:
    mov  al, BYTE PTR [esi]
    mov  ah, BYTE PTR [edi]
    cmp  ax, 0
    je   @@match
    ;compare letter
    cmp  al, ah
    je   @F
    ;tolower
    sub  al, 'A'
    cmp  al, 'Z'-'A'+1
    sbb  dl, dl
    and  dl, 'a'-'A'
    add  al, dl
    add  al, 'A'
    ;tolower
    sub  ah, 'A'
    cmp  ah, 'Z'-'A'+1
    sbb  dl, dl
    and  dl, 'a'-'A'
    add  ah, dl
    add  ah, 'A'
    ;compare letter again
    cmp  al, ah
    jne  @@mismatch
@@: inc  esi
    inc  edi
    jmp  @@loop
@@match:
    xor  eax, eax
    inc  eax
    jmp  @@end
@@mismatch:
    xor  eax, eax
@@end:
    ret  ;pop parameters
MyStrICmpA ENDP

;DWORD SimpleAtolA(LPSTR string)
ALIGN 4
SimpleAtolA PROC STDCALL USES ebx ecx edx, string:DWORD
    mov  ecx, DWORD PTR string
    xor  eax, eax
    xor  ebx, ebx
@@loop:
    mov  bl, BYTE PTR [ecx]
    inc  ecx
    cmp  bl, '0'
    jb   @@done
    cmp  bl, '9'
    ja   @@done
    sub  bl, '0'
    cmp  eax, 19999999h
    jae  @@overflow
    imul eax, 10
    add  eax, ebx
    jmp  @@loop
@@overflow:
    mov  eax, 0FFFFFFFFh
@@done:
    ret  ;pop parameters
SimpleAtolA ENDP

;LPVOID GetPEB()
ALIGN 4
OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE
GetPEB PROC STDCALL
ASSUME FS:NOTHING
    mov  eax, DWORD PTR [fs:30h]
ASSUME FS:ERROR
    ret
GetPEB ENDP
OPTION PROLOGUE:PrologueDef
OPTION EPILOGUE:EpilogueDef

;LPVOID GetLoaderLockAddr()
ALIGN 4
OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE
GetLoaderLockAddr PROC STDCALL
    call GetPEB
    mov  eax, DWORD PTR [eax+0A0h]
    ret
GetLoaderLockAddr ENDP
OPTION PROLOGUE:PrologueDef
OPTION EPILOGUE:EpilogueDef

;BOOL CheckImageType(LPVOID lpBase, LPVOID *lplpNtHdr)
ALIGN 4
CheckImageType PROC STDCALL USES ecx edx, lpBase:DWORD, lplpNtHdr:DWORD
    ;get lpBase and check for null
    xor  eax, eax
    mov  ecx, DWORD PTR lpBase
    test ecx, ecx
    je   @@end
    ;check dos header magic
    cmp  WORD PTR [ecx].IMAGE_DOS_HEADER.e_magic, IMAGE_DOS_SIGNATURE
    jne  @@end
    ;get header offset
    mov  edx, DWORD PTR [ecx].IMAGE_DOS_HEADER.e_lfanew
    add  ecx, edx ;ecx now points to NtHeader address
    ;check if we are asked to store NtHeader address 
    mov  edx, DWORD PTR lplpNtHdr
    test edx, edx
    je   @F
    mov  DWORD PTR [edx], ecx ;save it
@@: ;check image type
    cmp  WORD PTR [ecx].IMAGE_NT_HEADERS32.FileHeader.Machine, IMAGE_FILE_MACHINE_I386
    jne  @@end
    ;check magic
    cmp  WORD PTR [ecx].IMAGE_NT_HEADERS32.Signature, IMAGE_NT_SIGNATURE
    jne  @@end
    inc  eax
@@end:
    ret  ;pop parameters
CheckImageType ENDP

;---------------------------------------------------------------------------------

PUBLIC MxGetModuleBaseAddress

;LPVOID MxGetModuleBaseAddress(LPWSTR szDllNameW)
ALIGN 4
MxGetModuleBaseAddress PROC STDCALL USES ebx ecx edx esi, szDllNameW:DWORD
    cmp  DWORD PTR szDllNameW, 0
    je   @@not_found
    call GetPEB
    mov  eax, DWORD PTR [eax+0Ch] ;peb32+12 => pointer to PEB_LDR_DATA32
    test eax, eax
    je   @@not_found
    cmp  DWORD PTR [eax+4], 0h ;check PEB_LDR_DATA32.Initialize flag
    je   @@not_found
    mov  esi, eax
    add  esi, 0Ch ;esi has the first link (PEB_LDR_DATA32.InLoadOrderModuleList.Flink)
    mov  ebx, DWORD PTR [esi]
@@loop:
    cmp  ebx, esi
    je   @@not_found
    ;check if this is the entry we are looking for...
    movzx ecx, WORD PTR [ebx].MODULE_ENTRY32.BaseDllName._Length
    test ecx, ecx
    je   @@next
    shr  ecx, 1 ;divide by 2 because they are unicode chars
    mov  eax, DWORD PTR [ebx].MODULE_ENTRY32.BaseDllName.Buffer
    test eax, eax
    je   @@next
    push ecx                  ;push 3rd param (length)
    push DWORD PTR szDllNameW ;push 2nd param (dll name to find)
    push eax                  ;push 1st param (string)
    CALL MyStrNICmpW
    test eax, eax
    je   @@next
    ;got it
    push 0
    push DWORD PTR [ebx].MODULE_ENTRY32.DllBase
    call CheckImageType
    test eax, eax
    je   @@next
    mov  eax, DWORD PTR [ebx].MODULE_ENTRY32.DllBase
    jmp  @@found
@@next:
    mov  ebx, DWORD PTR [ebx].MODULE_ENTRY32.InLoadOrderLinks.Flink ;go to the next entry
    jmp  @@loop
@@not_found:
    xor  eax, eax
@@found:
    ret  ;pop parameters
MxGetModuleBaseAddress ENDP

;---------------------------------------------------------------------------------

PUBLIC MxGetProcedureAddress

;LPVOID MxGetProcedureAddress(LPVOID lpDllBase, LPSTR szFuncNameA)
ALIGN 4
MxGetProcedureAddress PROC STDCALL USES ebx ecx esi edi, lpDllBase:DWORD, szFuncNameA:DWORD
LOCAL _lpNtHdr$ : DWORD
LOCAL _nNamesCount$ : DWORD
LOCAL _lpAddrOfNames$ : DWORD
LOCAL _szTempStrW$[40] : DWORD

@@restart:
    ;check for non-null
    cmp  DWORD PTR szFuncNameA, 0
    je   @@not_found
    ;get module base address and check for null
    mov  ecx, DWORD PTR lpDllBase
    test ecx, ecx
    je   @@not_found
    ;get nt header
    lea  eax, DWORD PTR _lpNtHdr$
    push eax
    push ecx
    call CheckImageType
    test eax, eax
    je   @@not_found
    ;check export data directory
    mov  eax, DWORD PTR _lpNtHdr$
    mov  edi, DWORD PTR [eax].IMAGE_NT_HEADERS32.OptionalHeader.DataDirectory[0]._Size
    cmp  edi, 0
    je   @@not_found
    mov  esi, DWORD PTR [eax].IMAGE_NT_HEADERS32.OptionalHeader.DataDirectory[0].VirtualAddress
    test esi, esi
    je   @@not_found
    add  esi, DWORD PTR lpDllBase
    add  edi, esi
    ;locating an export by ordinal?
    mov  eax, DWORD PTR szFuncNameA
    cmp  BYTE PTR [eax], 23h ; is "#"
    jne  @@findByName
    inc  eax
    push eax
    call SimpleAtolA
    cmp  eax, DWORD PTR [esi].IMAGE_EXPORT_DIRECTORY.Base
    jb   @@not_found
    cmp  eax, DWORD PTR [esi].IMAGE_EXPORT_DIRECTORY.NumberOfFunctions
    jae  @@not_found
    ;get the ordinal
    sub  eax, DWORD PTR [esi].IMAGE_EXPORT_DIRECTORY.Base
    mov  ecx, eax
    jmp  @@getAddress
@@findByName:
    ;get the number of names
    mov  eax, DWORD PTR [esi].IMAGE_EXPORT_DIRECTORY.NumberOfNames
    mov  DWORD PTR _nNamesCount$, eax
    ;get the AddressOfNames
    mov  eax, DWORD PTR [esi].IMAGE_EXPORT_DIRECTORY.AddressOfNames
    add  eax, DWORD PTR lpDllBase
    mov  DWORD PTR _lpAddrOfNames$, eax
    ;main loop
    xor  ecx, ecx
@@loop:
    cmp  ecx, DWORD PTR _nNamesCount$
    jae  @@not_found
    ;get exported name
    push DWORD PTR szFuncNameA
    mov  eax, DWORD PTR _lpAddrOfNames$
    mov  eax, DWORD PTR [eax]
    add  eax, DWORD PTR lpDllBase
    push eax
    call MyStrICmpA
    test eax, eax
    je   @@next
    ;got the function
    mov  eax, DWORD PTR [esi].IMAGE_EXPORT_DIRECTORY.AddressOfNameOrdinals
    add  eax, DWORD PTR lpDllBase
    shl  ecx, 1
    add  eax, ecx
    movzx ecx, WORD PTR [eax] ;get the ordinal of this function
@@getAddress:
    mov  eax, DWORD PTR [esi].IMAGE_EXPORT_DIRECTORY.AddressOfFunctions
    add  eax, DWORD PTR lpDllBase
    shl  ecx, 2
    add  eax, ecx
    ;get the function address
    mov  eax, DWORD PTR [eax]
    add  eax, DWORD PTR lpDllBase
    ;check if inside export table zone
    cmp  eax, esi
    jb   @@found
    cmp  eax, edi
    jae  @@found
    ;got a forward declaration
    lea  ebx, DWORD PTR _szTempStrW$
    xor  ecx, ecx
@@: cmp  ecx, 32
    jae  @@not_found ;dll name is too long
    movzx dx, BYTE PTR [eax+ecx]
    cmp  dl, 2Eh ;a dot?
    je   @F
    mov  WORD PTR [ebx+ecx*2], dx
    inc  ecx
    jmp  @B
@@: ;add ".dll" to string
    mov  WORD PTR [ebx+ecx*2], 2Eh
    mov  WORD PTR [ebx+ecx*2+2], 64h
    mov  WORD PTR [ebx+ecx*2+4], 6Ch
    mov  WORD PTR [ebx+ecx*2+6], 6Ch
    mov  WORD PTR [ebx+ecx*2+8], 0
    add  ecx, eax ;ecx points to the dot
@@: ;skip after the dot (no validity checks are done)
    cmp  BYTE PTR [ecx], 2Eh
    je   @F
    inc  ecx
    jmp  @B
@@: inc  ecx ;skip the dot
    lea  eax, _szTempStrW$
    push eax
    call MxGetModuleBaseAddress
    cmp  eax, 0
    je   @@not_found ;forwarded module was not found
    ;EAX contains the module instance
    ;ECX contains the pointer to the api string (valid until module unload)
    mov  DWORD PTR lpDllBase, eax
    mov  DWORD PTR szFuncNameA, ecx
    jmp  @@restart
@@next:
    add  DWORD PTR _lpAddrOfNames$, 4
    inc  ecx
    jmp  @@loop
@@not_found:
    xor  eax, eax
@@found:
    ret  ;pop parameters
MxGetProcedureAddress ENDP

;---------------------------------------------------------------------------------

PUBLIC MxCallWithSEH0
PUBLIC MxCallStdCallWithSEH1
PUBLIC MxCallStdCallWithSEH2
PUBLIC MxCallStdCallWithSEH3
PUBLIC MxCallCDeclWithSEH1
PUBLIC MxCallCDeclWithSEH2
PUBLIC MxCallCDeclWithSEH3

MxCallWithSEH_SEH PROTO C, :DWORD,:DWORD,:DWORD,:DWORD
.SAFESEH MxCallWithSEH_SEH

MxCallWithSEH_PRE MACRO
.safeseh MxCallWithSEH_SEH
LOCAL seh : MINIMAL_SEH

    ;install SEH
    ASSUME FS:NOTHING
    push fs:[0h]
    pop  seh.PrevLink
    mov  seh.CurrentHandler, OFFSET MxCallWithSEH_SEH
    mov  seh.SafeOffset, OFFSET @@done
    lea  eax, seh
    mov  fs:[0], eax
    mov  seh.PrevEsp, esp
    mov  seh.PrevEbp, ebp
                  ENDM

MxCallWithSEH_POST MACRO
@@done:
    ;uninstall SEH
    push seh.PrevLink
    pop  fs:[0]
    ret
                   ENDM

;--------

ALIGN 4
;SIZE_T __stdcall MxCallWithSEH0(__in LPVOID lpFunc);
MxCallWithSEH0 PROC STDCALL lpFunc:DWORD
    MxCallWithSEH_PRE
    ;do call
    mov  eax, lpFunc
    call eax
    MxCallWithSEH_POST
MxCallWithSEH0 ENDP

ALIGN 4
;SIZE_T __stdcall MxCallStdCallWithSEH1(__in LPVOID lpFunc, __in SIZE_T nParam1);
MxCallStdCallWithSEH1 PROC STDCALL lpFunc:DWORD, nParam1:DWORD
    MxCallWithSEH_PRE
    ;do call
    mov  eax, nParam1
    push eax
    mov  eax, lpFunc
    call eax
    MxCallWithSEH_POST
MxCallStdCallWithSEH1 ENDP

ALIGN 4
;SIZE_T __stdcall MxCallStdCallWithSEH2(__in LPVOID lpFunc, __in SIZE_T nParam1, __in SIZE_T nParam2);
MxCallStdCallWithSEH2 PROC STDCALL lpFunc:DWORD, nParam1:DWORD, nParam2:DWORD
    MxCallWithSEH_PRE
    ;do call
    mov  eax, nParam2
    push eax
    mov  eax, nParam1
    push eax
    mov  eax, lpFunc
    call eax
    MxCallWithSEH_POST
MxCallStdCallWithSEH2 ENDP

ALIGN 4
;SIZE_T __stdcall MxCallStdCallWithSEH3(__in LPVOID lpFunc, __in SIZE_T nParam1, __in SIZE_T nParam2, __in SIZE_T nParam3);
MxCallStdCallWithSEH3 PROC STDCALL lpFunc:DWORD, nParam1:DWORD, nParam2:DWORD, nParam3:DWORD
    MxCallWithSEH_PRE
    ;do call
    mov  eax, nParam3
    push eax
    mov  eax, nParam2
    push eax
    mov  eax, nParam1
    push eax
    mov  eax, lpFunc
    call eax
    MxCallWithSEH_POST
MxCallStdCallWithSEH3 ENDP

ALIGN 4
;SIZE_T __stdcall MxCallCDeclWithSEH1(__in LPVOID lpFunc, __in SIZE_T nParam1);
MxCallCDeclWithSEH1 PROC STDCALL lpFunc:DWORD, nParam1:DWORD
    MxCallWithSEH_PRE
    ;do call
    mov  eax, nParam1
    push eax
    mov  eax, lpFunc
    call eax
    add  esp, 4
    MxCallWithSEH_POST
MxCallCDeclWithSEH1 ENDP

ALIGN 4
;SIZE_T __stdcall MxCallCDeclWithSEH2(__in LPVOID lpFunc, __in SIZE_T nParam1, __in SIZE_T nParam2);
MxCallCDeclWithSEH2 PROC STDCALL lpFunc:DWORD, nParam1:DWORD, nParam2:DWORD
    MxCallWithSEH_PRE
    ;do call
    mov  eax, nParam1
    push eax
    mov  eax, nParam2
    push eax
    mov  eax, lpFunc
    call eax
    add  esp, 8
    MxCallWithSEH_POST
MxCallCDeclWithSEH2 ENDP

ALIGN 4
;SIZE_T __stdcall MxCallCDeclWithSEH3(__in LPVOID lpFunc, __in SIZE_T nParam1, __in SIZE_T nParam2, __in SIZE_T nParam3);
MxCallCDeclWithSEH3 PROC STDCALL lpFunc:DWORD, nParam1:DWORD, nParam2:DWORD, nParam3:DWORD
    MxCallWithSEH_PRE
    ;do call
    mov  eax, nParam1
    push eax
    mov  eax, nParam2
    push eax
    mov  eax, nParam3
    push eax
    mov  eax, lpFunc
    call eax
    add  esp, 12
    MxCallWithSEH_POST
MxCallCDeclWithSEH3 ENDP

MxCallWithSEH_SEH PROC C USES ecx edx, pExcept:DWORD, pFrame:DWORD, pContext:DWORD, pDispatch:DWORD
    mov  edx, pExcept
    test DWORD PTR [edx+4], 2         ;edx.ExceptionFlags == EXCEPTION_UNWINDING?
    je   @F
    mov  eax, 1
    jmp  done
@@: mov  edx, pFrame
    ASSUME EDX:ptr MINIMAL_SEH
    mov  ecx, pContext
    mov  eax, [edx].SafeOffset
    mov  [ecx+0B8h], eax  ;ecx.CONTEXT.Eip
    mov  eax, [edx].PrevEsp
    mov  [ecx+0C4h], eax  ;ecx.CONTEXT.Esp
    mov  eax, [edx].PrevEbp
    mov  [ecx+0B4h], eax  ;ecx.CONTEXT.Ebp
    xor  eax, eax
    mov  [ecx+0B0h], eax  ;ecx.CONTEXT.Eax
    ;eax = 0 & ExceptionContinueExecution = 0 so leave eax untouched and return
done:
    ret
MxCallWithSEH_SEH ENDP

;---------------------------------------------------------------------------------

DECLARENTAPI_FORINIT MACRO apiName:REQ

    lea  eax, DWORD PTR [_&apiName&_Name]
    push eax
    push DWORD PTR [_hNtDll$]
    call MxGetProcedureAddress
    xchg DWORD PTR [_&apiName&_Proc], eax

                     ENDM

.data
ALIGN 4
_szNtDllW DB 'n', 0, 't', 0, 'd', 0, 'l', 0, 'l', 0, '.', 0, 'd', 0, 'l', 0, 'l', 0, 0, 0

.code
ALIGN 4
InitializeNtApis PROC C USES edx
LOCAL _hNtDll$ : DWORD

    lea  eax, DWORD PTR [_szNtDllW]
    push eax
    call MxGetModuleBaseAddress
    cmp  eax, 0
    je   @@afterGetProc
    mov  DWORD PTR [_hNtDll$], eax

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

@@afterGetProc:
    mov  edx, 1
    mov  eax, DWORD PTR [_apisInitialized]
@@: nop
    lock cmpxchg DWORD PTR [_apisInitialized], edx
    jne  @B
    ret  ;pop parameters
InitializeNtApis ENDP

END
