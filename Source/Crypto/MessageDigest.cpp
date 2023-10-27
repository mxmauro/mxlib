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
#include "..\..\Include\Crypto\MessageDigest.h"
#include "..\..\Include\AtomicOps.h"
#include "..\..\Include\Strings\Strings.h"
#include "InitOpenSSL.h"
#include <OpenSSL\evp.h>
#include <OpenSSL\core_names.h>

// #include <crypto\evp.h>

//-----------------------------------------------------------

typedef struct tagMD_DATA {
  EVP_MD_CTX *lpMdCtx;
  EVP_MD *lpMd;
  EVP_PKEY *lpKey;
  BYTE aOutput[64];
  SIZE_T nOutputSize;
} MD_DATA, *LPMD_DATA;

#define md_data ((LPMD_DATA)lpInternalData)

//-------------------------------------------------------

namespace MX {

CMessageDigest::CMessageDigest() : CBaseMemObj(), CNonCopyableObj()
{
  lpInternalData = NULL;
  return;
}

CMessageDigest::~CMessageDigest()
{
  if (lpInternalData != NULL)
  {
    CleanUp(TRUE);
    MxMemSet(lpInternalData, 0, sizeof(MD_DATA));
    MX_FREE(lpInternalData);
  }
  return;
}

HRESULT CMessageDigest::BeginDigest(_In_z_ LPCSTR szAlgorithmA, _In_opt_ LPCVOID lpKey, _In_opt_ SIZE_T nKeyLen)
{
  MX::CMessageDigest::eAlgorithm nAlgorithm;

  if (szAlgorithmA == NULL)
    return E_POINTER;
  nAlgorithm = GetAlgorithm(szAlgorithmA);
  if (nAlgorithm == MX::CMessageDigest::eAlgorithm::Invalid)
    return MX_E_Unsupported;
  return BeginDigest(nAlgorithm, lpKey, nKeyLen);
}

HRESULT CMessageDigest::BeginDigest(_In_ MX::CMessageDigest::eAlgorithm nAlgorithm, _In_opt_ LPCVOID lpKey,
                                    _In_opt_ SIZE_T nKeyLen)
{
  int ret;
  HRESULT hRes;

  if (lpKey == NULL && nKeyLen > 0)
    return E_POINTER;
  if (nKeyLen > 0x7FFFFFFFF)
    return E_INVALIDARG;
  if (nKeyLen > 0 && nAlgorithm == MX::CMessageDigest::eAlgorithm::CRC32)
    return MX_E_Unsupported;

  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;
  if (lpInternalData == NULL)
  {
    lpInternalData = MX_MALLOC(sizeof(MD_DATA));
    if (lpInternalData == NULL)
      return E_OUTOFMEMORY;
    MxMemSet(lpInternalData, 0, sizeof(MD_DATA));
  }
  CleanUp(TRUE);

  //create context
  md_data->lpMdCtx = EVP_MD_CTX_new();
  if (md_data->lpMdCtx == NULL)
  {
    CleanUp(TRUE);
    return E_OUTOFMEMORY;
  }

  //get digest manager
  ERR_clear_error();
  switch (nAlgorithm)
  {
    case MX::CMessageDigest::eAlgorithm::CRC32:
      md_data->lpMd = EVP_MD_fetch(NULL, "CRC32", "provider=mxlib");
      break;

    case MX::CMessageDigest::eAlgorithm::MD5:
      md_data->lpMd = EVP_MD_fetch(NULL, "MD5", NULL);
      break;

    case MX::CMessageDigest::eAlgorithm::MD4:
      md_data->lpMd = EVP_MD_fetch(NULL, "MD4", NULL);
      break;

    case MX::CMessageDigest::eAlgorithm::SHA1:
      md_data->lpMd = EVP_MD_fetch(NULL, "SHA1", NULL);
      break;

    case MX::CMessageDigest::eAlgorithm::SHA224:
      md_data->lpMd = EVP_MD_fetch(NULL, "SHA224", NULL);
      break;

    case MX::CMessageDigest::eAlgorithm::SHA256:
      md_data->lpMd = EVP_MD_fetch(NULL, "SHA256", NULL);
      break;

    case MX::CMessageDigest::eAlgorithm::SHA384:
      md_data->lpMd = EVP_MD_fetch(NULL, "SHA384", NULL);
      break;

    case MX::CMessageDigest::eAlgorithm::SHA512:
      md_data->lpMd = EVP_MD_fetch(NULL, "SHA512", NULL);
      break;

    case MX::CMessageDigest::eAlgorithm::SHA512_224:
      md_data->lpMd = EVP_MD_fetch(NULL, "SHA512-224", NULL);
      break;

    case MX::CMessageDigest::eAlgorithm::SHA512_256:
      md_data->lpMd = EVP_MD_fetch(NULL, "SHA512-256", NULL);
      break;

    case MX::CMessageDigest::eAlgorithm::SHA3_224:
      md_data->lpMd = EVP_MD_fetch(NULL, "SHA3-224", NULL);
      break;

    case MX::CMessageDigest::eAlgorithm::SHA3_256:
      md_data->lpMd = EVP_MD_fetch(NULL, "SHA3-256", NULL);
      break;

    case MX::CMessageDigest::eAlgorithm::SHA3_384:
      md_data->lpMd = EVP_MD_fetch(NULL, "SHA3-384", NULL);
      break;

    case MX::CMessageDigest::eAlgorithm::SHA3_512:
      md_data->lpMd = EVP_MD_fetch(NULL, "SHA3-512", NULL);
      break;

    case MX::CMessageDigest::eAlgorithm::Blake2s_256:
      md_data->lpMd = EVP_MD_fetch(NULL, "BLAKE2s256", NULL);
      break;

    case MX::CMessageDigest::eAlgorithm::Blake2b_512:
      md_data->lpMd = EVP_MD_fetch(NULL, "BLAKE2b512", NULL);
      break;

    default:
      CleanUp(TRUE);
      return E_INVALIDARG;
  }
  if (md_data->lpMd == NULL)
  {
    hRes = MX::Internals::OpenSSL::GetLastErrorCode(E_NOTIMPL);
    CleanUp(TRUE);
    return hRes;
  }

  //create key if needed
  if (nKeyLen > 0)
  {
    md_data->lpKey = EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, NULL, (const unsigned char *)lpKey, (int)nKeyLen);
    if (md_data->lpKey == NULL)
    {
      CleanUp(TRUE);
      return E_OUTOFMEMORY;
    }
  }
  //initialize
  if (md_data->lpMd != (EVP_MD *)1)
  {
    ret = (md_data->lpKey == NULL) ? EVP_DigestInit_ex(md_data->lpMdCtx, md_data->lpMd, NULL)
                                   : EVP_DigestSignInit(md_data->lpMdCtx, NULL, md_data->lpMd, NULL, md_data->lpKey);
    if (ret <= 0)
    {
      CleanUp(TRUE);
      return E_OUTOFMEMORY;
    }
  }
  //done
  return S_OK;
}

