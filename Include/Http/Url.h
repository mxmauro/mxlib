/**
 * Copyright (C) 2011 by Ben Noordhuis <info@bnoordhuis.nl>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *
 * C++ adaptation by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
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
private:
  CUrl(_In_ const CUrl& cSrc);
public:
  CUrl();
  ~CUrl();

  typedef enum {
    SchemeUnknown=-1, SchemeNone=0,
    SchemeMailTo, SchemeNews, SchemeHttp, SchemeHttps, SchemeFtp,
    SchemeFile, SchemeResource, SchemeNntp, SchemeGopher
  } eScheme;

  typedef enum {
    ToStringAddScheme=0x01, ToStringAddUserInfo=0x02, ToStringAddHostPort=0x04,
    ToStringAddPath=0x08, ToStringAddQueryStrings=0x10, ToStringAddFragment=0x20,
    ToStringShrinkPath=0x40,
    ToStringAddAll=0x7F
  } eToStringFlags;

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

  HRESULT ToString(_Inout_ CStringA &cStrDestA, _In_ int nFlags=ToStringAddAll);
  HRESULT ToString(_Inout_ CStringW &cStrDestW, _In_ int nFlags=ToStringAddAll);

  HRESULT ParseFromString(_In_z_ LPCSTR szUrlA, _In_opt_ SIZE_T nSrcLen=(SIZE_T)-1);
  HRESULT ParseFromString(_In_z_ LPCWSTR szUrlW, _In_opt_ SIZE_T nSrcLen=(SIZE_T)-1);

  HRESULT operator=(_In_ const CUrl& cSrc);
  HRESULT operator+=(_In_ const CUrl& cOtherUrl);

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

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_URL_H
