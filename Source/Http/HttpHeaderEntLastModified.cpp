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
#include "..\..\Include\Http\HttpHeaderEntLastModified.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderEntLastModified::CHttpHeaderEntLastModified() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderEntLastModified::~CHttpHeaderEntLastModified()
{
  return;
}

HRESULT CHttpHeaderEntLastModified::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  LPCSTR szValueEndA;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  szValueEndA = szValueA + nValueLen;

  //skip spaces
  szValueA = SkipSpaces(szValueA, szValueEndA);

  //parse date
  hRes = Http::ParseDate(cDt, szValueA, (SIZE_T)(szValueEndA - szValueA));
  if (FAILED(hRes))
    return (hRes == E_OUTOFMEMORY) ? hRes : MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpHeaderEntLastModified::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  return cDt.Format(cStrDestA, "%a, %d %b %Y %H:%m:%S %z");
}

HRESULT CHttpHeaderEntLastModified::SetDate(_In_ CDateTime &_cDt)
{
  cDt = _cDt;
  return S_OK;
}

CDateTime CHttpHeaderEntLastModified::GetDate() const
{
  return cDt;
}

} //namespace MX
