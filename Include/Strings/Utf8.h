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

HRESULT Utf8_Encode(_Inout_ CStringA &cStrDestA, _In_z_ LPCWSTR szSrcW, _In_opt_ SIZE_T nSrcLen=(SIZE_T)-1,
                    _In_opt_ BOOL bAppend=FALSE);
HRESULT Utf8_Decode(_Inout_ CStringW &cStrDestW, _In_z_ LPCSTR szSrcA, _In_opt_ SIZE_T nSrcLen=(SIZE_T)-1,
                    _In_opt_ BOOL bAppend=FALSE);

int Utf8_EncodeChar(_Out_opt_ CHAR szDestA[], _In_ WCHAR chW, _In_opt_ WCHAR chSurrogatePairW=0);
int Utf8_DecodeChar(_Out_opt_ WCHAR szDestW[], _In_z_ LPCSTR szSrcA, _In_opt_ SIZE_T nSrcLen=(SIZE_T)-1);

SIZE_T Utf8_StrLen(_In_z_ LPCSTR szSrcA);

int Utf8_StrCompareA(_In_z_ LPCSTR szSrcUtf8, _In_z_ LPCSTR szSrcA,_In_opt_ BOOL bCaseInsensitive=FALSE);
int Utf8_StrCompareW(_In_z_ LPCSTR szSrcUtf8, _In_z_ LPCWSTR szSrcW, _In_opt_ BOOL bCaseInsensitive=FALSE);
int Utf8_StrNCompareA(_In_z_ LPCSTR szSrcUtf8, _In_z_ LPCSTR szSrcA, _In_ SIZE_T nLen,
                     _In_opt_ BOOL bCaseInsensitive=FALSE);
int Utf8_StrNCompareW(_In_z_ LPCSTR szSrcUtf8, _In_z_ LPCWSTR szSrcW, _In_ SIZE_T nLen,
                      _In_opt_ BOOL bCaseInsensitive=FALSE);

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_STRINGS_UTF8_H
