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
#include "..\..\Include\Comm\SslCertificates.h"
#include "..\..\Include\AutoHandle.h"
#include "..\..\Include\Crypto\Base64.h"
#include "..\..\Include\Crypto\SecureBuffer.h"
#include "..\Crypto\InitOpenSSL.h"
#include "..\..\Include\Strings\Utf8.h"
#include "..\..\Include\AutoPtr.h"
#include "..\Internals\SystemDll.h"
#include <wincrypt.h>
#include <OpenSSL\des.h>
#include <OpenSSL\aes.h>
#include <OpenSSL\err.h>
#include <OpenSSL\evp.h>
#include <OpenSSL\pkcs12.h>

//-----------------------------------------------------------

#define BLOCK_TYPE_PublicKey                               1
#define BLOCK_TYPE_PrivateKey                              2
#define BLOCK_TYPE_RsaPublicKey                            3
#define BLOCK_TYPE_RsaPrivateKey                           4
#define BLOCK_TYPE_Certificate                             5
#define BLOCK_TYPE_OldCertificate                          6
#define BLOCK_TYPE_Crl                                     7

#define _x509                                ((X509*)lpX509)
#define _x509Crl                      ((X509_CRL*)lpX509Crl)

//-----------------------------------------------------------

typedef struct {
  SIZE_T nType;
  SIZE_T nStart, nLen;
} PEM_BLOCK;

typedef struct {
  SIZE_T nTag;
  SIZE_T nLen;
  LPBYTE lpPtr;
} X509_BLOCK;

//-----------------------------------------------------------

static const struct {
  SIZE_T nBlockType;
  LPCSTR szNameA;
} aPemTypes[] = {
  { BLOCK_TYPE_PublicKey,      "PUBLIC KEY" },
  { BLOCK_TYPE_PrivateKey,     "PRIVATE KEY" },
  { BLOCK_TYPE_RsaPublicKey,   "RSA PUBLIC KEY" },
  { BLOCK_TYPE_RsaPrivateKey,  "RSA PRIVATE KEY" },
  { BLOCK_TYPE_Certificate,    "CERTIFICATE" },
  { BLOCK_TYPE_OldCertificate, "X509 CERTIFICATE" },
  { BLOCK_TYPE_Crl,            "X509 CRL" }
};

//-----------------------------------------------------------

static int InitializeFromPEM_PasswordCallback(char *buf, int size, int rwflag, void *userdata);
static BOOL Asn1_ValidateLength(_Inout_ LPBYTE lpCurr, _In_ LPBYTE lpEnd);

//-----------------------------------------------------------

namespace MX {

CSslCertificateArray::CSslCertificateArray() : TRefCounted<CBaseMemObj>(), CNonCopyableObj()
{
  return;
}

CSslCertificateArray::~CSslCertificateArray()
{
  return;
}

VOID CSslCertificateArray::Reset()
{
  cCertsList.RemoveAllElements();
  cCertCrlsList.RemoveAllElements();
  cKeysList.RemoveAllElements();
  return;
}

HRESULT CSslCertificateArray::AddFromMemory(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen, _In_opt_z_ LPCSTR szPasswordA)
{
  HRESULT hRes;

  if (lpData == NULL && nDataLen > 0)
    return E_POINTER;
  if (nDataLen == 0)
    return S_OK;
  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;
  if (((LPBYTE)lpData)[0] == (V_ASN1_CONSTRUCTED | V_ASN1_SEQUENCE) &&
      Asn1_ValidateLength((LPBYTE)lpData + 1, (LPBYTE)lpData + nDataLen) != FALSE)
  {
    hRes = AddCertificateFromDER(lpData, nDataLen, szPasswordA);
    if (hRes == MX_E_InvalidData)
      hRes = AddCrlFromDER(lpData, nDataLen);
    if (hRes == MX_E_InvalidData)
      hRes = AddPrivateKeyFromDER(lpData, nDataLen);
    if (hRes == MX_E_InvalidData)
      hRes = AddPublicKeyFromDER(lpData, nDataLen);
    if (FAILED(hRes))
      return hRes;
  }
  else
  {
    MX::TAutoFreePtr<BYTE> cFastAnsiCopy;
    LPCSTR sA, szEndA, szBlockEndA;

    //assume PEM data
    sA = (LPCSTR)lpData;

    //is unicode?
    if (nDataLen >= 2 && ((LPBYTE)lpData)[0] == 0xFF && ((LPBYTE)lpData)[1] == 0xFE)
    {
      //unicode... quick convert to ansi
      LPBYTE s;
      LPCWSTR sW;
      SIZE_T i;

      if (nDataLen == 2)
        return S_OK;
      nDataLen = (nDataLen - 2) >> 1;
      cFastAnsiCopy.Attach((LPBYTE)MX_MALLOC(nDataLen));
      if (!cFastAnsiCopy)
        return E_OUTOFMEMORY;
      sW = (LPCWSTR)((LPBYTE)lpData + 2);
      s = cFastAnsiCopy.Get();
      //NOTE: unicode chars are converted to a pipe char because they are invalid too but inside ansi subset
      for (i=0; i<nDataLen; i++)
        s[i] = ((USHORT)(sW[i]) < 128) ? (BYTE)sW[i] : (BYTE)'|';
      //set new pointer
      sA = (LPCSTR)(cFastAnsiCopy.Get());
    }

    //process
    szEndA = sA + nDataLen;
    for (;;)
    {
      nDataLen = (SIZE_T)(szEndA - sA);
      sA = StrNFindA(sA, "-----BEGIN ", nDataLen);
      if (sA == NULL)
        break;
      if (nDataLen >= 15 && StrNCompareA(sA + 5, "PUBLIC KEY-----", 15) == 0)
      {
        szBlockEndA = StrNFindA(sA + (11+15), "-----END PUBLIC KEY-----", (SIZE_T)(szEndA - (sA + (11+15))));
        if (szBlockEndA == NULL)
          return MX_E_InvalidData;
        szBlockEndA += 24;
        //----
        hRes = AddPublicKeyFromPEM(sA, szPasswordA, (SIZE_T)(szBlockEndA - sA));
        if (FAILED(hRes))
          return hRes;
        sA = szBlockEndA;
      }
      else if (nDataLen >= 19 && StrNCompareA(sA + 5, "RSA PUBLIC KEY-----", 19) == 0)
      {
        szBlockEndA = StrNFindA(sA + (11+19), "-----END RSA PUBLIC KEY-----", (SIZE_T)(szEndA - (sA + (11+19))));
        if (szBlockEndA == NULL)
          return MX_E_InvalidData;
        szBlockEndA += 28;
        //----
        hRes = AddPublicKeyFromPEM(sA, szPasswordA, (SIZE_T)(szBlockEndA - sA));
        if (FAILED(hRes))
          return hRes;
        sA = szBlockEndA;
      }
      else if (nDataLen >= 16 && StrNCompareA(sA + 5, "PRIVATE KEY-----", 16) == 0)
      {
        szBlockEndA = StrNFindA(sA + (11+16), "-----END PRIVATE KEY-----", (SIZE_T)(szEndA - (sA + (11+16))));
        if (szBlockEndA == NULL)
          return MX_E_InvalidData;
        szBlockEndA += 25;
        //----
        hRes = AddPrivateKeyFromPEM(sA, szPasswordA, (SIZE_T)(szBlockEndA - sA));
        if (FAILED(hRes))
          return hRes;
        sA = szBlockEndA;
      }
      else if (nDataLen >= 20 && StrNCompareA(sA + 5, "RSA PRIVATE KEY-----", 20) == 0)
      {
        szBlockEndA = StrNFindA(sA + (11+20), "-----END RSA PRIVATE KEY-----", (SIZE_T)(szEndA - (sA + (11+20))));
        if (szBlockEndA == NULL)
          return MX_E_InvalidData;
        szBlockEndA += 29;
        //----
        hRes = AddPrivateKeyFromPEM(sA, szPasswordA, (SIZE_T)(szBlockEndA - sA));
        if (FAILED(hRes))
          return hRes;
        sA = szBlockEndA;
      }
      else if (nDataLen >= 16 && StrNCompareA(sA + 5, "CERTIFICATE-----", 16) == 0)
      {
        szBlockEndA = StrNFindA(sA + (11+16), "-----END CERTIFICATE-----", (SIZE_T)(szEndA - (sA + (11+16))));
        if (szBlockEndA == NULL)
          return MX_E_InvalidData;
        szBlockEndA += 25;
        //----
        hRes = AddCertificateFromPEM(sA, szPasswordA, (SIZE_T)(szBlockEndA - sA));
        if (FAILED(hRes))
          return hRes;
        sA = szBlockEndA;
      }
      else if (nDataLen >= 21 && StrNCompareA(sA + 5, "X509 CERTIFICATE-----", 21) == 0)
      {
        szBlockEndA = StrNFindA(sA + (11+21), "-----END X509 CERTIFICATE-----", (SIZE_T)(szEndA - (sA + (11+21))));
        if (szBlockEndA == NULL)
          return MX_E_InvalidData;
        szBlockEndA += 30;
        //----
        hRes = AddCertificateFromPEM(sA, szPasswordA, (SIZE_T)(szBlockEndA - sA));
        if (FAILED(hRes))
          return hRes;
        sA = szBlockEndA;
      }
      else if (nDataLen >= 13 && StrNCompareA(sA + 5, "X509 CRL-----", 13) == 0)
      {
        szBlockEndA = StrNFindA(sA + (11+13), "-----END X509 CRL-----", (SIZE_T)(szEndA - (sA + (11+13))));
        if (szBlockEndA == NULL)
          return MX_E_InvalidData;
        szBlockEndA += 22;
        //----
        hRes = AddCrlFromPEM(sA, szPasswordA, (SIZE_T)(szBlockEndA - sA));
        if (FAILED(hRes))
          return hRes;
        sA = szBlockEndA;
      }
      else
      {
        sA += 11;
      }
    }
  }
  //done
  return S_OK;
}

HRESULT CSslCertificateArray::AddFromFile(_In_z_ LPCWSTR szFileNameW, _In_opt_z_ LPCSTR szPasswordA)
{
  CWindowsHandle cFileH;
  DWORD dwSizeLo, dwSizeHi, dwReaded;
  TAutoFreePtr<BYTE> aTempBuf;

  if (szFileNameW == NULL)
    return E_POINTER;
  if (*szFileNameW == 0)
    return E_INVALIDARG;
  //open file
  cFileH.Attach(::CreateFileW(szFileNameW, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              NULL));
  if (!cFileH)
    return MX_HRESULT_FROM_LASTERROR();
  dwSizeLo = ::GetFileSize(cFileH, &dwSizeHi);
  if (dwSizeHi != 0 || dwSizeLo == 0)
    return MX_E_InvalidData;
  //allocate temporary storage and read file
  aTempBuf.Attach((LPBYTE)MX_MALLOC((SIZE_T)dwSizeLo));
  if (!aTempBuf)
    return E_OUTOFMEMORY;
  if (::ReadFile(cFileH, aTempBuf.Get(), dwSizeLo, &dwReaded, NULL) == FALSE)
    return MX_HRESULT_FROM_LASTERROR();
  if (dwReaded != dwSizeLo)
    return MX_E_ReadFault;
  //process certificates
  return AddFromMemory(aTempBuf.Get(), (SIZE_T)dwSizeLo, szPasswordA);
}

HRESULT CSslCertificateArray::AddPublicKeyFromDER(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen)
{
  TAutoRefCounted<CEncryptionKey> cKey;
  HRESULT hRes;

  //create object
  cKey.Attach(MX_DEBUG_NEW CEncryptionKey());
  if (!cKey)
    return E_OUTOFMEMORY;
  //set public key
  hRes = cKey->SetPublicKeyFromDER(lpData, nDataLen);
  if (SUCCEEDED(hRes))
  {
    //add to list
    if (cKeysList.AddElement(cKey.Get()) != FALSE)
      cKey.Detach();
    else
      hRes = E_OUTOFMEMORY;
  }
  //done
  return hRes;
}

HRESULT CSslCertificateArray::AddPublicKeyFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA,
                                                  _In_opt_ SIZE_T nPemLen)
{
  TAutoRefCounted<CEncryptionKey> cKey;
  HRESULT hRes;

  //create object
  cKey.Attach(MX_DEBUG_NEW CEncryptionKey());
  if (!cKey)
    return E_OUTOFMEMORY;
  //set public key
  hRes = cKey->SetPublicKeyFromPEM(szPemA, szPasswordA, nPemLen);
  if (SUCCEEDED(hRes))
  {
    //add to list
    if (cKeysList.AddElement(cKey.Get()) != FALSE)
      cKey.Detach();
    else
      hRes = E_OUTOFMEMORY;
  }
  //done
  return hRes;
}

