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
#include "..\..\Include\Crypto\CryptoRSA.h"
#include "InitOpenSSL.h"
#include "..\..\Include\Crypto\DigestAlgorithmMDx.h"
#include "..\..\Include\Crypto\DigestAlgorithmSHAx.h"
#include "..\..\Include\AutoPtr.h"
#include <OpenSSL\evp.h>
#include <OpenSSL\bn.h>
#include <OpenSSL\rsa.h>
#include <OpenSSL\pkcs12.h>

#define BLOCK_SIZE                                      2048

#define WhatEncoder                                        1
#define WhatDecoder                                        2
#define WhatSigner                                         3
#define WhatVerifier                                       4

//-----------------------------------------------------------

typedef struct {
  LPBYTE lpInputBuf;
  SIZE_T nCurrInputSize, nMaxInputSize;
  LPBYTE lpOutputBuf;
  SIZE_T nMaxOutputSize;
  int padding;
  BOOL bUsePrivateKey;
} ENCDEC_CONTEXT;

typedef struct {
  MX::CBaseDigestAlgorithm *lpSigner;
  int md_type;
  LPBYTE lpSignature;
  SIZE_T nSignatureSize, nMaxSignatureSize;
} SIGN_CONTEXT;

typedef struct {
  MX::CBaseDigestAlgorithm *lpVerifier;
  int md_type;
} VERIFY_CONTEXT;

typedef struct tagRSA_DATA {
  RSA *lpRsa;
  ENCDEC_CONTEXT sEncryptor;
  ENCDEC_CONTEXT sDecryptor;
  SIGN_CONTEXT sSignContext;
  VERIFY_CONTEXT sVerifyContext;
} RSA_DATA;

#define rsa_data ((RSA_DATA*)lpInternalData)

//-----------------------------------------------------------

static HRESULT GetDigest(__in MX::CCryptoRSA::eHashAlgorithm nHashAlgorithm, __out int &md_type,
                         __out MX::CBaseDigestAlgorithm **lplpDigest);
static int InitializeFromPEM_PasswordCallback(char *buf, int size, int rwflag, void *userdata);

//-----------------------------------------------------------

