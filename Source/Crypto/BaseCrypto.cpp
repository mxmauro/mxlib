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
#include "..\..\Include\Crypto\BaseCrypto.h"

//-------------------------------------------------------

namespace MX {

CBaseCrypto::CBaseCrypto() : CBaseMemObj()
{
  return;
}

SIZE_T CBaseCrypto::GetAvailableEncryptedData()
{
  SIZE_T nSize1, nSize2;

  cEncryptedData.GetReadPtr(NULL, &nSize1, NULL, &nSize2);
  return nSize1+nSize2;
}

SIZE_T CBaseCrypto::GetEncryptedData(_Out_writes_(nDestSize) LPVOID lpDest, _In_ SIZE_T nDestSize)
{
  LPBYTE lpPtr1, lpPtr2;
  SIZE_T nSize1, nSize2;

  if (lpDest == NULL || nDestSize == 0)
    return 0;
  cEncryptedData.GetReadPtr(&lpPtr1, &nSize1, &lpPtr2, &nSize2);
  if (nDestSize <= nSize1)
  {
    MemCopy(lpDest, lpPtr1, nDestSize);
  }
  else
  {
    if (nDestSize > nSize1+nSize2)
      nDestSize = nSize1+nSize2;
    MemCopy(lpDest,                lpPtr1, nSize1);
    MemCopy((LPBYTE)lpDest+nSize1, lpPtr2, nDestSize-nSize1);
  }
  cEncryptedData.AdvanceReadPtr(nDestSize);
  return nDestSize;
}

SIZE_T CBaseCrypto::GetAvailableDecryptedData()
{
  SIZE_T nSize1, nSize2;

  cDecryptedData.GetReadPtr(NULL, &nSize1, NULL, &nSize2);
  return nSize1+nSize2;
}

SIZE_T CBaseCrypto::GetDecryptedData(_Out_writes_(nDestSize) LPVOID lpDest, _In_ SIZE_T nDestSize)
{
  LPBYTE lpPtr1, lpPtr2;
  SIZE_T nSize1, nSize2;

  if (lpDest == NULL || nDestSize == 0)
    return 0;
  cDecryptedData.GetReadPtr(&lpPtr1, &nSize1, &lpPtr2, &nSize2);
  if (nDestSize <= nSize1)
  {
    MemCopy(lpDest, lpPtr1, nDestSize);
  }
  else
  {
    if (nDestSize > nSize1+nSize2)
      nDestSize = nSize1+nSize2;
    MemCopy(lpDest,                lpPtr1, nSize1);
    MemCopy((LPBYTE)lpDest+nSize1, lpPtr2, nDestSize-nSize1);
  }
  cDecryptedData.AdvanceReadPtr(nDestSize);
  return nDestSize;
}

} //namespace MX
