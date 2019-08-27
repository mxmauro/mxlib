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
#ifndef _MX_HTTPHEADERGENERIC_H
#define _MX_HTTPHEADERGENERIC_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

//This generic class can be used for the following generic headers:
//
//    "User-Agent", "Proxy-Authorization", "From", "Max-Forwards", "Content-Location", "Content-MD5",
//    "Pragma", "Trailer", "Via", "Warning", "Server", "Vary", "WWW-Authenticate"
//    or any other unparsed header
class CHttpHeaderGeneric : public CHttpHeaderBase
{
public:
  CHttpHeaderGeneric();
  ~CHttpHeaderGeneric();

  HRESULT SetHeaderName(_In_z_ LPCSTR szNameA);
  LPCSTR GetHeaderName() const;

  HRESULT Parse(_In_z_ LPCSTR szValueA);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser);

  eDuplicateBehavior GetDuplicateBehavior() const
    {
    return DuplicateBehaviorAdd;
    };

  HRESULT SetValue(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1);
  LPCSTR GetValue() const;

private:
  CStringA cStrHeaderNameA;
  CStringA cStrValueA;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERGENERIC_H
