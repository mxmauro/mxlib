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
#include "..\..\Include\Strings\Utf8.h"

//-----------------------------------------------------------

namespace MX {

HRESULT Utf8_Encode(_Inout_ CStringA &cStrDestA, _In_z_ LPCWSTR szSrcW, _In_opt_ SIZE_T nSrcLen,
                    _In_opt_ BOOL bAppend)
{
  int k;
  CHAR chA[4];

  if (bAppend == FALSE)
    cStrDestA.Empty();
  if (nSrcLen == (SIZE_T)-1)
    nSrcLen = StrLenW(szSrcW);
  if (szSrcW == NULL && nSrcLen > 0)
    return E_POINTER;
  while (nSrcLen > 0)
  {
    if (szSrcW[0] >= 0xD800 && szSrcW[0] <= 0xDBFF)
    {
      if (nSrcLen < 2 || szSrcW[1] < 0xDC00 || szSrcW[1] > 0xDFFF)
        return MX_E_InvalidData;
      k = Utf8_EncodeChar(chA, szSrcW[0], szSrcW[1]);
      szSrcW += 2;
      nSrcLen -= 2;
    }
    else
    {
      k = Utf8_EncodeChar(chA, szSrcW[0], 0);
      szSrcW++;
      nSrcLen--;
    }
    if (k < 0)
      return MX_E_InvalidData;
    if (cStrDestA.ConcatN(chA, (SIZE_T)k) == FALSE)
      return E_OUTOFMEMORY;
  }
  return S_OK;
}

HRESULT Utf8_Decode(_Inout_ CStringW &cStrDestW, _In_z_ LPCSTR szSrcA, _In_opt_ SIZE_T nSrcLen,
                    _In_opt_ BOOL bAppend)
{
  WCHAR chW[2];
  int res;

  if (bAppend == FALSE)
    cStrDestW.Empty();
  if (nSrcLen == (SIZE_T)-1)
    nSrcLen = StrLenA(szSrcA);
  if (szSrcA == NULL && nSrcLen > 0)
    return E_POINTER;
  while (nSrcLen > 0)
  {
    res = Utf8_DecodeChar(chW, szSrcA, nSrcLen);
    if (res <= 0)
      return MX_E_InvalidData;
    if (cStrDestW.ConcatN(chW, (chW[1] == 0) ? 1 : 2) == FALSE)
      return E_OUTOFMEMORY;
    szSrcA += (SIZE_T)res;
    nSrcLen -= (SIZE_T)res;
  }
  return S_OK;
}

int Utf8_EncodeChar(_Out_opt_ CHAR szDestA[], _In_ WCHAR chW, _In_opt_ WCHAR chSurrogatePairW)
{
  static const BYTE aFirstByteMark[4] ={ 0x00, 0xC0, 0xE0, 0xF0 };
  SIZE_T nVal;
  int k;
  CHAR chA[4];

  if (szDestA == NULL)
    szDestA = chA;
#pragma warning(suppress : 6386)
  szDestA[0] = szDestA[1] = szDestA[2] = szDestA[3] = 0;
  if (chW >= 0xD800 && chW <= 0xDBFF && chSurrogatePairW != 0)
  {
    if (chSurrogatePairW < 0xDC00 || chSurrogatePairW > 0xDFFF)
      return -1;
    nVal = ((SIZE_T)(chW              - 0xD800) << 10) + 0x10000;
    nVal += (SIZE_T)(chSurrogatePairW - 0xDC00);
  }
  else
  {
    nVal = (SIZE_T)chW;
  }
  if (nVal < 0x80)
    k = 1;
  else if (nVal < 0x800)
    k = 2;
  else if (nVal < 0x10000)
    k = 3;
  else
    k = 4;
  switch (k)
  {
    case 4:
      szDestA[3] = (CHAR)((nVal | 0x80) & 0xBF);
      nVal >>= 6;
    case 3:
      szDestA[2] = (CHAR)((nVal | 0x80) & 0xBF);
      nVal >>= 6;
    case 2:
      szDestA[1] = (CHAR)((nVal | 0x80) & 0xBF);
      nVal >>= 6;
    case 1:
      szDestA[0] =  (CHAR)(nVal | (SIZE_T)aFirstByteMark[k-1]);
      break;
  }
  return k;
}

int Utf8_DecodeChar(_Out_opt_ WCHAR szDestW[], _In_z_ LPCSTR szSrcA, _In_opt_ SIZE_T nSrcLen)
{
  WCHAR chW[2];
  UCHAR *sA;
  DWORD dw;

  if (szDestW == NULL)
    szDestW = chW;
#pragma warning(suppress : 6386)
  szDestW[0] = szDestW[1] = 0;
  if (szSrcA == NULL || *szSrcA == 0 || nSrcLen == 0)
    return 0;
  sA = (UCHAR*)szSrcA;
  if (*sA < 0x80)
  {
    *szDestW = (WCHAR)(*sA);
    return 1;
  }
  if (*sA < 0xC2 || *sA > 0xF8)
    return -1;
  if (*sA <= 0xDF)
  {
    if (nSrcLen < 2 || (sA[1] & 0xC0) != 0x80)
      return -1;
    *szDestW = (WCHAR)((((DWORD)sA[0] << 6) +
                         (DWORD)sA[1]) - 0x3080UL);
    return 2;
  }
  if (*szSrcA <= 0xEF)
  {
    if (nSrcLen < 3 || (sA[1] & 0xC0) != 0x80 || (sA[2] & 0xC0) != 0x80 ||
        (sA[0] == 0xE0 && sA[0] < 0xA0) || (sA[0] == 0xED && sA[0] >= 0xA0))
      return -1;
    *szDestW = (WCHAR)((((DWORD)sA[0] << 12) +
                        ((DWORD)sA[1] <<  6) +
                         (DWORD)sA[2]) - 0xE2080UL);
    return 3;
  }
  if (nSrcLen < 4 || (sA[1] & 0xC0) != 0x80 || (sA[2] & 0xC0) != 0x80 || (sA[3] & 0xC0) != 0x80 ||
      (sA[0] == 0xF0 && sA[1] < 0x90) || (sA[0] == 0xF4 && sA[1] >= 0x90))
    return -1;
  dw = (((DWORD)sA[0] << 18) +
        ((DWORD)sA[1] << 12) +
        ((DWORD)sA[2] <<  6) +
         (DWORD)sA[3]) - 0x3C82080UL;
    
  if (dw <= 0xFFFFUL)
  {
    *szDestW = (WCHAR)dw;
  }
  else
  {
    dw -= 0x10000UL;
    szDestW[0] = (WCHAR)((dw >> 10  ) + 0xD800);
    szDestW[1] = (WCHAR)((dw & 0x3FF) + 0xDC00);
  }
  return 4;
}

SIZE_T Utf8_StrLen(_In_z_ LPCSTR szSrcA)
{
  SIZE_T nLen;

  if (szSrcA == NULL)
    return 0;
  nLen = 0;
  while (*szSrcA != 0)
  {
    if (*szSrcA < 0x80)
    {
      nLen++;
      szSrcA++;
    }
    else if (*szSrcA < 0xC2 || *szSrcA > 0xF8)
    {
      //note that 0xC0 and 0xC1 will give a two-byte sequence but
      //with a codepoint <= 127... it is not legal in utf-8
      return 0;
    }
    else if (*szSrcA <= 0xDF)
    {
      if (szSrcA[1] == 0)
        return 0;
      nLen++;
      szSrcA += 2;
    }
    else if (*szSrcA <= 0xEF)
    {
      if (szSrcA[1] == 0 || szSrcA[2] == 0)
        return 0;
      nLen++;
      szSrcA += 3;
    }
    else
    {
      if (szSrcA[1] == 0 || szSrcA[2] == 0 || szSrcA[3] == 0)
        return 0;
      nLen += 2;
      szSrcA += 4;
    }
  }
  return nLen;
}

int Utf8_StrCompareA(_In_z_ LPCSTR szSrcUtf8, _In_z_ LPCSTR szSrcA, _In_opt_ BOOL bCaseInsensitive)
{
  SIZE_T nLen[3];
  int res;

  nLen[0] = Utf8_StrLen(szSrcUtf8);
  nLen[1] = StrLenA(szSrcA);
  //https://graphics.stanford.edu/~seander/bithacks.html#IntegerMinOrMax
  nLen[2] = nLen[1] ^ ((nLen[0] ^ nLen[1]) & -(nLen[0] < nLen[1]));
  res = Utf8_StrNCompareA(szSrcUtf8, szSrcA, nLen[2], bCaseInsensitive);
  if (res == 0)
  {
    if (nLen[0] < nLen[1])
      res = -1;
    else if (nLen[0] > nLen[1])
      res = 1;
  }
  return res;
}

int Utf8_StrCompareW(_In_z_ LPCSTR szSrcUtf8, _In_z_ LPCWSTR szSrcW, _In_opt_ BOOL bCaseInsensitive)
{
  SIZE_T nLen[3];
  int res;

  nLen[0] = Utf8_StrLen(szSrcUtf8);
  nLen[1] = StrLenW(szSrcW);
  //https://graphics.stanford.edu/~seander/bithacks.html#IntegerMinOrMax
  nLen[2] = nLen[1] ^ ((nLen[0] ^ nLen[1]) & -(nLen[0] < nLen[1]));
  res = Utf8_StrNCompareW(szSrcUtf8, szSrcW, nLen[2], bCaseInsensitive);
  if (res == 0)
  {
    if (nLen[0] < nLen[1])
      res = -1;
    else if (nLen[0] > nLen[1])
      res = 1;
  }
  return res;
}

int Utf8_StrNCompareA(_In_z_ LPCSTR szSrcUtf8, _In_z_ LPCSTR szSrcA, _In_ SIZE_T nLen, _In_opt_ BOOL bCaseInsensitive)
{
  WCHAR szTempBufW[64];
  SIZE_T nTempBufLen;
  int res;

  if (nLen == 0 || (szSrcUtf8 == NULL && szSrcA == NULL))
    return 0;
  if (szSrcUtf8 == NULL)
    return -1;
  if (szSrcA == NULL)
    return 1;
  res = 0;
  while (res == 0 && nLen > 0)
  {
    nTempBufLen = 0;
    while (nTempBufLen < nLen && nTempBufLen < MX_ARRAYLEN(szTempBufW)-1)
    {
      res = Utf8_DecodeChar(szTempBufW+nTempBufLen, szSrcUtf8);
      if (res <= 0)
        return 1; //on error, the other is less
      nTempBufLen += (szTempBufW[nTempBufLen+1] == 0) ? 1 : 2;
      szSrcUtf8 += res;
    }
    if (nTempBufLen > nLen)
      nTempBufLen = nLen;
    res = StrNCompareAW(szSrcA, szTempBufW, nTempBufLen, bCaseInsensitive);
    if (res == 0)
    {
      szSrcA += nTempBufLen;
      nLen -= nTempBufLen;
    }
  }
  return -res; //negate result because string parameter swap in call to 'StrNCompareAW'
}

int Utf8_StrNCompareW(_In_z_ LPCSTR szSrcUtf8, _In_z_ LPCWSTR szSrcW, _In_ SIZE_T nLen, _In_opt_ BOOL bCaseInsensitive)
{
  WCHAR szTempBufW[64];
  SIZE_T nTempBufLen;
  int res;

  if (nLen == 0 || (szSrcUtf8 == NULL && szSrcW == NULL))
    return 0;
  if (szSrcUtf8 == NULL)
    return -1;
  if (szSrcW == NULL)
    return 1;
  res = 0;
  while (res == 0 && nLen > 0)
  {
    nTempBufLen = 0;
    while (nTempBufLen < nLen && nTempBufLen < MX_ARRAYLEN(szTempBufW)-1)
    {
      res = Utf8_DecodeChar(szTempBufW+nTempBufLen, szSrcUtf8);
      if (res <= 0)
        return 1; //on error, the other is less
      nTempBufLen += (szTempBufW[nTempBufLen+1] == 0) ? 1 : 2;
      szSrcUtf8 += res;
    }
    if (nTempBufLen > nLen)
      nTempBufLen = nLen;
    res = StrNCompareW(szTempBufW, szSrcW, nTempBufLen, bCaseInsensitive);
    if (res == 0)
    {
      szSrcW += nTempBufLen;
      nLen -= nTempBufLen;
    }
  }
  return res;
}

} //namespace MX
