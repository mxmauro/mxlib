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
#include "..\..\Include\Http\HttpHeaderReqAccept.h"
#include <stdlib.h>

//-----------------------------------------------------------

static VOID RemoveTrailingZeroDecimals(_Inout_ MX::CStringA &cStrA);

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqAccept::CHttpHeaderReqAccept() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderReqAccept::~CHttpHeaderReqAccept()
{
  return;
}

HRESULT CHttpHeaderReqAccept::Parse(_In_z_ LPCSTR szValueA)
{
  CType *lpType;
  LPCSTR szStartA;
  CStringA cStrTokenA;
  CStringW cStrValueW;
  BOOL bGotItem;
  double nDbl;
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

    //type
    szValueA = SkipUntil(szStartA = szValueA, ";, \t");
    if (szValueA == szStartA)
      goto skip_null_listitem;

    bGotItem = TRUE;

    //add type to list
    hRes = AddType(szStartA, (SIZE_T)(szValueA - szStartA), &lpType);
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
        if (*szValueA == 0 || *szValueA == ',')
          break;
        if (*szValueA == ';')
          goto skip_null_listitem2;

        //get parameter
        hRes = GetParamNameAndValue(cStrTokenA, cStrValueW, szValueA);
        if (FAILED(hRes) && hRes != MX_E_NoData)
          return hRes;

        if (StrCompareA((LPCSTR)cStrTokenA, "q", TRUE) == 0)
        {
          if (cStrValueW.IsEmpty() == FALSE)
          {
            hRes = StrToDoubleW((LPCWSTR)cStrValueW, (SIZE_T)-1, &nDbl);
            if (SUCCEEDED(hRes))
              hRes = lpType->SetQ(nDbl);
          }
          else
          {
            hRes = MX_E_InvalidData;
          }
        }
        else
        {
          hRes = lpType->AddParam((LPCSTR)cStrTokenA, (LPCWSTR)cStrValueW);
        }
        if (FAILED(hRes))
          return hRes;

        //skip spaces
        szValueA = SkipSpaces(szValueA);

skip_null_listitem2:
        //check for separator or end
        if (*szValueA == ';')
          szValueA++;
        else if (*szValueA != 0 && *szValueA != ',')
          return MX_E_InvalidData;
      }
      while (*szValueA != 0 && *szValueA != ',');
    }

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

