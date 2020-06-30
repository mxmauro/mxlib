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
#ifndef _MX_DH_PARAM_H
#define _MX_DH_PARAM_H

#include "..\Defines.h"
#include "..\RefCounted.h"
#include "SecureBuffer.h"
typedef struct dh_st DH;

//-----------------------------------------------------------

namespace MX {

class CDhParam : public virtual TRefCounted<CBaseMemObj>
{
public:
  typedef enum {
    AlgorithmRSA = 0, AlgorithmED25519, AlgorithmED448
  } eAlgorithm;

public:
  CDhParam();
  CDhParam(_In_ const CDhParam &cSrc) throw(...);
  ~CDhParam();

  CDhParam &operator=(CDhParam const &cSrc) throw(...);

  HRESULT Generate(_In_ SIZE_T nBitsCount, _In_opt_ BOOL bUseDSA = FALSE);
  SIZE_T GetBitsCount() const;

  HRESULT SetFromDER(_In_ LPCVOID lpDer, _In_ SIZE_T nDerSize);
  HRESULT SetFromPEM(_In_z_ LPCSTR szPemA, _In_opt_z_ LPCSTR szPasswordA = NULL, _In_opt_ SIZE_T nPemLen = (SIZE_T)-1);
  HRESULT Get(_Out_ CSecureBuffer **lplpBuffer);

  DH* GetDH() const
    {
    return lpDH;
    };

private:
  DH *lpDH;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_DH_PARAM_H
