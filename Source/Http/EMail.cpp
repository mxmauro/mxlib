/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2006-2015. All rights reserved.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 *
 * 2. This notice may not be removed or altered from any source distribution.
 *
 * 3. YOU MAY NOT:
 *
 *    a. Modify, translate, adapt, alter, or create derivative works from
 *       this software.
 *    b. Copy (other than one back-up copy), distribute, publicly display,
 *       transmit, sell, rent, lease or otherwise exploit this software.
 *    c. Distribute, sub-license, rent, lease, loan [or grant any third party
 *       access to or use of the software to any third party.
 **/
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
