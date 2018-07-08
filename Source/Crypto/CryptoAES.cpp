/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2006-2015. All rights reserved.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 *
 * 2. This notice may not be removed or altered from any source distribution.
 *
 * 3. YOU MAY NOT:
 *
 *    a. Modify, translate, adapt, alter, or create derivative works from
 *       this software.
 *    b. Copy (other than one back-up copy), distribute, publicly display,
 *       transmit, sell, rent, lease or otherwise exploit this software.
 *    c. Distribute, sub-license, rent, lease, loan [or grant any third party
 *       access to or use of the software to any third party.
 **/
#include "..\..\Include\Crypto\CryptoAES.h"
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
} AES_DATA;

#define aes_data ((AES_DATA*)lpInternalData)

//-----------------------------------------------------------

static const EVP_CIPHER* GetCipher(_In_ MX::CCryptoAES::eAlgorithm nAlgorithm);

//-----------------------------------------------------------

namespace MX {

CCryptoAES::CCryptoAES() : CBaseCrypto()
{
  lpInternalData = NULL;
  return;
}

CCryptoAES::~CCryptoAES()
{
  if (lpInternalData != NULL)
  {
    CleanUp(FALSE, TRUE);
    CleanUp(TRUE, TRUE);
    MemSet(lpInternalData, 0, sizeof(AES_DATA));
    MX_FREE(lpInternalData);
  }
  return;
}

HRESULT CCryptoAES::BeginEncrypt()
{
  return BeginEncrypt(AlgorithmAes256Ecb, NULL, 0, NULL, TRUE);
}

HRESULT CCryptoAES::BeginEncrypt(_In_ eAlgorithm nAlgorithm, _In_opt_ LPCVOID lpKey, _In_opt_ SIZE_T nKeyLen,
                                 _In_opt_ LPCVOID lpInitVector, _In_opt_ BOOL bUsePadding)
{
  HRESULT hRes;

  if (nAlgorithm != AlgorithmAes128Ecb && nAlgorithm != AlgorithmAes128Cbc && nAlgorithm != AlgorithmAes128Cfb &&
      nAlgorithm != AlgorithmAes128Ofb && nAlgorithm != AlgorithmAes128Cfb1 && nAlgorithm != AlgorithmAes128Cfb8 &&
      nAlgorithm != AlgorithmAes192Ecb && nAlgorithm != AlgorithmAes192Cbc && nAlgorithm != AlgorithmAes192Cfb &&
      nAlgorithm != AlgorithmAes192Ofb && nAlgorithm != AlgorithmAes192Cfb1 && nAlgorithm != AlgorithmAes192Cfb8 &&
      nAlgorithm != AlgorithmAes256Ecb && nAlgorithm != AlgorithmAes256Cbc && nAlgorithm != AlgorithmAes256Cfb &&
      nAlgorithm != AlgorithmAes256Ofb && nAlgorithm != AlgorithmAes256Cfb1 && nAlgorithm != AlgorithmAes256Cfb8)
  {
    return E_INVALIDARG;
  }
  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;
  if (lpInternalData == NULL)
  {
    lpInternalData = MX_MALLOC(sizeof(AES_DATA));
    if (lpInternalData == NULL)
      return E_OUTOFMEMORY;
    MemSet(lpInternalData, 0, sizeof(AES_DATA));
  }
  CleanUp(TRUE, TRUE);
  //create context
  aes_data->sEncryptor.lpCipherCtx = EVP_CIPHER_CTX_new();
  if (aes_data->sEncryptor.lpCipherCtx == NULL)
  {
    CleanUp(TRUE, TRUE);
    return E_OUTOFMEMORY;
  }
  //get encryptor manager
  aes_data->sEncryptor.lpCipher = GetCipher(nAlgorithm);
  if (aes_data->sEncryptor.lpCipher == NULL)
  {
    CleanUp(TRUE, TRUE);
    return E_NOTIMPL;
  }
  //verify key length
  if (EVP_CIPHER_key_length(aes_data->sEncryptor.lpCipher) > 0)
  {
    if (lpKey == NULL)
    {
      CleanUp(TRUE, TRUE);
      return E_POINTER;
    }
    if (nKeyLen != (SIZE_T)EVP_CIPHER_key_length(aes_data->sEncryptor.lpCipher))
    {
      CleanUp(TRUE, TRUE);
      return E_INVALIDARG;
    }
  }
  //verify if IV is present
  if (EVP_CIPHER_iv_length(aes_data->sEncryptor.lpCipher) > 0)
  {
    if (lpInitVector == NULL)
    {
      CleanUp(TRUE, TRUE);
      return E_POINTER;
    }
  }
  //create temp output buffer
  aes_data->sEncryptor.nTempOutputSize = BLOCK_SIZE + (SIZE_T)EVP_CIPHER_block_size(aes_data->sEncryptor.lpCipher);
  aes_data->sEncryptor.lpTempOutputBuf = (LPBYTE)MX_MALLOC(aes_data->sEncryptor.nTempOutputSize);
  if (aes_data->sEncryptor.lpTempOutputBuf == NULL)
  {
    CleanUp(TRUE, TRUE);
    return E_OUTOFMEMORY;
  }
  //initialize
  if (EVP_EncryptInit(aes_data->sEncryptor.lpCipherCtx, aes_data->sEncryptor.lpCipher, (const unsigned char*)lpKey,
                      (const unsigned char*)lpInitVector) <= 0)
  {
    CleanUp(TRUE, TRUE);
    return E_FAIL;
  }
  EVP_CIPHER_CTX_set_padding(aes_data->sEncryptor.lpCipherCtx, (bUsePadding != FALSE) ? 1 : 0);
  //empty buffer
  cEncryptedData.SetBufferSize(0);
  //done
  return S_OK;
}

HRESULT CCryptoAES::EncryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)
{
  LPBYTE lpIn;
  int nInSize, nOutSize;
  HRESULT hRes;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (lpInternalData == NULL || aes_data->sEncryptor.lpCipherCtx == NULL)
    return MX_E_NotReady;
  lpIn = (LPBYTE)lpData;
  while (nDataLength > 0)
  {
    nInSize = (nDataLength > BLOCK_SIZE) ? BLOCK_SIZE : (int)nDataLength;
    if (EVP_EncryptUpdate(aes_data->sEncryptor.lpCipherCtx, aes_data->sEncryptor.lpTempOutputBuf, &nOutSize,
                          lpIn, nInSize) <= 0)
    {
      CleanUp(TRUE, TRUE);
      return MX_E_InvalidData;
    }
    lpIn += nInSize;
    nDataLength -= nInSize;
    if (nOutSize > 0)
    {
      hRes = cEncryptedData.Write(aes_data->sEncryptor.lpTempOutputBuf, (SIZE_T)nOutSize);
      if (FAILED(hRes))
      {
        CleanUp(TRUE, TRUE);
        return hRes;
      }
    }
  }
  return S_OK;
}

HRESULT CCryptoAES::EndEncrypt()
{
  int nOutSize;
  HRESULT hRes;

  if (lpInternalData == NULL || aes_data->sEncryptor.lpCipherCtx == NULL)
    return MX_E_NotReady;
  if (EVP_EncryptFinal_ex(aes_data->sEncryptor.lpCipherCtx, aes_data->sEncryptor.lpTempOutputBuf, &nOutSize) <= 0)
  {
    CleanUp(TRUE, TRUE);
    return MX_E_InvalidData;
  }
  if (nOutSize > 0)
  {
    hRes = cEncryptedData.Write(aes_data->sEncryptor.lpTempOutputBuf, (SIZE_T)nOutSize);
    if (FAILED(hRes))
    {
      CleanUp(TRUE, TRUE);
      return hRes;
    }
  }
  CleanUp(TRUE, FALSE);
  return S_OK;
}

HRESULT CCryptoAES::BeginDecrypt()
{
  return BeginDecrypt(AlgorithmAes256Ecb, NULL, 0, NULL, TRUE);
}

HRESULT CCryptoAES::BeginDecrypt(_In_ eAlgorithm nAlgorithm, _In_opt_ LPCVOID lpKey, _In_opt_ SIZE_T nKeyLen,
                                 _In_opt_ LPCVOID lpInitVector, _In_opt_ BOOL bUsePadding)
{
  HRESULT hRes;

  if (nAlgorithm != AlgorithmAes128Ecb && nAlgorithm != AlgorithmAes128Cbc && nAlgorithm != AlgorithmAes128Cfb &&
      nAlgorithm != AlgorithmAes128Ofb && nAlgorithm != AlgorithmAes128Cfb1 && nAlgorithm != AlgorithmAes128Cfb8 &&
      nAlgorithm != AlgorithmAes192Ecb && nAlgorithm != AlgorithmAes192Cbc && nAlgorithm != AlgorithmAes192Cfb &&
      nAlgorithm != AlgorithmAes192Ofb && nAlgorithm != AlgorithmAes192Cfb1 && nAlgorithm != AlgorithmAes192Cfb8 &&
      nAlgorithm != AlgorithmAes256Ecb && nAlgorithm != AlgorithmAes256Cbc && nAlgorithm != AlgorithmAes256Cfb &&
      nAlgorithm != AlgorithmAes256Ofb && nAlgorithm != AlgorithmAes256Cfb1 && nAlgorithm != AlgorithmAes256Cfb8)
  {
    return E_INVALIDARG;
  }
  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;
  if (lpInternalData == NULL)
  {
    lpInternalData = MX_MALLOC(sizeof(AES_DATA));
    if (lpInternalData == NULL)
      return E_OUTOFMEMORY;
    MemSet(lpInternalData, 0, sizeof(AES_DATA));
  }
  CleanUp(FALSE, TRUE);
  //create context
  aes_data->sDecryptor.lpCipherCtx = EVP_CIPHER_CTX_new();
  if (aes_data->sDecryptor.lpCipherCtx == NULL)
  {
    CleanUp(FALSE, TRUE);
    return E_OUTOFMEMORY;
  }
  //get decryptor manager
  aes_data->sDecryptor.lpCipher = GetCipher(nAlgorithm);
  if (aes_data->sDecryptor.lpCipher == NULL)
  {
    CleanUp(FALSE, TRUE);
    return E_NOTIMPL;
  }
  //verify key length
  if (EVP_CIPHER_key_length(aes_data->sDecryptor.lpCipher) > 0)
  {
    if (lpKey == NULL)
    {
      CleanUp(FALSE, TRUE);
      return E_POINTER;
    }
    if (nKeyLen != (SIZE_T)EVP_CIPHER_key_length(aes_data->sDecryptor.lpCipher))
    {
      CleanUp(FALSE, TRUE);
      return E_INVALIDARG;
    }
  }
  //verify if IV is present
  if (EVP_CIPHER_iv_length(aes_data->sDecryptor.lpCipher) > 0)
  {
    if (lpInitVector == NULL)
    {
      CleanUp(FALSE, TRUE);
      return E_POINTER;
    }
  }
  //create temp output buffer
  aes_data->sDecryptor.nTempOutputSize = BLOCK_SIZE + (SIZE_T)EVP_CIPHER_block_size(aes_data->sDecryptor.lpCipher);
  aes_data->sDecryptor.lpTempOutputBuf = (LPBYTE)MX_MALLOC(aes_data->sDecryptor.nTempOutputSize);
  if (aes_data->sDecryptor.lpTempOutputBuf == NULL)
  {
    CleanUp(FALSE, TRUE);
    return E_OUTOFMEMORY;
  }
  //initialize
  if (EVP_DecryptInit(aes_data->sDecryptor.lpCipherCtx, aes_data->sDecryptor.lpCipher, (const unsigned char*)lpKey,
                      (const unsigned char*)lpInitVector) <= 0)
  {
    CleanUp(TRUE, TRUE);
    return E_FAIL;
  }
  EVP_CIPHER_CTX_set_padding(aes_data->sDecryptor.lpCipherCtx, (bUsePadding != FALSE) ? 1 : 0);
  //empty buffer
  cDecryptedData.SetBufferSize(0);
  //done
  return S_OK;
}