HRESULT CMessageDigest::DigestStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)
{
  int ret;

  if (lpData == NULL && nDataLength > 0)
    return E_POINTER;
  if (lpInternalData == NULL || md_data->lpMdCtx == NULL)
    return MX_E_NotReady;
  ret = EVP_DigestUpdate(md_data->lpMdCtx, lpData, nDataLength); //NOTE: EVP_DigestSignUpdate == EVP_DigestUpdate
  return (ret > 0) ? S_OK : E_FAIL;
}

HRESULT CMessageDigest::DigestWordLE(_In_ LPWORD lpnValues, _In_ SIZE_T nCount)
{
  return DigestStream(lpnValues, nCount * sizeof(WORD));
}

HRESULT CMessageDigest::DigestWordBE(_In_ LPWORD lpnValues, _In_ SIZE_T nCount)
{
  WORD aTempValues[32];
  SIZE_T i;
  HRESULT hRes;

  if (lpnValues == NULL && nCount > 0)
    return E_POINTER;

  hRes = S_OK;
  while (SUCCEEDED(hRes) && nCount > 0)
  {
    for (i=0; nCount>0 && i<MX_ARRAYLEN(aTempValues); i++,nCount--,lpnValues++)
    {
      aTempValues[i] = (((*lpnValues) & 0xFF00) >> 8) |
                       (((*lpnValues) & 0x00FF) << 8);
    }
    hRes = DigestStream(aTempValues, i * sizeof(WORD));
  }

  //done
  return hRes;
}

