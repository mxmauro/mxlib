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
#include "NtLightWeightIPC.h"

//-----------------------------------------------------------

#ifndef OBJ_CASE_INSENSITIVE
  #define OBJ_CASE_INSENSITIVE         0x00000040
#endif //!OBJ_CASE_INSENSITIVE

#ifndef FILE_OPEN
  #define FILE_OPEN                    0x00000001
#endif //!FILE_OPEN
#ifndef FILE_WRITE_THROUGH
  #define FILE_WRITE_THROUGH           0x00000002
#endif //!FILE_WRITE_THROUGH
#ifndef FILE_NON_DIRECTORY_FILE
  #define FILE_NON_DIRECTORY_FILE      0x00000040
#endif //!FILE_NON_DIRECTORY_FILE

#define FNV1A_32_INIT 0x811C9DC5UL

#define CLIENTCONNECT_RETRY_COUNT 10
#define CLIENTCONNECT_RETRY_DELAY 100

#define CHAININDEX_CANCEL 0xFFFFFFFFUL

//-----------------------------------------------------------

#pragma pack(1)
typedef struct {
  ULONG nTotalSize;
  ULONG nId;
  ULONG nChainIndex;
  ULONG nSize;
  ULONG nCheckSum;
} HEADER;

typedef struct {
  HEADER sHdr;
  BYTE aBuffer[NTLIGHTWEIGHTIPC_PACKET_SIZE-sizeof(HEADER)];
} DATA;
#pragma pack()

//-----------------------------------------------------------

static BOOL ValidatePacket(__in DWORD dwReadedBytes, __in DATA &sData);
static ULONG FNV1A_32(__in LPCVOID lpBuf, __in SIZE_T nBufLen, __in ULONG nHash);

//-----------------------------------------------------------

