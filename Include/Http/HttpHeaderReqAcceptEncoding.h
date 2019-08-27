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
#ifndef _MX_HTTPHEADERREQUESTACCEPTENCODING_H
#define _MX_HTTPHEADERREQUESTACCEPTENCODING_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderReqAcceptEncoding : public CHttpHeaderBase
{
public:
  class CEncoding : public virtual CBaseMemObj
  {
  public:
    CEncoding();
    ~CEncoding();

    HRESULT SetEncoding(_In_z_ LPCSTR szEncodingA, _In_opt_ SIZE_T nEncodingLen = (SIZE_T)-1);
    LPCSTR GetEncoding() const;

    HRESULT SetQ(_In_ double q);
    double GetQ() const;

  private:
    CStringA cStrEncodingA;
    double q;
  };

  //----

public:
  CHttpHeaderReqAcceptEncoding();
  ~CHttpHeaderReqAcceptEncoding();

  MX_DECLARE_HTTPHEADER_NAME(Accept-Encoding)

  HRESULT Parse(_In_z_ LPCSTR szValueA);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser);

  eDuplicateBehavior GetDuplicateBehavior() const
    {
    return DuplicateBehaviorAppend;
    };

  HRESULT AddEncoding(_In_z_ LPCSTR szEncodingA, _In_opt_ SIZE_T nEncodingLen = (SIZE_T)-1,
                      _Out_opt_ CEncoding **lplpEncoding = NULL);

  SIZE_T GetEncodingsCount() const;
  CEncoding* GetEncoding(_In_ SIZE_T nIndex) const;
  CEncoding* GetEncoding(_In_z_ LPCSTR szEncodingA) const;

private:
  TArrayListWithDelete<CEncoding*> cEncodingsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERREQUESTACCEPTENCODING_H
