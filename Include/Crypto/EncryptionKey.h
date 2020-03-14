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
#ifndef _MX_ENCRYPTION_KEY_H
#define _MX_ENCRYPTION_KEY_H

#include "..\Defines.h"
#include "SecureBuffer.h"
typedef struct evp_pkey_st EVP_PKEY;

//-----------------------------------------------------------

namespace MX {

class CEncryptionKey : public virtual CBaseMemObj
{
public:
  typedef enum {
    AlgorithmRSA = 0, AlgorithmED25519, AlgorithmED448
  } eAlgorithm;

public:
  CEncryptionKey();
  CEncryptionKey(_In_ const CEncryptionKey &cSrc) throw(...);
  ~CEncryptionKey();

  CEncryptionKey &operator=(CEncryptionKey const &cSrc) throw(...);

  HRESULT Generate(_In_ MX::CEncryptionKey::eAlgorithm nAlgorithm, _In_opt_ SIZE_T nBitsCount = 0);
  SIZE_T GetBitsCount() const;

  BOOL HasPrivateKey() const;

  HRESULT SetPublicKeyFromDER(_In_ LPCVOID lpKey, _In_ SIZE_T nKeySize);
  HRESULT SetPublicKeyFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA = NULL,
                              _In_opt_ SIZE_T nPemLen = (SIZE_T)-1);
  HRESULT GetPublicKey(_Out_ CSecureBuffer **lplpBuffer);

  HRESULT SetPrivateKeyFromDER(_In_ LPCVOID lpKey, _In_ SIZE_T nKeySize);
  HRESULT SetPrivateKeyFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA = NULL,
                               _In_opt_ SIZE_T nPemLen = (SIZE_T)-1);
  HRESULT GetPrivateKey(_Out_ CSecureBuffer **lplpBuffer);

  EVP_PKEY* GetPKey() const
    {
    return lpKey;
    };

private:
  EVP_PKEY *lpKey;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_ENCRYPTION_KEY_H
