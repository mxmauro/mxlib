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
#include "..\..\Include\Http\HttpHeaderReqExpect.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqExpect::CHttpHeaderReqExpect() : CHttpHeaderBase()
{
  nExpectation = ExpectationUnsupported;
  return;
}

CHttpHeaderReqExpect::~CHttpHeaderReqExpect()
{
  return;
}

HRESULT CHttpHeaderReqExpect::Parse(__in_z LPCSTR szValueA)
{
  eExpectation _nExpectation = ExpectationUnsupported;

  if (szValueA == NULL)
    return E_POINTER;
  nExpectation = ExpectationUnsupported;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //check expectation
  if (StrNCompareA(szValueA, "100-continue", 12, TRUE) == 0)
  {
    _nExpectation = Expectation100Continue;
    szValueA += 12;
  }
  else
  {
    return MX_E_Unsupported;
  }
  //skip spaces and check for end
  if (*SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;
  //done
  nExpectation = _nExpectation;
  return S_OK;
}

HRESULT CHttpHeaderReqExpect::Build(__inout CStringA &cStrDestA)
{
  switch (nExpectation)
  {
    case Expectation100Continue:
      if (cStrDestA.Copy("100-continue") == FALSE)
        return E_OUTOFMEMORY;
      return S_OK;
  }
  cStrDestA.Empty();
  return MX_E_Unsupported;
}

HRESULT CHttpHeaderReqExpect::SetExpectation(__in eExpectation _nExpectation)
{
  if (_nExpectation != Expectation100Continue && _nExpectation != ExpectationUnsupported)
    return E_INVALIDARG;
  nExpectation = _nExpectation;
  return S_OK;
}

CHttpHeaderReqExpect::eExpectation CHttpHeaderReqExpect::GetExpectation() const
{
  return nExpectation;
}

} //namespace MX
