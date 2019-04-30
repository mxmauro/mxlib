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
#include "..\..\Include\Http\HttpHeaderReqHost.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqHost::CHttpHeaderReqHost() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderReqHost::~CHttpHeaderReqHost()
{
  return;
}

HRESULT CHttpHeaderReqHost::Parse(_In_z_ LPCSTR szValueA)
{
  LPCSTR szHostStartA, szHostEndA;
  int nPort;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //get host
  szValueA = SkipUntil(szHostStartA = szValueA, ": \t");
  if (szValueA == szHostStartA)
    return MX_E_InvalidData;
  szHostEndA = szValueA;
  //parse port if any
  nPort = -1;
  if (*szValueA == ':')
  {
    szValueA++;
    while (*szValueA == '0')
      szValueA++;
    nPort = 0;
    while (*szValueA >= '0' && *szValueA <= '9')
    {
      nPort = nPort * 10 + (int)(*szValueA++ - '0');
      if (nPort > 65535)
        return MX_E_InvalidData;
    }
    if (nPort == 0)
      return MX_E_InvalidData;
  }
  //skip spaces and check for end
  if (*CHttpHeaderBase::SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;
  //set new value
  hRes = cUrl.SetHost(szHostStartA, (SIZE_T)(szHostEndA - szHostStartA));
  if (SUCCEEDED(hRes) && nPort > 0)
    hRes = cUrl.SetPort(nPort);
  if (FAILED(hRes))
    return (hRes == E_INVALIDARG) ? MX_E_InvalidData : hRes;
  return S_OK;
}

HRESULT CHttpHeaderReqHost::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  if (cStrDestA.Copy(cUrl.GetHost()) == FALSE)
    return E_OUTOFMEMORY;
  if (cStrDestA.IsEmpty() == FALSE && cUrl.GetPort() > 0)
  {
    if (cStrDestA.AppendFormat(":%d", cUrl.GetPort()) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderReqHost::SetHost(_In_z_ LPCWSTR szHostW, _In_opt_ SIZE_T nHostLen)
{
  HRESULT hRes;

  if (nHostLen == (SIZE_T)-1)
    nHostLen = StrLenW(szHostW);
  if (nHostLen == 0)
    return MX_E_InvalidData;
  if (szHostW == NULL)
    return E_POINTER;
  //some checks
  hRes = cUrl.SetHost(szHostW);
  if (FAILED(hRes))
    return (hRes == E_INVALIDARG) ? MX_E_InvalidData : hRes;
  return S_OK;
}

LPCWSTR CHttpHeaderReqHost::GetHost() const
{
  return cUrl.GetHost();
}

HRESULT CHttpHeaderReqHost::SetPort(_In_ int nPort)
{
  return cUrl.SetPort(nPort);
}

int CHttpHeaderReqHost::GetPort() const
{
  return cUrl.GetPort();
}

} //namespace MX
