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

//-----------------------------------------------------------

#define BLOCK_SIZE                                      2048

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CAsymmetricCipherEncoderDecoder : public virtual CBaseMemObj
{
public:
  CAsymmetricCipherEncoderDecoder() : CBaseMemObj()
    {
    bInUse = FALSE;
    nPaddingMode = RSA_PKCS1_PADDING;
    nPaddingSize = 0;
    bUsePrivateKey = FALSE;
    return;
    };

  ~CAsymmetricCipherEncoderDecoder()
    {
    Reset(TRUE);
    return;
    };

  VOID Reset(_In_ BOOL bCleanOutputBuffer)
    {
    bInUse = FALSE;
    if (bCleanOutputBuffer != NULL)
    {
      CleanOutputBuffers();
    }
    cInputBuffer.Reset();
    nPaddingSize = 0;
    bUsePrivateKey = FALSE;
    return;
    };

  VOID CleanOutputBuffers()
    {
    cOutputBuffer.Reset();
    return;
    };

public:
  BOOL bInUse;
  MX::CSecureBuffer cOutputBuffer;
  MX::CSecureBuffer cInputBuffer;
  int nPaddingMode, nPaddingSize;
  BOOL bUsePrivateKey;
};

class CAsymmetricCipherSignerVerifier : public virtual CBaseMemObj
{
public:
  CAsymmetricCipherSignerVerifier() : CBaseMemObj()
    {
    bInUse = FALSE;
    return;
    };

  ~CAsymmetricCipherSignerVerifier()
    {
    Reset(TRUE);
    return;
    };

  VOID Reset(_In_ BOOL bCleanOutputBuffer)
    {
    bInUse = FALSE;
    if (bCleanOutputBuffer != NULL)
    {
      CleanOutputBuffers();
    }
    cDigest.EndDigest();
    return;
    };

  VOID CleanOutputBuffers()
    {
    cOutputBuffer.Reset();
    return;
    };

public:
  BOOL bInUse;
  MX::CSecureBuffer cOutputBuffer;
  MX::CMessageDigest cDigest;
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
    cEncryptor.Reset(TRUE);
    cDecryptor.Reset(TRUE);
    cSigner.Reset(TRUE);
    cVerifier.Reset(TRUE);
    SetKey(NULL);
    return;
    };

  VOID SetKey(_In_ EVP_PKEY *lpNewKey)
    {
    if (lpKey != NULL)
      EVP_PKEY_free(lpKey);
    lpKey = lpNewKey;
    return;
    };

  BOOL InUse() const
    {
    return (cEncryptor.bInUse != FALSE || cDecryptor.bInUse != FALSE ||
            cSigner.bInUse != FALSE || cVerifier.bInUse != FALSE) ? TRUE : FALSE;
    };

public:
  EVP_PKEY *lpKey;
  CAsymmetricCipherEncoderDecoder cEncryptor;
  CAsymmetricCipherEncoderDecoder cDecryptor;
  CAsymmetricCipherSignerVerifier cSigner;
  CAsymmetricCipherSignerVerifier cVerifier;
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

  hRes = InternalInitialize();
  if (FAILED(hRes))
    return hRes;
  if (asymcipher_data->InUse() != FALSE)
    return MX_E_Busy;

  EVP_PKEY_up_ref(lpNewKey);
  asymcipher_data->SetKey(lpNewKey);

  //done
  return S_OK;
}

