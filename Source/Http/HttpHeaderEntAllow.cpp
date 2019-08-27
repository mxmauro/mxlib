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
#include "..\..\Include\Http\HttpHeaderEntAllow.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderEntAllow::CHttpHeaderEntAllow() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderEntAllow::~CHttpHeaderEntAllow()
{
  cVerbsList.RemoveAllElements();
  return;
}

HRESULT CHttpHeaderEntAllow::Parse(_In_z_ LPCSTR szValueA)
{
  LPCSTR szStartA;
  BOOL bGotItem;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //parse verbs
  bGotItem = FALSE;
  do
  {
    //skip spaces
    szValueA = SkipSpaces(szValueA);
    if (*szValueA == 0)
      break;

    //get verb
    szValueA = GetToken(szStartA = szValueA);
    if (szValueA == szStartA)
      goto skip_null_listitem;

    bGotItem = TRUE;

    //add to list
    hRes = AddVerb(szStartA, (SIZE_T)(szValueA - szStartA));
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

HRESULT CHttpHeaderEntAllow::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  SIZE_T i, nCount;

  cStrDestA.Empty();
  //fill verbs
  nCount = cVerbsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (cStrDestA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(",", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrDestA.Concat(cVerbsList.GetElementAt(i)) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntAllow::AddVerb(_In_z_ LPCSTR szVerbA, _In_ SIZE_T nVerbLen)
{
  CStringA cStrTempA;

  if (nVerbLen == (SIZE_T)-1)
    nVerbLen = StrLenA(szVerbA);
  if (nVerbLen == 0)
    return MX_E_InvalidData;
  if (szVerbA == NULL)
    return E_POINTER;
  //convert to uppercase, validate and set new value
  if (cStrTempA.CopyN(szVerbA, nVerbLen) == FALSE)
    return E_OUTOFMEMORY;
  StrToUpperA((LPSTR)cStrTempA);
  //validate
  if (CHttpCommon::IsValidVerb((LPCSTR)cStrTempA, cStrTempA.GetLength()) == FALSE)
    return MX_E_InvalidData;
  //add to list if not there
  if (HasVerb((LPCSTR)cStrTempA) == FALSE)
  {
    if (cVerbsList.AddElement((LPSTR)cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
    cStrTempA.Detach();
  }
  //done
  return S_OK;
}

SIZE_T CHttpHeaderEntAllow::GetVerbsCount() const
{
  return cVerbsList.GetCount();
}

LPCSTR CHttpHeaderEntAllow::GetVerb(_In_ SIZE_T nIndex) const
{
  return (nIndex < cVerbsList.GetCount()) ? cVerbsList.GetElementAt(nIndex) : NULL;
}

BOOL CHttpHeaderEntAllow::HasVerb(_In_z_ LPCSTR szVerbA) const
{
  SIZE_T i, nCount;

  if (szVerbA != NULL && *szVerbA != 0)
  {
    nCount = cVerbsList.GetCount();
    for (i=0; i < nCount; i++)
    {
      if (StrCompareA(cVerbsList.GetElementAt(i), szVerbA, TRUE) == 0)
        return TRUE;
    }
  }
  return FALSE;
}

} //namespace MX
