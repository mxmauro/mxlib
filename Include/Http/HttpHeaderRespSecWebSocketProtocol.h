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
#ifndef _MX_HTTPHEADERRESPONSESECWEBSOCKETPROTOCOL_H
#define _MX_HTTPHEADERRESPONSESECWEBSOCKETPROTOCOL_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderRespSecWebSocketProtocol : public CHttpHeaderBase
{
public:
  CHttpHeaderRespSecWebSocketProtocol();
  ~CHttpHeaderRespSecWebSocketProtocol();

  MX_DECLARE_HTTPHEADER_NAME(Sec-WebSocket-Protocol)

  HRESULT Parse(_In_z_ LPCSTR szValueA);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser);

  HRESULT SetProtocol(_In_z_ LPCSTR szProtocolA, _In_opt_ SIZE_T nProtocolLen = (SIZE_T)-1);
  LPCSTR GetProtocol() const;

private:
  CStringA cStrProtocolA;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERRESPONSESECWEBSOCKETPROTOCOL_H