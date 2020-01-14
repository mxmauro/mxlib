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
#include "..\..\Include\Crypto\CryptoDES.h"
#include "InitOpenSSL.h"
#include <OpenSSL\evp.h>

//-----------------------------------------------------------

#define BLOCK_SIZE                                      2048

//-----------------------------------------------------------

typedef struct tagDES_DATA {
  struct tagCipherInfo {
    EVP_CIPHER_CTX *lpCipherCtx;
    const EVP_CIPHER *lpCipher;
    LPBYTE lpTempOutputBuf;
    SIZE_T nTempOutputSize;
  } sEncryptor, sDecryptor;
} DES_DATA;

#define des_data ((DES_DATA*)lpInternalData)

//-----------------------------------------------------------

static const EVP_CIPHER* GetCipher(_In_ MX::CCryptoDES::eAlgorithm nAlgorithm);

//-----------------------------------------------------------

namespace MX {

CCryptoDES::CCryptoDES() : CCryptoBase(), CNonCopyableObj()
{
  lpInternalData = NULL;
  return;
}

CCryptoDES::~CCryptoDES()
{
  if (lpInternalData != NULL)
  {
    CleanUp(FALSE, TRUE);
    CleanUp(TRUE, TRUE);
    MxMemSet(lpInternalData, 0, sizeof(DES_DATA));
    MX_FREE(lpInternalData);
  }
  return;
}

HRESULT CCryptoDES::BeginEncrypt()
{
  return BeginEncrypt(AlgorithmTripleDesEcb, NULL, 0, NULL, TRUE);
}

HRESULT CCryptoDES::BeginEncrypt(_In_ eAlgorithm nAlgorithm, _In_opt_ LPCVOID lpKey, _In_opt_ SIZE_T nKeyLen,
                                 _In_opt_ LPCVOID lpInitVector, _In_opt_ BOOL bUsePadding)
{
  HRESULT hRes;

  if (nAlgorithm != AlgorithmDesEcb && nAlgorithm != AlgorithmDesCbc && nAlgorithm != AlgorithmDesCfb &&
      nAlgorithm != AlgorithmDesOfb && nAlgorithm != AlgorithmDesCfb1 && nAlgorithm != AlgorithmDesCfb8 &&
      nAlgorithm != AlgorithmTripleDesEcb && nAlgorithm != AlgorithmTripleDesCbc &&
      nAlgorithm != AlgorithmTripleDesCfb && nAlgorithm != AlgorithmTripleDesOfb &&
      nAlgorithm != AlgorithmTripleDesCfb1 && nAlgorithm != AlgorithmTripleDesCfb8)
  {
    return E_INVALIDARG;
  }
  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;
  if (lpInternalData == NULL)
  {
    lpInternalData = MX_MALLOC(sizeof(DES_DATA));
    if (lpInternalData == NULL)
      return E_OUTOFMEMORY;
    MxMemSet(lpInternalData, 0, sizeof(DES_DATA));
  }
  CleanUp(TRUE, TRUE);
  //create context
  des_data->sEncryptor.lpCipherCtx = EVP_CIPHER_CTX_new();
  if (des_data->sEncryptor.lpCipherCtx == NULL)
  {
    CleanUp(TRUE, TRUE);
    return E_OUTOFMEMORY;
  }
  //get encryptor manager
  des_data->sEncryptor.lpCipher = GetCipher(nAlgorithm);
  if (des_data->sEncryptor.lpCipher == NULL)
  {
    CleanUp(TRUE, TRUE);
    return E_NOTIMPL;
  }
  //verify key length
  if (EVP_CIPHER_key_length(des_data->sEncryptor.lpCipher) > 0)
  {
    if (lpKey == NULL)
    {
      CleanUp(TRUE, TRUE);
      return E_POINTER;
    }
    if (nKeyLen != (SIZE_T)EVP_CIPHER_key_length(des_data->sEncryptor.lpCipher))
    {
      CleanUp(TRUE, TRUE);
      return E_INVALIDARG;
    }
  }
  //verify if IV is present
  if (EVP_CIPHER_iv_length(des_data->sEncryptor.lpCipher) > 0)
  {
    if (lpInitVector == NULL)
    {
      CleanUp(TRUE, TRUE);
      return E_POINTER;
    }
  }
  //create temp output buffer
  des_data->sEncryptor.nTempOutputSize = BLOCK_SIZE + (SIZE_T)EVP_CIPHER_block_size(des_data->sEncryptor.lpCipher);
  des_data->sEncryptor.lpTempOutputBuf = (LPBYTE)MX_MALLOC(des_data->sEncryptor.nTempOutputSize);
  if (des_data->sEncryptor.lpTempOutputBuf == NULL)
  {
    CleanUp(TRUE, TRUE);
    return E_OUTOFMEMORY;
  }
  //initialize
  if (EVP_EncryptInit(des_data->sEncryptor.lpCipherCtx, des_data->sEncryptor.lpCipher, (const unsigned char*)lpKey,
                      (const unsigned char*)lpInitVector) <= 0)
  {
    CleanUp(TRUE, TRUE);
    return E_FAIL;
  }
  EVP_CIPHER_CTX_set_padding(des_data->sEncryptor.lpCipherCtx, (bUsePadding != FALSE) ? 1 : 0);
  //empty buffer
  cEncryptedData.SetBufferSize(0);
  //done
  return S_OK;
}

HRESULT CCryptoDES::EncryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)
{
  LPBYTE lpIn;
  int nInSize, nOutSize;
  HRESULT hRes;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (lpInternalData == NULL || des_data->sEncryptor.lpCipherCtx == NULL)
    return MX_E_NotReady;
  lpIn = (LPBYTE)lpData;
  while (nDataLength > 0)
  {
    nInSize = (nDataLength > BLOCK_SIZE) ? BLOCK_SIZE : (int)nDataLength;
    if (EVP_EncryptUpdate(des_data->sEncryptor.lpCipherCtx, des_data->sEncryptor.lpTempOutputBuf, &nOutSize,
                          lpIn, nInSize) <= 0)
    {
      CleanUp(TRUE, TRUE);
      return MX_E_InvalidData;
    }
    lpIn += nInSize;
    nDataLength -= nInSize;
    if (nOutSize > 0)
    {
      hRes = cEncryptedData.Write(des_data->sEncryptor.lpTempOutputBuf, (SIZE_T)nOutSize);
      if (FAILED(hRes))
      {
        CleanUp(TRUE, TRUE);
        return hRes;
      }
    }
  }
  return S_OK;
}

