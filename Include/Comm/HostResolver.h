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
#ifndef _MX_HOSTRESOLVER_H
#define _MX_HOSTRESOLVER_H

#include "..\Defines.h"
#include "..\Callbacks.h"
#if (!defined(_WS2DEF_)) && (!defined(_WINSOCKAPI_))
  #include <WS2tcpip.h>
#endif //!_WS2DEF_ && !_WINSOCKAPI_

//-----------------------------------------------------------

namespace MX {

namespace HostResolver {

typedef Callback <VOID (_In_ LONG nResolverId, _In_ PSOCKADDR_INET lpSockAddr, _In_ HRESULT hrErrorCode,
                        _In_ LPVOID lpUserData)> OnResultCallback;

//-----------------------------------------------------------

HRESULT Resolve(_In_z_ LPCSTR szHostNameA, _In_ int nDesiredFamily, _Out_ PSOCKADDR_INET lpSockAddr,
                _In_ DWORD dwTimeoutMs, _In_opt_ OnResultCallback cCallback = NullCallback(),
                _In_opt_ LPVOID lpUserData = NULL, _Out_opt_ LONG volatile *lpnResolverId = NULL);
HRESULT Resolve(_In_z_ LPCWSTR szHostNameW, _In_ int nDesiredFamily, _Out_ PSOCKADDR_INET lpSockAddr,
                _In_ DWORD dwTimeoutMs, _In_opt_ OnResultCallback cCallback = NullCallback(),
                _In_opt_ LPVOID lpUserData = NULL, _Out_opt_ LONG volatile *lpnResolverId = NULL);
VOID Cancel(_Inout_ LONG volatile *lpnResolverId);

BOOL IsValidIPV4(_In_z_ LPCSTR szAddressA, _In_opt_ SIZE_T nAddressLen = (SIZE_T)-1,
                 _Out_opt_ PSOCKADDR_INET lpAddress = NULL);
BOOL IsValidIPV4(_In_z_ LPCWSTR szAddressW, _In_opt_ SIZE_T nAddressLen = (SIZE_T)-1,
                 _Out_opt_ PSOCKADDR_INET lpAddress = NULL);
BOOL IsValidIPV6(_In_z_ LPCSTR szAddressA, _In_opt_ SIZE_T nAddressLen = (SIZE_T)-1,
                 _Out_opt_ PSOCKADDR_INET lpAddress = NULL);
BOOL IsValidIPV6(_In_z_ LPCWSTR szAddressW, _In_opt_ SIZE_T nAddressLen = (SIZE_T)-1,
                 _Out_opt_ PSOCKADDR_INET lpAddress = NULL);

} //namespace HostResolver

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HOSTRESOLVER_H
