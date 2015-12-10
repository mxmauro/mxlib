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
#ifndef _MX_CRYPTO_RSA_H
#define _MX_CRYPTO_RSA_H

#include "BaseCrypto.h"

//-----------------------------------------------------------

namespace MX {

class CCryptoRSA : public CBaseCrypto
{
  MX_DISABLE_COPY_CONSTRUCTOR(CCryptoRSA);
public:
  typedef enum {
    PaddingNone=0, PaddingPKCS1, PaddingOAEP, PaddingSSLV23
  } ePadding;

  typedef enum {
    HashAlgorithmSHA1=0, HashAlgorithmSHA224, HashAlgorithmSHA256, HashAlgorithmSHA384, HashAlgorithmSHA512,
    HashAlgorithmMD2, HashAlgorithmMD4, HashAlgorithmMD5
  } eHashAlgorithm;

  CCryptoRSA();
  ~CCryptoRSA();

  HRESULT GenerateKeys(__in SIZE_T nBitsCount);
  SIZE_T GetBitsCount() const;

  BOOL HasPrivateKey() const;

  HRESULT SetPublicKeyFromDER(__in LPCVOID lpKey, __in SIZE_T nKeySize);
  HRESULT SetPublicKeyFromPEM(__in_z LPCSTR szPemA, __in_z_opt LPCSTR szPasswordA=NULL,
                              __in_opt SIZE_T nPemLen=(SIZE_T)-1);
  SIZE_T GetPublicKey(__out_opt LPVOID lpDest=NULL);

  HRESULT SetPrivateKeyFromDER(__in LPCVOID lpKey, __in SIZE_T nKeySize);
  HRESULT SetPrivateKeyFromPEM(__in_z LPCSTR szPemA, __in_z_opt LPCSTR szPasswordA=NULL,
                              __in_opt SIZE_T nPemLen=(SIZE_T)-1);
  SIZE_T GetPrivateKey(__out_opt LPVOID lpDest=NULL);

  HRESULT BeginEncrypt();
  HRESULT BeginEncrypt(__in ePadding nPadding, __in_opt BOOL bUsePublicKey=TRUE);
  HRESULT EncryptStream(__in LPCVOID lpData, __in SIZE_T nDataLength);
  HRESULT EndEncrypt();

  HRESULT BeginDecrypt();
  HRESULT BeginDecrypt(__in ePadding nPadding, __in_opt BOOL bUsePrivateKey=TRUE);
  HRESULT DecryptStream(__in LPCVOID lpData, __in SIZE_T nDataLength);
  HRESULT EndDecrypt();

  HRESULT BeginSign();
  HRESULT BeginSign(__in eHashAlgorithm nAlgorithm);
  HRESULT SignStream(__in LPCVOID lpData, __in SIZE_T nDataLength);
  HRESULT EndSign();
  LPBYTE GetSignature() const;
  SIZE_T GetSignatureSize() const;

  HRESULT BeginVerify();
  HRESULT BeginVerify(__in eHashAlgorithm nAlgorithm);
  HRESULT VerifyStream(__in LPCVOID lpData, __in SIZE_T nDataLength);
  HRESULT EndVerify(__in LPCVOID lpSignature, __in SIZE_T nSignatureLen);

private:
  VOID CleanUp(__in int nWhat, __in BOOL bZeroData);

private:
  LPVOID lpInternalData;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_CRYPTO_RSA_H
