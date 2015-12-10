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
#include "..\..\Include\Comm\SslCertificates.h"
#include "..\..\Include\AutoHandle.h"
#include "..\..\Include\Crypto\Base64.h"
#include "..\Crypto\InitOpenSSL.h"
#include "..\..\Include\Strings\Utf8.h"
#include "..\..\Include\AutoPtr.h"
#include <wincrypt.h>
#include <OpenSSL\des.h>
#include <OpenSSL\aes.h>
#include <OpenSSL\err.h>
#include <OpenSSL\evp.h>

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

//-----------------------------------------------------------

namespace MX {

CSslCertificateArray::CSslCertificateArray() : CBaseMemObj()
{
  return;
}

CSslCertificateArray::~CSslCertificateArray()
{
  return;
}

HRESULT CSslCertificateArray::AddFromString(__in_z LPCSTR szStringA, __in_z_opt LPCSTR szPasswordA,
                                            __in_opt SIZE_T nLen)
{
  static const BYTE aDashes[5] = {'-','-','-','-','-'};
  TArrayList4Structs<PEM_BLOCK> aBlocksList;
  PEM_BLOCK sBlock;
  LPCSTR sA, sA_2, szDataEndA;
  SIZE_T i, k, nTag, nCount, nPassLen;
  HRESULT hRes;

  if (nLen == (SIZE_T)-1)
    nLen = StrLenA(szStringA);
  if (szStringA == NULL && nLen > 0)
    return E_POINTER;
  nPassLen = StrLenA(szPasswordA);
  if (nPassLen > INT_MAX)
    return E_INVALIDARG;
  //initialize OpenSSL
  if (Internals::OpenSSL::Init() == FALSE)
    return E_OUTOFMEMORY;
  //locate blocks
  szDataEndA = (sA = szStringA) + nLen;
  while (sA < szDataEndA)
  {
    while (sA < szDataEndA && *sA != '-')
      sA++;
    if (sA >= szDataEndA || (sA+5) > szDataEndA)
      break;
    if (MemCompare(sA, aDashes, 5) != 0)
    {
      sA++;
      continue;
    }
    if (sA+11 <= szDataEndA && MemCompare(sA+5, "BEGIN ", 6) == 0)
    {
      nTag = 1;
      sA_2 = sA + 11;
    }
    else if (sA+9 <= szDataEndA && MemCompare(sA+5, "END ", 4) == 0)
    {
      nTag = 2;
      sA_2 = sA + 9;
    }
    else
    {
      sA++;
      continue;
    }
    for (i=0; i<MX_ARRAYLEN(aPemTypes); i++)
    {
      k = StrLenA(aPemTypes[i].szNameA);
      if (sA_2+k <= szDataEndA && MemCompare(sA_2, aPemTypes[i].szNameA, k) == 0)
        break;
    }
    if (i >= MX_ARRAYLEN(aPemTypes))
    {
      sA++;
      continue;
    }
    sBlock.nType = aPemTypes[i].nBlockType;
    sA_2 += k;
    //check "-----"
    if (sA_2+5 > szDataEndA)
      break;
    if (MemCompare(sA_2, aDashes, 5) != 0)
    {
      sA++;
      continue;
    }
    sA_2 += 5;
    if (nTag == 1)
    {
      while (sA_2 < szDataEndA && (*sA_2=='\r' || *sA_2=='\n' || *sA_2=='\t' || *sA_2==' '))
        sA_2++;
    }
    //here we are at the end of a -----BEGIN/END nnnnn-----
    nCount = aBlocksList.GetCount();
    if (nTag == 1)
    {
      //begin block
      if (nCount>0 && aBlocksList[nCount-1].nLen == (SIZE_T)-1)
        return MX_E_InvalidData; //previous block not finished
      sBlock.nStart = (SIZE_T)(sA_2 - szStringA);
      sBlock.nLen = (SIZE_T)-1;
      if (aBlocksList.AddElement(&sBlock) == FALSE)
        return E_OUTOFMEMORY;
    }
    else
    {
      //end block
      if (nCount == 0 || aBlocksList[nCount-1].nType != sBlock.nType)
        return MX_E_InvalidData; //no-previous block or begin/end mismatch
      aBlocksList[nCount - 1].nLen = (SIZE_T)(sA-szStringA) - sBlock.nStart;
    }
    //go after tag
    sA = sA_2;
  }
  nCount = aBlocksList.GetCount();
  if (nCount == 0)
    return S_OK;
  if (aBlocksList[nCount-1].nLen == (SIZE_T)-1)
    return MX_E_InvalidData; //previous block not finished
  //process blocks
  for (k=0; k<nCount; k++)
  {
    sA = szStringA + aBlocksList[k].nStart;
    switch (aBlocksList[k].nType)
    {
      case BLOCK_TYPE_PublicKey:
      case BLOCK_TYPE_RsaPublicKey:
        hRes = AddPublicKeyFromPEM(sA, szPasswordA, aBlocksList[k].nLen);
        break;
      case BLOCK_TYPE_PrivateKey:
      case BLOCK_TYPE_RsaPrivateKey:
        hRes = AddPrivateKeyFromPEM(sA, szPasswordA, aBlocksList[k].nLen);
        break;
      case BLOCK_TYPE_Certificate:
      case BLOCK_TYPE_OldCertificate:
        hRes = AddCertificateFromPEM(sA, szPasswordA, aBlocksList[k].nLen);
        break;
      case BLOCK_TYPE_Crl:
        hRes = AddCrlFromPEM(sA, szPasswordA, aBlocksList[k].nLen);
        break;
      default:
        hRes = MX_E_Unsupported;
        break;
    }
    if (FAILED(hRes))
      return hRes;
  }
  return S_OK;
}

HRESULT CSslCertificateArray::AddFromString(__in_z LPCWSTR szStringW, __in_z_opt LPCSTR szPasswordA,
                                            __in_opt SIZE_T nLen)
{
  CStringA cStrTempA;

  if (nLen == (SIZE_T)-1)
    nLen = StrLenW(szStringW);
  if (szStringW == NULL && nLen > 0)
    return E_POINTER;
  if (cStrTempA.CopyN(szStringW, nLen) == FALSE)
    return E_OUTOFMEMORY;
  return AddFromString((LPSTR)cStrTempA, szPasswordA, -1);
}

HRESULT CSslCertificateArray::AddFromFile(__in_z LPCWSTR szFileNameW, __in_z_opt LPCSTR szPasswordA)
{
  CWindowsHandle cFileH;
  DWORD dwSizeLo, dwSizeHi, dwReaded;
  TAutoFreePtr<BYTE> aTempBuf;

  if (szFileNameW == NULL)
    return E_POINTER;
  if (*szFileNameW == 0)
    return E_INVALIDARG;
  //open file
  cFileH.Attach(::CreateFileW(szFileNameW, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
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
  //check if unicode
  if (dwSizeLo >= 2 && aTempBuf.Get()[0] == 0xFF && aTempBuf.Get()[1] == 0xFE)
    return AddFromString((LPWSTR)(aTempBuf.Get()+2), szPasswordA, (SIZE_T)(dwSizeLo-2) / sizeof(WCHAR));
  //else treat as ansi
  return AddFromString((LPSTR)(aTempBuf.Get()), szPasswordA, (SIZE_T)dwSizeLo);
}

HRESULT CSslCertificateArray::AddPublicKeyFromDER(__in LPCVOID lpData, __in SIZE_T nDataLen)
{
  TAutoDeletePtr<CCryptoRSA> cRsa;
  HRESULT hRes;

  //create object
  cRsa.Attach(MX_DEBUG_NEW CCryptoRSA());
  if (!cRsa)
    return E_OUTOFMEMORY;
  //set public key
  hRes = cRsa->SetPublicKeyFromDER(lpData, nDataLen);
  if (SUCCEEDED(hRes))
  {
    //add to list
    if (cRsaKeysList.AddElement(cRsa.Get()) != FALSE)
      cRsa.Detach();
    else
      hRes = E_OUTOFMEMORY;
  }
  //done
  return hRes;
}

HRESULT CSslCertificateArray::AddPublicKeyFromPEM(__in_z LPCSTR szPemA, __in_z_opt LPCSTR szPasswordA,
                                                  __in_opt SIZE_T nPemLen)
{
  TAutoDeletePtr<CCryptoRSA> cRsa;
  HRESULT hRes;

  //create object
  cRsa.Attach(MX_DEBUG_NEW CCryptoRSA());
  if (!cRsa)
    return E_OUTOFMEMORY;
  //set public key
  hRes = cRsa->SetPublicKeyFromPEM(szPemA, szPasswordA, nPemLen);
  if (SUCCEEDED(hRes))
  {
    //add to list
    if (cRsaKeysList.AddElement(cRsa.Get()) != FALSE)
      cRsa.Detach();
    else
      hRes = E_OUTOFMEMORY;
  }
  //done
  return hRes;
}

HRESULT CSslCertificateArray::AddPrivateKeyFromDER(__in LPCVOID lpData, __in SIZE_T nDataLen)
{
  TAutoDeletePtr<CCryptoRSA> cRsa;
  HRESULT hRes;

  //create object
  cRsa.Attach(MX_DEBUG_NEW CCryptoRSA());
  if (!cRsa)
    return E_OUTOFMEMORY;
  //set private key
  hRes = cRsa->SetPrivateKeyFromDER(lpData, nDataLen);
  if (SUCCEEDED(hRes))
  {
    //add to list
    if (cRsaKeysList.AddElement(cRsa.Get()) != FALSE)
      cRsa.Detach();
    else
      hRes = E_OUTOFMEMORY;
  }
  //done
  return hRes;
}

HRESULT CSslCertificateArray::AddPrivateKeyFromPEM(__in_z LPCSTR szPemA, __in_z_opt LPCSTR szPasswordA,
                                                   __in_opt SIZE_T nPemLen)
{
  TAutoDeletePtr<CCryptoRSA> cRsa;
  HRESULT hRes;

  //create object
  cRsa.Attach(MX_DEBUG_NEW CCryptoRSA());
  if (!cRsa)
    return E_OUTOFMEMORY;
  //set public key
  hRes = cRsa->SetPrivateKeyFromPEM(szPemA, szPasswordA, nPemLen);
  if (SUCCEEDED(hRes))
  {
    //add to list
    if (cRsaKeysList.AddElement(cRsa.Get()) != FALSE)
      cRsa.Detach();
    else
      hRes = E_OUTOFMEMORY;
  }
  //done
  return hRes;
}

HRESULT CSslCertificate::InitializeFromDER(__in LPCVOID lpData, __in SIZE_T nDataLen)
{
  X509 *lpNewX509;
  BIO *lpBio;

  if (lpData == NULL)
    return E_POINTER;
  if (nDataLen == 0 || nDataLen > 0x7FFFFFFF)
    return E_INVALIDARG;
  //----
  ERR_clear_error();
  lpBio = BIO_new_mem_buf((void*)lpData, (int)nDataLen);
  if (lpBio == NULL)
    return E_OUTOFMEMORY;
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

HRESULT CSslCertificate::InitializeFromPEM(__in LPCSTR szPemA, __in_z_opt LPCSTR szPasswordA, __in_opt SIZE_T nPemLen)
{
  X509 *lpNewX509;
  BIO *lpBio;

  if (szPemA == NULL)
    return E_POINTER;
  if (nPemLen == -1)
    nPemLen = StrLenA(szPemA);
  if (nPemLen == 0 || nPemLen > 0x7FFFFFFF)
    return E_INVALIDARG;
  //----
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

HRESULT CSslCertificateArray::AddCertificateFromDER(__in LPCVOID lpData, __in SIZE_T nDataLen)
{
  TAutoDeletePtr<CSslCertificate> cNewCert;
  HRESULT hRes;

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

HRESULT CSslCertificateArray::AddCertificateFromPEM(__in_z LPCSTR szPemA, __in_z_opt LPCSTR szPasswordA,
                                                    __in_opt SIZE_T nPemLen)
{
  TAutoDeletePtr<CSslCertificate> cNewCert;
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

HRESULT CSslCertificateCrl::InitializeFromDER(__in LPCVOID lpData, __in SIZE_T nDataLen)
{
  X509_CRL *lpNewX509Crl;
  BIO *lpBio;

  if (lpData == NULL)
    return E_POINTER;
  if (nDataLen == 0 || nDataLen > 0x7FFFFFFF)
    return E_INVALIDARG;
  //----
  ERR_clear_error();
  lpBio = BIO_new_mem_buf((void*)lpData, (int)nDataLen);
  if (lpBio == NULL)
    return E_OUTOFMEMORY;
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

HRESULT CSslCertificateCrl::InitializeFromPEM(__in LPCSTR szPemA, __in_z_opt LPCSTR szPasswordA,
                                              __in_opt SIZE_T nPemLen)

{
  X509_CRL *lpNewX509Crl;
  BIO *lpBio;

  if (szPemA == NULL)
    return E_POINTER;
  if (nPemLen == -1)
    nPemLen = StrLenA(szPemA);
  if (nPemLen == 0 || nPemLen > 0x7FFFFFFF)
    return E_INVALIDARG;
  //----
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

HRESULT CSslCertificateArray::AddCrlFromDER(__in LPCVOID lpData, __in SIZE_T nDataLen)
{
  TAutoDeletePtr<CSslCertificateCrl> cNewCert;
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

HRESULT CSslCertificateArray::AddCrlFromPEM(__in_z LPCSTR szPemA, __in_z_opt LPCSTR szPasswordA,
                                            __in_opt SIZE_T nPemLen)
{
  TAutoDeletePtr<CSslCertificateCrl> cNewCert;
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
  typedef HCERTSTORE (WINAPI *lpfnCertOpenSystemStoreW)(__in_opt HCRYPTPROV_LEGACY hProv,
                                                        __in LPCWSTR szSubsystemProtocol);
  typedef PCCERT_CONTEXT (WINAPI *lpfnCertEnumCertificatesInStore)(__in HCERTSTORE hCertStore,
                                                                   __in_opt PCCERT_CONTEXT pPrevCertContext);
  typedef PCCRL_CONTEXT (WINAPI *lpfnCertEnumCRLsInStore)(__in HCERTSTORE hCertStore,
                                                          __in_opt PCCRL_CONTEXT pPrevCrlContext);
  typedef BOOL (WINAPI *lpfnCertCloseStore)(__in_opt HCERTSTORE hCertStore, __in DWORD dwFlags);
  HCERTSTORE hStore;
  PCCERT_CONTEXT lpCertCtx;
  PCCRL_CONTEXT lpCrlCtx;
  HINSTANCE hCrypt32DLL;
  lpfnCertOpenSystemStoreW fnCertOpenSystemStoreW;
  lpfnCertEnumCertificatesInStore fnCertEnumCertificatesInStore;
  lpfnCertEnumCRLsInStore fnCertEnumCRLsInStore;
  lpfnCertCloseStore fnCertCloseStore;
  HRESULT hRes;

  hCrypt32DLL = ::LoadLibraryW(L"crypt32.dll");
  if (hCrypt32DLL == NULL)
    return MX_HRESULT_FROM_LASTERROR();
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

SIZE_T CSslCertificateArray::GetCertificatesCount() const
{
  return cCertsList.GetCount();
}

CSslCertificate* CSslCertificateArray::GetCertificate(__in SIZE_T nIndex)
{
  if (nIndex >= cCertsList.GetCount())
    return NULL;
  return cCertsList.GetElementAt(nIndex);
}

SIZE_T CSslCertificateArray::GetCrlCertificatesCount() const
{
  return cCertCrlsList.GetCount();
}

CSslCertificateCrl* CSslCertificateArray::GetCrlCertificate(__in SIZE_T nIndex)
{
  if (nIndex >= cCertCrlsList.GetCount())
    return NULL;
  return cCertCrlsList.GetElementAt(nIndex);
}

SIZE_T CSslCertificateArray::GetRsaKeysCount() const
{
  return cRsaKeysList.GetCount();
}

CCryptoRSA* CSslCertificateArray::GetRsaKey(__in SIZE_T nIndex)
{
  if (nIndex >= cRsaKeysList.GetCount())
    return NULL;
  return cRsaKeysList.GetElementAt(nIndex);
}

} //namespace MX

//-----------------------------------------------------------

static int InitializeFromPEM_PasswordCallback(char *buf, int size, int rwflag, void *userdata)
{
  SIZE_T nPassLen;

  nPassLen = MX::StrLenA((LPSTR)userdata);
  if (size < 0)
    size = 0;
  if (nPassLen > (SIZE_T)size)
    nPassLen = (SIZE_T)size;
  MX::MemCopy(buf, userdata, nPassLen);
  return (int)nPassLen;
}
