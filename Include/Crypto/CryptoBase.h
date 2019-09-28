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
#ifndef _MX_CRYPTOBASE_H
#define _MX_CRYPTOBASE_H

#include "..\Defines.h"
#include "..\CircularBuffer.h"

//-----------------------------------------------------------

namespace MX {

class MX_NOVTABLE CCryptoBase : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CCryptoBase);
protected:
  CCryptoBase();

public:
  virtual HRESULT BeginEncrypt()=0;
  virtual HRESULT EncryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)=0;
  virtual HRESULT EndEncrypt()=0;
  SIZE_T GetAvailableEncryptedData();
  SIZE_T GetEncryptedData(_Out_writes_(nDestSize) LPVOID lpDest, _In_ SIZE_T nDestSize);

  virtual HRESULT BeginDecrypt()=0;
  virtual HRESULT DecryptStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)=0;
  virtual HRESULT EndDecrypt()=0;
  SIZE_T GetAvailableDecryptedData();
  SIZE_T GetDecryptedData(_Out_writes_(nDestSize) LPVOID lpDest, _In_ SIZE_T nDestSize);

protected:
  CCircularBuffer cEncryptedData;
  CCircularBuffer cDecryptedData;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_CRYPTOBASE_H
