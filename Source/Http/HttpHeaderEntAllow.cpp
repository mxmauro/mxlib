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
#include "..\..\Include\Http\HttpHeaderEntAllow.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderEntAllow::CHttpHeaderEntAllow() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderEntAllow::~CHttpHeaderEntAllow()
{
  cVerbsList.RemoveAllElements();
  return;
}

HRESULT CHttpHeaderEntAllow::Parse(_In_z_ LPCSTR szValueA)
{
  LPCSTR szStartA;
  BOOL bGotItem;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //parse verbs
  bGotItem = FALSE;
  do
  {
    //skip spaces
    szValueA = SkipSpaces(szValueA);
    if (*szValueA == 0)
      break;

    //get verb
    szValueA = GetToken(szStartA = szValueA);
    if (szValueA == szStartA)
      goto skip_null_listitem;

    bGotItem = TRUE;

    //add to list
    hRes = AddVerb(szStartA, (SIZE_T)(szValueA - szStartA));
    if (FAILED(hRes))
      return hRes;

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

HRESULT CHttpHeaderEntAllow::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  SIZE_T i, nCount;

  cStrDestA.Empty();
  //fill verbs
  nCount = cVerbsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (cStrDestA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(",", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrDestA.Concat(cVerbsList.GetElementAt(i)) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntAllow::AddVerb(_In_z_ LPCSTR szVerbA, _In_ SIZE_T nVerbLen)
{
  CStringA cStrTempA;

  if (nVerbLen == (SIZE_T)-1)
    nVerbLen = StrLenA(szVerbA);
  if (nVerbLen == 0)
    return MX_E_InvalidData;
  if (szVerbA == NULL)
    return E_POINTER;
  //convert to uppercase, validate and set new value
  if (cStrTempA.CopyN(szVerbA, nVerbLen) == FALSE)
    return E_OUTOFMEMORY;
  StrToUpperA((LPSTR)cStrTempA);
  //validate
  if (CHttpCommon::IsValidVerb((LPCSTR)cStrTempA, cStrTempA.GetLength()) == FALSE)
    return MX_E_InvalidData;
  //add to list if not there
  if (HasVerb((LPCSTR)cStrTempA) == FALSE)
  {
    if (cVerbsList.AddElement((LPSTR)cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
    cStrTempA.Detach();
  }
  //done
  return S_OK;
}

SIZE_T CHttpHeaderEntAllow::GetVerbsCount() const
{
  return cVerbsList.GetCount();
}

LPCSTR CHttpHeaderEntAllow::GetVerb(_In_ SIZE_T nIndex) const
{
  return (nIndex < cVerbsList.GetCount()) ? cVerbsList.GetElementAt(nIndex) : NULL;
}

BOOL CHttpHeaderEntAllow::HasVerb(_In_z_ LPCSTR szVerbA) const
{
  SIZE_T i, nCount;

  if (szVerbA != NULL && *szVerbA != 0)
  {
    nCount = cVerbsList.GetCount();
    for (i=0; i < nCount; i++)
    {
      if (StrCompareA(cVerbsList.GetElementAt(i), szVerbA, TRUE) == 0)
        return TRUE;
    }
  }
  return FALSE;
}

} //namespace MX
