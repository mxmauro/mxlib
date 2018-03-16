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
#include "..\..\Include\Http\HttpHeaderReqReferer.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqReferer::CHttpHeaderReqReferer() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderReqReferer::~CHttpHeaderReqReferer()
{
  return;
}

HRESULT CHttpHeaderReqReferer::Parse(_In_z_ LPCSTR szValueA)
{
  return SetReferer(szValueA);
}

HRESULT CHttpHeaderReqReferer::Build(_Inout_ CStringA &cStrDestA)
{
  if (cStrDestA.Copy((LPSTR)cStrRefererA) == FALSE)
  {
    cStrDestA.Empty();
    return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderReqReferer::SetReferer(_In_z_ LPCSTR szRefererA)
{
  LPCSTR szStartA, szEndA;
  CUrl cUrl;
  HRESULT hRes;

  if (szRefererA == NULL)
    return E_POINTER;
  //skip spaces
  szRefererA = SkipSpaces(szRefererA);
  //get host
  szRefererA = Advance(szStartA = szRefererA, " \t");
  if (szRefererA == szStartA)
    return MX_E_InvalidData;
  szEndA = szRefererA;
  //skip spaces and check for end
  if (*CHttpHeaderBase::SkipSpaces(szRefererA) != 0)
    return MX_E_InvalidData;
  //set new value
  hRes = cUrl.ParseFromString(szStartA, (SIZE_T)(szEndA-szStartA));
  if (SUCCEEDED(hRes))
  {
    if (cStrRefererA.CopyN(szStartA, (SIZE_T)(szEndA-szStartA)) == FALSE)
      hRes = E_OUTOFMEMORY;
  }
  //done
  return hRes;
}

LPCSTR CHttpHeaderReqReferer::GetReferer() const
{
  return (LPSTR)cStrRefererA;
}

} //namespace MX
