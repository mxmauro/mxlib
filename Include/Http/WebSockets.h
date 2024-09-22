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
namespace MX {
class CHttpServer;
class CHttpClient;
} //namespace MX

//-----------------------------------------------------------

namespace MX {

class CWebSocket : public CIpc::CUserData
{
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

  BOOL IsClosed() const;

  VOID Close(_In_opt_ HRESULT hrErrorCode = S_OK);

  virtual HRESULT OnConnected();
  virtual HRESULT OnTextMessage(_In_ LPCSTR szMsgA, _In_ SIZE_T nMsgLength);
  virtual HRESULT OnBinaryMessage(_In_ LPVOID lpData, _In_ SIZE_T nDataSize);
  virtual VOID OnPongFrame();
  virtual VOID OnCloseFrame(_In_ USHORT wCode, _In_  HRESULT hrErrorCode);

  virtual SIZE_T GetMaxMessageSize() const;

private:
  friend class CHttpServer;
  friend class CHttpClient;

#pragma pack(1)
  typedef struct tagFRAME_HEADER {
    BYTE nOpcode : 4;
    BYTE nRsv : 3;
    BYTE nFin : 1;
    BYTE nPayloadLen : 7;
    BYTE nMask : 1;
    BYTE aExtended[8 + 4];
  } FRAME_HEADER, *LPFRAME_HEADER;

  typedef struct tagRECEIVED_DATA {
    ULONG nSize;
    BYTE aData[1];
  } RECEIVED_DATA, *LPRECEIVED_DATA;
#pragma pack()

private:
  VOID OnSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                       _In_ HRESULT hrErrorCode);
  HRESULT OnSocketDataReceived(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData);

  SIZE_T BuildFrame(_Out_ LPFRAME_HEADER lpFrame, _In_ LPBYTE lpPayload, _In_ ULONG nPayloadSize, _In_ BYTE nOpcode,
                    _In_ BOOL bFinal);

  HRESULT InternalSendFrame(_In_ BOOL bFinalFrame);
  HRESULT InternalSendControlFrame(_In_ BYTE nOpcode, _In_ LPVOID lpPayload, _In_ ULONG nPayloadSize);

  LPBYTE GetReceiveBufferFromCache();
  VOID PutReceiveBufferOnCache(_In_ LPBYTE lpBuffer);
  VOID PutAllReceiveBuffersOnCache();

  HRESULT SetupIpc(_In_ CIpc *lpIpc, _In_ HANDLE hComm, _In_ BOOL bServerSide);
  VOID FireConnectedAndInitialRead();

private:
  CIpc *lpIpc{ NULL };
  HANDLE hConn{ NULL };
  BOOL bServerSide{ FALSE };
  //----
  struct {
    LONG nState{ 0 };
    FRAME_HEADER sFrameHeader{};
    ULONGLONG nPayloadLen{ 0 };
    union {
      BYTE nKey[4];
      DWORD dwKey;
    } uMasking{};
    struct {
      BYTE nOpcode{ 0 };
      TArrayListWithFree<LPBYTE> aReceivedDataList;
      LPBYTE lpData{ NULL };
      SIZE_T nFilledFrame{ 0 };
      SIZE_T nTotalDataLength{ 0 };
    } sCurrentMessage;
    struct {
      BYTE aBuffer[128]{};
      SIZE_T nFilledFrame{ 0 };
    } sCurrentControlFrame;
  } sReceive;

  struct {
    LONG volatile nSendInProgressMutex{ MX_FASTLOCK_INIT };
    FRAME_HEADER sFrameHeader{};
    TAutoFreePtr<BYTE> cFrameBuffer;
    LPBYTE lpFrameData{ NULL };
    ULONG nFilledFrame{ 0 };
  } sSend;

  struct {
    LPBYTE lpBuffer[4]{};
    SIZE_T nNextBufferIndex{ 0 };
  } sReceiveCache;

  LONG volatile hrCloseError{ S_FALSE };
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTP_WEBSOCKET_H
