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
#include "..\..\Include\Http\HttpHeaderReqAcceptCharset.h"
#include <stdlib.h>
#include "..\..\Include\AutoPtr.h"

//-----------------------------------------------------------

static VOID RemoveTrailingZeroDecimals(_Inout_ MX::CStringA &cStrA);

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqAcceptCharset::CHttpHeaderReqAcceptCharset() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderReqAcceptCharset::~CHttpHeaderReqAcceptCharset()
{
  return;
}

HRESULT CHttpHeaderReqAcceptCharset::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  CCharset *lpCharset;
  LPCSTR szValueEndA, szStartA;
  CStringA cStrTempA;
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

    //charset
    szValueA = SkipUntil(szStartA = szValueA, szValueEndA, ";, \t");
    if (szValueA == szStartA)
      goto skip_null_listitem;

    bGotItem = TRUE;

    //add charset to list
    hRes = AddCharset(szStartA, (SIZE_T)(szValueA - szStartA), &lpCharset);
    if (FAILED(hRes))
      return hRes;

    //skip spaces
    szValueA = SkipSpaces(szValueA, szValueEndA);

    //parameter
    if (szValueA < szValueEndA && *szValueA == ';')
    {
      //skip spaces
      szValueA = SkipSpaces(szValueA + 1, szValueEndA);
      if (szValueA >= szValueEndA || *szValueA != 'q')
        return MX_E_InvalidData;
      szValueA++;

      //skip spaces
      szValueA = SkipSpaces(szValueA, szValueEndA);

      //is equal sign?
      if (szValueA >= szValueEndA || *szValueA != '=')
        return MX_E_InvalidData;
      szValueA++;

      //skip spaces
      szValueA = SkipSpaces(szValueA, szValueEndA);

      //parse value
      szStartA = szValueA;
      while (szValueA < szValueEndA && ((*szValueA >= '0' && *szValueA <= '9') || *szValueA == '.'))
        szValueA++;
      hRes = StrToDoubleA(szStartA, (SIZE_T)(szValueA - szStartA), &nDbl);
      if (SUCCEEDED(hRes))
        hRes = lpCharset->SetQ(nDbl);
      if (FAILED(hRes))
        return hRes;
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

HRESULT CHttpHeaderReqAcceptCharset::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  CStringA cStrTempA;
  SIZE_T i, nCount;
  CCharset *lpCharset;

  cStrDestA.Empty();
  nCount = aCharsetsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    lpCharset = aCharsetsList.GetElementAt(i);
    if (cStrDestA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(",", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrDestA.Concat(lpCharset->GetCharset()) == FALSE)
      return E_OUTOFMEMORY;
    //q
    if (lpCharset->GetQ() < 1.0 - 0.00000001)
    {
      if (cStrDestA.AppendFormat(";q=%f", lpCharset->GetQ()) == FALSE)
        return E_OUTOFMEMORY;
      RemoveTrailingZeroDecimals(cStrDestA);
    }
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderReqAcceptCharset::AddCharset(_In_z_ LPCSTR szCharsetA, _In_opt_ SIZE_T nCharsetLen,
                                                _Out_opt_ CCharset **lplpCharset)
{
  TAutoDeletePtr<CCharset> cNewCharset;
  SIZE_T i, nCount;
  HRESULT hRes;

  if (lplpCharset != NULL)
    *lplpCharset = NULL;
  if (nCharsetLen == (SIZE_T)-1)
    nCharsetLen = StrLenA(szCharsetA);
  if (nCharsetLen == 0)
    return MX_E_InvalidData;
  if (szCharsetA == NULL)
    return E_POINTER;
  //create new type
  cNewCharset.Attach(MX_DEBUG_NEW CCharset());
  if (!cNewCharset)
    return E_OUTOFMEMORY;
  hRes = cNewCharset->SetCharset(szCharsetA, nCharsetLen);
  if (FAILED(hRes))
    return hRes;
  //check if already exists in list
  nCount = aCharsetsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (StrCompareA(aCharsetsList[i]->GetCharset(), cNewCharset->GetCharset(), TRUE) == 0)
    {
      aCharsetsList.RemoveElementAt(i); //remove previous definition
      break;
    }
  }
  //add to list
  if (aCharsetsList.AddElement(cNewCharset.Get()) == FALSE)
    return E_OUTOFMEMORY;
  //done
  if (lplpCharset != NULL)
    *lplpCharset = cNewCharset.Detach();
  else
    cNewCharset.Detach();
  return S_OK;
}

SIZE_T CHttpHeaderReqAcceptCharset::GetCharsetsCount() const
{
  return aCharsetsList.GetCount();
}

CHttpHeaderReqAcceptCharset::CCharset* CHttpHeaderReqAcceptCharset::GetCharset(_In_ SIZE_T nIndex) const
{
  return (nIndex < aCharsetsList.GetCount()) ? aCharsetsList.GetElementAt(nIndex) : NULL;
}

CHttpHeaderReqAcceptCharset::CCharset* CHttpHeaderReqAcceptCharset::GetCharset(_In_z_ LPCSTR szCharsetA) const
{
  SIZE_T i, nCount;

  if (szCharsetA != NULL && *szCharsetA != 0)
  {
    nCount = aCharsetsList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      if (StrCompareA(aCharsetsList[i]->GetCharset(), szCharsetA, TRUE) == 0)
        return aCharsetsList.GetElementAt(i);
    }
  }
  return NULL;
}

