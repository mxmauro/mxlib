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
#include "..\..\Include\Crypto\DhParam.h"
#include "..\..\Include\Strings\Strings.h"
#include "InitOpenSSL.h"

//-----------------------------------------------------------

namespace MX {

CDhParam::CDhParam() : TRefCounted<CBaseMemObj>()
{
  return;
}

CDhParam::CDhParam(_In_ const CDhParam &cSrc) throw(...) : TRefCounted<CBaseMemObj>()
{
  operator=(cSrc);
  return;
}

CDhParam::~CDhParam()
{
  if (lpDH != NULL)
  {
    DH_free(lpDH);
    lpDH = NULL;
  }
  return;
}

CDhParam& CDhParam::operator=(_In_ const CDhParam &cSrc) throw(...)
{
  if (this != &cSrc)
  {
    HRESULT hRes;

    hRes = Internals::OpenSSL::Init();
    if (FAILED(hRes))
      throw (LONG)hRes;
    //deref old
    if (lpDH != NULL)
      DH_free(lpDH);
    //increment reference on new
    if (cSrc.lpDH != NULL)
      DH_up_ref(cSrc.lpDH);
    lpDH = cSrc.lpDH;
  }
  return *this;
}

HRESULT CDhParam::Generate(_In_ SIZE_T nBitsCount, _In_opt_ BOOL bUseDSA)
{
  DH *lpNewDH;
  HRESULT hRes;

  if (nBitsCount != 1024 && nBitsCount != 2048 && nBitsCount != 4096)
    return E_INVALIDARG;

  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;

  if (bUseDSA != FALSE)
  {
    DSA *lpDSA = DSA_new();
    if (lpDSA == NULL)
      return E_OUTOFMEMORY;
    if (::DSA_generate_parameters_ex(lpDSA, (int)nBitsCount, NULL, 0, NULL, NULL, NULL) == 0)
    {
      DSA_free(lpDSA);
      return E_OUTOFMEMORY;
    }
    lpNewDH = DSA_dup_DH(lpDSA);
    DSA_free(lpDSA);
    if (lpNewDH == NULL)
      return E_OUTOFMEMORY;
  }
  else
  {
    lpNewDH = DH_new();
    if (lpNewDH == NULL)
      return E_OUTOFMEMORY;
    if (DH_generate_parameters_ex(lpDH, (int)nBitsCount, 2, NULL) == 0)
    {
      DH_free(lpNewDH);
      return E_OUTOFMEMORY;
    }
  }

  //replace
  if (lpDH != NULL)
    DH_free(lpDH);
  lpDH = lpNewDH;

  //done
  return S_OK;
}

SIZE_T CDhParam::GetBitsCount() const
{
  int size;

  if (lpDH == NULL)
    return 0;
  size = DH_bits(lpDH);
  return (size > 0) ? (SIZE_T)size : 0;
}

HRESULT CDhParam::SetFromDER(_In_ LPCVOID lpDer, _In_ SIZE_T nDerSize)
{
  DH *lpNewDH;
  BIO *bio;
  HRESULT hRes;

  if (lpDer == NULL)
    return E_POINTER;
  if (nDerSize == 0 || nDerSize > 0x7FFFFFFF)
    return E_INVALIDARG;

  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;

  bio = BIO_new_mem_buf(lpDer, (int)nDerSize);
  if (bio == NULL)
    return E_OUTOFMEMORY;

  ERR_clear_error();
  lpNewDH = d2i_DHparams_bio(bio, NULL);
  if (lpNewDH == NULL)
  {
    hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
    BIO_free(bio);
    return hRes;
  }
  BIO_free(bio);

  //replace
  if (lpDH != NULL)
    DH_free(lpDH);
  lpDH = lpNewDH;

  //done
  return S_OK;
}

HRESULT CDhParam::SetFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA, _In_opt_ SIZE_T nPemLen)
{
  DH *lpNewDH;
  BIO *bio;
  HRESULT hRes;

  if (szPemA == NULL)
    return E_POINTER;
  if (nPemLen == (SIZE_T)-1)
    nPemLen = StrLenA(szPemA);
  if (nPemLen == 0 || nPemLen > 0x7FFFFFFF)
    return E_INVALIDARG;

  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;

  bio = BIO_new_mem_buf(szPemA, (int)nPemLen);
  if (bio == NULL)
    return E_OUTOFMEMORY;

  lpNewDH = PEM_read_bio_DHparams(bio, NULL, 0, (void *)szPasswordA);
  if (lpNewDH == NULL)
  {
    hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
    BIO_free(bio);
    return hRes;
  }
  BIO_free(bio);

  //replace
  if (lpDH != NULL)
    DH_free(lpDH);
  lpDH = lpNewDH;

  //done
  return S_OK;
}

HRESULT CDhParam::Get(_Out_ CSecureBuffer **lplpBuffer)
{
  TAutoRefCounted<CSecureBuffer> cBuffer;
  BIO *bio;
  const BIGNUM *q;
  int ret;
  HRESULT hRes;

  if (lplpBuffer == NULL)
    return E_POINTER;
  *lplpBuffer = NULL;

  if (lpDH == NULL)
    return MX_E_NotReady;

  cBuffer.Attach(MX_DEBUG_NEW CSecureBuffer());
  if (!cBuffer)
    return E_OUTOFMEMORY;
  bio = cBuffer->CreateBIO();
  if (bio == NULL)
    return E_OUTOFMEMORY;

  DH_get0_pqg(lpDH, NULL, &q, NULL);

  ERR_clear_error();
  if (q != NULL)
    ret = ASN1_i2d_bio((i2d_of_void*)i2d_DHxparams, bio, (unsigned char *)lpDH); //ret = i2d_DHxparams_bio(bio, x);
  else
    ret = ASN1_i2d_bio((i2d_of_void *)i2d_DHparams, bio, (unsigned char *)lpDH); //ret = i2d_DHparams_bio(bio, x);
  if (ret <= 0)
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

} //namespace MX
