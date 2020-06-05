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
#include "..\..\Include\Crypto\EncryptionKey.h"
#include "..\..\Include\Strings\Strings.h"
#include "InitOpenSSL.h"

//-----------------------------------------------------------

namespace MX {

CEncryptionKey::CEncryptionKey() : TRefCounted<CBaseMemObj>()
{
  lpKey = NULL;
  return;
}

CEncryptionKey::CEncryptionKey(_In_ const CEncryptionKey &cSrc) throw(...) : TRefCounted<CBaseMemObj>()
{
  lpKey = NULL;
  operator=(cSrc);
  return;
}

CEncryptionKey::~CEncryptionKey()
{
  if (lpKey != NULL)
  {
    EVP_PKEY_free(lpKey);
    lpKey = NULL;
  }
  return;
}

CEncryptionKey& CEncryptionKey::operator=(_In_ const CEncryptionKey &cSrc) throw(...)
{
  if (this != &cSrc)
  {
    HRESULT hRes;

    hRes = Internals::OpenSSL::Init();
    if (FAILED(hRes))
      throw (LONG)hRes;
    //deref old
    if (lpKey != NULL)
      EVP_PKEY_free(lpKey);
    //increment reference on new
    if (cSrc.lpKey != NULL)
      EVP_PKEY_up_ref(cSrc.lpKey);
    lpKey = cSrc.lpKey;
  }
  return *this;
}

HRESULT CEncryptionKey::Generate(_In_ MX::CEncryptionKey::eAlgorithm nAlgorithm, _In_opt_ SIZE_T nBitsCount)
{
  EVP_PKEY_CTX *lpKeyCtx;
  EVP_PKEY *lpNewKey;
  HRESULT hRes;

  if (nAlgorithm == MX::CEncryptionKey::AlgorithmRSA)
  {
    if (nBitsCount < 512 || nBitsCount > 65536 ||
        (nBitsCount && !(nBitsCount & (nBitsCount - 1))) == 0)
    {
      return E_INVALIDARG;
    }
  }

  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;

  switch (nAlgorithm)
  {
    case MX::CEncryptionKey::AlgorithmRSA:
      lpKeyCtx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
      break;

    case MX::CEncryptionKey::AlgorithmED25519:
      lpKeyCtx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
      break;

    case MX::CEncryptionKey::AlgorithmED448:
      lpKeyCtx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED448, NULL);
      break;

    default:
      return E_INVALIDARG;
  }
  if (lpKeyCtx == NULL)
    return E_OUTOFMEMORY;

  hRes = S_OK;
  if (EVP_PKEY_keygen_init(lpKeyCtx) > 0)
  {
    if (nAlgorithm == MX::CEncryptionKey::AlgorithmRSA)
    {
      if (EVP_PKEY_CTX_set_rsa_keygen_bits(lpKeyCtx, (int)nBitsCount) <= 0)
        hRes = E_INVALIDARG;
    }

    if (SUCCEEDED(hRes))
    {
      if (EVP_PKEY_keygen(lpKeyCtx, &lpNewKey) > 0)
      {
        //replace key
        if (lpKey != NULL)
          EVP_PKEY_free(lpKey);
        lpKey = lpNewKey;
      }
      else
      {
        hRes = E_OUTOFMEMORY;
      }
    }
  }
  else
  {
    hRes = E_OUTOFMEMORY;
  }

  //cleanup
  EVP_PKEY_CTX_free(lpKeyCtx);
  //done
  return hRes;
}

SIZE_T CEncryptionKey::GetBitsCount() const
{
  int size;

  if (lpKey == NULL)
    return 0;
  size = EVP_PKEY_bits(lpKey);
  return (size > 0) ? (SIZE_T)size : 0;
}

BOOL CEncryptionKey::HasPrivateKey() const
{
  if (lpKey == NULL)
    return FALSE;
  return (i2d_PUBKEY(lpKey, NULL) > 0) ? TRUE : FALSE;
}

HRESULT CEncryptionKey::SetPublicKeyFromDER(_In_ LPCVOID _lpKey, _In_ SIZE_T nKeySize)
{
  EVP_PKEY *lpNewKey;
  BIO *bio;
  HRESULT hRes;

  if (_lpKey == NULL)
    return E_POINTER;
  if (nKeySize == 0 || nKeySize > 0x7FFFFFFF)
    return E_INVALIDARG;

  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;

  bio = BIO_new_mem_buf(_lpKey, (int)nKeySize);
  if (bio == NULL)
    return E_OUTOFMEMORY;

  ERR_clear_error();
  lpNewKey = d2i_PUBKEY_bio(bio, NULL);
  if (lpNewKey == NULL)
  {
    hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
    BIO_free(bio);
    return hRes;
  }
  BIO_free(bio);

  //replace key
  if (lpKey != NULL)
    EVP_PKEY_free(lpKey);
  lpKey = lpNewKey;

  //done
  return S_OK;
}

