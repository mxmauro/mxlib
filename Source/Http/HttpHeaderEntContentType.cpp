/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2006-2015. All rights reserved.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 *
 * 2. This notice may not be removed or altered from any source distribution.
 *
 * 3. YOU MAY NOT:
 *
 *    a. Modify, translate, adapt, alter, or create derivative works from
 *       this software.
 *    b. Copy (other than one back-up copy), distribute, publicly display,
 *       transmit, sell, rent, lease or otherwise exploit this software.
 *    c. Distribute, sub-license, rent, lease, loan [or grant any third party
 *       access to or use of the software to any third party.
 **/
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
