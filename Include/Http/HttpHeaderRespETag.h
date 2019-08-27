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
#ifndef _MX_HTTPHEADERRESPONSEETAG_H
#define _MX_HTTPHEADERRESPONSEETAG_H

#include "HttpHeaderBase.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderRespETag : public CHttpHeaderBase
{
public:
  CHttpHeaderRespETag();
  ~CHttpHeaderRespETag();

  MX_DECLARE_HTTPHEADER_NAME(ETag)

  HRESULT Parse(_In_z_ LPCSTR szValueA);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser);

  HRESULT SetTag(_In_z_ LPCSTR szTagA, _In_ SIZE_T nTagLen = (SIZE_T)-1);
  LPCSTR GetTag() const;

  HRESULT SetWeak(_In_ BOOL bIsWeak);
  BOOL GetWeak() const;

private:
  CStringA cStrTagA;
  BOOL bIsWeak;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERRESPONSEETAG_H
