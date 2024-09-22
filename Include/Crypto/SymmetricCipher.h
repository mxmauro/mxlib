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
#ifndef _MX_SYMMETRIC_CIPHER_H
#define _MX_SYMMETRIC_CIPHER_H

#include "..\Defines.h"

//-----------------------------------------------------------

namespace MX {

class CSymmetricCipher : public virtual CBaseMemObj, public CNonCopyableObj
{
public:
  enum class eAlgorithm
  {
    DES_ECB = 0, DES_CBC, DES_OFB,
    DES_CFB1, DES_CFB8, DES_CFB64,
    TripleDES_ECB, TripleDES_CBC, TripleDES_OFB,
    TripleDES_CFB1, TripleDES_CFB8, TripleDES_CFB64,
    AES_128_ECB, AES_128_CBC, AES_128_CTR, AES_128_GCM,
    AES_128_OFB, AES_128_CFB1, AES_128_CFB8, AES_128_CFB128,
    AES_192_ECB, AES_192_CBC, AES_192_CTR, AES_192_GCM,
    AES_192_OFB, AES_192_CFB1, AES_192_CFB8, AES_192_CFB128,
    AES_256_ECB, AES_256_CBC, AES_256_CTR, AES_256_GCM,
    AES_256_OFB, AES_256_CFB1, AES_256_CFB8, AES_256_CFB128
  };

public:
  CSymmetricCipher();
  ~CSymmetricCipher();

  HRESULT SetAlgorithm(_In_ MX::CSymmetricCipher::eAlgorithm nAlgorithm);

  HRESULT BeginEncrypt(_In_opt_ BOOL bUsePadding = TRUE, _In_opt_ LPCVOID lpKey = NULL, _In_opt_ SIZE_T nKeyLen = 0,
                       _In_opt_ LPCVOID lpInitVector = NULL);
  HRESULT EncryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength);
  HRESULT EndEncrypt();

  SIZE_T GetAvailableEncryptedData() const;
  SIZE_T GetEncryptedData(_Out_writes_(nDestSize) LPVOID lpDest, _In_ SIZE_T nDestSize);

  HRESULT BeginDecrypt(_In_opt_ BOOL bUsePadding = TRUE, _In_opt_ LPCVOID lpKey = NULL, _In_opt_ SIZE_T nKeyLen = 0,
                       _In_opt_ LPCVOID lpInitVector = NULL);
  HRESULT DecryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength);
  HRESULT EndDecrypt();

  SIZE_T GetAvailableDecryptedData() const;
  SIZE_T GetDecryptedData(_Out_writes_(nDestSize) LPVOID lpDest, _In_ SIZE_T nDestSize);

private:
  HRESULT InternalInitialize();

private:
  LPVOID lpInternalData;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_SYMMETRIC_CIPHER_H
