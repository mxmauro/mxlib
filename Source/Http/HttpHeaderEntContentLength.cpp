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
#include "..\..\Include\Http\HttpHeaderEntContentLength.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderEntContentLength::CHttpHeaderEntContentLength() : CHttpHeaderBase()
{
  nLength = 0;
  return;
}

CHttpHeaderEntContentLength::~CHttpHeaderEntContentLength()
{
  return;
}

HRESULT CHttpHeaderEntContentLength::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  LPCSTR szValueEndA;
  ULONGLONG nTemp;

  if (szValueA == NULL)
    return E_POINTER;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  szValueEndA = szValueA + nValueLen;

  //skip spaces
  szValueA = SkipSpaces(szValueA, szValueEndA);

  //parse value
  if (szValueA >= szValueEndA || *szValueA < '0' || *szValueA > '9')
    return MX_E_InvalidData;

  while (szValueA < szValueEndA && *szValueA == '0')
    szValueA++;

  nLength = 0;
  while (szValueA < szValueEndA && *szValueA >= '0' && *szValueA <= '9')
  {
    nTemp = nLength * 10ui64;
    if (nTemp < nLength)
      return MX_E_ArithmeticOverflow;
    nLength = nTemp + (ULONGLONG)(*szValueA) - (ULONGLONG)'0';
    if (nLength < nTemp)
      return MX_E_ArithmeticOverflow;
    szValueA++;
  }

  //skip spaces and check for end
  if (SkipSpaces(szValueA, szValueEndA) != szValueEndA)
    return MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentLength::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  if (cStrDestA.Format("%I64u", nLength) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentLength::SetLength(_In_ ULONGLONG _nLength)
{
  nLength = _nLength;
  return S_OK;
}

ULONGLONG CHttpHeaderEntContentLength::GetLength() const
{
  return nLength;
}

} //namespace MX
