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
#include "..\..\Include\Http\HttpHeaderReqAcceptLanguage.h"
#include <stdlib.h>
#include "..\..\Include\AutoPtr.h"

//-----------------------------------------------------------

static VOID RemoveTrailingZeroDecimals(_Inout_ MX::CStringA &cStrA);

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqAcceptLanguage::CHttpHeaderReqAcceptLanguage() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderReqAcceptLanguage::~CHttpHeaderReqAcceptLanguage()
{
  return;
}

HRESULT CHttpHeaderReqAcceptLanguage::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  CLanguage *lpLanguage;
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

    //mime type
    szValueA = SkipUntil(szStartA = szValueA, szValueEndA, ";, \t");
    if (szValueA == szStartA)
      goto skip_null_listitem;

    bGotItem = TRUE;

    //add to list
    hRes = AddLanguage(szStartA, (SIZE_T)(szValueA - szStartA), &lpLanguage);
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
        hRes = lpLanguage->SetQ(nDbl);
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

HRESULT CHttpHeaderReqAcceptLanguage::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  CStringA cStrTempA;
  SIZE_T i, nCount;
  CLanguage *lpLanguage;

  cStrDestA.Empty();
  nCount = aLanguagesList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    lpLanguage = aLanguagesList.GetElementAt(i);
    if (cStrDestA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(",", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrDestA.Concat(lpLanguage->GetLanguage()) == FALSE)
      return E_OUTOFMEMORY;

    //q
    if (lpLanguage->GetQ() < 1.0 - 0.00000001)
    {
      if (cStrDestA.AppendFormat(";q=%f", lpLanguage->GetQ()) == FALSE)
        return E_OUTOFMEMORY;
      RemoveTrailingZeroDecimals(cStrDestA);
    }
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderReqAcceptLanguage::AddLanguage(_In_z_ LPCSTR szLanguageA, _In_opt_ SIZE_T nLanguageLen,
                                                  _Out_opt_ CLanguage **lplpLanguage)
{
  TAutoDeletePtr<CLanguage> cNewLanguage;
  SIZE_T i, nCount;
  HRESULT hRes;

  if (lplpLanguage != NULL)
    *lplpLanguage = NULL;
  if (nLanguageLen == (SIZE_T)-1)
    nLanguageLen = StrLenA(szLanguageA);
  if (nLanguageLen == 0)
    return MX_E_InvalidData;
  if (szLanguageA == NULL)
    return E_POINTER;

  //create new type
  cNewLanguage.Attach(MX_DEBUG_NEW CLanguage());
  if (!cNewLanguage)
    return E_OUTOFMEMORY;
  hRes = cNewLanguage->SetLanguage(szLanguageA, nLanguageLen);
  if (FAILED(hRes))
    return hRes;

  //check if already exists in list
  nCount = aLanguagesList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (StrCompareA(aLanguagesList[i]->GetLanguage(), cNewLanguage->GetLanguage(), TRUE) == 0)
    {
      aLanguagesList.RemoveElementAt(i); //remove previous definition
      break;
    }
  }

  //add to list
  if (aLanguagesList.AddElement(cNewLanguage.Get()) == FALSE)
    return E_OUTOFMEMORY;

  //done
  if (lplpLanguage != NULL)
    *lplpLanguage = cNewLanguage.Detach();
  else
    cNewLanguage.Detach();
  return S_OK;
}

SIZE_T CHttpHeaderReqAcceptLanguage::GetLanguagesCount() const
{
  return aLanguagesList.GetCount();
}

CHttpHeaderReqAcceptLanguage::CLanguage* CHttpHeaderReqAcceptLanguage::GetLanguage(_In_ SIZE_T nIndex) const
{
  return (nIndex < aLanguagesList.GetCount()) ? aLanguagesList.GetElementAt(nIndex) : NULL;
}

CHttpHeaderReqAcceptLanguage::CLanguage* CHttpHeaderReqAcceptLanguage::GetLanguage(_In_z_ LPCSTR szLanguageA) const
{
  SIZE_T i, nCount;

  if (szLanguageA != NULL && *szLanguageA != 0)
  {
    nCount = aLanguagesList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      if (StrCompareA(aLanguagesList[i]->GetLanguage(), szLanguageA, TRUE) == 0)
        return aLanguagesList.GetElementAt(i);
    }
  }
  return NULL;
}

