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
#include "..\..\Include\Http\HttpHeaderRespSecWebSocketVersion.h"

//-----------------------------------------------------------

static int VersionCompareFunc(_In_ LPVOID lpContext, _In_ int *lpElem1, _In_ int *lpElem2);

//-----------------------------------------------------------

namespace MX {

CHttpHeaderRespSecWebSocketVersion::CHttpHeaderRespSecWebSocketVersion() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderRespSecWebSocketVersion::~CHttpHeaderRespSecWebSocketVersion()
{
  return;
}

HRESULT CHttpHeaderRespSecWebSocketVersion::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  LPCSTR szValueEndA;
  BOOL bGotItem;
  int nVersion;
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
    if (*szValueA == ',')
      goto skip_null_listitem;

    bGotItem = TRUE;

    //get value
    nVersion = 0;
    while (szValueA < szValueEndA && *szValueA >= '0' && *szValueA <= '9')
    {
      nVersion = nVersion * 10 + (int)((*szValueA) - '0');
      if (nVersion > 255)
        return MX_E_InvalidData;
      szValueA++;
    }
    hRes = AddVersion(nVersion);
    if (FAILED(hRes))
      return hRes;

    //skip spaces
    szValueA = SkipSpaces(szValueA, szValueEndA);

skip_null_listitem:
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

HRESULT CHttpHeaderRespSecWebSocketVersion::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  SIZE_T i, nCount;

  cStrDestA.Empty();

  nCount = cVersionsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (cStrDestA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(",", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrDestA.AppendFormat("%ld", cVersionsList.GetElementAt(i)) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderRespSecWebSocketVersion::AddVersion(_In_ int nVersion)
{
  if (nVersion < 13 || nVersion > 255)
    return E_INVALIDARG;

  //add version to list
  if (cVersionsList.SortedInsert(nVersion, &VersionCompareFunc, NULL, TRUE) == FALSE)
    return E_OUTOFMEMORY;

  //done
  return S_OK;
}

SIZE_T CHttpHeaderRespSecWebSocketVersion::GetVersionsCount() const
{
  return cVersionsList.GetCount();
}

int CHttpHeaderRespSecWebSocketVersion::GetVersion(_In_ SIZE_T nIndex) const
{
  return (nIndex < cVersionsList.GetCount()) ? cVersionsList.GetElementAt(nIndex) : -1;
}


HRESULT CHttpHeaderRespSecWebSocketVersion::Merge(_In_ CHttpHeaderBase *_lpHeader)
{
  CHttpHeaderRespSecWebSocketVersion *lpHeader = reinterpret_cast<CHttpHeaderRespSecWebSocketVersion*>(_lpHeader);
  SIZE_T i, nCount;
  HRESULT hRes;

  nCount = lpHeader->GetVersionsCount();
  for (i = 0; i < nCount; i++)
  {
    hRes = AddVersion(lpHeader->GetVersion(i));
    if (FAILED(hRes))
      return hRes;
  }

  //done
  return S_OK;
}

} //namespace MX

//-----------------------------------------------------------

static int VersionCompareFunc(_In_ LPVOID lpContext, _In_ int *lpElem1, _In_ int *lpElem2)
{
  //DESC order
  if ((*lpElem1) > (*lpElem2))
    return -1;
  if ((*lpElem1) < (*lpElem2))
    return 1;
  return 0;
}
