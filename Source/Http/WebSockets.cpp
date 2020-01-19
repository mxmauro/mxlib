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

#define MAX_PAYLOAD_SIZE                               32768
#define SEND_PAYLOAD_SIZE                              16384
#define RECEIVE_DATA_BLOCK_SIZE                        16384

#define XOR_PING                                  0xA64F239A

//-----------------------------------------------------------

namespace MX {

CWebSocket::CWebSocket() : CIpc::CUserData()
{
  bServerSide = FALSE;
  //----
  sReceive.nState = 0;
  sReceive.uMasking.dwKey = 0;
  sReceive.sCurrentMessage.nOpcode = _OPCODE_NONE;
  sReceive.sCurrentMessage.lpData = NULL;
  sReceive.sCurrentMessage.nFilledFrame = sReceive.sCurrentMessage.nTotalDataLength = 0;
  MxMemSet(&(sReceive.sCurrentControlFrame), 0, sizeof(sReceive.sCurrentControlFrame));
  sReceive.hrCloseError = S_OK;
  //----
  MxMemSet(&(sSend.sFrameHeader), 0, sizeof(sSend.sFrameHeader));
  sSend.sFrameHeader.nOpcode = _OPCODE_NONE;
  sSend.lpFrameData = NULL;
  sSend.nFilledFrame = 0;
  //----
  MxMemSet(&sReceiveCache, 0, sizeof(sReceiveCache));
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
  if (sSend.sFrameHeader.nOpcode != _OPCODE_NONE)
    return MX_E_OperationInProgress;
  sSend.sFrameHeader.nOpcode = _OPCODE_Text;
  //create frame buffer if not done yet
  if (!(sSend.cFrameBuffer))
  {
    sSend.cFrameBuffer.Attach((LPBYTE)MX_MALLOC(SEND_PAYLOAD_SIZE));
    if (!(sSend.cFrameBuffer))
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CWebSocket::BeginBinaryMessage()
{
  if (sSend.sFrameHeader.nOpcode != _OPCODE_NONE)
    return MX_E_OperationInProgress;
  sSend.sFrameHeader.nOpcode = _OPCODE_Binary;
  //create frame buffer if not done yet
  if (!(sSend.cFrameBuffer))
  {
    sSend.cFrameBuffer.Attach((LPBYTE)MX_MALLOC(SEND_PAYLOAD_SIZE));
    if (!(sSend.cFrameBuffer))
      return E_OUTOFMEMORY;
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
  if (sSend.sFrameHeader.nOpcode == _OPCODE_NONE)
    return MX_E_NotReady;
  if (nDataLen == 0)
    return S_OK;
  if (lpData == NULL)
    return E_POINTER;
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
    MxMemCopy(sSend.lpFrameData, lpData, nToWrite);

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
  if (sSend.sFrameHeader.nOpcode == _OPCODE_NONE)
    return MX_E_NotReady;

  if (sSend.nFilledFrame > 0)
  {
    HRESULT hRes;

    hRes = InternalSendFrame(TRUE);
    if (FAILED(hRes))
      return hRes;

    //finalize send
    MxMemSet(&(sSend.sFrameHeader), 0, sizeof(sSend.sFrameHeader));
    sSend.sFrameHeader.nOpcode = _OPCODE_NONE;
    sSend.lpFrameData = NULL;
    sSend.nFilledFrame = 0;
  }
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
  MxMemCopy(aPayload + 2, szReasonA, nReasonLen);
  return InternalSendControlFrame(_OPCODE_ConnectionClose, aPayload, 2 + (ULONG)nReasonLen);
}

HRESULT CWebSocket::SendPing()
{
  DWORD dw = (DWORD)(SIZE_T)this;

  dw ^= XOR_PING;
  return InternalSendControlFrame(_OPCODE_Ping, &dw, (ULONG)sizeof(dw));
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
  if (SUCCEEDED(hrErrorCode))
    SendClose(wCode);
  if (sReceive.nState != 1000)
    lpIpc->Close(hConn, hrErrorCode);
  return;
}

SIZE_T CWebSocket::GetMaxMessageSize() const
{
  return 8192;
}

VOID CWebSocket::OnSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                                 _In_ HRESULT hrErrorCode)
{
  if (sReceive.nState != 1000)
  {
    sReceive.nState = 1000;
    OnCloseFrame((SUCCEEDED(hrErrorCode) ? 1000 : 1006), hrErrorCode);
  }
  return;
}

HRESULT CWebSocket::OnSocketDataReceived(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData)
{
  BYTE aMsgBuf[4096], *lpMsg;
  SIZE_T nMsgSize;
  HRESULT hRes;

  //closed?
  if (sReceive.nState == 1000)
  {
    return sReceive.hrCloseError;
  }

  //start of loop
loop:
  //get buffered message
  nMsgSize = sizeof(aMsgBuf);
  hRes = lpIpc->GetBufferedMessage(hConn, aMsgBuf, &nMsgSize);
  if (FAILED(hRes))
    return hRes;
  if (nMsgSize == 0)
    return S_OK; //nothing else to do
  lpIpc->ConsumeBufferedMessage(hConn, nMsgSize);
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
    goto loop;

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
          MxMemCopy(sReceive.sCurrentMessage.lpData, lpMsg, nToRead);
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
            *d++ = *lpMsg++ ^sReceive.uMasking.nKey[nMaskIdx & 3];
            nMaskIdx++;
          }
        }
        else
        {
          MxMemCopy(sReceive.sCurrentControlFrame.aBuffer + sReceive.sCurrentControlFrame.nFilledFrame, lpMsg, nToRead);
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
            if (sReceive.sCurrentMessage.nOpcode == _OPCODE_Text)
              OnTextMessage((LPCSTR)lpData, sReceive.sCurrentMessage.nTotalDataLength);
            else
              OnBinaryMessage(lpData, sReceive.sCurrentMessage.nTotalDataLength);
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
              MxMemCopy(lpData, sReceive.sCurrentMessage.aReceivedDataList.GetElementAt(i),
                      (i < nBuffersCount - 1) ? RECEIVE_DATA_BLOCK_SIZE :
                                                sReceive.sCurrentMessage.nTotalDataLength -
                                                (nBuffersCount - 1) * RECEIVE_DATA_BLOCK_SIZE);
              lpData += RECEIVE_DATA_BLOCK_SIZE;
            }

            lpData = aFullMsgBuf.Get();
            if (sReceive.sCurrentMessage.nOpcode == _OPCODE_Text)
              OnTextMessage((LPCSTR)lpData, sReceive.sCurrentMessage.nTotalDataLength);
            else
              OnBinaryMessage(lpData, sReceive.sCurrentMessage.nTotalDataLength);
          }
        }

        //reset message
        PutAllReceiveBuffersOnCache();
        sReceive.sCurrentMessage.nOpcode = _OPCODE_NONE;
        sReceive.sCurrentMessage.lpData = NULL;
        sReceive.sCurrentMessage.nFilledFrame = sReceive.sCurrentMessage.nTotalDataLength = 0;
      }

      //reset state to read next frame
      sReceive.nState = 0;
      sReceive.uMasking.dwKey = 0;
    }
    else
    {
      //process control frame
      switch (sReceive.sFrameHeader.nOpcode)
      {
        case _OPCODE_ConnectionClose:
          {
          USHORT wCode = 1000;

          if (sReceive.sCurrentControlFrame.nFilledFrame >= 2)
          {
            wCode = ((USHORT)(sReceive.sCurrentControlFrame.aBuffer[1]) |
                    ((USHORT)(sReceive.sCurrentControlFrame.aBuffer[0]) << 8));
          }
          switch (wCode)
          {
            case 1000:
              break;

            case 1001:
              sReceive.hrCloseError = MX_HRESULT_FROM_WIN32(WSAECONNRESET);
              break;

            case 1002:
              sReceive.hrCloseError = MX_E_Unsupported;
              break;

            case 1003:
              sReceive.hrCloseError = MX_E_InvalidData;
              break;

            default:
              sReceive.hrCloseError = E_FAIL;
              break;
          }

          //closed state
          sReceive.nState = 1000;
          OnCloseFrame(wCode, sReceive.hrCloseError);
          }
          return sReceive.hrCloseError;

        case _OPCODE_Ping:
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
      MxMemSet(&(sReceive.sCurrentControlFrame), 0, sizeof(sReceive.sCurrentControlFrame));

      //reset state to read next frame
      sReceive.nState = 0;
      sReceive.uMasking.dwKey = 0;
    }
  }

  //jump to start
  goto loop;
}

SIZE_T CWebSocket::CalculateFrameSize(_In_ ULONG nPayloadSize)
{
  SIZE_T nSize;

  MX_ASSERT(nPayloadSize <= MAX_PAYLOAD_SIZE);
  nSize = (nPayloadSize > 125) ? 4 : 2;
  if (bServerSide == FALSE)
    nSize += 4; //client side MUST always mask
  return nSize + (SIZE_T)nPayloadSize;
}

LPBYTE CWebSocket::BuildFrame(_In_ LPVOID lpFrame, _In_ ULONG nPayloadSize, _In_ BOOL bFin, _In_ BYTE nRSV,
                              _In_ BYTE nOpcode)
{
  LPBYTE d;
  ULONG i;

  MX_ASSERT(lpFrame != NULL);
  MX_ASSERT(nPayloadSize <= MAX_PAYLOAD_SIZE);

  ((FRAME_HEADER*)lpFrame)->nOpcode = nOpcode;
  ((FRAME_HEADER*)lpFrame)->nRsv = nRSV;
  ((FRAME_HEADER*)lpFrame)->nFin = bFin ? 1 : 0;
  ((FRAME_HEADER*)lpFrame)->nPayloadLen = (nPayloadSize > 125) ? 126 : (BYTE)nPayloadSize;
  d = (LPBYTE)lpFrame + 2;

  if (nPayloadSize > 125)
  {
    *d++ = (BYTE)(nPayloadSize >> 8);
    *d++ = (BYTE)(nPayloadSize & 0xFF);
  }

  if (bServerSide == FALSE)
  {
    DWORD dw;
    union {
      BYTE nKey[4];
      DWORD dwKey;
    } uMasking;

    ((FRAME_HEADER*)lpFrame)->nMask = 1;

    dw = ::GetTickCount();
    uMasking.dwKey = fnv_32a_buf(&dw, 4, FNV1A_32_INIT);
    dw = (DWORD)(SIZE_T)this;
    uMasking.dwKey = fnv_32a_buf(&dw, 4, uMasking.dwKey);
    uMasking.dwKey = fnv_32a_buf(&lpFrame, sizeof(LPBYTE), uMasking.dwKey);
    uMasking.dwKey = fnv_32a_buf(&nPayloadSize, sizeof(nPayloadSize), uMasking.dwKey);
    uMasking.dwKey = fnv_32a_buf(lpFrame, 2, uMasking.dwKey);

    for (i = 0; i < 4; i++)
      *d++ = uMasking.nKey[i];
  }
  else
  {
    ((FRAME_HEADER*)lpFrame)->nMask = 0;
  }
  return d;
}

VOID CWebSocket::EncodeFrame(_In_ LPVOID _lpFrame)
{
  if (bServerSide == FALSE)
  {
    FRAME_HEADER *lpFrame = (FRAME_HEADER*)_lpFrame;
    LPBYTE d;
    ULONG i, nPayloadLen;
    LPBYTE lpMask;

    d = (LPBYTE)(lpFrame + 1);
    if (lpFrame->nPayloadLen < 126)
    {
      nPayloadLen = (ULONG)(lpFrame->nPayloadLen);
    }
    else
    {
      nPayloadLen = (ULONG)(*d++);
      nPayloadLen = (nPayloadLen << 8) | (ULONG)(*d++);
      d += 2;
    }

    lpMask = d;
    d += 4;

    for (i = 0; i < nPayloadLen; i++,d++)
    {
      *d ^= lpMask[i & 3];
    }
  }
  return;
}

HRESULT CWebSocket::InternalSendFrame(_In_ BOOL bFinalFrame)
{
  BYTE aTempBuffer[sizeof(FRAME_HEADER) + 2 + 4];
  SIZE_T nTempBufferLen;
  union {
    BYTE nKey[4];
    DWORD dwKey;
  } uMasking;
  HRESULT hRes;

  sSend.sFrameHeader.nFin = (bFinalFrame != FALSE) ? 1 : 0;
  sSend.sFrameHeader.nPayloadLen = (sSend.nFilledFrame <= 125) ?(BYTE)sSend.nFilledFrame : 126;

  MxMemCopy(aTempBuffer, &(sSend.sFrameHeader), sizeof(FRAME_HEADER));
  nTempBufferLen = sizeof(FRAME_HEADER);

  //prepare mask
  if (bServerSide == FALSE)
  {
    DWORD dw;
    LPBYTE d;

    sSend.sFrameHeader.nMask = 1;

    dw = ::GetTickCount();
    uMasking.dwKey = fnv_32a_buf(&dw, 4, FNV1A_32_INIT);
    dw = (DWORD)(SIZE_T)this;
    uMasking.dwKey = fnv_32a_buf(&dw, 4, uMasking.dwKey);
    uMasking.dwKey = fnv_32a_buf(&(sSend.sFrameHeader), sizeof(sSend.sFrameHeader), uMasking.dwKey);
    uMasking.dwKey = fnv_32a_buf(&(sSend.nFilledFrame), sizeof(sSend.nFilledFrame), uMasking.dwKey);

    //encode frame
    d = sSend.cFrameBuffer.Get();
    for (dw = 0; dw < sSend.nFilledFrame; dw++, d++)
    {
      *d ^= uMasking.nKey[dw & 3];
    }
  }
  else
  {
    sSend.sFrameHeader.nMask = 0;
  }

  //add extended payload size
  if (sSend.nFilledFrame > 125)
  {
    aTempBuffer[nTempBufferLen++] = (BYTE)(sSend.nFilledFrame >> 8);
    aTempBuffer[nTempBufferLen++] = (BYTE)(sSend.nFilledFrame & 0xFF);
  }

  //add masking key
  if (bServerSide == FALSE)
  {
    aTempBuffer[nTempBufferLen++] = uMasking.nKey[0];
    aTempBuffer[nTempBufferLen++] = uMasking.nKey[1];
    aTempBuffer[nTempBufferLen++] = uMasking.nKey[2];
    aTempBuffer[nTempBufferLen++] = uMasking.nKey[3];
  }

  //send header
  hRes = lpIpc->SendMsg(hConn, aTempBuffer, nTempBufferLen);
  //send data
  if (SUCCEEDED(hRes))
    hRes = lpIpc->SendMsg(hConn, sSend.cFrameBuffer.Get(), (SIZE_T)(sSend.nFilledFrame));
  //done
  return hRes;
}

HRESULT CWebSocket::InternalSendControlFrame(_In_ BYTE nOpcode, _In_ LPVOID lpPayload, _In_ ULONG nPayloadSize)
{
  BYTE aFrameBuffer[32 + 128], *lpFrameData;

  MX_ASSERT(nPayloadSize <= 125);

  lpFrameData = BuildFrame(aFrameBuffer, nPayloadSize, TRUE, 0, nOpcode);
  MxMemCopy(lpFrameData, lpPayload, nPayloadSize);
  EncodeFrame(aFrameBuffer);

  //send frame
  return lpIpc->SendMsg(hConn, &aFrameBuffer, CalculateFrameSize(nPayloadSize));
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

} //namespace MX