HRESULT CHttpHeaderReqAcceptLanguage::Merge(_In_ CHttpHeaderBase *_lpHeader)
{
  CHttpHeaderReqAcceptLanguage *lpHeader = reinterpret_cast<CHttpHeaderReqAcceptLanguage*>(_lpHeader);
  SIZE_T i, nCount, nThisIndex, nThisCount;

  nCount = lpHeader->aLanguagesList.GetCount();
  nThisCount = aLanguagesList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    CLanguage *lpLanguage = lpHeader->aLanguagesList.GetElementAt(i);

    for (nThisIndex = 0; nThisIndex < nThisCount; nThisIndex++)
    {
      CLanguage *lpThisLanguage = aLanguagesList.GetElementAt(nThisIndex);

      if (StrCompareA((LPCSTR)(lpLanguage->cStrLanguageA), (LPCSTR)(lpThisLanguage->cStrLanguageA), TRUE) == 0)
        break;
    }
    if (nThisIndex < nThisCount)
    {
      //have a match, add/replace parameters
      CLanguage *lpThisLanguage = aLanguagesList.GetElementAt(nThisIndex);

      lpThisLanguage->q = lpLanguage->q;
    }
    else
    {
      TAutoDeletePtr<CLanguage> cNewLanguage;

      //add a duplicate
      cNewLanguage.Attach(MX_DEBUG_NEW CLanguage());
      if (!cNewLanguage)
        return E_OUTOFMEMORY;
      try
      {
        *(cNewLanguage.Get()) = *lpLanguage;
      }
      catch (LONG hr)
      {
        return (HRESULT)hr;
      }

      //add to list
      if (aLanguagesList.AddElement(cNewLanguage.Get()) == FALSE)
        return E_OUTOFMEMORY;
      cNewLanguage.Detach();
      nThisCount++;
    }
  }

  //done
  return S_OK;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CHttpHeaderReqAcceptLanguage::CLanguage::CLanguage() : CBaseMemObj()
{
  q = 1.0;
  return;
}

CHttpHeaderReqAcceptLanguage::CLanguage::~CLanguage()
{
  return;
}

CHttpHeaderReqAcceptLanguage::CLanguage&
CHttpHeaderReqAcceptLanguage::CLanguage::operator=(_In_ const CHttpHeaderReqAcceptLanguage::CLanguage &cSrc) throw(...)
{
  if (this != &cSrc)
  {
    CStringA cStrTempLanguageA;

    if (cStrTempLanguageA.CopyN((LPCSTR)(cSrc.cStrLanguageA), cSrc.cStrLanguageA.GetLength()) == FALSE)
      throw (LONG)E_OUTOFMEMORY;

    cStrLanguageA.Attach(cStrTempLanguageA.Detach());
    q = cSrc.q;
  }
  return *this;
}

HRESULT CHttpHeaderReqAcceptLanguage::CLanguage::SetLanguage(_In_z_ LPCSTR szLanguageA, _In_opt_ SIZE_T nLanguageLen)
{
  LPCSTR szLanguageEndA, szStartA[2];

  if (nLanguageLen == (SIZE_T)-1)
    nLanguageLen = StrLenA(szLanguageA);
  if (nLanguageLen == 0)
    return MX_E_InvalidData;
  if (szLanguageA == NULL)
    return E_POINTER;
  szLanguageEndA = szLanguageA + nLanguageLen;

  //mark start
  szStartA[0] = szLanguageA;

  //get language
  if (*szLanguageA != '*')
  {
    while (szLanguageA < szLanguageEndA &&
           ((*szLanguageA >= 'A' && *szLanguageA <= 'Z') || (*szLanguageA >= 'a' && *szLanguageA <= 'z')))
    {
      szLanguageA++;
    }
    if (szLanguageA == szStartA[0] || szLanguageA > szStartA[0] + 8)
      return MX_E_InvalidData;
    while (szLanguageA < szLanguageEndA && *szLanguageA == '-')
    {
      szStartA[1] = ++szLanguageA;
      while (szLanguageA < szLanguageEndA &&
             ((*szLanguageA >= 'A' && *szLanguageA <= 'Z') || (*szLanguageA >= 'a' && *szLanguageA <= 'z') ||
              (*szLanguageA >= '0' && *szLanguageA <= '9')))
      {
        szLanguageA++;
      }
      if (szLanguageA == szStartA[1] || szLanguageA > szStartA[1] + 8)
        return MX_E_InvalidData;
    }
  }
  else
  {
    szLanguageA++;
  }
  if (szLanguageA != szLanguageEndA)
    return MX_E_InvalidData;

  //set new value
  if (cStrLanguageA.CopyN(szStartA[0], (SIZE_T)(szLanguageEndA - szStartA[0])) == FALSE)
    return E_OUTOFMEMORY;

  //done
  return S_OK;
}

LPCSTR CHttpHeaderReqAcceptLanguage::CLanguage::GetLanguage() const
{
  return (LPCSTR)cStrLanguageA;
}

HRESULT CHttpHeaderReqAcceptLanguage::CLanguage::SetQ(_In_ double _q)
{
  if (_q < -0.000001 || _q > 1.000001)
    return E_INVALIDARG;
  q = (_q > 0.000001) ? ((_q < 0.99999) ? _q : 1.0) : 0.0;
  return S_OK;
}

double CHttpHeaderReqAcceptLanguage::CLanguage::GetQ() const
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
