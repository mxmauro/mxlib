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
#ifndef _MX_HTTPHEADERENTITYCONTENTTYPE_H
#define _MX_HTTPHEADERENTITYCONTENTTYPE_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderEntContentType : public CHttpHeaderBase
{
public:
  CHttpHeaderEntContentType();
  ~CHttpHeaderEntContentType();

  MX_DECLARE_HTTPHEADER_NAME(Content-Type)

  HRESULT Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser);

  HRESULT SetType(_In_z_ LPCSTR szTypeA, _In_ SIZE_T nTypeLen = (SIZE_T)-1);
  LPCSTR GetType() const;

  HRESULT AddParam(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW);

  SIZE_T GetParamsCount() const;
  LPCSTR GetParamName(_In_ SIZE_T nIndex) const;
  LPCWSTR GetParamValue(_In_ SIZE_T nIndex) const;
  LPCWSTR GetParamValue(_In_z_ LPCSTR szNameA) const;

private:
  typedef struct {
    LPWSTR szValueW;
    CHAR szNameA[1];
  } PARAMETER, *LPPARAMETER;

  CStringA cStrTypeA;
  TArrayListWithFree<LPPARAMETER> aParamsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERENTITYCONTENTTYPE_H
