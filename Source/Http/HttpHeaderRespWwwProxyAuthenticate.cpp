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
#include "..\..\Include\Http\HttpHeaderRespWwwProxyAuthenticate.h"
#include "..\..\Include\AutoPtr.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderRespWwwProxyAuthenticateCommon::CHttpHeaderRespWwwProxyAuthenticateCommon() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderRespWwwProxyAuthenticateCommon::~CHttpHeaderRespWwwProxyAuthenticateCommon()
{
  aParamsList.RemoveAllElements();
  return;
}

HRESULT CHttpHeaderRespWwwProxyAuthenticateCommon::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  LPCSTR szValueEndA, szStartA;
  CStringA cStrTokenA;
  CStringW cStrValueW;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  szValueEndA = szValueA + nValueLen;

  //skip spaces
  szValueA = SkipSpaces(szValueA, szValueEndA);

  //scheme
  szValueA = SkipUntil(szStartA = szValueA, szValueEndA, ";, \t");
  if (szValueA == szStartA)
    return MX_E_InvalidData;

  hRes = SetScheme(szStartA, (SIZE_T)(szValueA - szStartA));
  if (FAILED(hRes))
    return hRes;

  //skip spaces
  szValueA = SkipSpaces(szValueA, szValueEndA);

  //parameters
  if (szValueA < szValueEndA)
  {
    szValueA++;
    do
    {
      BOOL bExtendedParam;

      //skip spaces
      szValueA = SkipSpaces(szValueA, szValueEndA);
      if (szValueA >= szValueEndA)
        break;

      //get parameter
      hRes = GetParamNameAndValue(TRUE, cStrTokenA, cStrValueW, szValueA, szValueEndA, &bExtendedParam);
      if (FAILED(hRes))
        return (hRes == MX_E_NoData) ? MX_E_InvalidData : hRes;

      //add parameter
      hRes = AddParam((LPCSTR)cStrTokenA, (LPCWSTR)cStrValueW);
      if (FAILED(hRes))
        return hRes;

      //check for separator or end
      if (szValueA < szValueEndA)
      {
        if (*szValueA == ';' || *szValueA == ',')
          szValueA++;
        else
          return MX_E_InvalidData;
      }
    }
    while (szValueA < szValueEndA);
  }

  //check for separator or end
  if (SkipSpaces(szValueA, szValueEndA) != szValueEndA)
    return MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpHeaderRespWwwProxyAuthenticateCommon::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  SIZE_T i, nCount;
  CStringA cStrTempA;

  if (cStrSchemeA.IsEmpty() != FALSE)
  {
    cStrDestA.Empty();
    return S_OK;
  }
  if (cStrDestA.Copy((LPCSTR)cStrSchemeA) == FALSE)
    return E_OUTOFMEMORY;

  //parameters
  nCount = aParamsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (Http::BuildQuotedString(cStrTempA, aParamsList[i]->szValueW, StrLenW(aParamsList[i]->szValueW), TRUE) == FALSE)
      return E_OUTOFMEMORY;
    if (cStrDestA.AppendFormat(" %s=%s", aParamsList[i]->szNameA, (LPCSTR)cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

HRESULT CHttpHeaderRespWwwProxyAuthenticateCommon::SetScheme(_In_z_ LPCSTR szSchemeA, _In_opt_ SIZE_T nSchemeLen)
{
  LPCSTR szStartA, szSchemeEndA;

  if (nSchemeLen == (SIZE_T)-1)
    nSchemeLen = StrLenA(szSchemeA);
  if (nSchemeLen == 0)
    return MX_E_InvalidData;
  if (szSchemeA == NULL)
    return E_POINTER;
  szSchemeEndA = szSchemeA + nSchemeLen;

  //get mime type
  szStartA = szSchemeA;
  szSchemeA = GetToken(szSchemeA, szSchemeEndA);

  //check for end
  if (szSchemeA != szSchemeEndA)
    return MX_E_InvalidData;

  //set new value
  if (cStrSchemeA.CopyN(szStartA, nSchemeLen) == FALSE)
    return E_OUTOFMEMORY;

  //done
  return S_OK;
}

LPCSTR CHttpHeaderRespWwwProxyAuthenticateCommon::GetScheme() const
{
  return (LPCSTR)cStrSchemeA;
}

HRESULT CHttpHeaderRespWwwProxyAuthenticateCommon::AddParam(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW)
{
  TAutoFreePtr<PARAMETER> cNewParam;
  LPCSTR szStartA;
  SIZE_T nNameLen, nValueLen;

  if (szNameA == NULL)
    return E_POINTER;
  if (szValueW == NULL)
    szValueW = L"";

  nNameLen = StrLenA(szNameA);

  //get token
  szNameA = GetToken(szStartA = szNameA, szNameA + nNameLen);
  if (szStartA == szNameA || szNameA != szStartA + nNameLen)
    return MX_E_InvalidData;

  //get value length
  nValueLen = (StrLenW(szValueW) + 1) * sizeof(WCHAR);

  //create new item
  cNewParam.Attach((LPPARAMETER)MX_MALLOC(sizeof(PARAMETER) + nNameLen + nValueLen));
  if (!cNewParam)
    return E_OUTOFMEMORY;
  MxMemCopy(cNewParam->szNameA, szStartA, nNameLen);
  cNewParam->szNameA[nNameLen] = 0;
  cNewParam->szValueW = (LPWSTR)((LPBYTE)(cNewParam->szNameA) + (nNameLen + 1));
  MxMemCopy(cNewParam->szValueW, szValueW, nValueLen);

  //add to list
  if (aParamsList.AddElement(cNewParam.Get()) == FALSE)
    return E_OUTOFMEMORY;
  cNewParam.Detach();

  //done
  return S_OK;
}

SIZE_T CHttpHeaderRespWwwProxyAuthenticateCommon::GetParamsCount() const
{
  return aParamsList.GetCount();
}

LPCSTR CHttpHeaderRespWwwProxyAuthenticateCommon::GetParamName(_In_ SIZE_T nIndex) const
{
  return (nIndex < aParamsList.GetCount()) ? aParamsList[nIndex]->szNameA : NULL;
}

LPCWSTR CHttpHeaderRespWwwProxyAuthenticateCommon::GetParamValue(_In_ SIZE_T nIndex) const
{
  return (nIndex < aParamsList.GetCount()) ? aParamsList[nIndex]->szValueW : NULL;
}

LPCWSTR CHttpHeaderRespWwwProxyAuthenticateCommon::GetParamValue(_In_z_ LPCSTR szNameA) const
{
  SIZE_T i, nCount;

  if (szNameA != NULL && *szNameA != 0)
  {
    nCount = aParamsList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      if (StrCompareA(aParamsList[i]->szNameA, szNameA, TRUE) == 0)
        return aParamsList[i]->szValueW;
    }
  }
  return NULL;
}

//--------

CHttpHeaderRespWwwAuthenticate::CHttpHeaderRespWwwAuthenticate() : CHttpHeaderRespWwwProxyAuthenticateCommon()
{
  return;
}

CHttpHeaderRespWwwAuthenticate::~CHttpHeaderRespWwwAuthenticate()
{
  return;
}

//--------

CHttpHeaderRespProxyAuthenticate::CHttpHeaderRespProxyAuthenticate() : CHttpHeaderRespWwwProxyAuthenticateCommon()
{
  return;
}

CHttpHeaderRespProxyAuthenticate::~CHttpHeaderRespProxyAuthenticate()
{
  return;
}

} //namespace MX
