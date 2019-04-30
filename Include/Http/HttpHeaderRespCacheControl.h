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
#ifndef _MX_HTTPHEADERRESPONSECACHECONTROL_H
#define _MX_HTTPHEADERRESPONSECACHECONTROL_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderRespCacheControl : public CHttpHeaderBase
{
public:
  CHttpHeaderRespCacheControl();
  ~CHttpHeaderRespCacheControl();

  MX_DECLARE_HTTPHEADER_NAME(Cache-Control)

  HRESULT Parse(_In_z_ LPCSTR szValueA);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser);

  eDuplicateBehavior GetDuplicateBehavior() const
    {
    return DuplicateBehaviorAppend;
    };

  HRESULT SetPublic(_In_ BOOL bPublic);
  BOOL GetPublic() const;

  HRESULT SetPrivate(_In_ BOOL bPrivate);
  BOOL GetPrivate() const;

  HRESULT AddPrivateField(_In_z_ LPCSTR szFieldA, _In_ SIZE_T nFieldLen = (SIZE_T)-1);

  SIZE_T GetPrivateFieldsCount() const;
  LPCSTR GetPrivateField(_In_ SIZE_T nIndex) const;
  BOOL HasPrivateField(_In_z_ LPCSTR szFieldA) const;

  HRESULT SetNoCache(_In_ BOOL bNoCache);
  BOOL GetNoCache() const;

  HRESULT AddNoCacheField(_In_z_ LPCSTR szFieldA, _In_ SIZE_T nFieldLen = (SIZE_T)-1);

  SIZE_T GetNoCacheFieldsCount() const;
  LPCSTR GetNoCacheField(_In_ SIZE_T nIndex) const;
  BOOL HasNoCacheField(_In_z_ LPCSTR szFieldA) const;

  HRESULT SetNoStore(_In_ BOOL bNoStore);
  BOOL GetNoStore() const;

  HRESULT SetNoTransform(_In_ BOOL bNoTransform);
  BOOL GetNoTransform() const;

  HRESULT SetMustRevalidate(_In_ BOOL bMustRevalidate);
  BOOL GetMustRevalidate() const;

  HRESULT SetProxyRevalidate(_In_ BOOL bProxyRevalidate);
  BOOL GetProxyRevalidate() const;

  HRESULT SetMaxAge(_In_ ULONGLONG nMaxAge);
  ULONGLONG GetMaxAge() const;

  HRESULT SetSharedMaxAge(_In_ ULONGLONG nSharedMaxAge);
  ULONGLONG GetSharedMaxAge() const;

  HRESULT AddExtension(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW);

  SIZE_T GetExtensionsCount() const;
  LPCSTR GetExtensionName(_In_ SIZE_T nIndex) const;
  LPCWSTR GetExtensionValue(_In_ SIZE_T nIndex) const;
  LPCWSTR GetExtensionValue(_In_z_ LPCSTR szNameA) const;

private:
  typedef struct {
    LPWSTR szValueW;
    CHAR szNameA[1];
  } EXTENSION, *LPEXTENSION;

  BOOL bPublic;
  BOOL bPrivate;
  TArrayListWithFree<LPSTR> cPrivateFieldsList;
  BOOL bNoCache;
  TArrayListWithFree<LPSTR> cNoCacheFieldsList;
  BOOL bNoStore;
  BOOL bNoTransform;
  BOOL bMustRevalidate;
  BOOL bProxyRevalidate;
  ULONGLONG nMaxAge;
  ULONGLONG nSMaxAge;
  TArrayListWithFree<LPEXTENSION> cExtensionsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERRESPONSECACHECONTROL_H
