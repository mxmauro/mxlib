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
#ifndef _MX_NTLIGHTWEIGHTIPC_H
#define _MX_NTLIGHTWEIGHTIPC_H

#include "Defines.h"
#include "WaitableObjects.h"
#include "AutoPtr.h"
#include "AutoHandle.h"

//-----------------------------------------------------------

#define NTLIGHTWEIGHTIPC_PACKET_SIZE                    4096
#define NTLIGHTWEIGHTIPC_MESSAGE_SIZE                0x1FFFF

//-----------------------------------------------------------

namespace MX {

class CNtLightWeightIPC : private virtual CBaseMemObj
{
public:
  class CMessage : public virtual CBaseMemObj
  {
  public:
    CMessage();
    ~CMessage();

    VOID Reset();
    BOOL EnsureSize(__in SIZE_T nSize);
    BOOL Add(__in LPCVOID lpData, __in SIZE_T nDataSize);

    operator LPBYTE() const
      {
      return GetData();
      };
    LPBYTE GetData() const
      {
      return lpData;
      };
    SIZE_T GetLength() const
      {
      return nDataLen;
      };

  private:
    LPBYTE lpData;
    SIZE_T nDataLen, nDataSize;
  };

public:
  CNtLightWeightIPC();
  ~CNtLightWeightIPC();

  NTSTATUS ConnectToServer(__in_z LPCWSTR szServerNameW);
  VOID Disconnect();

  NTSTATUS SendMsg(__in LPCVOID lpMsg, __in SIZE_T nMsgSize, __in_opt CMessage *lpReplyMsg=NULL,
                   __in_opt ULONG nTimeout=INFINITE);

private:
  CWindowsHandle cPipe;
  CWindowsEvent cEvent;
  LONG volatile nNextSendId;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_NTLIGHTWEIGHTIPC_H
