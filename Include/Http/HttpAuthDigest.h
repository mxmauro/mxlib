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
#ifndef _MX_HTTPAUTHDIGEST_H
#define _MX_HTTPAUTHDIGEST_H

#include "HttpAuthBase.h"

//-----------------------------------------------------------

namespace MX {

class CHttpAuthDigest : public CHttpAuthBase, public CNonCopyableObj
{
public:
  CHttpAuthDigest();
  ~CHttpAuthDigest();

  LPCSTR GetScheme() const
    {
    return "Digest";
    };

  HRESULT Parse(_In_ CHttpHeaderRespWwwProxyAuthenticateCommon *lpHeader);

  HRESULT GenerateResponse(_Out_ CStringA &cStrDestA, _In_z_ LPCWSTR szUserNameW, _In_z_ LPCWSTR szPasswordW,
                           _In_z_ LPCSTR szMethodA, _In_z_ LPCSTR szUriPathA);

  LPCWSTR GetRealm() const
    {
    return (LPCWSTR)cStrRealmW;
    };

  LPCWSTR GetDomain() const
    {
    return (LPCWSTR)cStrDomainW;
    };

  LPCWSTR GetNonce() const
    {
    return (LPCWSTR)cStrNonceW;
    };

  LPCWSTR GetOpaque() const
    {
    return (LPCWSTR)cStrOpaqueW;
    };

  BOOL IsStale() const
    {
    return bStale;
    };

  BOOL IsAlgorithmSession() const
    {
    return bAlgorithmSession;
    };

  BOOL IsUserHash() const
    {
    return bUserHash;
    };

  int GetAlgorithm() const
    {
    return nAlgorithm;
    };

  int GetQ() const
    {
    return nQop;
    };

  BOOL IsCharsetUTF8() const
    {
    return bCharsetIsUtf8;
    };

private:
  CStringW cStrRealmW, cStrDomainW, cStrNonceW, cStrOpaqueW;
  BOOL bStale, bAlgorithmSession, bUserHash;
  int nAlgorithm, nQop;
  BOOL bCharsetIsUtf8;
  LONG volatile nNonceCount;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPAUTHDIGEST_H
