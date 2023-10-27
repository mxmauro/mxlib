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

  HRESULT Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser);

  eDuplicateBehavior GetDuplicateBehavior() const
    {
    return eDuplicateBehavior::Merge;
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

  HRESULT Merge(_In_ CHttpHeaderBase *lpHeader);

private:
  typedef struct {
    LPWSTR szValueW;
    CHAR szNameA[1];
  } EXTENSION, *LPEXTENSION;

  BOOL bPublic;
  BOOL bPrivate;
  TArrayListWithFree<LPCSTR> cPrivateFieldsList;
  BOOL bNoCache;
  TArrayListWithFree<LPCSTR> cNoCacheFieldsList;
  BOOL bNoStore;
  BOOL bNoTransform;
  BOOL bMustRevalidate;
  BOOL bProxyRevalidate;
  ULONGLONG nMaxAge;
  ULONGLONG nSMaxAge;
  TArrayListWithFree<LPEXTENSION> aExtensionsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERRESPONSECACHECONTROL_H
