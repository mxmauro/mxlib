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
#ifndef _MX_HTTPAUTH_H
#define _MX_HTTPAUTH_H

#include "..\Defines.h"
#include "..\Strings\Strings.h"
#include "..\Strings\Utf8.h"
#include "..\RefCounted.h"
#include "Url.h"

//-----------------------------------------------------------

namespace MX {

class MX_NOVTABLE CHttpAuthBase : public TRefCounted<CBaseMemObj>
{
  MX_DISABLE_COPY_CONSTRUCTOR(CHttpAuthBase);
public:
  typedef enum {
    TypeBasic=1,
    TypeDigest
  } Type;

protected:
  CHttpAuthBase() : TRefCounted<CBaseMemObj>()
    {
    return;
    };

public:
  ~CHttpAuthBase()
    { }

  SIZE_T GetId()
    {
    return (SIZE_T)this;
    };

  virtual Type GetType() const = 0;

  virtual HRESULT Parse(_In_z_ LPCSTR szValueA) = 0;

  virtual BOOL IsReauthenticateRequst() const = 0;

  virtual BOOL IsSameThan(_In_ CHttpAuthBase *lpCompareTo) = 0;
};

//--------

class CHttpAuthBasic : public CHttpAuthBase
{
  MX_DISABLE_COPY_CONSTRUCTOR(CHttpAuthBasic);
public:
  CHttpAuthBasic();
  ~CHttpAuthBasic();

  Type GetType() const
    {
    return TypeBasic;
    };

  HRESULT Parse(_In_z_ LPCSTR szValueA);

  HRESULT MakeAuthenticateResponse(_Out_ CStringA &cStrDestA, _In_z_ LPCWSTR szUserNameW, _In_z_ LPCWSTR szPasswordW,
                                   _In_ BOOL bIsProxy);

  BOOL IsReauthenticateRequst() const
    {
    return FALSE;
    };

  BOOL IsSameThan(_In_ CHttpAuthBase *lpCompareTo);

private:
  CStringW cStrRealmW;
  BOOL bCharsetIsUtf8;
};

//--------

class CHttpAuthDigest : public CHttpAuthBase
{
  MX_DISABLE_COPY_CONSTRUCTOR(CHttpAuthDigest);
public:
  CHttpAuthDigest();
  ~CHttpAuthDigest();

  Type GetType() const
    {
    return TypeDigest;
    };

  HRESULT Parse(_In_z_ LPCSTR szValueA);

  HRESULT MakeAuthenticateResponse(_Out_ CStringA &cStrDestA, _In_z_ LPCWSTR szUserNameW, _In_z_ LPCWSTR szPasswordW,
                                   _In_z_ LPCSTR szMethodA, _In_z_ LPCSTR szUriPathA, _In_ BOOL bIsProxy);

  BOOL IsReauthenticateRequst() const
    {
    return bStale;
    };

  BOOL IsSameThan(_In_ CHttpAuthBase *lpCompareTo);

private:
  CStringW cStrRealmW, cStrDomainW;
  CStringA cStrNonceA, cStrOpaqueA;
  BOOL bStale, bAlgorithmSession, bUserHash;
  int nAlgorithm, nQop;
  BOOL bCharsetIsUtf8;
  LONG volatile nNonceCount;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPAUTH_H
