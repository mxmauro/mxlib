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
#ifndef _MX_HTTPBODYPARSERURLENCODEDFORM_H
#define _MX_HTTPBODYPARSERURLENCODEDFORM_H

#include "HttpBodyParserFormBase.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHttpBodyParserUrlEncodedForm : public CHttpBodyParserFormBase, public CNonCopyableObj
{
public:
  CHttpBodyParserUrlEncodedForm(_In_ DWORD dwMaxFieldSize = 256000);
  ~CHttpBodyParserUrlEncodedForm();

  LPCSTR GetType() const
    {
    return "application/x-www-form-urlencoded";
    };

protected:
  HRESULT Initialize(_In_ Internals::CHttpParser &cHttpParser);
  HRESULT Parse(_In_opt_ LPCVOID lpData, _In_opt_ SIZE_T nDataSize);

private:
  enum class eState
  {
    NameStart, Name, Value, Done, Error
  };

  DWORD dwMaxFieldSize;

  struct {
    eState nState{ eState::NameStart };
    CStringA cStrCurrA;
    CStringW cStrCurrFieldNameW;
  } sParser;
  SIZE_T nCurrContentSize{ 0 };
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPBODYPARSERURLENCODEDFORM_H
