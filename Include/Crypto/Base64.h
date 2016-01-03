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
#ifndef _MX_BASE64_H
#define _MX_BASE64_H

#include "..\Defines.h"
#include "..\Strings\Strings.h"

//-----------------------------------------------------------

namespace MX {

SIZE_T Base64GetEncodedLength(__in SIZE_T nDataLen);

//NOTE: szDestA is NOT nul-terminated
SIZE_T Base64Encode(__out LPSTR szDestA, __in LPVOID lpData, __in SIZE_T nDataLen);
SIZE_T Base64Encode(__inout CStringA &cStrDestA, __in LPVOID lpData, __in SIZE_T nDataLen);

SIZE_T Base64GetMaxDecodedLength(__in SIZE_T nDataLen);

SIZE_T Base64Decode(__out LPVOID lpDest, __in LPCSTR szStrA, __in_opt SIZE_T nSrcLen=(SIZE_T)-1);

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_BASE64_H
