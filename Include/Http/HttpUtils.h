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
#ifndef _MX_HTTPUTILS_H
#define _MX_HTTPUTILS_H

#include "..\Defines.h"
#include "..\Strings\Strings.h"
#include "..\DateTime\DateTime.h"

//-----------------------------------------------------------

namespace MX {

namespace Http {

enum class eBrowser
{
  Other, IE, IE6, Opera, Gecko, Chrome, Safari, Konqueror
};

} //namespace Http

} //namespace MX

//-----------------------------------------------------------

namespace MX {

namespace Http {

eBrowser GetBrowserFromUserAgent(_In_ LPCSTR szUserAgentA, _In_opt_ SIZE_T nUserAgentLen = (SIZE_T)-1);

LPCSTR IsValidVerb(_In_ LPCSTR szStrA, _In_ SIZE_T nStrLen);

BOOL IsValidNameChar(_In_ CHAR chA);

BOOL FromISO_8859_1(_Out_ CStringW &cStrDestW, _In_ LPCSTR szStrA, _In_ SIZE_T nStrLen);
BOOL ToISO_8859_1(_Out_ CStringA &cStrDestA, _In_ LPCWSTR szStrW, _In_ SIZE_T nStrLen);

BOOL BuildQuotedString(_Out_ CStringA &cStrA, _In_ LPCSTR szStrA, _In_ SIZE_T nStrLen, _In_ BOOL bDontQuoteIfToken);
BOOL BuildQuotedString(_Out_ CStringA &cStrA, _In_ LPCWSTR szStrW, _In_ SIZE_T nStrLen, _In_ BOOL bDontQuoteIfToken);
BOOL BuildExtendedValueString(_Out_ CStringA &cStrA, _In_ LPCWSTR szStrW, _In_ SIZE_T nStrLen);

LPCSTR GetMimeType(_In_z_ LPCSTR szFileNameA);
LPCSTR GetMimeType(_In_z_ LPCWSTR szFileNameW);

HRESULT ParseDate(_Out_ CDateTime &cDt, _In_z_ LPCSTR szDateTimeA, _In_opt_ SIZE_T nDateTimeLen = (SIZE_T)-1);
HRESULT ParseDate(_Out_ CDateTime &cDt, _In_z_ LPCWSTR szDateTimeW, _In_opt_ SIZE_T nDateTimeLen = (SIZE_T)-1);

} //namespace Http

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPUTILS_H
