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
#ifndef _MX_SECURE_BUFFER_H
#define _MX_SECURE_BUFFER_H

#include "..\Defines.h"
#include "..\RefCounted.h"
typedef struct bio_st BIO;

//-----------------------------------------------------------

namespace MX {

class CSecureBuffer : public virtual TRefCounted<CBaseMemObj>, public CNonCopyableObj
{
public:
  CSecureBuffer();
  ~CSecureBuffer();

  VOID Reset();

  LPBYTE GetBuffer() const
    {
    return lpBuffer;
    };
  SIZE_T GetLength() const
    {
    return nLength;
    };

  SIZE_T Read(_Out_writes_(nDestSize) LPVOID lpDest, _In_ SIZE_T nDestSize);
  SIZE_T Peek(_Out_writes_(nDestSize) LPVOID lpDest, _In_ SIZE_T nDestSize);
  VOID Skip(_In_ SIZE_T nSize);

  HRESULT WriteStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength);
  HRESULT WriteWordLE(_In_ LPWORD lpnValues, _In_ SIZE_T nCount);
  HRESULT WriteWordBE(_In_ LPWORD lpnValues, _In_ SIZE_T nCount);
  HRESULT WriteDWordLE(_In_ LPDWORD lpnValues, _In_ SIZE_T nCount);
  HRESULT WriteDWordBE(_In_ LPDWORD lpnValues, _In_ SIZE_T nCount);
  HRESULT WriteQWordLE(_In_ ULONGLONG *lpnValues, _In_ SIZE_T nCount);
  HRESULT WriteQWordBE(_In_ ULONGLONG *lpnValues, _In_ SIZE_T nCount);

  LPBYTE WriteReserve(_In_ SIZE_T nDataLength);
  VOID EndWriteReserve(_In_ SIZE_T nDataLength);

  BIO* CreateBIO();

private:
  BOOL EnsureWritableSpace(_In_ SIZE_T nBytes);

private:
  LPBYTE lpBuffer;
  SIZE_T nLength, nSize;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_SECURE_BUFFER_H
