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
  CStringA cStrTempA, cStrTempA_2;
  CStringW cStrTempW;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //mime type
  szValueA = Advance(szStartA = szValueA, ";, \t");
  if (cStrTempA.CopyN(szStartA, (SIZE_T)(szValueA-szStartA)) == FALSE)
    return E_OUTOFMEMORY;
  if (szValueA == szStartA)
    return MX_E_InvalidData;
  hRes = SetType((LPCSTR)cStrTempA);
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
      //skip spaces
      szValueA = SkipSpaces(szValueA);
      if (*szValueA == 0)
        break;
      //parse name
      szValueA = GetToken(szStartA = szValueA);
      if (cStrTempA.CopyN(szStartA, (SIZE_T)(szValueA-szStartA)) == FALSE)
        return E_OUTOFMEMORY;
      //skip spaces
      szValueA = SkipSpaces(szValueA);
      //is equal sign?
      if (*szValueA++ != '=')
        return MX_E_InvalidData;
      //skip spaces
      szValueA = SkipSpaces(szValueA);
      //parse value
      if (*szValueA == '"')
      {
        szValueA = Advance(szStartA = ++szValueA, "\"");
        if (*szValueA != '"')
          return MX_E_InvalidData;
        if (cStrTempA_2.CopyN(szStartA, (SIZE_T)(szValueA-szStartA)) == FALSE)
          return E_OUTOFMEMORY;
        szValueA++;
      }
      else
      {
        szValueA = Advance(szStartA = szValueA, " \t;,");
        if (cStrTempA_2.CopyN(szStartA, (SIZE_T)(szValueA-szStartA)) == FALSE)
          return E_OUTOFMEMORY;
      }
      //add parameter
      hRes = Utf8_Decode(cStrTempW, (LPSTR)cStrTempA_2);
      if (SUCCEEDED(hRes))
        hRes = AddParam((LPSTR)cStrTempA, (LPWSTR)cStrTempW);
      if (FAILED(hRes))
        return hRes;
      //skip spaces
      szValueA = SkipSpaces(szValueA);
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

HRESULT CHttpHeaderEntContentType::Build(_Inout_ CStringA &cStrDestA)
{
  SIZE_T i, nCount;
  LPWSTR sW;
  CStringA cStrTempA;
  HRESULT hRes;

  if (cStrTypeA.IsEmpty() != FALSE)
    return S_OK;
  if (cStrDestA.Copy((LPCSTR)cStrTypeA) == FALSE)
    return E_OUTOFMEMORY;
  //parameters
  nCount = cParamsList.GetCount();
  for (i=0; i<nCount; i++)
  {
    if (cStrDestA.AppendFormat("; %s=", cParamsList[i]->szNameA) == FALSE)
      return E_OUTOFMEMORY;
    sW = cParamsList[i]->szValueW;
    while (*sW != 0)
    {
      if (*sW < 33 || *sW > 126 || CHttpCommon::IsValidNameChar((CHAR)(*sW)) == FALSE)
        break;
      sW++;
    }
    if (*sW == 0)
    {
      if (cStrDestA.AppendFormat("%S", cParamsList[i]->szValueW) == FALSE)
        return E_OUTOFMEMORY;
    }
    else
    {
      hRes = Utf8_Encode(cStrTempA, cParamsList[i]->szValueW);
      if (FAILED(hRes))
        return hRes;
      if (CHttpCommon::EncodeQuotedString(cStrTempA) == FALSE)
        return E_OUTOFMEMORY;
      if (cStrDestA.AppendFormat("\"%s\"", (LPCSTR)cStrTempA) == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentType::SetType(_In_z_ LPCSTR szTypeA)
{
  LPCSTR szStartA, szEndA;

  if (szTypeA == NULL)
    return E_POINTER;
  //skip spaces
  szTypeA = SkipSpaces(szTypeA);
  //get mime type
  szTypeA = GetToken(szStartA = szTypeA);
  //check slash separator
  if (*szTypeA++ != '/')
    return MX_E_InvalidData;
  //get mime subtype
  szTypeA = GetToken(szTypeA);
  if (*(szTypeA-1) == '/' || *szTypeA == '/') //no subtype?
    return MX_E_InvalidData;
  szEndA = szTypeA;
  //skip spaces and check for end
  if (*SkipSpaces(szTypeA) != 0)
    return MX_E_InvalidData;
  //set new value
  if (cStrTypeA.CopyN(szStartA, (SIZE_T)(szEndA-szStartA)) == FALSE)
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
  LPCSTR szStartA, szEndA;
  SIZE_T nNameLen, nValueLen;

  if (szNameA == NULL)
    return E_POINTER;
  if (szValueW == NULL)
    szValueW = L"";
  //skip spaces
  szNameA = CHttpHeaderBase::SkipSpaces(szNameA);
  //get token
  szNameA = CHttpHeaderBase::GetToken(szStartA = szNameA);
  if (szStartA == szNameA)
    return MX_E_InvalidData;
  szEndA = szNameA;
  //skip spaces and check for end
  if (*CHttpHeaderBase::SkipSpaces(szNameA) != 0)
    return MX_E_InvalidData;
  //get name and value length
  nNameLen = (SIZE_T)(szEndA-szStartA);
  nValueLen = (StrLenW(szValueW) + 1) * sizeof(WCHAR);
  //create new item
  cNewParam.Attach((LPPARAMETER)MX_MALLOC(sizeof(PARAMETER) + nNameLen + nValueLen));
  if (!cNewParam)
    return E_OUTOFMEMORY;
  MemCopy(cNewParam->szNameA, szStartA, nNameLen);
  cNewParam->szNameA[nNameLen] = 0;
  cNewParam->szValueW = (LPWSTR)((LPBYTE)(cNewParam->szNameA) + (nNameLen+1));
  MemCopy(cNewParam->szValueW, szValueW, nValueLen);
  //add to list
  if (cParamsList.AddElement(cNewParam.Get()) == FALSE)
    return E_OUTOFMEMORY;
  //done
  cNewParam.Detach();
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

  if (szNameA != NULL && szNameA[0] != 0)
  {
    nCount = cParamsList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (StrCompareA(cParamsList[i]->szNameA, szNameA, TRUE) == 0)
        return cParamsList[i]->szValueW;
    }
  }
  return NULL;
}

} //namespace MX
