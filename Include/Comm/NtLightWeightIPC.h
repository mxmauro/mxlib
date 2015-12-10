/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2006-2014. All rights reserved.
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
 *       access to or use of] the software to any third party.
 **/
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
