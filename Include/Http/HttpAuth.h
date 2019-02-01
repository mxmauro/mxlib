/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2019. All rights reserved.
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
#ifndef _MX_HTTPAUTH_H
#define _MX_HTTPAUTH_H

#include "..\Defines.h"
#include "..\Strings\Strings.h"
#include "..\Strings\Utf8.h"
#include "..\RefCounted.h"
#include "Url.h"

//-----------------------------------------------------------

namespace MX {

class CHttpBaseAuth : public TRefCounted<CBaseMemObj>
{
  MX_DISABLE_COPY_CONSTRUCTOR(CHttpBaseAuth);
public:
  typedef enum {
    TypeBasic=1,
    TypeDigest
  } Type;

protected:
  CHttpBaseAuth() : TRefCounted<CBaseMemObj>()
    {
    return;
    };

public:
  ~CHttpBaseAuth()
    { }

  SIZE_T GetId()
    {
    return (SIZE_T)this;
    };

  virtual Type GetType() const = 0;

  virtual HRESULT Parse(_In_z_ LPCSTR szValueA) = 0;

  virtual BOOL IsReauthenticateRequst() const = 0;

  virtual BOOL IsSameThan(_In_ CHttpBaseAuth *lpCompareTo) = 0;
};

//--------

class CHttpAuthBasic : public CHttpBaseAuth
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

  BOOL IsSameThan(_In_ CHttpBaseAuth *lpCompareTo);

private:
  CStringW cStrRealmW;
  BOOL bCharsetIsUtf8;
};

//--------

class CHttpAuthDigest : public CHttpBaseAuth
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

  BOOL IsSameThan(_In_ CHttpBaseAuth *lpCompareTo);

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
