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

CDigestAlgorithmSecureHash::CDigestAlgorithmSecureHash() : CDigestAlgorithmBase(), CNonCopyableObj()
{
  lpInternalData = NULL;
  return;
}

CDigestAlgorithmSecureHash::~CDigestAlgorithmSecureHash()
{
  if (lpInternalData != NULL)
  {
    CleanUp(TRUE);
    MxMemSet(lpInternalData, 0, sizeof(SHAx_DATA));
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
  int ret;
  HRESULT hRes;

  if (nAlgorithm != AlgorithmSHA1 && nAlgorithm != AlgorithmSHA224 && nAlgorithm != AlgorithmSHA256 &&
      nAlgorithm != AlgorithmSHA384 && nAlgorithm != AlgorithmSHA512)
  {
    return E_INVALIDARG;
  }
  if (lpKey == NULL && nKeyLen > 0)
    return E_POINTER;
  if (nKeyLen > 0x7FFFFFFFF)
    return E_INVALIDARG;
  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;
  if (lpInternalData == NULL)
  {
    lpInternalData = MX_MALLOC(sizeof(SHAx_DATA));
    if (lpInternalData == NULL)
      return E_OUTOFMEMORY;
    MxMemSet(lpInternalData, 0, sizeof(SHAx_DATA));
  }
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
  ret = (shax_data->lpKey == NULL) ? EVP_DigestInit_ex(shax_data->lpMdCtx, shax_data->lpMd, NULL)
                                   : EVP_DigestSignInit(shax_data->lpMdCtx, NULL, shax_data->lpMd,
                                                        NULL, shax_data->lpKey);
  if (ret <= 0)
  {
    CleanUp(TRUE);
    return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CDigestAlgorithmSecureHash::DigestStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)
{
  int ret;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (lpInternalData == NULL || shax_data->lpMdCtx == NULL)
    return MX_E_NotReady;
  ret = EVP_DigestUpdate(shax_data->lpMdCtx, lpData, nDataLength); //NOTE: EVP_DigestSignUpdate == EVP_DigestUpdate
  return (ret > 0) ? S_OK : E_FAIL;
}

HRESULT CDigestAlgorithmSecureHash::EndDigest()
{
  union {
    unsigned int nOutputSizeUI;
    size_t nOutputSizeST;
  };
  int ret;

  if (lpInternalData == NULL || shax_data->lpMdCtx == NULL)
    return MX_E_NotReady;
  if (shax_data->lpKey == NULL)
  {
    ret = EVP_DigestFinal_ex(shax_data->lpMdCtx, shax_data->aOutput, &nOutputSizeUI);
    if (ret > 0)
      shax_data->nOutputSize = (SIZE_T)nOutputSizeUI;
  }
  else
  {
    ret = EVP_DigestSignFinal(shax_data->lpMdCtx, shax_data->aOutput, &nOutputSizeST);
    if (ret > 0)
      shax_data->nOutputSize = nOutputSizeST;
  }
  CleanUp(FALSE);
  return (ret > 0) ? S_OK : E_FAIL;
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
    MxMemSet(shax_data->aOutput, 0, sizeof(shax_data->aOutput));
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
