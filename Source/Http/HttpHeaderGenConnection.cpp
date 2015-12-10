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
#include "..\..\Include\Http\HttpHeaderGenConnection.h"
#include <intsafe.h>

//-----------------------------------------------------------

namespace MX {

CHttpHeaderGenConnection::CHttpHeaderGenConnection() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderGenConnection::~CHttpHeaderGenConnection()
{
  cConnectionsList.RemoveAllElements();
  return;
}

HRESULT CHttpHeaderGenConnection::Parse(__in_z LPCSTR szValueA)
{
  CStringA cStrTempA;
  LPCSTR szStartA;
  BOOL bGotItem;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //parse tokens
  bGotItem = FALSE;
  do
  {
    //skip spaces
    szValueA = SkipSpaces(szValueA);
    if (*szValueA == 0)
      break;
    bGotItem = TRUE;
    //connection
    szValueA = Advance(szStartA = szValueA, ",");
    if (cStrTempA.CopyN(szStartA, (SIZE_T)(szValueA-szStartA)) == FALSE)
      return E_OUTOFMEMORY;
    hRes = AddConnection((LPSTR)cStrTempA);
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

HRESULT CHttpHeaderGenConnection::Build(__inout CStringA &cStrDestA)
{
  SIZE_T i, nCount;

  cStrDestA.Empty();
  nCount = cConnectionsList.GetCount();
  for (i=0; i<nCount; i++)
  {
    if (cStrDestA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(",", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrDestA.Concat(cConnectionsList.GetElementAt(i)) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderGenConnection::AddConnection(__in_z LPCSTR szConnectionA)
{
  CStringA cStrTempA;
  LPCSTR szStartA, szEndA;

  if (szConnectionA == NULL)
    return E_POINTER;
  //skip spaces
  szConnectionA = CHttpHeaderBase::SkipSpaces(szConnectionA);
  //get token
  szConnectionA = CHttpHeaderBase::GetToken(szStartA = szConnectionA);
  if (szStartA == szConnectionA)
    return MX_E_InvalidData;
  szEndA = szConnectionA;
  //skip spaces and check for end
  if (*CHttpHeaderBase::SkipSpaces(szConnectionA) != 0)
    return MX_E_InvalidData;
  //create new item
  if (cStrTempA.CopyN(szStartA, (SIZE_T)(szEndA-szStartA)) == FALSE)
    return E_OUTOFMEMORY;
  if (cConnectionsList.AddElement((LPSTR)cStrTempA) == FALSE)
    return E_OUTOFMEMORY;
  //done
  cStrTempA.Detach();
  return S_OK;
}

SIZE_T CHttpHeaderGenConnection::GetConnectionsCount() const
{
  return cConnectionsList.GetCount();
}

LPCSTR CHttpHeaderGenConnection::GetConnection(__in SIZE_T nIndex) const
{
  return (nIndex < cConnectionsList.GetCount()) ? cConnectionsList.GetElementAt(nIndex) : NULL;
}

BOOL CHttpHeaderGenConnection::HasConnection(__in_z LPCSTR szConnectionA) const
{
  SIZE_T i, nCount;

  if (szConnectionA != NULL && szConnectionA[0] != 0)
  {
    nCount = cConnectionsList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (StrCompareA(cConnectionsList.GetElementAt(i), szConnectionA, TRUE) == 0)
        return TRUE;
    }
  }
  return FALSE;
}

} //namespace MX
