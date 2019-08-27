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
#include "..\..\Include\Crypto\BaseDigestAlgorithm.h"

//-----------------------------------------------------------

namespace MX {

CBaseDigestAlgorithm::CBaseDigestAlgorithm() : CBaseMemObj()
{
  return;
}

HRESULT CBaseDigestAlgorithm::DigestWordLE(_In_ LPWORD lpnValues, _In_ SIZE_T nCount)
{
  return DigestStream(lpnValues, nCount * sizeof(WORD));
}

HRESULT CBaseDigestAlgorithm::DigestWordBE(_In_ LPWORD lpnValues, _In_ SIZE_T nCount)
{
  WORD aTempValues[32];
  SIZE_T i;
  HRESULT hRes;

  if (lpnValues == NULL && nCount > 0)
    return E_POINTER;
  hRes = S_OK;
  while (SUCCEEDED(hRes) && nCount > 0)
  {
    for (i=0; nCount>0 && i<MX_ARRAYLEN(aTempValues); i++,nCount--,lpnValues++)
    {
      aTempValues[i] = (((*lpnValues) & 0xFF00) >> 8) |
                       (((*lpnValues) & 0x00FF) << 8);
    }
    hRes = DigestStream(aTempValues, i * sizeof(WORD));
  }
  return hRes;
}

HRESULT CBaseDigestAlgorithm::DigestDWordLE(_In_ LPDWORD lpnValues, _In_ SIZE_T nCount)
{
  return DigestStream(lpnValues, nCount * sizeof(DWORD));
}

HRESULT CBaseDigestAlgorithm::DigestDWordBE(_In_ LPDWORD lpnValues, _In_ SIZE_T nCount)
{
  DWORD aTempValues[32];
  SIZE_T i;
  HRESULT hRes;

  if (lpnValues == NULL && nCount > 0)
    return E_POINTER;
  hRes = S_OK;
  while (SUCCEEDED(hRes) && nCount > 0)
  {
    for (i=0; nCount>0 && i<MX_ARRAYLEN(aTempValues); i++,nCount--,lpnValues++)
    {
      aTempValues[i] = (((*lpnValues) & 0xFF000000) >> 24) |
                       (((*lpnValues) & 0x00FF0000) >>  8) |
                       (((*lpnValues) & 0x0000FF00) <<  8) |
                       (((*lpnValues) & 0x000000FF) << 24);
    }
    hRes = DigestStream(aTempValues, i * sizeof(DWORD));
  }
  return hRes;
}

HRESULT CBaseDigestAlgorithm::DigestQWordLE(_In_ ULONGLONG *lpnValues, _In_ SIZE_T nCount)
{
  return DigestStream(lpnValues, nCount * sizeof(ULONGLONG));
}

HRESULT CBaseDigestAlgorithm::DigestQWordBE(_In_ ULONGLONG *lpnValues, _In_ SIZE_T nCount)
{
  ULONGLONG aTempValues[32];
  SIZE_T i;
  HRESULT hRes;

  if (lpnValues == NULL && nCount > 0)
    return E_POINTER;
  hRes = S_OK;
  while (SUCCEEDED(hRes) && nCount > 0)
  {
    for (i=0; nCount>0 && i<MX_ARRAYLEN(aTempValues); i++,nCount--,lpnValues++)
    {
      aTempValues[i] = (((*lpnValues) & 0xFF00000000000000ui64) >> 56) |
                       (((*lpnValues) & 0x00FF000000000000ui64) >> 40) |
                       (((*lpnValues) & 0x0000FF0000000000ui64) >> 24) |
                       (((*lpnValues) & 0x000000FF00000000ui64) >>  8) |
                       (((*lpnValues) & 0x00000000FF000000ui64) <<  8) |
                       (((*lpnValues) & 0x0000000000FF0000ui64) << 24) |
                       (((*lpnValues) & 0x000000000000FF00ui64) << 40) |
                       (((*lpnValues) & 0x00000000000000FFui64) << 56);
    }
    hRes = DigestStream(aTempValues, i * sizeof(ULONGLONG));
  }
  return hRes;
}


} //namespace MX
