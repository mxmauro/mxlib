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
#ifndef _MX_HTTPHEADERREQUESTACCEPTCHARSET_H
#define _MX_HTTPHEADERREQUESTACCEPTCHARSET_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderReqAcceptCharset : public CHttpHeaderBase
{
public:
  class CCharset : public virtual CBaseMemObj
  {
  public:
    CCharset();
    ~CCharset();

    CCharset& operator=(_In_ const CCharset &cSrc) throw(...);

    HRESULT SetCharset(_In_z_ LPCSTR szCharsetA, _In_opt_ SIZE_T nCharsetLen = (SIZE_T)-1);
    LPCSTR GetCharset() const;

    HRESULT SetQ(_In_ double q);
    double GetQ() const;

  private:
    friend class CHttpHeaderReqAcceptCharset;

    CStringA cStrCharsetA;
    double q;
  };

  //----

public:
  CHttpHeaderReqAcceptCharset();
  ~CHttpHeaderReqAcceptCharset();

  MX_DECLARE_HTTPHEADER_NAME(Accept-Charset)

  HRESULT Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser);

  eDuplicateBehavior GetDuplicateBehavior() const
    {
    return eDuplicateBehavior::Merge;
    };

  HRESULT AddCharset(_In_z_ LPCSTR szCharsetA, _In_opt_ SIZE_T nCharsetLen = (SIZE_T)-1,
                     _Out_opt_ CCharset **lplpCharset = NULL);

  SIZE_T GetCharsetsCount() const;
  CCharset* GetCharset(_In_ SIZE_T nIndex) const;
  CCharset* GetCharset(_In_z_ LPCSTR szCharsetA) const;

  HRESULT Merge(_In_ CHttpHeaderBase *lpHeader);

private:
  TArrayListWithDelete<CCharset*> aCharsetsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERREQUESTACCEPTCHARSET_H
