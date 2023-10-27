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
#include "..\..\Include\Crypto\AsymmetricCipher.h"
#include "..\..\Include\RefCounted.h"
#include "..\..\Include\Crypto\SecureBuffer.h"
#include "InitOpenSSL.h"
#include <OpenSSL\evp.h>
#include "openssl\encoder.h"
#include "..\..\Include\AutoPtr.h"

//-----------------------------------------------------------

#define BLOCK_SIZE                                      2048

#define ZONE_Encryptor 1
#define ZONE_Decryptor 2
#define ZONE_Signer    3
#define ZONE_Verifier  4

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CAsymmetricCipherEncoderDecoder : public virtual CBaseMemObj
{
public:
  CAsymmetricCipherEncoderDecoder() : CBaseMemObj()
    {
    cOutputBuffer.Attach(MX_DEBUG_NEW CSecureBuffer());
    cInputBuffer.Attach(MX_DEBUG_NEW CSecureBuffer());
    return;
    };

  ~CAsymmetricCipherEncoderDecoder()
    {
    Reset(TRUE);
    return;
    };

  BOOL InitOK() const
    {
    return (cOutputBuffer && cInputBuffer) ? TRUE : FALSE;
    };

  VOID Reset(_In_ BOOL bCleanOutputBuffer)
    {
    if (lpKeyCtx != NULL)
    {
      EVP_PKEY_CTX_free(lpKeyCtx);
      lpKeyCtx = NULL;
    }
    if (bCleanOutputBuffer != NULL)
    {
      CleanOutputBuffers();
    }
    cInputBuffer->Reset();
    nPaddingSize = 0;
    return;
    };

  VOID CleanOutputBuffers()
    {
    cOutputBuffer->Reset();
    return;
    };

public:
  TAutoRefCounted<CSecureBuffer> cOutputBuffer;
  TAutoRefCounted<CSecureBuffer> cInputBuffer;
  int nPaddingSize = 0;
  EVP_PKEY_CTX *lpKeyCtx{ NULL };
};

class CAsymmetricCipherSignerVerifier : public virtual CBaseMemObj
{
public:
  CAsymmetricCipherSignerVerifier() : CBaseMemObj()
    {
    cOutputBuffer.Attach(MX_DEBUG_NEW CSecureBuffer());
    return;
    };

  ~CAsymmetricCipherSignerVerifier()
    {
    Reset(TRUE);
    return;
    };

  BOOL InitOK() const
    {
    return (cOutputBuffer) ? TRUE : FALSE;
    };

  VOID Reset(_In_ BOOL bCleanOutputBuffer)
    {
    if (lpKeyCtx != NULL)
    {
      EVP_PKEY_CTX_free(lpKeyCtx);
      lpKeyCtx = NULL;
    }
    if (bCleanOutputBuffer != NULL)
    {
      CleanOutputBuffers();
    }
    cDigest.EndDigest();
    return;
    };

  VOID CleanOutputBuffers()
    {
    cOutputBuffer->Reset();
    return;
    };

public:
  TAutoRefCounted<CSecureBuffer> cOutputBuffer;
  CMessageDigest cDigest;
  EVP_PKEY_CTX *lpKeyCtx{ NULL };
};

class CAsymmetricCipherData : public virtual CBaseMemObj
{
public:
  CAsymmetricCipherData()
    {
    lpKey = NULL;
    return;
    };

  ~CAsymmetricCipherData()
    {
    cEncryptor.Reset();
    cDecryptor.Reset();
    cSigner.Reset();
    cVerifier.Reset();
    if (lpKey != NULL)
      EVP_PKEY_free(lpKey);
    return;
    };

