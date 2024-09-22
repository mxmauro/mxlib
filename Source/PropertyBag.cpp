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
#include "..\Include\PropertyBag.h"
#include "..\Include\WaitableObjects.h"
#include "..\Include\Strings\Strings.h"

//-----------------------------------------------------------

namespace MX {

CPropertyBag::CPropertyBag() : CBaseMemObj(), CNonCopyableObj()
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

HRESULT CPropertyBag::Clear(_In_z_ LPCSTR szNameA)
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

HRESULT CPropertyBag::SetNull(_In_z_ LPCSTR szNameA)
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
  MxMemCopy(lpNewProp->szNameA, szNameA, nNameLen);
  lpNewProp->nType = eType::Null;
  if (Insert(lpNewProp) == FALSE)
  {
    MX_FREE(lpNewProp);
    return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CPropertyBag::SetDWord(_In_z_ LPCSTR szNameA, _In_ DWORD dwValue)
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
  MxMemCopy(lpNewProp->szNameA, szNameA, nNameLen);
  lpNewProp->nType = eType::DWord;
  lpNewProp->u.dwValue = dwValue;
  if (Insert(lpNewProp) == FALSE)
  {
    MX_FREE(lpNewProp);
    return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CPropertyBag::SetQWord(_In_z_ LPCSTR szNameA, _In_ ULONGLONG ullValue)
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
  MxMemCopy(lpNewProp->szNameA, szNameA, nNameLen);
  lpNewProp->nType = eType::QWord;
  lpNewProp->u.ullValue = ullValue;
  if (Insert(lpNewProp) == FALSE)
  {
    MX_FREE(lpNewProp);
    return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CPropertyBag::SetDouble(_In_z_ LPCSTR szNameA, _In_ double nValue)
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
  MxMemCopy(lpNewProp->szNameA, szNameA, nNameLen);
  lpNewProp->nType = eType::Double;
  lpNewProp->u.dblValue = nValue;
  if (Insert(lpNewProp) == FALSE)
  {
    MX_FREE(lpNewProp);
    return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CPropertyBag::SetString(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA)
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
  MxMemCopy(lpNewProp->szNameA, szNameA, nNameLen);
  lpNewProp->nType = eType::AnsiString;
  lpNewProp->u.szValueA = (LPSTR)((LPBYTE)(lpNewProp+1) + nNameLen);
  MxMemCopy(lpNewProp->u.szValueA, szValueA, nValueLen);
  if (Insert(lpNewProp) == FALSE)
  {
    MX_FREE(lpNewProp);
    return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CPropertyBag::SetString(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW)
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
  MxMemCopy(lpNewProp->szNameA, szNameA, nNameLen);
  lpNewProp->nType = eType::WideString;
  lpNewProp->u.szValueW = (LPWSTR)((LPBYTE)(lpNewProp+1) + nNameLen);
  MxMemCopy(lpNewProp->u.szValueW, szValueW, nValueLen);
  if (Insert(lpNewProp) == FALSE)
  {
    MX_FREE(lpNewProp);
    return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

LPCSTR CPropertyBag::GetAt(_In_ SIZE_T nIndex) const
{
  if (nIndex >= cPropertiesList.GetCount())
    return NULL;
  //done
  return cPropertiesList[nIndex]->szNameA;
}

CPropertyBag::eType CPropertyBag::GetType(_In_ SIZE_T nIndex) const
{
  if (nIndex >= cPropertiesList.GetCount())
    return eType::Undefined;
  //done
  return cPropertiesList[nIndex]->nType;
}

CPropertyBag::eType CPropertyBag::GetType(_In_z_ LPCSTR szNameA) const
{
  SIZE_T nIndex;

  if (szNameA == NULL || *szNameA == 0)
    return eType::Undefined;
  nIndex = const_cast<CPropertyBag*>(this)->Find(szNameA);
  if (nIndex == (SIZE_T)-1)
    return eType::Undefined;
  //done
  return cPropertiesList[nIndex]->nType;
}

HRESULT CPropertyBag::GetDWord(_In_ SIZE_T nIndex, _Out_ DWORD &dwValue, _In_opt_ DWORD dwDefValue)
{
  dwValue = dwDefValue;
  if (nIndex >= cPropertiesList.GetCount())
    return E_INVALIDARG;
  if (cPropertiesList[nIndex]->nType != eType::DWord)
    return E_FAIL;
  dwValue = cPropertiesList[nIndex]->u.dwValue;
  //done
  return S_OK;
}

HRESULT CPropertyBag::GetQWord(_In_ SIZE_T nIndex, _Out_ ULONGLONG &ullValue, _In_opt_ ULONGLONG ullDefValue)
{
  ullValue = ullDefValue;
  if (nIndex >= cPropertiesList.GetCount())
    return E_INVALIDARG;
  if (cPropertiesList[nIndex]->nType != eType::QWord)
    return E_FAIL;
  ullValue = cPropertiesList[nIndex]->u.ullValue;
  //done
  return S_OK;
}

HRESULT CPropertyBag::GetDouble(_In_ SIZE_T nIndex, _Out_ double &nValue, _In_opt_ double nDefValue)
{
  nValue = nDefValue;
  if (nIndex >= cPropertiesList.GetCount())
    return E_INVALIDARG;
  if (cPropertiesList[nIndex]->nType != eType::Double)
    return E_FAIL;
  nValue = cPropertiesList[nIndex]->u.dblValue;
  //done
  return S_OK;
}

HRESULT CPropertyBag::GetString(_In_ SIZE_T nIndex, _Out_ LPCSTR &szValueA, _In_opt_z_ LPCSTR szDefValueA)
{
  szValueA = szDefValueA;
  if (nIndex >= cPropertiesList.GetCount())
    return E_INVALIDARG;
  if (cPropertiesList[nIndex]->nType != eType::AnsiString)
    return E_FAIL;
  szValueA = cPropertiesList[nIndex]->u.szValueA;
  //done
  return S_OK;
}

HRESULT CPropertyBag::GetString(_In_ SIZE_T nIndex, _Out_ LPCWSTR &szValueW, _In_opt_z_ LPCWSTR szDefValueW)
{
  szValueW = szDefValueW;
  if (nIndex >= cPropertiesList.GetCount())
    return E_INVALIDARG;
  if (cPropertiesList[nIndex]->nType != eType::WideString)
    return E_FAIL;
  szValueW = cPropertiesList[nIndex]->u.szValueW;
  //done
  return S_OK;
}

HRESULT CPropertyBag::GetDWord(_In_z_ LPCSTR szNameA, _Out_ DWORD &dwValue, _In_opt_ DWORD dwDefValue)
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
  if (cPropertiesList[nIndex]->nType != eType::DWord)
    return E_FAIL;
  dwValue = cPropertiesList[nIndex]->u.dwValue;
  //done
  return S_OK;
}

HRESULT CPropertyBag::GetQWord(_In_z_ LPCSTR szNameA, _Out_ ULONGLONG &ullValue, _In_opt_ ULONGLONG ullDefValue)
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
  if (cPropertiesList[nIndex]->nType != eType::QWord)
    return E_FAIL;
  ullValue = cPropertiesList[nIndex]->u.ullValue;
  //done
  return S_OK;
}

HRESULT CPropertyBag::GetDouble(_In_z_ LPCSTR szNameA, _Out_ double &nValue, _In_opt_ double nDefValue)
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
  if (cPropertiesList[nIndex]->nType != eType::Double)
    return E_FAIL;
  nValue = cPropertiesList[nIndex]->u.dblValue;
  //done
  return S_OK;
}

HRESULT CPropertyBag::GetString(_In_z_ LPCSTR szNameA, _Out_ LPCSTR &szValueA, _In_opt_z_ LPCSTR szDefValueA)
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
  if (cPropertiesList[nIndex]->nType != eType::AnsiString)
    return E_FAIL;
  szValueA = cPropertiesList[nIndex]->u.szValueA;
  //done
  return S_OK;
}

HRESULT CPropertyBag::GetString(_In_z_ LPCSTR szNameA, _Out_ LPCWSTR &szValueW, _In_opt_z_ LPCWSTR szDefValueW)
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
  if (cPropertiesList[nIndex]->nType != eType::WideString)
    return E_FAIL;
  szValueW = cPropertiesList[nIndex]->u.szValueW;
  //done
  return S_OK;
}


BOOL CPropertyBag::Insert(_In_ PROPERTY *lpNewProp)
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
  return cPropertiesList.SortedInsert(lpNewProp, &CPropertyBag::InsertCompareFunc, NULL);
}

SIZE_T CPropertyBag::Find(_In_z_ LPCSTR szNameA)
{
  return cPropertiesList.BinarySearch(szNameA, &CPropertyBag::SearchCompareFunc, NULL);
}

} //namespace MX
