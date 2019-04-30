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
#include "..\..\Include\Http\HttpHeaderEntContentLength.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderEntContentLength::CHttpHeaderEntContentLength() : CHttpHeaderBase()
{
  nLength = 0;
  return;
}

CHttpHeaderEntContentLength::~CHttpHeaderEntContentLength()
{
  return;
}

HRESULT CHttpHeaderEntContentLength::Parse(_In_z_ LPCSTR szValueA)
{
  ULONGLONG nTemp;

  if (szValueA == NULL)
    return E_POINTER;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //parse value
  if (*szValueA < '0' || *szValueA > '9')
    return MX_E_InvalidData;
  while (*szValueA == '0')
    szValueA++;
  nLength = 0;
  while (*szValueA >= '0' && *szValueA <= '9')
  {
    nTemp = nLength * 10ui64;
    if (nTemp < nLength)
      return MX_E_ArithmeticOverflow;
    nLength = nTemp + (ULONGLONG)(*szValueA - '0');
    if (nLength < nTemp)
      return MX_E_ArithmeticOverflow;
    szValueA++;
  }
  //skip spaces and check for end
  if (*SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentLength::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  if (cStrDestA.Format("%I64u", nLength) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentLength::SetLength(_In_ ULONGLONG _nLength)
{
  nLength = _nLength;
  return S_OK;
}

ULONGLONG CHttpHeaderEntContentLength::GetLength() const
{
  return nLength;
}

} //namespace MX