  BOOL InUse(_In_ int nZone) const
    {
    switch (nZone)
    {
      case 0:
        return ((cEncryptor && cEncryptor->lpKeyCtx != NULL) ||
                (cDecryptor && cDecryptor->lpKeyCtx != NULL) ||
                (cSigner && cSigner->lpKeyCtx != NULL) ||
                (cVerifier && cVerifier->lpKeyCtx != NULL)) ? TRUE : FALSE;

      case ZONE_Encryptor:
        return (cEncryptor && cEncryptor->lpKeyCtx != NULL) ? TRUE : FALSE;

      case ZONE_Decryptor:
        return (cDecryptor && cDecryptor->lpKeyCtx != NULL) ? TRUE : FALSE;

      case ZONE_Signer:
        return (cSigner && cSigner->lpKeyCtx != NULL) ? TRUE : FALSE;

      case ZONE_Verifier:
        return (cVerifier && cVerifier->lpKeyCtx != NULL) ? TRUE : FALSE;
    }
    return FALSE;
    };

  VOID SetKey(_In_opt_ EVP_PKEY *lpNewKey)
    {
    if (lpKey != NULL)
      EVP_PKEY_free(lpKey);
    lpKey = lpNewKey;
    if (lpKey != NULL)
      EVP_PKEY_up_ref(lpKey);
    return;
    };

  BOOL InitZone(_In_ int nZone)
    {
    switch (nZone)
    {
      case ZONE_Encryptor:
        if (!cEncryptor)
        {
          cEncryptor.Attach(MX_DEBUG_NEW CAsymmetricCipherEncoderDecoder());
          if (!cEncryptor)
            return FALSE;
          if (cEncryptor->InitOK() == FALSE)
          {
            cEncryptor.Reset();
            return FALSE;
          }
        }
        break;

      case ZONE_Decryptor:
        if (!cDecryptor)
        {
          cDecryptor.Attach(MX_DEBUG_NEW CAsymmetricCipherEncoderDecoder());
          if (!cDecryptor)
            return FALSE;
          if (cDecryptor->InitOK() == FALSE)
          {
            cDecryptor.Reset();
            return FALSE;
          }
        }
        break;

      case ZONE_Signer:
        if (!cSigner)
        {
          cSigner.Attach(MX_DEBUG_NEW CAsymmetricCipherSignerVerifier());
          if (!cSigner)
            return FALSE;
          if (cSigner->InitOK() == FALSE)
          {
            cSigner.Reset();
            return FALSE;
          }
        }
        break;

      case ZONE_Verifier:
        if (!cVerifier)
        {
          cVerifier.Attach(MX_DEBUG_NEW CAsymmetricCipherSignerVerifier());
          if (!cVerifier)
            return FALSE;
          if (cVerifier->InitOK() == FALSE)
          {
            cVerifier.Reset();
            return FALSE;
          }
        }
        break;

      default:
        return FALSE;
    }

    return TRUE;
    };

