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
#include "..\..\Include\Http\HttpHeaderGenUpgrade.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderGenUpgrade::CHttpHeaderGenUpgrade() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderGenUpgrade::~CHttpHeaderGenUpgrade()
{
  cProductsList.RemoveAllElements();
  return;
}

HRESULT CHttpHeaderGenUpgrade::Parse(__in_z LPCSTR szValueA)
{
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
    //upgrade
    szValueA = Advance(szStartA = szValueA, ";, \t");
    if (cStrTempA.CopyN(szStartA, (SIZE_T)(szValueA-szStartA)) == FALSE)
      return E_OUTOFMEMORY;
    hRes = AddProduct((LPCSTR)cStrTempA);
    if (FAILED(hRes))
      return hRes;
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

HRESULT CHttpHeaderGenUpgrade::Build(__inout CStringA &cStrDestA)
{
  SIZE_T i, nCount;

  cStrDestA.Empty();
  nCount = cProductsList.GetCount();
  for (i=0; i<nCount; i++)
  {
    if (cStrDestA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(",", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrDestA.Concat(cProductsList.GetElementAt(i)) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderGenUpgrade::AddProduct(__in_z LPCSTR szProductA)
{
  LPCSTR szStartA, szEndA;
  CStringA cStrTempA;

  if (szProductA == NULL)
    return E_POINTER;
  //skip spaces
  szProductA = SkipSpaces(szProductA);
  //get upgrade
  szProductA = GetToken(szStartA = szProductA);
  //check slash separator
  if (*szProductA == '/')
  {
    szProductA = GetToken(++szProductA);
    if (*(szProductA-1) == '/' || *szProductA == '/') //no subtype?
      return MX_E_InvalidData;
  }
  else if (szProductA == szStartA)
  {
    return MX_E_InvalidData;
  }
  szEndA = szProductA;
  //skip spaces and check for end
  if (*SkipSpaces(szProductA) != 0)
    return MX_E_InvalidData;
  //set new value
  if (cStrTempA.CopyN(szStartA, (SIZE_T)(szEndA-szStartA)) == FALSE)
    return E_OUTOFMEMORY;
  if (cProductsList.AddElement((LPSTR)cStrTempA) == FALSE)
    return E_OUTOFMEMORY;
  //done
  cStrTempA.Detach();
  return S_OK;
}

SIZE_T CHttpHeaderGenUpgrade::GetProductsCount() const
{
  return cProductsList.GetCount();
}

LPCSTR CHttpHeaderGenUpgrade::GetProduct(__in SIZE_T nIndex) const
{
  return (nIndex < cProductsList.GetCount()) ? cProductsList.GetElementAt(nIndex) : NULL;
}

BOOL CHttpHeaderGenUpgrade::HasProduct(__in_z LPCSTR szProductA) const
{
  SIZE_T i, nCount;

  if (szProductA != NULL && szProductA[0] != 0)
  {
    nCount = cProductsList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (StrCompareA(cProductsList.GetElementAt(i), szProductA, TRUE) == 0)
        return TRUE;
    }
  }
  return FALSE;
}

} //namespace MX
