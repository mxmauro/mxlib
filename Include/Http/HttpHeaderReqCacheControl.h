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
#ifndef _MX_HTTPHEADERREQUESTCACHECONTROL_H
#define _MX_HTTPHEADERREQUESTCACHECONTROL_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderReqCacheControl : public CHttpHeaderBase
{
public:
  CHttpHeaderReqCacheControl();
  ~CHttpHeaderReqCacheControl();

  MX_DECLARE_HTTPHEADER_NAME(Cache-Control)

  HRESULT Parse(_In_z_ LPCSTR szValueA);

  HRESULT Build(_Inout_ CStringA &cStrDestA);

  HRESULT SetNoCache(_In_ BOOL bNoCache);
  BOOL GetNoCache() const;

  HRESULT SetNoStore(_In_ BOOL bNoStore);
  BOOL GetNoStore() const;

  HRESULT SetMaxAge(_In_ ULONGLONG nMaxAge);
  ULONGLONG GetMaxAge() const;

  HRESULT SetMaxStale(_In_ ULONGLONG nMaxStale);
  ULONGLONG GetMaxStale() const;

  HRESULT SetMinFresh(_In_ ULONGLONG nMinFresh);
  ULONGLONG GetMinFresh() const;

  HRESULT SetNoTransform(_In_ BOOL bNoTransform);
  BOOL GetNoTransform() const;

  HRESULT SetOnlyIfCached(_In_ BOOL bOnlyIfCached);
  BOOL GetOnlyIfCached() const;

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

  BOOL bNoCache;
  BOOL bNoStore;
  ULONGLONG nMaxAge;
  ULONGLONG nMaxStale;
  ULONGLONG nMinFresh;
  BOOL bNoTransform;
  BOOL bOnlyIfCached;
  TArrayListWithFree<LPEXTENSION> cExtensionsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERREQUESTCACHECONTROL_H
