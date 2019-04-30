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
#include "..\..\Include\Http\HttpHeaderEntLastModified.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderEntLastModified::CHttpHeaderEntLastModified() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderEntLastModified::~CHttpHeaderEntLastModified()
{
  return;
}

HRESULT CHttpHeaderEntLastModified::Parse(_In_z_ LPCSTR szValueA)
{
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //parse date
  hRes = CHttpCommon::ParseDate(cDt, szValueA);
  if (FAILED(hRes))
    return (hRes == E_OUTOFMEMORY) ? hRes : MX_E_InvalidData;
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntLastModified::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  return cDt.Format(cStrDestA, "%a, %d %b %Y %H:%m:%S %z");
}

HRESULT CHttpHeaderEntLastModified::SetDate(_In_ CDateTime &_cDt)
{
  cDt = _cDt;
  return S_OK;
}

CDateTime CHttpHeaderEntLastModified::GetDate() const
{
  return cDt;
}

} //namespace MX