HRESULT CCryptoAES::DecryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)
{
  LPBYTE lpIn;
  int nInSize, nOutSize;
  HRESULT hRes;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (lpInternalData == NULL || aes_data->sDecryptor.lpCipherCtx == NULL)
    return MX_E_NotReady;
  lpIn = (LPBYTE)lpData;
  while (nDataLength > 0)
  {
    nInSize = (nDataLength > BLOCK_SIZE) ? BLOCK_SIZE : (int)nDataLength;
    if (EVP_DecryptUpdate(aes_data->sDecryptor.lpCipherCtx, aes_data->sDecryptor.lpTempOutputBuf, &nOutSize,
                          lpIn, nInSize) <= 0)
    {
      CleanUp(FALSE, TRUE);
      return MX_E_InvalidData;
    }
    lpIn += nInSize;
    nDataLength -= nInSize;
    if (nOutSize > 0)
    {
      hRes = cDecryptedData.Write(aes_data->sDecryptor.lpTempOutputBuf, (SIZE_T)nOutSize);
      if (FAILED(hRes))
      {
        CleanUp(FALSE, TRUE);
        return hRes;
      }
    }
  }
  return S_OK;
}

HRESULT CCryptoAES::EndDecrypt()
{
  int nOutSize;
  HRESULT hRes;

  if (lpInternalData == NULL || aes_data->sDecryptor.lpCipherCtx == NULL)
    return MX_E_NotReady;
  if (EVP_DecryptFinal_ex(aes_data->sDecryptor.lpCipherCtx, aes_data->sDecryptor.lpTempOutputBuf, &nOutSize) <= 0)
  {
    CleanUp(FALSE, TRUE);
    return MX_E_InvalidData;
  }
  if (nOutSize > 0)
  {
    hRes = cDecryptedData.Write(aes_data->sDecryptor.lpTempOutputBuf, (SIZE_T)nOutSize);
    if (FAILED(hRes))
    {
      CleanUp(FALSE, TRUE);
      return hRes;
    }
  }
  CleanUp(FALSE, FALSE);
  return S_OK;
}

VOID CCryptoAES::CleanUp(_In_ BOOL bEncoder, _In_ BOOL bZeroData)
{
  struct tagDES_DATA::tagCipherInfo *lpInfo = (bEncoder != FALSE) ? &(aes_data->sEncryptor) : &(aes_data->sDecryptor);

  if (lpInfo->lpTempOutputBuf != NULL)
    MemSet(lpInfo->lpTempOutputBuf, 0, lpInfo->nTempOutputSize);
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
    MemSet(lpPtr1, 0, nSize1);
    MemSet(lpPtr2, 0, nSize2);
  }
  return;
}

} //namespace MX

//-----------------------------------------------------------

static const EVP_CIPHER* GetCipher(_In_ MX::CCryptoAES::eAlgorithm nAlgorithm)
{
  switch (nAlgorithm)
  {
    case MX::CCryptoAES::AlgorithmAes128Ecb:
      return EVP_get_cipherbyname("AES-128-EBC");
    case MX::CCryptoAES::AlgorithmAes128Cbc:
      return EVP_get_cipherbyname("AES-128-CBC");
    case MX::CCryptoAES::AlgorithmAes128Cfb:
      return EVP_get_cipherbyname("AES-128-CFB");
    case MX::CCryptoAES::AlgorithmAes128Ofb:
      return EVP_get_cipherbyname("AES-128-OFB");
    case MX::CCryptoAES::AlgorithmAes128Cfb1:
      return EVP_get_cipherbyname("AES-128-CFB1");
    case MX::CCryptoAES::AlgorithmAes128Cfb8:
      return EVP_get_cipherbyname("AES-128-CFB8");
    case MX::CCryptoAES::AlgorithmAes192Ecb:
      return EVP_get_cipherbyname("AES-192-ECB");
    case MX::CCryptoAES::AlgorithmAes192Cbc:
      return EVP_get_cipherbyname("AES-192-CBC");
    case MX::CCryptoAES::AlgorithmAes192Cfb:
      return EVP_get_cipherbyname("AES-192-CFB");
    case MX::CCryptoAES::AlgorithmAes192Ofb:
      return EVP_get_cipherbyname("AES-192-OFB");
    case MX::CCryptoAES::AlgorithmAes192Cfb1:
      return EVP_get_cipherbyname("AES-192-CFB1");
    case MX::CCryptoAES::AlgorithmAes192Cfb8:
      return EVP_get_cipherbyname("AES-192-CFB8");
    case MX::CCryptoAES::AlgorithmAes256Ecb:
      return EVP_get_cipherbyname("AES-256-ECB");
    case MX::CCryptoAES::AlgorithmAes256Cbc:
      return EVP_get_cipherbyname("AES-256-CBC");
    case MX::CCryptoAES::AlgorithmAes256Cfb:
      return EVP_get_cipherbyname("AES-256-CFB");
    case MX::CCryptoAES::AlgorithmAes256Ofb:
      return EVP_get_cipherbyname("AES-256-OFB");
    case MX::CCryptoAES::AlgorithmAes256Cfb1:
      return EVP_get_cipherbyname("AES-256-CFB1");
    case MX::CCryptoAES::AlgorithmAes256Cfb8:
      return EVP_get_cipherbyname("AES-256-CFB8");
  }
  return NULL;
}
