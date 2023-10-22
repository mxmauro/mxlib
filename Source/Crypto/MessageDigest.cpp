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
#include <intrin.h>
#pragma intrinsic(_InterlockedExchangeAdd)
static int _InterlockedExchangeAdd(volatile int *val, int value)
{
  return (int)_InterlockedExchangeAdd((volatile LONG *)val, (LONG)value);
}
#include <OpenSSL\evp.h>
#include <crypto\evp.h>

//-----------------------------------------------------------

typedef struct tagMD_DATA {
  EVP_MD_CTX *lpMdCtx;
  const EVP_MD *lpMd;
  EVP_PKEY *lpKey;
  BYTE aOutput[64];
  SIZE_T nOutputSize;
} MD_DATA, *LPMD_DATA;

#define md_data ((LPMD_DATA)lpInternalData)

//-------------------------------------------------------

static int CRC32_init(_In_ EVP_MD_CTX *ctx);
static int CRC32_update(_In_ EVP_MD_CTX *ctx, _In_ const void *data, _In_ size_t count);
static int CRC32_final(_In_ EVP_MD_CTX *ctx, _In_ unsigned char *md);

//-------------------------------------------------------

static const DWORD dwCRCTable[256] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};

typedef struct {
  DWORD dwCurrentCRC;
} CRC32_CTX, *LPCRC32_CTX;

static const evp_md_st EVP_MD_Crc32 = {
  NID_undef, //type
  NID_undef, //pkey_type
  4, //md_size
  0, //flags
  &CRC32_init,
  &CRC32_update,
  &CRC32_final,
  NULL,
  NULL,
  4, //block_size
  sizeof(EVP_MD*) + sizeof(CRC32_CTX)
};

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
  if (nAlgorithm == (MX::CMessageDigest::eAlgorithm) - 1)
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
  switch (nAlgorithm)
  {
    case MX::CMessageDigest::eAlgorithm::CRC32:
      md_data->lpMd = &EVP_MD_Crc32;
      break;

    case MX::CMessageDigest::eAlgorithm::MD5:
      md_data->lpMd = EVP_md5();
      break;

    case MX::CMessageDigest::eAlgorithm::MD4:
      md_data->lpMd = EVP_md4();
      break;

    case MX::CMessageDigest::eAlgorithm::SHA1:
      md_data->lpMd = EVP_sha1();
      break;

    case MX::CMessageDigest::eAlgorithm::SHA224:
      md_data->lpMd = EVP_sha224();
      break;

    case MX::CMessageDigest::eAlgorithm::SHA256:
      md_data->lpMd = EVP_sha256();
      break;

    case MX::CMessageDigest::eAlgorithm::SHA384:
      md_data->lpMd = EVP_sha384();
      break;

    case MX::CMessageDigest::eAlgorithm::SHA512:
      md_data->lpMd = EVP_sha512();
      break;

    case MX::CMessageDigest::eAlgorithm::SHA512_224:
      md_data->lpMd = EVP_sha512_224();
      break;

    case MX::CMessageDigest::eAlgorithm::SHA512_256:
      md_data->lpMd = EVP_sha512_256();
      break;

    case MX::CMessageDigest::eAlgorithm::SHA3_224:
      md_data->lpMd = EVP_sha3_224();
      break;

    case MX::CMessageDigest::eAlgorithm::SHA3_256:
      md_data->lpMd = EVP_sha3_256();
      break;

    case MX::CMessageDigest::eAlgorithm::SHA3_384:
      md_data->lpMd = EVP_sha3_384();
      break;

    case MX::CMessageDigest::eAlgorithm::SHA3_512:
      md_data->lpMd = EVP_sha3_512();
      break;

    case MX::CMessageDigest::eAlgorithm::Blake2s_256:
      md_data->lpMd = EVP_blake2s256();
      break;

    case MX::CMessageDigest::eAlgorithm::Blake2b_512:
      md_data->lpMd = EVP_blake2b512();
      break;

    default:
      CleanUp(TRUE);
      return E_INVALIDARG;
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
    return (MX::CMessageDigest::eAlgorithm)-1;

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
  md_data->lpMd = NULL;
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

//-------------------------------------------------------

static int CRC32_init(_In_ EVP_MD_CTX *ctx)
{
  LPCRC32_CTX c = (LPCRC32_CTX)EVP_MD_CTX_md_data(ctx);

  c->dwCurrentCRC = 0;
  return 1;
}

static int CRC32_update(_In_ EVP_MD_CTX *ctx, _In_ const void *data, _In_ size_t count)
{
  if (count > 0)
  {
    LPCRC32_CTX c = (LPCRC32_CTX)EVP_MD_CTX_md_data(ctx);
    LPBYTE s = (LPBYTE)data;
    DWORD dwCRC = ~(c->dwCurrentCRC);

    while (count > 0)
    {
      dwCRC = dwCRCTable[(dwCRC ^ (*s++)) & 0xFF] ^ (dwCRC >> 8);
      count--;
    }
    c->dwCurrentCRC = ~dwCRC;
  }
  return 1;
}

static int CRC32_final(_In_ EVP_MD_CTX *ctx, _In_ unsigned char *md)
{
  LPCRC32_CTX c = (LPCRC32_CTX)EVP_MD_CTX_md_data(ctx);

  *((LPDWORD)md) = c->dwCurrentCRC;
  return 1;
}
