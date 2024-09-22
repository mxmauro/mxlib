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
#include <OpenSSL\decoder.h>

//-----------------------------------------------------------

typedef int (*lpfn_i2d_XXX_bio)(BIO *bp, const EVP_PKEY *pkey);

//-----------------------------------------------------------

static HRESULT GetCommon(_In_ EVP_PKEY *lpKey, _In_ lpfn_i2d_XXX_bio fn_i2d_XXX_bio, _Out_ MX::CSecureBuffer **lplpBuffer);

//-----------------------------------------------------------

namespace MX {

CEncryptionKey::CEncryptionKey() : TRefCounted<CBaseMemObj>()
{
  return;
}

CEncryptionKey::CEncryptionKey(_In_ const CEncryptionKey &cSrc) throw(...) : TRefCounted<CBaseMemObj>()
{
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
    //dereference old
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

  switch (nAlgorithm)
  {
    case MX::CEncryptionKey::RSA:
    case MX::CEncryptionKey::DSA:
    case MX::CEncryptionKey::DH:
    case MX::CEncryptionKey::DHX:
      if (nBitsCount < 512 || nBitsCount > 65536 ||
          (nBitsCount && !(nBitsCount & (nBitsCount - 1))) == 0)
      {
        return E_INVALIDARG;
      }
      break;
  }

  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;

  switch (nAlgorithm)
  {
    case MX::CEncryptionKey::RSA:
      lpKeyCtx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
      break;

    case MX::CEncryptionKey::ED25519:
      lpKeyCtx = EVP_PKEY_CTX_new_from_name(NULL, "ED25519", NULL);
      break;

    case MX::CEncryptionKey::ED448:
      lpKeyCtx = EVP_PKEY_CTX_new_from_name(NULL, "ED448", NULL);
      break;

    case MX::CEncryptionKey::Poly1305:
      lpKeyCtx = EVP_PKEY_CTX_new_from_name(NULL, "POLY1305", NULL);
      break;

    case MX::CEncryptionKey::DSA:
      lpKeyCtx = EVP_PKEY_CTX_new_from_name(NULL, "DSA", NULL);
      break;

    case MX::CEncryptionKey::DH:
      lpKeyCtx = EVP_PKEY_CTX_new_from_name(NULL, "DH", NULL);
      break;

    case MX::CEncryptionKey::DHX:
      lpKeyCtx = EVP_PKEY_CTX_new_from_name(NULL, "DHX", NULL);
      break;

    default:
      return E_INVALIDARG;
  }
  if (lpKeyCtx == NULL)
    return MX::Internals::OpenSSL::GetLastErrorCode(E_NOTIMPL);

  switch (nAlgorithm)
  {
    case MX::CEncryptionKey::RSA:
    case MX::CEncryptionKey::ED25519:
    case MX::CEncryptionKey::ED448:
    case MX::CEncryptionKey::Poly1305:
      if (EVP_PKEY_keygen_init(lpKeyCtx) <= 0)
      {
        hRes = MX::Internals::OpenSSL::GetLastErrorCode(E_OUTOFMEMORY);
        EVP_PKEY_CTX_free(lpKeyCtx);
        return hRes;
      }
      break;

    case MX::CEncryptionKey::DSA:
    case MX::CEncryptionKey::DH:
    case MX::CEncryptionKey::DHX:
      if (EVP_PKEY_paramgen_init(lpKeyCtx) <= 0)
      {
        hRes = MX::Internals::OpenSSL::GetLastErrorCode(E_OUTOFMEMORY);
        EVP_PKEY_CTX_free(lpKeyCtx);
        return hRes;
      }
      break;
  }

  switch (nAlgorithm)
  {
    case MX::CEncryptionKey::RSA:
      if (EVP_PKEY_CTX_set_rsa_keygen_bits(lpKeyCtx, (int)nBitsCount) <= 0)
      {
        hRes = MX::Internals::OpenSSL::GetLastErrorCode(E_INVALIDARG);
        EVP_PKEY_CTX_free(lpKeyCtx);
        return hRes;
      }
      break;

    case MX::CEncryptionKey::ED25519:
      break;

    case MX::CEncryptionKey::ED448:
      break;

    case MX::CEncryptionKey::Poly1305:
      break;

    case MX::CEncryptionKey::DSA:
      if (EVP_PKEY_CTX_set_dsa_paramgen_bits(lpKeyCtx, (int)nBitsCount) <= 0)
      {
        hRes = MX::Internals::OpenSSL::GetLastErrorCode(E_INVALIDARG);
        EVP_PKEY_CTX_free(lpKeyCtx);
        return hRes;
      }
      break;

    case MX::CEncryptionKey::DH:
    case MX::CEncryptionKey::DHX:
      if (EVP_PKEY_CTX_set_dh_paramgen_prime_len(lpKeyCtx, (int)nBitsCount) <= 0 ||
          EVP_PKEY_CTX_set_dh_paramgen_generator(lpKeyCtx, 2) <= 0)
      {
        hRes = MX::Internals::OpenSSL::GetLastErrorCode(E_INVALIDARG);
        EVP_PKEY_CTX_free(lpKeyCtx);
        return hRes;
      }
      break;
  }

  if (EVP_PKEY_generate(lpKeyCtx, &lpNewKey) <= 0)
  {
    hRes = MX::Internals::OpenSSL::GetLastErrorCode(E_INVALIDARG);
    EVP_PKEY_CTX_free(lpKeyCtx);
    return hRes;
  }

  //replace key
  if (lpKey != NULL)
    EVP_PKEY_free(lpKey);
  lpKey = lpNewKey;

  //done
  EVP_PKEY_CTX_free(lpKeyCtx);
  return hRes;
}

SIZE_T CEncryptionKey::GetBitsCount() const
{
  int size;

  if (lpKey == NULL)
    return 0;
  size = EVP_PKEY_get_bits(lpKey);
  return (size > 0) ? (SIZE_T)size : 0;
}

BOOL CEncryptionKey::HasPrivateKey() const
{
  if (lpKey == NULL)
    return FALSE;
  return (i2d_PUBKEY(lpKey, NULL) > 0) ? TRUE : FALSE;
}

HRESULT CEncryptionKey::Set(_In_ LPCVOID _lpKey, _In_ SIZE_T _nKeySize, _In_opt_z_ LPCSTR szPasswordA)
{
  HRESULT hRes;

  if (_lpKey == NULL)
    return E_POINTER;
  if (_nKeySize == 0 || _nKeySize > 0x7FFFFFFF)
    return E_INVALIDARG;

  //detect PEM or DER
  if (MX::Internals::OpenSSL::IsPEM(_lpKey, _nKeySize) != FALSE)
  {
    static const int nSelections[3] = {
      OSSL_KEYMGMT_SELECT_PUBLIC_KEY, OSSL_KEYMGMT_SELECT_PRIVATE_KEY, OSSL_KEYMGMT_SELECT_ALL_PARAMETERS
    };

    for (int pass = 1; pass <= MX_ARRAYLEN(nSelections); pass++)
    {
      const unsigned char *data;
      size_t datalen;
      OSSL_DECODER_CTX *lpDecCtx = NULL;
      EVP_PKEY *lpDecKey = NULL;

      ERR_clear_error();
      lpDecCtx = OSSL_DECODER_CTX_new_for_pkey(&lpDecKey, "PEM", NULL, NULL, nSelections[pass - 1], NULL, NULL);
      if (lpDecCtx == NULL)
      {
        hRes = MX::Internals::OpenSSL::GetLastErrorCode(E_NOTIMPL);
        goto pem_try_next;
      }
      if (szPasswordA != NULL && *szPasswordA != 0)
      {
        if (OSSL_DECODER_CTX_set_passphrase(lpDecCtx, (unsigned char *)szPasswordA, MX::StrLenA(szPasswordA)) <= 0)
        {
          hRes = E_OUTOFMEMORY;
          goto pem_try_next;
        }
      }

      data = (const unsigned char *)_lpKey;
      datalen = (size_t)_nKeySize;
      if (OSSL_DECODER_from_data(lpDecCtx, &data, &datalen) <= 0)
      {
        hRes = MX::Internals::OpenSSL::GetLastErrorCode(E_NOTIMPL);
        goto pem_try_next;
      }

      //we have a key

      //cleanup
      if (lpDecCtx != NULL)
        OSSL_DECODER_CTX_free(lpDecCtx);

      //replace key
      if (lpKey != NULL)
        EVP_PKEY_free(lpKey);
      lpKey = lpDecKey;

      //done
      hRes = S_OK;
      break;

pem_try_next:
      if (lpDecCtx != NULL)
        OSSL_DECODER_CTX_free(lpDecCtx);
      if (hRes == E_OUTOFMEMORY)
        break;
    }
  }
  else
  {
    typedef struct _KEYTYPE {
      LPCSTR szNameA;
      BOOL bIsParam;
    } KEYTYPE;
    static const KEYTYPE aKeyTypes[] = {
      { "RSA", FALSE }, { "ED25519", FALSE }, { "ED448", FALSE }, { "POLY1305", FALSE },
      { "DSA", TRUE },  { "DH", TRUE },       { "DHX", TRUE }
    };

    for (int pass = 1; pass <= MX_ARRAYLEN(aKeyTypes); pass++)
    {
      int subpassmax = (aKeyTypes[pass - 1].bIsParam == FALSE) ? 2 : 1;

      for (int subpass = 1; subpass < subpassmax; subpass++)
      {
        const unsigned char *data;
        size_t datalen;
        OSSL_DECODER_CTX *lpDecCtx = NULL;
        EVP_PKEY *lpDecKey = NULL;
        int selection;

        if (aKeyTypes[pass - 1].bIsParam == FALSE)
        {
          selection = (subpass == 1) ? OSSL_KEYMGMT_SELECT_PUBLIC_KEY : OSSL_KEYMGMT_SELECT_PRIVATE_KEY;
        }
        else
        {
          selection = OSSL_KEYMGMT_SELECT_ALL_PARAMETERS;
        }

        ERR_clear_error();
        lpDecCtx = OSSL_DECODER_CTX_new_for_pkey(&lpDecKey, "DER", NULL, aKeyTypes[pass - 1].szNameA,
                                                 selection, NULL, NULL);
        if (lpDecCtx == NULL)
        {
          hRes = MX::Internals::OpenSSL::GetLastErrorCode(E_NOTIMPL);
          goto der_try_next;
        }
        if (szPasswordA != NULL && *szPasswordA != 0)
        {
          if (OSSL_DECODER_CTX_set_passphrase(lpDecCtx, (unsigned char *)szPasswordA, MX::StrLenA(szPasswordA)) <= 0)
          {
            hRes = E_OUTOFMEMORY;
            goto der_try_next;
          }
        }

        data = (const unsigned char *)_lpKey;
        datalen = (size_t)_nKeySize;
        if (OSSL_DECODER_from_data(lpDecCtx, &data, &datalen) <= 0)
        {
          hRes = MX::Internals::OpenSSL::GetLastErrorCode(E_NOTIMPL);
          goto der_try_next;
        }

        //we have a key

        //cleanup
        if (lpDecCtx != NULL)
          OSSL_DECODER_CTX_free(lpDecCtx);

        //replace key
        if (lpKey != NULL)
          EVP_PKEY_free(lpKey);
        lpKey = lpDecKey;

        //done
        hRes = S_OK;
        break;

der_try_next:
        if (lpDecCtx != NULL)
          OSSL_DECODER_CTX_free(lpDecCtx);
        if (hRes == E_OUTOFMEMORY)
          break;
      }

      if (SUCCEEDED(hRes) || hRes == E_OUTOFMEMORY)
        break;
    }
  }

  //done
  return hRes;
}

VOID CEncryptionKey::Set(_In_ EVP_PKEY *_lpKey)
{
  if (lpKey != NULL)
    EVP_PKEY_free(lpKey);
  lpKey = _lpKey;
  if (lpKey != NULL)
    EVP_PKEY_up_ref(lpKey);
  return;
}

HRESULT CEncryptionKey::GetPublicKey(_Out_ CSecureBuffer **lplpBuffer)
{
  return GetCommon(lpKey, i2d_PUBKEY_bio, lplpBuffer);
}

HRESULT CEncryptionKey::GetPrivateKey(_Out_ CSecureBuffer **lplpBuffer)
{
  return GetCommon(lpKey, i2d_PrivateKey_bio, lplpBuffer);
}

HRESULT CEncryptionKey::GetKeyParams(_Out_ CSecureBuffer **lplpBuffer)
{
  return GetCommon(lpKey, i2d_KeyParams_bio, lplpBuffer);
}

int CEncryptionKey::GetBaseId() const
{
  if (lpKey == NULL)
    return 0;
  return EVP_PKEY_get_base_id(lpKey);
}

CEncryptionKey::eAlgorithm CEncryptionKey::GetAlgorithm() const
{
  switch (GetBaseId())
  {
    case EVP_PKEY_RSA:
    case EVP_PKEY_RSA2:
      return eAlgorithm::RSA;
    case EVP_PKEY_ED25519:
      return eAlgorithm::ED25519;
    case EVP_PKEY_ED448:
      return eAlgorithm::ED448;
    case EVP_PKEY_POLY1305:
      return eAlgorithm::Poly1305;
    case EVP_PKEY_DSA:
      return eAlgorithm::DSA;
    case EVP_PKEY_DH:
      return eAlgorithm::DH;
    case EVP_PKEY_DHX:
      return eAlgorithm::DHX;
  }
  return eAlgorithm::Unknown;
}

} //namespace MX

//-----------------------------------------------------------

static HRESULT GetCommon(_In_ EVP_PKEY *lpKey, _In_ lpfn_i2d_XXX_bio fn_i2d_XXX_bio, _Out_ MX::CSecureBuffer **lplpBuffer)
{
  MX::TAutoRefCounted<MX::CSecureBuffer> cBuffer;
  BIO *bio;
  HRESULT hRes;

  if (lplpBuffer == NULL)
    return E_POINTER;
  *lplpBuffer = NULL;

  if (lpKey == NULL)
    return MX_E_NotReady;

  cBuffer.Attach(MX_DEBUG_NEW MX::CSecureBuffer());
  if (!cBuffer)
    return E_OUTOFMEMORY;
  bio = cBuffer->CreateBIO();
  if (bio == NULL)
    return E_OUTOFMEMORY;

  ERR_clear_error();
  if (fn_i2d_XXX_bio(bio, lpKey) <= 0)
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
