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
#ifndef _MX_HTTP_WEBSOCKET_H
#define _MX_HTTP_WEBSOCKET_H

#include "..\Comm\IpcCommon.h"
#include "..\AutoPtr.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CWebSocket : public CIpc::CUserData
{
  MX_DISABLE_COPY_CONSTRUCTOR(CWebSocket);
protected:
  CWebSocket();
public:
  ~CWebSocket();

  HRESULT BeginTextMessage();
  HRESULT BeginBinaryMessage();

  HRESULT SendTextMessage(_In_ LPCSTR szMsgA, _In_ SIZE_T nMsgLen = (SIZE_T)-1);
  HRESULT SendTextMessage(_In_ LPCWSTR szMsgW, _In_ SIZE_T nMsgLen = (SIZE_T)-1);

  HRESULT SendBinaryMessage(_In_ LPVOID lpData, _In_ SIZE_T nDataLen);

  HRESULT EndMessage();

  HRESULT SendClose(_In_ USHORT wCode, _In_opt_z_ LPCSTR szReasonA = NULL);
  HRESULT SendPing();

  virtual HRESULT OnConnected();
  virtual HRESULT OnTextMessage(_In_ LPCSTR szMsgA, _In_ SIZE_T nMsgLength);
  virtual HRESULT OnBinaryMessage(_In_ LPVOID lpData, _In_ SIZE_T nDataSize);
  virtual VOID OnPongFrame();
  virtual VOID OnCloseFrame(_In_ USHORT wCode, _In_  HRESULT hrErrorCode);

  virtual SIZE_T GetMaxMessageSize() const;

private:
  VOID OnSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                       _In_ HRESULT hrErrorCode);
  HRESULT OnSocketDataReceived(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData);

  SIZE_T CalculateFrameSize(_In_ ULONG nPayloadSize);
  LPBYTE BuildFrame(_In_ LPVOID lpFrame, _In_ ULONG nPayloadSize, _In_ BOOL bFin, _In_ BYTE nRSV, _In_ BYTE nOpcode);
  VOID EncodeFrame(_In_ LPVOID lpFrame);

  HRESULT InternalSendFrame(_In_ BOOL bFinalFrame);
  HRESULT InternalSendControlFrame(_In_ BYTE nOpcode, _In_ LPVOID lpPayload, _In_ ULONG nPayloadSize);

  LPBYTE GetReceiveBufferFromCache();
  VOID PutReceiveBufferOnCache(_In_ LPBYTE lpBuffer);
  VOID PutAllReceiveBuffersOnCache();

private:
#pragma pack(1)
  typedef struct tagFRAME_HEADER {
    BYTE nOpcode : 4;
    BYTE nRsv : 3;
    BYTE nFin : 1;
    BYTE nPayloadLen : 7;
    BYTE nMask : 1;
  } FRAME_HEADER;

  typedef struct tagRECEIVED_DATA {
    ULONG nSize;
    BYTE aData[1];
  } RECEIVED_DATA, *LPRECEIVED_DATA;
#pragma pack()

private:
  friend class CHttpServer;

  CIpc *lpIpc;
  HANDLE hConn;
  BOOL bServerSide;
  //----
  struct {
    LONG nState;
    FRAME_HEADER sFrameHeader;
    ULONGLONG nPayloadLen;
    union {
      BYTE nKey[4];
      DWORD dwKey;
    } uMasking;
    struct {
      BYTE nOpcode;
      TArrayListWithFree<LPBYTE> aReceivedDataList;
      LPBYTE lpData;
      SIZE_T nFilledFrame;
      SIZE_T nTotalDataLength;
    } sCurrentMessage;
    struct {
      BYTE aBuffer[128];
      SIZE_T nFilledFrame;
    } sCurrentControlFrame;
    HRESULT hrCloseError;
  } sReceive;

  struct {
    FRAME_HEADER sFrameHeader;
    TAutoFreePtr<BYTE> cFrameBuffer;
    LPBYTE lpFrameData;
    ULONG nFilledFrame;
  } sSend;

  struct {
    LPBYTE lpBuffer[4];
    SIZE_T nNextBufferIndex;
  } sReceiveCache;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTP_WEBSOCKET_H
