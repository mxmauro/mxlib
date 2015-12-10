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

  HRESULT Parse(__in_z LPCSTR szValueA);

  HRESULT Build(__inout CStringA &cStrDestA);

  HRESULT SetNoCache(__in BOOL bNoCache);
  BOOL GetNoCache() const;

  HRESULT SetNoStore(__in BOOL bNoStore);
  BOOL GetNoStore() const;

  HRESULT SetMaxAge(__in ULONGLONG nMaxAge);
  ULONGLONG GetMaxAge() const;

  HRESULT SetMaxStale(__in ULONGLONG nMaxStale);
  ULONGLONG GetMaxStale() const;

  HRESULT SetMinFresh(__in ULONGLONG nMinFresh);
  ULONGLONG GetMinFresh() const;

  HRESULT SetNoTransform(__in BOOL bNoTransform);
  BOOL GetNoTransform() const;

  HRESULT SetOnlyIfCached(__in BOOL bOnlyIfCached);
  BOOL GetOnlyIfCached() const;

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
