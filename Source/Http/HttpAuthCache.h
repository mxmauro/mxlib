/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2019. All rights reserved.
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
#ifndef _MX_HTTPAUTHCACHE_H
#define _MX_HTTPAUTHCACHE_H

#include "..\..\Include\Defines.h"
#include "..\..\Include\Http\HttpAuth.h"
#include "..\..\Include\RefCounted.h"

//-----------------------------------------------------------

namespace MX {

namespace HttpAuthCache {

HRESULT Add(_In_ CUrl &cUrl, _In_ CHttpBaseAuth *lpHttpAuth);
HRESULT Add(_In_ CUrl::eScheme nScheme, _In_z_ LPCWSTR szHostW, _In_ int nPort, _In_z_ LPCWSTR szPathW,
            _In_ CHttpBaseAuth *lpHttpAuth);

VOID Remove(_In_ CHttpBaseAuth *lpHttpAuth);
VOID RemoveAll();

CHttpBaseAuth* Lookup(_In_ CUrl &cUrl);
CHttpBaseAuth* Lookup(_In_ CUrl::eScheme nScheme, _In_z_ LPCWSTR szHostW, _In_ int nPort, _In_z_ LPCWSTR szPathW);

} //namespace HttpAuthCache

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPAUTHCACHE_H