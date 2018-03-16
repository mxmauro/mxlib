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
