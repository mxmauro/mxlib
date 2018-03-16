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
#include "..\..\Include\Http\HttpHeaderRespAge.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderRespAge::CHttpHeaderRespAge() : CHttpHeaderBase()
{
  nAge = 0ui64;
  return;
}

CHttpHeaderRespAge::~CHttpHeaderRespAge()
{
  return;
}

HRESULT CHttpHeaderRespAge::Parse(_In_z_ LPCSTR szValueA)
{
  ULONGLONG _nAge, nTemp;

  if (szValueA == NULL)
    return E_POINTER;
  _nAge = 0ui64;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //get first byte
  if (*szValueA < '0' || *szValueA > '9')
    return MX_E_InvalidData;
  while (*szValueA == '0')
    szValueA++;
  while (*szValueA >= '0' && *szValueA <= '9')
  {
    nTemp = _nAge * 10ui64;
    if (nTemp < _nAge)
      return MX_E_ArithmeticOverflow;
    _nAge = nTemp + (ULONGLONG)(*szValueA - '0');
    if (_nAge < nTemp)
      return MX_E_ArithmeticOverflow;
    szValueA++;
  }
  //skip spaces and check for end
  if (*SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;
  //set new value
  nAge = _nAge;
  //done
  return S_OK;
}

HRESULT CHttpHeaderRespAge::Build(_Inout_ CStringA &cStrDestA)
{
  if (cStrDestA.Format("%I64u", nAge) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

HRESULT CHttpHeaderRespAge::SetAge(_In_ ULONGLONG _nAge)
{
  nAge = _nAge;
  return S_OK;
}

ULONGLONG CHttpHeaderRespAge::GetAge() const
{
  return nAge;
}

} //namespace MX
