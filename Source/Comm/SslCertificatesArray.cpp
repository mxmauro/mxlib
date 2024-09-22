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
#include <OpenSSL\pkcs12.h>

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

HRESULT CSslCertificateArray::Add(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen, _In_opt_z_ LPCSTR szPasswordA)
{
  typedef struct {
    SIZE_T nOffset, nLength;
  } BLOCK;
  MX::TArrayList4Structs<BLOCK> aBlocksList;
  BOOL bIsPEM;
  SIZE_T nCount;
  HRESULT hRes;

  if (lpData == NULL && nDataLen > 0)
    return E_POINTER;
  if (nDataLen == 0)
    return S_OK;

  hRes = Internals::OpenSSL::Init();
  if (FAILED(hRes))
    return hRes;

  bIsPEM = MX::Internals::OpenSSL::IsPEM(lpData, nDataLen);
  if (bIsPEM != FALSE)
  {
    LPCSTR sA = (LPCSTR)lpData;
    while (1)
    {
      LPCSTR szBeginA, szEndA;
      SIZE_T nRemaining;
      BLOCK sBlock;

      szBeginA = MX::StrNFindA(sA, "-----BEGIN ", nDataLen - (SIZE_T)(sA - (LPCSTR)lpData));
      if (szBeginA == NULL)
        break;

      sA = szBeginA + 11;

      szEndA = MX::StrNFindA(sA, "-----END ", nDataLen - (SIZE_T)(sA - (LPCSTR)lpData));
      if (szEndA == NULL)
        return MX_E_InvalidData;

      szEndA += 9;
      nRemaining = nDataLen - (SIZE_T)(szEndA - (LPCSTR)lpData);
      while (nRemaining >= 5)
      {
        if (MX::StrCompareA(szEndA, "-----", 5) == 0)
          break;
        szEndA += 1;
        nRemaining -= 1;
      }
      if (nRemaining < 5)
        return MX_E_InvalidData;

      szEndA += 5;

      //add block
      sBlock.nOffset = (SIZE_T)(szBeginA - (LPCSTR)lpData);
      sBlock.nLength = (SIZE_T)(szEndA - szBeginA);
      if (aBlocksList.AddElement(&sBlock) == FALSE)
        return E_OUTOFMEMORY;

      //advance to next
      sA = szEndA + 5;
    }
  }
  else
  {
    BLOCK sBlock;

    sBlock.nOffset = 0;
    sBlock.nLength = nDataLen;
    if (aBlocksList.AddElement(&sBlock) == FALSE)
      return E_OUTOFMEMORY;
  }

  //process each block
  nCount = aBlocksList.GetCount();
  for (SIZE_T i = 0; i < nCount; i++)
  {
    BLOCK &sBlock = aBlocksList.GetElementAt(i);

    if (bIsPEM != FALSE)
    {
      LPCSTR sA = (LPCSTR)lpData + sBlock.nOffset;

      if ((sBlock.nLength >= 11 + 16 && StrNCompareA(sA + 11, "CERTIFICATE-----", 16) == 0) ||
          (sBlock.nLength >= 11 + 21 && StrNCompareA(sA + 11, "X509 CERTIFICATE-----", 21) == 0))
      {
        TAutoRefCounted<CSslCertificate> cNewCert;

        //create object
        cNewCert.Attach(MX_DEBUG_NEW CSslCertificate());
        if (!cNewCert)
          return E_OUTOFMEMORY;
        hRes = cNewCert->Set(sA, sBlock.nLength, szPasswordA);
        if (FAILED(hRes))
          return hRes;
        //add to list
        if (cCertsList.AddElement(cNewCert.Get()) == FALSE)
          return E_OUTOFMEMORY;
        cNewCert.Detach();
      }
      else if (nDataLen >= 11 + 13 && StrNCompareA(sA + 11, "X509 CRL-----", 13) == 0)
      {
        TAutoRefCounted<CSslCertificateCrl> cNewCrlCert;

        cNewCrlCert.Attach(MX_DEBUG_NEW CSslCertificateCrl());
        if (!cNewCrlCert)
          return E_OUTOFMEMORY;
        hRes = cNewCrlCert->Set(sA, sBlock.nLength, szPasswordA);
        if (FAILED(hRes))
          return hRes;
        //add to list
        if (cCertCrlsList.AddElement(cNewCrlCert.Get()) == FALSE)
          return E_OUTOFMEMORY;
        cNewCrlCert.Detach();
      }
      else
      {
        TAutoRefCounted<CEncryptionKey> cNewKey;

        cNewKey.Attach(MX_DEBUG_NEW CEncryptionKey());
        if (!cNewKey)
          return E_OUTOFMEMORY;
        hRes = cNewKey->Set(sA, sBlock.nLength, szPasswordA);
        if (FAILED(hRes))
          return hRes;
        //add to list
        if (cKeysList.AddElement(cNewKey.Get()) == FALSE)
          return E_OUTOFMEMORY;
        cNewKey.Detach();
      }
    }
    else
    {
      const unsigned char *lpBuf;
      PKCS12 *lpPKCS2;

      //first check if we are dealing with a PKCS12 certificate
      ERR_clear_error();
      lpBuf = (const unsigned char *)lpData + sBlock.nOffset;
      lpPKCS2 = d2i_PKCS12(NULL, &lpBuf, (long)(sBlock.nLength));
      if (lpPKCS2 != NULL)
      {
        EVP_PKEY *lpEvpKey = NULL;
        X509 *lpX509 = NULL;

        //a pkcs12 can have a certificate and a private key so parse both
        ERR_clear_error();
        if (PKCS12_parse(lpPKCS2, szPasswordA, &lpEvpKey, &lpX509, NULL) <= 0)
          return Internals::OpenSSL::GetLastErrorCode(E_OUTOFMEMORY);

        if (lpX509 != NULL)
        {
          TAutoRefCounted<CSslCertificate> cNewCert;

          hRes = S_OK;
          cNewCert.Attach(MX_DEBUG_NEW CSslCertificate());
          if (cNewCert)
          {
            cNewCert->Set(lpX509);
            if (cCertsList.AddElement(cNewCert.Get()) != FALSE)
              cNewCert.Detach();
            else
              hRes = E_OUTOFMEMORY;
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
            PKCS12_free(lpPKCS2);
            return hRes;
          }
        }

        if (lpEvpKey != NULL)
        {
          TAutoRefCounted<CEncryptionKey> cNewKey;

          hRes = S_OK;
          cNewKey.Attach(MX_DEBUG_NEW CEncryptionKey());
          if (cNewKey)
          {
            cNewKey->Set(lpEvpKey);
            if (cKeysList.AddElement(cNewKey.Get()) != FALSE)
              cNewKey.Detach();
            else
              hRes = E_OUTOFMEMORY;
          }
          else
          {
            hRes = E_OUTOFMEMORY;
          }

          EVP_PKEY_free(lpEvpKey);

          if (FAILED(hRes))
          {
            PKCS12_free(lpPKCS2);
            return hRes;
          }
        }

        PKCS12_free(lpPKCS2);
      }
      else
      {
        //trial an error
        TAutoRefCounted<CSslCertificate> cNewCert;
        TAutoRefCounted<CSslCertificateCrl> cNewCrlCert;
        TAutoRefCounted<CEncryptionKey> cNewKey;

        //not optimal but...
        cNewCert.Attach(MX_DEBUG_NEW CSslCertificate());
        cNewCrlCert.Attach(MX_DEBUG_NEW CSslCertificateCrl());
        cNewKey.Attach(MX_DEBUG_NEW CEncryptionKey());

        if (!(cNewCert && cNewCrlCert && cNewKey))
          return E_OUTOFMEMORY;

        hRes = cNewKey->Set((LPBYTE)lpData + sBlock.nOffset, sBlock.nLength, szPasswordA);
        if (SUCCEEDED(hRes))
        {
          //add to list
          if (cKeysList.AddElement(cNewKey.Get()) == FALSE)
            return E_OUTOFMEMORY;
          cNewKey.Detach();
        }
        else
        {
          hRes = cNewCert->Set((LPBYTE)lpData + sBlock.nOffset, sBlock.nLength, szPasswordA);
          if (SUCCEEDED(hRes))
          {
            //add to list
            if (cCertsList.AddElement(cNewCert.Get()) == FALSE)
              return E_OUTOFMEMORY;
            cNewCert.Detach();
          }
          else
          {
            hRes = cNewCrlCert->Set((LPBYTE)lpData + sBlock.nOffset, sBlock.nLength, szPasswordA);
            if (SUCCEEDED(hRes))
            {
              //add to list
              if (cCertCrlsList.AddElement(cNewCrlCert.Get()) == FALSE)
                return E_OUTOFMEMORY;
              cNewCrlCert.Detach();
            }
            else
            {
              return hRes;
            }
          }
        }
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
  LPBYTE lpBuf;

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
  if (dwSizeHi != 0 || dwSizeLo == 0 || dwSizeLo > 0x7FFFFFFFUL)
    return MX_E_InvalidData;

  //allocate temporary storage and read file
  aTempBuf.Attach((LPBYTE)MX_MALLOC((SIZE_T)dwSizeLo));
  if (!aTempBuf)
    return E_OUTOFMEMORY;
  if (::ReadFile(cFileH, aTempBuf.Get(), dwSizeLo, &dwReaded, NULL) == FALSE)
    return MX_HRESULT_FROM_LASTERROR();
  if (dwReaded != dwSizeLo)
    return MX_E_ReadFault;

  //is Unicode?
  lpBuf = aTempBuf.Get();
  if (dwSizeLo >= 2 && lpBuf[0] == 0xFF && lpBuf[1] == 0xFE)
  {
    //convert to ansi
    MX::CStringA cStrTempA;

    if (cStrTempA.CopyN((LPCWSTR)(lpBuf + 2), (SIZE_T)dwSizeLo - 2) == FALSE)
      return E_OUTOFMEMORY;

    //process certificates
    return Add((LPCSTR)cStrTempA, cStrTempA.GetLength(), szPasswordA);
  }
  //is utf8?
  if (dwSizeLo >= 3 && lpBuf[0] == 0xEF && lpBuf[1] == 0xBB && lpBuf[1] == 0xBF)
  {
    lpBuf += 3;
    dwSizeLo -= 3;
  }

  //process certificates
  return Add(lpBuf, (SIZE_T)dwSizeLo, szPasswordA);
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
    hRes = Add(lpCertCtx->pbCertEncoded, (SIZE_T)(lpCertCtx->cbCertEncoded));
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
    hRes = Add(lpCrlCtx->pbCrlEncoded, (SIZE_T)(lpCrlCtx->cbCrlEncoded));
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