HRESULT CHttpHeaderReqAccept::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  CStringA cStrTempA;
  SIZE_T i, nCount, nParamIdx, nParamsCount;
  CType *lpType;

  cStrDestA.Empty();
  nCount = cTypesList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    lpType = cTypesList.GetElementAt(i);
    if (cStrDestA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(",", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrDestA.Concat(lpType->GetType()) == FALSE)
      return E_OUTOFMEMORY;
    //q
    if (lpType->GetQ() < 1.0 - 0.00000001)
    {
      if (cStrDestA.AppendFormat(";q=%f", lpType->GetQ()) == FALSE)
        return E_OUTOFMEMORY;
      RemoveTrailingZeroDecimals(cStrDestA);
    }
    //parameters
    nParamsCount = lpType->GetParamsCount();
    for (nParamIdx = 0; nParamIdx < nParamsCount; nParamIdx++)
    {
      if (CHttpCommon::BuildQuotedString(cStrTempA, lpType->GetParamValue(nParamIdx),
                                         StrLenW(lpType->GetParamValue(nParamIdx)), FALSE) == FALSE)
      {
        return E_OUTOFMEMORY;
      }
      if (cStrDestA.AppendFormat(";%s=%s", lpType->GetParamName(nParamIdx), (LPCSTR)cStrTempA) == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderReqAccept::AddType(_In_z_ LPCSTR szTypeA, _In_opt_ SIZE_T nTypeLen, _Out_opt_ CType **lplpType)
{
  TAutoDeletePtr<CType> cNewType;
  SIZE_T i, nCount;
  HRESULT hRes;

  if (lplpType != NULL)
    *lplpType = NULL;
  if (nTypeLen == (SIZE_T)-1)
    nTypeLen = StrLenA(szTypeA);
  if (nTypeLen == 0)
    return MX_E_InvalidData;
  if (szTypeA == NULL)
    return E_POINTER;
  //create new type
  cNewType.Attach(MX_DEBUG_NEW CType());
  if (!cNewType)
    return E_OUTOFMEMORY;
  hRes = cNewType->SetType(szTypeA, nTypeLen);
  if (FAILED(hRes))
    return hRes;
  //check if already exists in list
  nCount = cTypesList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (StrCompareA(cTypesList[i]->GetType(), cNewType->GetType(), TRUE) == 0)
    {
      cTypesList.RemoveElementAt(i); //remove previous definition
      break;
    }
  }
  //add to list
  if (cTypesList.AddElement(cNewType.Get()) == FALSE)
    return E_OUTOFMEMORY;
  //done
  if (lplpType != NULL)
    *lplpType = cNewType.Detach();
  else
    cNewType.Detach();
  return S_OK;
}

SIZE_T CHttpHeaderReqAccept::GetTypesCount() const
{
  return cTypesList.GetCount();
}

CHttpHeaderReqAccept::CType* CHttpHeaderReqAccept::GetType(_In_ SIZE_T nIndex) const
{
  return (nIndex < cTypesList.GetCount()) ? cTypesList.GetElementAt(nIndex) : NULL;
}

CHttpHeaderReqAccept::CType* CHttpHeaderReqAccept::GetType(_In_z_ LPCSTR szTypeA) const
{
  SIZE_T i, nCount;

  if (szTypeA != NULL && *szTypeA != 0)
  {
    nCount = cTypesList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      if (StrCompareA(cTypesList[i]->GetType(), szTypeA, TRUE) == 0)
        return cTypesList.GetElementAt(i);
    }
  }
  return NULL;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CHttpHeaderReqAccept::CType::CType() : CBaseMemObj()
{
  q = 1.0;
  return;
}

CHttpHeaderReqAccept::CType::~CType()
{
  return;
}

HRESULT CHttpHeaderReqAccept::CType::SetType(_In_z_ LPCSTR szTypeA, _In_ SIZE_T nTypeLen)
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
  if (*szTypeA != '*')
    szTypeA = GetToken(szTypeA, nTypeLen);
  else
    szTypeA++;
  //check slash separator
  if (szTypeA >= szTypeEndA || *szTypeA++ != '/')
    return MX_E_InvalidData;
  //get mime subtype
  if (*szTypeA != '*')
    szTypeA = GetToken(szTypeA, (SIZE_T)(szTypeEndA - szTypeA));
  else
    szTypeA++;
  if (*(szTypeA - 1) == '/' || (szTypeA < szTypeEndA && *szTypeA == '/')) //no subtype?
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

LPCSTR CHttpHeaderReqAccept::CType::GetType() const
{
  return (LPCSTR)cStrTypeA;
}

HRESULT CHttpHeaderReqAccept::CType::SetQ(_In_ double _q)
{
  if (_q < -0.000001 || _q > 1.000001)
    return E_INVALIDARG;
  q = (_q > 0.000001) ? ((_q < 0.99999) ? _q : 1.0) : 0.0;
  return S_OK;
}

double CHttpHeaderReqAccept::CType::GetQ() const
{
  return q;
}

HRESULT CHttpHeaderReqAccept::CType::AddParam(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW)
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

SIZE_T CHttpHeaderReqAccept::CType::GetParamsCount() const
{
  return cParamsList.GetCount();
}

LPCSTR CHttpHeaderReqAccept::CType::GetParamName(_In_ SIZE_T nIndex) const
{
  return (nIndex < cParamsList.GetCount()) ? cParamsList[nIndex]->szNameA : NULL;
}

LPCWSTR CHttpHeaderReqAccept::CType::GetParamValue(_In_ SIZE_T nIndex) const
{
  return (nIndex < cParamsList.GetCount()) ? cParamsList[nIndex]->szValueW : NULL;
}

LPCWSTR CHttpHeaderReqAccept::CType::GetParamValue(_In_z_ LPCSTR szNameA) const
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

//-----------------------------------------------------------

static VOID RemoveTrailingZeroDecimals(_Inout_ MX::CStringA &cStrA)
{
  SIZE_T nOfs, nCount = 0, nLen = cStrA.GetLength();
  LPCSTR sA = (LPCSTR)cStrA;

  for (nOfs = nLen; nOfs >= 2; nOfs--)
  {
    if (sA[nOfs - 1] != '0' || sA[nOfs - 2] == '.')
      break;
  }
  if (nOfs < nLen)
    cStrA.Delete(nOfs, nLen - nOfs);
  return;
}
