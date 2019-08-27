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
#include "..\..\Include\Http\HttpHeaderGeneric.h"
#include <stdlib.h>

//-----------------------------------------------------------

namespace MX {

CHttpHeaderGeneric::CHttpHeaderGeneric() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderGeneric::~CHttpHeaderGeneric()
{
  return;
}

HRESULT CHttpHeaderGeneric::SetHeaderName(_In_z_ LPCSTR szNameA)
{
  SIZE_T i;

  if (szNameA == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  for (i = 0; szNameA[i] != 0; i++)
  {
    if (CHttpCommon::IsValidNameChar(szNameA[i]) == FALSE)
      return E_INVALIDARG;
  }
  return (cStrHeaderNameA.Copy(szNameA) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

LPCSTR CHttpHeaderGeneric::GetHeaderName() const
{
  return (LPCSTR)cStrHeaderNameA;
}

HRESULT CHttpHeaderGeneric::Parse(_In_z_ LPCSTR szValueA)
{
  return SetValue(szValueA);
}

HRESULT CHttpHeaderGeneric::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  if (cStrDestA.CopyN((LPCSTR)cStrValueA, cStrValueA.GetLength()) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

HRESULT CHttpHeaderGeneric::SetValue(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  LPCSTR szStartA, szEndA, szValueEndA;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  if (nValueLen == 0)
    return MX_E_InvalidData;
  if (szValueA == NULL)
    return E_POINTER;
  szValueEndA = szValueA + nValueLen;
  //parse value
  szStartA = szEndA = szValueA = SkipSpaces(szValueA, nValueLen);
  while (szValueA < szValueEndA)
  {
    if (*((LPBYTE)szValueA) < 32)
      return MX_E_InvalidData;
    szEndA = ++szValueA;
    szValueA = SkipSpaces(szValueA, (SIZE_T)(szValueEndA - szValueA));
  }
  //set value
  if (cStrValueA.CopyN(szStartA, (SIZE_T)(szEndA - szStartA)) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCSTR CHttpHeaderGeneric::GetValue() const
{
  return (LPSTR)cStrValueA;
}

} //namespace MX