HRESULT CSslCertificateArray::AddPrivateKeyFromDER(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen)
{
  TAutoRefCounted<CEncryptionKey> cKey;
  HRESULT hRes;

  //create object
  cKey.Attach(MX_DEBUG_NEW CEncryptionKey());
  if (!cKey)
    return E_OUTOFMEMORY;
  //set private key
  hRes = cKey->SetPrivateKeyFromDER(lpData, nDataLen);
  if (SUCCEEDED(hRes))
  {
    //add to list
    if (cKeysList.AddElement(cKey.Get()) != FALSE)
      cKey.Detach();
    else
      hRes = E_OUTOFMEMORY;
  }
  //done
  return hRes;
}

HRESULT CSslCertificateArray::AddPrivateKeyFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA,
                                                   _In_opt_ SIZE_T nPemLen)
{
  TAutoRefCounted<CEncryptionKey> cKey;
  HRESULT hRes;

  //create object
  cKey.Attach(MX_DEBUG_NEW CEncryptionKey());
  if (!cKey)
    return E_OUTOFMEMORY;
  //set public key
  hRes = cKey->SetPrivateKeyFromPEM(szPemA, szPasswordA, nPemLen);
  if (SUCCEEDED(hRes))
  {
    //add to list
    if (cKeysList.AddElement(cKey.Get()) != FALSE)
      cKey.Detach();
    else
      hRes = E_OUTOFMEMORY;
  }
  //done
  return hRes;
}

