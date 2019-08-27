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
#include "..\..\Include\Http\HttpHeaderEntContentType.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderEntContentType::CHttpHeaderEntContentType() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderEntContentType::~CHttpHeaderEntContentType()
{
  cParamsList.RemoveAllElements();
  return;
}

HRESULT CHttpHeaderEntContentType::Parse(_In_z_ LPCSTR szValueA)
{
  LPCSTR szStartA;
  CStringA cStrTokenA;
  CStringW cStrValueW;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //mime type
  szValueA = SkipUntil(szStartA = szValueA, ";, \t");
  if (szValueA == szStartA)
    return MX_E_InvalidData;
  hRes = SetType(szStartA, (SIZE_T)(szValueA - szStartA));
  if (FAILED(hRes))
    return hRes;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //parameters
  if (*szValueA == ';')
  {
    szValueA++;
    do
    {
      BOOL bExtendedParam;

      //skip spaces
      szValueA = SkipSpaces(szValueA);
      if (*szValueA == 0)
        break;

      //get parameter
      hRes = GetParamNameAndValue(cStrTokenA, cStrValueW, szValueA, &bExtendedParam);
      if (FAILED(hRes))
        return (hRes == MX_E_NoData) ? MX_E_InvalidData : hRes;

      //add parameter
      hRes = AddParam((LPCSTR)cStrTokenA, (LPCWSTR)cStrValueW);
      if (FAILED(hRes))
        return hRes;

      //check for separator or end
      if (*szValueA == ';')
        szValueA++;
      else if (*szValueA != 0)
        return MX_E_InvalidData;
    }
    while (*szValueA != 0);
  }
  //check for separator or end
  if (*SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentType::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  SIZE_T i, nCount;
  CStringA cStrTempA;

  if (cStrTypeA.IsEmpty() != FALSE)
  {
    cStrDestA.Empty();
    return S_OK;
  }
  if (cStrDestA.Copy((LPCSTR)cStrTypeA) == FALSE)
    return E_OUTOFMEMORY;
  //parameters
  nCount = cParamsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (CHttpCommon::BuildQuotedString(cStrTempA, cParamsList[i]->szValueW, StrLenW(cParamsList[i]->szValueW),
                                       FALSE) == FALSE)
    {
      return E_OUTOFMEMORY;
    }
    if (cStrDestA.AppendFormat("; %s=%s", cParamsList[i]->szNameA, (LPCSTR)cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentType::SetType(_In_z_ LPCSTR szTypeA, _In_ SIZE_T nTypeLen)
{
  LPCSTR szStartA, szTypeEndA;

  if (nTypeLen == (SIZE_T)-1)
    nTypeLen = StrLenA(szTypeA);
  if (nTypeLen == 0)
    return MX_E_InvalidData;
  if (szTypeA == NULL)
    return E_POINTER;
  szTypeEndA = szTypeA + nTypeLen;
  szStartA = szTypeA;
  //get mime type
  szTypeA = GetToken(szTypeA, nTypeLen);
  //check slash separator
  if (szTypeA >= szTypeEndA || *szTypeA++ != '/')
    return MX_E_InvalidData;
  //get mime subtype
  szTypeA = GetToken(szTypeA, (SIZE_T)(szTypeEndA - szTypeA));
  if (*(szTypeA-1) == '/' || (szTypeA < szTypeEndA && *szTypeA == '/')) //no subtype?
    return MX_E_InvalidData;
  //check for end
  if (szTypeA != szTypeEndA)
    return MX_E_InvalidData;
  //set new value
  if (cStrTypeA.CopyN(szStartA, nTypeLen) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCSTR CHttpHeaderEntContentType::GetType() const
{
  return (LPCSTR)cStrTypeA;
}

HRESULT CHttpHeaderEntContentType::AddParam(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW)
{
  TAutoFreePtr<PARAMETER> cNewParam;
  LPCSTR szStartA;
  SIZE_T nNameLen, nValueLen;

  if (szNameA == NULL)
    return E_POINTER;
  if (szValueW == NULL)
    szValueW = L"";
  //get token
  szNameA = CHttpHeaderBase::GetToken(szStartA = szNameA);
  if (szStartA == szNameA || *szNameA != 0)
    return MX_E_InvalidData;
  //get name and value length
  nNameLen = (SIZE_T)(szNameA - szStartA);
  nValueLen = (StrLenW(szValueW) + 1) * sizeof(WCHAR);
  //create new item
  cNewParam.Attach((LPPARAMETER)MX_MALLOC(sizeof(PARAMETER) + nNameLen + nValueLen));
  if (!cNewParam)
    return E_OUTOFMEMORY;
  MemCopy(cNewParam->szNameA, szStartA, nNameLen);
  cNewParam->szNameA[nNameLen] = 0;
  cNewParam->szValueW = (LPWSTR)((LPBYTE)(cNewParam->szNameA) + (nNameLen + 1));
  MemCopy(cNewParam->szValueW, szValueW, nValueLen);
  //add to list
  if (cParamsList.AddElement(cNewParam.Get()) == FALSE)
    return E_OUTOFMEMORY;
  cNewParam.Detach();
  //done
  return S_OK;
}

SIZE_T CHttpHeaderEntContentType::GetParamsCount() const
{
  return cParamsList.GetCount();
}

LPCSTR CHttpHeaderEntContentType::GetParamName(_In_ SIZE_T nIndex) const
{
  return (nIndex < cParamsList.GetCount()) ? cParamsList[nIndex]->szNameA : NULL;
}

LPCWSTR CHttpHeaderEntContentType::GetParamValue(_In_ SIZE_T nIndex) const
{
  return (nIndex < cParamsList.GetCount()) ? cParamsList[nIndex]->szValueW : NULL;
}

LPCWSTR CHttpHeaderEntContentType::GetParamValue(_In_z_ LPCSTR szNameA) const
{
  SIZE_T i, nCount;

  if (szNameA != NULL && *szNameA != 0)
  {
    nCount = cParamsList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      if (StrCompareA(cParamsList[i]->szNameA, szNameA, TRUE) == 0)
        return cParamsList[i]->szValueW;
    }
  }
  return NULL;
}

} //namespace MX