HRESULT CMessageDigest::DigestDWordLE(_In_ LPDWORD lpnValues, _In_ SIZE_T nCount)
{
  return DigestStream(lpnValues, nCount * sizeof(DWORD));
}

HRESULT CMessageDigest::DigestDWordBE(_In_ LPDWORD lpnValues, _In_ SIZE_T nCount)
{
  DWORD aTempValues[32];
  SIZE_T i;
  HRESULT hRes;

  if (lpnValues == NULL && nCount > 0)
    return E_POINTER;

  hRes = S_OK;
  while (SUCCEEDED(hRes) && nCount > 0)
  {
    for (i=0; nCount>0 && i<MX_ARRAYLEN(aTempValues); i++,nCount--,lpnValues++)
    {
      aTempValues[i] = (((*lpnValues) & 0xFF000000) >> 24) |
                       (((*lpnValues) & 0x00FF0000) >>  8) |
                       (((*lpnValues) & 0x0000FF00) <<  8) |
                       (((*lpnValues) & 0x000000FF) << 24);
    }
    hRes = DigestStream(aTempValues, i * sizeof(DWORD));
  }

  //done
  return hRes;
}

HRESULT CMessageDigest::DigestQWordLE(_In_ ULONGLONG *lpnValues, _In_ SIZE_T nCount)
{
  return DigestStream(lpnValues, nCount * sizeof(ULONGLONG));
}

HRESULT CMessageDigest::DigestQWordBE(_In_ ULONGLONG *lpnValues, _In_ SIZE_T nCount)
{
  ULONGLONG aTempValues[32];
  SIZE_T i;
  HRESULT hRes;

  if (lpnValues == NULL && nCount > 0)
    return E_POINTER;

  hRes = S_OK;
  while (SUCCEEDED(hRes) && nCount > 0)
  {
    for (i=0; nCount>0 && i<MX_ARRAYLEN(aTempValues); i++,nCount--,lpnValues++)
    {
      aTempValues[i] = (((*lpnValues) & 0xFF00000000000000ui64) >> 56) |
                       (((*lpnValues) & 0x00FF000000000000ui64) >> 40) |
                       (((*lpnValues) & 0x0000FF0000000000ui64) >> 24) |
                       (((*lpnValues) & 0x000000FF00000000ui64) >>  8) |
                       (((*lpnValues) & 0x00000000FF000000ui64) <<  8) |
                       (((*lpnValues) & 0x0000000000FF0000ui64) << 24) |
                       (((*lpnValues) & 0x000000000000FF00ui64) << 40) |
                       (((*lpnValues) & 0x00000000000000FFui64) << 56);
    }
    hRes = DigestStream(aTempValues, i * sizeof(ULONGLONG));
  }

  //done
  return hRes;
}

HRESULT CMessageDigest::EndDigest()
{
  union {
    unsigned int nOutputSizeUI;
    size_t nOutputSizeST;
  };
  int ret;

  if (lpInternalData == NULL || md_data->lpMdCtx == NULL)
    return MX_E_NotReady;

  if (md_data->lpKey == NULL)
  {
    ret = EVP_DigestFinal_ex(md_data->lpMdCtx, md_data->aOutput, &nOutputSizeUI);
    if (ret > 0)
      md_data->nOutputSize = (SIZE_T)nOutputSizeUI;
  }
  else
  {
    ret = EVP_DigestSignFinal(md_data->lpMdCtx, md_data->aOutput, &nOutputSizeST);
    if (ret > 0)
      md_data->nOutputSize = nOutputSizeST;
  }

  //done
  CleanUp(FALSE);
  return (ret > 0) ? S_OK : E_FAIL;
}

LPBYTE CMessageDigest::GetResult() const
{
  static BYTE aZero[64] = { 0 };

  return (lpInternalData != NULL) ? md_data->aOutput : aZero;
}