HRESULT CSslCertificate::InitializeFromDER(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen)
{
  X509 *lpNewX509;
  BIO *lpBio;
  HRESULT hRes;

  if (lpData == NULL)
    return E_POINTER;
  if (nDataLen == 0 || nDataLen > 0x7FFFFFFF)
    return E_INVALIDARG;
  //----
  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;

  ERR_clear_error();
  lpBio = BIO_new_mem_buf((void *)lpData, (int)nDataLen);
  if (lpBio == NULL)
    return E_OUTOFMEMORY;

  ERR_clear_error();
  lpNewX509 = d2i_X509_bio(lpBio, NULL);
  BIO_free(lpBio);
  if (lpNewX509 == NULL)
    return MX_E_InvalidData;

  //done
  if (lpX509)
    X509_free(_x509);
  lpX509 = lpNewX509;
  return S_OK;
}

HRESULT CSslCertificate::InitializeFromPEM(_In_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA, _In_opt_ SIZE_T nPemLen)
{
  X509 *lpNewX509;
  BIO *lpBio;
  HRESULT hRes;

  if (szPemA == NULL)
    return E_POINTER;
  if (nPemLen == (SIZE_T)-1)
    nPemLen = StrLenA(szPemA);
  if (nPemLen == 0 || nPemLen > 0x7FFFFFFF)
    return E_INVALIDARG;
  //----
  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;
  ERR_clear_error();
  lpBio = BIO_new_mem_buf((void*)szPemA, (int)nPemLen);
  if (lpBio == NULL)
    return E_OUTOFMEMORY;
  lpNewX509 = PEM_read_bio_X509(lpBio, NULL, &InitializeFromPEM_PasswordCallback, (LPSTR)szPasswordA);
  BIO_free(lpBio);
  if (lpNewX509 == NULL)
    return MX_E_InvalidData;
  //done
  if (lpX509)
    X509_free(_x509);
  lpX509 = lpNewX509;
  return S_OK;
}

