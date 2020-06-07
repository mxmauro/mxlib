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
#include "..\..\Include\Http\HttpHeaderReqAccept.h"
#include <stdlib.h>
#include "..\..\Include\AutoPtr.h"

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

HRESULT CHttpHeaderReqAccept::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  CType *lpType;
  LPCSTR szValueEndA, szStartA;
  CStringA cStrTokenA;
  CStringW cStrValueW;
  BOOL bGotItem;
  double nDbl;
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

    //type
    szValueA = SkipUntil(szStartA = szValueA, szValueEndA, ";, \t");
    if (szValueA == szStartA)
      goto skip_null_listitem;

    bGotItem = TRUE;

    //add type to list
    hRes = AddType(szStartA, (SIZE_T)(szValueA - szStartA), &lpType);
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
  while (szValueA < szValueEndA);

  //do we got one?
  if (bGotItem == FALSE)
    return MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpHeaderReqAccept::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  CStringA cStrTempA;
  SIZE_T i, nCount, nParamIdx, nParamsCount;
  CType *lpType;

  cStrDestA.Empty();
  nCount = aTypesList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    lpType = aTypesList.GetElementAt(i);
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
      if (Http::BuildQuotedString(cStrTempA, lpType->GetParamValue(nParamIdx),
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
  nCount = aTypesList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (StrCompareA(aTypesList[i]->GetType(), cNewType->GetType(), TRUE) == 0)
    {
      aTypesList.RemoveElementAt(i); //remove previous definition
      break;
    }
  }

  //add to list
  if (aTypesList.AddElement(cNewType.Get()) == FALSE)
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
  return aTypesList.GetCount();
}

CHttpHeaderReqAccept::CType* CHttpHeaderReqAccept::GetType(_In_ SIZE_T nIndex) const
{
  return (nIndex < aTypesList.GetCount()) ? aTypesList.GetElementAt(nIndex) : NULL;
}

CHttpHeaderReqAccept::CType* CHttpHeaderReqAccept::GetType(_In_z_ LPCSTR szTypeA) const
{
  SIZE_T i, nCount;

  if (szTypeA != NULL && *szTypeA != 0)
  {
    nCount = aTypesList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      if (StrCompareA(aTypesList[i]->GetType(), szTypeA, TRUE) == 0)
        return aTypesList.GetElementAt(i);
    }
  }
  return NULL;
}

HRESULT CHttpHeaderReqAccept::Merge(_In_ CHttpHeaderBase *_lpHeader)
{
  CHttpHeaderReqAccept *lpHeader = reinterpret_cast<CHttpHeaderReqAccept*>(_lpHeader);
  SIZE_T i, nCount, nThisIndex, nThisCount;

  nCount = lpHeader->aTypesList.GetCount();
  nThisCount = aTypesList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    CType *lpType = lpHeader->aTypesList.GetElementAt(i);

    for (nThisIndex = 0; nThisIndex < nThisCount; nThisIndex++)
    {
      CType *lpThisType = aTypesList.GetElementAt(nThisIndex);

      if (StrCompareA((LPCSTR)(lpType->cStrTypeA), (LPCSTR)(lpThisType->cStrTypeA), TRUE) == 0)
        break;
    }
    if (nThisIndex < nThisCount)
    {
      //have a match, add/replace parameters
      CType *lpThisType = aTypesList.GetElementAt(nThisIndex);
      SIZE_T nParamsIndex, nParamsCount, nThisParamsIndex, nThisParamsCount;

      lpThisType->q = lpType->q;

      nThisParamsCount = lpThisType->aParamsList.GetCount();
      nParamsCount = lpType->aParamsList.GetCount();
      for (nParamsIndex = 0; nParamsIndex < nParamsCount; nParamsIndex++)
      {
        CType::LPPARAMETER lpParam = lpType->aParamsList.GetElementAt(nParamsIndex);
        TAutoFreePtr<CType::PARAMETER> cNewParam;
        SIZE_T nNameLen, nValueLen;

        for (nThisParamsIndex = 0; nThisParamsIndex < nThisParamsCount; nThisParamsIndex++)
        {
          CType::LPPARAMETER lpThisParam = lpThisType->aParamsList.GetElementAt(nThisParamsIndex);

          if (StrCompareA(lpParam->szNameA, lpThisParam->szNameA, TRUE) == 0)
            break;
        }
        if (nThisParamsIndex < nThisParamsCount)
        {
          //parameter was found, replace
          lpThisType->aParamsList.RemoveElementAt(nThisIndex);
        }

        //add the source parameter

        //get name and value length
        nNameLen = StrLenA(lpParam->szNameA) + 1;
        nValueLen = (StrLenW(lpParam->szValueW) + 1) * sizeof(WCHAR);

        //create new item
        cNewParam.Attach((CType::LPPARAMETER)MX_MALLOC(sizeof(CType::PARAMETER) + nNameLen + nValueLen));
        if (!cNewParam)
          return E_OUTOFMEMORY;
        ::MxMemCopy(cNewParam->szNameA, lpParam->szNameA, nNameLen);
        cNewParam->szNameA[nNameLen] = 0;
        cNewParam->szValueW = (LPWSTR)((LPBYTE)(cNewParam->szNameA) + (nNameLen + 1));
        ::MxMemCopy(cNewParam->szValueW, lpParam->szValueW, nValueLen);

        if (lpThisType->aParamsList.AddElement(cNewParam.Get()) == FALSE)
          return E_OUTOFMEMORY;
        cNewParam.Detach();
      }
    }
    else
    {
      TAutoDeletePtr<CType> cNewType;

      //add a duplicate
      cNewType.Attach(MX_DEBUG_NEW CType());
      if (!cNewType)
        return E_OUTOFMEMORY;
      try
      {
        *(cNewType.Get()) = *lpType;
      }
      catch (LONG hr)
      {
        return (HRESULT)hr;
      }

      //add to list
      if (aTypesList.AddElement(cNewType.Get()) == FALSE)
        return E_OUTOFMEMORY;
      cNewType.Detach();
      nThisCount++;
    }
  }

  //done
  return S_OK;
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

