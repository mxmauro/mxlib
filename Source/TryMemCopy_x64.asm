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
_TEXT SEGMENT

;---------------------------------------------------------------------------------

PUBLIC MxTryMemCopy

MxTryMemCopy_SEH PROTO

ALIGN 16
;SIZE_T __stdcall MxTryMemCopy(_Out_writes_to_(nCount) LPVOID lpDest, _In_ LPVOID lpSrc, _In_ SIZE_T nCount);
MxTryMemCopy PROC FRAME :MxTryMemCopy_SEH
		sub  rsp, 28h+20h
.allocstack 28h+20h
.endprolog

		mov  r9, r8
MxTryMemCopy_GuardedRegionStart::
		test rcx, 7
		jne  @slow
		test rdx, 7 ;source aligned?
		jne  @slow

@@: cmp  r8, 8
		jb   @slow
		mov  rax, QWORD PTR [rdx]
		add  rdx, 8
		mov  QWORD PTR [rcx], rax
		add  rcx, 8
		sub  r8, 8
		jmp  @B

@slow:
		test r8, r8
		je   MxTryMemCopy_GuardedRegionEnd
		mov  al, BYTE PTR [rdx]
		inc  rdx
		mov  BYTE PTR [rcx], al
		inc  rcx
		dec  r8
		jmp  @slow

MxTryMemCopy_GuardedRegionEnd::
		mov  rax, r9
		sub  rax, r8
		add  rsp, 28h+20h
		ret
MxTryMemCopy ENDP

ALIGN 16
MxTryMemCopy_SEH PROC
		mov  rdx, 1 ;ExceptionContinueSearch
		lea  rax, MxTryMemCopy_GuardedRegionStart
		cmp  [rcx+10h], rax ;EXCEPTION_RECORD.ExceptionAddress
		jb   @F
		lea  rax, MxTryMemCopy_GuardedRegionEnd
		cmp  [rcx+10h], rax ;EXCEPTION_RECORD.ExceptionAddress
		jae  @F
		mov  [r8+0F8h], rax ;CONTEXT.Rip (resume point == guarded region end)
		xor  rdx, rdx ;ExceptionContinueExecution
@@: mov  rax, rdx
		ret
MxTryMemCopy_SEH ENDP

;---------------------------------------------------------------------------------

_TEXT ENDS

END
