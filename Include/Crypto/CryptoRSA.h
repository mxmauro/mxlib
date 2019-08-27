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

  HRESULT GenerateKeys(_In_ SIZE_T nBitsCount);
  SIZE_T GetBitsCount() const;

  BOOL HasPrivateKey() const;

  HRESULT SetPublicKeyFromDER(_In_ LPCVOID lpKey, _In_ SIZE_T nKeySize, _In_opt_z_ LPCSTR szPasswordA=NULL);
  HRESULT SetPublicKeyFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA=NULL,
                              _In_opt_ SIZE_T nPemLen=(SIZE_T)-1);
  SIZE_T GetPublicKey(_Out_opt_ LPVOID lpDest=NULL);

  HRESULT SetPrivateKeyFromDER(_In_ LPCVOID lpKey, _In_ SIZE_T nKeySize, _In_opt_z_ LPCSTR szPasswordA=NULL);
  HRESULT SetPrivateKeyFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA=NULL,
                              _In_opt_ SIZE_T nPemLen=(SIZE_T)-1);
  SIZE_T GetPrivateKey(_Out_opt_ LPVOID lpDest=NULL);

  HRESULT BeginEncrypt();
  HRESULT BeginEncrypt(_In_ ePadding nPadding, _In_opt_ BOOL bUsePublicKey=TRUE);
  HRESULT EncryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength);
  HRESULT EndEncrypt();

  HRESULT BeginDecrypt();
  HRESULT BeginDecrypt(_In_ ePadding nPadding, _In_opt_ BOOL bUsePrivateKey=TRUE);
  HRESULT DecryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength);
  HRESULT EndDecrypt();

  HRESULT BeginSign();
  HRESULT BeginSign(_In_ eHashAlgorithm nAlgorithm);
  HRESULT SignStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength);
  HRESULT EndSign();
  LPBYTE GetSignature() const;
  SIZE_T GetSignatureSize() const;

  HRESULT BeginVerify();
  HRESULT BeginVerify(_In_ eHashAlgorithm nAlgorithm);
  HRESULT VerifyStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength);
  HRESULT EndVerify(_In_ LPCVOID lpSignature, _In_ SIZE_T nSignatureLen);

private:
  VOID CleanUp(_In_ int nWhat, _In_ BOOL bZeroData);

private:
  LPVOID lpInternalData;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_CRYPTO_RSA_H