HRESULT CSslCertificateArray::AddCertificateFromDER(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen,
                                                    _In_opt_z_ LPCSTR szPasswordA)
{
  TAutoRefCounted<CSslCertificate> cNewCert;
  TAutoRefCounted<CEncryptionKey> cNewKey;
  PKCS12 *lpPkcs12;
  const unsigned char *buf;
  HRESULT hRes;

  if (lpData == NULL)
    return E_POINTER;
  if (nDataLen == 0 || nDataLen > 0x7FFFFFFF)
    return E_INVALIDARG;

  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;

  //first check if we are dealing with a PKCS12 certificate
  ERR_clear_error();
  buf = (const unsigned char *)lpData;
  lpPkcs12 = d2i_PKCS12(NULL, &buf, (long)nDataLen);
  if (lpPkcs12 != NULL)
  {
    EVP_PKEY *lpEvpKey = NULL;
    X509 *lpX509 = NULL;

    //a pkcs12 can have a certificate and a private key so parse both
    ERR_clear_error();
    if (PKCS12_parse(lpPkcs12, szPasswordA, &lpEvpKey, &lpX509, NULL) > 0 &&
        (lpEvpKey != NULL || lpX509 != NULL))
    {
      hRes = S_OK;

      if (lpX509 != NULL)
      {
        CSecureBuffer cTempBuffer;
        BIO *lpBio;

        lpBio = cTempBuffer.CreateBIO();
        if (lpBio != NULL)
        {
          ERR_clear_error();
          if (i2d_X509_bio(lpBio, lpX509) > 0)
          {
            //create certificate from temporary buffer
            cNewCert.Attach(MX_DEBUG_NEW CSslCertificate());
            if (cNewCert)
            {
              hRes = cNewCert->InitializeFromDER(cTempBuffer.GetBuffer(), cTempBuffer.GetLength());
              if (SUCCEEDED(hRes))
              {
                if (cCertsList.AddElement(cNewCert.Get()) != FALSE)
                  cNewCert.Detach();
                else
                  hRes = E_OUTOFMEMORY;
              }
            }
            else
            {
              hRes = E_OUTOFMEMORY;
            }
          }
          else
          {
            hRes = Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
          }

          BIO_free(lpBio);
        }
        else
        {
          hRes = E_OUTOFMEMORY;
        }

        X509_free(lpX509);

        if (FAILED(hRes))
        {
          if (lpEvpKey != NULL)
            EVP_PKEY_free(lpEvpKey);
          PKCS12_free(lpPkcs12);
          return hRes;
        }
      }

      if (lpEvpKey != NULL)
      {
        CSecureBuffer cTempBuffer;
        BIO *lpBio;

        lpBio = cTempBuffer.CreateBIO();
        if (lpBio != NULL)
        {
          ERR_clear_error();
          if (i2d_PrivateKey_bio(lpBio, lpEvpKey) > 0)
          {
            //create certificate from temporary buffer
            cNewKey.Attach(MX_DEBUG_NEW CEncryptionKey());
            if (cNewKey)
            {
              hRes = cNewKey->SetPrivateKeyFromDER(cTempBuffer.GetBuffer(), cTempBuffer.GetLength());
              if (SUCCEEDED(hRes))
              {
                if (cKeysList.AddElement(cNewKey.Get()) != FALSE)
                  cNewKey.Detach();
                else
                  hRes = E_OUTOFMEMORY;
              }
            }
            else
            {
              hRes = E_OUTOFMEMORY;
            }
          }
          else
          {
            hRes = MX::Internals::OpenSSL::GetLastErrorCode(MX_E_InvalidData);
          }

          BIO_free(lpBio);
        }

        EVP_PKEY_free(lpEvpKey);

        if (FAILED(hRes))
        {
          PKCS12_free(lpPkcs12);
          return hRes;
        }
      }

      PKCS12_free(lpPkcs12);

      //done
      return S_OK;
    }

    hRes = Internals::OpenSSL::GetLastErrorCode(S_OK);
    PKCS12_free(lpPkcs12);
    if (hRes == E_OUTOFMEMORY)
      return hRes;
  }
  else
  {
    hRes = Internals::OpenSSL::GetLastErrorCode(S_OK);
    if (hRes == E_OUTOFMEMORY)
      return hRes;
  }

  //if we reach here, try a normal certificate

  //create object
  cNewCert.Attach(MX_DEBUG_NEW CSslCertificate());
  if (!cNewCert)
    return E_OUTOFMEMORY;
  hRes = cNewCert->InitializeFromDER(lpData, nDataLen);
  if (FAILED(hRes))
    return hRes;

  //add to list
  if (cCertsList.AddElement(cNewCert.Get()) == FALSE)
    return E_OUTOFMEMORY;
  cNewCert.Detach();

  //done
  return S_OK;
}

