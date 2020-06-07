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
#include "..\..\Include\Http\HttpHeaderGenSecWebSocketExtensions.h"
#include "..\..\Include\AutoPtr.h"

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

HRESULT CHttpHeaderGenSecWebSocketExtensions::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  CExtension *lpExtension;
  LPCSTR szValueEndA, szStartA;
  CStringA cStrTokenA;
  CStringW cStrValueW;
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

    //extension
    szValueA = SkipUntil(szStartA = szValueA, szValueEndA, ";, \t");
    if (szValueA == szStartA)
      goto skip_null_listitem;

    bGotItem = TRUE;

    //add extension to list
    hRes = AddExtension(szStartA, (SIZE_T)(szValueA - szStartA), &lpExtension);
    if (FAILED(hRes))
      return hRes;

    //skip spaces
    szValueA = SkipSpaces(szValueA, szValueEndA);

    //parameters
    if (szValueA < szValueEndA && *szValueA == ';')
    {
      szValueA++;
      do
      {
        //skip spaces
        szValueA = SkipSpaces(szValueA, szValueEndA);
        if (szValueA >= szValueEndA || *szValueA == ',')
          break;
        if (*szValueA == ';')
          goto skip_null_listitem2;

        //get parameter
        hRes = GetParamNameAndValue(TRUE, cStrTokenA, cStrValueW, szValueA, szValueEndA);
        if (FAILED(hRes) && hRes != MX_E_NoData)
          return hRes;

        hRes = lpExtension->AddParam((LPCSTR)cStrTokenA, (LPCWSTR)cStrValueW);
        if (FAILED(hRes))
          return hRes;

        //skip spaces
        szValueA = SkipSpaces(szValueA, szValueEndA);

skip_null_listitem2:
        //check for separator or end
        if (szValueA < szValueEndA)
        {
          if (*szValueA == ';')
            szValueA++;
          else if (*szValueA != ',')
            return MX_E_InvalidData;
        }
      }
      while (szValueA < szValueEndA && *szValueA != ',');
    }

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
  while (szValueA < szValueEndA );

  //do we got one?
  if (bGotItem == FALSE)
    return MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpHeaderGenSecWebSocketExtensions::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
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
      if (Http::BuildQuotedString(cStrTempA, lpExtension->GetParamValue(nParamIdx),
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

HRESULT CHttpHeaderGenSecWebSocketExtensions::Merge(_In_ CHttpHeaderBase *_lpHeader)
{
  CHttpHeaderGenSecWebSocketExtensions *lpHeader = reinterpret_cast<CHttpHeaderGenSecWebSocketExtensions*>(_lpHeader);
  SIZE_T i, nCount, nThisIndex, nThisCount;

  nCount = lpHeader->cExtensionsList.GetCount();
  nThisCount = cExtensionsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    CExtension *lpExtension = lpHeader->cExtensionsList.GetElementAt(i);

    for (nThisIndex = 0; nThisIndex < nThisCount; nThisIndex++)
    {
      if (StrCompareA(cExtensionsList[nThisIndex]->GetExtension(), lpExtension->GetExtension(), TRUE) == 0)
        break;
    }
    if (nThisIndex < nThisCount)
    {
      //have a match, add/replace parameters
      CExtension *lpThisExtension = cExtensionsList.GetElementAt(nThisIndex);
      SIZE_T nParamsIndex, nParamsCount, nThisParamsIndex, nThisParamsCount;

      nThisParamsCount = lpThisExtension->aParamsList.GetCount();
      nParamsCount = lpExtension->aParamsList.GetCount();
      for (nParamsIndex = 0; nParamsIndex < nParamsCount; nParamsIndex++)
      {
        CExtension::LPPARAMETER lpParam = lpExtension->aParamsList.GetElementAt(nParamsIndex);
        TAutoFreePtr<CExtension::PARAMETER> cNewParam;
        SIZE_T nNameLen, nValueLen;

        for (nThisParamsIndex = 0; nThisParamsIndex < nThisParamsCount; nThisParamsIndex++)
        {
          CExtension::LPPARAMETER lpThisParam = lpThisExtension->aParamsList.GetElementAt(nThisParamsIndex);

          if (StrCompareA(lpParam->szNameA, lpThisParam->szNameA, TRUE) == 0)
            break;
        }
        if (nThisParamsIndex < nThisParamsCount)
        {
          //parameter was found, replace
          lpThisExtension->aParamsList.RemoveElementAt(nThisIndex);
        }

        //add the source parameter

        //get name and value length
        nNameLen = StrLenA(lpParam->szNameA) + 1;
        nValueLen = (StrLenW(lpParam->szValueW) + 1) * sizeof(WCHAR);

        //create new item
        cNewParam.Attach((CExtension::LPPARAMETER)MX_MALLOC(sizeof(CExtension::PARAMETER) + nNameLen + nValueLen));
        if (!cNewParam)
          return E_OUTOFMEMORY;
        ::MxMemCopy(cNewParam->szNameA, lpParam->szNameA, nNameLen);
        cNewParam->szNameA[nNameLen] = 0;
        cNewParam->szValueW = (LPWSTR)((LPBYTE)(cNewParam->szNameA) + (nNameLen + 1));
        ::MxMemCopy(cNewParam->szValueW, lpParam->szValueW, nValueLen);

        if (lpThisExtension->aParamsList.AddElement(cNewParam.Get()) == FALSE)
          return E_OUTOFMEMORY;
        cNewParam.Detach();
      }
    }
    else
    {
      TAutoDeletePtr<CHttpHeaderGenSecWebSocketExtensions::CExtension> cNewExtension;

      cNewExtension.Attach(MX_DEBUG_NEW CExtension());
      if (!cNewExtension)
        return E_OUTOFMEMORY;
      try
      {
        *(cNewExtension.Get()) = *lpExtension;
      }
      catch (LONG hr)
      {
        return (HRESULT)hr;
      }

      //add to list
      if (cExtensionsList.AddElement(cNewExtension.Get()) == FALSE)
        return E_OUTOFMEMORY;
      cNewExtension.Detach();
      nThisCount++;
    }
  }
  return S_OK;
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

CHttpHeaderGenSecWebSocketExtensions::CExtension &
CHttpHeaderGenSecWebSocketExtensions::CExtension::operator=(_In_ const CExtension &cSrc) throw(...)
{
  if (&cSrc != this)
  {
    CStringA cStrTempExtensionA;
    TArrayListWithFree<LPPARAMETER> cTempParamsList;
    SIZE_T i, nCount;

    if (cStrTempExtensionA.Copy(cSrc.GetExtension()) == FALSE)
      throw (LONG)E_OUTOFMEMORY;
    nCount = cSrc.aParamsList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      LPPARAMETER lpParam = cSrc.aParamsList.GetElementAt(i);
      TAutoFreePtr<PARAMETER> cNewParam;
      SIZE_T nNameLen, nValueLen;

      //get name and value length
      nNameLen = StrLenA(lpParam->szNameA) + 1;
      nValueLen = (StrLenW(lpParam->szValueW) + 1) * sizeof(WCHAR);

      //create new item
      cNewParam.Attach((LPPARAMETER)MX_MALLOC(sizeof(PARAMETER) + nNameLen + nValueLen));
      if (!cNewParam)
        throw (LONG)E_OUTOFMEMORY;
      ::MxMemCopy(cNewParam->szNameA, lpParam->szNameA, nNameLen);
      cNewParam->szNameA[nNameLen] = 0;
      cNewParam->szValueW = (LPWSTR)((LPBYTE)(cNewParam->szNameA) + (nNameLen + 1));
      ::MxMemCopy(cNewParam->szValueW, lpParam->szValueW, nValueLen);

      if (cTempParamsList.AddElement(cNewParam.Get()) == FALSE)
        throw (LONG)E_OUTOFMEMORY;
      cNewParam.Detach();
    }

    cStrExtensionA.Attach(cStrTempExtensionA.Detach());
    aParamsList.Attach(cTempParamsList.Detach(), nCount);
  }
  return *this;
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
  szExtensionA = CHttpHeaderBase::GetToken(szExtensionA, szExtensionA + nExtensionLen);
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

  nNameLen = StrLenA(szNameA);

  //get token
  szNameA = CHttpHeaderBase::GetToken(szStartA = szNameA, szNameA + nNameLen);
  if (szStartA == szNameA || szNameA != szStartA + nNameLen)
    return MX_E_InvalidData;

  //get value length
  nValueLen = (StrLenW(szValueW) + 1) * sizeof(WCHAR);
  //create new item
  cNewParam.Attach((LPPARAMETER)MX_MALLOC(sizeof(PARAMETER) + nNameLen + nValueLen));
  if (!cNewParam)
    return E_OUTOFMEMORY;
  ::MxMemCopy(cNewParam->szNameA, szStartA, nNameLen);
  cNewParam->szNameA[nNameLen] = 0;
  cNewParam->szValueW = (LPWSTR)((LPBYTE)(cNewParam->szNameA) + (nNameLen + 1));
  ::MxMemCopy(cNewParam->szValueW, szValueW, nValueLen);

  //add to list
  if (aParamsList.AddElement(cNewParam.Get()) == FALSE)
    return E_OUTOFMEMORY;
  cNewParam.Detach();

  //done
  return S_OK;
}

SIZE_T CHttpHeaderGenSecWebSocketExtensions::CExtension::GetParamsCount() const
{
  return aParamsList.GetCount();
}

LPCSTR CHttpHeaderGenSecWebSocketExtensions::CExtension::GetParamName(_In_ SIZE_T nIndex) const
{
  return (nIndex < aParamsList.GetCount()) ? aParamsList[nIndex]->szNameA : NULL;
}

LPCWSTR CHttpHeaderGenSecWebSocketExtensions::CExtension::GetParamValue(_In_ SIZE_T nIndex) const
{
  return (nIndex < aParamsList.GetCount()) ? aParamsList[nIndex]->szValueW : NULL;
}

LPCWSTR CHttpHeaderGenSecWebSocketExtensions::CExtension::GetParamValue(_In_z_ LPCSTR szNameA) const
{
  SIZE_T i, nCount;

  if (szNameA != NULL && szNameA[0] != 0)
  {
    nCount = aParamsList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (StrCompareA(aParamsList[i]->szNameA, szNameA, TRUE) == 0)
        return aParamsList[i]->szValueW;
    }
  }
  return NULL;
}

} //namespace MX
