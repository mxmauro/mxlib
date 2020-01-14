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
#ifndef _MX_CRYPTO_AES_H
#define _MX_CRYPTO_AES_H

#include "CryptoBase.h"

//-----------------------------------------------------------

namespace MX {

class CCryptoAES : public CCryptoBase, public CNonCopyableObj
{
public:
  typedef enum {
    AlgorithmAes128Ecb=0, AlgorithmAes128Cbc, AlgorithmAes128Cfb,
    AlgorithmAes128Ofb, AlgorithmAes128Cfb1, AlgorithmAes128Cfb8,
    AlgorithmAes192Ecb, AlgorithmAes192Cbc, AlgorithmAes192Cfb,
    AlgorithmAes192Ofb, AlgorithmAes192Cfb1, AlgorithmAes192Cfb8,
    AlgorithmAes256Ecb, AlgorithmAes256Cbc, AlgorithmAes256Cfb,
    AlgorithmAes256Ofb, AlgorithmAes256Cfb1, AlgorithmAes256Cfb8
  } eAlgorithm;

public:
  CCryptoAES();
  ~CCryptoAES();

  HRESULT BeginEncrypt();
  HRESULT BeginEncrypt(_In_ eAlgorithm nAlgorithm, _In_opt_ LPCVOID lpKey=NULL, _In_opt_ SIZE_T nKeyLen=0,
                       _In_opt_ LPCVOID lpInitVector=NULL, _In_opt_ BOOL bUsePadding=TRUE);
  HRESULT EncryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength);
  HRESULT EndEncrypt();

  HRESULT BeginDecrypt();
  HRESULT BeginDecrypt(_In_ eAlgorithm nAlgorithm, _In_opt_ LPCVOID lpKey=NULL, _In_opt_ SIZE_T nKeyLen=0,
                       _In_opt_ LPCVOID lpInitVector=NULL, _In_opt_ BOOL bUsePadding=TRUE);
  HRESULT DecryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength);
  HRESULT EndDecrypt();

private:
  VOID CleanUp(_In_ BOOL bEncoder, _In_ BOOL bZeroData);

private:
  LPVOID lpInternalData;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_CRYPTO_AES_H
