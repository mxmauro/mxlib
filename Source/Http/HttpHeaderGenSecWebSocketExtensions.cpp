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
#include "..\..\Include\Http\HttpHeaderGenSecWebSocketExtensions.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderGenSecWebSocketExtensions::CHttpHeaderGenSecWebSocketExtensions() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderGenSecWebSocketExtensions::~CHttpHeaderGenSecWebSocketExtensions()
{
  return;
}

HRESULT CHttpHeaderGenSecWebSocketExtensions::Parse(_In_z_ LPCSTR szValueA)
{
  CExtension *lpExtension;
  LPCSTR szStartA;
  CStringA cStrTokenA;
  CStringW cStrValueW;
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

    //extension
    szValueA = SkipUntil(szStartA = szValueA, ";, \t");
    if (szValueA == szStartA)
      goto skip_null_listitem;

    bGotItem = TRUE;

    //add extension to list
    hRes = AddExtension(szStartA, (SIZE_T)(szValueA - szStartA), &lpExtension);
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

        hRes = lpExtension->AddParam((LPCSTR)cStrTokenA, (LPCWSTR)cStrValueW);
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

HRESULT CHttpHeaderGenSecWebSocketExtensions::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  CStringA cStrTempA;
  SIZE_T i, nCount, nParamIdx, nParamsCount;
  CExtension *lpExtension;

  cStrDestA.Empty();
  nCount = cExtensionsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    lpExtension = cExtensionsList.GetElementAt(i);
    if (cStrDestA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(",", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrDestA.Concat(lpExtension->GetExtension()) == FALSE)
      return E_OUTOFMEMORY;
    //parameters
    nParamsCount = lpExtension->GetParamsCount();
    for (nParamIdx = 0; nParamIdx < nParamsCount; nParamIdx++)
    {
      if (CHttpCommon::BuildQuotedString(cStrTempA, lpExtension->GetParamValue(nParamIdx),
                                         StrLenW(lpExtension->GetParamValue(nParamIdx)), FALSE) == FALSE)
      {
        return E_OUTOFMEMORY;
      }
      if (cStrDestA.AppendFormat(";%s=%s", lpExtension->GetParamName(nParamIdx), (LPCSTR)cStrTempA) == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderGenSecWebSocketExtensions::AddExtension(_In_z_ LPCSTR szExtensionA, _In_opt_ SIZE_T nExtensionLen,
                                                           _Out_opt_ CExtension **lplpExtension)
{
  TAutoDeletePtr<CExtension> cNewExtension;
  SIZE_T i, nCount;
  HRESULT hRes;

  if (lplpExtension != NULL)
    *lplpExtension = NULL;
  if (nExtensionLen == (SIZE_T)-1)
    nExtensionLen = StrLenA(szExtensionA);
  if (nExtensionLen == 0)
    return MX_E_InvalidData;
  if (szExtensionA == NULL)
    return E_POINTER;
  //create new type
  cNewExtension.Attach(MX_DEBUG_NEW CExtension());
  if (!cNewExtension)
    return E_OUTOFMEMORY;
  hRes = cNewExtension->SetExtension(szExtensionA, nExtensionLen);
  if (FAILED(hRes))
    return hRes;
  //check if already exists in list
  nCount = cExtensionsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (StrCompareA(cExtensionsList[i]->GetExtension(), cNewExtension->GetExtension(), TRUE) == 0)
    {
      cExtensionsList.RemoveElementAt(i); //remove previous definition
      break;
    }
  }
  //add to list
  if (cExtensionsList.AddElement(cNewExtension.Get()) == FALSE)
    return E_OUTOFMEMORY;
  //done
  if (lplpExtension != NULL)
    *lplpExtension = cNewExtension.Detach();
  else
    cNewExtension.Detach();
  return S_OK;
}

SIZE_T CHttpHeaderGenSecWebSocketExtensions::GetExtensionsCount() const
{
  return cExtensionsList.GetCount();
}

CHttpHeaderGenSecWebSocketExtensions::CExtension* CHttpHeaderGenSecWebSocketExtensions::
                                                  GetExtension(_In_ SIZE_T nIndex) const
{
  return (nIndex < cExtensionsList.GetCount()) ? cExtensionsList.GetElementAt(nIndex) : NULL;
}

CHttpHeaderGenSecWebSocketExtensions::CExtension* CHttpHeaderGenSecWebSocketExtensions::
                                                  GetExtension(_In_z_ LPCSTR szExtensionA) const
{
  SIZE_T i, nCount;

  if (szExtensionA != NULL && *szExtensionA != 0)
  {
    nCount = cExtensionsList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      if (StrCompareA(cExtensionsList[i]->GetExtension(), szExtensionA, TRUE) == 0)
        return cExtensionsList.GetElementAt(i);
    }
  }
  return NULL;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CHttpHeaderGenSecWebSocketExtensions::CExtension::CExtension() : CBaseMemObj()
{
  return;
}

CHttpHeaderGenSecWebSocketExtensions::CExtension::~CExtension()
{
  return;
}

HRESULT CHttpHeaderGenSecWebSocketExtensions::CExtension::SetExtension(_In_z_ LPCSTR szExtensionA,
                                                                       _In_ SIZE_T nExtensionLen)
{
  LPCSTR szStartA;

  if (nExtensionLen == (SIZE_T)-1)
    nExtensionLen = StrLenA(szExtensionA);
  if (nExtensionLen == 0)
    return MX_E_InvalidData;
  if (szExtensionA == NULL)
    return E_POINTER;
  szStartA = szExtensionA;
  //get extension
  szExtensionA = GetToken(szExtensionA, nExtensionLen);
  //check for end
  if (szExtensionA != szStartA + nExtensionLen)
    return MX_E_InvalidData;
  //set new value
  if (cStrExtensionA.CopyN(szStartA, nExtensionLen) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCSTR CHttpHeaderGenSecWebSocketExtensions::CExtension::GetExtension() const
{
  return (LPCSTR)cStrExtensionA;
}

HRESULT CHttpHeaderGenSecWebSocketExtensions::CExtension::AddParam(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW)
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

SIZE_T CHttpHeaderGenSecWebSocketExtensions::CExtension::GetParamsCount() const
{
  return cParamsList.GetCount();
}

LPCSTR CHttpHeaderGenSecWebSocketExtensions::CExtension::GetParamName(_In_ SIZE_T nIndex) const
{
  return (nIndex < cParamsList.GetCount()) ? cParamsList[nIndex]->szNameA : NULL;
}

LPCWSTR CHttpHeaderGenSecWebSocketExtensions::CExtension::GetParamValue(_In_ SIZE_T nIndex) const
{
  return (nIndex < cParamsList.GetCount()) ? cParamsList[nIndex]->szValueW : NULL;
}

LPCWSTR CHttpHeaderGenSecWebSocketExtensions::CExtension::GetParamValue(_In_z_ LPCSTR szNameA) const
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
