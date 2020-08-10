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
#ifndef _IPC_DEFS_H
#define _IPC_DEFS_H

#include "..\..\Include\Comm\IpcCommon.h"
#include "..\..\Include\ArrayList.h"
#include "..\..\Include\StackTrace.h"
#include "..\Crypto\InitOpenSSL.h"
#include "..\..\Include\CircularBuffer.h"
#include "..\..\Include\Comm\SslCertificates.h"
#include <intrin.h>
#pragma intrinsic(_InterlockedExchangeAdd)
static FORCEINLINE int InterlockedExchangeAdd(_Inout_ _Interlocked_operand_ int volatile *Addend, _In_ int Value)
{
  return (int)_InterlockedExchangeAdd((LONG volatile *)Addend, (LONG)Value);
}
#include "..\OpenSSL\Source\include\internal\bio.h"
#include "..\OpenSSL\Source\crypto\x509\x509_local.h"

//-----------------------------------------------------------

#define CHECKTAG1                               0x6F7E283CUL
#define CHECKTAG2                               0x3A9B4D8EUL

#define FLAG_Connected                                0x0001
#define FLAG_Closed                                   0x0002
#define FLAG_ShutdownProcessed                        0x0004
#define FLAG_NewReceivedDataAvailable                 0x0008
#define FLAG_GracefulShutdown                         0x0010
#define FLAG_InSendTransaction                        0x0020
#define FLAG_ClosingOnShutdown                        0x0040
#define FLAG_InitialSetupExecuted                     0x0080
#define FLAG_InputProcessingPaused                    0x0100
#define FLAG_OutputProcessingPaused                   0x0200
#define FLAG_HasSSL                                   0x0400
#define FLAG_SslHandshakeCompleted                    0x0800
#define FLAG_SslWantRead                              0x1000

//-----------------------------------------------------------

#endif //_IPC_DEFS_H
