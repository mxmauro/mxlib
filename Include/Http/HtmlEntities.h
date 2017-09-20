/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2015. All rights reserved.
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
#ifndef _MX_HTMLENTITIES_H
#define _MX_HTMLENTITIES_H

#include "..\Defines.h"
#include "..\Strings\Strings.h"

//-----------------------------------------------------------

namespace MX {

namespace HtmlEntities {

LPCSTR Get(__in WCHAR chW);

HRESULT ConvertTo(__inout CStringA &cStrA);
HRESULT ConvertTo(__inout CStringW &cStrW);
HRESULT ConvertFrom(__inout CStringA &cStrA);
HRESULT ConvertFrom(__inout CStringW &cStrW);

WCHAR Decode(__in LPCSTR szStrA, __in SIZE_T nStrLen=(SIZE_T)-1, __out_opt LPCSTR *lpszAfterEntityA=NULL);
WCHAR Decode(__in LPCWSTR szStrW, __in SIZE_T nStrLen=(SIZE_T)-1, __out_opt LPCWSTR *lpszAfterEntityW=NULL);

} //namespace HtmlEntities

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTMLENTITIES_H
