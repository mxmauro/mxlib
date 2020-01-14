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
#ifndef _MX_DIGESTALGORITHMBASE_H
#define _MX_DIGESTALGORITHMBASE_H

#include "..\Defines.h"

//-----------------------------------------------------------

namespace MX {

class MX_NOVTABLE CDigestAlgorithmBase : public virtual CBaseMemObj
{
protected:
  CDigestAlgorithmBase();

public:
  virtual HRESULT BeginDigest() = 0;
  virtual HRESULT DigestStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength) = 0;
  HRESULT DigestWordLE(_In_ LPWORD lpnValues, _In_ SIZE_T nCount);
  HRESULT DigestWordBE(_In_ LPWORD lpnValues, _In_ SIZE_T nCount);
  HRESULT DigestDWordLE(_In_ LPDWORD lpnValues, _In_ SIZE_T nCount);
  HRESULT DigestDWordBE(_In_ LPDWORD lpnValues, _In_ SIZE_T nCount);
  HRESULT DigestQWordLE(_In_ ULONGLONG *lpnValues, _In_ SIZE_T nCount);
  HRESULT DigestQWordBE(_In_ ULONGLONG *lpnValues, _In_ SIZE_T nCount);
  virtual HRESULT EndDigest() = 0;

  virtual LPBYTE GetResult() const = 0;
  virtual SIZE_T GetResultSize() const = 0;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_DIGESTALGORITHMBASE_H