  HRESULT InitKeyCtx(_In_ int nZone)
    {
    if (lpKey == NULL)
      return MX_E_NotReady;

    switch (nZone)
    {
      case ZONE_Encryptor:
        cEncryptor->Reset(TRUE);
        ERR_clear_error();
        cEncryptor->lpKeyCtx = EVP_PKEY_CTX_new_from_pkey(NULL, lpKey, NULL);
        if (cEncryptor->lpKeyCtx == NULL)
          return MX::Internals::OpenSSL::GetLastErrorCode(E_OUTOFMEMORY);
        if (EVP_PKEY_encrypt_init(cEncryptor->lpKeyCtx) <= 0)
          return MX::Internals::OpenSSL::GetLastErrorCode(E_NOTIMPL);
        break;

      case ZONE_Decryptor:
        cDecryptor->Reset(TRUE);
        ERR_clear_error();
        cDecryptor->lpKeyCtx = EVP_PKEY_CTX_new_from_pkey(NULL, lpKey, NULL);
        if (cDecryptor->lpKeyCtx == NULL)
          return MX::Internals::OpenSSL::GetLastErrorCode(E_OUTOFMEMORY);
        if (EVP_PKEY_decrypt_init(cEncryptor->lpKeyCtx) <= 0)
          return MX::Internals::OpenSSL::GetLastErrorCode(E_NOTIMPL);
        break;

      case ZONE_Signer:
        cSigner->Reset(TRUE);
        ERR_clear_error();
        cSigner->lpKeyCtx = EVP_PKEY_CTX_new_from_pkey(NULL, lpKey, NULL);
        if (cSigner->lpKeyCtx == NULL)
          return MX::Internals::OpenSSL::GetLastErrorCode(E_OUTOFMEMORY);
        if (EVP_PKEY_sign_init(cEncryptor->lpKeyCtx) <= 0)
          return MX::Internals::OpenSSL::GetLastErrorCode(E_NOTIMPL);
        break;

      case ZONE_Verifier:
        cVerifier->Reset(TRUE);
        ERR_clear_error();
        cVerifier->lpKeyCtx = EVP_PKEY_CTX_new_from_pkey(NULL, lpKey, NULL);
        if (cVerifier->lpKeyCtx == NULL)
          return MX::Internals::OpenSSL::GetLastErrorCode(E_OUTOFMEMORY);
        if (EVP_PKEY_verify_init(cEncryptor->lpKeyCtx) <= 0)
          return MX::Internals::OpenSSL::GetLastErrorCode(E_NOTIMPL);
        break;

      default:
        return E_INVALIDARG;
    }

    return S_OK;
    };

public:
  EVP_PKEY *lpKey{ NULL };
  MX::TAutoDeletePtr<CAsymmetricCipherEncoderDecoder> cEncryptor;
  MX::TAutoDeletePtr<CAsymmetricCipherEncoderDecoder> cDecryptor;
  MX::TAutoDeletePtr<CAsymmetricCipherSignerVerifier> cSigner;
  MX::TAutoDeletePtr<CAsymmetricCipherSignerVerifier> cVerifier;
};

} //namespace Internals

} //namespace MX

#define asymcipher_data ((MX::Internals::CAsymmetricCipherData*)lpInternalData)

//-----------------------------------------------------------

namespace MX {

CAsymmetricCipher::CAsymmetricCipher() : CBaseMemObj(), CNonCopyableObj()
{
  lpInternalData = NULL;
  return;
}

CAsymmetricCipher::~CAsymmetricCipher()
{
  if (lpInternalData != NULL)
  {
    delete asymcipher_data;
    lpInternalData = NULL;
  }
  return;
}

HRESULT CAsymmetricCipher::SetKey(_In_ CEncryptionKey *lpKey)
{
  EVP_PKEY *lpNewKey;
  HRESULT hRes;

  if (lpKey == NULL)
    return E_POINTER;
  lpNewKey = lpKey->GetPKey();
  if (lpNewKey == NULL)
    return E_POINTER;

  hRes = InternalInitialize(0);
  if (FAILED(hRes))
    return hRes;
  if (asymcipher_data->InUse(0) != FALSE)
    return MX_E_Busy;

  asymcipher_data->SetKey(lpNewKey);

  //done
  return S_OK;
}

HRESULT CAsymmetricCipher::BeginEncrypt(_In_opt_ MX::CAsymmetricCipher::ePadding nPadding)
{
  HRESULT hRes;

  hRes = InternalInitialize(ZONE_Encryptor);
  if (FAILED(hRes))
    return hRes;

  hRes = asymcipher_data->InitKeyCtx(ZONE_Encryptor);
  if (FAILED(hRes))
    return hRes;

  switch (EVP_PKEY_get_base_id(asymcipher_data->lpKey))
  {
    case EVP_PKEY_RSA:
      switch (nPadding)
      {
        case MX::CAsymmetricCipher::None:
          if (EVP_PKEY_CTX_set_rsa_padding(asymcipher_data->cEncryptor->lpKeyCtx, RSA_NO_PADDING) <= 0)
          {
            hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_Unsupported);
            asymcipher_data->cEncryptor->Reset(TRUE);
            return hRes;
          }
          asymcipher_data->cEncryptor->nPaddingSize = 0;
          break;

        case MX::CAsymmetricCipher::PKCS1:
          if (EVP_PKEY_CTX_set_rsa_padding(asymcipher_data->cEncryptor->lpKeyCtx, RSA_PKCS1_PADDING) <= 0)
          {
            hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_Unsupported);
            asymcipher_data->cEncryptor->Reset(TRUE);
            return hRes;
          }
          asymcipher_data->cEncryptor->nPaddingSize = 11;
          break;

        case MX::CAsymmetricCipher::OAEP:
          if (EVP_PKEY_CTX_set_rsa_padding(asymcipher_data->cEncryptor->lpKeyCtx, RSA_PKCS1_OAEP_PADDING) <= 0)
          {
            hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_Unsupported);
            asymcipher_data->cEncryptor->Reset(TRUE);
            return hRes;
          }
          asymcipher_data->cEncryptor->nPaddingSize = 41;
          break;