HRESULT CSslCertificateArray::AddCertificateFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA,
                                                    _In_opt_ SIZE_T nPemLen)
{
  TAutoRefCounted<CSslCertificate> cNewCert;
  HRESULT hRes;

  //create object
  cNewCert.Attach(MX_DEBUG_NEW CSslCertificate());
  if (!cNewCert)
    return E_OUTOFMEMORY;
  hRes = cNewCert->InitializeFromPEM(szPemA, szPasswordA, nPemLen);
  if (FAILED(hRes))
    return hRes;
  //add to list
  if (cCertsList.AddElement(cNewCert.Get()) == FALSE)
    return E_OUTOFMEMORY;
  cNewCert.Detach();
  //done
  return S_OK;
}

HRESULT CSslCertificateCrl::InitializeFromDER(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen)
{
  X509_CRL *lpNewX509Crl;
  BIO *lpBio;
  HRESULT hRes;

  if (lpData == NULL)
    return E_POINTER;
  if (nDataLen == 0 || nDataLen > 0x7FFFFFFF)
    return E_INVALIDARG;
  //----
  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;

  lpBio = BIO_new_mem_buf((void*)lpData, (int)nDataLen);
  if (lpBio == NULL)
    return E_OUTOFMEMORY;

  ERR_clear_error();
  lpNewX509Crl = d2i_X509_CRL_bio(lpBio, NULL);
  BIO_free(lpBio);
  if (lpNewX509Crl == NULL)
    return MX_E_InvalidData;
  //done
  if (lpX509Crl)
    X509_CRL_free(_x509Crl);
  lpX509Crl = lpNewX509Crl;
  return S_OK;
}

HRESULT CSslCertificateCrl::InitializeFromPEM(_In_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA,
                                              _In_opt_ SIZE_T nPemLen)
{
  X509_CRL *lpNewX509Crl;
  BIO *lpBio;
  HRESULT hRes;

  if (szPemA == NULL)
    return E_POINTER;
  if (nPemLen == (SIZE_T)-1)
    nPemLen = StrLenA(szPemA);
  if (nPemLen == 0 || nPemLen > 0x7FFFFFFF)
    return E_INVALIDARG;
  //----
  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;
  ERR_clear_error();
  lpBio = BIO_new_mem_buf((void*)szPemA, (int)nPemLen);
  if (lpBio == NULL)
    return E_OUTOFMEMORY;
  lpNewX509Crl = PEM_read_bio_X509_CRL(lpBio, NULL, &InitializeFromPEM_PasswordCallback, (LPSTR)szPasswordA);
  BIO_free(lpBio);
  if (lpNewX509Crl == NULL)
    return MX_E_InvalidData;
  //done
  if (lpX509Crl)
    X509_CRL_free(_x509Crl);
  lpX509Crl = lpNewX509Crl;
  return S_OK;
}

