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
#include "..\..\Include\Crypto\Base64.h"

//-----------------------------------------------------------

static const LPCSTR szBase64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "abcdefghijklmnopqrstuvwxyz"
                                    "0123456789+/";

static const BYTE aDecodeTable[] = "|$$$}rstuvwxyz{$$$=$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

//-----------------------------------------------------------

namespace MX {

SIZE_T Base64GetEncodedLength(__in SIZE_T nDataLen)
{
  return ((nDataLen+2) / 3) << 2;
}

SIZE_T Base64Encode(__out LPSTR szDestA, __in LPVOID lpData, __in SIZE_T nDataLen)
{
  BYTE aTempBuf[3];
  SIZE_T i, j, nOutLen;

  if (lpData == NULL && nDataLen > 0)
    return 0;
  if (szDestA == NULL)
    return Base64GetEncodedLength(nDataLen);
  i = nOutLen = 0;
  while (nDataLen > 0)
  {
    aTempBuf[i] = *((LPBYTE)lpData);
    if ((++i) == 3)
    {
      szDestA[0] = szBase64Chars[ (aTempBuf[0] & 0xFC) >> 2                               ];
      szDestA[1] = szBase64Chars[((aTempBuf[0] & 0x03) << 4) + ((aTempBuf[1] & 0xF0) >> 4)];
      szDestA[2] = szBase64Chars[((aTempBuf[1] & 0x0F) << 2) + ((aTempBuf[2] & 0xC0) >> 6)];
      szDestA[3] = szBase64Chars[  aTempBuf[2] & 0x3F                                     ];
      szDestA += 4;
      nOutLen += 4;
      i = 0;
    }
    lpData = (LPBYTE)lpData + 1;
    nDataLen--;
  }
  if (i > 0)
  {
    for (j=i; j<3; j++)
      aTempBuf[j] = 0;
    szDestA[0] = szBase64Chars[ (aTempBuf[0] & 0xFC) >> 2                               ];
    szDestA[1] = szBase64Chars[((aTempBuf[0] & 0x03) << 4) + ((aTempBuf[1] & 0xF0) >> 4)];
    szDestA[2] = szBase64Chars[((aTempBuf[1] & 0x0F) << 2) + ((aTempBuf[2] & 0xC0) >> 6)];
    szDestA[3] = szBase64Chars[  aTempBuf[2] & 0x3F                                     ];
    while (i < 3)
      szDestA[++i] = '=';
    nOutLen += 4;
  }
  return nOutLen;
}

SIZE_T Base64Encode(__inout CStringA &cStrDestA, __in LPVOID lpData, __in SIZE_T nDataLen)
{
  SIZE_T nDestLen, nOutLen;

  cStrDestA.Empty();
  if (lpData == NULL || nDataLen == 0)
    return 0;
  nDestLen = Base64GetEncodedLength(nDataLen);
  if (cStrDestA.EnsureBuffer(nDestLen+2) == FALSE)
    return 0;
  nOutLen = Base64Encode((LPSTR)cStrDestA, lpData, nDataLen);
  if (nOutLen > 0)
  {
    ((LPSTR)cStrDestA)[nOutLen] = 0;
    cStrDestA.Refresh();
  }
  else
  {
    cStrDestA.Empty();
  }
  return nOutLen;
}

SIZE_T Base64GetMaxDecodedLength(__in SIZE_T nDataLen)
{
  return ((nDataLen+3) >> 2) * 3;
}

SIZE_T Base64Decode(__out LPVOID lpDest, __in LPCSTR szStrA, __in_opt SIZE_T nSrcLen)
{
  BYTE aTempBuf[4], aTempBuf2[3];
  SIZE_T i, j, nEqualCounter, nDestLen;
  BYTE val;

  if (nSrcLen == (SIZE_T)-1)
    nSrcLen = StrLenA(szStrA);
  if (szStrA == NULL && nSrcLen > 0)
    return 0;
  i = nEqualCounter = nDestLen = 0;
  while (nSrcLen > 0)
  {
    val = (*((LPBYTE)szStrA) < 43 || *((LPBYTE)szStrA) > 122) ? '$' : aDecodeTable[*((LPBYTE)szStrA) - 43];
    if (val == (BYTE)'=')
    {
      if ((++nEqualCounter) > 2)
        return 0;
      //just ignore char
    }
    else if (val == (BYTE)'$')
    {
      //just ignore char
    }
    else
    {
      //reset 'equal signs' that where in the middle
      nEqualCounter = 0;
      if (lpDest != NULL)
      {
        aTempBuf[i++] = val - 62;
        if (i == 4)
        {
          ((LPBYTE)lpDest)[0] = ((aTempBuf[0] << 2)         | (aTempBuf[1] >> 4));
          ((LPBYTE)lpDest)[1] = ((aTempBuf[1] << 4)         | (aTempBuf[2] >> 2));
          ((LPBYTE)lpDest)[2] = (((aTempBuf[2] << 6) & 0xC0) |  aTempBuf[3]);
          lpDest = (LPBYTE)lpDest + 3;
          i = 0;
          nDestLen += 3;
        }
      }
      else
      {
        if ((++i) == 4)
        {
          i = 0;
          nDestLen += 3;
        }
      }
    }
    szStrA++;
    nSrcLen--;
  }
  if (i > 1) //if only one remaining char, just ignore
  {
    if (lpDest != NULL)
    {
      for (j=i; j<4; j++)
        aTempBuf[j] = 0;
      aTempBuf2[0] = ( (aTempBuf[0] << 2)         | (aTempBuf[1] >> 4));
      aTempBuf2[1] = ( (aTempBuf[1] << 4)         | (aTempBuf[2] >> 2));
      aTempBuf2[2] = (((aTempBuf[2] << 6) & 0xC0) |  aTempBuf[3]);
      i--;
      MemCopy(lpDest, aTempBuf2, i);
    }
    else
    {
      i--;
    }
    nDestLen += i;
  }
  return nDestLen;
}

} //namespace MX
