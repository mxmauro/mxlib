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
#include "..\..\Include\Http\WebSockets.h"
#include "..\..\Include\FnvHash.h"
#include "..\..\Include\Strings\Utf8.h"

#define _OPCODE_Continuation        0
#define _OPCODE_Text                1
#define _OPCODE_Binary              2
#define _OPCODE_ConnectionClose     8
#define _OPCODE_Ping                9
#define _OPCODE_Pong               10
#define _OPCODE_NONE               15
#define _OPCODE_ControlFrameMask 0x08

#define MAX_FRAME_PAYLOAD_SIZE                        131072
#define SEND_PAYLOAD_SIZE                              16384
#define RECEIVE_DATA_BLOCK_SIZE                        16384

#define XOR_PING                                  0xA64F239A

//-----------------------------------------------------------

namespace MX {

CWebSocket::CWebSocket() : CIpc::CUserData()
{
  lpIpc = NULL;
  hConn = NULL;
  bServerSide = FALSE;
  //----
  sReceive.nState = 0;
  sReceive.uMasking.dwKey = 0;
  sReceive.sCurrentMessage.nOpcode = _OPCODE_NONE;
  sReceive.sCurrentMessage.lpData = NULL;
  sReceive.sCurrentMessage.nFilledFrame = sReceive.sCurrentMessage.nTotalDataLength = 0;
  ::MxMemSet(&(sReceive.sCurrentControlFrame), 0, sizeof(sReceive.sCurrentControlFrame));
  _InterlockedExchange(&hrCloseError, S_FALSE);
  //----
  FastLock_Initialize(&(sSend.nSendInProgressMutex));
  ::MxMemSet(&(sSend.sFrameHeader), 0, sizeof(sSend.sFrameHeader));
  sSend.sFrameHeader.nOpcode = _OPCODE_NONE;
  sSend.lpFrameData = NULL;
  sSend.nFilledFrame = 0;
  //----
  ::MxMemSet(&sReceiveCache, 0, sizeof(sReceiveCache));
  return;
}

CWebSocket::~CWebSocket()
{
  SIZE_T i;

  for (i = 0; i < MX_ARRAYLEN(sReceiveCache.lpBuffer); i++)
  {
    MX_FREE(sReceiveCache.lpBuffer[i]);
  }
  return;
}

HRESULT CWebSocket::BeginTextMessage()
{
  FastLock_Enter(&(sSend.nSendInProgressMutex));

  sSend.sFrameHeader.nOpcode = _OPCODE_Text;

  //create frame buffer if not done yet
  if (!(sSend.cFrameBuffer))
  {
    sSend.cFrameBuffer.Attach((LPBYTE)MX_MALLOC(SEND_PAYLOAD_SIZE));
    if (!(sSend.cFrameBuffer))
    {
      FastLock_Exit(&(sSend.nSendInProgressMutex));
      return E_OUTOFMEMORY;
    }
  }

  //done
  return S_OK;
}

HRESULT CWebSocket::BeginBinaryMessage()
{
  FastLock_Enter(&(sSend.nSendInProgressMutex));

  sSend.sFrameHeader.nOpcode = _OPCODE_Binary;

  //create frame buffer if not done yet
  if (!(sSend.cFrameBuffer))
  {
    sSend.cFrameBuffer.Attach((LPBYTE)MX_MALLOC(SEND_PAYLOAD_SIZE));
    if (!(sSend.cFrameBuffer))
    {
      FastLock_Exit(&(sSend.nSendInProgressMutex));
      return E_OUTOFMEMORY;
    }
  }

  //done
  return S_OK;
}

HRESULT CWebSocket::SendTextMessage(_In_ LPCSTR szMsgA, _In_ SIZE_T nMsgLen)
{
  if (nMsgLen == (SIZE_T)-1)
    nMsgLen = StrLenA(szMsgA);
  return SendBinaryMessage((LPSTR)szMsgA, nMsgLen);
}

HRESULT CWebSocket::SendTextMessage(_In_ LPCWSTR szMsgW, _In_ SIZE_T nMsgLen)
{
  CStringA cStrTempA;
  HRESULT hRes;

  hRes = Utf8_Encode(cStrTempA, szMsgW, nMsgLen);
  if (SUCCEEDED(hRes))
    hRes =  SendTextMessage((LPCSTR)cStrTempA, cStrTempA.GetLength());
  return hRes;
}

HRESULT CWebSocket::SendBinaryMessage(_In_ LPVOID lpData, _In_ SIZE_T nDataLen)
{
  if (FastLock_IsActive(&(sSend.nSendInProgressMutex)) != ::GetCurrentThreadId())
    return MX_E_NotReady;

  if (nDataLen == 0)
    return S_OK;
  if (lpData == NULL)
  {
    //on error, unlock
    FastLock_Exit(&(sSend.nSendInProgressMutex));
    return E_POINTER;
  }

  //begin framing
  while (nDataLen > 0)
  {
    ULONG nToWrite;
    HRESULT hRes;

    if (sSend.nFilledFrame >= SEND_PAYLOAD_SIZE)
    {
      hRes = InternalSendFrame(FALSE);
      if (FAILED(hRes))
        return hRes;

      //prepare new frame
      sSend.sFrameHeader.nOpcode = _OPCODE_Continuation;
      sSend.lpFrameData = NULL;
      sSend.nFilledFrame = 0;
    }

    //do we need a new frame?
    if (sSend.lpFrameData == NULL)
    {
      //prepare pointer
      sSend.lpFrameData = sSend.cFrameBuffer.Get();
    }

    //calculate amount to copy
    nToWrite = SEND_PAYLOAD_SIZE - sSend.nFilledFrame;
    if ((SIZE_T)nToWrite > nDataLen)
      nToWrite = (ULONG)nDataLen;

    //copy data
    ::MxMemCopy(sSend.lpFrameData, lpData, nToWrite);

    //advance pointer
    lpData = (LPBYTE)lpData + nToWrite;
    nDataLen -= nToWrite;
    sSend.nFilledFrame += nToWrite;
  }

  //done
  return S_OK;
}

HRESULT CWebSocket::EndMessage()
{
  if (FastLock_IsActive(&(sSend.nSendInProgressMutex)) != ::GetCurrentThreadId())
    return MX_E_NotReady;

  if (sSend.nFilledFrame > 0)
  {
    HRESULT hRes;

    hRes = InternalSendFrame(TRUE);
    if (FAILED(hRes))
    {
      //on error, unlock
      FastLock_Exit(&(sSend.nSendInProgressMutex));
      return hRes;
    }
  }

  //finalize send
  ::MxMemSet(&(sSend.sFrameHeader), 0, sizeof(sSend.sFrameHeader));
  sSend.sFrameHeader.nOpcode = _OPCODE_NONE;
  sSend.lpFrameData = NULL;
  sSend.nFilledFrame = 0;

  //unlock
  FastLock_Exit(&(sSend.nSendInProgressMutex));

  //done
  return S_OK;
}

HRESULT CWebSocket::SendClose(_In_ USHORT wCode, _In_opt_z_ LPCSTR szReasonA)
{
  BYTE aPayload[128];
  SIZE_T nReasonLen;

  aPayload[0] = (BYTE)(wCode >> 8);
  aPayload[1] = (BYTE)(wCode & 0xFF);
  nReasonLen = StrLenA(szReasonA);
  if (nReasonLen > 123)
    nReasonLen = 123;
  ::MxMemCopy(aPayload + 2, szReasonA, nReasonLen);
  //NOTE: InternalSendControlFrame modifies the payload if mask is active
  return InternalSendControlFrame(_OPCODE_ConnectionClose, aPayload, 2 + (ULONG)nReasonLen);
}

HRESULT CWebSocket::SendPing()
{
  DWORD dw = (DWORD)(SIZE_T)this;

  dw ^= XOR_PING;
  //NOTE: InternalSendControlFrame modifies the payload if mask is active
  return InternalSendControlFrame(_OPCODE_Ping, &dw, (ULONG)sizeof(dw));
}

BOOL CWebSocket::IsClosed() const
{
  return (__InterlockedRead(&(const_cast<CWebSocket*>(this)->hrCloseError)) != S_FALSE) ? TRUE : FALSE;
}

VOID CWebSocket::Close(_In_opt_ HRESULT hrErrorCode)
{
  if (hConn != NULL)
    lpIpc->Close(hConn, hrErrorCode);
  return;
}

HRESULT CWebSocket::OnConnected()
{
  return S_OK;
}

HRESULT CWebSocket::OnTextMessage(_In_ LPCSTR szMsgA, _In_ SIZE_T nMsgLength)
{
  return MX_E_Unsupported;
}

HRESULT CWebSocket::OnBinaryMessage(_In_ LPVOID lpData, _In_ SIZE_T nDataSize)
{
  return MX_E_Unsupported;
}

VOID CWebSocket::OnPongFrame()
{
  return;
}

VOID CWebSocket::OnCloseFrame(_In_ USHORT wCode, _In_  HRESULT hrErrorCode)
{
  return;
}

SIZE_T CWebSocket::GetMaxMessageSize() const
{
  return 8192;
}

VOID CWebSocket::OnSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                                 _In_ HRESULT hrErrorCode)
{
  if (_InterlockedCompareExchange(&hrCloseError, hrErrorCode, S_FALSE) == S_FALSE)
  {
    HRESULT hRes = __InterlockedRead(&hrErrorCode);

    OnCloseFrame((SUCCEEDED(hRes) ? 1000 : 1006), hRes);
  }
  hConn = NULL;
  return;
}

