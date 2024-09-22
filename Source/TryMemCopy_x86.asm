; Licensed to the Apache Software Foundation (ASF) under one or more
; contributor license agreements.  See the LICENSE file distributed with
; this work for additional information regarding copyright ownership.
;
; Also, if exists, check the Licenses directory for information about
; third-party modules.
;
; The ASF licenses this file to You under the Apache License, Version 2.0
; (the "License"); you may not use this file except in compliance with
; the License.  You may obtain a copy of the License at
;
;     http://www.apache.org/licenses/LICENSE-2.0
;
; Unless required by applicable law or agreed to in writing, software
; distributed under the License is distributed on an "AS IS" BASIS,
; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; See the License for the specific language governing permissions and
; limitations under the License.
;
.386
.model flat, stdcall
.code

_TEXT SEGMENT

;---------------------------------------------------------------------------------

PUBLIC MxTryMemCopy

MxTryMemCopy_SEH PROTO C, :DWORD,:DWORD,:DWORD,:DWORD
.SAFESEH MxTryMemCopy_SEH

ALIGN 4
MxTryMemCopy PROC STDCALL USES ecx esi edi, lpDest:DWORD, lpSrc:DWORD, nCount:DWORD
ASSUME FS:NOTHING

		mov  ecx, DWORD PTR nCount ;copy original count
		mov  esi, DWORD PTR lpSrc
		mov  edi, DWORD PTR lpDest

		push offset MxTryMemCopy_SEH
		push fs:[0h]
		mov  fs:[0h], esp

		test edi, 3
		jne  @slow
		test esi, 3
		jne  @slow

@@: cmp  ecx, 4
		jb   @slow
		mov  eax, DWORD PTR [esi]
		add  esi, 4
		mov  DWORD PTR [edi], eax
		add  edi, 4
		sub  ecx, 4
		jmp  @B

@slow:
		test ecx, ecx
		je   MxTryMemCopy_Finalize
		mov  al, BYTE PTR [esi]
		inc  esi
		mov  BYTE PTR [edi], al
		inc  edi
		dec  ecx
		jmp  @slow

MxTryMemCopy_Finalize::
		pop  fs:[0h]
		pop  eax

		mov  eax, DWORD PTR [ebp+10h]
		sub  eax, ecx
		ret
ASSUME FS:ERROR
MxTryMemCopy ENDP

MxTryMemCopy_SEH PROC C USES ecx, pExcept:DWORD, pFrame:DWORD, pContext:DWORD, pDispatch:DWORD
		mov  ecx, pContext
		lea  eax, OFFSET MxTryMemCopy_Finalize
		mov  DWORD PTR [ecx+0B8h], eax ;CONTEXT.Eip
		xor  eax, eax ;ExceptionContinueExecution
		ret
MxTryMemCopy_SEH ENDP

;---------------------------------------------------------------------------------

_TEXT ENDS

END
