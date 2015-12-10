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

HRESULT CHttpHeaderEntContentLanguage::Parse(__in_z LPCSTR szValueA)
{
  return SetLanguage(szValueA);
}

HRESULT CHttpHeaderEntContentLanguage::Build(__inout CStringA &cStrDestA)
{
  if (cStrDestA.Copy((LPSTR)cStrLanguageA) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentLanguage::SetLanguage(__in_z LPCSTR szLanguageA)
{
  LPCSTR szStartA, szStartA_2, szEndA;

  if (szLanguageA == NULL)
    return E_POINTER;
  //skip spaces
  szLanguageA = CHttpHeaderBase::SkipSpaces(szLanguageA);
  //mark start
  szStartA = szLanguageA;
  //get language
  while ((*szLanguageA >= 'A' && *szLanguageA <= 'Z') || (*szLanguageA >= 'a' && *szLanguageA <= 'z'))
    szLanguageA++;
  if (szLanguageA == szStartA || szLanguageA > szStartA+8)
    return MX_E_InvalidData;
  if (*szLanguageA == '-')
  {
    szStartA_2 = ++szLanguageA;
    while ((*szLanguageA >= 'A' && *szLanguageA <= 'Z') || (*szLanguageA >= 'a' && *szLanguageA <= 'z'))
      szLanguageA++;
    if (szLanguageA == szStartA_2 || szLanguageA > szStartA_2+8)
      return MX_E_InvalidData;
  }
  szEndA = szLanguageA;
  //skip spaces and check for end
  if (*SkipSpaces(szLanguageA) != 0)
    return MX_E_InvalidData;
  //set new value
  if (cStrLanguageA.CopyN(szStartA, (SIZE_T)(szEndA-szStartA)) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCSTR CHttpHeaderEntContentLanguage::GetLanguage() const
{
  return (LPCSTR)cStrLanguageA;
}

} //namespace MX
