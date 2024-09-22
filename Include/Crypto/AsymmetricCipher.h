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
#ifndef _MX_ASYMMETRIC_CIPHER_H
#define _MX_ASYMMETRIC_CIPHER_H

#include "..\Defines.h"
#include "EncryptionKey.h"
#include "MessageDigest.h"

//-----------------------------------------------------------

namespace MX {

class CAsymmetricCipher : public virtual CBaseMemObj, public CNonCopyableObj
{
public:
  typedef enum {
    None=0, PKCS1, OAEP
  } ePadding;

public:
  CAsymmetricCipher();
  ~CAsymmetricCipher();

  HRESULT SetKey(_In_ CEncryptionKey *lpKey);

  HRESULT BeginEncrypt(_In_opt_ MX::CAsymmetricCipher::ePadding nPadding = MX::CAsymmetricCipher::PKCS1);
  HRESULT EncryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength);
  HRESULT EndEncrypt();

  LPBYTE GetEncryptedData() const;
  SIZE_T GetEncryptedDataSize() const;

  HRESULT BeginDecrypt(_In_opt_ MX::CAsymmetricCipher::ePadding nPadding = MX::CAsymmetricCipher::PKCS1);
  HRESULT DecryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength);
  HRESULT EndDecrypt();

  LPBYTE GetDecryptedData() const;
  SIZE_T GetDecryptedDataSize() const;

  HRESULT BeginSign(_In_ MX::CMessageDigest::eAlgorithm nAlgorithm);
  HRESULT SignStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength);
  HRESULT EndSign();

  LPBYTE GetSignature() const;
  SIZE_T GetSignatureSize() const;

  HRESULT BeginVerify(_In_ MX::CMessageDigest::eAlgorithm nAlgorithm);
  HRESULT VerifyStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength);
  HRESULT EndVerify(_In_ LPCVOID lpSignature, _In_ SIZE_T nSignatureLen);

private:
  HRESULT InternalInitialize(_In_ int nZone);

private:
  LPVOID lpInternalData;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_ASYMMETRIC_CIPHER_H

