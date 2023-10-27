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
#include "..\..\Include\Crypto\SymmetricCipher.h"
#include "..\..\Include\Crypto\SecureBuffer.h"
#include "InitOpenSSL.h"
#include <OpenSSL\evp.h>

//-----------------------------------------------------------

#define BLOCK_SIZE                                      2048

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CSymmetricCipherEncoderDecoder : public virtual CBaseMemObj
{
public:
  CSymmetricCipherEncoderDecoder() : CBaseMemObj()
    {
    lpCipherCtx = NULL;
    cOutputBuffer.Attach(MX_DEBUG_NEW CSecureBuffer());
    return;
    };

  ~CSymmetricCipherEncoderDecoder()
    {
    Reset(TRUE);
    return;
    };

  VOID Reset(_In_ BOOL bCleanOutputBuffer)
    {
    if (lpCipherCtx != NULL)
    {
      EVP_CIPHER_CTX_free(lpCipherCtx);
      lpCipherCtx = NULL;
    }
    if (bCleanOutputBuffer != NULL)
    {
      CleanOutputBuffers();
    }
    return;
    };

  VOID CleanOutputBuffers()
    {
    cOutputBuffer->Reset();
    return;
    };

public:
  EVP_CIPHER_CTX *lpCipherCtx;
  TAutoRefCounted<CSecureBuffer> cOutputBuffer;
};

class CSymmetricCipherData : public virtual CBaseMemObj
{
public:
  CSymmetricCipherData() : CBaseMemObj()
    {
    lpCipher = NULL;
    return;
    };

  BOOL InUse() const
    {
    return (cEncryptor.lpCipherCtx != NULL && cDecryptor.lpCipherCtx != NULL) ? TRUE : FALSE;
    };

public:
  EVP_CIPHER *lpCipher;
  CSymmetricCipherEncoderDecoder cEncryptor;
  CSymmetricCipherEncoderDecoder cDecryptor;
};

} //namespace Internals

} //namespace MX

#define symcipher_data ((MX::Internals::CSymmetricCipherData*)lpInternalData)

//-----------------------------------------------------------

static const EVP_CIPHER* GetCipher(_In_ MX::CSymmetricCipher::eAlgorithm nAlgorithm);

//-----------------------------------------------------------

