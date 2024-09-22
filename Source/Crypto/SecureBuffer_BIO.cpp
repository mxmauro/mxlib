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
#include "SecureBuffer_BIO.h"
#include <openssl\bio.h>
#include "..\..\Include\Strings\Strings.h"
#include "..\..\Include\Crypto\SecureBuffer.h"

//-------------------------------------------------------

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
  lpBuf = (MX::CSecureBuffer *)BIO_get_data(bi);
  if (lpBuf != NULL && BIO_get_shutdown(bi) != 0 && BIO_get_init(bi) != 0)
  {
    lpBuf->Release();
    BIO_set_data(bi, NULL);
  }
  return 1;
}

static int secure_buffer_read(BIO *bi, char *out, int outl)
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

static int secure_buffer_read_ex(BIO *bio, char *data, size_t datal, size_t *readbytes)
{
  int ret;

  if (datal > INT_MAX)
    datal = INT_MAX;
  ret = secure_buffer_read(bio, data, (int)datal);
  if (ret <= 0)
  {
    *readbytes = 0;
    return ret;
  }
  *readbytes = (size_t)ret;
  return 1;
}

static int secure_buffer_write(BIO *bi, const char *in, int inl)
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
  lpBuf = (MX::CSecureBuffer *)BIO_get_data(bi);
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

static int secure_buffer_write_ex(BIO *bio, const char *data, size_t datal, size_t *written)
{
  int ret;

  if (datal > INT_MAX)
    datal = INT_MAX;
  ret = secure_buffer_write(bio, data, (int)datal);
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
  MX::CSecureBuffer *lpBuf = (MX::CSecureBuffer *)BIO_get_data(bi);

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
  return secure_buffer_write(bi, str, (int)MX::StrLenA(str));
}

//-------------------------------------------------------

static BIO_METHOD *lpSecureBufferMem = NULL;

//-------------------------------------------------------

namespace MX {

namespace Internals {

namespace OpenSSL {

HRESULT InitializeSecureBufferBIO()
{
  lpSecureBufferMem = BIO_meth_new(BIO_TYPE_MEM, "secure memory buffer");
  if (lpSecureBufferMem == NULL)
    return E_OUTOFMEMORY;

  BIO_meth_set_write_ex(lpSecureBufferMem, secure_buffer_write_ex);
  BIO_meth_set_write(lpSecureBufferMem, secure_buffer_write);
  BIO_meth_set_read_ex(lpSecureBufferMem, secure_buffer_read_ex);
  BIO_meth_set_read(lpSecureBufferMem, secure_buffer_read);
  BIO_meth_set_puts(lpSecureBufferMem, secure_buffer_puts);
  BIO_meth_set_gets(lpSecureBufferMem, secure_buffer_gets);
  BIO_meth_set_ctrl(lpSecureBufferMem, secure_buffer_ctrl);
  BIO_meth_set_create(lpSecureBufferMem, secure_buffer_create);
  BIO_meth_set_destroy(lpSecureBufferMem, secure_buffer_destroy);

  return S_OK;
}

VOID FinalizeSecureBufferBIO()
{
  if (lpSecureBufferMem != NULL)
  {
    BIO_meth_free(lpSecureBufferMem);
    lpSecureBufferMem = NULL;
  }
  return;
}

BIO_METHOD* BIO_secure_buffer_mem()
{
  return lpSecureBufferMem;
}

} //namespace OpenSSL

} //namespace Internals

} //namespace MX