namespace MX {

CNtLightWeightIPC::CNtLightWeightIPC()
{
  _InterlockedExchange(&nNextSendId, 0);
  return;
}

CNtLightWeightIPC::~CNtLightWeightIPC()
{
  Disconnect();
  return;
}

HRESULT CNtLightWeightIPC::ConnectToServer(__in_z LPCWSTR szServerNameW)
{
  PMX_UNICODE_STRING usServerName;
  SIZE_T nLen;
  LARGE_INTEGER liTime;
  SECURITY_DESCRIPTOR sSecDesc;
  MX_OBJECT_ATTRIBUTES sObjAttr;
  MX_IO_STATUS_BLOCK sIosb;
  DWORD dwRetry;
  NTSTATUS nNtStatus;

  if (szServerNameW == NULL || szServerNameW[0] == 0)
    return STATUS_INVALID_PARAMETER;
  nLen = StrLenW(szServerNameW);
  if (nLen > 10240)
    return STATUS_INVALID_PARAMETER;
  nLen *= sizeof(WCHAR);
  usServerName = (PMX_UNICODE_STRING)::MxRtlAllocateHeap(::MxGetProcessHeap(), 0, sizeof(MX_UNICODE_STRING)+18+nLen);
  if (usServerName != NULL)
  {
    usServerName->Length = usServerName->MaximumLength = 18 + (USHORT)nLen;
    usServerName->Buffer = (PWSTR)(usServerName+1);
    MemCopy(usServerName->Buffer, L"\\??\\Pipe\\", 18);
    MemCopy(&usServerName->Buffer[9], szServerNameW, nLen);
    nNtStatus = STATUS_SUCCESS;
  }
  else
  {
    nNtStatus = STATUS_INSUFFICIENT_RESOURCES;
  }
  //create read/write event
  if (NT_SUCCESS(nNtStatus))
  {
    if (cEvent.Create(TRUE, FALSE) == FALSE)
      nNtStatus = STATUS_INSUFFICIENT_RESOURCES;
  }
  //create pipe
  if (NT_SUCCESS(nNtStatus))
  {
    MemSet(&sSecDesc, 0, sizeof(sSecDesc));
    sSecDesc.Revision = SECURITY_DESCRIPTOR_REVISION;
    sSecDesc.Control = SE_DACL_PRESENT;
    MemSet(&sObjAttr, 0, sizeof(sObjAttr));
    sObjAttr.Length = (ULONG)sizeof(sObjAttr);
    sObjAttr.Attributes = OBJ_CASE_INSENSITIVE;
    sObjAttr.ObjectName = usServerName;
    sObjAttr.SecurityDescriptor = &sSecDesc;
    dwRetry = CLIENTCONNECT_RETRY_COUNT;
    while (1)
    {
      MX::MemSet(&sIosb, 0, sizeof(sIosb));
      nNtStatus = ::MxNtCreateFile(&cPipe, GENERIC_READ|GENERIC_WRITE|SYNCHRONIZE|FILE_READ_ATTRIBUTES, &sObjAttr,
                                   &sIosb, NULL, 0, 0, FILE_OPEN, FILE_NON_DIRECTORY_FILE|FILE_WRITE_THROUGH, NULL, 0);
      if (NT_SUCCESS(nNtStatus))
        break;
      if ((--dwRetry) == 0 ||
          (nNtStatus != STATUS_PIPE_NOT_AVAILABLE && nNtStatus != STATUS_INSTANCE_NOT_AVAILABLE &&
           nNtStatus != STATUS_PIPE_BUSY))
        break;
      liTime.QuadPart = MX_MILLISECONDS_TO_100NS(CLIENTCONNECT_RETRY_DELAY);
      ::MxNtDelayExecution(FALSE, &liTime);
    }
  }
  //done
  if (usServerName != NULL)
    ::MxRtlFreeHeap(::MxGetProcessHeap(), 0, usServerName);
  if (!NT_SUCCESS(nNtStatus))
    Disconnect();
  return nNtStatus;
}

VOID CNtLightWeightIPC::Disconnect()
{
  cEvent.Close();
  cPipe.Close();
  _InterlockedExchange(&nNextSendId, 0);
  return;
}

NTSTATUS CNtLightWeightIPC::SendMsg(__in LPCVOID lpMsg, __in SIZE_T nMsgSize, __in_opt CMessage *lpReplyMsg,
                                    __in_opt ULONG nTimeout)
{
  MX_IO_STATUS_BLOCK sIosb;
  DATA sData;
  ULONG nMsgId, nChainIdx;
  LARGE_INTEGER liTimeout, liTimeoutStep;
  LPBYTE s;
  SIZE_T nCurrMsgSize, nTotalMsgSize;
  NTSTATUS nNtStatus, nNtStatus2;

  if (lpReplyMsg != NULL)
    lpReplyMsg->Reset();
  if (lpMsg == NULL || nMsgSize == 0 || nMsgSize > 0x7FFFFFFF)
    return STATUS_INVALID_PARAMETER;
  if (cPipe == NULL)
    return STATUS_PIPE_DISCONNECTED;
  s = (LPBYTE)lpMsg;
  do
  {
    nMsgId = (ULONG)_InterlockedIncrement(&nNextSendId) & 0x7FFFFFFFUL;
  }
  while (nMsgId == 0);
  nNtStatus = STATUS_SUCCESS;
  nTotalMsgSize = (ULONG)nMsgSize;
  nChainIdx = 1;
  while (NT_SUCCESS(nNtStatus) && nMsgSize > 0)
  {
    sData.sHdr.nId = nMsgId;
    sData.sHdr.nChainIndex = nChainIdx++;
    sData.sHdr.nTotalSize = (ULONG)nTotalMsgSize;
    sData.sHdr.nSize = (ULONG)((nMsgSize > sizeof(sData.aBuffer)) ? sizeof(sData.aBuffer) : nMsgSize);
    MemCopy(sData.aBuffer, s, (SIZE_T)(sData.sHdr.nSize));
    //calculate checksum
    sData.sHdr.nCheckSum = FNV1A_32(&(sData.sHdr), sizeof(sData.sHdr)-sizeof(DWORD), FNV1A_32_INIT);
    sData.sHdr.nCheckSum = FNV1A_32(s, (SIZE_T)(sData.sHdr.nSize), sData.sHdr.nCheckSum);
    cEvent.Reset();
    MX::MemSet(&sIosb, 0, sizeof(sIosb));
    nNtStatus = ::MxNtWriteFile(cPipe, cEvent, NULL, NULL, &sIosb, &sData, (ULONG)sizeof(sData.sHdr)+sData.sHdr.nSize,
                                NULL, NULL);
    if (nNtStatus == STATUS_PENDING)
    {
      //wait until completed
      if (nTimeout == INFINITE)
      {
        nNtStatus = ::MxNtWaitForSingleObject(cEvent, FALSE, NULL);
      }
      else
      {
        liTimeout.QuadPart = MX_MILLISECONDS_TO_100NS((ULONGLONG)nTimeout);
        while (nNtStatus == STATUS_PENDING && liTimeout.QuadPart < 0)
        {
          liTimeoutStep.QuadPart = liTimeout.QuadPart;
          if (liTimeoutStep.QuadPart < MX_MILLISECONDS_TO_100NS(100))
            liTimeoutStep.QuadPart = MX_MILLISECONDS_TO_100NS(100);
          nNtStatus = ::MxNtWaitForSingleObject(cEvent, FALSE, &liTimeoutStep);
          if (nNtStatus == STATUS_TIMEOUT)
            nNtStatus = STATUS_PENDING;
          liTimeout.QuadPart -= liTimeoutStep.QuadPart;
        }
      }
      if (NT_SUCCESS(nNtStatus))
        nNtStatus = sIosb.Status;
    }
    if (!NT_SUCCESS(nNtStatus))
    {
      MX::MemSet(&sIosb, 0, sizeof(sIosb));
      ::MxNtCancelIoFile(cPipe, &sIosb);
      //send cancel message
      sData.sHdr.nChainIndex = CHAININDEX_CANCEL;
      sData.sHdr.nSize = sData.sHdr.nTotalSize = 0;
      //calculate checksum
      sData.sHdr.nCheckSum = FNV1A_32(&(sData.sHdr), sizeof(sData.sHdr)-sizeof(DWORD), FNV1A_32_INIT);
      sData.sHdr.nCheckSum = FNV1A_32(s, (SIZE_T)(sData.sHdr.nSize), sData.sHdr.nCheckSum);
      cEvent.Reset();
      nNtStatus2 = ::MxNtWriteFile(cPipe, cEvent, NULL, NULL, &sIosb, &sData, (ULONG)sizeof(sData), NULL, NULL);
      if (nNtStatus2 == STATUS_PENDING)
        nNtStatus2 = ::MxNtWaitForSingleObject(cEvent, FALSE, NULL);
      if (NT_SUCCESS(nNtStatus2))
        nNtStatus2 = sIosb.Status;
      break;
    }
    //next block
    s += (SIZE_T)(sData.sHdr.nSize);
    nMsgSize -= (SIZE_T)(sData.sHdr.nSize);
  }
  //now wait for the answer
  if (NT_SUCCESS(nNtStatus) && lpReplyMsg != NULL)
  {
    nChainIdx = 0;
    nCurrMsgSize = nTotalMsgSize = 0;
    while (NT_SUCCESS(nNtStatus))
    {
      cEvent.Reset();
      MX::MemSet(&sIosb, 0, sizeof(sIosb));
      nNtStatus = ::MxNtReadFile(cPipe, cEvent, NULL, NULL, &sIosb, &sData, (ULONG)sizeof(sData), NULL, NULL);
      if (nNtStatus == STATUS_PENDING)
      {
        //wait until completed
        if (nTimeout == INFINITE)
        {
          nNtStatus = ::MxNtWaitForSingleObject(cEvent, FALSE, NULL);
        }
        else
        {
          liTimeout.QuadPart = MX_MILLISECONDS_TO_100NS((ULONGLONG)nTimeout);
          while (nNtStatus == STATUS_PENDING && liTimeout.QuadPart < 0)
          {
            liTimeoutStep.QuadPart = liTimeout.QuadPart;
            if (liTimeoutStep.QuadPart < MX_MILLISECONDS_TO_100NS(100))
              liTimeoutStep.QuadPart = MX_MILLISECONDS_TO_100NS(100);
            nNtStatus = ::MxNtWaitForSingleObject(cEvent, FALSE, &liTimeoutStep);
            if (nNtStatus == STATUS_TIMEOUT)
              nNtStatus = STATUS_PENDING;
            liTimeout.QuadPart -= liTimeoutStep.QuadPart;
          }
        }
        if (NT_SUCCESS(nNtStatus))
          nNtStatus = sIosb.Status;
      }
      //process packet
      if (NT_SUCCESS(nNtStatus))
      {
        //do validation
        if (ValidatePacket((DWORD)(sIosb.Information), sData) == FALSE)
        {
          MX_ASSERT(FALSE);
          nNtStatus = STATUS_INTERNAL_ERROR;
        }
      }
      //check if the packet belongs to our message's answer and ignore the rest
      if (NT_SUCCESS(nNtStatus) && sData.sHdr.nId == (nMsgId|0x80000000UL))
      {
        //check if a "cancel" message code
        if (sData.sHdr.nChainIndex != CHAININDEX_CANCEL)
        {
          //normal message
          if (nChainIdx == 0)
          {
            if (sData.sHdr.nChainIndex == 1)
            {
              nTotalMsgSize = (SIZE_T)(sData.sHdr.nTotalSize);
              if (lpReplyMsg->EnsureSize(nTotalMsgSize) == FALSE)
                nNtStatus = STATUS_INSUFFICIENT_RESOURCES;
            }
            else
            {
              MX_ASSERT(FALSE);
              nNtStatus = STATUS_INTERNAL_ERROR;
            }
          }
          else
          {
            //a message exists, first validate packet
            if (sData.sHdr.nChainIndex != nChainIdx + 1 ||
                (SIZE_T)(sData.sHdr.nTotalSize) != nTotalMsgSize ||
                nCurrMsgSize + (SIZE_T)(sData.sHdr.nSize) < nCurrMsgSize ||
                nCurrMsgSize + (SIZE_T)(sData.sHdr.nSize) > nTotalMsgSize)
            {
              MX_ASSERT(FALSE);
              nNtStatus = STATUS_INTERNAL_ERROR;
            }
          }
          //add packet to chain
          if (NT_SUCCESS(nNtStatus))
          {
            if (lpReplyMsg->Add(sData.aBuffer, (SIZE_T)(sData.sHdr.nSize)) != FALSE)
            {
              nCurrMsgSize += (SIZE_T)(sData.sHdr.nSize);
              //end of message reached?
              if (nCurrMsgSize == nTotalMsgSize)
                break;
            }
            else
            {
              nNtStatus = STATUS_INSUFFICIENT_RESOURCES;
            }
          }
        }
        else
        {
          nNtStatus = STATUS_CANCELLED;
        }
      }
    }
  }
  //done
  if (!NT_SUCCESS(nNtStatus) && lpReplyMsg != NULL)
    lpReplyMsg->Reset();
  return nNtStatus;
}

//----------------

CNtLightWeightIPC::CMessage::CMessage() : CBaseMemObj()
{
  lpData = NULL;
  nDataLen = nDataSize = 0;
  return;
}

CNtLightWeightIPC::CMessage::~CMessage()
{
  Reset();
  return;
}

VOID CNtLightWeightIPC::CMessage::Reset()
{
  if (lpData != NULL)
  {
    ::MxRtlFreeHeap(::MxGetProcessHeap(), 0, lpData);
    lpData = NULL;
  }
  nDataLen = nDataSize = 0;
  return;
}

BOOL CNtLightWeightIPC::CMessage::EnsureSize(__in SIZE_T nSize)
{
  LPBYTE lpNewData;

  if (nSize > nDataSize)
  {
    nSize = (nSize + 4095) & (~4095);
    if (lpData == NULL)
      lpNewData = (LPBYTE)::MxRtlAllocateHeap(::MxGetProcessHeap(), 0, nSize);
    else
      lpNewData = (LPBYTE)::MxRtlReAllocateHeap(::MxGetProcessHeap(), 0, lpData, nSize);
    if (lpNewData == NULL)
      return FALSE;
    lpData = lpNewData;
    nDataSize = nSize;
  }
  return TRUE;
}

BOOL CNtLightWeightIPC::CMessage::Add(__in LPCVOID _lpData, __in SIZE_T _nDataSize)
{
  if (_nDataSize == 0)
    return TRUE;
  if (_lpData == NULL)
    return FALSE;
  if (nDataLen+_nDataSize < nDataLen ||
      EnsureSize(nDataLen+_nDataSize) == FALSE)
    return FALSE;
  MemCopy(lpData+nDataLen, _lpData, _nDataSize);
  nDataLen += _nDataSize;
  return TRUE;
}

} //namespace MX

