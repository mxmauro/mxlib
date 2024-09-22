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
#include "..\..\Include\Http\Email.h"

//-----------------------------------------------------------

namespace MX {

BOOL IsValidEMailAddress(_In_ LPCSTR szAddressA, _In_opt_ SIZE_T nAddressLen)
{
  BOOL bLastWasDot, bAtFound;
  LPCSTR szAtPosA;

  if (nAddressLen == (SIZE_T)-1)
    nAddressLen = StrLenA(szAddressA);
  if (szAddressA == NULL || nAddressLen == 0)
    return FALSE;
  if (*szAddressA == '@')
    return FALSE;
  bLastWasDot = bAtFound = FALSE;
  szAtPosA = NULL;
  for (; nAddressLen>0; szAddressA++, nAddressLen--)
  {
    if (*((LPBYTE)szAddressA) <= ' ')
      return FALSE;
    switch (*szAddressA)
    {
      case '.':
        if (bLastWasDot != FALSE || szAtPosA == szAddressA-1)
          return FALSE;
        bLastWasDot = TRUE;
        break;

      case '@':
        if (bLastWasDot != FALSE || szAtPosA != NULL)
          return FALSE;
        szAtPosA = szAddressA;
        bLastWasDot = FALSE;
        break;

      default:
        if (StrChrA("()<>,;:\\/\"[]", *szAddressA) != NULL)
          return FALSE;
        bLastWasDot = FALSE;
        break;
    }
  }
  return (bLastWasDot != FALSE || szAtPosA == NULL || szAtPosA == szAddressA-1) ? FALSE : TRUE;
}

BOOL IsValidEMailAddress(_In_ LPCWSTR szAddressW, _In_opt_ SIZE_T nAddressLen)
{
  BOOL bLastWasDot, bAtFound;
  LPCWSTR szAtPosW;

  if (nAddressLen == (SIZE_T)-1)
    nAddressLen = StrLenW(szAddressW);
  if (szAddressW == NULL || nAddressLen == 0)
    return FALSE;
  if (*szAddressW == L'@')
    return FALSE;
  bLastWasDot = bAtFound = FALSE;
  szAtPosW = NULL;
  for (; nAddressLen>0; szAddressW++, nAddressLen--)
  {
    if (*szAddressW <= L' ')
      return FALSE;
    switch (*szAddressW)
    {
      case L'.':
        if (bLastWasDot != FALSE || szAtPosW == szAddressW-1)
          return FALSE;
        bLastWasDot = TRUE;
        break;

      case L'@':
        if (bLastWasDot != FALSE || szAtPosW != NULL)
          return FALSE;
        szAtPosW = szAddressW;
        bLastWasDot = FALSE;
        break;

      default:
        if (StrChrW(L"()<>,;:\\/\"[]", *szAddressW) != NULL)
          return FALSE;
        bLastWasDot = FALSE;
        break;
    }
  }
  return (bLastWasDot != FALSE || szAtPosW == NULL || szAtPosW == szAddressW-1) ? FALSE : TRUE;
}

} //namespace MX
