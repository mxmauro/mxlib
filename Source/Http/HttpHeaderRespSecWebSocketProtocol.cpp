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
#include "..\..\Include\Http\HttpHeaderRespSecWebSocketProtocol.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderRespSecWebSocketProtocol::CHttpHeaderRespSecWebSocketProtocol() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderRespSecWebSocketProtocol::~CHttpHeaderRespSecWebSocketProtocol()
{
  return;
}

HRESULT CHttpHeaderRespSecWebSocketProtocol::Parse(_In_z_ LPCSTR szValueA)
{
  LPCSTR szStartA;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;

  //skip spaces
  szValueA = SkipSpaces(szValueA);
  if (*szValueA == 0)
    return MX_E_InvalidData;

  //protocol
  szValueA = SkipUntil(szStartA = szValueA, ";, \t");
  if (szValueA == szStartA)
    return MX_E_InvalidData;

  hRes = SetProtocol(szStartA, (SIZE_T)(szValueA - szStartA));
  if (FAILED(hRes))
    return hRes;

  //check for end
  if (*SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpHeaderRespSecWebSocketProtocol::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  if (cStrProtocolA.IsEmpty() != FALSE)
  {
    cStrDestA.Empty();
    return MX_E_NotReady;
  }
  //done
  return (cStrDestA.CopyN((LPCSTR)cStrProtocolA, cStrProtocolA.GetLength()) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

HRESULT CHttpHeaderRespSecWebSocketProtocol::SetProtocol(_In_z_ LPCSTR szProtocolA, _In_opt_ SIZE_T nProtocolLen)
{
  LPCSTR szStartA;

  if (nProtocolLen == (SIZE_T)-1)
    nProtocolLen = StrLenA(szProtocolA);
  if (nProtocolLen == 0)
    return MX_E_InvalidData;
  if (szProtocolA == NULL)
    return E_POINTER;

  szProtocolA = GetToken(szStartA = szProtocolA, nProtocolLen);
  if (szProtocolA != szStartA + nProtocolLen)
    return MX_E_InvalidData;

  //set protocol
  if (cStrProtocolA.CopyN(szStartA, nProtocolLen) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCSTR CHttpHeaderRespSecWebSocketProtocol::GetProtocol() const
{
  return (LPCSTR)cStrProtocolA;
}

} //namespace MX
