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
