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
#include "..\..\Include\Http\HttpHeaderEntContentLanguage.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderEntContentLanguage::CHttpHeaderEntContentLanguage() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderEntContentLanguage::~CHttpHeaderEntContentLanguage()
{
  return;
}

HRESULT CHttpHeaderEntContentLanguage::Parse(_In_z_ LPCSTR szValueA)
{
  LPCSTR szStartA;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //mark start
  szStartA = szValueA;
  //get language
  while (*szValueA >= 0x21 && *szValueA <= 0x7E)
    szValueA++;
  //set language
  hRes = SetLanguage(szStartA, (SIZE_T)(szValueA - szStartA));
  if (FAILED(hRes))
    return hRes;
  //skip spaces and check for end
  if (*SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentLanguage::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  if (cStrDestA.CopyN((LPCSTR)cStrLanguageA, cStrLanguageA.GetLength()) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentLanguage::SetLanguage(_In_z_ LPCSTR szLanguageA, _In_ SIZE_T nLanguageLen)
{
  LPCSTR szLanguageEndA, szStartA[2];

  if (nLanguageLen == (SIZE_T)-1)
    nLanguageLen = StrLenA(szLanguageA);
  if (nLanguageLen == 0)
    return MX_E_InvalidData;
  if (szLanguageA == NULL)
    return E_POINTER;
  szLanguageEndA = szLanguageA + nLanguageLen;

  //get language
  szStartA[0] = szLanguageA;
  while (szLanguageA < szLanguageEndA &&
         ((*szLanguageA >= 'A' && *szLanguageA <= 'Z') || (*szLanguageA >= 'a' && *szLanguageA <= 'z')))
  {
    szLanguageA++;
  }
  if (szLanguageA == szStartA[0] || szLanguageA > szStartA[0] + 8)
    return MX_E_InvalidData;
  while (szLanguageA < szLanguageEndA && *szLanguageA == '-')
  {
    szStartA[1] = ++szLanguageA;
    while (szLanguageA < szLanguageEndA &&
           ((*szLanguageA >= 'A' && *szLanguageA <= 'Z') || (*szLanguageA >= 'a' && *szLanguageA <= 'z') ||
            (*szLanguageA >= '0' && *szLanguageA <= '9')))
    {
      szLanguageA++;
    }
    if (szLanguageA == szStartA[1] || szLanguageA > szStartA[1] + 8)
      return MX_E_InvalidData;
  }

  //check for end
  if (szLanguageA != szLanguageEndA)
    return MX_E_InvalidData;
  //set new value
  if (cStrLanguageA.CopyN(szStartA[0], (SIZE_T)(szLanguageEndA - szStartA[0])) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCSTR CHttpHeaderEntContentLanguage::GetLanguage() const
{
  return (LPCSTR)cStrLanguageA;
}

} //namespace MX
