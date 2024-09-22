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
#include "..\..\Include\Http\HttpHeaderReqSecWebSocketProtocol.h"

//-----------------------------------------------------------

static int ProtocolCompareFunc(_In_ LPVOID lpContext, _In_ LPSTR *lpElem1, _In_ LPSTR *lpElem2);

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqSecWebSocketProtocol::CHttpHeaderReqSecWebSocketProtocol() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderReqSecWebSocketProtocol::~CHttpHeaderReqSecWebSocketProtocol()
{
  return;
}

HRESULT CHttpHeaderReqSecWebSocketProtocol::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  LPCSTR szValueEndA, szStartA;
  BOOL bGotItem;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  szValueEndA = szValueA + nValueLen;

  //parse
  bGotItem = FALSE;

  do
  {
    //skip spaces
    szValueA = SkipSpaces(szValueA, szValueEndA);
    if (szValueA >= szValueEndA)
      break;

    //protocol
    szValueA = SkipUntil(szStartA = szValueA, szValueEndA, ";, \t");
    if (szValueA == szStartA)
      goto skip_null_listitem;

    bGotItem = TRUE;

    //add protocol to list
    hRes = AddProtocol(szStartA, (SIZE_T)(szValueA - szStartA));
    if (FAILED(hRes))
      return hRes;

skip_null_listitem:
    //skip spaces
    szValueA = SkipSpaces(szValueA, szValueEndA);

    //check for separator or end
    if (szValueA < szValueEndA)
    {
      if (*szValueA == ',')
        szValueA++;
      else
        return MX_E_InvalidData;
    }
  }
  while (szValueA < szValueEndA);

  //do we got one?
  if (bGotItem == FALSE)
    return MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpHeaderReqSecWebSocketProtocol::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  SIZE_T i, nCount;

  cStrDestA.Empty();

  nCount = aProtocolsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (cStrDestA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(",", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrDestA.Concat(aProtocolsList.GetElementAt(i)) == FALSE)
      return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

HRESULT CHttpHeaderReqSecWebSocketProtocol::AddProtocol(_In_z_ LPCSTR szProtocolA, _In_ SIZE_T nProtocolLen)
{
  CStringA cStrNewProtocolA;
  LPCSTR szStartA;
  BOOL bAlreadyOnList;

  if (nProtocolLen == (SIZE_T)-1)
    nProtocolLen = StrLenA(szProtocolA);
  if (nProtocolLen == 0)
    return MX_E_InvalidData;
  if (szProtocolA == NULL)
    return E_POINTER;

  szProtocolA = GetToken(szStartA = szProtocolA, szProtocolA + nProtocolLen);
  if (szProtocolA != szStartA + nProtocolLen)
    return MX_E_InvalidData;

  if (cStrNewProtocolA.CopyN(szStartA, nProtocolLen) == FALSE)
    return E_OUTOFMEMORY;

  //add protocol to list
  if (aProtocolsList.SortedInsert((LPSTR)cStrNewProtocolA, &ProtocolCompareFunc, NULL, TRUE,
                                  &bAlreadyOnList) == FALSE)
  {
    return E_OUTOFMEMORY;
  }
  if (bAlreadyOnList == FALSE)
    cStrNewProtocolA.Detach();
  //done
  return S_OK;
}

SIZE_T CHttpHeaderReqSecWebSocketProtocol::GetProtocolsCount() const
{
  return aProtocolsList.GetCount();
}

LPCSTR CHttpHeaderReqSecWebSocketProtocol::GetProtocol(_In_ SIZE_T nIndex) const
{
  return (nIndex < aProtocolsList.GetCount()) ? aProtocolsList.GetElementAt(nIndex) : NULL;
}

BOOL CHttpHeaderReqSecWebSocketProtocol::HasProtocol(_In_z_ LPCSTR szProtocolA, _In_ SIZE_T nProtocolLen) const
{
  SIZE_T i, nCount;

  if (nProtocolLen == (SIZE_T)-1)
    nProtocolLen = StrLenA(szProtocolA);
  if (nProtocolLen > 0)
  {
    nCount = aProtocolsList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      LPCSTR sA = aProtocolsList.GetElementAt(i);

      if (StrNCompareA(sA, szProtocolA, nProtocolLen, TRUE) == 0 && sA[nProtocolLen] == 0)
        return TRUE;
    }
  }
  return FALSE;
}

} //namespace MX

//-----------------------------------------------------------

static int ProtocolCompareFunc(_In_ LPVOID lpContext, _In_ LPSTR *lpElem1, _In_ LPSTR *lpElem2)
{
  return MX::StrCompareA(*lpElem1, *lpElem2, TRUE);
}
