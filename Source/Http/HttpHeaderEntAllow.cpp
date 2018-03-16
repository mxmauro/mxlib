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
  CStringA cStrTempA;
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
    bGotItem = TRUE;
    //get verb
    szValueA = Advance(szStartA = szValueA, ", \t");
    if (szValueA == szStartA)
      return MX_E_InvalidData;
    if (cStrTempA.CopyN(szStartA, (SIZE_T)(szValueA-szStartA)) == FALSE)
      return E_OUTOFMEMORY;
    hRes = AddVerb((LPCSTR)cStrTempA);
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

HRESULT CHttpHeaderEntAllow::Build(_Inout_ CStringA &cStrDestA)
{
  SIZE_T i, nCount;

  cStrDestA.Empty();
  nCount = cVerbsList.GetCount();
  if (nCount == 0)
    return S_OK;
  //fill verbs
  for (i=0; i<nCount; i++)
  {
    if (cStrDestA.AppendFormat("%s%s", ((i > 0) ? "," : ""), cVerbsList.GetElementAt(i)) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntAllow::AddVerb(_In_z_ LPCSTR szVerbA)
{
  LPCSTR szStartA, szEndA;
  CStringA cStrTempA;

  if (szVerbA == NULL)
    return E_POINTER;
  //skip spaces
  szVerbA = SkipSpaces(szVerbA);
  //get verb
  szVerbA = GetToken(szStartA = szVerbA);
  if (szVerbA == szStartA)
    return MX_E_InvalidData;
  szEndA = szVerbA;
  //skip spaces and check for end
  if (*SkipSpaces(szVerbA) != 0)
    return MX_E_InvalidData;
  //convert to uppercase, validate and set new value
  if (cStrTempA.Copy(szVerbA) == FALSE)
    return E_OUTOFMEMORY;
  StrToUpperA((LPSTR)cStrTempA);
  if (CHttpCommon::IsValidVerb((LPSTR)cStrTempA) == FALSE)
    return MX_E_InvalidData;
  if (cVerbsList.AddElement((LPSTR)cStrTempA) == FALSE)
    return E_OUTOFMEMORY;
  //done
  cStrTempA.Detach();
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

  if (szVerbA != NULL && szVerbA[0] != 0)
  {
    nCount = cVerbsList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (StrCompareA(cVerbsList.GetElementAt(i), szVerbA, TRUE) == 0)
        return TRUE;
    }
  }
  return FALSE;
}

} //namespace MX