HRESULT CCryptoDES::EndEncrypt()
{
  int nOutSize;
  HRESULT hRes;

  if (lpInternalData == NULL || des_data->sEncryptor.lpCipherCtx == NULL)
    return MX_E_NotReady;
  if (EVP_EncryptFinal_ex(des_data->sEncryptor.lpCipherCtx, des_data->sEncryptor.lpTempOutputBuf, &nOutSize) <= 0)
  {
    CleanUp(TRUE, TRUE);
    return MX_E_InvalidData;
  }
  if (nOutSize > 0)
  {
    hRes = cEncryptedData.Write(des_data->sEncryptor.lpTempOutputBuf, (SIZE_T)nOutSize);
    if (FAILED(hRes))
    {
      CleanUp(TRUE, TRUE);
      return hRes;
    }
  }
  CleanUp(TRUE, FALSE);
  return S_OK;
}

HRESULT CCryptoDES::BeginDecrypt()
{
  return BeginDecrypt(AlgorithmTripleDesEcb, NULL, 0, NULL, TRUE);
}

HRESULT CCryptoDES::BeginDecrypt(_In_ eAlgorithm nAlgorithm, _In_opt_ LPCVOID lpKey, _In_opt_ SIZE_T nKeyLen,
                                 _In_opt_ LPCVOID lpInitVector, _In_opt_ BOOL bUsePadding)
{
  HRESULT hRes;

  if (nAlgorithm != AlgorithmDesEcb && nAlgorithm != AlgorithmDesCbc && nAlgorithm != AlgorithmDesCfb &&
      nAlgorithm != AlgorithmDesOfb && nAlgorithm != AlgorithmDesCfb1 && nAlgorithm != AlgorithmDesCfb8 &&
      nAlgorithm != AlgorithmTripleDesEcb && nAlgorithm != AlgorithmTripleDesCbc &&
      nAlgorithm != AlgorithmTripleDesCfb && nAlgorithm != AlgorithmTripleDesOfb &&
      nAlgorithm != AlgorithmTripleDesCfb1 && nAlgorithm != AlgorithmTripleDesCfb8)
  {
    return E_INVALIDARG;
  }
  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;
  if (lpInternalData == NULL)
  {
    lpInternalData = MX_MALLOC(sizeof(DES_DATA));
    if (lpInternalData == NULL)
      return E_OUTOFMEMORY;
    MxMemSet(lpInternalData, 0, sizeof(DES_DATA));
  }
  CleanUp(FALSE, TRUE);
  //create context
  des_data->sDecryptor.lpCipherCtx = EVP_CIPHER_CTX_new();
  if (des_data->sDecryptor.lpCipherCtx == NULL)
  {
    CleanUp(FALSE, TRUE);
    return E_OUTOFMEMORY;
  }
  //get decryptor manager
  des_data->sDecryptor.lpCipher = GetCipher(nAlgorithm);
  if (des_data->sDecryptor.lpCipher == NULL)
  {
    CleanUp(FALSE, TRUE);
    return E_NOTIMPL;
  }
  //verify key length
  if (EVP_CIPHER_key_length(des_data->sDecryptor.lpCipher) > 0)
  {
    if (lpKey == NULL)
    {
      CleanUp(FALSE, TRUE);
      return E_POINTER;
    }
    if (nKeyLen != (SIZE_T)EVP_CIPHER_key_length(des_data->sDecryptor.lpCipher))
    {
      CleanUp(FALSE, TRUE);
      return E_INVALIDARG;
    }
  }
  //verify if IV is present
  if (EVP_CIPHER_iv_length(des_data->sDecryptor.lpCipher) > 0)
  {
    if (lpInitVector == NULL)
    {
      CleanUp(FALSE, TRUE);
      return E_POINTER;
    }
  }
  //create temp output buffer
  des_data->sDecryptor.nTempOutputSize = BLOCK_SIZE + (SIZE_T)EVP_CIPHER_block_size(des_data->sDecryptor.lpCipher);
  des_data->sDecryptor.lpTempOutputBuf = (LPBYTE)MX_MALLOC(des_data->sDecryptor.nTempOutputSize);
  if (des_data->sDecryptor.lpTempOutputBuf == NULL)
  {
    CleanUp(FALSE, TRUE);
    return E_OUTOFMEMORY;
  }
  //initialize
  if (EVP_DecryptInit(des_data->sDecryptor.lpCipherCtx, des_data->sDecryptor.lpCipher, (const unsigned char*)lpKey,
                      (const unsigned char*)lpInitVector) <= 0)
  {
    CleanUp(TRUE, TRUE);
    return E_FAIL;
  }
  EVP_CIPHER_CTX_set_padding(des_data->sDecryptor.lpCipherCtx, (bUsePadding != FALSE) ? 1 : 0);
  //empty buffer
  cDecryptedData.SetBufferSize(0);
  //done
  return S_OK;
}

HRESULT CCryptoDES::DecryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)
{
  LPBYTE lpIn;
  int nInSize, nOutSize;
  HRESULT hRes;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (lpInternalData == NULL || des_data->sDecryptor.lpCipherCtx == NULL)
    return MX_E_NotReady;
  lpIn = (LPBYTE)lpData;
  while (nDataLength > 0)
  {
    nInSize = (nDataLength > BLOCK_SIZE) ? BLOCK_SIZE : (int)nDataLength;
    if (EVP_DecryptUpdate(des_data->sDecryptor.lpCipherCtx, des_data->sDecryptor.lpTempOutputBuf, &nOutSize,
                          lpIn, nInSize) <= 0)
    {
      CleanUp(FALSE, TRUE);
      return MX_E_InvalidData;
    }
    lpIn += nInSize;
    nDataLength -= nInSize;
    if (nOutSize > 0)
    {
      hRes = cDecryptedData.Write(des_data->sDecryptor.lpTempOutputBuf, (SIZE_T)nOutSize);
      if (FAILED(hRes))
      {
        CleanUp(FALSE, TRUE);
        return hRes;
      }
    }
  }
  return S_OK;
}

HRESULT CCryptoDES::EndDecrypt()
{
  int nOutSize;
  HRESULT hRes;

  if (lpInternalData == NULL || des_data->sDecryptor.lpCipherCtx == NULL)
    return MX_E_NotReady;
  if (EVP_DecryptFinal_ex(des_data->sDecryptor.lpCipherCtx, des_data->sDecryptor.lpTempOutputBuf, &nOutSize) <= 0)
  {
    CleanUp(FALSE, TRUE);
    return MX_E_InvalidData;
  }
  if (nOutSize > 0)
  {
    hRes = cDecryptedData.Write(des_data->sDecryptor.lpTempOutputBuf, (SIZE_T)nOutSize);
    if (FAILED(hRes))
    {
      CleanUp(FALSE, TRUE);
      return hRes;
    }
  }
  CleanUp(FALSE, FALSE);
  return S_OK;
}

