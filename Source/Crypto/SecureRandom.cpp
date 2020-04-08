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
#include "..\..\Include\Crypto\SecureRandom.h"
#include "..\..\Include\WaitableObjects.h"
#include "..\..\Include\Finalizer.h"
#include "..\Internals\SystemDll.h"
#include <wincrypt.h>
#define _CRT_RAND_S
#include <stdlib.h>

//-----------------------------------------------------------

typedef BOOLEAN (__stdcall *lpfnRtlGenRandom)(_Out_writes_bytes_(RandomBufferLength) PVOID RandomBuffer,
                                              _In_ ULONG RandomBufferLength);

//-----------------------------------------------------------

static lpfnRtlGenRandom fnRtlGenRandom = NULL;

//-----------------------------------------------------------

static BOOL InitializeSecureRandom();

//-----------------------------------------------------------

namespace MX {

namespace SecureRandom {

VOID Generate(_Out_ LPBYTE lpOut, _In_ SIZE_T nSize)
{
  if (lpOut != NULL && nSize > 0)
  {
    if (InitializeSecureRandom() != FALSE)
    {
      while (nSize > 0)
      {
        SIZE_T nThisRoundSize = (nSize > 32768) ? 32768 : nSize;
        if (fnRtlGenRandom(lpOut, (ULONG)nThisRoundSize) == FALSE)
          break;
        lpOut += nThisRoundSize;
        nSize -= nThisRoundSize;
      }
    }
    while (nSize > 0)
    {
      unsigned int v;

      rand_s(&v);
      *lpOut++ = (BYTE)v;
      nSize--;
    }
  }
  return;
}

} //namespace SecureRandom

} //namespace MX

//-----------------------------------------------------------

static BOOL InitializeSecureRandom()
{
  static LONG volatile nMutex = 0;

  if (fnRtlGenRandom == NULL)
  {
    MX::CFastLock cLock(&nMutex);

    if (fnRtlGenRandom == NULL)
    {
      lpfnRtlGenRandom _fnRtlGenRandom = (lpfnRtlGenRandom)1;
      HINSTANCE hAdvApi32Dll;

      //load library
      if (SUCCEEDED(MX::Internals::LoadSystemDll(L"advapi32.dll", &hAdvApi32Dll)))
      {
        _fnRtlGenRandom = (lpfnRtlGenRandom)::GetProcAddress(hAdvApi32Dll, "SystemFunction036");
        if (_fnRtlGenRandom == NULL)
        {
          _fnRtlGenRandom = (lpfnRtlGenRandom)1;
          ::FreeLibrary(hAdvApi32Dll);
        }
      }
    
      fnRtlGenRandom = _fnRtlGenRandom;
    }
  }
  return (fnRtlGenRandom != (lpfnRtlGenRandom)1) ? TRUE : FALSE;
}
