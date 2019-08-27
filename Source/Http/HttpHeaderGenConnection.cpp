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
#include "..\..\Include\Http\HttpHeaderGenConnection.h"
#include <intsafe.h>

//-----------------------------------------------------------

namespace MX {

CHttpHeaderGenConnection::CHttpHeaderGenConnection() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderGenConnection::~CHttpHeaderGenConnection()
{
  cConnectionsList.RemoveAllElements();
  return;
}

HRESULT CHttpHeaderGenConnection::Parse(_In_z_ LPCSTR szValueA)
{
  LPCSTR szStartA;
  BOOL bGotItem;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //parse tokens
  bGotItem = FALSE;
  do
  {
    //skip spaces
    szValueA = SkipSpaces(szValueA);
    if (*szValueA == 0)
      break;

    //get connection
    szValueA = SkipUntil(szStartA = szValueA, ";, \t");
    if (szValueA == szStartA)
      goto skip_null_listitem;

    bGotItem = TRUE;

    //add connection
    hRes = AddConnection(szStartA, (SIZE_T)(szValueA - szStartA));
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

HRESULT CHttpHeaderGenConnection::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  SIZE_T i, nCount;

  cStrDestA.Empty();
  nCount = cConnectionsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (cStrDestA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(",", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrDestA.Concat(cConnectionsList.GetElementAt(i)) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderGenConnection::AddConnection(_In_z_ LPCSTR szConnectionA, _In_ SIZE_T nConnectionLen)
{
  CStringA cStrTempA;
  LPCSTR szStartA;

  if (nConnectionLen == (SIZE_T)-1)
    nConnectionLen = StrLenA(szConnectionA);
  if (nConnectionLen == 0)
    return MX_E_InvalidData;
  if (szConnectionA == NULL)
    return E_POINTER;
  //get token
  szConnectionA = CHttpHeaderBase::GetToken(szStartA = szConnectionA, nConnectionLen);
  if (szStartA == szConnectionA)
    return MX_E_InvalidData;
  //check for end
  if (szConnectionA != szStartA + nConnectionLen)
    return MX_E_InvalidData;
  //create new item
  if (cStrTempA.CopyN(szStartA, nConnectionLen) == FALSE)
    return E_OUTOFMEMORY;
  if (cConnectionsList.AddElement((LPSTR)cStrTempA) == FALSE)
    return E_OUTOFMEMORY;
  cStrTempA.Detach();
  //done
  return S_OK;
}

SIZE_T CHttpHeaderGenConnection::GetConnectionsCount() const
{
  return cConnectionsList.GetCount();
}

LPCSTR CHttpHeaderGenConnection::GetConnection(_In_ SIZE_T nIndex) const
{
  return (nIndex < cConnectionsList.GetCount()) ? cConnectionsList.GetElementAt(nIndex) : NULL;
}

BOOL CHttpHeaderGenConnection::HasConnection(_In_z_ LPCSTR szConnectionA) const
{
  SIZE_T i, nCount;

  if (szConnectionA != NULL && szConnectionA[0] != 0)
  {
    nCount = cConnectionsList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (StrCompareA(cConnectionsList.GetElementAt(i), szConnectionA, TRUE) == 0)
        return TRUE;
    }
  }
  return FALSE;
}

} //namespace MX
