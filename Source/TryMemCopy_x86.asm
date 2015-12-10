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
