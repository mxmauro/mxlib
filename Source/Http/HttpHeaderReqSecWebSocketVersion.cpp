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
#include "..\..\Include\Http\HttpHeaderReqSecWebSocketVersion.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqSecWebSocketVersion::CHttpHeaderReqSecWebSocketVersion() : CHttpHeaderBase()
{
  nVersion = -1;
  return;
}

CHttpHeaderReqSecWebSocketVersion::~CHttpHeaderReqSecWebSocketVersion()
{
  return;
}

HRESULT CHttpHeaderReqSecWebSocketVersion::Parse(_In_z_ LPCSTR szValueA)
{
  int _nVersion;

  if (szValueA == NULL)
    return E_POINTER;

  //skip spaces
  szValueA = SkipSpaces(szValueA);
  if (*szValueA == 0)
    return MX_E_InvalidData;

  //get value
  _nVersion = 0;
  while (*szValueA >= '0' && *szValueA <= '9')
  {
    _nVersion = _nVersion * 10 + (int)((*szValueA) - '0');
    if (_nVersion > 255)
      return MX_E_InvalidData;
    szValueA++;
  }

  //check for end
  if (*SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;

  //done
  nVersion = _nVersion;
  return S_OK;
}

HRESULT CHttpHeaderReqSecWebSocketVersion::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  if (nVersion < 0)
  {
    cStrDestA.Empty();
    return MX_E_NotReady;
  }
  //done
  return (cStrDestA.Format("%ld", nVersion) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

HRESULT CHttpHeaderReqSecWebSocketVersion::SetVersion(_In_ int _nVersion)
{
  if (_nVersion < 0 || _nVersion > 255)
    return E_INVALIDARG;
  nVersion = _nVersion;
  //done
  return S_OK;
}

int CHttpHeaderReqSecWebSocketVersion::GetVersion() const
{
  return nVersion;
}

} //namespace MX