namespace MX {

CSymmetricCipher::CSymmetricCipher() : CBaseMemObj(), CNonCopyableObj()
{
  lpInternalData = NULL;
  return;
}

CSymmetricCipher::~CSymmetricCipher()
{
  if (lpInternalData != NULL)
  {
    if (symcipher_data->lpCipher != NULL)
      EVP_CIPHER_free(symcipher_data->lpCipher);
    delete symcipher_data;
    lpInternalData = NULL;
  }
  return;
}

HRESULT CSymmetricCipher::SetAlgorithm(_In_ MX::CSymmetricCipher::eAlgorithm nAlgorithm)
{
  EVP_CIPHER *lpNewCipher;
  HRESULT hRes;

  hRes = InternalInitialize();
  if (FAILED(hRes))
    return hRes;
  if (symcipher_data->InUse() != FALSE)
    return MX_E_Busy;

  lpNewCipher = (EVP_CIPHER*)GetCipher(nAlgorithm);
  if (lpNewCipher == NULL)
    return MX::Internals::OpenSSL::GetLastErrorCode(E_NOTIMPL);

  //change
  if (symcipher_data->lpCipher != NULL)
    EVP_CIPHER_free(symcipher_data->lpCipher);
  symcipher_data->lpCipher = lpNewCipher;

  //done
  return S_OK;
}

HRESULT CSymmetricCipher::BeginEncrypt(_In_opt_ BOOL bUsePadding, _In_opt_ LPCVOID lpKey, _In_opt_ SIZE_T nKeyLen,
                                       _In_opt_ LPCVOID lpInitVector)
{
  HRESULT hRes;

  hRes = InternalInitialize();
  if (FAILED(hRes))
    return hRes;
  if (symcipher_data->lpCipher == NULL)
    return MX_E_NotReady;

  //create context
  symcipher_data->cEncryptor.Reset(TRUE);
  symcipher_data->cEncryptor.lpCipherCtx = EVP_CIPHER_CTX_new();
  if (symcipher_data->cEncryptor.lpCipherCtx == NULL)
    return E_OUTOFMEMORY;

  //verify key length
  if (EVP_CIPHER_get_key_length(symcipher_data->lpCipher) > 0)
  {
    if (lpKey == NULL)
    {
      symcipher_data->cEncryptor.Reset(TRUE);
      return E_POINTER;
    }
    if (nKeyLen != (SIZE_T)EVP_CIPHER_get_key_length(symcipher_data->lpCipher))
    {
      symcipher_data->cEncryptor.Reset(TRUE);
      return E_INVALIDARG;
    }
  }

  //verify if IV is present
  if (EVP_CIPHER_get_iv_length(symcipher_data->lpCipher) > 0)
  {
    if (lpInitVector == NULL)
    {
      symcipher_data->cEncryptor.Reset(TRUE);
      return E_POINTER;
    }
  }

  //initialize
  ERR_clear_error();
  if (EVP_EncryptInit(symcipher_data->cEncryptor.lpCipherCtx, symcipher_data->lpCipher,
                      (const unsigned char*)lpKey, (const unsigned char*)lpInitVector) <= 0)
  {
    hRes = Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
    symcipher_data->cEncryptor.Reset(TRUE);
    return hRes;
  }
  EVP_CIPHER_CTX_set_padding(symcipher_data->cEncryptor.lpCipherCtx, (bUsePadding != FALSE) ? 1 : 0);

  //empty buffer
  symcipher_data->cEncryptor.CleanOutputBuffers();

  //done
  return S_OK;
}

HRESULT CSymmetricCipher::EncryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)
{
  LPBYTE lpIn, lpOut;
  int nInSize, nOutSize;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (lpInternalData == NULL || symcipher_data->cEncryptor.lpCipherCtx == NULL)
    return MX_E_NotReady;

  lpIn = (LPBYTE)lpData;
  while (nDataLength > 0)
  {
    nInSize = (nDataLength > BLOCK_SIZE) ? BLOCK_SIZE : (int)nDataLength;

    lpOut = symcipher_data->cEncryptor.cOutputBuffer->WriteReserve(EVP_MAX_BLOCK_LENGTH + BLOCK_SIZE);
    if (lpOut == NULL)
    {
      symcipher_data->cEncryptor.Reset(TRUE);
      return E_OUTOFMEMORY;
    }

    if (EVP_EncryptUpdate(symcipher_data->cEncryptor.lpCipherCtx, lpOut, &nOutSize, lpIn, nInSize) <= 0)
    {
      symcipher_data->cEncryptor.Reset(TRUE);
      return MX_E_InvalidData;
    }

    lpIn += nInSize;
    nDataLength -= nInSize;

    symcipher_data->cEncryptor.cOutputBuffer->EndWriteReserve((SIZE_T)nOutSize);
  }

  //done
  return S_OK;
}

HRESULT CSymmetricCipher::EndEncrypt()
{
  LPBYTE lpOut;
  int nOutSize;

  if (lpInternalData == NULL || symcipher_data->cEncryptor.lpCipherCtx == NULL)
    return MX_E_NotReady;

  lpOut = symcipher_data->cEncryptor.cOutputBuffer->WriteReserve(EVP_MAX_BLOCK_LENGTH + BLOCK_SIZE);
  if (lpOut == NULL)
  {
    symcipher_data->cEncryptor.Reset(TRUE);
    return E_OUTOFMEMORY;
  }

  if (EVP_EncryptFinal_ex(symcipher_data->cEncryptor.lpCipherCtx, lpOut, &nOutSize) <= 0)
  {
    symcipher_data->cEncryptor.Reset(TRUE);
    return MX_E_InvalidData;
  }

  symcipher_data->cEncryptor.cOutputBuffer->EndWriteReserve((SIZE_T)nOutSize);
  symcipher_data->cEncryptor.Reset(FALSE);

  //done
  return S_OK;
}