HRESULT CHttpHeaderReqAcceptCharset::Merge(_In_ CHttpHeaderBase *_lpHeader)
{
  CHttpHeaderReqAcceptCharset *lpHeader = reinterpret_cast<CHttpHeaderReqAcceptCharset*>(_lpHeader);
  SIZE_T i, nCount, nThisIndex, nThisCount;

  nCount = lpHeader->aCharsetsList.GetCount();
  nThisCount = aCharsetsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    CCharset *lpCharset = lpHeader->aCharsetsList.GetElementAt(i);

    for (nThisIndex = 0; nThisIndex < nThisCount; nThisIndex++)
    {
      CCharset *lpThisCharset = aCharsetsList.GetElementAt(nThisIndex);

      if (StrCompareA((LPCSTR)(lpCharset->cStrCharsetA), (LPCSTR)(lpThisCharset->cStrCharsetA), TRUE) == 0)
        break;
    }
    if (nThisIndex < nThisCount)
    {
      //have a match, add/replace parameters
      CCharset *lpThisCharset = aCharsetsList.GetElementAt(nThisIndex);

      lpThisCharset->q = lpCharset->q;
    }
    else
    {
      TAutoDeletePtr<CCharset> cNewCharset;

      //add a duplicate
      cNewCharset.Attach(MX_DEBUG_NEW CCharset());
      if (!cNewCharset)
        return E_OUTOFMEMORY;
      try
      {
        *(cNewCharset.Get()) = *lpCharset;
      }
      catch (LONG hr)
      {
        return (HRESULT)hr;
      }

      //add to list
      if (aCharsetsList.AddElement(cNewCharset.Get()) == FALSE)
        return E_OUTOFMEMORY;
      cNewCharset.Detach();
      nThisCount++;
    }
  }

  //done
  return S_OK;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CHttpHeaderReqAcceptCharset::CCharset::CCharset() : CBaseMemObj()
{
  q = 1.0;
  return;
}

CHttpHeaderReqAcceptCharset::CCharset::~CCharset()
{
  return;
}

CHttpHeaderReqAcceptCharset::CCharset&
CHttpHeaderReqAcceptCharset::CCharset::operator=(_In_ const CHttpHeaderReqAcceptCharset::CCharset &cSrc) throw(...)
{
  if (this != &cSrc)
  {
    CStringA cStrTempCharsetA;

    if (cStrTempCharsetA.CopyN((LPCSTR)(cSrc.cStrCharsetA), cSrc.cStrCharsetA.GetLength()) == FALSE)
      throw (LONG)E_OUTOFMEMORY;

    cStrCharsetA.Attach(cStrTempCharsetA.Detach());
    q = cSrc.q;
  }
  return *this;
}

HRESULT CHttpHeaderReqAcceptCharset::CCharset::SetCharset(_In_z_ LPCSTR szCharsetA, _In_opt_ SIZE_T nCharsetLen)
{
  LPCSTR szCharsetEndA, szStartA;

  if (nCharsetLen == (SIZE_T)-1)
    nCharsetLen = StrLenA(szCharsetA);
  if (nCharsetLen == 0)
    return MX_E_InvalidData;
  if (szCharsetA == NULL)
    return E_POINTER;
  szCharsetEndA = szCharsetA + nCharsetLen;

  //mark start
  szStartA = szCharsetA;
  //get language
  if (*szCharsetA != '*')
  {
    szCharsetA = CHttpHeaderBase::GetToken(szCharsetA, szCharsetEndA);
  }
  else
  {
    szCharsetA++;
  }
  if (szCharsetA != szCharsetEndA)
    return MX_E_InvalidData;

  //set new value
  if (cStrCharsetA.CopyN(szStartA, nCharsetLen) == FALSE)
    return E_OUTOFMEMORY;

  //done
  return S_OK;
}

LPCSTR CHttpHeaderReqAcceptCharset::CCharset::GetCharset() const
{
  return (LPCSTR)cStrCharsetA;
}

HRESULT CHttpHeaderReqAcceptCharset::CCharset::SetQ(_In_ double _q)
{
  if (_q < -0.000001 || _q > 1.000001)
    return E_INVALIDARG;
  q = (_q > 0.000001) ? ((_q < 0.99999) ? _q : 1.0) : 0.0;
  return S_OK;
}

double CHttpHeaderReqAcceptCharset::CCharset::GetQ() const
{
  return q;
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
