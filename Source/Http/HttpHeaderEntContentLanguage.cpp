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
#include "..\..\Include\Http\HttpHeaderEntContentLanguage.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderEntContentLanguage::CHttpHeaderEntContentLanguage() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderEntContentLanguage::~CHttpHeaderEntContentLanguage()
{
  return;
}

HRESULT CHttpHeaderEntContentLanguage::Parse(_In_z_ LPCSTR szValueA)
{
  LPCSTR szStartA;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //mark start
  szStartA = szValueA;
  //get language
  while (*szValueA >= 0x21 && *szValueA <= 0x7E)
    szValueA++;
  //set language
  hRes = SetLanguage(szStartA, (SIZE_T)(szValueA - szStartA));
  if (FAILED(hRes))
    return hRes;
  //skip spaces and check for end
  if (*SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentLanguage::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  if (cStrDestA.CopyN((LPCSTR)cStrLanguageA, cStrLanguageA.GetLength()) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentLanguage::SetLanguage(_In_z_ LPCSTR szLanguageA, _In_ SIZE_T nLanguageLen)
{
  LPCSTR szLanguageEndA, szStartA[2];

  if (nLanguageLen == (SIZE_T)-1)
    nLanguageLen = StrLenA(szLanguageA);
  if (nLanguageLen == 0)
    return MX_E_InvalidData;
  if (szLanguageA == NULL)
    return E_POINTER;
  szLanguageEndA = szLanguageA + nLanguageLen;

  //get language
  szStartA[0] = szLanguageA;
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

  //check for end
  if (szLanguageA != szLanguageEndA)
    return MX_E_InvalidData;
  //set new value
  if (cStrLanguageA.CopyN(szStartA[0], (SIZE_T)(szLanguageEndA - szStartA[0])) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCSTR CHttpHeaderEntContentLanguage::GetLanguage() const
{
  return (LPCSTR)cStrLanguageA;
}

} //namespace MX
