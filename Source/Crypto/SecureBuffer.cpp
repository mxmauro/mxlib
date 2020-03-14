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
#include <intrin.h>
static FORCEINLINE int InterlockedExchangeAdd(_Inout_ _Interlocked_operand_ int volatile *Addend, _In_ int Value)
{
  return (int)_InterlockedExchangeAdd((LONG volatile *)Addend, (LONG)Value);
}
#include "..\OpenSSL\Source\include\internal\bio.h"
#include "..\OpenSSL\Source\crypto\x509\x509_local.h"

//-----------------------------------------------------------

static BIO_METHOD *BIO_secure_buffer_mem();

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

  bio = BIO_new(BIO_secure_buffer_mem());
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

//-----------------------------------------------------------

static int secure_buffer_create(BIO *bi)
{
  BIO_set_init(bi, 1);
  BIO_set_shutdown(bi, 1);
  return 1;
}

static int secure_buffer_destroy(BIO *bi)
{
  MX::CSecureBuffer *lpBuf;

  if (bi == NULL)
    return 0;
  lpBuf = (MX::CSecureBuffer*)BIO_get_data(bi);
  if (lpBuf != NULL && BIO_get_shutdown(bi) != 0 && BIO_get_init(bi) != 0)
  {
    lpBuf->Release();
    BIO_set_data(bi, NULL);
  }
  return 1;
}

static int secure_buffer_read_old(BIO *bi, char *out, int outl)
{
  MX::CSecureBuffer *lpBuf = (MX::CSecureBuffer *)BIO_get_data(bi);
  SIZE_T nAvailable;
  int ret;

  BIO_clear_retry_flags(bi);
  nAvailable = lpBuf->GetLength();
  ret = (outl > 0 && (SIZE_T)outl > nAvailable) ? (int)nAvailable : outl;
  if (out != NULL && ret > 0)
  {
    ret = (int)(lpBuf->Read(out, (SIZE_T)ret));
  }
  else if (nAvailable == 0)
  {
    ret = -1;
    BIO_set_retry_read(bi);
  }
  return ret;
}

static int secure_buffer_read(BIO *bio, char *data, size_t datal, size_t *readbytes)
{
  int ret;

  if (datal > INT_MAX)
    datal = INT_MAX;
  ret = secure_buffer_read_old(bio, data, (int)datal);
  if (ret <= 0)
  {
    *readbytes = 0;
    return ret;
  }
  *readbytes = (size_t)ret;
  return 1;
}

static int secure_buffer_write_old(BIO *bi, const char *in, int inl)
{
  MX::CSecureBuffer *lpBuf;

  if (in == NULL)
  {
    if (inl == 0)
      return 0;
    BIOerr(BIO_F_MEM_WRITE, BIO_R_NULL_PARAMETER);
    return -1;
  }
  if (BIO_test_flags(bi, BIO_FLAGS_MEM_RDONLY))
  {
    BIOerr(BIO_F_MEM_WRITE, BIO_R_WRITE_TO_READ_ONLY_BIO);
    return -1;
  }

  BIO_clear_retry_flags(bi);
  lpBuf = (MX::CSecureBuffer*)BIO_get_data(bi);
  if (inl > 0)
  {
    if (FAILED(lpBuf->WriteStream(in, (SIZE_T)inl)))
    {
      BIOerr(BIO_F_MEM_WRITE, ERR_R_MALLOC_FAILURE);
      return -1;
    }
  }
  return inl;
}

static int secure_buffer_write(BIO *bio, const char *data, size_t datal, size_t *written)
{
  int ret;

  if (datal > INT_MAX)
    datal = INT_MAX;
  ret = secure_buffer_write_old(bio, data, (int)datal);
  if (ret <= 0)
  {
    *written = 0;
    return ret;
  }
  *written = (size_t)ret;
  return 1;
}

static long secure_buffer_ctrl(BIO *bi, int cmd, long num, void *ptr)
{
  MX::CSecureBuffer *lpBuf = (MX::CSecureBuffer*)BIO_get_data(bi);

  switch (cmd)
  {
    case BIO_CTRL_RESET:
      lpBuf->Reset();
      return 1;

    case BIO_CTRL_GET_CLOSE:
      return (long)BIO_get_shutdown(bi);

    case BIO_CTRL_SET_CLOSE:
      BIO_set_shutdown(bi, (int)num);
      return 1;

    case BIO_CTRL_PENDING:
      {
      SIZE_T nAvail = lpBuf->GetLength();

      if (nAvail > (SIZE_T)LONG_MAX)
        nAvail = (SIZE_T)LONG_MAX;
      return (long)nAvail;
      }
      break;

    case BIO_CTRL_DUP:
    case BIO_CTRL_FLUSH:
      return 1;
  }
  return 0;
}

static int secure_buffer_gets(BIO *bi, char *buf, int size)
{
  if (buf == NULL)
  {
    BIOerr(BIO_F_BIO_READ, BIO_R_NULL_PARAMETER);
    return -1;
  }
  BIO_clear_retry_flags(bi);
  if (size > 0)
  {
    MX::CSecureBuffer *lpBuf = (MX::CSecureBuffer *)BIO_get_data(bi);
    SIZE_T i, nToRead, nReaded;

    nToRead = (SIZE_T)size;
    nReaded = lpBuf->Peek(buf, nToRead);
    if (nReaded == 0)
    {
      *buf = 0;
      return 0;
    }
    for (i = 0; i < nReaded; i++)
    {
      if (buf[i] == '\n')
      {
        i++;
        break;
      }
    }
    lpBuf->Skip(i);
    buf[i] = 0;
    return (int)i;
  }
  if (buf != NULL)
    *buf = '\0';
  return 0;
}

static int secure_buffer_puts(BIO *bi, const char *str)
{
  return secure_buffer_write_old(bi, str, (int)MX::StrLenA(str));
}

static BIO_METHOD *BIO_secure_buffer_mem()
{
  static const BIO_METHOD secure_buffer_mem = {
    BIO_TYPE_MEM,
    (char *)"secure memory buffer",
    &secure_buffer_write,
    &secure_buffer_write_old,
    &secure_buffer_read,
    &secure_buffer_read_old,
    &secure_buffer_puts,
    &secure_buffer_gets,
    &secure_buffer_ctrl,
    &secure_buffer_create,
    &secure_buffer_destroy,
    NULL
  };
  return (BIO_METHOD *)&secure_buffer_mem;
}