HRESULT CAsymmetricCipher::BeginEncrypt(_In_opt_ MX::CAsymmetricCipher::ePadding nPadding, _In_opt_ BOOL bUsePublicKey)
{
  HRESULT hRes;

  hRes = InternalInitialize();
  if (FAILED(hRes))
    return hRes;

  if (asymcipher_data->lpKey == NULL)
    return MX_E_NotReady;

  //create context
  asymcipher_data->cEncryptor.Reset(TRUE);
  asymcipher_data->cEncryptor.bInUse = TRUE;

  switch (EVP_PKEY_base_id(asymcipher_data->lpKey))
  {
    case EVP_PKEY_RSA:
      switch (nPadding)
      {
        case MX::CAsymmetricCipher::PaddingNone:
          asymcipher_data->cEncryptor.nPaddingMode = RSA_PKCS1_PADDING;
          asymcipher_data->cEncryptor.nPaddingSize = 0;
          break;

        case MX::CAsymmetricCipher::PaddingPKCS1:
          asymcipher_data->cEncryptor.nPaddingMode = RSA_PKCS1_PADDING;
          asymcipher_data->cEncryptor.nPaddingSize = 11;
          break;

        case MX::CAsymmetricCipher::PaddingOAEP:
          asymcipher_data->cEncryptor.nPaddingMode = RSA_PKCS1_OAEP_PADDING;
          asymcipher_data->cEncryptor.nPaddingSize = 41;
          break;

        case MX::CAsymmetricCipher::PaddingSSLV23:
          asymcipher_data->cEncryptor.nPaddingMode = RSA_SSLV23_PADDING;
          asymcipher_data->cEncryptor.nPaddingSize = 11;
          break;

        default:
          asymcipher_data->cEncryptor.Reset(TRUE);
          return E_INVALIDARG;
      }

      asymcipher_data->cEncryptor.bUsePrivateKey = !bUsePublicKey;
      break;

    default:
      //cannot encrypt with a non-rsa key
      asymcipher_data->cEncryptor.Reset(TRUE);
      return MX_E_Unsupported;
  }

  //empty buffer
  asymcipher_data->cEncryptor.CleanOutputBuffers();

  //done
  return S_OK;
}

HRESULT CAsymmetricCipher::EncryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)
{
  SIZE_T nMaxWritable;
  HRESULT hRes;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (lpInternalData == NULL || asymcipher_data->cEncryptor.bInUse == FALSE)
    return MX_E_NotReady;

  nMaxWritable = EVP_PKEY_size(asymcipher_data->lpKey) - asymcipher_data->cEncryptor.nPaddingSize -
                 asymcipher_data->cEncryptor.cInputBuffer.GetLength();
  if (nDataLength > nMaxWritable)
  {
    asymcipher_data->cEncryptor.Reset(TRUE);
    return MX_E_InvalidData;
  }

  hRes = asymcipher_data->cEncryptor.cInputBuffer.WriteStream(lpData, nDataLength);
  if (FAILED(hRes))
  {
    asymcipher_data->cEncryptor.Reset(TRUE);
    return hRes;
  }

  //done
  return S_OK;
}

HRESULT CAsymmetricCipher::EndEncrypt()
{
  LPBYTE lpOut;
  int res;
  HRESULT hRes;

  if (lpInternalData == NULL || asymcipher_data->cEncryptor.bInUse == FALSE)
    return MX_E_NotReady;

  lpOut = asymcipher_data->cEncryptor.cOutputBuffer.WriteReserve(EVP_MAX_BLOCK_LENGTH + BLOCK_SIZE);
  if (lpOut == NULL)
  {
    asymcipher_data->cEncryptor.Reset(TRUE);
    return E_OUTOFMEMORY;
  }

  ERR_clear_error();
  if (asymcipher_data->cEncryptor.bUsePrivateKey != FALSE)
  {
    res = RSA_private_encrypt((int)(asymcipher_data->cEncryptor.cInputBuffer.GetLength()),
                              asymcipher_data->cEncryptor.cInputBuffer.GetBuffer(),
                              lpOut, EVP_PKEY_get0_RSA(asymcipher_data->lpKey),
                              asymcipher_data->cEncryptor.nPaddingMode);
  }
  else
  {
    res = RSA_public_encrypt((int)(asymcipher_data->cEncryptor.cInputBuffer.GetLength()),
                             asymcipher_data->cEncryptor.cInputBuffer.GetBuffer(),
                             lpOut, EVP_PKEY_get0_RSA(asymcipher_data->lpKey),
                             asymcipher_data->cEncryptor.nPaddingMode);
  }
  if (res <= 0)
  {
    hRes = Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
    asymcipher_data->cEncryptor.Reset(TRUE);
    return hRes;
  }

  asymcipher_data->cEncryptor.cOutputBuffer.EndWriteReserve((SIZE_T)res);
  asymcipher_data->cEncryptor.Reset(FALSE);

  //done
  return S_OK;
}