        default:
          asymcipher_data->cEncryptor->Reset(TRUE);
          return E_INVALIDARG;
      }
      break;
  }

  //done
  return S_OK;
}

HRESULT CAsymmetricCipher::EncryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)
{
  SIZE_T nMaxWritable;
  HRESULT hRes;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (lpInternalData == NULL || asymcipher_data->InUse(ZONE_Encryptor) == FALSE)
    return MX_E_NotReady;

  nMaxWritable = EVP_PKEY_get_size(asymcipher_data->lpKey) - asymcipher_data->cEncryptor->nPaddingSize -
                 asymcipher_data->cEncryptor->cInputBuffer->GetLength();
  if (nDataLength > nMaxWritable)
  {
    asymcipher_data->cEncryptor->Reset(TRUE);
    return MX_E_InvalidData;
  }

  hRes = asymcipher_data->cEncryptor->cInputBuffer->WriteStream(lpData, nDataLength);
  if (FAILED(hRes))
  {
    asymcipher_data->cEncryptor->Reset(TRUE);
    return hRes;
  }

  //done
  return S_OK;
}

HRESULT CAsymmetricCipher::EndEncrypt()
{
  size_t nOutLen;
  HRESULT hRes;

  if (lpInternalData == NULL || asymcipher_data->InUse(ZONE_Encryptor) == FALSE)
    return MX_E_NotReady;

  ERR_clear_error();
  if (EVP_PKEY_encrypt(asymcipher_data->cEncryptor->lpKeyCtx, NULL, &nOutLen,
                       asymcipher_data->cEncryptor->cInputBuffer->GetBuffer(),
                       (int)(asymcipher_data->cEncryptor->cInputBuffer->GetLength())) <= 0)
  {
    hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_Unsupported);
    asymcipher_data->cEncryptor->Reset(FALSE);
    return hRes;
  }

  if (nOutLen > 0)
  {
    LPBYTE lpOut;

    lpOut = asymcipher_data->cEncryptor->cOutputBuffer->WriteReserve((SIZE_T)nOutLen);
    if (lpOut == NULL)
    {
      asymcipher_data->cEncryptor->Reset(TRUE);
      return E_OUTOFMEMORY;
    }

    if (EVP_PKEY_encrypt(asymcipher_data->cEncryptor->lpKeyCtx, lpOut, &nOutLen,
                         asymcipher_data->cEncryptor->cInputBuffer->GetBuffer(),
                         (int)(asymcipher_data->cEncryptor->cInputBuffer->GetLength())) <= 0)
    {
      hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_Unsupported);
      asymcipher_data->cEncryptor->Reset(TRUE);
      return hRes;
    }

    asymcipher_data->cEncryptor->cOutputBuffer->EndWriteReserve((SIZE_T)nOutLen);
  }

  //done
  asymcipher_data->cEncryptor->Reset(FALSE);
  return S_OK;
}

LPBYTE CAsymmetricCipher::GetEncryptedData() const
{
  if (lpInternalData == NULL || (!(asymcipher_data->cEncryptor)))
    return NULL;
  return asymcipher_data->cEncryptor->cOutputBuffer->GetBuffer();
}

