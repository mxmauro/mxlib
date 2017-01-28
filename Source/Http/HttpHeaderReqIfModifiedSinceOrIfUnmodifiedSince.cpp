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
#include "..\..\Include\Http\HttpHeaderReqIfModifiedSinceOrIfUnmodifiedSince.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqIfXXXSinceBase::CHttpHeaderReqIfXXXSinceBase(__in BOOL _bIfModified) : CHttpHeaderBase()
{
  bIfModified = _bIfModified;
  return;
}

CHttpHeaderReqIfXXXSinceBase::~CHttpHeaderReqIfXXXSinceBase()
{
  return;
}

HRESULT CHttpHeaderReqIfXXXSinceBase::Parse(__in_z LPCSTR szValueA)
{
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //parse date
  hRes = CHttpCommon::ParseDate(cDt, szValueA);
  //done
  return hRes;
}

HRESULT CHttpHeaderReqIfXXXSinceBase::Build(__inout CStringA &cStrDestA)
{
  return cDt.Format(cStrDestA, "%a, %d %b %Y %H:%m:%S %z");
}

HRESULT CHttpHeaderReqIfXXXSinceBase::SetDate(__in CDateTime &_cDt)
{
  cDt = _cDt;
  return S_OK;
}

CDateTime CHttpHeaderReqIfXXXSinceBase::GetDate() const
{
  return cDt;
}

} //namespace MX
