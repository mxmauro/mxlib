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
#include "..\..\Include\Http\HttpHeaderGeneric.h"
#include <stdlib.h>

//-----------------------------------------------------------

namespace MX {

CHttpHeaderGeneric::CHttpHeaderGeneric() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderGeneric::~CHttpHeaderGeneric()
{
  return;
}

HRESULT CHttpHeaderGeneric::SetHeaderName(_In_z_ LPCSTR szNameA)
{
  SIZE_T i;

  if (szNameA == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  for (i = 0; szNameA[i] != 0; i++)
  {
    if (CHttpCommon::IsValidNameChar(szNameA[i]) == FALSE)
      return E_INVALIDARG;
  }
  return (cStrHeaderNameA.Copy(szNameA) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

LPCSTR CHttpHeaderGeneric::GetHeaderName() const
{
  return (LPCSTR)cStrHeaderNameA;
}

HRESULT CHttpHeaderGeneric::Parse(_In_z_ LPCSTR szValueA)
{
  return SetValue(szValueA);
}

HRESULT CHttpHeaderGeneric::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  if (cStrDestA.CopyN((LPCSTR)cStrValueA, cStrValueA.GetLength()) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

HRESULT CHttpHeaderGeneric::SetValue(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  LPCSTR szStartA, szEndA, szValueEndA;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  if (nValueLen == 0)
    return MX_E_InvalidData;
  if (szValueA == NULL)
    return E_POINTER;
  szValueEndA = szValueA + nValueLen;
  //parse value
  szStartA = szEndA = szValueA = SkipSpaces(szValueA, nValueLen);
  while (szValueA < szValueEndA)
  {
    if (*((LPBYTE)szValueA) < 32)
      return MX_E_InvalidData;
    szEndA = ++szValueA;
    szValueA = SkipSpaces(szValueA, (SIZE_T)(szValueEndA - szValueA));
  }
  //set value
  if (cStrValueA.CopyN(szStartA, (SIZE_T)(szEndA - szStartA)) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCSTR CHttpHeaderGeneric::GetValue() const
{
  return (LPSTR)cStrValueA;
}

} //namespace MX
