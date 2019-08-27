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
#include "..\..\Include\Http\HttpHeaderReqIfModifiedSinceOrIfUnmodifiedSince.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqIfXXXSinceBase::CHttpHeaderReqIfXXXSinceBase(_In_ BOOL _bIfModified) : CHttpHeaderBase()
{
  bIfModified = _bIfModified;
  return;
}

CHttpHeaderReqIfXXXSinceBase::~CHttpHeaderReqIfXXXSinceBase()
{
  return;
}

HRESULT CHttpHeaderReqIfXXXSinceBase::Parse(_In_z_ LPCSTR szValueA)
{
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //parse date
  hRes = CHttpCommon::ParseDate(cDt, szValueA);
  if (FAILED(hRes))
    return (hRes == E_OUTOFMEMORY) ? hRes : MX_E_InvalidData;
  //done
  return S_OK;
}

HRESULT CHttpHeaderReqIfXXXSinceBase::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  return cDt.Format(cStrDestA, "%a, %d %b %Y %H:%m:%S %z");
}

HRESULT CHttpHeaderReqIfXXXSinceBase::SetDate(_In_ CDateTime &_cDt)
{
  cDt = _cDt;
  return S_OK;
}

CDateTime CHttpHeaderReqIfXXXSinceBase::GetDate() const
{
  return cDt;
}

} //namespace MX
