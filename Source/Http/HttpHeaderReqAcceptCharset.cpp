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

HRESULT CHttpHeaderReqAcceptCharset::Parse(_In_z_ LPCSTR szValueA)
{
  CCharset *lpCharset;
  LPCSTR szStartA;
  CStringA cStrTempA;
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

    //charset
    szValueA = SkipUntil(szStartA = szValueA, ";, \t");
    if (szValueA == szStartA)
      goto skip_null_listitem;

    bGotItem = TRUE;

    //add charset to list
    hRes = AddCharset(szStartA, (SIZE_T)(szValueA - szStartA), &lpCharset);
    if (FAILED(hRes))
      return hRes;

    //skip spaces
    szValueA = SkipSpaces(szValueA);

    //parameter
    if (*szValueA == ';')
    {
      //skip spaces
      szValueA = SkipSpaces(szValueA + 1);
      if (*szValueA++ != 'q')
        return MX_E_InvalidData;

      //skip spaces
      szValueA = SkipSpaces(szValueA);

      //is equal sign?
      if (*szValueA++ != '=')
        return MX_E_InvalidData;

      //skip spaces
      szValueA = SkipSpaces(szValueA);

      //parse value
      szStartA = szValueA;
      while ((*szValueA >= '0' && *szValueA <= '9') || *szValueA == '.')
        szValueA++;
      hRes = StrToDoubleA(szStartA, (SIZE_T)(szValueA - szStartA), &nDbl);
      if (SUCCEEDED(hRes))
        hRes = lpCharset->SetQ(nDbl);
      if (FAILED(hRes))
        return hRes;
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

HRESULT CHttpHeaderReqAcceptCharset::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  CStringA cStrTempA;
  SIZE_T i, nCount;
  CCharset *lpCharset;

  cStrDestA.Empty();
  nCount = cCharsetsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    lpCharset = cCharsetsList.GetElementAt(i);
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
  nCount = cCharsetsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (StrCompareA(cCharsetsList[i]->GetCharset(), cNewCharset->GetCharset(), TRUE) == 0)
    {
      cCharsetsList.RemoveElementAt(i); //remove previous definition
      break;
    }
  }
  //add to list
  if (cCharsetsList.AddElement(cNewCharset.Get()) == FALSE)
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
  return cCharsetsList.GetCount();
}

CHttpHeaderReqAcceptCharset::CCharset* CHttpHeaderReqAcceptCharset::GetCharset(_In_ SIZE_T nIndex) const
{
  return (nIndex < cCharsetsList.GetCount()) ? cCharsetsList.GetElementAt(nIndex) : NULL;
}

CHttpHeaderReqAcceptCharset::CCharset* CHttpHeaderReqAcceptCharset::GetCharset(_In_z_ LPCSTR szCharsetA) const
{
  SIZE_T i, nCount;

  if (szCharsetA != NULL && *szCharsetA != 0)
  {
    nCount = cCharsetsList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      if (StrCompareA(cCharsetsList[i]->GetCharset(), szCharsetA, TRUE) == 0)
        return cCharsetsList.GetElementAt(i);
    }
  }
  return NULL;
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

HRESULT CHttpHeaderReqAcceptCharset::CCharset::SetCharset(_In_z_ LPCSTR szCharsetA, _In_ SIZE_T nCharsetLen)
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
    szCharsetA = CHttpHeaderBase::GetToken(szCharsetA, nCharsetLen);
  }
  else
  {
    szCharsetA++;
  }
  if (szCharsetA != szCharsetEndA)
    return MX_E_InvalidData;
  //set new value
  if (cStrCharsetA.CopyN(szStartA, (SIZE_T)(szCharsetEndA - szStartA)) == FALSE)
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