//-----------------------------------------------------------

static BOOL ValidatePacket(__in DWORD dwReadedBytes, __in DATA &sData)
{
  ULONG nHash;

  if (dwReadedBytes < sizeof(sData.sHdr))
    return FALSE;
  nHash = FNV1A_32(&(sData.sHdr), sizeof(sData.sHdr)-sizeof(DWORD), FNV1A_32_INIT);
  nHash = FNV1A_32(sData.aBuffer, (SIZE_T)dwReadedBytes-sizeof(sData.sHdr), nHash);
  if (sData.sHdr.nCheckSum != nHash ||
      sData.sHdr.nTotalSize == 0 || sData.sHdr.nTotalSize > NTLIGHTWEIGHTIPC_MESSAGE_SIZE ||
      dwReadedBytes != ((DWORD)sizeof(sData.sHdr) + sData.sHdr.nSize) ||
      sData.sHdr.nSize > sData.sHdr.nTotalSize || sData.sHdr.nSize > sizeof(sData.aBuffer))
    return FALSE;
  return TRUE;
}

static ULONG FNV1A_32(__in LPCVOID lpBuf, __in SIZE_T nBufLen, __in ULONG nHash)
{
  LPBYTE s, sEnd;

  s = (LPBYTE)lpBuf;
  sEnd = s + nBufLen;
  while (s < sEnd)
  {
    nHash ^= (ULONG)(*s++);
    nHash += (nHash<<1) + (nHash<<4) + (nHash<<7) + (nHash<<8) + (nHash<<24);
  }
  return nHash;
}
