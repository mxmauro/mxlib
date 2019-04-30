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
#include "..\..\Include\Http\HttpHeaderReqSecWebSocketKey.h"
#include "..\..\Include\Crypto\Base64.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqSecWebSocketKey::CHttpHeaderReqSecWebSocketKey() : CHttpHeaderBase()
{
  nKeyLength = 0;
  return;
}

CHttpHeaderReqSecWebSocketKey::~CHttpHeaderReqSecWebSocketKey()
{
  return;
}

HRESULT CHttpHeaderReqSecWebSocketKey::Parse(_In_z_ LPCSTR szValueA)
{
  CBase64Decoder cDecoder;
  LPCSTR szStartA;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;

  //skip spaces
  szStartA = SkipSpaces(szValueA);

  //reach the end
  for (szValueA = szStartA; *szValueA > L' '; szValueA++);
  if (szValueA == szStartA)
    return MX_E_InvalidData;

  //start decoding
  hRes = cDecoder.Begin(cDecoder.GetRequiredSpace((SIZE_T)(szValueA - szStartA)));
  if (SUCCEEDED(hRes))
    hRes = cDecoder.Process(szStartA, (SIZE_T)(szValueA - szStartA));
  if (SUCCEEDED(hRes))
    hRes = cDecoder.End();
  //and set key
  if (SUCCEEDED(hRes))
    hRes = InternalSetKey(cDecoder.GetBuffer(), cDecoder.GetOutputLength());
  if (FAILED(hRes))
    return hRes;

  //check after data
  if (*SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpHeaderReqSecWebSocketKey::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  CBase64Encoder cEncoder;
  HRESULT hRes;

  cStrDestA.Empty();

  if (nKeyLength == 0)
    return MX_E_NotReady;

  //start encoding
  hRes = cEncoder.Begin(cEncoder.GetRequiredSpace(nKeyLength));
  if (SUCCEEDED(hRes))
    hRes = cEncoder.Process(cKey.Get(), nKeyLength);
  if (SUCCEEDED(hRes))
    hRes = cEncoder.End();
  if (FAILED(hRes))
    return hRes;

  //build output
  return (cStrDestA.CopyN(cEncoder.GetBuffer(), cEncoder.GetOutputLength()) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

HRESULT CHttpHeaderReqSecWebSocketKey::SetKey(_In_ LPVOID lpKey, _In_ SIZE_T nKeyLen)
{
  if (lpKey == NULL && nKeyLen > 0)
    return E_POINTER;
  return InternalSetKey(lpKey, nKeyLen);
}

HRESULT CHttpHeaderReqSecWebSocketKey::InternalSetKey(_In_ LPVOID lpKey, _In_ SIZE_T nKeyLen)
{
  if (nKeyLen > 0)
  {
    LPBYTE lpNewKey;

    lpNewKey = (LPBYTE)MX_MALLOC(nKeyLen);
    if (lpNewKey == NULL)
      return E_POINTER;
    cKey.Attach(lpNewKey);
    MemCopy(lpNewKey, lpKey, nKeyLen);
  }
  else
  {
    cKey.Reset();
  }
  nKeyLength = nKeyLen;
  //done
  return S_OK;
}

LPBYTE CHttpHeaderReqSecWebSocketKey::GetKey() const
{
  return (const_cast<TAutoFreePtr<BYTE>&>(cKey)).Get();
}

SIZE_T CHttpHeaderReqSecWebSocketKey::GetKeyLength() const
{
  return nKeyLength;
}

} //namespace MX