SIZE_T CSymmetricCipher::GetAvailableEncryptedData() const
{
  return (lpInternalData != NULL) ? symcipher_data->cEncryptor.cOutputBuffer->GetLength() : 0;
}

SIZE_T CSymmetricCipher::GetEncryptedData(_Out_writes_(nDestSize) LPVOID lpDest, _In_ SIZE_T nDestSize)
{
  if (lpInternalData == NULL)
    return 0;
  return symcipher_data->cEncryptor.cOutputBuffer->Read(lpDest, nDestSize);
}

HRESULT CSymmetricCipher::BeginDecrypt(_In_opt_ BOOL bUsePadding, _In_opt_ LPCVOID lpKey, _In_opt_ SIZE_T nKeyLen,
                                       _In_opt_ LPCVOID lpInitVector)
{
  HRESULT hRes;

  hRes = InternalInitialize();
  if (FAILED(hRes))
    return hRes;
  symcipher_data->cDecryptor.Reset(TRUE);

  //create context
  symcipher_data->cDecryptor.lpCipherCtx = EVP_CIPHER_CTX_new();
  if (symcipher_data->cDecryptor.lpCipherCtx == NULL)
  {
    symcipher_data->cDecryptor.Reset(TRUE);
    return E_OUTOFMEMORY;
  }

  //verify key length
  if (EVP_CIPHER_get_key_length(symcipher_data->lpCipher) > 0)
  {
    if (lpKey == NULL)
    {
      symcipher_data->cDecryptor.Reset(TRUE);
      return E_POINTER;
    }
    if (nKeyLen != (SIZE_T)EVP_CIPHER_get_key_length(symcipher_data->lpCipher))
    {
      symcipher_data->cDecryptor.Reset(TRUE);
      return E_INVALIDARG;
    }
  }

  //verify if IV is present
  if (EVP_CIPHER_get_iv_length(symcipher_data->lpCipher) > 0)
  {
    if (lpInitVector == NULL)
    {
      symcipher_data->cDecryptor.Reset(TRUE);
      return E_POINTER;
    }
  }

  //initialize
  ERR_clear_error();
  if (EVP_DecryptInit(symcipher_data->cDecryptor.lpCipherCtx, symcipher_data->lpCipher,
                      (const unsigned char*)lpKey, (const unsigned char*)lpInitVector) <= 0)
  {
    hRes = Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
    symcipher_data->cDecryptor.Reset(TRUE);
    return hRes;
  }
  EVP_CIPHER_CTX_set_padding(symcipher_data->cDecryptor.lpCipherCtx, (bUsePadding != FALSE) ? 1 : 0);

  //empty buffer
  symcipher_data->cDecryptor.CleanOutputBuffers();

  //done
  return S_OK;
}

HRESULT CSymmetricCipher::DecryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)
{
  LPBYTE lpIn, lpOut;
  int nInSize, nOutSize;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (lpInternalData == NULL || symcipher_data->cDecryptor.lpCipherCtx == NULL)
    return MX_E_NotReady;

  lpIn = (LPBYTE)lpData;
  while (nDataLength > 0)
  {
    nInSize = (nDataLength > BLOCK_SIZE) ? BLOCK_SIZE : (int)nDataLength;

    lpOut = symcipher_data->cDecryptor.cOutputBuffer->WriteReserve(EVP_MAX_BLOCK_LENGTH + BLOCK_SIZE);
    if (lpOut == NULL)
    {
      symcipher_data->cDecryptor.Reset(TRUE);
      return E_OUTOFMEMORY;
    }

    if (EVP_DecryptUpdate(symcipher_data->cDecryptor.lpCipherCtx, lpOut, &nOutSize, lpIn, nInSize) <= 0)
    {
      symcipher_data->cDecryptor.Reset(TRUE);
      return MX_E_InvalidData;
    }

    lpIn += nInSize;
    nDataLength -= nInSize;

    symcipher_data->cDecryptor.cOutputBuffer->EndWriteReserve((SIZE_T)nOutSize);
  }

  //done
  return S_OK;
}

