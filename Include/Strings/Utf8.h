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
#ifndef _MX_STRINGS_UTF8_H
#define _MX_STRINGS_UTF8_H

#include "..\Defines.h"
#include "Strings.h"

//-----------------------------------------------------------

namespace MX {

HRESULT Utf8_Encode(__inout CStringA &cStrDestA, __in_z LPCWSTR szSrcW, __in_opt SIZE_T nSrcLen=(SIZE_T)-1,
                    __in_opt BOOL bAppend=FALSE);
HRESULT Utf8_Decode(__inout CStringW &cStrDestW, __in_z LPCSTR szSrcA, __in_opt SIZE_T nSrcLen=(SIZE_T)-1,
                    __in_opt BOOL bAppend=FALSE);

int Utf8_EncodeChar(__out LPSTR szDestA, __in WCHAR chW, __in_opt WCHAR chSurrogatePairW=0);
int Utf8_DecodeChar(__out LPWSTR szDestW, __in_z LPCSTR szSrcA, __in_opt SIZE_T nSrcLen=(SIZE_T)-1);

SIZE_T Utf8_StrLen(__in_z LPCSTR szSrcA);

int Utf8_StrCompareA(__in_z LPCSTR szSrcUtf8, __in_z LPCSTR szSrcA,__in_opt BOOL bCaseInsensitive=FALSE);
int Utf8_StrCompareW(__in_z LPCSTR szSrcUtf8, __in_z LPCWSTR szSrcW, __in_opt BOOL bCaseInsensitive=FALSE);
int Utf8_StrNCompareA(__in_z LPCSTR szSrcUtf8, __in_z LPCSTR szSrcA, __in SIZE_T nLen,
                     __in_opt BOOL bCaseInsensitive=FALSE);
int Utf8_StrNCompareW(__in_z LPCSTR szSrcUtf8, __in_z LPCWSTR szSrcW, __in SIZE_T nLen,
                      __in_opt BOOL bCaseInsensitive=FALSE);

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_STRINGS_UTF8_H
