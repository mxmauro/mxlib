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

SIZE_T CBaseCrypto::GetEncryptedData(__out LPVOID lpDest, __in SIZE_T nDestSize)
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

SIZE_T CBaseCrypto::GetDecryptedData(__out LPVOID lpDest, __in SIZE_T nDestSize)
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
