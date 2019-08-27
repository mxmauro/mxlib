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
#include "..\..\Include\Http\HttpHeaderGenUpgrade.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderGenUpgrade::CHttpHeaderGenUpgrade() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderGenUpgrade::~CHttpHeaderGenUpgrade()
{
  cProductsList.RemoveAllElements();
  return;
}

HRESULT CHttpHeaderGenUpgrade::Parse(_In_z_ LPCSTR szValueA)
{
  LPCSTR szStartA;
  BOOL bGotItem;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //parse
  bGotItem = FALSE;
  do
  {
    //skip spaces
    szValueA = SkipSpaces(szValueA);
    if (*szValueA == 0)
      break;

    //get product
    szValueA = SkipUntil(szStartA = szValueA, ";, \t");
    if (szStartA == szValueA)
      goto skip_null_listitem;

    bGotItem = TRUE;

    //add product
    hRes = AddProduct(szStartA, (SIZE_T)(szValueA - szStartA));
    if (FAILED(hRes))
      return hRes;

skip_null_listitem:
    //skip spaces
    szValueA = SkipSpaces(szValueA);

    //check for separator or end
    if (*szValueA == ',')
      szValueA++;
    else if (*szValueA != 0)
      return MX_E_InvalidData;
  }
  while (*szValueA != 0);
  //done
  return (bGotItem != FALSE) ? S_OK : MX_E_InvalidData;
}

HRESULT CHttpHeaderGenUpgrade::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  SIZE_T i, nCount;

  cStrDestA.Empty();
  //fill products
  nCount = cProductsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (cStrDestA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(",", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrDestA.Concat(cProductsList.GetElementAt(i)) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderGenUpgrade::AddProduct(_In_z_ LPCSTR szProductA, _In_ SIZE_T nProductLen)
{
  LPCSTR szStartA, szProductEndA;
  CStringA cStrTempA;

  if (nProductLen == (SIZE_T)-1)
    nProductLen = StrLenA(szProductA);
  if (nProductLen == 0)
    return MX_E_InvalidData;
  if (szProductA == NULL)
    return E_POINTER;
  szProductEndA = szProductA + nProductLen;
  //validate and set new value
  if (cStrTempA.CopyN(szProductA, nProductLen) == FALSE)
    return E_OUTOFMEMORY;
  //validate
  szProductA = GetToken(szStartA = szProductA, nProductLen);
  if (szProductA == szStartA)
    return MX_E_InvalidData;
  //check slash separator
  if (szProductA < szProductEndA && *szProductA == '/')
  {
    szProductA++;
    szProductA = GetToken(szProductA, (SIZE_T)(szProductEndA - szProductA));
    if (*(szProductA-1) == '/' || (szProductA < szProductEndA && *szProductA == '/')) //no subtype?
      return MX_E_InvalidData;
  }
  //check for end
  if (szProductA != szProductEndA)
    return MX_E_InvalidData;
  //set new value
  if (cProductsList.AddElement((LPSTR)cStrTempA) == FALSE)
    return E_OUTOFMEMORY;
  cStrTempA.Detach();
  //done
  return S_OK;
}

SIZE_T CHttpHeaderGenUpgrade::GetProductsCount() const
{
  return cProductsList.GetCount();
}

LPCSTR CHttpHeaderGenUpgrade::GetProduct(_In_ SIZE_T nIndex) const
{
  return (nIndex < cProductsList.GetCount()) ? cProductsList.GetElementAt(nIndex) : NULL;
}

BOOL CHttpHeaderGenUpgrade::HasProduct(_In_z_ LPCSTR szProductA) const
{
  SIZE_T i, nCount;

  if (szProductA != NULL && szProductA[0] != 0)
  {
    nCount = cProductsList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (StrCompareA(cProductsList.GetElementAt(i), szProductA, TRUE) == 0)
        return TRUE;
    }
  }
  return FALSE;
}

} //namespace MX