HRESULT CSymmetricCipher::EndDecrypt()
{
  LPBYTE lpOut;
  int nOutSize;

  if (lpInternalData == NULL || symcipher_data->cDecryptor.lpCipherCtx == NULL)
    return MX_E_NotReady;

  lpOut = symcipher_data->cDecryptor.cOutputBuffer->WriteReserve(EVP_MAX_BLOCK_LENGTH + BLOCK_SIZE);
  if (lpOut == NULL)
  {
    symcipher_data->cDecryptor.Reset(TRUE);
    return E_OUTOFMEMORY;
  }

  if (EVP_DecryptFinal_ex(symcipher_data->cDecryptor.lpCipherCtx, lpOut, &nOutSize) <= 0)
  {
    symcipher_data->cDecryptor.Reset(TRUE);
    return MX_E_InvalidData;
  }

  symcipher_data->cDecryptor.cOutputBuffer->EndWriteReserve((SIZE_T)nOutSize);
  symcipher_data->cDecryptor.Reset(FALSE);

  //done
  return S_OK;
}

SIZE_T CSymmetricCipher::GetAvailableDecryptedData() const
{
  return (lpInternalData != NULL) ? symcipher_data->cDecryptor.cOutputBuffer->GetLength() : 0;
}

SIZE_T CSymmetricCipher::GetDecryptedData(_Out_writes_(nDestSize) LPVOID lpDest, _In_ SIZE_T nDestSize)
{
  if (lpInternalData == NULL)
    return 0;
  return symcipher_data->cDecryptor.cOutputBuffer->Read(lpDest, nDestSize);
}

HRESULT CSymmetricCipher::InternalInitialize()
{
  HRESULT hRes;

  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;
  if (lpInternalData == NULL)
  {
    lpInternalData = MX_DEBUG_NEW Internals::CSymmetricCipherData();
    if (lpInternalData == NULL)
      return E_OUTOFMEMORY;
    if (!(symcipher_data->cEncryptor.cOutputBuffer && symcipher_data->cDecryptor.cOutputBuffer))
    {
      delete symcipher_data;
      lpInternalData = NULL;
      return E_OUTOFMEMORY;
    }
  }
  //done
  return S_OK;
}

} //namespace MX

//-----------------------------------------------------------