HRESULT CEncryptionKey::SetPublicKeyFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA,
                                            _In_opt_ SIZE_T nPemLen)
{
  EVP_PKEY *lpNewKey;
  BIO *bio;
  HRESULT hRes;

  if (szPemA == NULL)
    return E_POINTER;
  if (nPemLen == (SIZE_T)-1)
    nPemLen = StrLenA(szPemA);
  if (nPemLen == 0 || nPemLen > 0x7FFFFFFF)
    return E_INVALIDARG;

  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;

  bio = BIO_new_mem_buf(szPemA, (int)nPemLen);
  if (bio == NULL)
    return E_OUTOFMEMORY;

  lpNewKey = PEM_read_bio_PUBKEY(bio, NULL, 0, (void *)szPasswordA);
  if (lpNewKey == NULL)
  {
    hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
    BIO_free(bio);
    return hRes;
  }
  BIO_free(bio);

  //replace key
  if (lpKey != NULL)
    EVP_PKEY_free(lpKey);
  lpKey = lpNewKey;

  //done
  return S_OK;
}

HRESULT CEncryptionKey::GetPublicKey(_Out_ CSecureBuffer **lplpBuffer)
{
  TAutoRefCounted<CSecureBuffer> cBuffer;
  BIO *bio;
  HRESULT hRes;

  if (lplpBuffer == NULL)
    return E_POINTER;
  *lplpBuffer = NULL;

  if (lpKey == NULL)
    return MX_E_NotReady;

  cBuffer.Attach(MX_DEBUG_NEW CSecureBuffer());
  if (!cBuffer)
    return E_OUTOFMEMORY;
  bio = cBuffer->CreateBIO();
  if (bio == NULL)
    return E_OUTOFMEMORY;

  ERR_clear_error();
  if (i2d_PrivateKey_bio(bio, lpKey) <= 0)
  {
    hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
    BIO_free(bio);
    return hRes;
  }
  BIO_free(bio);

  //done
  *lplpBuffer = cBuffer.Detach();
  return S_OK;
}

HRESULT CEncryptionKey::SetPrivateKeyFromDER(_In_ LPCVOID _lpKey, _In_ SIZE_T nKeySize)
{
  EVP_PKEY *lpNewKey;
  BIO *bio;
  HRESULT hRes;

  if (_lpKey == NULL)
    return E_POINTER;
  if (nKeySize == 0 || nKeySize > 0x7FFFFFFF)
    return E_INVALIDARG;

  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;

  bio = BIO_new_mem_buf(_lpKey, (int)nKeySize);
  if (bio == NULL)
    return E_OUTOFMEMORY;

  ERR_clear_error();
  lpNewKey = d2i_PrivateKey_bio(bio, NULL);
  if (lpNewKey == NULL)
  {
    hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
    BIO_free(bio);
    return hRes;
  }
  BIO_free(bio);

  //replace key
  if (lpKey != NULL)
    EVP_PKEY_free(lpKey);
  lpKey = lpNewKey;

  //done
  return S_OK;
}

HRESULT CEncryptionKey::SetPrivateKeyFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA,
                                             _In_opt_ SIZE_T nPemLen)
{
  EVP_PKEY *lpNewKey;
  BIO *bio;
  HRESULT hRes;

  if (szPemA == NULL)
    return E_POINTER;
  if (nPemLen == (SIZE_T)-1)
    nPemLen = StrLenA(szPemA);
  if (nPemLen == 0 || nPemLen > 0x7FFFFFFF)
    return E_INVALIDARG;

  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;

  bio = BIO_new_mem_buf(szPemA, (int)nPemLen);
  if (bio == NULL)
    return E_OUTOFMEMORY;

  lpNewKey = PEM_read_bio_PrivateKey(bio, NULL, 0, (void *)szPasswordA);
  if (lpNewKey == NULL)
  {
    hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
    BIO_free(bio);
    return hRes;
  }
  BIO_free(bio);

  //replace key
  if (lpKey != NULL)
    EVP_PKEY_free(lpKey);
  lpKey = lpNewKey;

  //done
  return S_OK;
}

HRESULT CEncryptionKey::GetPrivateKey(_Out_ CSecureBuffer **lplpBuffer)
{
  TAutoRefCounted<CSecureBuffer> cBuffer;
  BIO *bio;
  HRESULT hRes;

  if (lplpBuffer == NULL)
    return E_POINTER;
  *lplpBuffer = NULL;

  if (lpKey == NULL)
    return MX_E_NotReady;

  cBuffer.Attach(MX_DEBUG_NEW CSecureBuffer());
  if (!cBuffer)
    return E_OUTOFMEMORY;
  bio = cBuffer->CreateBIO();
  if (bio == NULL)
    return E_OUTOFMEMORY;

  ERR_clear_error();
  if (i2d_PrivateKey_bio(bio, lpKey) <= 0)
  {
    hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
    BIO_free(bio);
    return hRes;
  }
  BIO_free(bio);

  //done
  *lplpBuffer = cBuffer.Detach();
  return S_OK;
}

} //namespace MX
