/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2019. All rights reserved.
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
#ifndef _MX_HTTPHEADERREQUESTSECWEBSOCKETKEY_H
#define _MX_HTTPHEADERREQUESTSECWEBSOCKETKEY_H

#include "HttpHeaderBase.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderReqSecWebSocketKey : public CHttpHeaderBase
{
public:
  CHttpHeaderReqSecWebSocketKey();
  ~CHttpHeaderReqSecWebSocketKey();

  MX_DECLARE_HTTPHEADER_NAME(Sec-WebSocket-Key)

  HRESULT Parse(_In_z_ LPCSTR szValueA);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser);

  HRESULT SetKey(_In_ LPVOID lpKey, _In_ SIZE_T nKeyLen);

  LPBYTE GetKey() const;
  SIZE_T GetKeyLength() const;

private:
  HRESULT InternalSetKey(_In_ LPVOID lpKey, _In_ SIZE_T nKeyLen);

private:
  TAutoFreePtr<BYTE> cKey;
  SIZE_T nKeyLength;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERREQUESTSECWEBSOCKETKEY_H