SIZE_T CAsymmetricCipher::GetEncryptedDataSize() const
{
  if (lpInternalData == NULL || (!(asymcipher_data->cEncryptor)))
    return 0;
  return asymcipher_data->cEncryptor->cOutputBuffer->GetLength();
}

HRESULT CAsymmetricCipher::BeginDecrypt(_In_opt_ MX::CAsymmetricCipher::ePadding nPadding)
{
  HRESULT hRes;

  hRes = InternalInitialize(ZONE_Decryptor);
  if (FAILED(hRes))
    return hRes;

  hRes = asymcipher_data->InitKeyCtx(ZONE_Decryptor);
  if (FAILED(hRes))
    return hRes;

  switch (EVP_PKEY_get_base_id(asymcipher_data->lpKey))
  {
    case EVP_PKEY_RSA:
      switch (nPadding)
      {
        case MX::CAsymmetricCipher::None:
          if (EVP_PKEY_CTX_set_rsa_padding(asymcipher_data->cDecryptor->lpKeyCtx, RSA_NO_PADDING) <= 0)
          {
            hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_Unsupported);
            asymcipher_data->cDecryptor->Reset(TRUE);
            return hRes;
          }
          asymcipher_data->cDecryptor->nPaddingSize = 0;
          break;

        case MX::CAsymmetricCipher::PKCS1:
          if (EVP_PKEY_CTX_set_rsa_padding(asymcipher_data->cDecryptor->lpKeyCtx, RSA_PKCS1_PADDING) <= 0)
          {
            hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_Unsupported);
            asymcipher_data->cDecryptor->Reset(TRUE);
            return hRes;
          }
          asymcipher_data->cDecryptor->nPaddingSize = 11;
          break;

        case MX::CAsymmetricCipher::OAEP:
          if (EVP_PKEY_CTX_set_rsa_padding(asymcipher_data->cDecryptor->lpKeyCtx, RSA_PKCS1_OAEP_PADDING) <= 0)
          {
            hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_Unsupported);
            asymcipher_data->cDecryptor->Reset(TRUE);
            return hRes;
          }
          asymcipher_data->cDecryptor->nPaddingSize = 41;
          break;

        default:
          asymcipher_data->cDecryptor->Reset(TRUE);
          return E_INVALIDARG;
      }
      break;
  }

  //done
  return S_OK;
}

HRESULT CAsymmetricCipher::DecryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)
{
  SIZE_T nMaxWritable;
  HRESULT hRes;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (lpInternalData == NULL || asymcipher_data->InUse(ZONE_Decryptor) == FALSE)
    return MX_E_NotReady;

  nMaxWritable = EVP_PKEY_size(asymcipher_data->lpKey) - asymcipher_data->cDecryptor->nPaddingSize -
                 asymcipher_data->cDecryptor->cInputBuffer->GetLength();
  if (nDataLength > nMaxWritable)
  {
    asymcipher_data->cDecryptor->Reset(TRUE);
    return MX_E_InvalidData;
  }

  hRes = asymcipher_data->cDecryptor->cInputBuffer->WriteStream(lpData, nDataLength);
  if (FAILED(hRes))
  {
    asymcipher_data->cDecryptor->Reset(TRUE);
    return hRes;
  }

  //done
  return S_OK;
}

