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
#include "..\..\Include\Crypto\DigestAlgorithmSHAx.h"
#include "InitOpenSSL.h"
#include <OpenSSL\evp.h>

//-----------------------------------------------------------

typedef struct tagSHAx_DATA {
  EVP_MD_CTX *lpMdCtx;
  const EVP_MD *lpMd;
  EVP_PKEY *lpKey;
  BYTE aOutput[64];
  SIZE_T nOutputSize;
} SHAx_DATA;

#define shax_data ((SHAx_DATA*)lpInternalData)

//-----------------------------------------------------------

static const EVP_MD* GetDigest(_In_ MX::CDigestAlgorithmSecureHash::eAlgorithm nAlgorithm);

//-------------------------------------------------------

namespace MX {

CDigestAlgorithmSecureHash::CDigestAlgorithmSecureHash() : CBaseDigestAlgorithm()
{
  lpInternalData = NULL;
  if (Internals::OpenSSL::Init() != FALSE)
  {
    lpInternalData = MX_MALLOC(sizeof(SHAx_DATA));
    if (lpInternalData != NULL)
      MemSet(lpInternalData, 0, sizeof(SHAx_DATA));
  }
  return;
}

CDigestAlgorithmSecureHash::~CDigestAlgorithmSecureHash()
{
  if (lpInternalData != NULL)
  {
    CleanUp(TRUE);
    MemSet(lpInternalData, 0, sizeof(SHAx_DATA));
    MX_FREE(lpInternalData);
  }
  return;
}

HRESULT CDigestAlgorithmSecureHash::BeginDigest()
{
  return BeginDigest(AlgorithmSHA1, NULL, 0);
}

HRESULT CDigestAlgorithmSecureHash::BeginDigest(_In_ eAlgorithm nAlgorithm, _In_opt_ LPCVOID lpKey,
                                                _In_opt_ SIZE_T nKeyLen)
{
  if (nAlgorithm != AlgorithmSHA1 && nAlgorithm != AlgorithmSHA224 && nAlgorithm != AlgorithmSHA256 &&
      nAlgorithm != AlgorithmSHA384 && nAlgorithm != AlgorithmSHA512)
    return E_INVALIDARG;
  if (lpKey == NULL && nKeyLen > 0)
    return E_POINTER;
  if (nKeyLen > 0x7FFFFFFFF)
    return E_INVALIDARG;
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  CleanUp(TRUE);
  //create context
  shax_data->lpMdCtx = EVP_MD_CTX_new();
  if (shax_data->lpMdCtx == NULL)
  {
    CleanUp(TRUE);
    return E_OUTOFMEMORY;
  }
  //get digest manager
  shax_data->lpMd = GetDigest(nAlgorithm);
  if (shax_data->lpMd == NULL)
  {
    CleanUp(TRUE);
    return E_NOTIMPL;
  }
  //create key if needed
  if (nKeyLen > 0)
  {
    shax_data->lpKey = EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, NULL, (const unsigned char *)lpKey, (int)nKeyLen);
    if (shax_data->lpKey == NULL)
    {
      CleanUp(TRUE);
      return E_OUTOFMEMORY;
    }
  }
  //initialize
  if (shax_data->lpKey == NULL)
  {
    if (!EVP_DigestInit_ex(shax_data->lpMdCtx, shax_data->lpMd, NULL))
    {
      CleanUp(TRUE);
      return E_OUTOFMEMORY;
    }
  }
  else
  {
    if (EVP_DigestSignInit(shax_data->lpMdCtx, NULL, shax_data->lpMd, NULL, shax_data->lpKey) <= 0)
    {
      CleanUp(TRUE);
      return E_OUTOFMEMORY;
    }
  }
  //done
  return S_OK;
}

HRESULT CDigestAlgorithmSecureHash::DigestStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)
{
  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (shax_data->lpMdCtx == NULL)
    return MX_E_NotReady;
  EVP_DigestUpdate(shax_data->lpMdCtx, lpData, nDataLength);
  return S_OK;
}

HRESULT CDigestAlgorithmSecureHash::EndDigest()
{
  unsigned int nOutputSize;

  if (lpInternalData == NULL)
    return E_OUTOFMEMORY;
  if (shax_data->lpMdCtx == NULL)
    return MX_E_NotReady;
  if (shax_data->lpKey == NULL)
    EVP_DigestFinal_ex(shax_data->lpMdCtx, shax_data->aOutput, &nOutputSize);
  else
    EVP_DigestFinal(shax_data->lpMdCtx, shax_data->aOutput, &nOutputSize);
  shax_data->nOutputSize = (SIZE_T)nOutputSize;
  CleanUp(FALSE);
  return S_OK;
}

LPBYTE CDigestAlgorithmSecureHash::GetResult() const
{
  static BYTE aZero[64] = { 0 };

  return (lpInternalData != NULL) ? shax_data->aOutput : aZero;
}

SIZE_T CDigestAlgorithmSecureHash::GetResultSize() const
{
  return (lpInternalData != NULL) ? shax_data->nOutputSize : 0;
}

VOID CDigestAlgorithmSecureHash::CleanUp(_In_ BOOL bZeroData)
{
  if (shax_data->lpMdCtx != NULL)
  {
    EVP_MD_CTX_free(shax_data->lpMdCtx);
    shax_data->lpMdCtx = NULL;
  }
  //----
  shax_data->lpMd = NULL;
  //----
  if (shax_data->lpKey != NULL)
  {
    EVP_PKEY_free(shax_data->lpKey);
    shax_data->lpKey = NULL;
  }
  //----
  if (bZeroData != FALSE)
    MemSet(shax_data->aOutput, 0, sizeof(shax_data->aOutput));
  return;
}

} //namespace MX

//-----------------------------------------------------------

static const EVP_MD* GetDigest(_In_ MX::CDigestAlgorithmSecureHash::eAlgorithm nAlgorithm)
{
  switch (nAlgorithm)
  {
    case MX::CDigestAlgorithmSecureHash::AlgorithmSHA1:
      return EVP_get_digestbyname("SHA1");
    case MX::CDigestAlgorithmSecureHash::AlgorithmSHA224:
      return EVP_get_digestbyname("SHA224");
    case MX::CDigestAlgorithmSecureHash::AlgorithmSHA256:
      return EVP_get_digestbyname("SHA256");
    case MX::CDigestAlgorithmSecureHash::AlgorithmSHA384:
      return EVP_get_digestbyname("SHA384");
    case MX::CDigestAlgorithmSecureHash::AlgorithmSHA512:
      return EVP_get_digestbyname("SHA512");
  }
  return NULL;
}
