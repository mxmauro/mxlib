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
#include "..\..\Include\Http\HttpHeaderReqAcceptLanguage.h"
#include <stdlib.h>

//-----------------------------------------------------------

//NOTE: UNDOCUMENTED!!!
extern "C" {
  extern _locale_tstruct __initiallocalestructinfo;
};

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqAcceptLanguage::CHttpHeaderReqAcceptLanguage() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderReqAcceptLanguage::~CHttpHeaderReqAcceptLanguage()
{
  cLanguagesList.RemoveAllElements();
  return;
}

HRESULT CHttpHeaderReqAcceptLanguage::Parse(__in_z LPCSTR szValueA)
{
  CLanguage *lpLanguage;
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
    hRes = AddLanguage((LPSTR)cStrTempA, &lpLanguage);
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
      hRes = lpLanguage->SetQ(_atof_l((LPSTR)cStrTempA_2, &__initiallocalestructinfo));
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

HRESULT CHttpHeaderReqAcceptLanguage::Build(__inout CStringA &cStrDestA)
{
  SIZE_T i, nCount;
  CLanguage *lpLanguage;

  cStrDestA.Empty();
  nCount = cLanguagesList.GetCount();
  for (i=0; i<nCount; i++)
  {
    lpLanguage = cLanguagesList.GetElementAt(i);
    if (cStrDestA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(",", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrDestA.Concat(lpLanguage->GetLanguage()) == FALSE)
      return E_OUTOFMEMORY;
    //q
    if (lpLanguage->GetQ() < 1.0)
    {
      if (cStrDestA.AppendFormat(";q=%f", lpLanguage->GetQ()) == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderReqAcceptLanguage::AddLanguage(__in_z LPCSTR szLanguageA, __out_opt CLanguage **lplpLanguage)
{
  TAutoDeletePtr<CLanguage> cNewLanguage;
  SIZE_T i, nCount;
  HRESULT hRes;

  if (lplpLanguage != NULL)
    *lplpLanguage = NULL;
  if (szLanguageA == NULL)
    return E_POINTER;
  //create new type
  cNewLanguage.Attach(MX_DEBUG_NEW CLanguage());
  if (!cNewLanguage)
    return E_OUTOFMEMORY;
  hRes = cNewLanguage->SetLanguage(szLanguageA);
  if (FAILED(hRes))
    return hRes;
  //check if already exists in list
  nCount = cLanguagesList.GetCount();
  for (i=0; i<nCount; i++)
  {
    if (StrCompareA(cLanguagesList[i]->GetLanguage(), cNewLanguage->GetLanguage()) == 0)
      return MX_E_AlreadyExists;
  }
  //add to list
  if (cLanguagesList.AddElement(cNewLanguage.Get()) == FALSE)
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
  return cLanguagesList.GetCount();
}

CHttpHeaderReqAcceptLanguage::CLanguage* CHttpHeaderReqAcceptLanguage::GetLanguage(__in SIZE_T nIndex) const
{
  return (nIndex < cLanguagesList.GetCount()) ? cLanguagesList.GetElementAt(nIndex) : NULL;
}

CHttpHeaderReqAcceptLanguage::CLanguage* CHttpHeaderReqAcceptLanguage::GetLanguage(__in_z LPCSTR szLanguageA) const
{
  SIZE_T i, nCount;

  if (szLanguageA != NULL && szLanguageA[0] != 0)
  {
    nCount = cLanguagesList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (StrCompareA(cLanguagesList[i]->GetLanguage(), szLanguageA, TRUE) == 0)
        return cLanguagesList.GetElementAt(i);
    }
  }
  return NULL;
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

HRESULT CHttpHeaderReqAcceptLanguage::CLanguage::SetLanguage(__in_z LPCSTR szLanguageA)
{
  LPCSTR szStartA, szStartA_2, szEndA;

  if (szLanguageA == NULL)
    return E_POINTER;
  //skip spaces
  szLanguageA = CHttpHeaderBase::SkipSpaces(szLanguageA);
  //mark start
  szStartA = szLanguageA;
  //get language
  if (szLanguageA[0] != '*')
  {
    while ((*szLanguageA >= 'A' && *szLanguageA <= 'Z') || (*szLanguageA >= 'a' && *szLanguageA <= 'z'))
      szLanguageA++;
    if (szLanguageA == szStartA || szLanguageA > szStartA+8)
      return MX_E_InvalidData;
    if (*szLanguageA == '-')
    {
      szStartA_2 = ++szLanguageA;
      while ((*szLanguageA >= 'A' && *szLanguageA <= 'Z') || (*szLanguageA >= 'a' && *szLanguageA <= 'z') ||
             (*szLanguageA >= '0' && *szLanguageA <= '9'))
        szLanguageA++;
      if (szLanguageA == szStartA_2 || szLanguageA > szStartA_2+8)
        return MX_E_InvalidData;
    }
  }
  else
  {
    szLanguageA++;
  }
  if (szLanguageA == szStartA)
    return MX_E_InvalidData;
  szEndA = szLanguageA;
  //skip spaces and check for end
  if (*CHttpHeaderBase::SkipSpaces(szLanguageA) != 0)
    return MX_E_InvalidData;
  //set new value
  if (cStrLanguageA.CopyN(szStartA, (SIZE_T)(szEndA-szStartA)) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCSTR CHttpHeaderReqAcceptLanguage::CLanguage::GetLanguage() const
{
  return (LPCSTR)cStrLanguageA;
}

HRESULT CHttpHeaderReqAcceptLanguage::CLanguage::SetQ(__in double _q)
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
