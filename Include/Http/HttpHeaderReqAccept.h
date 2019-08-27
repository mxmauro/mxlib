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
#ifndef _MX_HTTPHEADERREQUESTACCEPT_H
#define _MX_HTTPHEADERREQUESTACCEPT_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderReqAccept : public CHttpHeaderBase
{
public:
  class CType : public virtual CBaseMemObj
  {
  public:
    CType();
    ~CType();

    HRESULT SetType(_In_z_ LPCSTR szTypeA, _In_ SIZE_T nTypeLen = (SIZE_T)-1);
    LPCSTR GetType() const;

    HRESULT SetQ(_In_ double q);
    double GetQ() const;

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
    double q;
    TArrayListWithFree<LPPARAMETER> cParamsList;
  };

  //----

public:
  CHttpHeaderReqAccept();
  ~CHttpHeaderReqAccept();

  MX_DECLARE_HTTPHEADER_NAME(Accept)

  HRESULT Parse(_In_z_ LPCSTR szValueA);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser);

  eDuplicateBehavior GetDuplicateBehavior() const
    {
    return DuplicateBehaviorAppend;
    };

  HRESULT AddType(_In_z_ LPCSTR szTypeA, _In_opt_ SIZE_T nTypeLen = (SIZE_T)-1, _Out_opt_ CType **lplpType = NULL);

  SIZE_T GetTypesCount() const;
  CType* GetType(_In_ SIZE_T nIndex) const;
  CType* GetType(_In_z_ LPCSTR szTypeA) const;

private:
  TArrayListWithDelete<CType*> cTypesList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERREQUESTACCEPT_H