CHttpHeaderReqAccept::CType&
CHttpHeaderReqAccept::CType::operator=(_In_ const CHttpHeaderReqAccept::CType &cSrc) throw(...)
{
  if (this != &cSrc)
  {
    CStringA cStrTempTypeA;
    TArrayListWithFree<LPPARAMETER> cTempParamsList;
    SIZE_T i, nCount;

    if (cStrTempTypeA.CopyN((LPCSTR)(cSrc.cStrTypeA), cSrc.cStrTypeA.GetLength()) == FALSE)
      throw (LONG)E_OUTOFMEMORY;

    nCount = cSrc.aParamsList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      CType::LPPARAMETER lpParam = cSrc.aParamsList.GetElementAt(i);
      TAutoFreePtr<PARAMETER> cNewParam;
      SIZE_T nNameLen, nValueLen;

      //get name and value length
      nNameLen = StrLenA(lpParam->szNameA);
      nValueLen = (StrLenW(lpParam->szValueW) + 1) * sizeof(WCHAR);

      //create new item
      cNewParam.Attach((LPPARAMETER)MX_MALLOC(sizeof(PARAMETER) + nNameLen + nValueLen));
      if (!cNewParam)
        throw (LONG)E_OUTOFMEMORY;
      ::MxMemCopy(cNewParam->szNameA, lpParam->szNameA, nNameLen);
      cNewParam->szNameA[nNameLen] = 0;
      cNewParam->szValueW = (LPWSTR)((LPBYTE)(cNewParam->szNameA) + (nNameLen + 1));
      ::MxMemCopy(cNewParam->szValueW, lpParam->szValueW, nValueLen);

      //add to list
      if (cTempParamsList.AddElement(cNewParam.Get()) == FALSE)
        throw (LONG)E_OUTOFMEMORY;
      cNewParam.Detach();
    }

    cStrTypeA.Attach(cStrTempTypeA.Detach());
    q = cSrc.q;
    aParamsList.Attach(cTempParamsList.Detach(), nCount);
  }
  return *this;
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
    szTypeA = GetToken(szTypeA, szTypeEndA);
  else
    szTypeA++;

  //check slash separator
  if (szTypeA >= szTypeEndA || *szTypeA++ != '/')
    return MX_E_InvalidData;

  //get mime subtype
  if (*szTypeA != '*')
    szTypeA = GetToken(szTypeA, szTypeEndA);
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

SIZE_T CHttpHeaderReqAccept::CType::GetParamsCount() const
{
  return aParamsList.GetCount();
}

LPCSTR CHttpHeaderReqAccept::CType::GetParamName(_In_ SIZE_T nIndex) const
{
  return (nIndex < aParamsList.GetCount()) ? aParamsList[nIndex]->szNameA : NULL;
}

LPCWSTR CHttpHeaderReqAccept::CType::GetParamValue(_In_ SIZE_T nIndex) const
{
  return (nIndex < aParamsList.GetCount()) ? aParamsList[nIndex]->szValueW : NULL;
}

LPCWSTR CHttpHeaderReqAccept::CType::GetParamValue(_In_z_ LPCSTR szNameA) const
{
  SIZE_T i, nCount;

  if (szNameA != NULL && szNameA[0] != 0)
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
