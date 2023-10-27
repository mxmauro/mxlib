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
#include "..\RefCounted.h"
#include "SecureBuffer.h"
typedef struct evp_pkey_st EVP_PKEY;

//-----------------------------------------------------------

namespace MX {

class CEncryptionKey : public virtual TRefCounted<CBaseMemObj>
{
public:
  typedef enum {
    Unknown = -1,
    RSA = 0, ED25519, ED448, Poly1305, DSA, DH, DHX
  } eAlgorithm;

public:
  CEncryptionKey();
  CEncryptionKey(_In_ const CEncryptionKey &cSrc) throw(...);
  ~CEncryptionKey();

  CEncryptionKey &operator=(_In_ CEncryptionKey const &cSrc) throw(...);

  HRESULT Generate(_In_ MX::CEncryptionKey::eAlgorithm nAlgorithm, _In_opt_ SIZE_T nBitsCount = 0);
  SIZE_T GetBitsCount() const;

  BOOL HasPrivateKey() const;

  // This method auto detects PEM and DER formats
  HRESULT Set(_In_ LPCVOID lpKey, _In_ SIZE_T nKeySize, _In_opt_z_ LPCSTR szPasswordA = NULL);
  VOID Set(_In_ EVP_PKEY *lpKey);

  HRESULT GetPublicKey(_Out_ CSecureBuffer **lplpBuffer);
  HRESULT GetPrivateKey(_Out_ CSecureBuffer **lplpBuffer);
  HRESULT GetKeyParams(_Out_ CSecureBuffer **lplpBuffer);

  EVP_PKEY* GetPKey() const
    {
    return lpKey;
    };

  int GetBaseId() const;
  eAlgorithm GetAlgorithm() const;

private:
  EVP_PKEY *lpKey{ NULL };
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_ENCRYPTION_KEY_H