HRESULT CWebSocket::OnSocketDataReceived(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData)
{
  BYTE aMsgBuf[4096], *lpMsg;
  SIZE_T nMsgSize;
  HRESULT hRes;

  //closed?
  /*
  if (hConn == NULL || __InterlockedRead(&hrCloseError) != S_FALSE)
  {
    return sReceive.hrCloseError;
  }
  */

  //start of loop
loop:
  //get buffered message
  nMsgSize = sizeof(aMsgBuf);
  hRes = lpIpc->GetBufferedMessage(hConn, aMsgBuf, &nMsgSize);
  if (FAILED(hRes))
    return hRes;
  if (nMsgSize == 0)
    return S_OK; //nothing else to do

  //start parsing
  lpMsg = aMsgBuf;

  //on header?
  while (nMsgSize > 0 && sReceive.nState < 16)
  {
    //states 0 & 1
    if (sReceive.nState < 2)
    {
      //reading frame header
      ((LPBYTE)&(sReceive.sFrameHeader))[sReceive.nState] = *lpMsg++;
      nMsgSize--;

      switch (++(sReceive.nState))
      {
        case 1:
          //RSV fields MUST be zero
          if (sReceive.sFrameHeader.nRsv != 0)
            return MX_E_Unsupported;

          /*
          NOTE: This is not always respected

          //check for masking (client->server must mask / server->client must not)
          if (sReceive.sFrameHeader.nPayloadLen != 0)
          {
            if (sReceive.sFrameHeader.nMask != ((bServerSide != FALSE) ? 1 : 0))
              return MX_E_InvalidData;
          }
          */

          //validate opcode
          switch (sReceive.sFrameHeader.nOpcode)
          {
            case _OPCODE_Continuation:
              if (sReceive.sCurrentMessage.nOpcode == _OPCODE_NONE)
                return MX_E_InvalidData;
              break;

            case _OPCODE_Text:
            case _OPCODE_Binary:
              if (sReceive.sCurrentMessage.nOpcode == _OPCODE_NONE)
                sReceive.sCurrentMessage.nOpcode = sReceive.sFrameHeader.nOpcode;
              else if (sReceive.sCurrentMessage.nOpcode != sReceive.sFrameHeader.nOpcode)
                return MX_E_InvalidData;
              break;

            case _OPCODE_ConnectionClose:
            case _OPCODE_Ping:
            case _OPCODE_Pong:
              break;

            default:
              return MX_E_Unsupported;
          }
          break;

        case 2:
          if (sReceive.sFrameHeader.nPayloadLen < 126)
          {
            sReceive.nPayloadLen = (ULONGLONG)(sReceive.sFrameHeader.nPayloadLen);

jump_to_maskkey_or_payloaddata:
            if (sReceive.sFrameHeader.nMask != 0)
            {
              sReceive.nState = 12; //jump to mask key
            }
            else
            {
              sReceive.nState = 16; //jump to payload data
              goto validate_message_length;
            }
          }
          else
          {
            //control frames cannot be large
            if ((sReceive.sFrameHeader.nOpcode & 0x08) != 0)
              return MX_E_InvalidData;

            //prepare for large payload
            sReceive.nPayloadLen = 0;

            //jump to 16-bit or 64-bit payload length
            sReceive.nState = (sReceive.sFrameHeader.nPayloadLen == 126) ? 2 : 4;
          }
          break;
      }
    }
    //states 2 & 3
    else if (sReceive.nState < 4)
    {
      //reading 16-bit payload length
      ((LPBYTE)&(sReceive.nPayloadLen))[3 - sReceive.nState] = *lpMsg++;
      nMsgSize--;

      if ((++(sReceive.nState)) == 4)
        goto jump_to_maskkey_or_payloaddata;
    }
    //states 4 to 11
    else if (sReceive.nState < 12)
    {
      //reading 64-bit payload length
      ((LPBYTE)&(sReceive.nPayloadLen))[11 - sReceive.nState] = *lpMsg++;
      nMsgSize--;

      if ((++(sReceive.nState)) == 12 && sReceive.sFrameHeader.nMask == 0)
      {
        sReceive.nState = 16; //jump to payload data

validate_message_length:
        if ((ULONGLONG)(sReceive.sCurrentMessage.nTotalDataLength) + sReceive.nPayloadLen < sReceive.nPayloadLen)
          return MX_E_InvalidData;
        if ((ULONGLONG)(sReceive.sCurrentMessage.nTotalDataLength) +
            sReceive.nPayloadLen > (ULONGLONG)GetMaxMessageSize())
        {
          return MX_E_Unsupported;
        }
      }
    }
    //states 12 to 15
    else
    {
      //reading 32-bit masking key
      sReceive.uMasking.nKey[sReceive.nState - 12] = *lpMsg++;
      nMsgSize--;

      if ((++(sReceive.nState)) == 16)
        goto validate_message_length;
    }
  }
  if (sReceive.nState < 16)
  {
consume_used_and_loop:
    if (lpMsg > aMsgBuf)
    {
      hRes = lpIpc->ConsumeBufferedMessage(hConn, (SIZE_T)(lpMsg - aMsgBuf));
      if (FAILED(hRes))
        return hRes;
    }
    goto loop;
  }

  //if we reach here, we are processing the payload
  if (nMsgSize > 0)
  {
    SIZE_T nToRead;

    //is it a control frame?
    if ((sReceive.sFrameHeader.nOpcode & _OPCODE_ControlFrameMask) == 0)
    {
      while (nMsgSize > 0 && sReceive.nPayloadLen > 0)
      {
        if (sReceive.sCurrentMessage.lpData == NULL)
        {
          //allocate a new buffer
          sReceive.sCurrentMessage.lpData = GetReceiveBufferFromCache();
          if (sReceive.sCurrentMessage.lpData == NULL)
            return E_OUTOFMEMORY;
          if (sReceive.sCurrentMessage.aReceivedDataList.AddElement(sReceive.sCurrentMessage.lpData) == FALSE)
          {
            PutReceiveBufferOnCache(sReceive.sCurrentMessage.lpData);
            return E_OUTOFMEMORY;
          }
          sReceive.sCurrentMessage.nFilledFrame = 0;
        }

        //calculate data to copy
        nToRead = RECEIVE_DATA_BLOCK_SIZE - sReceive.sCurrentMessage.nFilledFrame;
        if (nToRead > nMsgSize)
          nToRead = nMsgSize;
        if (nToRead > (SIZE_T)(sReceive.nPayloadLen))
          nToRead = (SIZE_T)(sReceive.nPayloadLen);

        //copy data and apply masking if required
        if (sReceive.sFrameHeader.nMask != 0)
        {
          SIZE_T nMaskIdx = sReceive.sCurrentMessage.nFilledFrame;
          LPBYTE d = sReceive.sCurrentMessage.lpData;
          LPBYTE dEnd = d + nToRead;

          while (d < dEnd)
          {
            *d++ = *lpMsg++ ^sReceive.uMasking.nKey[nMaskIdx & 3];
            nMaskIdx++;
          }
        }
        else
        {
          ::MxMemCopy(sReceive.sCurrentMessage.lpData, lpMsg, nToRead);
          lpMsg += nToRead;
        }

        //advance buffer
        nMsgSize -= nToRead;

        sReceive.sCurrentMessage.lpData += nToRead;
        sReceive.sCurrentMessage.nTotalDataLength += nToRead;
        sReceive.nPayloadLen -= (ULONGLONG)nToRead;
        sReceive.sCurrentMessage.nFilledFrame += nToRead;

        //if the buffer is full, prepare to request a new one
        if (sReceive.sCurrentMessage.nFilledFrame == RECEIVE_DATA_BLOCK_SIZE)
        {
          sReceive.sCurrentMessage.lpData = NULL;
        }
      }
    }
    else
    {
      //control frame
      while (nMsgSize > 0 && sReceive.nPayloadLen > 0)
      {
        //calculate data to copy
        nToRead = (SIZE_T)(sReceive.nPayloadLen);
        if (nToRead > nMsgSize)
          nToRead = nMsgSize;

        //copy data and apply masking if required
        if (sReceive.sFrameHeader.nMask != 0)
        {
          SIZE_T nMaskIdx = sReceive.sCurrentControlFrame.nFilledFrame;
          LPBYTE d = sReceive.sCurrentControlFrame.aBuffer + sReceive.sCurrentControlFrame.nFilledFrame;
          LPBYTE dEnd = d + nToRead;

          while (d < dEnd)
          {
            *d++ = (*lpMsg) ^sReceive.uMasking.nKey[nMaskIdx & 3];
            lpMsg++;
            nMaskIdx++;
          }
        }
        else
        {
          ::MxMemCopy(sReceive.sCurrentControlFrame.aBuffer + sReceive.sCurrentControlFrame.nFilledFrame, lpMsg,
                      nToRead);
          lpMsg += nToRead;
        }

        //advance buffer
        nMsgSize -= nToRead;

        sReceive.sCurrentControlFrame.nFilledFrame += nToRead;
        sReceive.nPayloadLen -= (ULONGLONG)nToRead;
      }
    }
  }

  //do we reach the end of the frame?
  if (sReceive.nPayloadLen == 0ui64)
  {
    if ((sReceive.sFrameHeader.nOpcode & _OPCODE_ControlFrameMask) == 0)
    {
      //check if we reached a final frame
      if (sReceive.sFrameHeader.nFin != 0)
      {
        //final frame for message
        if (sReceive.sCurrentMessage.nTotalDataLength > 0)
        {
          LPBYTE lpData;

          if (sReceive.sCurrentMessage.nTotalDataLength < RECEIVE_DATA_BLOCK_SIZE)
          {
            //fast path
            lpData = sReceive.sCurrentMessage.aReceivedDataList.GetElementAt(0);
          }
          else
          {
            TAutoFreePtr<BYTE> aFullMsgBuf;
            SIZE_T i, nBuffersCount;

            aFullMsgBuf.Attach((LPBYTE)MX_MALLOC(sReceive.sCurrentMessage.nTotalDataLength));
            if (!aFullMsgBuf)
              return E_OUTOFMEMORY;

            lpData = aFullMsgBuf.Get();
            nBuffersCount = sReceive.sCurrentMessage.aReceivedDataList.GetCount();
            for (i = 0; i < nBuffersCount; i++)
            {
              ::MxMemCopy(lpData, sReceive.sCurrentMessage.aReceivedDataList.GetElementAt(i),
                          (i < nBuffersCount - 1)
                          ? RECEIVE_DATA_BLOCK_SIZE
                          : sReceive.sCurrentMessage.nTotalDataLength - (nBuffersCount - 1) * RECEIVE_DATA_BLOCK_SIZE);
              lpData += RECEIVE_DATA_BLOCK_SIZE;
            }

            lpData = aFullMsgBuf.Get();
          }

          if (sReceive.sCurrentMessage.nOpcode == _OPCODE_Text)
            OnTextMessage((LPCSTR)lpData, sReceive.sCurrentMessage.nTotalDataLength);
          else
            OnBinaryMessage(lpData, sReceive.sCurrentMessage.nTotalDataLength);
        }

        //reset message
        PutAllReceiveBuffersOnCache();
        sReceive.sCurrentMessage.nOpcode = _OPCODE_NONE;
        sReceive.sCurrentMessage.lpData = NULL;
        sReceive.sCurrentMessage.nFilledFrame = sReceive.sCurrentMessage.nTotalDataLength = 0;
      }
    }
    else
    {
      //process control frame
      switch (sReceive.sFrameHeader.nOpcode)
      {
        case _OPCODE_ConnectionClose:
          {
          USHORT wCode = 1000;
          BOOL bChanged = FALSE;

          if (sReceive.sCurrentControlFrame.nFilledFrame >= 2)
          {
            wCode = ((USHORT)(sReceive.sCurrentControlFrame.aBuffer[1]) |
                    ((USHORT)(sReceive.sCurrentControlFrame.aBuffer[0]) << 8));
          }
          switch (wCode)
          {
            case 1000:
              bChanged = (_InterlockedCompareExchange(&hrCloseError, S_OK, S_FALSE) == S_FALSE) ? TRUE : FALSE;
              break;

            case 1001:
              bChanged = (_InterlockedCompareExchange(&hrCloseError, MX_HRESULT_FROM_WIN32(WSAECONNRESET),
                                                      S_FALSE) == S_FALSE) ? TRUE : FALSE;
              break;

            case 1002:
              bChanged = (_InterlockedCompareExchange(&hrCloseError, MX_E_Unsupported,
                                                      S_FALSE) == S_FALSE) ? TRUE : FALSE;
              break;

            case 1003:
              bChanged = (_InterlockedCompareExchange(&hrCloseError, MX_E_InvalidData,
                                                      S_FALSE) == S_FALSE) ? TRUE : FALSE;
              break;

            default:
              bChanged = (_InterlockedCompareExchange(&hrCloseError, E_FAIL, S_FALSE) == S_FALSE) ? TRUE : FALSE;
              break;
          }

          //closed state
          if (bChanged != FALSE)
          {
            OnCloseFrame(wCode, __InterlockedRead(&hrCloseError));

            if (wCode == 1000)
              SendClose(wCode);
            lpIpc->Close(hConn, S_OK);
          }
          }
          return S_OK;

        case _OPCODE_Ping:
          //NOTE: InternalSendControlFrame modifies the payload if mask is active
          hRes = InternalSendControlFrame(_OPCODE_Pong, sReceive.sCurrentControlFrame.aBuffer,
                                          (ULONG)(sReceive.sCurrentControlFrame.nFilledFrame));
          if (FAILED(hRes))
            return hRes;
          break;

        case _OPCODE_Pong:
          OnPongFrame();
          break;
      }

      //reset control frame state
      ::MxMemSet(&(sReceive.sCurrentControlFrame), 0, sizeof(sReceive.sCurrentControlFrame));
    }

    //reset state to read next frame
    sReceive.nState = 0;
    sReceive.uMasking.dwKey = 0;
  }

  //jump to start
  goto consume_used_and_loop;
}

