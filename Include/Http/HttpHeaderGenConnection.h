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
#ifndef _MX_HTTPHEADERGENERALCONNECTION_H
#define _MX_HTTPHEADERGENERALCONNECTION_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderGenConnection : public CHttpHeaderBase
{
public:
  CHttpHeaderGenConnection();
  ~CHttpHeaderGenConnection();

  MX_DECLARE_HTTPHEADER_NAME(Connection)

  HRESULT Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser);

  HRESULT AddConnection(_In_z_ LPCSTR szConnectionA, _In_ SIZE_T nConnectionLen = (SIZE_T)-1);

  SIZE_T GetConnectionsCount() const;
  LPCSTR GetConnection(_In_ SIZE_T nIndex) const;
  BOOL HasConnection(_In_z_ LPCSTR szConnectionA) const;

private:
  TArrayListWithFree<LPSTR> cConnectionsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERGENERALCONNECTION_H
