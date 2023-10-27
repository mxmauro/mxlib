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
#include "..\..\Include\Crypto\SecureBuffer.h"
#include "..\..\Include\Strings\Strings.h"
#include "InitOpenSSL.h"
#include <OpenSSL\bio.h>
#include <intrin.h>
#include "SecureBuffer_BIO.h"

//-----------------------------------------------------------

namespace MX {

CSecureBuffer::CSecureBuffer() : TRefCounted<CBaseMemObj>(), CNonCopyableObj()
{
  lpBuffer = NULL;
  nLength = nSize = 0;
  return;
}

CSecureBuffer::~CSecureBuffer()
{
  Reset();
  return;
}

VOID CSecureBuffer::Reset()
{
  if (lpBuffer != NULL)
  {
    ::MxMemSet(lpBuffer, 0, nSize);
    MxMemFree(lpBuffer);
    lpBuffer = NULL;
  }
  nLength = nSize = 0;
  return;
}

SIZE_T CSecureBuffer::Read(_Out_writes_(nDestSize) LPVOID lpDest, _In_ SIZE_T nDestSize)
{
  nDestSize = Peek(lpDest, nDestSize);
  Skip(nDestSize);
  return nDestSize;
}

SIZE_T CSecureBuffer::Peek(_Out_writes_(nDestSize) LPVOID lpDest, _In_ SIZE_T nDestSize)
{
  if (lpBuffer == NULL)
    return 0;
  //crop size
  if (nDestSize > nLength)
    nDestSize = nLength;
  //copy data
  ::MxMemCopy(lpDest, lpBuffer, nDestSize);
  //done
  return nDestSize;
}

VOID CSecureBuffer::Skip(_In_ SIZE_T nSize)
{
  //crop size
  if (nSize > nLength)
    nSize = nLength;
  //move the rest of the buffer to the beginning
  nLength -= nSize;
  ::MxMemMove(lpBuffer, lpBuffer + nSize, nLength);
  //zero the rest
  ::MxMemSet(lpBuffer + nLength, 0, nSize);
  //done
  return;
}

HRESULT CSecureBuffer::WriteStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)
{
  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (EnsureWritableSpace(nDataLength) == FALSE)
    return E_OUTOFMEMORY;
  //copy data
  ::MxMemCopy(lpBuffer + nLength, lpData, nDataLength);
  nLength += nDataLength;
  //done
  return S_OK;
}

HRESULT CSecureBuffer::WriteWordLE(_In_ LPWORD lpnValues, _In_ SIZE_T nCount)
{
  return WriteStream(lpnValues, nCount * sizeof(WORD));
}

HRESULT CSecureBuffer::WriteWordBE(_In_ LPWORD lpnValues, _In_ SIZE_T nCount)
{
  WORD aTempValues[32];
  SIZE_T i;
  HRESULT hRes;

  if (lpnValues == NULL && nCount > 0)
    return E_POINTER;
  hRes = S_OK;
  while (SUCCEEDED(hRes) && nCount > 0)
  {
    for (i = 0; nCount > 0 && i < MX_ARRAYLEN(aTempValues); i++, nCount--, lpnValues++)
    {
      aTempValues[i] = (((*lpnValues) & 0xFF00) >> 8) |
        (((*lpnValues) & 0x00FF) << 8);
    }
    hRes = WriteStream(aTempValues, i * sizeof(WORD));
  }
  return hRes;
}

HRESULT CSecureBuffer::WriteDWordLE(_In_ LPDWORD lpnValues, _In_ SIZE_T nCount)
{
  return WriteStream(lpnValues, nCount * sizeof(DWORD));
}

