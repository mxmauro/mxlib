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
#include "..\..\Include\Http\HttpHeaderRespWwwProxyAuthenticate.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderRespWwwProxyAuthenticateCommon::CHttpHeaderRespWwwProxyAuthenticateCommon() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderRespWwwProxyAuthenticateCommon::~CHttpHeaderRespWwwProxyAuthenticateCommon()
{
  return;
}

HRESULT CHttpHeaderRespWwwProxyAuthenticateCommon::Parse(_In_z_ LPCSTR szValueA)
{
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //check type
  if (StrNCompareA(szValueA, "basic", 5, TRUE) == 0)
  {
    szValueA += 5;
    cHttpAuth.Attach(MX_DEBUG_NEW CHttpAuthBasic());
    if (!cHttpAuth)
      return E_OUTOFMEMORY;
  }
  else if (StrNCompareA(szValueA, "digest", 6, TRUE) == 0)
  {
    szValueA += 6;
    cHttpAuth.Attach(MX_DEBUG_NEW CHttpAuthDigest());
    if (!cHttpAuth)
      return E_OUTOFMEMORY;
  }
  else
  {
    return MX_E_Unsupported;
  }
  if (*szValueA != ' ' && *szValueA != '\t')
  {
    return ((*szValueA >= 'A' && *szValueA <= 'Z') ||
            (*szValueA >= 'a' && *szValueA <= 'z') ||
            (*szValueA >= '0' && *szValueA <= '9')) ? MX_E_Unsupported : MX_E_InvalidData;
  }
  //parse header
  hRes = cHttpAuth->Parse(szValueA);
  //done
  if (FAILED(hRes))
    cHttpAuth.Release();
  return hRes;
}

HRESULT CHttpHeaderRespWwwProxyAuthenticateCommon::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  cStrDestA.Empty();
  return MX_E_Unsupported;
}

CHttpBaseAuth* CHttpHeaderRespWwwProxyAuthenticateCommon::GetHttpAuth()
{
  return cHttpAuth.Get();
}

//--------

CHttpHeaderRespWwwAuthenticate::CHttpHeaderRespWwwAuthenticate() : CHttpHeaderRespWwwProxyAuthenticateCommon()
{
  return;
}

CHttpHeaderRespWwwAuthenticate::~CHttpHeaderRespWwwAuthenticate()
{
  return;
}

//--------

CHttpHeaderRespProxyAuthenticate::CHttpHeaderRespProxyAuthenticate() : CHttpHeaderRespWwwProxyAuthenticateCommon()
{
  return;
}

CHttpHeaderRespProxyAuthenticate::~CHttpHeaderRespProxyAuthenticate()
{
  return;
}

} //namespace MX