SIZE_T CMessageDigest::GetResultSize() const
{
  return (lpInternalData != NULL) ? md_data->nOutputSize : 0;
}

MX::CMessageDigest::eAlgorithm CMessageDigest::GetAlgorithm(_In_z_ LPCSTR szAlgorithmA)
{
  if (szAlgorithmA == NULL)
    return MX::CMessageDigest::eAlgorithm::Invalid;

  if (StrCompareA(szAlgorithmA, "crc32", TRUE) == 0)
  {
    return MX::CMessageDigest::eAlgorithm::CRC32;
  }
  else if (StrNCompareA(szAlgorithmA, "md", 2, TRUE) == 0)
  {
    szAlgorithmA += 2;

    if (StrCompareA(szAlgorithmA, "5") == 0)
      return MX::CMessageDigest::eAlgorithm::MD5;
    if (StrCompareA(szAlgorithmA, "4") == 0)
      return MX::CMessageDigest::eAlgorithm::MD4;
  }
  else if (StrNCompareA(szAlgorithmA, "sha", 3, TRUE) == 0)
  {
    szAlgorithmA += 3;
    if (*szAlgorithmA == '-')
      szAlgorithmA++;

    if (StrCompareA(szAlgorithmA, "1") == 0)
      return MX::CMessageDigest::eAlgorithm::SHA1;
    if (StrCompareA(szAlgorithmA, "2") == 0 || StrCompareA(szAlgorithmA, "256") == 0)
      return MX::CMessageDigest::eAlgorithm::SHA256;
    if (StrCompareA(szAlgorithmA, "224") == 0)
      return MX::CMessageDigest::eAlgorithm::SHA224;
    if (StrCompareA(szAlgorithmA, "384") == 0)
      return MX::CMessageDigest::eAlgorithm::SHA384;
    if (StrCompareA(szAlgorithmA, "512") == 0)
      return MX::CMessageDigest::eAlgorithm::SHA512;
    if (StrCompareA(szAlgorithmA, "512-256") == 0)
      return MX::CMessageDigest::eAlgorithm::SHA512_256;
    if (StrCompareA(szAlgorithmA, "512-224") == 0)
      return MX::CMessageDigest::eAlgorithm::SHA512_224;
    if (StrCompareA(szAlgorithmA, "3") == 0 || StrCompareA(szAlgorithmA, "3-256") == 0)
      return MX::CMessageDigest::eAlgorithm::SHA3_256;
    if (StrCompareA(szAlgorithmA, "3-224") == 0)
      return MX::CMessageDigest::eAlgorithm::SHA3_224;
    if (StrCompareA(szAlgorithmA, "3-384") == 0)
      return MX::CMessageDigest::eAlgorithm::SHA3_384;
    if (StrCompareA(szAlgorithmA, "3-512") == 0)
      return MX::CMessageDigest::eAlgorithm::SHA3_512;
  }
  else if (StrNCompareA(szAlgorithmA, "blake2", 6, TRUE) == 0)
  {
    szAlgorithmA += 6;

    if (StrCompareA(szAlgorithmA, "s-256") == 0)
      return MX::CMessageDigest::eAlgorithm::Blake2s_256;
    if (StrCompareA(szAlgorithmA, "b-512") == 0)
      return MX::CMessageDigest::eAlgorithm::Blake2b_512;
  }
  return MX::CMessageDigest::eAlgorithm::Invalid;
}

VOID CMessageDigest::CleanUp(_In_ BOOL bZeroData)
{
  if (md_data->lpMdCtx != NULL)
  {
    EVP_MD_CTX_free(md_data->lpMdCtx);
    md_data->lpMdCtx = NULL;
  }
  //----
  if (md_data->lpMd != NULL)
  {
    EVP_MD_free(md_data->lpMd);
    md_data->lpMd = NULL;
  }
  //----
  if (md_data->lpKey != NULL)
  {
    EVP_PKEY_free(md_data->lpKey);
    md_data->lpKey = NULL;
  }
  //----
  if (bZeroData != FALSE)
    MxMemSet(md_data->aOutput, 0, sizeof(md_data->aOutput));
  return;
}

} //namespace MX