HRESULT CAsymmetricCipher::EndDecrypt()
{
  size_t nOutLen;
  HRESULT hRes;

  if (lpInternalData == NULL || asymcipher_data->InUse(ZONE_Decryptor) == FALSE)
    return MX_E_NotReady;

  ERR_clear_error();
  if (EVP_PKEY_encrypt(asymcipher_data->cDecryptor->lpKeyCtx, NULL, &nOutLen,
                       asymcipher_data->cDecryptor->cInputBuffer->GetBuffer(),
                       (int)(asymcipher_data->cDecryptor->cInputBuffer->GetLength())) <= 0)
  {
    hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_Unsupported);
    asymcipher_data->cDecryptor->Reset(TRUE);
    return hRes;
  }

  if (nOutLen > 0)
  {
    LPBYTE lpOut;

    lpOut = asymcipher_data->cDecryptor->cOutputBuffer->WriteReserve((SIZE_T)nOutLen);
    if (lpOut == NULL)
    {
      asymcipher_data->cDecryptor->Reset(TRUE);
      return E_OUTOFMEMORY;
    }

    if (EVP_PKEY_encrypt(asymcipher_data->cDecryptor->lpKeyCtx, lpOut, &nOutLen,
                         asymcipher_data->cDecryptor->cInputBuffer->GetBuffer(),
                         (int)(asymcipher_data->cDecryptor->cInputBuffer->GetLength())) <= 0)
    {
      hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_Unsupported);
      asymcipher_data->cDecryptor->Reset(TRUE);
      return hRes;
    }

    asymcipher_data->cDecryptor->cOutputBuffer->EndWriteReserve((SIZE_T)nOutLen);
  }

  //done
  asymcipher_data->cDecryptor->Reset(FALSE);
  return S_OK;
}

LPBYTE CAsymmetricCipher::GetDecryptedData() const
{
  if (lpInternalData == NULL || (!(asymcipher_data->cDecryptor)))
    return NULL;
  return asymcipher_data->cDecryptor->cOutputBuffer->GetBuffer();
}

SIZE_T CAsymmetricCipher::GetDecryptedDataSize() const
{
  if (lpInternalData == NULL || (!(asymcipher_data->cDecryptor)))
    return 0;
  return asymcipher_data->cDecryptor->cOutputBuffer->GetLength();
}

HRESULT CAsymmetricCipher::BeginSign(_In_ MX::CMessageDigest::eAlgorithm nAlgorithm)
{
  HRESULT hRes;

  hRes = InternalInitialize(ZONE_Signer);
  if (FAILED(hRes))
    return hRes;

  hRes = asymcipher_data->InitKeyCtx(ZONE_Signer);
  if (FAILED(hRes))
    return hRes;

  hRes = asymcipher_data->cSigner->cDigest.BeginDigest(nAlgorithm);
  if (FAILED(hRes))
  {
    asymcipher_data->cSigner->Reset(TRUE);
    return hRes;
  }

  //done
  return S_OK;
}

HRESULT CAsymmetricCipher::SignStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)
{
  HRESULT hRes;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;

  if (lpInternalData == NULL || asymcipher_data->InUse(ZONE_Signer) == FALSE)
    return MX_E_NotReady;

  hRes = asymcipher_data->cSigner->cDigest.DigestStream(lpData, nDataLength);
  if (FAILED(hRes))
  {
    asymcipher_data->cSigner->Reset(TRUE);
    return hRes;
  }

  //done
  return S_OK;
}

HRESULT CAsymmetricCipher::EndSign()
{
  LPBYTE lpSig;
  size_t nSigSize;
  HRESULT hRes;

  if (lpInternalData == NULL || asymcipher_data->InUse(ZONE_Signer) == FALSE)
    return MX_E_NotReady;

  hRes = asymcipher_data->cSigner->cDigest.EndDigest();
  if (FAILED(hRes))
  {
    asymcipher_data->cSigner->Reset(TRUE);
    return hRes;
  }

  nSigSize = (size_t)EVP_PKEY_get_size(asymcipher_data->lpKey);

  lpSig = asymcipher_data->cSigner->cOutputBuffer->WriteReserve(nSigSize);
  if (lpSig == NULL)
  {
    asymcipher_data->cSigner->Reset(TRUE);
    return E_OUTOFMEMORY;
  }

  ERR_clear_error();
  if (EVP_PKEY_sign(asymcipher_data->cSigner->lpKeyCtx, (unsigned char *)lpSig, &nSigSize,
                    asymcipher_data->cSigner->cDigest.GetResult(),
                    asymcipher_data->cSigner->cDigest.GetResultSize()) <= 0)
  {
    hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_Unsupported);
    asymcipher_data->cSigner->Reset(TRUE);
    return hRes;
  };
  asymcipher_data->cSigner->cOutputBuffer->EndWriteReserve(nSigSize);

  //done
  asymcipher_data->cSigner->Reset(FALSE);
  return S_OK;
}

