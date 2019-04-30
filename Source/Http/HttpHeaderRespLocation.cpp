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
  LPCSTR szStartA;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;

  //skip spaces
  szValueA = SkipSpaces(szValueA);

  //get location
  szValueA = SkipUntil(szStartA = szValueA, " \t");
  if (szValueA == szStartA)
    return MX_E_InvalidData;

  //set location
  hRes = SetLocation(szStartA, (SIZE_T)(szValueA - szStartA));
  if (FAILED(hRes))
    return hRes;

  //skip spaces and check for end
  if (*CHttpHeaderBase::SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;
  //done
  return S_OK;
}

HRESULT CHttpHeaderRespLocation::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  return (cStrDestA.Copy((LPCSTR)cStrLocationA) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

HRESULT CHttpHeaderRespLocation::SetLocation(_In_z_ LPCSTR szLocationA, _In_ SIZE_T nLocationLen)
{
  CUrl cUrl;
  HRESULT hRes;

  if (nLocationLen == (SIZE_T)-1)
    nLocationLen = StrLenA(szLocationA);
  if (nLocationLen == 0)
    return MX_E_InvalidData;
  if (szLocationA == NULL)
    return E_POINTER;
  //some checks
  hRes = cUrl.ParseFromString(szLocationA, nLocationLen);
  if (FAILED(hRes))
    return hRes;
  //set new value
  if (cStrLocationA.CopyN(szLocationA, nLocationLen) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCSTR CHttpHeaderRespLocation::GetLocation() const
{
  return (LPCSTR)cStrLocationA;
}

} //namespace MX
