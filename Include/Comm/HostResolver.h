/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the LICENSE file distributed with
 * this work for additional information regarding copyright ownership.
 *
 * Also, if exists, check the Licenses directory for information about
 * third-party modules.
 *
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _MX_HOSTRESOLVER_H
#define _MX_HOSTRESOLVER_H

#include "..\Defines.h"
#include "..\Callbacks.h"
#if (!defined(_WS2DEF_)) && (!defined(_WINSOCKAPI_))
  #include <WS2tcpip.h>
#endif //!_WS2DEF_ && !_WINSOCKAPI_
#include "..\Strings\Strings.h"

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

HRESULT FormatAddress(_In_ PSOCKADDR_INET lpAddress, _Out_ CStringA &cStrDestA);
HRESULT FormatAddress(_In_ PSOCKADDR_INET lpAddress, _Out_ CStringW &cStrDestW);

} //namespace HostResolver

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HOSTRESOLVER_H
