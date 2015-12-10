/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2015. All rights reserved.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 *
 * 2. This notice may not be removed or altered from any source distribution.
 *
 * 3. YOU MAY NOT:
 *
 *    a. Modify, translate, adapt, alter, or create derivative works from
 *       this software.
 *    b. Copy (other than one back-up copy), distribute, publicly display,
 *       transmit, sell, rent, lease or otherwise exploit this software.
 *    c. Distribute, sub-license, rent, lease, loan [or grant any third party
 *       access to or use of the software to any third party.
 **/
#ifndef _MX_HTTPCOOKIE_H
#define _MX_HTTPCOOKIE_H

#include "..\Defines.h"
#include "..\ArrayList.h"
#include "..\AutoPtr.h"
#include "..\DateTime\DateTime.h"
#include "..\Strings\Strings.h"

//-----------------------------------------------------------

//NOTE: CHttpCookie only accepts ASCII characters. If you
//      need to use another charset like Unicode, you must
//      encode the string prior assignation any member of
//      CHttpCookie and decode it when retrieving values.

//-----------------------------------------------------------

namespace MX {

class CHttpCookie : public virtual CBaseMemObj
{
public:
  CHttpCookie();
  ~CHttpCookie();

  VOID Clear();

  HRESULT SetName(__in_z LPCSTR szNameA);
  //NOTE: Name will be UTF-8 encoded
  HRESULT SetName(__in_z LPCWSTR szNameW);
  LPCSTR GetName() const;
  //NOTE: Name will be UTF-8 decoded
  HRESULT GetName(__inout CStringW &cStrDestW);

  HRESULT SetValue(__in_z LPCSTR szValueA);
  //NOTE: Value will be UTF-8 encoded
  HRESULT SetValue(__in_z LPCWSTR szValueW);
  LPCSTR GetValue() const;
  //NOTE: Value will be UTF-8 decoded
  HRESULT GetValue(__inout CStringW &cStrDestW);

  HRESULT SetDomain(__in_z LPCSTR szDomainA);
  //NOTE: Domain will be PUNY encoded
  HRESULT SetDomain(__in_z LPCWSTR szDomainW);
  LPCSTR GetDomain() const;
  //NOTE: Domain will be PUNY decoded
  HRESULT GetDomain(__inout CStringW &cStrDestW);

  HRESULT SetPath(__in_z LPCSTR szPathA);
  //NOTE: Path will be UTF-8 encoded
  HRESULT SetPath(__in_z LPCWSTR szPathW);
  LPCSTR GetPath() const;
  //NOTE: Path will be UTF-8 decoded
  HRESULT GetPath(__inout CStringW &cStrDestW);

  VOID SetExpireDate(__in const CDateTime *lpDate=NULL);
  CDateTime* GetExpireDate() const;

  VOID SetSecureFlag(__in BOOL bIsSecure);
  BOOL GetSecureFlag() const;

  VOID SetHttpOnlyFlag(__in BOOL bIsHttpOnly);
  BOOL GetHttpOnlyFlag() const;

  HRESULT operator=(__in const CHttpCookie& cSrc);

  HRESULT ToString(__inout CStringA& cStrDestA, __in BOOL bAddAttributes=TRUE);

  HRESULT DoesDomainMatch(__in_z LPCSTR szDomainToMatchA);
  //NOTE: Domain will be PUNY encoded
  HRESULT DoesDomainMatch(__in_z LPCWSTR szDomainToMatchW);
  HRESULT HasExpired(__in_opt const CDateTime *lpDate=NULL);

  HRESULT ParseFromResponseHeader(__in_z LPCSTR szSrcA, __in_opt SIZE_T nSrcLen=(SIZE_T)-1);

private:
  ULONG nFlags;
  CStringA cStrNameA;
  CStringA cStrValueA;
  CStringA cStrDomainA;
  CStringA cStrPathA;
  CDateTime cExpiresDt;
};

//-----------------------------------------------------------

class CHttpCookieArray : public TArrayListWithDelete<CHttpCookie*>
{
private:
  CHttpCookieArray(__in const CHttpCookieArray& cSrc);
public:
  CHttpCookieArray() : TArrayListWithDelete<CHttpCookie*>()
    { };
  HRESULT operator=(const CHttpCookieArray& cSrc);

  HRESULT ParseFromRequestHeader(__in_z LPCSTR szSrcA, __in_opt SIZE_T nSrcLen=(SIZE_T)-1);

  HRESULT Update(__in const CHttpCookieArray& cSrc, __in BOOL bReplaceExisting);
  HRESULT Update(__in const CHttpCookie& cSrc, __in BOOL bReplaceExisting);

  HRESULT RemoveExpiredAndInvalid(__in_opt const CDateTime *lpDate=NULL);
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPCOOKIE_H