SIZE_T CWebSocket::BuildFrame(_In_ LPFRAME_HEADER lpFrame, _In_ LPBYTE lpPayload, _In_ ULONG nPayloadSize,
                              _In_ BYTE nOpcode, _In_ BOOL bFinal)
{
  SIZE_T nFrameLength, nExtendedLength;

  nFrameLength = (SIZE_T)FIELD_OFFSET(FRAME_HEADER, aExtended);

  lpFrame->nOpcode = nOpcode;
  lpFrame->nFin = (bFinal != FALSE) ? 1 : 0;
  lpFrame->nRsv = 0;
  if (nPayloadSize >= 65536)
  {
    lpFrame->nPayloadLen = 127;
    nExtendedLength = 8;
    lpFrame->aExtended[0] = lpFrame->aExtended[1] = lpFrame->aExtended[2] = lpFrame->aExtended[3] = 0;
    lpFrame->aExtended[4] = (BYTE)((nPayloadSize >> 24) & 255);
    lpFrame->aExtended[5] = (BYTE)((nPayloadSize >> 16) & 255);
    lpFrame->aExtended[6] = (BYTE)((nPayloadSize >>  8) & 255);
    lpFrame->aExtended[7] = (BYTE)( nPayloadSize        & 255);
  }
  else if (nPayloadSize >= 126)
  {
    lpFrame->nPayloadLen = 126;
    nExtendedLength = 2;
    lpFrame->aExtended[0] = (BYTE)((nPayloadSize >> 8) & 255);
    lpFrame->aExtended[1] = (BYTE)( nPayloadSize       & 255);
  }
  else
  {
    lpFrame->nPayloadLen = (BYTE)nPayloadSize;
    nExtendedLength = 0;
  }

  //prepare mask
  if (bServerSide == FALSE)
  {
    SIZE_T nThis;
    union {
      DWORD dw;
      BYTE b[4];
    } uMasking;
    LPBYTE d, dEnd;
    ULONG i;

    lpFrame->nMask = 1;

    uMasking.dw = ::GetTickCount();
    uMasking.dw = fnv_32a_buf(&(uMasking.dw), 4, FNV1A_32_INIT);
    nThis = (SIZE_T)this;
    uMasking.dw = fnv_32a_buf(&nThis, sizeof(nThis), uMasking.dw);
    uMasking.dw = fnv_32a_buf(lpFrame, nFrameLength, uMasking.dw);
    uMasking.dw = fnv_32a_buf(lpPayload, nPayloadSize, uMasking.dw);

    //encode frame
    dEnd = (d = lpPayload) + (SIZE_T)nPayloadSize;
    for (i = 0; d < dEnd; i++, d++)
    {
      *d ^= uMasking.b[i & 3];
    }

    lpFrame->aExtended[nExtendedLength    ] = uMasking.b[0];
    lpFrame->aExtended[nExtendedLength + 1] = uMasking.b[1];
    lpFrame->aExtended[nExtendedLength + 2] = uMasking.b[2];
    lpFrame->aExtended[nExtendedLength + 3] = uMasking.b[3];
    nExtendedLength += 4;
  }
  else
  {
    lpFrame->nMask = 0;
  }

  //done
  return nFrameLength + nExtendedLength;
}