HRESULT CSecureBuffer::WriteDWordBE(_In_ LPDWORD lpnValues, _In_ SIZE_T nCount)
{
  DWORD aTempValues[32];
  SIZE_T i;
  HRESULT hRes;

  if (lpnValues == NULL && nCount > 0)
    return E_POINTER;
  hRes = S_OK;
  while (SUCCEEDED(hRes) && nCount > 0)
  {
    for (i = 0; nCount > 0 && i < MX_ARRAYLEN(aTempValues); i++, nCount--, lpnValues++)
    {
      aTempValues[i] = (((*lpnValues) & 0xFF000000) >> 24) |
                       (((*lpnValues) & 0x00FF0000) >>  8) |
                       (((*lpnValues) & 0x0000FF00) <<  8) |
                       (((*lpnValues) & 0x000000FF) << 24);
    }
    hRes = WriteStream(aTempValues, i * sizeof(DWORD));
  }
  return hRes;
}

HRESULT CSecureBuffer::WriteQWordLE(_In_ ULONGLONG *lpnValues, _In_ SIZE_T nCount)
{
  return WriteStream(lpnValues, nCount * sizeof(ULONGLONG));
}

HRESULT CSecureBuffer::WriteQWordBE(_In_ ULONGLONG *lpnValues, _In_ SIZE_T nCount)
{
  ULONGLONG aTempValues[32];
  SIZE_T i;
  HRESULT hRes;

  if (lpnValues == NULL && nCount > 0)
    return E_POINTER;
  hRes = S_OK;
  while (SUCCEEDED(hRes) && nCount > 0)
  {
    for (i = 0; nCount > 0 && i < MX_ARRAYLEN(aTempValues); i++, nCount--, lpnValues++)
    {
      aTempValues[i] = (((*lpnValues) & 0xFF00000000000000ui64) >> 56) |
                       (((*lpnValues) & 0x00FF000000000000ui64) >> 40) |
                       (((*lpnValues) & 0x0000FF0000000000ui64) >> 24) |
                       (((*lpnValues) & 0x000000FF00000000ui64) >>  8) |
                       (((*lpnValues) & 0x00000000FF000000ui64) <<  8) |
                       (((*lpnValues) & 0x0000000000FF0000ui64) << 24) |
                       (((*lpnValues) & 0x000000000000FF00ui64) << 40) |
                       (((*lpnValues) & 0x00000000000000FFui64) << 56);
    }
    hRes = WriteStream(aTempValues, i * sizeof(ULONGLONG));
  }
  return hRes;
}

LPBYTE CSecureBuffer::WriteReserve(_In_ SIZE_T nDataLength)
{
  return (EnsureWritableSpace(nDataLength) != FALSE) ? (lpBuffer + nLength) : NULL;
}

VOID CSecureBuffer::EndWriteReserve(_In_ SIZE_T nDataLength)
{
  MX_ASSERT(nLength + nDataLength >= nLength && nLength + nDataLength <= nSize);
  nLength += nDataLength;
  return;
}

BIO* CSecureBuffer::CreateBIO()
{
  BIO *bio;

  bio = BIO_new(MX::Internals::OpenSSL::BIO_secure_buffer_mem());
  if (bio != NULL)
  {
    BIO_set_data(bio, this);
    AddRef();
  }
  return bio;
}

BOOL CSecureBuffer::EnsureWritableSpace(_In_ SIZE_T nBytes)
{
  SIZE_T nAllocSize;

  if (lpBuffer == NULL)
  {
    nAllocSize = (nBytes + 4095) & (~4095);
    lpBuffer = (LPBYTE)MX_MALLOC(nAllocSize);
    if (lpBuffer == NULL)
      return FALSE;
    nSize = nAllocSize;
  }
  else if (nBytes > nSize - nLength)
  {
    LPBYTE lpNewBuffer;

    nAllocSize = (nBytes + nLength + 4095) & (~4095);
    lpNewBuffer = (LPBYTE)MX_MALLOC(nAllocSize);
    if (lpNewBuffer == NULL)
      return FALSE;
    //free old buffer
    ::MxMemCopy(lpNewBuffer, lpBuffer, nLength);
    ::MxMemSet(lpBuffer, 0, nLength);
    ::MxMemFree(lpBuffer);
    //update vars
    lpBuffer = lpNewBuffer;
    nSize = nAllocSize;
  }
  return TRUE;
}

} //namespace MX