SIZE_T CAsymmetricCipher::GetAvailableEncryptedData() const
{
  return (lpInternalData != NULL) ? asymcipher_data->cEncryptor.cOutputBuffer.GetLength() : 0;
}

SIZE_T CAsymmetricCipher::GetEncryptedData(_Out_writes_(nDestSize) LPVOID lpDest, _In_ SIZE_T nDestSize)
{
  if (lpInternalData == NULL)
    return 0;
  return asymcipher_data->cEncryptor.cOutputBuffer.Read(lpDest, nDestSize);
}

HRESULT CAsymmetricCipher::BeginDecrypt(_In_opt_ MX::CAsymmetricCipher::ePadding nPadding, _In_opt_ BOOL bUsePrivateKey)
{
  HRESULT hRes;

  hRes = InternalInitialize();
  if (FAILED(hRes))
    return hRes;

  if (asymcipher_data->lpKey == NULL)
    return MX_E_NotReady;

  //create context
  asymcipher_data->cDecryptor.Reset(TRUE);
  asymcipher_data->cDecryptor.bInUse = TRUE;

  switch (EVP_PKEY_base_id(asymcipher_data->lpKey))
  {
    case EVP_PKEY_RSA:
      switch (nPadding)
      {
        case MX::CAsymmetricCipher::PaddingNone:
          asymcipher_data->cDecryptor.nPaddingMode = RSA_PKCS1_PADDING;
          asymcipher_data->cDecryptor.nPaddingSize = 0;
          break;

        case MX::CAsymmetricCipher::PaddingPKCS1:
          asymcipher_data->cDecryptor.nPaddingMode = RSA_PKCS1_PADDING;
          asymcipher_data->cDecryptor.nPaddingSize = 11;
          break;

        case MX::CAsymmetricCipher::PaddingOAEP:
          asymcipher_data->cDecryptor.nPaddingMode = RSA_PKCS1_OAEP_PADDING;
          asymcipher_data->cDecryptor.nPaddingSize = 41;
          break;

        case MX::CAsymmetricCipher::PaddingSSLV23:
          asymcipher_data->cDecryptor.nPaddingMode = RSA_SSLV23_PADDING;
          asymcipher_data->cDecryptor.nPaddingSize = 11;
          break;

        default:
          asymcipher_data->cDecryptor.Reset(TRUE);
          return E_INVALIDARG;
      }

      asymcipher_data->cDecryptor.bUsePrivateKey = bUsePrivateKey;
      break;

    default:
      //cannot encrypt with a non-rsa key
      asymcipher_data->cDecryptor.Reset(TRUE);
      return MX_E_Unsupported;
  }

  //empty buffer
  asymcipher_data->cDecryptor.CleanOutputBuffers();

  //done
  return S_OK;
}

HRESULT CAsymmetricCipher::DecryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)
{
  SIZE_T nMaxWritable;
  HRESULT hRes;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (lpInternalData == NULL || asymcipher_data->cDecryptor.bInUse == FALSE)
    return MX_E_NotReady;

  nMaxWritable = EVP_PKEY_size(asymcipher_data->lpKey) - asymcipher_data->cDecryptor.nPaddingSize -
                 asymcipher_data->cDecryptor.cInputBuffer.GetLength();
  if (nDataLength > nMaxWritable)
  {
    asymcipher_data->cDecryptor.Reset(TRUE);
    return MX_E_InvalidData;
  }

  hRes = asymcipher_data->cDecryptor.cInputBuffer.WriteStream(lpData, nDataLength);
  if (FAILED(hRes))
  {
    asymcipher_data->cDecryptor.Reset(TRUE);
    return hRes;
  }

  //done
  return S_OK;
}

