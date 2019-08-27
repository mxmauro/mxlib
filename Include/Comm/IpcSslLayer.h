/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the LICENSE file distributed with
 * this work for additional information regarding copyright ownership.
 *
 * Also, if exists, check the Licenses directory for information about
 * third-party modules.
 *
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
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
  VOID OnShutdown();

  HRESULT OnReceivedData(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize);
  HRESULT OnSendPacket(_In_ CIpc::CPacketBase *lpPacket);

  HRESULT HandleSsl(_In_ BOOL bCanWrite);
  HRESULT ExecSslRead();
  HRESULT SendEncryptedOutput();
  HRESULT ProcessOutgoingData(_Inout_ PSIZE_T lpnProcessedPackets, _Inout_ CIpc::CPacketBase **lplpAfterWritePacket);
  HRESULT FinalizeHandshake();

private:
  LONG volatile nMutex;
  LONG volatile hNetworkError;
  LPVOID lpInternalData;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_IPCSSL_H
