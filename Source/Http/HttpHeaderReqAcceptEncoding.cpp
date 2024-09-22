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
#include "..\..\Include\Http\HttpHeaderReqAcceptEncoding.h"
#include <stdlib.h>
#include "..\..\Include\AutoPtr.h"

//-----------------------------------------------------------

static VOID RemoveTrailingZeroDecimals(_Inout_ MX::CStringA &cStrA);

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqAcceptEncoding::CHttpHeaderReqAcceptEncoding() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderReqAcceptEncoding::~CHttpHeaderReqAcceptEncoding()
{
  return;
}

HRESULT CHttpHeaderReqAcceptEncoding::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  CEncoding *lpEncoding;
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

    //encoding
    szValueA = SkipUntil(szStartA = szValueA, szValueEndA, ";, \t");
    if (szValueA == szStartA)
      goto skip_null_listitem;

    bGotItem = TRUE;

    //add encoding to list
    hRes = AddEncoding(szStartA, (SIZE_T)(szValueA - szStartA), &lpEncoding);
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

      //skip spaces
      szValueA = SkipSpaces(szValueA, szValueEndA);

      //parse value
      szStartA = szValueA;
      while (szValueA < szValueEndA && ((*szValueA >= '0' && *szValueA <= '9') || *szValueA == '.'))
        szValueA++;
      hRes = StrToDoubleA(szStartA, (SIZE_T)(szValueA - szStartA), &nDbl);
      if (SUCCEEDED(hRes))
        hRes = lpEncoding->SetQ(nDbl);
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

HRESULT CHttpHeaderReqAcceptEncoding::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  CStringA cStrTempA;
  SIZE_T i, nCount;
  CEncoding *lpEncoding;

  cStrDestA.Empty();
  nCount = aEncodingsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    lpEncoding = aEncodingsList.GetElementAt(i);
    if (cStrDestA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(",", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrDestA.Concat(lpEncoding->GetEncoding()) == FALSE)
      return E_OUTOFMEMORY;

    //q
    if (lpEncoding->GetQ() < 1.0 - 0.00000001)
    {
      if (cStrDestA.AppendFormat(";q=%f", lpEncoding->GetQ()) == FALSE)
        return E_OUTOFMEMORY;
      RemoveTrailingZeroDecimals(cStrDestA);
    }
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderReqAcceptEncoding::AddEncoding(_In_z_ LPCSTR szEncodingA, _In_opt_ SIZE_T nEncodingLen,
                                                  _Out_opt_ CEncoding **lplpEncoding)
{
  TAutoDeletePtr<CEncoding> cNewEncoding;
  SIZE_T i, nCount;
  HRESULT hRes;

  if (lplpEncoding != NULL)
    *lplpEncoding = NULL;
  if (nEncodingLen == (SIZE_T)-1)
    nEncodingLen = StrLenA(szEncodingA);
  if (nEncodingLen == 0)
    return MX_E_InvalidData;
  if (szEncodingA == NULL)
    return E_POINTER;

  //create new type
  cNewEncoding.Attach(MX_DEBUG_NEW CEncoding());
  if (!cNewEncoding)
    return E_OUTOFMEMORY;
  hRes = cNewEncoding->SetEncoding(szEncodingA, nEncodingLen);
  if (FAILED(hRes))
    return hRes;

  //check if already exists in list
  nCount = aEncodingsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (StrCompareA(aEncodingsList[i]->GetEncoding(), cNewEncoding->GetEncoding(), TRUE) == 0)
    {
      aEncodingsList.RemoveElementAt(i); //remove previous definition
      break;
    }
  }

  //add to list
  if (aEncodingsList.AddElement(cNewEncoding.Get()) == FALSE)
    return E_OUTOFMEMORY;

  //done
  if (lplpEncoding != NULL)
    *lplpEncoding = cNewEncoding.Detach();
  else
    cNewEncoding.Detach();
  return S_OK;
}

SIZE_T CHttpHeaderReqAcceptEncoding::GetEncodingsCount() const
{
  return aEncodingsList.GetCount();
}

CHttpHeaderReqAcceptEncoding::CEncoding* CHttpHeaderReqAcceptEncoding::GetEncoding(_In_ SIZE_T nIndex) const
{
  return (nIndex < aEncodingsList.GetCount()) ? aEncodingsList.GetElementAt(nIndex) : NULL;
}

CHttpHeaderReqAcceptEncoding::CEncoding* CHttpHeaderReqAcceptEncoding::GetEncoding(_In_z_ LPCSTR szEncodingA) const
{
  SIZE_T i, nCount;

  if (szEncodingA != NULL && *szEncodingA != 0)
  {
    nCount = aEncodingsList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      if (StrCompareA(aEncodingsList[i]->GetEncoding(), szEncodingA, TRUE) == 0)
        return aEncodingsList.GetElementAt(i);
    }
  }
  return NULL;
}