HRESULT CAsymmetricCipher::EndDecrypt()
{
  LPBYTE lpOut;
  int res;
  HRESULT hRes;

  if (lpInternalData == NULL || asymcipher_data->cDecryptor.bInUse == FALSE)
    return MX_E_NotReady;

  lpOut = asymcipher_data->cDecryptor.cOutputBuffer.WriteReserve(EVP_MAX_BLOCK_LENGTH + BLOCK_SIZE);
  if (lpOut == NULL)
  {
    asymcipher_data->cDecryptor.Reset(TRUE);
    return E_OUTOFMEMORY;
  }

  ERR_clear_error();
  if (asymcipher_data->cDecryptor.bUsePrivateKey != FALSE)
  {
    res = RSA_private_decrypt((int)(asymcipher_data->cDecryptor.cInputBuffer.GetLength()),
                              asymcipher_data->cDecryptor.cInputBuffer.GetBuffer(),
                              lpOut, EVP_PKEY_get0_RSA(asymcipher_data->lpKey),
                              asymcipher_data->cDecryptor.nPaddingMode);
  }
  else
  {
    res = RSA_public_decrypt((int)(asymcipher_data->cDecryptor.cInputBuffer.GetLength()),
                             asymcipher_data->cDecryptor.cInputBuffer.GetBuffer(),
                             lpOut, EVP_PKEY_get0_RSA(asymcipher_data->lpKey),
                             asymcipher_data->cDecryptor.nPaddingMode);
  }
  if (res <= 0)
  {
    hRes = Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
    asymcipher_data->cDecryptor.Reset(TRUE);
    return hRes;
  }

  asymcipher_data->cEncryptor.cOutputBuffer.EndWriteReserve((SIZE_T)res);
  asymcipher_data->cEncryptor.Reset(FALSE);

  //done
  return S_OK;
}

SIZE_T CAsymmetricCipher::GetAvailableDecryptedData() const
{
  return (lpInternalData != NULL) ? asymcipher_data->cDecryptor.cOutputBuffer.GetLength() : 0;
}

SIZE_T CAsymmetricCipher::GetDecryptedData(_Out_writes_(nDestSize) LPVOID lpDest, _In_ SIZE_T nDestSize)
{
  if (lpInternalData == NULL)
    return 0;
  return asymcipher_data->cDecryptor.cOutputBuffer.Read(lpDest, nDestSize);
}

