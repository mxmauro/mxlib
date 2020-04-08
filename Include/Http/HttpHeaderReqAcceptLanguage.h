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
#ifndef _MX_HTTPHEADERREQUESTACCEPTLANGUAGE_H
#define _MX_HTTPHEADERREQUESTACCEPTLANGUAGE_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderReqAcceptLanguage : public CHttpHeaderBase
{
public:
  class CLanguage : public virtual CBaseMemObj
  {
  public:
    CLanguage();
    ~CLanguage();

    CLanguage& operator=(_In_ const CLanguage &cSrc) throw(...);

    HRESULT SetLanguage(_In_z_ LPCSTR szLanguageA, _In_opt_ SIZE_T nLanguageLen = (SIZE_T)-1);
    LPCSTR GetLanguage() const;

    HRESULT SetQ(_In_ double q);
    double GetQ() const;

  private:
    friend class CHttpHeaderReqAcceptLanguage;

    CStringA cStrLanguageA;
    double q;
  };

  //----

public:
  CHttpHeaderReqAcceptLanguage();
  ~CHttpHeaderReqAcceptLanguage();

  MX_DECLARE_HTTPHEADER_NAME(Accept-Language)

  HRESULT Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser);

  eDuplicateBehavior GetDuplicateBehavior() const
    {
    return DuplicateBehaviorMerge;
    };

  HRESULT AddLanguage(_In_z_ LPCSTR szLanguageA, _In_opt_ SIZE_T nLanguageLen = (SIZE_T)-1,
                      _Out_opt_ CLanguage **lplpLanguage=NULL);

  SIZE_T GetLanguagesCount() const;
  CLanguage* GetLanguage(_In_ SIZE_T nIndex) const;
  CLanguage* GetLanguage(_In_z_ LPCSTR szLanguageA) const;

  HRESULT Merge(_In_ CHttpHeaderBase *lpHeader);

private:
  TArrayListWithDelete<CLanguage*> aLanguagesList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERREQUESTACCEPTLANGUAGE_H
