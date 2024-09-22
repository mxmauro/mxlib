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
#ifndef _MX_MESSAGE_DIGEST_H
#define _MX_MESSAGE_DIGEST_H

#include "..\Defines.h"

//-----------------------------------------------------------

namespace MX {

class CMessageDigest : public virtual CBaseMemObj, public CNonCopyableObj
{
public:
  enum class eAlgorithm
  {
    Invalid = 1,
    CRC32 = 0,
    MD5, MD4,
    SHA1, SHA224, SHA256, SHA384, SHA512, SHA512_224, SHA512_256,
    SHA3_224, SHA3_256, SHA3_384, SHA3_512,
    Blake2s_256, Blake2b_512
  };

public:
  CMessageDigest();
  ~CMessageDigest();

  HRESULT BeginDigest(_In_z_ LPCSTR szAlgorithmA, _In_opt_ LPCVOID lpKey = NULL, _In_opt_ SIZE_T nKeyLen = 0);
  HRESULT BeginDigest(_In_ MX::CMessageDigest::eAlgorithm nAlgorithm, _In_opt_ LPCVOID lpKey = NULL,
                      _In_opt_ SIZE_T nKeyLen = 0);

  HRESULT DigestStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength);
  HRESULT DigestWordLE(_In_ LPWORD lpnValues, _In_ SIZE_T nCount);
  HRESULT DigestWordBE(_In_ LPWORD lpnValues, _In_ SIZE_T nCount);
  HRESULT DigestDWordLE(_In_ LPDWORD lpnValues, _In_ SIZE_T nCount);
  HRESULT DigestDWordBE(_In_ LPDWORD lpnValues, _In_ SIZE_T nCount);
  HRESULT DigestQWordLE(_In_ ULONGLONG *lpnValues, _In_ SIZE_T nCount);
  HRESULT DigestQWordBE(_In_ ULONGLONG *lpnValues, _In_ SIZE_T nCount);

  HRESULT EndDigest();

  LPBYTE GetResult() const;
  SIZE_T GetResultSize() const;

  //NOTE: Returns -1 if unsupported
  static MX::CMessageDigest::eAlgorithm GetAlgorithm(_In_z_ LPCSTR szAlgorithmA);

private:
  VOID CleanUp(_In_ BOOL bZeroData);

private:
  LPVOID lpInternalData;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_MESSAGE_DIGEST_H