HRESULT CAsymmetricCipher::BeginSign(_In_ MX::CMessageDigest::eAlgorithm nAlgorithm)
{
  HRESULT hRes;

  hRes = InternalInitialize();
  if (FAILED(hRes))
    return hRes;

  if (asymcipher_data->lpKey == NULL)
    return MX_E_NotReady;

  //create context
  asymcipher_data->cSigner.Reset(TRUE);
  asymcipher_data->cSigner.bInUse = TRUE;

  //initialize
  hRes = asymcipher_data->cSigner.cDigest.BeginDigest(nAlgorithm);
  if (FAILED(hRes))
  {
    asymcipher_data->cSigner.Reset(TRUE);
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

  if (lpInternalData == NULL || asymcipher_data->cSigner.bInUse == FALSE)
    return MX_E_NotReady;

  hRes = asymcipher_data->cSigner.cDigest.DigestStream(lpData, nDataLength);
  if (FAILED(hRes))
  {
    asymcipher_data->cSigner.Reset(TRUE);
    return hRes;
  }

  //done
  return S_OK;
}

HRESULT CAsymmetricCipher::EndSign()
{
  EVP_PKEY_CTX *lpKeyCtx;
  HRESULT hRes;

  if (lpInternalData == NULL || asymcipher_data->cSigner.bInUse == FALSE)
    return MX_E_NotReady;

  hRes = asymcipher_data->cSigner.cDigest.EndDigest();
  if (FAILED(hRes))
  {
    asymcipher_data->cSigner.Reset(TRUE);
    return hRes;
  }

  lpKeyCtx = EVP_PKEY_CTX_new(asymcipher_data->lpKey, NULL);
  if (lpKeyCtx != NULL)
  {
    int res;

    res = EVP_PKEY_sign_init(lpKeyCtx);
    if (res > 0)
    {
      LPBYTE lpSig;
      size_t nSigSize;

      nSigSize = (size_t)EVP_PKEY_size(asymcipher_data->lpKey);

      lpSig = asymcipher_data->cSigner.cOutputBuffer.WriteReserve(nSigSize);
      if (lpSig != NULL)
      {
        res = EVP_PKEY_sign(lpKeyCtx, (unsigned char*)lpSig, &nSigSize, asymcipher_data->cSigner.cDigest.GetResult(),
                            asymcipher_data->cSigner.cDigest.GetResultSize());
        if (res > 0)
        {
          asymcipher_data->cSigner.cOutputBuffer.EndWriteReserve((SIZE_T)res);
        }
        else if (res == -2)
        {
          hRes = MX_E_Unsupported;
        }
        else
        {
          hRes = Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
        }
      }
      else
      {
        hRes = E_OUTOFMEMORY;
      }
    }
    else
    {
      hRes = Internals::OpenSSL::GetLastErrorCode(MX_E_Unsupported);
    }

    EVP_PKEY_CTX_free(lpKeyCtx);
  }
  else
  {
    hRes = E_OUTOFMEMORY;
  }

  if (FAILED(hRes))
  {
    asymcipher_data->cSigner.Reset(TRUE);
    return hRes;
  }
  
  asymcipher_data->cSigner.Reset(FALSE);

  //done
  return S_OK;
}

LPBYTE CAsymmetricCipher::GetSignature() const
{
  if (lpInternalData == NULL)
    return NULL;
  return asymcipher_data->cSigner.cOutputBuffer.GetBuffer();
}

SIZE_T CAsymmetricCipher::GetSignatureSize() const
{
  if (lpInternalData == NULL)
    return 0;
  return asymcipher_data->cSigner.cOutputBuffer.GetLength();
}

HRESULT CAsymmetricCipher::BeginVerify(_In_ MX::CMessageDigest::eAlgorithm nAlgorithm)
{
  HRESULT hRes;

  hRes = InternalInitialize();
  if (FAILED(hRes))
    return hRes;

  if (asymcipher_data->lpKey == NULL)
    return MX_E_NotReady;

  //create context
  asymcipher_data->cVerifier.Reset(TRUE);
  asymcipher_data->cVerifier.bInUse = TRUE;

  //initialize
  hRes = asymcipher_data->cVerifier.cDigest.BeginDigest(nAlgorithm);
  if (FAILED(hRes))
  {
    asymcipher_data->cVerifier.Reset(TRUE);
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

  if (lpInternalData == NULL || asymcipher_data->cVerifier.bInUse == FALSE)
    return MX_E_NotReady;

  hRes = asymcipher_data->cVerifier.cDigest.DigestStream(lpData, nDataLength);
  if (FAILED(hRes))
  {
    asymcipher_data->cVerifier.Reset(TRUE);
    return hRes;
  }

  //done
  return S_OK;
}

HRESULT CAsymmetricCipher::EndVerify(_In_ LPCVOID lpSignature, _In_ SIZE_T nSignatureLen)
{
  EVP_PKEY_CTX *lpKeyCtx;
  HRESULT hRes;

  if (lpSignature == NULL)
    return E_POINTER;
  if (nSignatureLen == 0)
    return E_INVALIDARG;

  if (lpInternalData == NULL || asymcipher_data->cVerifier.bInUse == FALSE)
    return MX_E_NotReady;

  hRes = asymcipher_data->cVerifier.cDigest.EndDigest();
  if (FAILED(hRes))
  {
    asymcipher_data->cVerifier.Reset(TRUE);
    return hRes;
  }

  lpKeyCtx = EVP_PKEY_CTX_new(asymcipher_data->lpKey, NULL);
  if (lpKeyCtx != NULL)
  {
    int res;

    res = EVP_PKEY_sign_init(lpKeyCtx);
    if (res > 0)
    {
      res = EVP_PKEY_verify(lpKeyCtx, (unsigned char *)lpSignature, nSignatureLen,
                            asymcipher_data->cVerifier.cDigest.GetResult(),
                            asymcipher_data->cVerifier.cDigest.GetResultSize());
      if (res > 0)
      {
        hRes = S_OK;
      }
      else if (res == 0)
      {
        hRes = S_FALSE;
      }
      else if (res == -2)
      {
        hRes = MX_E_Unsupported;
      }
      else
      {
        hRes = Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
      }
    }
    else
    {
      hRes = Internals::OpenSSL::GetLastErrorCode(MX_E_Unsupported);
    }

    EVP_PKEY_CTX_free(lpKeyCtx);
  }
  else
  {
    hRes = E_OUTOFMEMORY;
  }

  asymcipher_data->cVerifier.Reset(TRUE);

  //done
  return hRes;
}

HRESULT CAsymmetricCipher::InternalInitialize()
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
  //done
  return S_OK;
}

} //namespace MX
