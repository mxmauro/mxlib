/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2015. All rights reserved.
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
#include "..\Include\PropertyBag.h"
#include "..\Include\WaitableObjects.h"
#include "..\Include\Strings\Strings.h"

//-----------------------------------------------------------

namespace MX {

CPropertyBag::CPropertyBag() : CBaseMemObj()
{
  return;
}

CPropertyBag::~CPropertyBag()
{
  Reset();
  return;
}

VOID CPropertyBag::Reset()
{
  cPropertiesList.RemoveAllElements();
  return;
}

HRESULT CPropertyBag::Clear(__in_z LPCSTR szNameA)
{
  SIZE_T nIndex;

  if (szNameA == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  nIndex = Find(szNameA);
  if (nIndex == (SIZE_T)-1)
    return MX_E_NotFound;
  //clear element
  cPropertiesList.RemoveElementAt(nIndex);
  //done
  return S_OK;
}

HRESULT CPropertyBag::SetNull(__in_z LPCSTR szNameA)
{
  PROPERTY *lpNewProp;
  SIZE_T nNameLen;

  if (szNameA == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  //create new property
  nNameLen = StrLenA(szNameA) + 1;
  lpNewProp = (PROPERTY*)MX_MALLOC(sizeof(PROPERTY) + nNameLen);
  if (lpNewProp == NULL)
    return E_OUTOFMEMORY;
  lpNewProp->szNameA = (LPSTR)(lpNewProp + 1);
  MemCopy(lpNewProp->szNameA, szNameA, nNameLen);
  lpNewProp->nType = PropertyTypeNull;
  if (Insert(lpNewProp) == FALSE)
  {
    MX_FREE(lpNewProp);
    return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CPropertyBag::SetDWord(__in_z LPCSTR szNameA, __in DWORD dwValue)
{
  PROPERTY *lpNewProp;
  SIZE_T nNameLen;

  if (szNameA == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  //create new property
  nNameLen = StrLenA(szNameA) + 1;
  lpNewProp = (PROPERTY*)MX_MALLOC(sizeof(PROPERTY) + nNameLen);
  if (lpNewProp == NULL)
    return E_OUTOFMEMORY;
  lpNewProp->szNameA = (LPSTR)(lpNewProp + 1);
  MemCopy(lpNewProp->szNameA, szNameA, nNameLen);
  lpNewProp->nType = PropertyTypeDWord;
  lpNewProp->u.dwValue = dwValue;
  if (Insert(lpNewProp) == FALSE)
  {
    MX_FREE(lpNewProp);
    return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CPropertyBag::SetQWord(__in_z LPCSTR szNameA, __in ULONGLONG ullValue)
{
  PROPERTY *lpNewProp;
  SIZE_T nNameLen;

  if (szNameA == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  //create new property
  nNameLen = StrLenA(szNameA) + 1;
  lpNewProp = (PROPERTY*)MX_MALLOC(sizeof(PROPERTY) + nNameLen);
  if (lpNewProp == NULL)
    return E_OUTOFMEMORY;
  lpNewProp->szNameA = (LPSTR)(lpNewProp + 1);
  MemCopy(lpNewProp->szNameA, szNameA, nNameLen);
  lpNewProp->nType = PropertyTypeQWord;
  lpNewProp->u.ullValue = ullValue;
  if (Insert(lpNewProp) == FALSE)
  {
    MX_FREE(lpNewProp);
    return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CPropertyBag::SetDouble(__in_z LPCSTR szNameA, __in double nValue)
{
  PROPERTY *lpNewProp;
  SIZE_T nNameLen;

  if (szNameA == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  //create new property
  nNameLen = StrLenA(szNameA) + 1;
  lpNewProp = (PROPERTY*)MX_MALLOC(sizeof(PROPERTY) + nNameLen);
  if (lpNewProp == NULL)
    return E_OUTOFMEMORY;
  lpNewProp->szNameA = (LPSTR)(lpNewProp + 1);
  MemCopy(lpNewProp->szNameA, szNameA, nNameLen);
  lpNewProp->nType = PropertyTypeDouble;
  lpNewProp->u.dblValue = nValue;
  if (Insert(lpNewProp) == FALSE)
  {
    MX_FREE(lpNewProp);
    return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CPropertyBag::SetString(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA)
{
  PROPERTY *lpNewProp;
  SIZE_T nNameLen, nValueLen;

  if (szNameA == NULL || szValueA == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  //create new property
  nNameLen = StrLenA(szNameA) + 1;
  nValueLen = StrLenA(szValueA) + 1;
  lpNewProp = (PROPERTY*)MX_MALLOC(sizeof(PROPERTY) + nNameLen + nValueLen);
  if (lpNewProp == NULL)
    return E_OUTOFMEMORY;
  lpNewProp->szNameA = (LPSTR)(lpNewProp + 1);
  MemCopy(lpNewProp->szNameA, szNameA, nNameLen);
  lpNewProp->nType = PropertyTypeAnsiString;
  lpNewProp->u.szValueA = (LPSTR)((LPBYTE)(lpNewProp+1) + nNameLen);
  MemCopy(lpNewProp->u.szValueA, szValueA, nValueLen);
  if (Insert(lpNewProp) == FALSE)
  {
    MX_FREE(lpNewProp);
    return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CPropertyBag::SetString(__in_z LPCSTR szNameA, __in_z LPCWSTR szValueW)
{
  PROPERTY *lpNewProp;
  SIZE_T nNameLen, nValueLen;

  if (szNameA == NULL || szValueW == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  //create new property
  nNameLen = StrLenA(szNameA) + 1;
  nValueLen = (StrLenW(szValueW) + 1) * sizeof(WCHAR);
  lpNewProp = (PROPERTY*)MX_MALLOC(sizeof(PROPERTY) + nNameLen + nValueLen);
  if (lpNewProp == NULL)
    return E_OUTOFMEMORY;
  lpNewProp->szNameA = (LPSTR)(lpNewProp + 1);
  MemCopy(lpNewProp->szNameA, szNameA, nNameLen);
  lpNewProp->nType = PropertyTypeWideString;
  lpNewProp->u.szValueW = (LPWSTR)((LPBYTE)(lpNewProp+1) + nNameLen);
  MemCopy(lpNewProp->u.szValueW, szValueW, nValueLen);
  if (Insert(lpNewProp) == FALSE)
  {
    MX_FREE(lpNewProp);
    return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

LPCSTR CPropertyBag::GetAt(__in SIZE_T nIndex) const
{
  if (nIndex >= cPropertiesList.GetCount())
    return NULL;
  //done
  return cPropertiesList[nIndex]->szNameA;
}

CPropertyBag::eType CPropertyBag::GetType(__in SIZE_T nIndex) const
{
  if (nIndex >= cPropertiesList.GetCount())
    return PropertyTypeUndefined;
  //done
  return cPropertiesList[nIndex]->nType;
}

CPropertyBag::eType CPropertyBag::GetType(__in_z LPCSTR szNameA) const
{
  SIZE_T nIndex;

  if (szNameA == NULL || *szNameA == 0)
    return PropertyTypeUndefined;
  nIndex = const_cast<CPropertyBag*>(this)->Find(szNameA);
  if (nIndex == (SIZE_T)-1)
    return PropertyTypeUndefined;
  //done
  return cPropertiesList[nIndex]->nType;
}

HRESULT CPropertyBag::GetDWord(__in SIZE_T nIndex, __out DWORD &dwValue, __in_opt DWORD dwDefValue)
{
  dwValue = dwDefValue;
  if (nIndex >= cPropertiesList.GetCount())
    return E_INVALIDARG;
  if (cPropertiesList[nIndex]->nType != PropertyTypeDWord)
    return E_FAIL;
  dwValue = cPropertiesList[nIndex]->u.dwValue;
  //done
  return S_OK;
}

HRESULT CPropertyBag::GetQWord(__in SIZE_T nIndex, __out ULONGLONG &ullValue, __in_opt ULONGLONG ullDefValue)
{
  ullValue = ullDefValue;
  if (nIndex >= cPropertiesList.GetCount())
    return E_INVALIDARG;
  if (cPropertiesList[nIndex]->nType != PropertyTypeQWord)
    return E_FAIL;
  ullValue = cPropertiesList[nIndex]->u.ullValue;
  //done
  return S_OK;
}

HRESULT CPropertyBag::GetDouble(__in SIZE_T nIndex, __out double &nValue, __in_opt double nDefValue)
{
  nValue = nDefValue;
  if (nIndex >= cPropertiesList.GetCount())
    return E_INVALIDARG;
  if (cPropertiesList[nIndex]->nType != PropertyTypeDouble)
    return E_FAIL;
  nValue = cPropertiesList[nIndex]->u.dblValue;
  //done
  return S_OK;
}

HRESULT CPropertyBag::GetString(__in SIZE_T nIndex, __out LPCSTR &szValueA, __in_z_opt LPCSTR szDefValueA)
{
  szValueA = szDefValueA;
  if (nIndex >= cPropertiesList.GetCount())
    return E_INVALIDARG;
  if (cPropertiesList[nIndex]->nType != PropertyTypeAnsiString)
    return E_FAIL;
  szValueA = cPropertiesList[nIndex]->u.szValueA;
  //done
  return S_OK;
}

HRESULT CPropertyBag::GetString(__in SIZE_T nIndex, __out LPCWSTR &szValueW, __in_z_opt LPCWSTR szDefValueW)
{
  szValueW = szDefValueW;
  if (nIndex >= cPropertiesList.GetCount())
    return E_INVALIDARG;
  if (cPropertiesList[nIndex]->nType != PropertyTypeWideString)
    return E_FAIL;
  szValueW = cPropertiesList[nIndex]->u.szValueW;
  //done
  return S_OK;
}

HRESULT CPropertyBag::GetDWord(__in_z LPCSTR szNameA, __out DWORD &dwValue, __in_opt DWORD dwDefValue)
{
  SIZE_T nIndex;

  dwValue = dwDefValue;
  if (szNameA == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  nIndex = Find(szNameA);
  if (nIndex == (SIZE_T)-1)
    return MX_E_NotFound;
  if (cPropertiesList[nIndex]->nType != PropertyTypeDWord)
    return E_FAIL;
  dwValue = cPropertiesList[nIndex]->u.dwValue;
  //done
  return S_OK;
}

HRESULT CPropertyBag::GetQWord(__in_z LPCSTR szNameA, __out ULONGLONG &ullValue, __in_opt ULONGLONG ullDefValue)
{
  SIZE_T nIndex;

  ullValue = ullDefValue;
  if (szNameA == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  nIndex = Find(szNameA);
  if (nIndex == (SIZE_T)-1)
    return MX_E_NotFound;
  if (cPropertiesList[nIndex]->nType != PropertyTypeQWord)
    return E_FAIL;
  ullValue = cPropertiesList[nIndex]->u.ullValue;
  //done
  return S_OK;
}

HRESULT CPropertyBag::GetDouble(__in_z LPCSTR szNameA, __out double &nValue, __in_opt double nDefValue)
{
  SIZE_T nIndex;

  nValue = nDefValue;
  if (szNameA == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  nIndex = Find(szNameA);
  if (nIndex == (SIZE_T)-1)
    return MX_E_NotFound;
  if (cPropertiesList[nIndex]->nType != PropertyTypeDouble)
    return E_FAIL;
  nValue = cPropertiesList[nIndex]->u.dblValue;
  //done
  return S_OK;
}

HRESULT CPropertyBag::GetString(__in_z LPCSTR szNameA, __out LPCSTR &szValueA, __in_z_opt LPCSTR szDefValueA)
{
  SIZE_T nIndex;

  szValueA = szDefValueA;
  if (szNameA == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  nIndex = Find(szNameA);
  if (nIndex == (SIZE_T)-1)
    return MX_E_NotFound;
  if (cPropertiesList[nIndex]->nType != PropertyTypeAnsiString)
    return E_FAIL;
  szValueA = cPropertiesList[nIndex]->u.szValueA;
  //done
  return S_OK;
}

HRESULT CPropertyBag::GetString(__in_z LPCSTR szNameA, __out LPCWSTR &szValueW, __in_z_opt LPCWSTR szDefValueW)
{
  SIZE_T nIndex;

  szValueW = szDefValueW;
  if (szNameA == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  nIndex = Find(szNameA);
  if (nIndex == (SIZE_T)-1)
    return MX_E_NotFound;
  if (cPropertiesList[nIndex]->nType != PropertyTypeWideString)
    return E_FAIL;
  szValueW = cPropertiesList[nIndex]->u.szValueW;
  //done
  return S_OK;
}


BOOL CPropertyBag::Insert(__in PROPERTY *lpNewProp)
{
  SIZE_T nIndex;

  nIndex = Find(lpNewProp->szNameA);
  if (nIndex != (SIZE_T)-1)
  {
    PROPERTY *lpOldProp = cPropertiesList.GetElementAt(nIndex);
    cPropertiesList[nIndex] = lpNewProp;
    MX_FREE(lpOldProp);
    return TRUE;
  }
  return cPropertiesList.SortedInsert(lpNewProp, &CPropertyBag::SearchCompareFunc, NULL);
}

SIZE_T CPropertyBag::Find(__in_z LPCSTR szNameA)
{
  PROPERTY sProp, *lpProp;

  sProp.szNameA = (LPSTR)szNameA;
  lpProp = &sProp;
  return cPropertiesList.BinarySearch(&lpProp, &CPropertyBag::SearchCompareFunc, NULL);
}

int CPropertyBag::SearchCompareFunc(__in LPVOID lpContext, __in PROPERTY **lpItem1, __in PROPERTY **lpItem2)
{
  return StrCompareA((*lpItem1)->szNameA, (*lpItem2)->szNameA, TRUE);
}

} //namespace MX
