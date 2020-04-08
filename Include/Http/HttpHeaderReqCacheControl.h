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

  HRESULT Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser);

  eDuplicateBehavior GetDuplicateBehavior() const
    {
    return DuplicateBehaviorMerge;
    };

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

  HRESULT Merge(_In_ CHttpHeaderBase *lpHeader);

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
  TArrayListWithFree<LPEXTENSION> aExtensionsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERREQUESTCACHECONTROL_H
