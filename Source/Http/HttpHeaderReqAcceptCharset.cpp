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
#include "..\..\Include\Http\HttpHeaderReqAcceptCharset.h"
#include <stdlib.h>

//-----------------------------------------------------------

//NOTE: UNDOCUMENTED!!!
extern "C" {
  extern _locale_tstruct __initiallocalestructinfo;
};

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqAcceptCharset::CHttpHeaderReqAcceptCharset() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderReqAcceptCharset::~CHttpHeaderReqAcceptCharset()
{
  cTypesList.RemoveAllElements();
  return;
}

HRESULT CHttpHeaderReqAcceptCharset::Parse(__in_z LPCSTR szValueA)
{
  CType *lpType;
  LPCSTR szStartA;
  CStringA cStrTempA, cStrTempA_2;
  CStringW cStrTempW;
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
    bGotItem = TRUE;
    //mime type
    szValueA = Advance(szStartA = szValueA, ";,");
    if (cStrTempA.CopyN(szStartA, (SIZE_T)(szValueA-szStartA)) == FALSE)
      return E_OUTOFMEMORY;
    hRes = AddType((LPCSTR)cStrTempA, &lpType);
    if (FAILED(hRes))
      return hRes;
    //parameter
    if (*szValueA == ';')
    {
      //skip spaces
      szValueA = SkipSpaces(szValueA+1);
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
      if (cStrTempA_2.CopyN(szStartA, (SIZE_T)(szValueA-szStartA)) == FALSE)
        return E_OUTOFMEMORY;
      //add parameter
      hRes = lpType->SetQ(_atof_l((LPSTR)cStrTempA_2, &__initiallocalestructinfo));
      if (FAILED(hRes))
        return hRes;
    }
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

HRESULT CHttpHeaderReqAcceptCharset::Build(__inout CStringA &cStrDestA)
{
  SIZE_T i, nCount;
  CType *lpType;

  cStrDestA.Empty();
  nCount = cTypesList.GetCount();
  for (i=0; i<nCount; i++)
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
    if (lpType->GetQ() < 1.0)
    {
      if (cStrDestA.AppendFormat("; q=%f", lpType->GetQ()) == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderReqAcceptCharset::AddType(__in_z LPCSTR szTypeA, __out_opt CType **lplpType)
{
  TAutoDeletePtr<CType> cNewType;
  SIZE_T i, nCount;
  HRESULT hRes;

  if (lplpType != NULL)
    *lplpType = NULL;
  if (szTypeA == NULL)
    return E_POINTER;
  //create new type
  cNewType.Attach(MX_DEBUG_NEW CType());
  if (!cNewType)
    return E_OUTOFMEMORY;
  hRes = cNewType->SetType(szTypeA);
  if (FAILED(hRes))
    return hRes;
  //check if already exists in list
  nCount = cTypesList.GetCount();
  for (i=0; i<nCount; i++)
  {
    if (StrCompareA(cTypesList[i]->GetType(), cNewType->GetType()) == 0)
      return MX_E_AlreadyExists;
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

SIZE_T CHttpHeaderReqAcceptCharset::GetTypesCount() const
{
  return cTypesList.GetCount();
}

CHttpHeaderReqAcceptCharset::CType* CHttpHeaderReqAcceptCharset::GetType(__in SIZE_T nIndex) const
{
  return (nIndex < cTypesList.GetCount()) ? cTypesList.GetElementAt(nIndex) : NULL;
}

CHttpHeaderReqAcceptCharset::CType* CHttpHeaderReqAcceptCharset::GetType(__in_z LPCSTR szTypeA) const
{
  SIZE_T i, nCount;

  if (szTypeA != NULL && szTypeA[0] != 0)
  {
    nCount = cTypesList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (StrCompareA(cTypesList[i]->GetType(), szTypeA, TRUE) == 0)
        return cTypesList.GetElementAt(i);
    }
  }
  return NULL;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CHttpHeaderReqAcceptCharset::CType::CType() : CBaseMemObj()
{
  q = 1.0;
  return;
}

CHttpHeaderReqAcceptCharset::CType::~CType()
{
  return;
}

HRESULT CHttpHeaderReqAcceptCharset::CType::SetType(__in_z LPCSTR szTypeA)
{
  LPCSTR szStartA, szEndA;

  if (szTypeA == NULL)
    return E_POINTER;
  //skip spaces
  szTypeA = CHttpHeaderBase::SkipSpaces(szTypeA);
  //mark start
  szStartA = szTypeA;
  //get mime type
  szTypeA = (szTypeA[0] != '*') ? CHttpHeaderBase::GetToken(szTypeA, "/") : szTypeA++;
  if (szTypeA == szStartA)
    return MX_E_InvalidData;
  szEndA = szTypeA;
  //skip spaces and check for end
  if (*CHttpHeaderBase::SkipSpaces(szTypeA) != 0)
    return MX_E_InvalidData;
  //set new value
  if (cStrTypeA.CopyN(szStartA, (SIZE_T)(szEndA-szStartA)) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCSTR CHttpHeaderReqAcceptCharset::CType::GetType() const
{
  return (LPCSTR)cStrTypeA;
}

HRESULT CHttpHeaderReqAcceptCharset::CType::SetQ(__in double _q)
{
  if (_q < -0.000001 || _q > 1.000001)
    return E_INVALIDARG;
  q = (_q > 0.000001) ? ((_q < 0.99999) ? _q : 1.0) : 0.0;
  return S_OK;
}

double CHttpHeaderReqAcceptCharset::CType::GetQ() const
{
  return q;
}

} //namespace MX
