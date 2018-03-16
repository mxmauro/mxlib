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
