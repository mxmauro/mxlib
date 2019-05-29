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
#ifndef _MX_IPCSSL_H
#define _MX_IPCSSL_H

#include "IpcCommon.h"
#include "..\DateTime\DateTime.h"
#include "..\ArrayList.h"
#include "SslCertificates.h"

//-----------------------------------------------------------

namespace MX {

class CIpcSslLayer : public CIpc::CLayer, public CLoggable
{
  MX_DISABLE_COPY_CONSTRUCTOR(CIpcSslLayer);
public:
  typedef enum {
    ProtocolUnknown=0,
    ProtocolTLSv1_0,
    ProtocolTLSv1_1,
    ProtocolTLSv1_2,
  } eProtocol;

public:
  CIpcSslLayer();
  ~CIpcSslLayer();

  HRESULT Initialize(_In_ BOOL bServerSide, _In_ eProtocol nProtocol, _In_opt_ LPCSTR szHostNameA = NULL,
                     _In_opt_ CSslCertificateArray *lpCheckCertificates = NULL,
                     _In_opt_ CSslCertificate *lpSelfCert = NULL, _In_opt_ CCryptoRSA *lpPrivKey = NULL);

private:
  HRESULT OnConnect();
  HRESULT OnDisconnect();

  HRESULT OnData(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize);
  HRESULT OnSendMsg(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize);

  HRESULT HandleSsl(_In_ BOOL bCanWrite);
  HRESULT ExecSslRead();
  HRESULT SendEncryptedOutput();
  HRESULT ProcessDataToSend();
  HRESULT FinalizeHandshake();

private:
  LONG volatile nMutex;
  LONG volatile hNetworkError;
  LPVOID lpInternalData;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_IPCSSL_H
