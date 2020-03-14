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
  typedef enum {
    AlgorithmDES_ECB = 0, AlgorithmDES_CBC, AlgorithmDES_OFB,
    AlgorithmDES_CFB1, AlgorithmDES_CFB8, AlgorithmDES_CFB64,
    Algorithm3DES_ECB, Algorithm3DES_CBC, Algorithm3DES_OFB,
    Algorithm3DES_CFB1, Algorithm3DES_CFB8, Algorithm3DES_CFB64,
    AlgorithmAES_128_ECB, AlgorithmAES_128_CBC, AlgorithmAES_128_CTR, AlgorithmAES_128_GCM,
    AlgorithmAES_128_OFB, AlgorithmAES_128_CFB1, AlgorithmAES_128_CFB8, AlgorithmAES_128_CFB128,
    AlgorithmAES_192_ECB, AlgorithmAES_192_CBC, AlgorithmAES_192_CTR, AlgorithmAES_192_GCM,
    AlgorithmAES_192_OFB, AlgorithmAES_192_CFB1, AlgorithmAES_192_CFB8, AlgorithmAES_192_CFB128,
    AlgorithmAES_256_ECB, AlgorithmAES_256_CBC, AlgorithmAES_256_CTR, AlgorithmAES_256_GCM,
    AlgorithmAES_256_OFB, AlgorithmAES_256_CFB1, AlgorithmAES_256_CFB8, AlgorithmAES_256_CFB128
  } eAlgorithm;

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
