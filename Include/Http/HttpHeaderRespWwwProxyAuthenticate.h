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
#ifndef _MX_HTTPHEADERRESPONSEWWWPROXYAUTHENTICATE_H
#define _MX_HTTPHEADERRESPONSEWWWPROXYAUTHENTICATE_H

#include "HttpHeaderBase.h"

//-----------------------------------------------------------

namespace MX {

class MX_NOVTABLE CHttpHeaderRespWwwProxyAuthenticateCommon : public CHttpHeaderBase
{
protected:
  CHttpHeaderRespWwwProxyAuthenticateCommon();
public:
  ~CHttpHeaderRespWwwProxyAuthenticateCommon();

  HRESULT Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser);

  HRESULT SetScheme(_In_z_ LPCSTR szSchemeA, _In_opt_ SIZE_T nSchemeLen = (SIZE_T)-1);
  LPCSTR GetScheme() const;

  HRESULT AddParam(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW);

  SIZE_T GetParamsCount() const;
  LPCSTR GetParamName(_In_ SIZE_T nIndex) const;
  LPCWSTR GetParamValue(_In_ SIZE_T nIndex) const;
  LPCWSTR GetParamValue(_In_z_ LPCSTR szNameA) const;

private:
  typedef struct {
    LPWSTR szValueW;
    CHAR szNameA[1];
  } PARAMETER, *LPPARAMETER;

  CStringA cStrSchemeA;
  TArrayListWithFree<LPPARAMETER> aParamsList;
};

//--------

class CHttpHeaderRespWwwAuthenticate : public CHttpHeaderRespWwwProxyAuthenticateCommon
{
public:
  CHttpHeaderRespWwwAuthenticate();
  ~CHttpHeaderRespWwwAuthenticate();

  MX_DECLARE_HTTPHEADER_NAME(WWW-Authenticate)
};

//--------

class CHttpHeaderRespProxyAuthenticate : public CHttpHeaderRespWwwProxyAuthenticateCommon
{
public:
  CHttpHeaderRespProxyAuthenticate();
  ~CHttpHeaderRespProxyAuthenticate();

  MX_DECLARE_HTTPHEADER_NAME(Proxy-Authenticate)
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERRESPONSEWWWPROXYAUTHENTICATE_H
