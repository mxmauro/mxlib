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

  HRESULT Parse(__in_z LPCSTR szValueA);

  HRESULT Build(__inout CStringA &cStrDestA);

  HRESULT SetPublic(__in BOOL bPublic);
  BOOL GetPublic() const;

  HRESULT SetPrivate(__in BOOL bPrivate);
  BOOL GetPrivate() const;

  HRESULT AddPrivateField(__in_z LPCSTR szFieldA);

  SIZE_T GetPrivateFieldsCount() const;
  LPCSTR GetPrivateField(__in SIZE_T nIndex) const;
  BOOL HasPrivateField(__in_z LPCSTR szFieldA) const;

  HRESULT SetNoCache(__in BOOL bNoCache);
  BOOL GetNoCache() const;

  HRESULT AddNoCacheField(__in_z LPCSTR szFieldA);

  SIZE_T GetNoCacheFieldsCount() const;
  LPCSTR GetNoCacheField(__in SIZE_T nIndex) const;
  BOOL HasNoCacheField(__in_z LPCSTR szFieldA) const;

  HRESULT SetNoStore(__in BOOL bNoStore);
  BOOL GetNoStore() const;

  HRESULT SetNoTransform(__in BOOL bNoTransform);
  BOOL GetNoTransform() const;

  HRESULT SetMustRevalidate(__in BOOL bMustRevalidate);
  BOOL GetMustRevalidate() const;

  HRESULT SetProxyRevalidate(__in BOOL bProxyRevalidate);
  BOOL GetProxyRevalidate() const;

  HRESULT SetMaxAge(__in ULONGLONG nMaxAge);
  ULONGLONG GetMaxAge() const;

  HRESULT SetSharedMaxAge(__in ULONGLONG nSharedMaxAge);
  ULONGLONG GetSharedMaxAge() const;

  HRESULT AddExtension(__in_z LPCSTR szNameA, __in_z LPCWSTR szValueW);

  SIZE_T GetExtensionsCount() const;
  LPCSTR GetExtensionName(__in SIZE_T nIndex) const;
  LPCWSTR GetExtensionValue(__in SIZE_T nIndex) const;
  LPCWSTR GetExtensionValue(__in_z LPCSTR szNameA) const;

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
