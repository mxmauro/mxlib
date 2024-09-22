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
#ifndef _MX_HTTPAUTHBEARER_H
#define _MX_HTTPAUTHBEARER_H

#include "HttpAuthBase.h"

//-----------------------------------------------------------

namespace MX {

class CHttpAuthBearer : public CHttpAuthBase, public CNonCopyableObj
{
public:
  CHttpAuthBearer();
  ~CHttpAuthBearer();

  LPCSTR GetScheme() const
    {
    return "Bearer";
    };

  HRESULT Parse(_In_ CHttpHeaderRespWwwProxyAuthenticateCommon *lpHeader);

  HRESULT GenerateResponse(_Out_ CStringA &cStrDestA, _In_z_ LPCSTR szAccessTokenA);

  LPCWSTR GetRealm() const
    {
    return (LPCWSTR)cStrRealmW;
    };

  SIZE_T GetScopesCount() const
    {
    return aScopesList.GetCount();
    };

  LPCWSTR GetScope(_In_ SIZE_T nIndex) const
    {
    return (nIndex < aScopesList.GetCount()) ? aScopesList.GetElementAt(nIndex) : NULL;
    };

  LPCWSTR GetError() const
    {
    return (LPCWSTR)cStrErrorW;
    };

  LPCWSTR GetErrorDescription() const
    {
    return (LPCWSTR)cStrErrorDescriptionW;
    };

private:
  CStringW cStrRealmW;
  TArrayListWithFree<LPCWSTR> aScopesList;
  CStringW cStrErrorW, cStrErrorDescriptionW;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPAUTHBEARER_H
