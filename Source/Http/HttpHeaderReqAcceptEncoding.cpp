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

HRESULT CHttpHeaderReqAcceptEncoding::Parse(_In_z_ LPCSTR szValueA)
{
  CEncoding *lpEncoding;
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

    //encoding
    szValueA = SkipUntil(szStartA = szValueA, ";, \t");
    if (szValueA == szStartA)
      goto skip_null_listitem;

    bGotItem = TRUE;

    //add encoding to list
    hRes = AddEncoding(szStartA, (SIZE_T)(szValueA - szStartA), &lpEncoding);
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
        hRes = lpEncoding->SetQ(nDbl);
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

HRESULT CHttpHeaderReqAcceptEncoding::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  CStringA cStrTempA;
  SIZE_T i, nCount;
  CEncoding *lpEncoding;

  cStrDestA.Empty();
  nCount = cEncodingsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    lpEncoding = cEncodingsList.GetElementAt(i);
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
  nCount = cEncodingsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (StrCompareA(cEncodingsList[i]->GetEncoding(), cNewEncoding->GetEncoding(), TRUE) == 0)
    {
      cEncodingsList.RemoveElementAt(i); //remove previous definition
      break;
    }
  }
  //add to list
  if (cEncodingsList.AddElement(cNewEncoding.Get()) == FALSE)
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
  return cEncodingsList.GetCount();
}

CHttpHeaderReqAcceptEncoding::CEncoding* CHttpHeaderReqAcceptEncoding::GetEncoding(_In_ SIZE_T nIndex) const
{
  return (nIndex < cEncodingsList.GetCount()) ? cEncodingsList.GetElementAt(nIndex) : NULL;
}

CHttpHeaderReqAcceptEncoding::CEncoding* CHttpHeaderReqAcceptEncoding::GetEncoding(_In_z_ LPCSTR szEncodingA) const
{
  SIZE_T i, nCount;

  if (szEncodingA != NULL && *szEncodingA != 0)
  {
    nCount = cEncodingsList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      if (StrCompareA(cEncodingsList[i]->GetEncoding(), szEncodingA, TRUE) == 0)
        return cEncodingsList.GetElementAt(i);
    }
  }
  return NULL;
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

HRESULT CHttpHeaderReqAcceptEncoding::CEncoding::SetEncoding(_In_z_ LPCSTR szEncodingA, _In_ SIZE_T nEncodingLen)
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
    szEncodingA = CHttpHeaderBase::GetToken(szEncodingA, nEncodingLen);
  }
  else
  {
    szEncodingA++;
  }
  if (szEncodingA != szEncodingEndA)
    return MX_E_InvalidData;
  //set new value
  if (cStrEncodingA.CopyN(szStartA, (SIZE_T)(szEncodingEndA - szStartA)) == FALSE)
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