HRESULT CWebSocket::InternalSendFrame(_In_ BOOL bFinalFrame)
{
  SIZE_T nFrameLength;
  HRESULT hRes;

  nFrameLength = BuildFrame(&(sSend.sFrameHeader), sSend.cFrameBuffer.Get(), sSend.nFilledFrame,
                            sSend.sFrameHeader.nOpcode, bFinalFrame);

  //send header and data
  hRes = lpIpc->SendMsg(hConn, &(sSend.sFrameHeader), nFrameLength);
  if (SUCCEEDED(hRes))
    hRes = lpIpc->SendMsg(hConn, sSend.cFrameBuffer.Get(), (SIZE_T)(sSend.nFilledFrame));

  //done
  return hRes;
}

HRESULT CWebSocket::InternalSendControlFrame(_In_ BYTE nOpcode, _In_ LPVOID lpPayload, _In_ ULONG nPayloadSize)
{
  FRAME_HEADER sFrameHeader;
  SIZE_T nFrameLength;
  HRESULT hRes;

  MX_ASSERT(nPayloadSize <= 125);

  nFrameLength = BuildFrame(&sFrameHeader, (LPBYTE)lpPayload, nPayloadSize, nOpcode, TRUE);

  //send header and data
  hRes = lpIpc->SendMsg(hConn, &sFrameHeader, nFrameLength);
  if (SUCCEEDED(hRes) && nPayloadSize > 0)
    hRes = lpIpc->SendMsg(hConn, lpPayload, (SIZE_T)nPayloadSize);

  //done
  return hRes;
}