HRESULT CSslCertificateArray::AddCrlFromDER(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen)
{
  TAutoRefCounted<CSslCertificateCrl> cNewCert;
  HRESULT hRes;

  cNewCert.Attach(MX_DEBUG_NEW CSslCertificateCrl());
  if (!cNewCert)
    return E_OUTOFMEMORY;
  hRes = cNewCert->InitializeFromDER(lpData, nDataLen);
  if (FAILED(hRes))
    return hRes;
  //add to list
  if (cCertCrlsList.AddElement(cNewCert.Get()) == FALSE)
    return E_OUTOFMEMORY;
  cNewCert.Detach();
  //done
  return S_OK;
}

HRESULT CSslCertificateArray::AddCrlFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA,
                                            _In_opt_ SIZE_T nPemLen)
{
  TAutoRefCounted<CSslCertificateCrl> cNewCert;
  HRESULT hRes;

  cNewCert.Attach(MX_DEBUG_NEW CSslCertificateCrl());
  if (!cNewCert)
    return E_OUTOFMEMORY;
  hRes = cNewCert->InitializeFromPEM(szPemA, szPasswordA, nPemLen);
  if (FAILED(hRes))
    return hRes;
  //add to list
  if (cCertCrlsList.AddElement(cNewCert.Get()) == FALSE)
    return E_OUTOFMEMORY;
  cNewCert.Detach();
  //done
  return S_OK;
}