LPBYTE CAsymmetricCipher::GetSignature() const
{
  if (lpInternalData == NULL || (!(asymcipher_data->cSigner)))
    return NULL;
  return asymcipher_data->cSigner->cOutputBuffer->GetBuffer();
}

SIZE_T CAsymmetricCipher::GetSignatureSize() const
{
  if (lpInternalData == NULL || (!(asymcipher_data->cSigner)))
    return 0;
  return asymcipher_data->cSigner->cOutputBuffer->GetLength();
}

HRESULT CAsymmetricCipher::BeginVerify(_In_ MX::CMessageDigest::eAlgorithm nAlgorithm)
{
  HRESULT hRes;

  hRes = InternalInitialize(ZONE_Verifier);
  if (FAILED(hRes))
    return hRes;

  hRes = asymcipher_data->InitKeyCtx(ZONE_Verifier);
  if (FAILED(hRes))
    return hRes;

  //initialize
  hRes = asymcipher_data->cVerifier->cDigest.BeginDigest(nAlgorithm);
  if (FAILED(hRes))
  {
    asymcipher_data->cVerifier->Reset(TRUE);
    return hRes;
  }

  //done
  return S_OK;
}

HRESULT CAsymmetricCipher::VerifyStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)
{
  HRESULT hRes;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;

  if (lpInternalData == NULL || asymcipher_data->InUse(ZONE_Verifier) == FALSE)
    return MX_E_NotReady;

  hRes = asymcipher_data->cVerifier->cDigest.DigestStream(lpData, nDataLength);
  if (FAILED(hRes))
  {
    asymcipher_data->cVerifier->Reset(TRUE);
    return hRes;
  }

  //done
  return S_OK;
}

HRESULT CAsymmetricCipher::EndVerify(_In_ LPCVOID lpSignature, _In_ SIZE_T nSignatureLen)
{
  int res;
  HRESULT hRes;

  if (lpSignature == NULL)
    return E_POINTER;
  if (nSignatureLen == 0)
    return E_INVALIDARG;

  if (lpInternalData == NULL || asymcipher_data->InUse(ZONE_Verifier) == FALSE)
    return MX_E_NotReady;

  hRes = asymcipher_data->cVerifier->cDigest.EndDigest();
  if (FAILED(hRes))
  {
    asymcipher_data->cVerifier->Reset(TRUE);
    return hRes;
  }

  ERR_clear_error();
  res = EVP_PKEY_verify(asymcipher_data->cVerifier->lpKeyCtx, (unsigned char *)lpSignature, nSignatureLen,
                        asymcipher_data->cVerifier->cDigest.GetResult(),
                        asymcipher_data->cVerifier->cDigest.GetResultSize());
  if (res > 0)
  {
    hRes = S_OK;
  }
  else if (res == 0)
  {
    hRes = S_FALSE;
  }
  else
  {
    hRes = Internals::OpenSSL::GetLastErrorCode(MX_E_Unsupported);
  }
  
  //done
  asymcipher_data->cVerifier->Reset(TRUE);
  return hRes;
}

HRESULT CAsymmetricCipher::InternalInitialize(_In_ int nZone)
{
  HRESULT hRes;

  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;

  if (lpInternalData == NULL)
  {
    lpInternalData = MX_DEBUG_NEW MX::Internals::CAsymmetricCipherData();
    if (lpInternalData == NULL)
      return E_OUTOFMEMORY;
  }

  if (nZone != 0)
  {
    if (asymcipher_data->InitZone(nZone) == FALSE)
      return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

} //namespace MX