VOID CCryptoDES::CleanUp(_In_ BOOL bEncoder, _In_ BOOL bZeroData)
{
  struct tagDES_DATA::tagCipherInfo *lpInfo = (bEncoder != FALSE) ? &(des_data->sEncryptor) : &(des_data->sDecryptor);

  if (lpInfo->lpTempOutputBuf != NULL)
    MxMemSet(lpInfo->lpTempOutputBuf, 0, lpInfo->nTempOutputSize);
  MX_FREE(lpInfo->lpTempOutputBuf);
  lpInfo->nTempOutputSize = 0;
  //----
  if (lpInfo->lpCipherCtx != NULL)
  {
    EVP_CIPHER_CTX_free(lpInfo->lpCipherCtx);
    lpInfo->lpCipherCtx = NULL;
  }
  //----
  lpInfo->lpCipher = NULL;
  //----
  if (bZeroData != FALSE)
  {
    LPBYTE lpPtr1, lpPtr2;
    SIZE_T nSize1, nSize2;

    if (bEncoder != FALSE)
      cEncryptedData.GetReadPtr(&lpPtr1, &nSize1, &lpPtr2, &nSize2);
    else
      cDecryptedData.GetReadPtr(&lpPtr1, &nSize1, &lpPtr2, &nSize2);
    MxMemSet(lpPtr1, 0, nSize1);
    MxMemSet(lpPtr2, 0, nSize2);
  }
  return;
}

} //namespace MX

//-----------------------------------------------------------

static const EVP_CIPHER* GetCipher(_In_ MX::CCryptoDES::eAlgorithm nAlgorithm)
{
  switch (nAlgorithm)
  {
    case MX::CCryptoDES::AlgorithmDesEcb:
      return EVP_get_cipherbyname("DES-ECB");
    case MX::CCryptoDES::AlgorithmDesCbc:
      return EVP_get_cipherbyname("DES-CBC");
    case MX::CCryptoDES::AlgorithmDesCfb:
      return EVP_get_cipherbyname("DES-CFB");
    case MX::CCryptoDES::AlgorithmDesOfb:
      return EVP_get_cipherbyname("DES-OFB");
    case MX::CCryptoDES::AlgorithmDesCfb1:
      return EVP_get_cipherbyname("DES-CFB1");
    case MX::CCryptoDES::AlgorithmDesCfb8:
      return EVP_get_cipherbyname("DES-CFB8");
    case MX::CCryptoDES::AlgorithmTripleDesEcb:
      return EVP_get_cipherbyname("DES-EDE3");
    case MX::CCryptoDES::AlgorithmTripleDesCbc:
      return EVP_get_cipherbyname("DES-EDE3-CBC");
    case MX::CCryptoDES::AlgorithmTripleDesCfb:
      return EVP_get_cipherbyname("DES-EDE3-CFB");
    case MX::CCryptoDES::AlgorithmTripleDesOfb:
      return EVP_get_cipherbyname("DES-EDE3-OFB");
    case MX::CCryptoDES::AlgorithmTripleDesCfb1:
      return EVP_get_cipherbyname("DES-EDE3-CFB1");
    case MX::CCryptoDES::AlgorithmTripleDesCfb8:
      return EVP_get_cipherbyname("DES-EDE3-CFB8");
  }
  return NULL;
}