HRESULT CSslCertificateArray::ImportFromWindowsStore()
{
  typedef HCERTSTORE (WINAPI *lpfnCertOpenSystemStoreW)(_In_opt_ HCRYPTPROV_LEGACY hProv,
                                                        _In_ LPCWSTR szSubsystemProtocol);
  typedef PCCERT_CONTEXT (WINAPI *lpfnCertEnumCertificatesInStore)(_In_ HCERTSTORE hCertStore,
                                                                   _In_opt_ PCCERT_CONTEXT pPrevCertContext);
  typedef PCCRL_CONTEXT (WINAPI *lpfnCertEnumCRLsInStore)(_In_ HCERTSTORE hCertStore,
                                                          _In_opt_ PCCRL_CONTEXT pPrevCrlContext);
  typedef BOOL (WINAPI *lpfnCertCloseStore)(_In_opt_ HCERTSTORE hCertStore, _In_ DWORD dwFlags);
  HCERTSTORE hStore;
  PCCERT_CONTEXT lpCertCtx;
  PCCRL_CONTEXT lpCrlCtx;
  HINSTANCE hCrypt32DLL;
  lpfnCertOpenSystemStoreW fnCertOpenSystemStoreW;
  lpfnCertEnumCertificatesInStore fnCertEnumCertificatesInStore;
  lpfnCertEnumCRLsInStore fnCertEnumCRLsInStore;
  lpfnCertCloseStore fnCertCloseStore;
  HRESULT hRes;

  hRes = Internals::LoadSystemDll(L"crypt32.dll", &hCrypt32DLL);
  if (FAILED(hRes))
    return hRes;
  fnCertOpenSystemStoreW = (lpfnCertOpenSystemStoreW)::GetProcAddress(hCrypt32DLL, "CertOpenSystemStoreW");
  fnCertEnumCertificatesInStore = (lpfnCertEnumCertificatesInStore)::GetProcAddress(hCrypt32DLL,
                                                                                    "CertEnumCertificatesInStore");
  fnCertEnumCRLsInStore = (lpfnCertEnumCRLsInStore)::GetProcAddress(hCrypt32DLL, "CertEnumCRLsInStore");
  fnCertCloseStore = (lpfnCertCloseStore)::GetProcAddress(hCrypt32DLL, "CertCloseStore");
  if (fnCertOpenSystemStoreW == NULL || fnCertEnumCertificatesInStore == NULL ||
      fnCertEnumCRLsInStore == NULL || fnCertCloseStore == NULL)
  {
    ::FreeLibrary(hCrypt32DLL);
    return MX_E_ProcNotFound;
  }
  hStore = fnCertOpenSystemStoreW(NULL, L"ROOT");
  if (hStore == NULL)
  {
    hRes = MX_HRESULT_FROM_LASTERROR();
    ::FreeLibrary(hCrypt32DLL);
    return hRes;
  }
  //process certificates
  lpCertCtx = fnCertEnumCertificatesInStore(hStore, NULL);
  while (lpCertCtx != NULL)
  {
    hRes = AddCertificateFromDER(lpCertCtx->pbCertEncoded, (SIZE_T)(lpCertCtx->cbCertEncoded));
    if (hRes == E_OUTOFMEMORY)
    {
      //only cancel on fatal errors
      fnCertCloseStore(hStore, 0);
      ::FreeLibrary(hCrypt32DLL);
      return hRes;
    }
    lpCertCtx = fnCertEnumCertificatesInStore(hStore, lpCertCtx);
  }
  //process CRL certificates
  lpCrlCtx = fnCertEnumCRLsInStore(hStore, NULL);
  while (lpCrlCtx != NULL)
  {
    hRes = AddCrlFromDER(lpCrlCtx->pbCrlEncoded, (SIZE_T)(lpCrlCtx->cbCrlEncoded));
    if (hRes == E_OUTOFMEMORY)
    {
      //only cancel on fatal errors
      fnCertCloseStore(hStore, 0);
      ::FreeLibrary(hCrypt32DLL);
      return hRes;
    }
    lpCrlCtx = fnCertEnumCRLsInStore(hStore, lpCrlCtx);
  }
  fnCertCloseStore(hStore, 0);
  ::FreeLibrary(hCrypt32DLL);
  return S_OK;
}

} //namespace MX

//-----------------------------------------------------------

static int InitializeFromPEM_PasswordCallback(char *buf, int size, int rwflag, void *userdata)
{
  SIZE_T nPassLen;

  nPassLen = MX::StrLenA((LPCSTR)userdata);
  if (size < 0)
    size = 0;
  if (nPassLen > (SIZE_T)size)
    nPassLen = (SIZE_T)size;
  ::MxMemCopy(buf, userdata, nPassLen);
  return (int)nPassLen;
}

static BOOL Asn1_ValidateLength(_Inout_ LPBYTE lpCurr, _In_ LPBYTE lpEnd)
{
  SIZE_T nLen;

  if ((SIZE_T)(lpEnd - lpCurr) < 1)
    return FALSE;
  if (((*lpCurr) & 0x80) == 0)
  {
    nLen = (SIZE_T)(lpCurr[0]);
    lpCurr++;
  }
  else
  {
    switch ((*lpCurr) & 0x7F)
    {
      case 1:
        if ((SIZE_T)(lpEnd - lpCurr) < 2)
          return FALSE;
        nLen = (SIZE_T)(lpCurr[1]);
        lpCurr += 2;
        break;
      case 2:
        if ((SIZE_T)(lpEnd - lpCurr) < 3)
          return FALSE;
        nLen = ((SIZE_T)(lpCurr[1]) << 8) | (SIZE_T)(lpCurr[2]);
        lpCurr += 3;
        break;
      default:
        return FALSE;
    }
  }
  return (nLen <= (SIZE_T)(lpEnd - lpCurr)) ? TRUE : FALSE;
}