LPBYTE CWebSocket::GetReceiveBufferFromCache()
{
  if (sReceiveCache.nNextBufferIndex > 0)
  {
    LPBYTE lpBuffer;

    (sReceiveCache.nNextBufferIndex)--;
    lpBuffer = sReceiveCache.lpBuffer[sReceiveCache.nNextBufferIndex];
    MX_ASSERT(lpBuffer != NULL);
    sReceiveCache.lpBuffer[sReceiveCache.nNextBufferIndex] = NULL;
    return lpBuffer;
  }
  return (LPBYTE)MX_MALLOC(RECEIVE_DATA_BLOCK_SIZE);
}

VOID CWebSocket::PutReceiveBufferOnCache(_In_ LPBYTE lpBuffer)
{
  if (sReceiveCache.nNextBufferIndex < MX_ARRAYLEN(sReceiveCache.lpBuffer))
  {
    MX_ASSERT(sReceiveCache.lpBuffer[sReceiveCache.nNextBufferIndex] == NULL);
    sReceiveCache.lpBuffer[sReceiveCache.nNextBufferIndex] = lpBuffer;
    (sReceiveCache.nNextBufferIndex)++;
    return;
  }
  MX_FREE(lpBuffer);
  return;
}

VOID CWebSocket::PutAllReceiveBuffersOnCache()
{
  SIZE_T i, nCount;
  LPBYTE lpBuffers[MX_ARRAYLEN(sReceiveCache.lpBuffer)];

  nCount = sReceive.sCurrentMessage.aReceivedDataList.GetCount();
  if (nCount > MX_ARRAYLEN(sReceiveCache.lpBuffer) - sReceiveCache.nNextBufferIndex)
    nCount = MX_ARRAYLEN(sReceiveCache.lpBuffer) - sReceiveCache.nNextBufferIndex;

  if (nCount > 0)
  {
    for (i = 0; i < nCount; i++)
      lpBuffers[i] = sReceive.sCurrentMessage.aReceivedDataList.GetElementAt(i);
    sReceive.sCurrentMessage.aReceivedDataList.RemoveElementAt(0, nCount, FALSE);
    for (i = 0; i < nCount; i++)
      PutReceiveBufferOnCache(lpBuffers[i]);
  }
  sReceive.sCurrentMessage.aReceivedDataList.RemoveAllElements();
  return;
}

