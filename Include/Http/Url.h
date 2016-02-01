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
  CUrl(__in const CUrl& cSrc);
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

  HRESULT SetScheme(__in_z LPCSTR szSchemeA);
  HRESULT SetScheme(__in_z LPCWSTR szSchemeW);
  LPCWSTR GetScheme() const;
  HRESULT GetScheme(__inout CStringA &cStrDestA);
  eScheme GetSchemeCode() const;

  HRESULT SetHost(__in_z LPCSTR szHostA, __in_opt SIZE_T nHostLen=(SIZE_T)-1);
  HRESULT SetHost(__in_z LPCWSTR szHostW, __in_opt SIZE_T nHostLen=(SIZE_T)-1);
  LPCWSTR GetHost() const;
  HRESULT GetHost(__inout CStringA &cStrDestA);

  HRESULT SetPort(__in int nPort=-1);
  int GetPort() const;

  HRESULT SetPath(__in_z LPCSTR szPathA, __in_opt SIZE_T nPathLen=(SIZE_T)-1);
  HRESULT SetPath(__in_z LPCWSTR szPathW, __in_opt SIZE_T nPathLen=(SIZE_T)-1);
  LPCWSTR GetPath() const;

  HRESULT ShrinkPath();

  VOID ResetQueryStrings();
  HRESULT AddQueryString(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA, __in_opt SIZE_T nValueLen=(SIZE_T)-1,
                         __in_opt SIZE_T nNameLen=(SIZE_T)-1);
  HRESULT AddQueryString(__in_z LPCWSTR szNameW, __in_z LPCWSTR szValueW, __in_opt SIZE_T nValueLen=(SIZE_T)-1,
                         __in_opt SIZE_T nNameLen=(SIZE_T)-1);
  HRESULT RemoveQueryString(__in SIZE_T nIndex);
  SIZE_T GetQueryStringCount() const;

  LPCWSTR GetQueryStringName(__in SIZE_T nIndex) const;
  LPCWSTR GetQueryStringValue(__in SIZE_T nIndex) const;
  LPCWSTR GetQueryStringValue(__in_z LPCWSTR szNameW) const;

  HRESULT SetFragment(__in_z LPCSTR szFragmentA, __in_opt SIZE_T nFragmentLen=(SIZE_T)-1);
  HRESULT SetFragment(__in_z LPCWSTR szFragmentW, __in_opt SIZE_T nFragmentLen=(SIZE_T)-1);
  LPCWSTR GetFragment() const;

  HRESULT SetUserInfo(__in_z LPCSTR szUserInfoA, __in_opt SIZE_T nUserInfoLen=(SIZE_T)-1);
  HRESULT SetUserInfo(__in_z LPCWSTR szUserInfoW, __in_opt SIZE_T nUserInfoLen=(SIZE_T)-1);
  LPCWSTR GetUserInfo() const;

  HRESULT ToString(__inout CStringA &cStrDestA, __in int nFlags=ToStringAddAll);
  HRESULT ToString(__inout CStringW &cStrDestW, __in int nFlags=ToStringAddAll);

  HRESULT ParseFromString(__in_z LPCSTR szUrlA, __in_opt SIZE_T nSrcLen=(SIZE_T)-1);
  HRESULT ParseFromString(__in_z LPCWSTR szUrlW, __in_opt SIZE_T nSrcLen=(SIZE_T)-1);

  HRESULT operator=(__in const CUrl& cSrc);
  HRESULT operator+=(__in const CUrl& cOtherUrl);

  HRESULT Merge(__in const CUrl& cOtherUrl);

  static HRESULT Encode(__inout CStringA &cStrDestA, __in_z LPCSTR szUrlA, __in_opt SIZE_T nUrlLen=(SIZE_T)-1,
                        __in_z_opt LPCSTR szAllowedCharsA=NULL, __in_opt BOOL bAppend=FALSE);
  static SIZE_T GetEncodedLength(__in_z LPCSTR szUrlA, __in_opt SIZE_T nUrlLen=(SIZE_T)-1,
                                 __in_z_opt LPCSTR szAllowedCharsA=NULL);
  static HRESULT Decode(__inout CStringA &cStrDestA, __in_z LPCSTR szUrlA, __in_opt SIZE_T nUrlLen=(SIZE_T)-1,
                        __in_opt BOOL bAppend=FALSE);
  static SIZE_T GetDecodedLength(__in_z LPCSTR szUrlA, __in_opt SIZE_T nUrlLen=(SIZE_T)-1);

  static BOOL IsValidHostAddress(__in_z LPCSTR szHostA, __in_opt SIZE_T nHostLen=(SIZE_T)-1);
  static BOOL IsValidHostAddress(__in_z LPCWSTR szHostW, __in_opt SIZE_T nHostLen=(SIZE_T)-1);

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
