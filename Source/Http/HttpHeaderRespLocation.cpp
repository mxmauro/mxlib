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
#include "..\..\Include\Http\HttpHeaderRespLocation.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderRespLocation::CHttpHeaderRespLocation() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderRespLocation::~CHttpHeaderRespLocation()
{
  return;
}

HRESULT CHttpHeaderRespLocation::Parse(_In_z_ LPCSTR szValueA)
{
  return SetLocation(szValueA);
}

HRESULT CHttpHeaderRespLocation::Build(_Inout_ CStringA &cStrDestA)
{
  if (cStrDestA.Copy((LPSTR)cStrLocationA) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

HRESULT CHttpHeaderRespLocation::SetLocation(_In_z_ LPCSTR szLocationA)
{
  LPCSTR szStartA, szEndA;
  CUrl cUrl;
  HRESULT hRes;

  if (szLocationA == NULL)
    return E_POINTER;
  //skip spaces
  szLocationA = SkipSpaces(szLocationA);
  //get location
  szLocationA = Advance(szStartA = szLocationA, " \t");
  if (szLocationA == szStartA)
    return MX_E_InvalidData;
  szEndA = szLocationA;
  //skip spaces and check for end
  if (*CHttpHeaderBase::SkipSpaces(szLocationA) != 0)
    return MX_E_InvalidData;
  //some checks
  hRes = cUrl.ParseFromString(szStartA, (SIZE_T)(szEndA-szStartA));
  if (FAILED(hRes))
    return hRes;
  //set new value
  if (cStrLocationA.CopyN(szStartA, (SIZE_T)(szEndA-szStartA)) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCSTR CHttpHeaderRespLocation::GetLocation() const
{
  return (LPCSTR)cStrLocationA;
}

} //namespace MX
