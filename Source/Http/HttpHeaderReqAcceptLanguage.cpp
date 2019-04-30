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

HRESULT CHttpHeaderReqAcceptLanguage::Parse(_In_z_ LPCSTR szValueA)
{
  CLanguage *lpLanguage;
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

    //mime type
    szValueA = SkipUntil(szStartA = szValueA, ";, \t");
    if (szValueA == szStartA)
      goto skip_null_listitem;

    bGotItem = TRUE;

    //add to list
    hRes = AddLanguage(szStartA, (SIZE_T)(szValueA - szStartA), &lpLanguage);
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
        hRes = lpLanguage->SetQ(nDbl);
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

HRESULT CHttpHeaderReqAcceptLanguage::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  CStringA cStrTempA;
  SIZE_T i, nCount;
  CLanguage *lpLanguage;

  cStrDestA.Empty();
  nCount = cLanguagesList.GetCount();
  for (i = 0; i < nCount; i++)
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
  nCount = cLanguagesList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (StrCompareA(cLanguagesList[i]->GetLanguage(), cNewLanguage->GetLanguage(), TRUE) == 0)
    {
      cLanguagesList.RemoveElementAt(i); //remove previous definition
      break;
    }
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

CHttpHeaderReqAcceptLanguage::CLanguage* CHttpHeaderReqAcceptLanguage::GetLanguage(_In_ SIZE_T nIndex) const
{
  return (nIndex < cLanguagesList.GetCount()) ? cLanguagesList.GetElementAt(nIndex) : NULL;
}

CHttpHeaderReqAcceptLanguage::CLanguage* CHttpHeaderReqAcceptLanguage::GetLanguage(_In_z_ LPCSTR szLanguageA) const
{
  SIZE_T i, nCount;

  if (szLanguageA != NULL && *szLanguageA != 0)
  {
    nCount = cLanguagesList.GetCount();
    for (i = 0; i < nCount; i++)
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
           (*szLanguageA >= 'A' && *szLanguageA <= 'Z') || (*szLanguageA >= 'a' && *szLanguageA <= 'z'))
    {
      szLanguageA++;
    }
    if (szLanguageA == szStartA[0] || szLanguageA > szStartA[0] + 8)
      return MX_E_InvalidData;
    if (szLanguageA < szLanguageEndA && *szLanguageA == '-')
    {
      szStartA[1] = ++szLanguageA;
      while (szLanguageA < szLanguageEndA &&
             (*szLanguageA >= 'A' && *szLanguageA <= 'Z') || (*szLanguageA >= 'a' && *szLanguageA <= 'z'))
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