static const EVP_CIPHER* GetCipher(_In_ MX::CSymmetricCipher::eAlgorithm nAlgorithm)
{
  ERR_clear_error();
  switch (nAlgorithm)
  {
    case MX::CSymmetricCipher::eAlgorithm::DES_ECB:
      return EVP_CIPHER_fetch(NULL, "DES-ECB", NULL);
    case MX::CSymmetricCipher::eAlgorithm::DES_CBC:
      return EVP_CIPHER_fetch(NULL, "DES-CBC", NULL);
    case MX::CSymmetricCipher::eAlgorithm::DES_OFB:
      return EVP_CIPHER_fetch(NULL, "DES-OFB", NULL);
    case MX::CSymmetricCipher::eAlgorithm::DES_CFB1:
      return EVP_CIPHER_fetch(NULL, "DES-CFB1", NULL);
    case MX::CSymmetricCipher::eAlgorithm::DES_CFB8:
      return EVP_CIPHER_fetch(NULL, "DES-CFB8", NULL);
    case MX::CSymmetricCipher::eAlgorithm::DES_CFB64:
      return EVP_CIPHER_fetch(NULL, "DES-CFB", NULL);

    case MX::CSymmetricCipher::eAlgorithm::TripleDES_ECB:
      return EVP_CIPHER_fetch(NULL, "DES-EDE3-ECB", NULL);
    case MX::CSymmetricCipher::eAlgorithm::TripleDES_CBC:
      return EVP_CIPHER_fetch(NULL, "DES-EDE3-CBC", NULL);
    case MX::CSymmetricCipher::eAlgorithm::TripleDES_OFB:
      return EVP_CIPHER_fetch(NULL, "DES-EDE3-OFB", NULL);
    case MX::CSymmetricCipher::eAlgorithm::TripleDES_CFB1:
      return EVP_CIPHER_fetch(NULL, "DES-EDE3-CFB1", NULL);
    case MX::CSymmetricCipher::eAlgorithm::TripleDES_CFB8:
      return EVP_CIPHER_fetch(NULL, "DES-EDE3-CFB8", NULL);
    case MX::CSymmetricCipher::eAlgorithm::TripleDES_CFB64:
      return EVP_CIPHER_fetch(NULL, "DES-EDE3-CFB", NULL);

    case MX::CSymmetricCipher::eAlgorithm::AES_128_ECB:
      return EVP_CIPHER_fetch(NULL, "AES-128-ECB", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_128_CBC:
      return EVP_CIPHER_fetch(NULL, "AES-128-ECB", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_128_CTR:
      return EVP_CIPHER_fetch(NULL, "AES-128-CTR", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_128_GCM:
      return EVP_CIPHER_fetch(NULL, "AES-128-GCM", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_128_OFB:
      return EVP_CIPHER_fetch(NULL, "AES-128-OFB", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_128_CFB1:
      return EVP_CIPHER_fetch(NULL, "AES-128-CFB1", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_128_CFB8:
      return EVP_CIPHER_fetch(NULL, "AES-128-CFB8", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_128_CFB128:
      return EVP_CIPHER_fetch(NULL, "AES-128-CFB", NULL);

    case MX::CSymmetricCipher::eAlgorithm::AES_192_ECB:
      return EVP_CIPHER_fetch(NULL, "AES-192-ECB", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_192_CBC:
      return EVP_CIPHER_fetch(NULL, "AES-192-ECB", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_192_CTR:
      return EVP_CIPHER_fetch(NULL, "AES-192-CTR", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_192_GCM:
      return EVP_CIPHER_fetch(NULL, "AES-192-GCM", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_192_OFB:
      return EVP_CIPHER_fetch(NULL, "AES-192-OFB", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_192_CFB1:
      return EVP_CIPHER_fetch(NULL, "AES-192-CFB1", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_192_CFB8:
      return EVP_CIPHER_fetch(NULL, "AES-192-CFB8", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_192_CFB128:
      return EVP_CIPHER_fetch(NULL, "AES-192-CFB", NULL);

    case MX::CSymmetricCipher::eAlgorithm::AES_256_ECB:
      return EVP_CIPHER_fetch(NULL, "AES-256-ECB", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_256_CBC:
      return EVP_CIPHER_fetch(NULL, "AES-256-ECB", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_256_CTR:
      return EVP_CIPHER_fetch(NULL, "AES-256-CTR", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_256_GCM:
      return EVP_CIPHER_fetch(NULL, "AES-256-GCM", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_256_OFB:
      return EVP_CIPHER_fetch(NULL, "AES-256-OFB", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_256_CFB1:
      return EVP_CIPHER_fetch(NULL, "AES-256-CFB1", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_256_CFB8:
      return EVP_CIPHER_fetch(NULL, "AES-256-CFB8", NULL);
    case MX::CSymmetricCipher::eAlgorithm::AES_256_CFB128:
      return EVP_CIPHER_fetch(NULL, "AES-256-CFB", NULL);
  }
  return NULL;
}
