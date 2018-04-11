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
  CHttpCookie(_In_ const CHttpCookie& cSrc) throw(...);
  ~CHttpCookie();

  CHttpCookie& operator=(_In_ const CHttpCookie& cSrc) throw(...);

  VOID Clear();

  HRESULT SetName(_In_z_ LPCSTR szNameA);
  //NOTE: Name will be UTF-8 encoded
  HRESULT SetName(_In_z_ LPCWSTR szNameW);
  LPCSTR GetName() const;
  //NOTE: Name will be UTF-8 decoded
  HRESULT GetName(_Inout_ CStringW &cStrDestW);

  HRESULT SetValue(_In_z_ LPCSTR szValueA);
  //NOTE: Value will be UTF-8 encoded
  HRESULT SetValue(_In_z_ LPCWSTR szValueW);
  LPCSTR GetValue() const;
  //NOTE: Value will be UTF-8 decoded
  HRESULT GetValue(_Inout_ CStringW &cStrDestW);

  HRESULT SetDomain(_In_z_ LPCSTR szDomainA);
  //NOTE: Domain will be PUNY encoded
  HRESULT SetDomain(_In_z_ LPCWSTR szDomainW);
  LPCSTR GetDomain() const;
  //NOTE: Domain will be PUNY decoded
  HRESULT GetDomain(_Inout_ CStringW &cStrDestW);

  HRESULT SetPath(_In_z_ LPCSTR szPathA);
  //NOTE: Path will be UTF-8 encoded
  HRESULT SetPath(_In_z_ LPCWSTR szPathW);
  LPCSTR GetPath() const;
  //NOTE: Path will be UTF-8 decoded
  HRESULT GetPath(_Inout_ CStringW &cStrDestW);

  VOID SetExpireDate(_In_opt_ const CDateTime *lpDate=NULL);
  CDateTime* GetExpireDate() const;

  VOID SetSecureFlag(_In_ BOOL bIsSecure);
  BOOL GetSecureFlag() const;

  VOID SetHttpOnlyFlag(_In_ BOOL bIsHttpOnly);
  BOOL GetHttpOnlyFlag() const;

  HRESULT ToString(_Inout_ CStringA& cStrDestA, _In_ BOOL bAddAttributes=TRUE);

  HRESULT DoesDomainMatch(_In_z_ LPCSTR szDomainToMatchA);
  //NOTE: Domain will be PUNY encoded
  HRESULT DoesDomainMatch(_In_z_ LPCWSTR szDomainToMatchW);
  HRESULT HasExpired(_In_opt_ const CDateTime *lpDate=NULL);

  HRESULT ParseFromResponseHeader(_In_z_ LPCSTR szSrcA, _In_opt_ SIZE_T nSrcLen=(SIZE_T)-1);

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
public:
  CHttpCookieArray() : TArrayListWithDelete<CHttpCookie*>()
    { };
  CHttpCookieArray(_In_ const CHttpCookieArray& cSrc) throw(...);

  CHttpCookieArray& operator=(_In_ const CHttpCookieArray& cSrc) throw(...);

  HRESULT ParseFromRequestHeader(_In_z_ LPCSTR szSrcA, _In_opt_ SIZE_T nSrcLen=(SIZE_T)-1);

  HRESULT Update(_In_ const CHttpCookieArray& cSrc, _In_ BOOL bReplaceExisting);
  HRESULT Update(_In_ const CHttpCookie& cSrc, _In_ BOOL bReplaceExisting);

  HRESULT RemoveExpiredAndInvalid(_In_opt_ const CDateTime *lpDate=NULL);
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPCOOKIE_H