namespace MX {

CCryptoRSA::CCryptoRSA() : CBaseCrypto()
{
  lpInternalData = NULL;
  if (Internals::OpenSSL::Init() != FALSE)
  {
    lpInternalData = MX_MALLOC(sizeof(RSA_DATA));
    if (lpInternalData != NULL)
      MemSet(lpInternalData, 0, sizeof(RSA_DATA));
  }
  return;
}

CCryptoRSA::~CCryptoRSA()
{
  if (lpInternalData != NULL)
  {
    CleanUp(WhatEncoder, TRUE);
    CleanUp(WhatDecoder, TRUE);
    CleanUp(WhatSigner, TRUE);
    CleanUp(WhatVerifier, TRUE);
    if (rsa_data->lpRsa != NULL)
      RSA_free(rsa_data->lpRsa);
    MemSet(rsa_data, 0, sizeof(RSA_DATA));
    MX_FREE(lpInternalData);
  }
  return;
}

HRESULT CCryptoRSA::GenerateKeys(__in SIZE_T nBitsCount)
{
  BIGNUM *lpBn;
  RSA *lpRsa;

  if (nBitsCount < 512 || nBitsCount > 65536 ||
     (nBitsCount && !(nBitsCount & (nBitsCount - 1))) == 0)
    return E_INVALIDARG;
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (rsa_data->sEncryptor.lpInputBuf != NULL || rsa_data->sDecryptor.lpInputBuf != NULL ||
      rsa_data->sSignContext.lpSigner != NULL || rsa_data->sVerifyContext.lpVerifier != NULL)
  {
    return MX_E_Busy;
  }
  //create holders
  lpBn = BN_new();
  if (lpBn == NULL)
    return E_OUTOFMEMORY;
  lpRsa = RSA_new();
  if (lpRsa != NULL)
  {
    BN_free(lpBn);
    return E_OUTOFMEMORY;
  }
  //generate key
  if (BN_set_word(lpBn, RSA_F4) <= 0 || RSA_generate_key_ex(lpRsa, (int)nBitsCount, lpBn, NULL) <= 0)
  {
    BN_free(lpBn);
    RSA_free(lpRsa);
    return E_OUTOFMEMORY;
  }
  BN_free(lpBn);
  //replace key
  if (rsa_data->lpRsa != NULL)
    RSA_free(rsa_data->lpRsa);
  rsa_data->lpRsa = lpRsa;
  //done
  return S_OK;
}

SIZE_T CCryptoRSA::GetBitsCount() const
{
  const BIGNUM *lpN;

  if (lpInternalData == NULL || rsa_data->lpRsa == NULL)
    return 0;
  RSA_get0_key(rsa_data->lpRsa, &lpN, NULL, NULL);
  return (lpN != NULL) ? (SIZE_T)BN_num_bits(lpN) : 0;
}

BOOL CCryptoRSA::HasPrivateKey() const
{
  return (const_cast<CCryptoRSA*>(this)->GetPrivateKey(NULL) > 0) ? TRUE : FALSE;
}

HRESULT CCryptoRSA::SetPublicKeyFromDER(__in LPCVOID lpKey, __in SIZE_T nKeySize, __in_z_opt LPCSTR szPasswordA)
{
  EVP_PKEY *lpEvpKey;
  RSA *lpRsa, *lpTempRsa;
  PKCS12 *lpPkcs12;
  X509 *lpX509;
  const unsigned char *buf;
  HRESULT hRes;

  if (lpKey == NULL)
    return E_POINTER;
  if (nKeySize == 0 || nKeySize > 0x7FFFFFFF)
    return E_INVALIDARG;
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (rsa_data->sEncryptor.lpInputBuf != NULL || rsa_data->sDecryptor.lpInputBuf != NULL ||
      rsa_data->sSignContext.lpSigner != NULL || rsa_data->sVerifyContext.lpVerifier != NULL)
  {
    return MX_E_Busy;
  }
  //try rsa public key
  ERR_clear_error();
  buf = (const unsigned char*)lpKey;
  lpRsa = d2i_RSAPublicKey(NULL, &buf, (long)nKeySize);
  if (lpRsa != NULL)
    goto done;
  hRes = Internals::OpenSSL::GetLastErrorCode(FALSE);
  if (hRes == E_OUTOFMEMORY)
    return hRes;
  //try standard public key
  ERR_clear_error();
  buf = (const unsigned char*)lpKey;
  lpEvpKey = d2i_PUBKEY(NULL, &buf, (long)nKeySize);
  if (lpEvpKey != NULL)
  {
    lpTempRsa = EVP_PKEY_get1_RSA(lpEvpKey);
    if (lpTempRsa != NULL)
    {
      lpRsa = RSAPublicKey_dup(lpTempRsa);
      EVP_PKEY_free(lpEvpKey);
      if (lpRsa == NULL)
        return E_OUTOFMEMORY;
      goto done;
    }
    EVP_PKEY_free(lpEvpKey);
  }
  else if (Internals::OpenSSL::GetLastErrorCode(FALSE) == E_OUTOFMEMORY)
  {
    return E_OUTOFMEMORY;
  }
  //try to extract from a pkcs12
  ERR_clear_error();
  buf = (const unsigned char*)lpKey;
  lpPkcs12 = d2i_PKCS12(NULL, &buf, (long)nKeySize);
  if (lpPkcs12 != NULL)
  {
    ERR_clear_error();
    if (PKCS12_parse(lpPkcs12, szPasswordA, &lpEvpKey, &lpX509, NULL))
    {
      if (lpX509 != NULL)
        X509_free(lpX509);
      if (lpEvpKey != NULL)
      {
        lpTempRsa = EVP_PKEY_get1_RSA(lpEvpKey);
        if (lpTempRsa != NULL)
        {
          lpRsa = RSAPublicKey_dup(lpTempRsa);
          EVP_PKEY_free(lpEvpKey);
          PKCS12_free(lpPkcs12);
          if (lpRsa == NULL)
            return E_OUTOFMEMORY;
          goto done;
        }
        EVP_PKEY_free(lpEvpKey);
      }
    }
    else if (Internals::OpenSSL::GetLastErrorCode(FALSE) == E_OUTOFMEMORY)
    {
      PKCS12_free(lpPkcs12);
      return E_OUTOFMEMORY;
    }
    PKCS12_free(lpPkcs12);
  }
  else if (Internals::OpenSSL::GetLastErrorCode(FALSE) == E_OUTOFMEMORY)
  {
    return E_OUTOFMEMORY;
  }
  //nothing else to try
  return MX_E_InvalidData;

done:
  //replace key
  if (rsa_data->lpRsa != NULL)
    RSA_free(rsa_data->lpRsa);
  rsa_data->lpRsa = lpRsa;
  //done
  return S_OK;
}

HRESULT CCryptoRSA::SetPublicKeyFromPEM(__in_z LPCSTR szPemA, __in_z_opt LPCSTR szPasswordA, __in_opt SIZE_T nPemLen)
{
  BIO *lpBio;
  EVP_PKEY *lpEvpKey;
  RSA *lpRsa, *lpTempRsa;

  if (szPemA == NULL)
    return E_POINTER;
  if (nPemLen == (SIZE_T)-1)
    nPemLen = StrLenA(szPemA);
  if (nPemLen == 0)
    return E_INVALIDARG;
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (rsa_data->sEncryptor.lpInputBuf != NULL || rsa_data->sDecryptor.lpInputBuf != NULL ||
      rsa_data->sSignContext.lpSigner != NULL || rsa_data->sVerifyContext.lpVerifier != NULL)
  {
    return MX_E_Busy;
  }
  //try RSA public key
  lpBio = BIO_new_mem_buf((void*)szPemA, (int)nPemLen);
  if (lpBio == NULL)
    return E_OUTOFMEMORY;
  lpRsa = PEM_read_bio_RSAPublicKey(lpBio, NULL, &InitializeFromPEM_PasswordCallback, (LPSTR)szPasswordA);
  if (lpRsa != NULL)
  {
    BIO_free(lpBio);
    goto done;
  }
  if (Internals::OpenSSL::GetLastErrorCode(FALSE) == E_OUTOFMEMORY)
  {
    BIO_free(lpBio);
    return E_OUTOFMEMORY;
  }
  //try standard public key
  lpBio = BIO_new_mem_buf((void*)szPemA, (int)nPemLen);
  if (lpBio == NULL)
    return E_OUTOFMEMORY;
  ERR_clear_error();
  lpEvpKey = PEM_read_bio_PUBKEY(lpBio, NULL, &InitializeFromPEM_PasswordCallback, (LPSTR)szPasswordA);
  if (lpEvpKey != NULL)
  {
    BIO_free(lpBio);
    lpTempRsa = EVP_PKEY_get1_RSA(lpEvpKey);
    if (lpTempRsa != NULL)
    {
      lpRsa = RSAPublicKey_dup(lpTempRsa);
      EVP_PKEY_free(lpEvpKey);
      if (lpRsa == NULL)
        return E_OUTOFMEMORY;
      goto done;
    }
    EVP_PKEY_free(lpEvpKey);
  }
  else if (Internals::OpenSSL::GetLastErrorCode(FALSE) == E_OUTOFMEMORY)
  {
    BIO_free(lpBio);
    return E_OUTOFMEMORY;
  }
  //nothing else to try
  BIO_free(lpBio);
  return MX_E_InvalidData;

done:
  //replace key
  if (rsa_data->lpRsa != NULL)
    RSA_free(rsa_data->lpRsa);
  rsa_data->lpRsa = lpRsa;
  //done
  return S_OK;
}

SIZE_T CCryptoRSA::GetPublicKey(__out_opt LPVOID lpDest)
{
  LPVOID lpData = NULL;
  int len;

  if (lpInternalData == NULL || rsa_data->lpRsa == NULL)
    return 0;
  len = i2d_RSAPublicKey(rsa_data->lpRsa, (unsigned char**)&lpData);
  if (len > 0 && lpData != NULL && lpDest != NULL)
    MX::MemCopy(lpDest, lpData, (SIZE_T)len);
  if (lpData != NULL)
    OPENSSL_free(lpData);
  return (len > 0) ? (SIZE_T)len : 0;
}

HRESULT CCryptoRSA::SetPrivateKeyFromDER(__in LPCVOID lpKey, __in SIZE_T nKeySize, __in_z_opt LPCSTR szPasswordA)
{
  EVP_PKEY *lpEvpKey;
  RSA *lpRsa, *lpTempRsa;
  PKCS12 *lpPkcs12;
  X509 *lpX509;
  const unsigned char *buf;
  HRESULT hRes;

  if (lpKey == NULL)
    return E_POINTER;
  if (nKeySize == 0 || nKeySize > 0x7FFFFFFF)
    return E_INVALIDARG;
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (rsa_data->sEncryptor.lpInputBuf != NULL || rsa_data->sDecryptor.lpInputBuf != NULL ||
      rsa_data->sSignContext.lpSigner != NULL || rsa_data->sVerifyContext.lpVerifier != NULL)
  {
    return MX_E_Busy;
  }
  //try rsa private key
  ERR_clear_error();
  buf = (const unsigned char*)lpKey;
  lpRsa = d2i_RSAPrivateKey(NULL, &buf, (long)nKeySize);
  if (lpRsa != NULL)
    goto done;
  hRes = Internals::OpenSSL::GetLastErrorCode(FALSE);
  if (hRes == E_OUTOFMEMORY)
    return hRes;
  //try standard private key
  ERR_clear_error();
  buf = (const unsigned char*)lpKey;
  lpEvpKey = d2i_AutoPrivateKey(NULL, &buf, (long)nKeySize);
  if (lpEvpKey != NULL)
  {
    lpTempRsa = EVP_PKEY_get1_RSA(lpEvpKey);
    if (lpTempRsa != NULL)
    {
      lpRsa = RSAPrivateKey_dup(lpTempRsa);
      EVP_PKEY_free(lpEvpKey);
      if (lpRsa == NULL)
        return E_OUTOFMEMORY;
      goto done;
    }
    EVP_PKEY_free(lpEvpKey);
  }
  else if (Internals::OpenSSL::GetLastErrorCode(FALSE) == E_OUTOFMEMORY)
  {
    return E_OUTOFMEMORY;
  }
  //try to extract from a pkcs12
  ERR_clear_error();
  buf = (const unsigned char*)lpKey;
  lpPkcs12 = d2i_PKCS12(NULL, &buf, (long)nKeySize);
  if (lpPkcs12 != NULL)
  {
    ERR_clear_error();
    if (PKCS12_parse(lpPkcs12, szPasswordA, &lpEvpKey, &lpX509, NULL))
    {
      if (lpX509 != NULL)
        X509_free(lpX509);
      if (lpEvpKey != NULL)
      {
        lpTempRsa = EVP_PKEY_get1_RSA(lpEvpKey);
        if (lpTempRsa != NULL)
        {
          lpRsa = RSAPrivateKey_dup(lpTempRsa);
          EVP_PKEY_free(lpEvpKey);
          PKCS12_free(lpPkcs12);
          if (lpRsa == NULL)
            return E_OUTOFMEMORY;
          goto done;
        }
        EVP_PKEY_free(lpEvpKey);
      }
    }
    else if (Internals::OpenSSL::GetLastErrorCode(FALSE) == E_OUTOFMEMORY)
    {
      PKCS12_free(lpPkcs12);
      return E_OUTOFMEMORY;
    }
    PKCS12_free(lpPkcs12);
  }
  else if (Internals::OpenSSL::GetLastErrorCode(FALSE) == E_OUTOFMEMORY)
  {
    return E_OUTOFMEMORY;
  }
  //nothing else to try
  return MX_E_InvalidData;

done:
  //replace key
  if (rsa_data->lpRsa != NULL)
    RSA_free(rsa_data->lpRsa);
  rsa_data->lpRsa = lpRsa;
  //done
  return S_OK;
}

HRESULT CCryptoRSA::SetPrivateKeyFromPEM(__in_z LPCSTR szPemA, __in_z_opt LPCSTR szPasswordA, __in_opt SIZE_T nPemLen)
{
  BIO *lpBio;
  EVP_PKEY *lpEvpKey;
  RSA *lpRsa, *lpTempRsa;

  if (szPemA == NULL)
    return E_POINTER;
  if (nPemLen == (SIZE_T)-1)
    nPemLen = StrLenA(szPemA);
  if (nPemLen == 0)
    return E_INVALIDARG;
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (rsa_data->sEncryptor.lpInputBuf != NULL || rsa_data->sDecryptor.lpInputBuf != NULL ||
      rsa_data->sSignContext.lpSigner != NULL || rsa_data->sVerifyContext.lpVerifier != NULL)
  {
    return MX_E_Busy;
  }
  //try rsa private key
  lpBio = BIO_new_mem_buf((void*)szPemA, (int)nPemLen);
  if (lpBio == NULL)
    return E_OUTOFMEMORY;
  lpRsa = PEM_read_bio_RSAPrivateKey(lpBio, NULL, &InitializeFromPEM_PasswordCallback, (LPSTR)szPasswordA);
  if (lpRsa != NULL)
  {
    BIO_free(lpBio);
    goto done;
  }
  if (Internals::OpenSSL::GetLastErrorCode(FALSE) == E_OUTOFMEMORY)
  {
    BIO_free(lpBio);
    return E_OUTOFMEMORY;
  }
  BIO_free(lpBio);
  //try standard private key
  lpBio = BIO_new_mem_buf((void*)szPemA, (int)nPemLen);
  if (lpBio == NULL)
    return E_OUTOFMEMORY;
  ERR_clear_error();
  lpEvpKey = PEM_read_bio_PrivateKey(lpBio, NULL, &InitializeFromPEM_PasswordCallback, (LPSTR)szPasswordA);
  if (lpEvpKey != NULL)
  {
    BIO_free(lpBio);
    lpTempRsa = EVP_PKEY_get1_RSA(lpEvpKey);
    if (lpTempRsa != NULL)
    {
      lpRsa = RSAPrivateKey_dup(lpTempRsa);
      EVP_PKEY_free(lpEvpKey);
      if (lpRsa == NULL)
        return E_OUTOFMEMORY;
      goto done;
    }
    EVP_PKEY_free(lpEvpKey);
  }
  else if (Internals::OpenSSL::GetLastErrorCode(FALSE) == E_OUTOFMEMORY)
  {
    BIO_free(lpBio);
    return E_OUTOFMEMORY;
  }
  //try extract from a pkcs12
  //nothing else to try
  BIO_free(lpBio);
  return MX_E_InvalidData;

done:
  //replace key
  if (rsa_data->lpRsa != NULL)
    RSA_free(rsa_data->lpRsa);
  rsa_data->lpRsa = lpRsa;
  //done
  return S_OK;
}

SIZE_T CCryptoRSA::GetPrivateKey(__out_opt LPVOID lpDest)
{
  LPVOID lpData = NULL;
  int len;

  if (lpInternalData == NULL || rsa_data->lpRsa == NULL)
    return 0;
  len = i2d_RSAPrivateKey(rsa_data->lpRsa, (unsigned char**)&lpData);
  if (len > 0 && lpData != NULL && lpDest != NULL)
    MX::MemCopy(lpDest, lpData, (SIZE_T)len);
  if (lpData != NULL)
    OPENSSL_free(lpData);
  return (len > 0) ? (SIZE_T)len : 0;
}

HRESULT CCryptoRSA::BeginEncrypt()
{
  return BeginEncrypt(PaddingPKCS1);
}

HRESULT CCryptoRSA::BeginEncrypt(__in ePadding nPadding, __in_opt BOOL bUsePublicKey)
{
  int padSize, padding;

  switch (nPadding)
  {
    case PaddingNone:
      padSize = 0;
      padding = RSA_NO_PADDING;
      break;
    case PaddingPKCS1:
      padSize = 11;
      padding = RSA_PKCS1_PADDING;
      break;
    case PaddingSSLV23:
      padding = RSA_SSLV23_PADDING;
      padSize = 11;
      break;
    case PaddingOAEP:
      padding = RSA_PKCS1_OAEP_PADDING;
      padSize = 41;
      break;
    default:
      return E_INVALIDARG;
  }
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (rsa_data->lpRsa == NULL)
    return MX_E_NotReady;
  if (bUsePublicKey == FALSE && HasPrivateKey() == FALSE)
    return MX_E_NotReady;
  CleanUp(WhatEncoder, TRUE);
  //initialize
  rsa_data->sEncryptor.padding = padding;
  rsa_data->sEncryptor.bUsePrivateKey = !bUsePublicKey;
  rsa_data->sEncryptor.nMaxInputSize = (SIZE_T)(RSA_size(rsa_data->lpRsa) - padSize);
  rsa_data->sEncryptor.lpInputBuf = (LPBYTE)MX_MALLOC(rsa_data->sEncryptor.nMaxInputSize);
  rsa_data->sEncryptor.nMaxOutputSize = (SIZE_T)RSA_size(rsa_data->lpRsa);
  rsa_data->sEncryptor.lpOutputBuf = (LPBYTE)MX_MALLOC(rsa_data->sEncryptor.nMaxOutputSize);
  if (rsa_data->sEncryptor.lpInputBuf == NULL || rsa_data->sEncryptor.lpOutputBuf == NULL)
  {
    CleanUp(WhatEncoder, TRUE);
    return E_OUTOFMEMORY;
  }
  //empty buffer
  cEncryptedData.SetBufferSize(0);
  //done
  return S_OK;
}

HRESULT CCryptoRSA::EncryptStream(__in LPCVOID lpData, __in SIZE_T nDataLength)
{
  SIZE_T nMaxData;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (rsa_data->sEncryptor.lpInputBuf == NULL)
    return MX_E_NotReady;
  nMaxData = rsa_data->sEncryptor.nMaxInputSize - rsa_data->sEncryptor.nCurrInputSize;
  if (nDataLength > nMaxData)
  {
    CleanUp(WhatEncoder, TRUE);
    return MX_E_InvalidData;
  }
  MemCopy(rsa_data->sEncryptor.lpInputBuf + rsa_data->sEncryptor.nCurrInputSize, lpData, nDataLength);
  rsa_data->sEncryptor.nCurrInputSize += nDataLength;
  return S_OK;
}

HRESULT CCryptoRSA::EndEncrypt()
{
  int r;
  HRESULT hRes;

  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (rsa_data->sEncryptor.lpInputBuf == NULL)
    return MX_E_NotReady;
  ERR_clear_error();
  if (rsa_data->sEncryptor.bUsePrivateKey != FALSE)
  {
    r = RSA_private_encrypt((int)(rsa_data->sEncryptor.nCurrInputSize), rsa_data->sEncryptor.lpInputBuf,
                            rsa_data->sEncryptor.lpOutputBuf, rsa_data->lpRsa, rsa_data->sEncryptor.padding);
  }
  else
  {
    r = RSA_public_encrypt((int)(rsa_data->sEncryptor.nCurrInputSize), rsa_data->sEncryptor.lpInputBuf,
                           rsa_data->sEncryptor.lpOutputBuf, rsa_data->lpRsa, rsa_data->sEncryptor.padding);
  }
  if (r <= 0)
  {
    hRes = Internals::OpenSSL::GetLastErrorCode(TRUE);
    CleanUp(WhatEncoder, TRUE);
    return hRes;
  }
  hRes = cEncryptedData.Write(rsa_data->sEncryptor.lpOutputBuf, (SIZE_T)r);
  if (FAILED(hRes))
  {
    CleanUp(WhatEncoder, TRUE);
    return hRes;
  }
  CleanUp(WhatEncoder, FALSE);
  return S_OK;
}

HRESULT CCryptoRSA::BeginDecrypt()
{
  return BeginDecrypt(PaddingPKCS1);
}

HRESULT CCryptoRSA::BeginDecrypt(__in ePadding nPadding, __in_opt BOOL bUsePrivateKey)
{
  int padding;

  switch (nPadding)
  {
    case PaddingNone:
      padding = RSA_NO_PADDING;
      break;
    case PaddingPKCS1:
      padding = RSA_PKCS1_PADDING;
      break;
    case PaddingSSLV23:
      padding = RSA_SSLV23_PADDING;
      break;
    case PaddingOAEP:
      padding = RSA_PKCS1_OAEP_PADDING;
      break;
    default:
      return E_INVALIDARG;
  }
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (rsa_data->lpRsa == NULL)
    return MX_E_NotReady;
  if (bUsePrivateKey != FALSE && HasPrivateKey() == FALSE)
    return MX_E_NotReady;
  CleanUp(WhatDecoder, TRUE);
  //initialize
  rsa_data->sDecryptor.padding = padding;
  rsa_data->sDecryptor.bUsePrivateKey = bUsePrivateKey;
  rsa_data->sDecryptor.nMaxInputSize = (SIZE_T)RSA_size(rsa_data->lpRsa);
  rsa_data->sDecryptor.lpInputBuf = (LPBYTE)MX_MALLOC(rsa_data->sDecryptor.nMaxInputSize);
  rsa_data->sDecryptor.nMaxOutputSize = (SIZE_T)RSA_size(rsa_data->lpRsa);
  rsa_data->sDecryptor.lpOutputBuf = (LPBYTE)MX_MALLOC(rsa_data->sDecryptor.nMaxOutputSize);
  if (rsa_data->sDecryptor.lpInputBuf == NULL || rsa_data->sDecryptor.lpOutputBuf == NULL)
  {
    CleanUp(WhatDecoder, TRUE);
    return E_OUTOFMEMORY;
  }
  //empty buffer
  cDecryptedData.SetBufferSize(0);
  //done
  return S_OK;
}

HRESULT CCryptoRSA::DecryptStream(__in LPCVOID lpData, __in SIZE_T nDataLength)
{
  SIZE_T nMaxData;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (rsa_data->sDecryptor.lpInputBuf == NULL)
    return MX_E_NotReady;
  nMaxData = rsa_data->sDecryptor.nMaxInputSize - rsa_data->sDecryptor.nCurrInputSize;
  if (nDataLength > nMaxData)
  {
    CleanUp(WhatDecoder, TRUE);
    return MX_E_InvalidData;
  }
  MemCopy(rsa_data->sDecryptor.lpInputBuf + rsa_data->sDecryptor.nCurrInputSize, lpData, nDataLength);
  rsa_data->sDecryptor.nCurrInputSize += nDataLength;
  return S_OK;
}

HRESULT CCryptoRSA::EndDecrypt()
{
  int r;
  HRESULT hRes;

  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (rsa_data->sDecryptor.lpInputBuf == NULL)
    return MX_E_NotReady;
  ERR_clear_error();
  if (rsa_data->sDecryptor.bUsePrivateKey != FALSE)
  {
    r = RSA_private_decrypt((int)(rsa_data->sDecryptor.nCurrInputSize), rsa_data->sDecryptor.lpInputBuf,
                            rsa_data->sDecryptor.lpOutputBuf, rsa_data->lpRsa, rsa_data->sDecryptor.padding);
  }
  else
  {
    r = RSA_public_decrypt((int)(rsa_data->sDecryptor.nCurrInputSize), rsa_data->sDecryptor.lpInputBuf,
                           rsa_data->sDecryptor.lpOutputBuf, rsa_data->lpRsa, rsa_data->sDecryptor.padding);
  }
  if (r <= 0)
  {
    hRes = Internals::OpenSSL::GetLastErrorCode(TRUE);
    CleanUp(WhatDecoder, TRUE);
    return hRes;
  }
  hRes = cDecryptedData.Write(rsa_data->sDecryptor.lpOutputBuf, (SIZE_T)r);
  if (FAILED(hRes))
  {
    CleanUp(WhatDecoder, TRUE);
    return hRes;
  }
  CleanUp(WhatDecoder, FALSE);
  return S_OK;
}

HRESULT CCryptoRSA::BeginSign()
{
  return BeginSign(HashAlgorithmSHA1);
}

HRESULT CCryptoRSA::BeginSign(__in eHashAlgorithm nAlgorithm)
{
  TAutoDeletePtr<MX::CBaseDigestAlgorithm> cDigest;
  int md_type;
  HRESULT hRes;

  hRes = GetDigest(nAlgorithm, md_type, &cDigest);
  if (FAILED(hRes))
    return hRes;
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (rsa_data->lpRsa == NULL)
    return MX_E_NotReady;
  if (HasPrivateKey() == FALSE)
    return MX_E_NotReady;
  CleanUp(WhatSigner, TRUE);
  //initialize
  rsa_data->sSignContext.md_type = md_type;
  rsa_data->sSignContext.lpSigner = cDigest.Detach();
  rsa_data->sSignContext.nMaxSignatureSize = (SIZE_T)RSA_size(rsa_data->lpRsa);
  rsa_data->sSignContext.lpSignature = (LPBYTE)MX_MALLOC(rsa_data->sSignContext.nMaxSignatureSize);
  if (rsa_data->sSignContext.lpSignature == NULL)
  {
    CleanUp(WhatSigner, TRUE);
    return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CCryptoRSA::SignStream(__in LPCVOID lpData, __in SIZE_T nDataLength)
{
  HRESULT hRes;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (rsa_data->sSignContext.lpSigner == NULL)
    return MX_E_NotReady;
  hRes = rsa_data->sSignContext.lpSigner->DigestStream(lpData, nDataLength);
  if (FAILED(hRes))
  {
    CleanUp(WhatSigner, TRUE);
    return hRes;
  }
  return S_OK;
}

HRESULT CCryptoRSA::EndSign()
{
  int r;
  unsigned int siglen;
  HRESULT hRes;

  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (rsa_data->sSignContext.lpSigner == NULL)
    return MX_E_NotReady;
  hRes = rsa_data->sSignContext.lpSigner->EndDigest();
  if (FAILED(hRes))
  {
    CleanUp(WhatSigner, TRUE);
    return hRes;
  }
  r = RSA_sign(rsa_data->sSignContext.md_type, rsa_data->sSignContext.lpSigner->GetResult(),
               (unsigned int)(rsa_data->sSignContext.lpSigner->GetResultSize()),
               rsa_data->sSignContext.lpSignature, &siglen, rsa_data->lpRsa);
  if (r <= 0)
  {
    hRes = Internals::OpenSSL::GetLastErrorCode(TRUE);
    CleanUp(WhatSigner, TRUE);
    return hRes;
  }
  rsa_data->sSignContext.nSignatureSize = (SIZE_T)siglen;
  CleanUp(WhatSigner, FALSE);
  return S_OK;
}

LPBYTE CCryptoRSA::GetSignature() const
{
  return (lpInternalData != NULL) ? rsa_data->sSignContext.lpSignature : NULL;
}

SIZE_T CCryptoRSA::GetSignatureSize() const
{
  return (lpInternalData != NULL) ? rsa_data->sSignContext.nSignatureSize : 0;
}

HRESULT CCryptoRSA::BeginVerify()
{
  return BeginVerify(HashAlgorithmSHA1);
}

HRESULT CCryptoRSA::BeginVerify(__in eHashAlgorithm nAlgorithm)
{
  TAutoDeletePtr<MX::CBaseDigestAlgorithm> cDigest;
  int md_type;
  HRESULT hRes;

  hRes = GetDigest(nAlgorithm, md_type, &cDigest);
  if (FAILED(hRes))
    return hRes;
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (rsa_data->lpRsa == NULL)
    return MX_E_NotReady;
  if (HasPrivateKey() == FALSE)
    return MX_E_NotReady;
  CleanUp(WhatVerifier, TRUE);
  //initialize
  rsa_data->sVerifyContext.md_type = md_type;
  rsa_data->sVerifyContext.lpVerifier = cDigest.Detach();
  //done
  return S_OK;
}

HRESULT CCryptoRSA::VerifyStream(__in LPCVOID lpData, __in SIZE_T nDataLength)
{
  HRESULT hRes;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (rsa_data->sVerifyContext.lpVerifier == NULL)
    return MX_E_NotReady;
  hRes = rsa_data->sVerifyContext.lpVerifier->DigestStream(lpData, nDataLength);
  if (FAILED(hRes))
  {
    CleanUp(WhatVerifier, TRUE);
    return hRes;
  }
  return S_OK;
}

HRESULT CCryptoRSA::EndVerify(__in LPCVOID lpSignature, __in SIZE_T nSignatureLen)
{
  int r;
  HRESULT hRes;

  if (lpSignature == NULL)
    return E_POINTER;
  if (nSignatureLen == 0 || nSignatureLen > 0x7FFFFFFF)
    return E_INVALIDARG;
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (rsa_data->sVerifyContext.lpVerifier == NULL)
    return MX_E_NotReady;
  hRes = rsa_data->sVerifyContext.lpVerifier->EndDigest();
  if (FAILED(hRes))
  {
    CleanUp(WhatVerifier, TRUE);
    return hRes;
  }
  r = RSA_verify(rsa_data->sVerifyContext.md_type, rsa_data->sVerifyContext.lpVerifier->GetResult(),
                 (unsigned int)(rsa_data->sVerifyContext.lpVerifier->GetResultSize()),
                 (const unsigned char*)lpSignature, (unsigned int)nSignatureLen, rsa_data->lpRsa);
  if (r <= 0)
  {
    hRes = Internals::OpenSSL::GetLastErrorCode(TRUE);
    CleanUp(WhatSigner, TRUE);
    return hRes;
  }
  CleanUp(WhatSigner, FALSE);
  return S_OK;
}

VOID CCryptoRSA::CleanUp(__in int nWhat, __in BOOL bZeroData)
{
  switch (nWhat)
  {
    case WhatEncoder:
      if (rsa_data->sEncryptor.lpInputBuf != NULL)
        MemSet(rsa_data->sEncryptor.lpInputBuf, 0, rsa_data->sEncryptor.nMaxInputSize);
      MX_FREE(rsa_data->sEncryptor.lpInputBuf);
      rsa_data->sEncryptor.nCurrInputSize = 0;
      rsa_data->sEncryptor.nMaxInputSize = 0;
      //----
      if (rsa_data->sEncryptor.lpOutputBuf != NULL)
        MemSet(rsa_data->sEncryptor.lpOutputBuf, 0, rsa_data->sEncryptor.nMaxOutputSize);
      MX_FREE(rsa_data->sEncryptor.lpOutputBuf);
      rsa_data->sEncryptor.nMaxOutputSize = 0;
      //----
      rsa_data->sEncryptor.padding = 0;
      rsa_data->sEncryptor.bUsePrivateKey = FALSE;
      //----
      if (bZeroData != FALSE)
      {
        LPBYTE lpPtr1, lpPtr2;
        SIZE_T nSize1, nSize2;

        cEncryptedData.GetReadPtr(&lpPtr1, &nSize1, &lpPtr2, &nSize2);
        MemSet(lpPtr1, 0, nSize1);
        MemSet(lpPtr2, 0, nSize2);
      }
      break;

    case WhatDecoder:
      if (rsa_data->sDecryptor.lpInputBuf != NULL)
        MemSet(rsa_data->sDecryptor.lpInputBuf, 0, rsa_data->sDecryptor.nMaxInputSize);
      MX_FREE(rsa_data->sDecryptor.lpInputBuf);
      rsa_data->sDecryptor.nCurrInputSize = 0;
      rsa_data->sDecryptor.nMaxInputSize = 0;
      //----
      if (rsa_data->sDecryptor.lpOutputBuf != NULL)
        MemSet(rsa_data->sDecryptor.lpOutputBuf, 0, rsa_data->sDecryptor.nMaxOutputSize);
      MX_FREE(rsa_data->sDecryptor.lpOutputBuf);
      rsa_data->sDecryptor.nMaxOutputSize = 0;
      //----
      rsa_data->sDecryptor.padding = 0;
      rsa_data->sDecryptor.bUsePrivateKey = TRUE;
      //----
      if (bZeroData != FALSE)
      {
        LPBYTE lpPtr1, lpPtr2;
        SIZE_T nSize1, nSize2;

        cDecryptedData.GetReadPtr(&lpPtr1, &nSize1, &lpPtr2, &nSize2);
        MemSet(lpPtr1, 0, nSize1);
        MemSet(lpPtr2, 0, nSize2);
      }
      break;

    case WhatSigner:
      MX_DELETE(rsa_data->sSignContext.lpSigner);
      //----
      rsa_data->sSignContext.md_type = 0;
      //----
      if (bZeroData != FALSE)
      {
        if (rsa_data->sSignContext.lpSignature != NULL)
          MemSet(rsa_data->sSignContext.lpSignature, 0, rsa_data->sSignContext.nMaxSignatureSize);
        MX_FREE(rsa_data->sSignContext.lpSignature);
        rsa_data->sSignContext.nSignatureSize = rsa_data->sSignContext.nMaxSignatureSize = 0;
      }
      break;

    case WhatVerifier:
      MX_DELETE(rsa_data->sVerifyContext.lpVerifier);
      //----
      rsa_data->sVerifyContext.md_type = 0;
      break;
  }
  return;
}

} //namespace MX

//-----------------------------------------------------------

static HRESULT GetDigest(__in MX::CCryptoRSA::eHashAlgorithm nHashAlgorithm, __out int &md_type,
                         __out MX::CBaseDigestAlgorithm **lplpDigest)
{
  HRESULT hRes;

  *lplpDigest = NULL;
  md_type = 0;
  switch (nHashAlgorithm)
  {
    case MX::CCryptoRSA::HashAlgorithmSHA1:
    case MX::CCryptoRSA::HashAlgorithmSHA224:
    case MX::CCryptoRSA::HashAlgorithmSHA256:
    case MX::CCryptoRSA::HashAlgorithmSHA384:
    case MX::CCryptoRSA::HashAlgorithmSHA512:
      {
      MX::TAutoDeletePtr<MX::CDigestAlgorithmSecureHash> cDigest;
      MX::CDigestAlgorithmSecureHash::eAlgorithm nAlg;

      switch (nHashAlgorithm)
      {
        case MX::CCryptoRSA::HashAlgorithmSHA1:
          nAlg = MX::CDigestAlgorithmSecureHash::AlgorithmSHA1;
          md_type = NID_sha1;
          break;
        case MX::CCryptoRSA::HashAlgorithmSHA224:
          nAlg = MX::CDigestAlgorithmSecureHash::AlgorithmSHA224;
          md_type = NID_sha224;
          break;
        case MX::CCryptoRSA::HashAlgorithmSHA256:
          nAlg = MX::CDigestAlgorithmSecureHash::AlgorithmSHA256;
          md_type = NID_sha256;
          break;
        case MX::CCryptoRSA::HashAlgorithmSHA384:
          nAlg = MX::CDigestAlgorithmSecureHash::AlgorithmSHA384;
          md_type = NID_sha384;
          break;
        case MX::CCryptoRSA::HashAlgorithmSHA512:
          nAlg = MX::CDigestAlgorithmSecureHash::AlgorithmSHA512;
          md_type = NID_sha512;
          break;
      }
      cDigest.Attach(MX_DEBUG_NEW MX::CDigestAlgorithmSecureHash());
      if (!cDigest)
        return E_OUTOFMEMORY;
      hRes = cDigest->BeginDigest(nAlg, NULL, 0);
      if (FAILED(hRes))
        return hRes;
      *lplpDigest = cDigest.Detach();
      }
      break;

    case MX::CCryptoRSA::HashAlgorithmMD2:
    case MX::CCryptoRSA::HashAlgorithmMD4:
    case MX::CCryptoRSA::HashAlgorithmMD5:
      {
      MX::TAutoDeletePtr<MX::CDigestAlgorithmMessageDigest> cDigest;
      MX::CDigestAlgorithmMessageDigest::eAlgorithm nAlg;

      switch (nHashAlgorithm)
      {
        case MX::CCryptoRSA::HashAlgorithmMD2:
          nAlg = MX::CDigestAlgorithmMessageDigest::AlgorithmMD2;
          md_type = NID_md2;
          break;
        case MX::CCryptoRSA::HashAlgorithmSHA224:
          nAlg = MX::CDigestAlgorithmMessageDigest::AlgorithmMD4;
          md_type = NID_md4;
          break;
        case MX::CCryptoRSA::HashAlgorithmSHA256:
          nAlg = MX::CDigestAlgorithmMessageDigest::AlgorithmMD5;
          md_type = NID_md5;
          break;
      }
      cDigest.Attach(MX_DEBUG_NEW MX::CDigestAlgorithmMessageDigest());
      if (!cDigest)
        return E_OUTOFMEMORY;
      hRes = cDigest->BeginDigest(nAlg, NULL, 0);
      if (FAILED(hRes))
        return hRes;
      *lplpDigest = cDigest.Detach();
      }
      break;

    default:
      return E_INVALIDARG;
  }
  return S_OK;
}

static int InitializeFromPEM_PasswordCallback(char *buf, int size, int rwflag, void *userdata)
{
  SIZE_T nPassLen;

  nPassLen = MX::StrLenA((LPSTR)userdata);
  if (size < 0)
    size = 0;
  if (nPassLen >(SIZE_T)size)
    nPassLen = (SIZE_T)size;
  MX::MemCopy(buf, userdata, nPassLen);
  return (int)nPassLen;
}