HRESULT CHttpHeaderReqAcceptEncoding::Merge(_In_ CHttpHeaderBase *_lpHeader)
{
  CHttpHeaderReqAcceptEncoding *lpHeader = reinterpret_cast<CHttpHeaderReqAcceptEncoding*>(_lpHeader);
  SIZE_T i, nCount, nThisIndex, nThisCount;

  nCount = lpHeader->aEncodingsList.GetCount();
  nThisCount = aEncodingsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    CEncoding *lpEncoding = lpHeader->aEncodingsList.GetElementAt(i);

    for (nThisIndex = 0; nThisIndex < nThisCount; nThisIndex++)
    {
      CEncoding *lpThisEncoding = aEncodingsList.GetElementAt(nThisIndex);

      if (StrCompareA((LPCSTR)(lpEncoding->cStrEncodingA), (LPCSTR)(lpThisEncoding->cStrEncodingA), TRUE) == 0)
        break;
    }
    if (nThisIndex < nThisCount)
    {
      //have a match, add/replace parameters
      CEncoding *lpThisEncoding = aEncodingsList.GetElementAt(nThisIndex);

      lpThisEncoding->q = lpEncoding->q;
    }
    else
    {
      TAutoDeletePtr<CEncoding> cNewEncoding;

      //add a duplicate
      cNewEncoding.Attach(MX_DEBUG_NEW CEncoding());
      if (!cNewEncoding)
        return E_OUTOFMEMORY;
      try
      {
        *(cNewEncoding.Get()) = *lpEncoding;
      }
      catch (LONG hr)
      {
        return (HRESULT)hr;
      }

      //add to list
      if (aEncodingsList.AddElement(cNewEncoding.Get()) == FALSE)
        return E_OUTOFMEMORY;
      cNewEncoding.Detach();
      nThisCount++;
    }
  }

  //done
  return S_OK;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CHttpHeaderReqAcceptEncoding::CEncoding::CEncoding() : CBaseMemObj()
{
  q = 1.0;
  return;
}

CHttpHeaderReqAcceptEncoding::CEncoding::~CEncoding()
{
  return;
}

CHttpHeaderReqAcceptEncoding::CEncoding&
CHttpHeaderReqAcceptEncoding::CEncoding::operator=(_In_ const CHttpHeaderReqAcceptEncoding::CEncoding &cSrc) throw(...)
{
  if (this != &cSrc)
  {
    CStringA cStrTempEncodingA;

    if (cStrTempEncodingA.CopyN((LPCSTR)(cSrc.cStrEncodingA), cSrc.cStrEncodingA.GetLength()) == FALSE)
      throw (LONG)E_OUTOFMEMORY;

    cStrEncodingA.Attach(cStrTempEncodingA.Detach());
    q = cSrc.q;
  }
  return *this;
}

HRESULT CHttpHeaderReqAcceptEncoding::CEncoding::SetEncoding(_In_z_ LPCSTR szEncodingA, _In_opt_ SIZE_T nEncodingLen)
{
  LPCSTR szEncodingEndA, szStartA;

  if (nEncodingLen == (SIZE_T)-1)
    nEncodingLen = StrLenA(szEncodingA);
  if (nEncodingLen == 0)
    return MX_E_InvalidData;
  if (szEncodingA == NULL)
    return E_POINTER;
  szEncodingEndA = szEncodingA + nEncodingLen;

  //mark start
  szStartA = szEncodingA;
  //get language
  if (*szEncodingA != '*')
  {
    szEncodingA = CHttpHeaderBase::GetToken(szEncodingA, szEncodingEndA);
  }
  else
  {
    szEncodingA++;
  }
  if (szEncodingA != szEncodingEndA)
    return MX_E_InvalidData;

  //set new value
  if (cStrEncodingA.CopyN(szStartA, nEncodingLen) == FALSE)
    return E_OUTOFMEMORY;

  //done
  return S_OK;
}

LPCSTR CHttpHeaderReqAcceptEncoding::CEncoding::GetEncoding() const
{
  return (LPCSTR)cStrEncodingA;
}

HRESULT CHttpHeaderReqAcceptEncoding::CEncoding::SetQ(_In_ double _q)
{
  if (_q < -0.000001 || _q > 1.000001)
    return E_INVALIDARG;
  q = (_q > 0.000001) ? ((_q < 0.99999) ? _q : 1.0) : 0.0;
  return S_OK;
}

double CHttpHeaderReqAcceptEncoding::CEncoding::GetQ() const
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
