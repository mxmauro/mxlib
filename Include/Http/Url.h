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
#ifndef _MX_URL_H
#define _MX_URL_H

#include "..\Defines.h"
#include "..\Strings\Strings.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CUrl : public virtual CBaseMemObj
{
public:
  enum class eScheme
  {
    Unknown=-1, None=0,
    MailTo, News, Http, Https, Ftp,
    File, Resource, Nntp, Gopher, WebSocket, SecureWebSocket
  };

  enum class eToStringFlags : int
  {
    AddScheme=0x01, AddUserInfo=0x02, AddHostPort=0x04,
    AddPath=0x08, AddQueryStrings=0x10, AddFragment=0x20,
    ShrinkPath=0x40,
    AddAll=0x7F,
    DontAddHostPortIfDefault=0x10000000
  };




public:
  CUrl();
  CUrl(_In_ const CUrl& cSrc) throw(...);
  ~CUrl();

  CUrl& operator=(_In_ const CUrl& cSrc) throw(...);
  CUrl& operator+=(_In_ const CUrl& cOtherUrl) throw(...);

  VOID Reset();

  HRESULT SetScheme(_In_opt_z_ LPCSTR szSchemeA);
  HRESULT SetScheme(_In_opt_z_ LPCWSTR szSchemeW);
  LPCWSTR GetScheme() const;
  HRESULT GetScheme(_Inout_ CStringA &cStrDestA);
  eScheme GetSchemeCode() const;

  HRESULT SetHost(_In_z_ LPCSTR szHostA, _In_opt_ SIZE_T nHostLen=(SIZE_T)-1);
  HRESULT SetHost(_In_z_ LPCWSTR szHostW, _In_opt_ SIZE_T nHostLen=(SIZE_T)-1);
  LPCWSTR GetHost() const;
  HRESULT GetHost(_Inout_ CStringA &cStrDestA);

  HRESULT SetPort(_In_ int nPort=-1);
  int GetPort() const;

  HRESULT SetPath(_In_z_ LPCSTR szPathA, _In_opt_ SIZE_T nPathLen=(SIZE_T)-1);
  HRESULT SetPath(_In_z_ LPCWSTR szPathW, _In_opt_ SIZE_T nPathLen=(SIZE_T)-1);
  LPCWSTR GetPath() const;

  HRESULT ShrinkPath();

  VOID ResetQueryStrings();
  HRESULT AddQueryString(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen=(SIZE_T)-1,
                         _In_opt_ SIZE_T nNameLen=(SIZE_T)-1);
  HRESULT AddQueryString(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szValueW, _In_opt_ SIZE_T nValueLen=(SIZE_T)-1,
                         _In_opt_ SIZE_T nNameLen=(SIZE_T)-1);
  HRESULT RemoveQueryString(_In_ SIZE_T nIndex);
  SIZE_T GetQueryStringCount() const;

  LPCWSTR GetQueryStringName(_In_ SIZE_T nIndex) const;
  LPCWSTR GetQueryStringValue(_In_ SIZE_T nIndex) const;
  LPCWSTR GetQueryStringValue(_In_z_ LPCWSTR szNameW) const;

  HRESULT SetFragment(_In_z_ LPCSTR szFragmentA, _In_opt_ SIZE_T nFragmentLen=(SIZE_T)-1);
  HRESULT SetFragment(_In_z_ LPCWSTR szFragmentW, _In_opt_ SIZE_T nFragmentLen=(SIZE_T)-1);
  LPCWSTR GetFragment() const;

  HRESULT SetUserInfo(_In_z_ LPCSTR szUserInfoA, _In_opt_ SIZE_T nUserInfoLen=(SIZE_T)-1);
  HRESULT SetUserInfo(_In_z_ LPCWSTR szUserInfoW, _In_opt_ SIZE_T nUserInfoLen=(SIZE_T)-1);
  LPCWSTR GetUserInfo() const;

  HRESULT ToString(_Inout_ CStringA &cStrDestA, _In_ eToStringFlags nFlags=eToStringFlags::AddAll);
  HRESULT ToString(_Inout_ CStringW &cStrDestW, _In_ eToStringFlags nFlags=eToStringFlags::AddAll);

  HRESULT ParseFromString(_In_z_ LPCSTR szUrlA, _In_opt_ SIZE_T nSrcLen=(SIZE_T)-1);
  HRESULT ParseFromString(_In_z_ LPCWSTR szUrlW, _In_opt_ SIZE_T nSrcLen=(SIZE_T)-1);

  HRESULT Merge(_In_ const CUrl& cOtherUrl);

  static HRESULT Encode(_Inout_ CStringA &cStrDestA, _In_z_ LPCSTR szUrlA, _In_opt_ SIZE_T nUrlLen=(SIZE_T)-1,
                        _In_opt_z_ LPCSTR szAllowedCharsA=NULL, _In_opt_ BOOL bAppend=FALSE);
  static SIZE_T GetEncodedLength(_In_z_ LPCSTR szUrlA, _In_opt_ SIZE_T nUrlLen=(SIZE_T)-1,
                                 _In_opt_z_ LPCSTR szAllowedCharsA=NULL);
  static HRESULT Decode(_Inout_ CStringA &cStrDestA, _In_z_ LPCSTR szUrlA, _In_opt_ SIZE_T nUrlLen=(SIZE_T)-1,
                        _In_opt_ BOOL bAppend=FALSE);
  static SIZE_T GetDecodedLength(_In_z_ LPCSTR szUrlA, _In_opt_ SIZE_T nUrlLen=(SIZE_T)-1);

  static BOOL IsValidHostAddress(_In_z_ LPCSTR szHostA, _In_opt_ SIZE_T nHostLen=(SIZE_T)-1);
  static BOOL IsValidHostAddress(_In_z_ LPCWSTR szHostW, _In_opt_ SIZE_T nHostLen=(SIZE_T)-1);

private:
  typedef struct {
    LPWSTR szValueW;
    WCHAR szNameW[1];
  } QUERYSTRINGITEM, *LPQUERYSTRINGITEM;

  CStringW cStrSchemeW;
  CStringW cStrHostW;
  CStringW cStrPathW;
  int nPort;
  TArrayListWithFree<LPQUERYSTRINGITEM> cQueryStringsList;
  CStringW cStrUserInfoW;
  CStringW cStrFragmentW;
  BOOL bIdnaAllowUnassigned, bIdnaUseStd3AsciiRules;
};

inline CUrl::eToStringFlags operator|(CUrl::eToStringFlags lhs, CUrl::eToStringFlags rhs)
{
  return static_cast<CUrl::eToStringFlags>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

inline CUrl::eToStringFlags operator&(CUrl::eToStringFlags lhs, CUrl::eToStringFlags rhs)
{
  return static_cast<CUrl::eToStringFlags>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_URL_H
