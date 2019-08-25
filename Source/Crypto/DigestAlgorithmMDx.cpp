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
#include "..\..\Include\Crypto\DigestAlgorithmMDx.h"
#include "InitOpenSSL.h"
#include <OpenSSL\evp.h>

//-----------------------------------------------------------

typedef struct tagMDx_DATA {
  EVP_MD_CTX *lpMdCtx;
  const EVP_MD *lpMd;
  EVP_PKEY *lpKey;
  BYTE aOutput[16];
  SIZE_T nOutputSize;
} MDx_DATA;

#define mdx_data ((MDx_DATA*)lpInternalData)

//-----------------------------------------------------------

static const EVP_MD* GetDigest(_In_ MX::CDigestAlgorithmMessageDigest::eAlgorithm nAlgorithm);

//-------------------------------------------------------

namespace MX {

CDigestAlgorithmMessageDigest::CDigestAlgorithmMessageDigest() : CBaseDigestAlgorithm()
{
  lpInternalData = NULL;
  return;
}

CDigestAlgorithmMessageDigest::~CDigestAlgorithmMessageDigest()
{
  if (lpInternalData != NULL)
  {
    CleanUp(TRUE);
    MemSet(lpInternalData, 0, sizeof(MDx_DATA));
    MX_FREE(lpInternalData);
  }
  return;
}

HRESULT CDigestAlgorithmMessageDigest::BeginDigest()
{
  return BeginDigest(AlgorithmMD5, NULL, 0);
}

HRESULT CDigestAlgorithmMessageDigest::BeginDigest(_In_ eAlgorithm nAlgorithm, _In_opt_ LPCVOID lpKey,
                                                   _In_opt_ SIZE_T nKeyLen)
{
  int ret;
  HRESULT hRes;

  if (nAlgorithm != AlgorithmMD2 && nAlgorithm != AlgorithmMD4 && nAlgorithm != AlgorithmMD5)
    return E_INVALIDARG;
  if (lpKey == NULL && nKeyLen > 0)
    return E_POINTER;
  if (nKeyLen > 0x7FFFFFFFF)
    return E_INVALIDARG;
  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;
  if (lpInternalData == NULL)
  {
    lpInternalData = MX_MALLOC(sizeof(MDx_DATA));
    if (lpInternalData == NULL)
      return E_OUTOFMEMORY;
    MemSet(lpInternalData, 0, sizeof(MDx_DATA));
  }
  CleanUp(TRUE);
  //create context
  mdx_data->lpMdCtx = EVP_MD_CTX_new();
  if (mdx_data->lpMdCtx == NULL)
  {
    CleanUp(TRUE);
    return E_OUTOFMEMORY;
  }
  //get digest manager
  mdx_data->lpMd = GetDigest(nAlgorithm);
  if (mdx_data->lpMd == NULL)
  {
    CleanUp(TRUE);
    return E_NOTIMPL;
  }
  //create key if needed
  if (nKeyLen > 0)
  {
    mdx_data->lpKey = EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, NULL, (const unsigned char *)lpKey, (int)nKeyLen);
    if (mdx_data->lpKey == NULL)
    {
      CleanUp(TRUE);
      return E_OUTOFMEMORY;
    }
  }
  //initialize
  ret = (mdx_data->lpKey == NULL) ? EVP_DigestInit_ex(mdx_data->lpMdCtx, mdx_data->lpMd, NULL)
                                  : EVP_DigestSignInit(mdx_data->lpMdCtx, NULL, mdx_data->lpMd, NULL, mdx_data->lpKey);
  if (ret <= 0)
  {
    CleanUp(TRUE);
    return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CDigestAlgorithmMessageDigest::DigestStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)
{
  int ret;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (lpInternalData == NULL || mdx_data->lpMdCtx == NULL)
    return MX_E_NotReady;
  ret = EVP_DigestUpdate(mdx_data->lpMdCtx, lpData, nDataLength); //NOTE: EVP_DigestSignUpdate == EVP_DigestUpdate
  return (ret > 0) ? S_OK : E_FAIL;
}

HRESULT CDigestAlgorithmMessageDigest::EndDigest()
{
  union {
    unsigned int nOutputSizeUI;
    size_t nOutputSizeST;
  };
  int ret;

  if (lpInternalData == NULL || mdx_data->lpMdCtx == NULL)
    return MX_E_NotReady;
  if (mdx_data->lpKey == NULL)
  {
    ret = EVP_DigestFinal_ex(mdx_data->lpMdCtx, mdx_data->aOutput, &nOutputSizeUI);
    if (ret > 0)
      mdx_data->nOutputSize = (SIZE_T)nOutputSizeUI;
  }
  else
  {
    ret = EVP_DigestSignFinal(mdx_data->lpMdCtx, mdx_data->aOutput, &nOutputSizeST);
    if (ret > 0)
      mdx_data->nOutputSize = nOutputSizeST;
  }
  CleanUp(FALSE);
  return (ret > 0) ? S_OK : E_FAIL;
}

LPBYTE CDigestAlgorithmMessageDigest::GetResult() const
{
  static BYTE aZero[16] = { 0 };

  return (lpInternalData != NULL) ? mdx_data->aOutput : aZero;
}

SIZE_T CDigestAlgorithmMessageDigest::GetResultSize() const
{
  return (lpInternalData != NULL) ? mdx_data->nOutputSize : 0;
}

VOID CDigestAlgorithmMessageDigest::CleanUp(_In_ BOOL bZeroData)
{
  if (mdx_data->lpMdCtx != NULL)
  {
    EVP_MD_CTX_free(mdx_data->lpMdCtx);
    mdx_data->lpMdCtx = NULL;
  }
  //----
  mdx_data->lpMd = NULL;
  //----
  if (mdx_data->lpKey != NULL)
  {
    EVP_PKEY_free(mdx_data->lpKey);
    mdx_data->lpKey = NULL;
  }
  //----
  if (bZeroData != FALSE)
    MemSet(mdx_data->aOutput, 0, sizeof(mdx_data->aOutput));
  return;
}

} //namespace MX

//-----------------------------------------------------------

static const EVP_MD* GetDigest(_In_ MX::CDigestAlgorithmMessageDigest::eAlgorithm nAlgorithm)
{
  switch (nAlgorithm)
  {
  case MX::CDigestAlgorithmMessageDigest::AlgorithmMD2:
    return EVP_get_digestbyname("MD2");
  case MX::CDigestAlgorithmMessageDigest::AlgorithmMD4:
    return EVP_get_digestbyname("MD4");
  case MX::CDigestAlgorithmMessageDigest::AlgorithmMD5:
    return EVP_get_digestbyname("MD5");
  }
  return NULL;
}
