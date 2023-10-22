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
#ifndef _MX_PROXY_H
#define _MX_PROXY_H

#include "..\Defines.h"
#include "..\Strings\Strings.h"
#include "..\Http\Url.h"

//-----------------------------------------------------------

namespace MX {

class CProxy : public CBaseMemObj
{
public:
  enum class eType
  {
    None = 0,
    UseIE,
    Manual
  };

public:
  CProxy();
  CProxy(_In_ const CProxy& cSrc) throw(...);
  ~CProxy();

  CProxy& operator=(_In_ const CProxy& cSrc) throw(...);

  VOID SetDirect();
  HRESULT SetManual(_In_z_ LPCWSTR szProxyServerW);
  VOID SetUseIE();

  HRESULT SetCredentials(_In_opt_z_ LPCWSTR szUserNameW, _In_opt_z_ LPCWSTR szPasswordW);

  HRESULT Resolve(_In_opt_z_ LPCWSTR szTargetUrlW);
  HRESULT Resolve(_In_ CUrl &cUrl);

  eType GetType() const
    {
    return nType;
    };

  LPCWSTR GetAddress() const
    {
    return (LPCWSTR)cStrAddressW;
    };
  int GetPort() const
    {
    return nPort;
    };

  LPCWSTR _GetUserName() const
    {
    return (LPCWSTR)cStrUserNameW;
    };

  LPCWSTR GetUserPassword() const
    {
    return (LPCWSTR)cStrUserPasswordW;
    };

private:
  eType nType;
  MX::CStringW cStrAddressW;
  MX::CSecureStringW cStrUserNameW, cStrUserPasswordW;
  int nPort;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_PROXY_H