HRESULT CWebSocket::SetupIpc(_In_ CIpc *_lpIpc, _In_ HANDLE _hConn, _In_ BOOL _bServerSide)
{
  CIpc::CHANGE_CALLBACKS_DATA sNewCallbacks;
  HRESULT hRes;

  //setup new connections callbacks
  sNewCallbacks.bDataReceivedCallbackIsValid = TRUE;
  sNewCallbacks.cDataReceivedCallback = MX_BIND_MEMBER_CALLBACK(&CWebSocket::OnSocketDataReceived, this);
  sNewCallbacks.bDestroyCallbackIsValid = TRUE;
  sNewCallbacks.cDestroyCallback = MX_BIND_MEMBER_CALLBACK(&CWebSocket::OnSocketDestroy, this);
  sNewCallbacks.bUserDataIsValid = TRUE;
  sNewCallbacks.cUserData = this;
  hRes = _lpIpc->SetCallbacks(_hConn, sNewCallbacks);
  if (FAILED(hRes))
    return hRes;

  //set websocket parameters
  lpIpc = _lpIpc;
  hConn = _hConn;
  bServerSide = _bServerSide;

  //done
  return S_OK;
}

VOID CWebSocket::FireConnectedAndInitialRead()
{
  TAutoRefCounted<CWebSocket> cWebSocket = this;
  HRESULT hRes;

  hRes = OnConnected();
  if (SUCCEEDED(hRes))
    hRes = OnSocketDataReceived(lpIpc, hConn, this);
  if (FAILED(hRes))
    lpIpc->Close(hConn, hRes);
  return;
}

} //namespace MX
