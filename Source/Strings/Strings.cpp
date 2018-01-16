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
#include "..\..\Include\Strings\Strings.h"
#include "..\..\Include\WaitableObjects.h"
#include <stdio.h>

//-----------------------------------------------------------

#define X_ALIGN(x, _size)                                  \
                        (((x)+((_size)-1)) & (~((_size)-1)))

//-----------------------------------------------------------

typedef int (__cdecl *lpfn__stdio_common_vsnprintf_s)(_In_ unsigned __int64 _Options,
                                                      _Out_writes_z_(_BufferCount) char* _Buffer,
                                                      _In_ size_t _BufferCount, _In_ size_t _MaxCount,
                                                      _In_z_ _Printf_format_string_params_(2) char const* _Format,
                                                      _In_opt_ _locale_t _Locale, va_list _ArgList);
typedef int (__cdecl *lpfn__stdio_common_vsnwprintf_s)(_In_ unsigned __int64 _Options,
                                                       _Out_writes_z_(_BufferCount) wchar_t* _Buffer,
                                                       _In_ size_t _BufferCount, _In_ size_t _MaxCount,
                                                       _In_z_ _Printf_format_string_params_(2) wchar_t const* _Format,
                                                       _In_opt_ _locale_t _Locale, va_list _ArgList);

//-----------------------------------------------------------

static LONG volatile nInitialized = 0;
static CHAR volatile aToLowerChar[256] = { 0 };
static WCHAR volatile aToUnicodeChar[256] = { 0 };

extern const lpfn__stdio_common_vsnprintf_s mx__stdio_common_vsnprintf_s_default = NULL;
extern const lpfn__stdio_common_vsnwprintf_s mx__stdio_common_vsnwprintf_s_default = NULL;
#if defined (_M_IX86)
  #pragma comment(linker, "/alternatename:___stdio_common_vsnprintf_s=_mx__stdio_common_vsnprintf_s_default")
  #pragma comment(linker, "/alternatename:___stdio_common_vsnwprintf_s=_mx__stdio_common_vsnwprintf_s_default")
#elif defined (_M_IA64) || defined (_M_AMD64)
  #pragma comment(linker, "/alternatename:__stdio_common_vsnprintf_s=mx__stdio_common_vsnprintf_s_default")
  #pragma comment(linker, "/alternatename:__stdio_common_vsnwprintf_s=mx__stdio_common_vsnwprintf_s_default")
#endif

//-----------------------------------------------------------

static VOID InitializeTables();

//-----------------------------------------------------------

namespace MX {

SIZE_T StrLenA(__in_z_opt LPCSTR szSrcA)
{
  SIZE_T nLen = 0;

  if (szSrcA != NULL)
  {
    for (; szSrcA[nLen] != 0; nLen++);
  }
  return nLen;
}

SIZE_T StrLenW(__in_z_opt LPCWSTR szSrcW)
{
  SIZE_T nLen = 0;

  if (szSrcW != NULL)
  {
    for (; szSrcW[nLen] != 0; nLen++);
  }
  return nLen;
}

int StrCompareA(__in_z LPCSTR szSrcA1, __in_z LPCSTR szSrcA2, __in_opt BOOL bCaseInsensitive)
{
  SIZE_T nLen[3];
  int res;

  nLen[0] = StrLenA(szSrcA1);
  nLen[1] = StrLenA(szSrcA2);
  //https://graphics.stanford.edu/~seander/bithacks.html#IntegerMinOrMax
  nLen[2] = nLen[1] ^ ((nLen[0] ^ nLen[1]) & -(nLen[0] < nLen[1]));
  res = StrNCompareA(szSrcA1, szSrcA2, nLen[2], bCaseInsensitive);
  if (res == 0)
  {
    if (nLen[0] < nLen[1])
      res = -1;
    else if (nLen[0] > nLen[1])
      res = 1;
  }
  return res;
}

int StrCompareW(__in_z LPCWSTR szSrcW1, __in_z LPCWSTR szSrcW2, __in_opt BOOL bCaseInsensitive)
{
  SIZE_T nLen[3];
  int res;

  nLen[0] = StrLenW(szSrcW1);
  nLen[1] = StrLenW(szSrcW2);
  //https://graphics.stanford.edu/~seander/bithacks.html#IntegerMinOrMax
  nLen[2] = nLen[1] ^ ((nLen[0] ^ nLen[1]) & -(nLen[0] < nLen[1]));
  res = StrNCompareW(szSrcW1, szSrcW2, nLen[2], bCaseInsensitive);
  if (res == 0)
  {
    if (nLen[0] < nLen[1])
      res = -1;
    else if (nLen[0] > nLen[1])
      res = 1;
  }
  return res;
}

int StrCompareAW(__in_z LPCSTR szSrcA1, __in_z LPCWSTR szSrcW2, __in_opt BOOL bCaseInsensitive)
{
  SIZE_T nLen[3];
  int res;

  nLen[0] = StrLenA(szSrcA1);
  nLen[1] = StrLenW(szSrcW2);
  //https://graphics.stanford.edu/~seander/bithacks.html#IntegerMinOrMax
  nLen[2] = nLen[1] ^ ((nLen[0] ^ nLen[1]) & -(nLen[0] < nLen[1]));
  res = StrNCompareAW(szSrcA1, szSrcW2, nLen[2], bCaseInsensitive);
  if (res == 0)
  {
    if (nLen[0] < nLen[1])
      res = -1;
    else if (nLen[0] > nLen[1])
      res = 1;
  }
  return res;
}

int StrNCompareA(__in_z LPCSTR szSrcA1, __in_z LPCSTR szSrcA2, __in SIZE_T nLen, __in_opt BOOL bCaseInsensitive)
{
  MX_ANSI_STRING asStr[2];
  LONG res;

  if (nLen == 0 || (szSrcA1 == NULL && szSrcA2 == NULL))
    return 0;
  if (szSrcA1 == NULL)
    return -1;
  if (szSrcA2 == NULL)
    return 1;
  asStr[0].Buffer = (PSTR)szSrcA1;
  asStr[1].Buffer = (PSTR)szSrcA2;
  res = 0;
  while (res == 0 && nLen > 0)
  {
    asStr[0].Length = asStr[0].MaximumLength =
      asStr[1].Length = asStr[1].MaximumLength = (nLen > 16384) ? 16384 : (USHORT)nLen;
    res = ::MxRtlCompareString(&asStr[0], &asStr[1], bCaseInsensitive);
    if (res == 0)
    {
      nLen -= (SIZE_T)(asStr[0].Length);
      asStr[0].Buffer += (SIZE_T)(asStr[0].Length);
      asStr[1].Buffer += (SIZE_T)(asStr[0].Length);
    }
  }
  return res;
}

int StrNCompareW(__in_z LPCWSTR szSrcW1, __in_z LPCWSTR szSrcW2, __in SIZE_T nLen, __in_opt BOOL bCaseInsensitive)
{
  MX_UNICODE_STRING usStr[2];

  LONG res;

  if (nLen == 0 || (szSrcW1 == NULL && szSrcW2 == NULL))
    return 0;
  if (szSrcW1 == NULL)
    return -1;
  if (szSrcW2 == NULL)
    return 1;
  usStr[0].Buffer = (PWSTR)szSrcW1;
  usStr[1].Buffer = (PWSTR)szSrcW2;
  res = 0;
  while (res == 0 && nLen > 0)
  {
    usStr[0].Length = usStr[0].MaximumLength =
      usStr[1].Length = usStr[1].MaximumLength = (nLen > 16384) ? 32768 : (USHORT)(nLen * 2);
    res = ::MxRtlCompareUnicodeString(&usStr[0], &usStr[1], bCaseInsensitive);
    if (res == 0)
    {
      usStr[0].Length >>= 1;
      nLen -= (SIZE_T)(usStr[0].Length);
      usStr[0].Buffer += (SIZE_T)(usStr[0].Length);
      usStr[1].Buffer += (SIZE_T)(usStr[0].Length);
    }
  }
  return res;
}

int StrNCompareAW(__in_z LPCSTR szSrcA1, __in_z LPCWSTR szSrcW2, __in SIZE_T nLen,
                  __in_opt BOOL bCaseInsensitive)
{
  MX_UNICODE_STRING usStr[2];
  WCHAR szTempBufW[32];
  USHORT i;
  LONG res;

  if (nLen == 0 || (szSrcA1 == NULL && szSrcW2 == NULL))
    return 0;
  if (szSrcA1 == NULL)
    return -1;
  if (szSrcW2 == NULL)
    return 1;
  InitializeTables();
  usStr[0].Buffer = (PWSTR)szTempBufW;
  usStr[1].Buffer = (PWSTR)szSrcW2;
  res = 0;
  while (res == 0 && nLen > 0)
  {
    usStr[0].Length = (USHORT)((nLen > MX_ARRAYLEN(szTempBufW)) ? MX_ARRAYLEN(szTempBufW) : nLen);
    for (i=0; i<usStr[0].Length; i++)
      szTempBufW[i] = aToUnicodeChar[szSrcA1[i]];
    usStr[0].Length <<= 1;
    usStr[1].Length = usStr[1].MaximumLength =  usStr[0].MaximumLength = usStr[0].Length;
    res = ::MxRtlCompareUnicodeString(&usStr[0], &usStr[1], bCaseInsensitive);
    if (res == 0)
    {
      usStr[0].Length >>= 1;
      nLen -= (SIZE_T)(usStr[0].Length);
      szSrcA1 += (SIZE_T)(usStr[0].Length);
      usStr[1].Buffer += (SIZE_T)(usStr[0].Length);
    }
  }
  return res;
}

LPCSTR StrChrA(__in_z LPCSTR szSrcA, __in CHAR chA, __in_opt BOOL bReverse)
{
  return StrNChrA(szSrcA, chA, StrLenA(szSrcA), bReverse);
}

LPCWSTR StrChrW(__in_z LPCWSTR szSrcW, __in WCHAR chW, __in_opt BOOL bReverse)
{
  return StrNChrW(szSrcW, chW, StrLenW(szSrcW), bReverse);
}

LPCSTR StrNChrA(__in_z LPCSTR szSrcA, __in CHAR chA, __in SIZE_T nLen, __in_opt BOOL bReverse)
{
  SSIZE_T nAdv;

  nAdv = (bReverse == FALSE) ? 1 : (-1);
  if (bReverse != FALSE)
    szSrcA += (nLen-1);
  while (nLen > 0 && *szSrcA != 0)
  {
    if (chA == *szSrcA)
      return szSrcA;
    szSrcA += nAdv;
    nLen--;
  }
  return NULL;
}

LPCWSTR StrNChrW(__in_z LPCWSTR szSrcW, __in WCHAR chW, __in SIZE_T nLen, __in_opt BOOL bReverse)
{
  SSIZE_T nAdv;

  nAdv = (bReverse == FALSE) ? 1 : (-1);
  if (bReverse != FALSE)
    szSrcW += (nLen-1);
  while (nLen > 0 && *szSrcW != 0)
  {
    if (chW == *szSrcW)
      return szSrcW;
    szSrcW += nAdv;
    nLen--;
  }
  return NULL;
}

LPCSTR StrFindA(__in_z LPCSTR szSrcA, __in_z LPCSTR szToFindA, __in_opt BOOL bReverse, __in_opt BOOL bCaseInsensitive)
{
  return StrNFindA(szSrcA, szToFindA, StrLenA(szSrcA), bReverse, bCaseInsensitive);
}

LPCWSTR StrFindW(__in_z LPCWSTR szSrcW, __in_z LPCWSTR szToFindW, __in_opt BOOL bReverse,
                 __in_opt BOOL bCaseInsensitive)
{
  return StrNFindW(szSrcW, szToFindW, StrLenW(szSrcW), bReverse, bCaseInsensitive);
}

LPCSTR StrNFindA(__in_z LPCSTR szSrcA, __in_z LPCSTR szToFindA, __in SIZE_T nLen, __in_opt BOOL bReverse,
                 __in_opt BOOL bCaseInsensitive)
{
  SSIZE_T nAdv;
  SIZE_T nToFindLen;

  nAdv = (bReverse == FALSE) ? 1 : (-1);
  nToFindLen = StrLenA(szToFindA);
  if (nToFindLen == 0 || nToFindLen > nLen)
    return NULL;
  nLen -= (nToFindLen-1);
  if (bReverse != FALSE)
    szSrcA += (nLen-1);
  while (nLen > 0 && *szSrcA != 0)
  {
    if (StrNCompareA(szSrcA, szToFindA, nToFindLen, bCaseInsensitive) == 0)
      return szSrcA;
    szSrcA += nAdv;
    nLen--;
  }
  return NULL;
}

LPCWSTR StrNFindW(__in_z LPCWSTR szSrcW, __in_z LPCWSTR szToFindW, __in SIZE_T nLen, __in_opt BOOL bReverse,
                  __in_opt BOOL bCaseInsensitive)
{
  SSIZE_T nAdv;
  SIZE_T nToFindLen;

  nAdv = (bReverse == FALSE) ? 1 : (-1);
  nToFindLen = StrLenW(szToFindW);
  if (nToFindLen == 0 || nToFindLen > nLen)
    return NULL;
  nLen -= (nToFindLen-1);
  if (bReverse != FALSE)
    szSrcW += (nLen-1);
  while (nLen > 0 && *szSrcW != 0)
  {
    if (StrNCompareW(szSrcW, szToFindW, nToFindLen, bCaseInsensitive) == 0)
      return szSrcW;
    szSrcW += nAdv;
    nLen--;
  }
  return NULL;
}

CHAR CharToLowerA(__in CHAR chA)
{
  return aToLowerChar[chA];
}

CHAR CharToUpperA(__in CHAR chA)
{
  return ::MxRtlUpperChar(chA);
}

WCHAR CharToLowerW(__in WCHAR chW)
{
  return ::MxRtlDowncaseUnicodeChar(chW);
}

WCHAR CharToUpperW(__in WCHAR chW)
{
  return ::MxRtlUpcaseUnicodeChar(chW);
}

VOID StrToLowerA(__inout_z LPSTR szSrcA)
{
  StrNToLowerA(szSrcA, StrLenA(szSrcA));
  return;
}

VOID StrNToLowerA(__inout_z LPSTR szSrcA, __in SIZE_T nLen)
{
  if (szSrcA != NULL)
  {
    InitializeTables();
    while (nLen > 0)
    {
      *szSrcA = aToLowerChar[*szSrcA];
      szSrcA++;
      nLen--;
    }
  }
  return;
}

VOID StrToLowerW(__inout_z LPWSTR szSrcW)
{
  StrNToLowerW(szSrcW, StrLenW(szSrcW));
  return;
}

VOID StrNToLowerW(__inout_z LPWSTR szSrcW, __in SIZE_T nLen)
{
  if (szSrcW != NULL)
  {
    while (nLen > 0)
    {
      *szSrcW = ::MxRtlDowncaseUnicodeChar(*szSrcW);
      szSrcW++;
      nLen--;
    }
  }
  return;
}

VOID StrToUpperA(__inout_z LPSTR szSrcA)
{
  StrNToUpperA(szSrcA, StrLenA(szSrcA));
  return;
}

VOID StrNToUpperA(__inout_z LPSTR szSrcA, __in SIZE_T nLen)
{
  if (szSrcA != NULL)
  {
    while (nLen > 0)
    {
      *szSrcA = ::MxRtlUpperChar(*szSrcA);
      szSrcA++;
      nLen--;
    }
  }
  return;
}

VOID StrToUpperW(__inout_z LPWSTR szSrcW)
{
  StrNToUpperW(szSrcW, StrLenW(szSrcW));
  return;
}

VOID StrNToUpperW(__inout_z LPWSTR szSrcW, __in SIZE_T nLen)
{
  if (szSrcW != NULL)
  {
    while (nLen > 0)
    {
      *szSrcW = ::MxRtlUpcaseUnicodeChar(*szSrcW);
      szSrcW++;
      nLen--;
    }
  }
  return;
}

//-----------------------------------------------------------

CStringA::CStringA() : CBaseMemObj()
{
  szStrA = NULL;
  nSize = nLen = 0;
  return;
}

CStringA::~CStringA()
{
  Empty();
  return;
}

VOID CStringA::Empty()
{
  MX_FREE(szStrA);
  szStrA = NULL;
  nSize = nLen = 0;
  return;
}

VOID CStringA::Refresh()
{
  SIZE_T _nLen;

  _nLen = 0;
  if (szStrA != NULL)
  {
    szStrA[nSize-1] = 0;
    while (szStrA[_nLen] != 0)
      _nLen++;
  }
  nLen = _nLen;
  return;
}

SIZE_T CStringA::GetLength() const
{
  return nLen;
}

BOOL CStringA::Copy(__in_z_opt LPCSTR szSrcA)
{
  Empty();
  return Concat(szSrcA);
}

BOOL CStringA::CopyN(__in_nz_opt LPCSTR szSrcA, __in SIZE_T nSrcLen)
{
  Empty();
  return ConcatN(szSrcA, nSrcLen);
}

BOOL CStringA::Concat(__in_z_opt LPCSTR szSrcA)
{
  SIZE_T nSrcLen;

  nSrcLen = 0;
  if (szSrcA != NULL)
  {
    while (szSrcA[nSrcLen] != 0)
      nSrcLen++;
  }
  return ConcatN(szSrcA, nSrcLen);
}

BOOL CStringA::ConcatN(__in_nz_opt LPCSTR szSrcA, __in SIZE_T nSrcLen)
{
  if (nSrcLen == 0)
    return TRUE;
  if (szSrcA == NULL)
    return FALSE;
  if (nLen+nSrcLen+1 < nLen)
    return FALSE; //overflow
  if (EnsureBuffer(nLen+nSrcLen+1) == FALSE)
    return FALSE;
  MemCopy(szStrA+nLen, szSrcA, nSrcLen);
  nLen += nSrcLen;
  szStrA[nLen] = 0;
  return TRUE;
}

BOOL CStringA::Copy(__in_z_opt LPCWSTR szSrcW)
{
  Empty();
  return Concat(szSrcW);
}

BOOL CStringA::CopyN(__in_nz_opt LPCWSTR szSrcW, __in SIZE_T nSrcLen)
{
  Empty();
  return ConcatN(szSrcW, nSrcLen);
}

BOOL CStringA::Concat(__in_z_opt LPCWSTR szSrcW)
{
  SIZE_T nSrcLen;

  nSrcLen = 0;
  if (szSrcW != NULL)
  {
    while (szSrcW[nSrcLen] != 0)
      nSrcLen++;
  }
  return ConcatN(szSrcW, nSrcLen);
}

BOOL CStringA::ConcatN(__in_nz_opt LPCWSTR szSrcW, __in SIZE_T nSrcLen)
{
  MX_ANSI_STRING asTemp;
  MX_UNICODE_STRING usStr;
  SIZE_T k, nAnsiLen, nThisRoundLen;

  if (nSrcLen == 0)
    return TRUE;
  if (szSrcW == NULL)
    return FALSE;
  nAnsiLen = 0;
  usStr.Buffer = (PWSTR)szSrcW;
  for (k=nSrcLen; k>0; k-=nThisRoundLen)
  {
    nThisRoundLen = (k > 16384) ? 16384 : k;
    if (nThisRoundLen > 1 && usStr.Buffer[nThisRoundLen-1] >= 0xD800 && usStr.Buffer[nThisRoundLen-1] <= 0xDBFF)
      nThisRoundLen--; //don't split surrogate pairs
    usStr.Length = usStr.MaximumLength = (USHORT)nThisRoundLen * sizeof(WCHAR);
    nAnsiLen += (SIZE_T)::MxRtlUnicodeStringToAnsiSize(&usStr) - 1; //remove NUL char terminator
    usStr.Buffer += nThisRoundLen;
  }
  if (nLen+nAnsiLen+1 < nLen)
    return FALSE; //overflow
  if (EnsureBuffer(nLen+nAnsiLen+1) == FALSE)
    return FALSE;
  usStr.Buffer = (PWSTR)szSrcW;
  for (k=nSrcLen; k>0; k-=nThisRoundLen)
  {
    nThisRoundLen = (k > 16384) ? 16384 : k;
    if (nThisRoundLen > 1 && usStr.Buffer[nThisRoundLen-1] >= 0xD800 && usStr.Buffer[nThisRoundLen-1] <= 0xDBFF)
      nThisRoundLen--; //don't split surrogate pairs
    usStr.Length = (USHORT)(nThisRoundLen * sizeof(WCHAR));
    asTemp.Buffer = (PSTR)(szStrA+nLen);
    asTemp.MaximumLength = 32768;
    ::MxRtlUnicodeStringToAnsiString(&asTemp, &usStr, FALSE);
    usStr.Buffer += nThisRoundLen;
    nLen += (SIZE_T)(asTemp.Length);
  }
  szStrA[nLen] = 0;
  return TRUE;
}

BOOL CStringA::Copy(__in PCMX_ANSI_STRING pASStr)
{
  Empty();
  return Concat(pASStr);
}

BOOL CStringA::Concat(__in PCMX_ANSI_STRING pASStr)
{
  if (pASStr != NULL && pASStr->Buffer != NULL && pASStr->Length != 0)
    return ConcatN(pASStr->Buffer, (SIZE_T)(pASStr->Length));
  return TRUE;
}

BOOL CStringA::Copy(__in LONG nSrc)
{
  Empty();
  return Concat(nSrc);
}

BOOL CStringA::Concat(__in LONG nSrc)
{
  return Concat((LONGLONG)nSrc);
}

BOOL CStringA::Copy(__in ULONG nSrc)
{
  Empty();
  return Concat(nSrc);
}

BOOL CStringA::Concat(__in ULONG nSrc)
{
  return Concat((ULONGLONG)nSrc);
}

BOOL CStringA::Copy(__in LONGLONG nSrc)
{
  Empty();
  return Concat(nSrc);
}

BOOL CStringA::Concat(__in LONGLONG nSrc)
{
  CHAR szTempA[128];

  mx_sprintf_s(szTempA, MX_ARRAYLEN(szTempA), "%I64d", nSrc);
  return Concat(szTempA);
}

BOOL CStringA::Copy(__in ULONGLONG nSrc)
{
  Empty();
  return Concat(nSrc);
}

BOOL CStringA::Concat(__in ULONGLONG nSrc)
{
  CHAR szTempA[128];

  mx_sprintf_s(szTempA, MX_ARRAYLEN(szTempA), "%I64u", nSrc);
  return Concat(szTempA);
}

BOOL CStringA::Format(__in_z LPCSTR szFormatA, ...)
{
  va_list argptr;
  BOOL b;

  Empty();
  if (szFormatA == NULL)
    return FALSE;
  va_start(argptr, szFormatA);
  b = FormatV(szFormatA, argptr);
  va_end(argptr);
  return b;
}

BOOL CStringA::Format(__in_z LPCWSTR szFormatW, ...)
{
  va_list argptr;
  BOOL b;

  Empty();
  if (szFormatW == NULL)
    return FALSE;
  va_start(argptr, szFormatW);
  b = FormatV(szFormatW, argptr);
  va_end(argptr);
  return b;
}

BOOL CStringA::FormatV(__in_z LPCSTR szFormatA, __in va_list argptr)
{
  Empty();
  return AppendFormatV(szFormatA, argptr);
}

BOOL CStringA::FormatV(__in_z LPCWSTR szFormatW, __in va_list argptr)
{
  Empty();
  return AppendFormatV(szFormatW, argptr);
}

BOOL CStringA::AppendFormat(__in_z LPCSTR szFormatA, ...)
{
  va_list argptr;
  BOOL b;

  if (szFormatA == NULL)
    return FALSE;
  va_start(argptr, szFormatA);
  b = AppendFormatV(szFormatA, argptr);
  va_end(argptr);
  return b;
}

BOOL CStringA::AppendFormat(__in_z LPCWSTR szFormatW, ...)
{
  va_list argptr;
  BOOL b;

  if (szFormatW == NULL)
    return FALSE;
  va_start(argptr, szFormatW);
  b = AppendFormatV(szFormatW, argptr);
  va_end(argptr);
  return b;
}

BOOL CStringA::AppendFormatV(__in_z LPCSTR szFormatA, __in va_list argptr)
{
  int nChars;
  SIZE_T nBufSize;

  if (szFormatA == NULL)
    return FALSE;
  nBufSize = 512;
  while (1)
  {
    if (EnsureBuffer(nLen+nBufSize+1) == FALSE)
      return FALSE;
    nChars = mx_vsnprintf(szStrA+nLen, (size_t)nBufSize, szFormatA, argptr);
    if ((SIZE_T)nChars < nBufSize-2)
      break;
    nBufSize += 4096;
  }
  nLen += (SIZE_T)nChars;
  szStrA[nLen] = 0;
  return TRUE;
}

BOOL CStringA::AppendFormatV(__in_z LPCWSTR szFormatW, __in va_list argptr)
{
  WCHAR szTempBufW[512], *szTempW;
  int nChars, nBufSize;
  BOOL b;

  if (szFormatW == NULL)
    return FALSE;
  nChars = mx_vsnwprintf(szTempBufW, 512, szFormatW, argptr);
  if (nChars > 0)
  {
    if (nChars < 510)
      return ConcatN(szTempBufW, (size_t)nChars);
    nBufSize = nChars*2;
    while (1)
    {
      szTempW = (LPWSTR)MX_MALLOC((SIZE_T)(nBufSize+1) * sizeof(WCHAR));
      if (szTempW == NULL)
        return FALSE;
      nChars = mx_vsnwprintf(szTempW, (SIZE_T)nBufSize, szFormatW, argptr);
      if (nChars < nBufSize-2)
        break;
      nBufSize += 4096;
      MX_FREE(szTempW);
    }
    b = ConcatN(szTempW, (SIZE_T)nChars);
    MX_FREE(szTempW);
    if (b == FALSE)
      return FALSE;
  }
  return TRUE;
}

BOOL CStringA::Insert(__in_z_opt LPCSTR szSrcA, __in SIZE_T nInsertPosition)
{
  SIZE_T nSrcLen;

  nSrcLen = 0;
  if (szSrcA != NULL)
  {
    while (szSrcA[nSrcLen] != 0)
      nSrcLen++;
  }
  return InsertN(szSrcA, nInsertPosition, nSrcLen);
}

BOOL CStringA::InsertN(__in_nz_opt LPCSTR szSrcA, __in SIZE_T nInsertPosition, __in SIZE_T nSrcLen)
{
  LPSTR sA;

  if (nSrcLen == 0)
    return TRUE;
  if (szSrcA == NULL)
    return FALSE;
  if (nLen+nSrcLen+1 < nLen)
    return FALSE; //overflow
  if (EnsureBuffer(nLen+nSrcLen+1) == FALSE)
    return FALSE;
  if (nInsertPosition > nLen)
    nInsertPosition = nLen;
  sA = szStrA + nInsertPosition;
  MemMove(sA+nSrcLen, sA, (nLen-nInsertPosition));
  MemCopy(sA, szSrcA, nSrcLen);
  nLen += nSrcLen;
  szStrA[nLen] = 0;
  return TRUE;
}

VOID CStringA::Delete(__in SIZE_T nStartChar, __in SIZE_T nChars)
{
  LPSTR sA;
  SIZE_T k;

  if (nLen > 0)
  {
    if (nStartChar > nLen)
      nStartChar = nLen;
    if (nStartChar+nChars > nLen || nStartChar+nChars < nStartChar) //overflow
      nChars = nLen-nStartChar;
    k = nLen - nChars - nStartChar;
    sA = szStrA + nStartChar;
    nLen -= nChars;
    MemMove(sA, sA+nChars, k);
    szStrA[nLen] = 0;
  }
  return;
}


BOOL CStringA::StartsWith(__in_z LPCSTR _szStrA, __in_opt BOOL bCaseInsensitive)
{
  SIZE_T nSrcLen;

  nSrcLen = StrLenA(_szStrA);
  if (nSrcLen == 0 || nLen < nSrcLen)
    return FALSE;
  return (StrNCompareA(szStrA, _szStrA, nSrcLen, bCaseInsensitive) == 0) ? TRUE : FALSE;
}

BOOL CStringA::EndsWith(__in_z LPCSTR _szStrA, __in_opt BOOL bCaseInsensitive)
{
  SIZE_T nSrcLen;

  nSrcLen = StrLenA(_szStrA);
  if (nSrcLen == 0 || nLen < nSrcLen)
    return FALSE;
  return (StrNCompareA(szStrA+(nLen-nSrcLen), _szStrA, nSrcLen, bCaseInsensitive) == 0) ? TRUE : FALSE;
}

LPCSTR CStringA::Contains(__in_z LPCSTR _szStrA, __in_opt BOOL bCaseInsensitive)
{
  if (_szStrA == NULL || *_szStrA == 0)
    return NULL;
  return MX::StrFindA(szStrA, _szStrA, FALSE, bCaseInsensitive);
}

VOID CStringA::Attach(__in_z LPSTR szSrcA)
{
  Empty();
  szStrA = szSrcA;
  if (szSrcA != NULL)
  {
    nLen = 0;
    while (szStrA[nLen] != 0)
      nLen++;
    nSize = nLen + 1;
  }
  return;
}

LPSTR CStringA::Detach()
{
  LPSTR szRetA;

  szRetA = szStrA;
  szStrA = NULL;
  nSize = nLen = 0;
  return szRetA;
}

BOOL CStringA::EnsureBuffer(__in SIZE_T nChars)
{
  LPSTR szNewStrA;

  nChars++;
  if (nChars >= nSize)
  {
    if (nSize > 0)
      nChars = X_ALIGN(nChars, 256);
    szNewStrA = (LPSTR)MX_MALLOC(nChars);
    if (szNewStrA == NULL)
      return FALSE;
    MemCopy(szNewStrA, szStrA, nLen);
    szNewStrA[nLen] = 0;
    MX_FREE(szStrA);
    szStrA = szNewStrA;
    nSize = nChars;
  }
  return TRUE;
}

CHAR& CStringA::operator[](__in SIZE_T nIndex)
{
  return szStrA[nIndex];
}

LPWSTR CStringA::ToWide()
{
  return Ansi2Wide(szStrA, nLen);
}

LPWSTR CStringA::Ansi2Wide(__in_z LPCSTR szStrA, __in SIZE_T nSrcLen)
{
  CStringW cStrTempW;

  if (nSrcLen == 0)
    szStrA = "";
  else if (szStrA == NULL)
    return NULL;
  if (cStrTempW.CopyN(szStrA, nSrcLen) == FALSE)
    return NULL;
  return cStrTempW.Detach();
}

//-----------------------------------------------------------

CStringW::CStringW() : CBaseMemObj()
{
  szStrW = NULL;
  nSize = nLen = 0;
  return;
}

CStringW::~CStringW()
{
  Empty();
  return;
}

VOID CStringW::Empty()
{
  MX_FREE(szStrW);
  szStrW = NULL;
  nSize = nLen = 0;
  return;
}

VOID CStringW::Refresh()
{
  SIZE_T _nLen;

  _nLen = 0;
  if (szStrW != NULL)
  {
    szStrW[nSize-1] = 0;
    while (szStrW[_nLen] != 0)
      _nLen++;
  }
  nLen = _nLen;
  return;
}

SIZE_T CStringW::GetLength() const
{
  return nLen;
}

BOOL CStringW::Copy(__in_z_opt LPCSTR szSrcA)
{
  Empty();
  return Concat(szSrcA);
}

BOOL CStringW::CopyN(__in_nz_opt LPCSTR szSrcA, __in SIZE_T nSrcLen)
{
  Empty();
  return ConcatN(szSrcA, nSrcLen);
}

BOOL CStringW::Concat(__in_z_opt LPCSTR szSrcA)
{
  SIZE_T nSrcLen;

  nSrcLen = 0;
  if (szSrcA != NULL)
  {
    while (szSrcA[nSrcLen] != 0)
      nSrcLen++;
  }
  return ConcatN(szSrcA, nSrcLen);
}

BOOL CStringW::ConcatN(__in_nz_opt LPCSTR szSrcA, __in SIZE_T nSrcLen)
{
  MX_ANSI_STRING asStr;
  MX_UNICODE_STRING usTemp;
  SIZE_T k, nWideLen;

  if (nSrcLen == 0)
    return TRUE;
  if (szSrcA == NULL)
    return FALSE;
  nWideLen = 0;
  asStr.Buffer = (PSTR)szSrcA;
  for (k=nSrcLen; k>0; k-=(SIZE_T)(asStr.Length))
  {
    asStr.Length = asStr.MaximumLength = (USHORT)((k > 16384) ? 16384 : k);
    nWideLen += ((SIZE_T)::MxRtlAnsiStringToUnicodeSize(&asStr) / sizeof(WCHAR)) - 1; //remove NUL char terminator
    asStr.Buffer += (SIZE_T)(asStr.Length);
  }
  if (nLen+nWideLen+1 < nLen)
    return FALSE; //overflow
  if (EnsureBuffer(nLen+nWideLen+1) == FALSE)
    return FALSE;
  asStr.Buffer = (PSTR)szSrcA;
  for (k=nSrcLen; k>0; k-=(SIZE_T)(asStr.Length))
  {
    asStr.Length = (USHORT)((k > 16384) ? 16384 : k); 
    usTemp.Buffer = (PWSTR)(szStrW+nLen);
    usTemp.MaximumLength = 32768;
    ::MxRtlAnsiStringToUnicodeString(&usTemp, &asStr, FALSE);
    asStr.Buffer += (SIZE_T)(asStr.Length);
    nLen += (SIZE_T)(usTemp.Length) / sizeof(WCHAR);
  }
  szStrW[nLen] = 0;
  return TRUE;
}

BOOL CStringW::Copy(__in_z_opt LPCWSTR szSrcW)
{
  Empty();
  return Concat(szSrcW);
}

BOOL CStringW::CopyN(__in_nz_opt LPCWSTR szSrcW, __in SIZE_T nSrcLen)
{
  Empty();
  return ConcatN(szSrcW, nSrcLen);
}

BOOL CStringW::Concat(__in_z_opt LPCWSTR szSrcW)
{
  SIZE_T nSrcLen;

  nSrcLen = 0;
  if (szSrcW != NULL)
  {
    while (szSrcW[nSrcLen] != 0)
      nSrcLen++;
  }
  return ConcatN(szSrcW, nSrcLen);
}

BOOL CStringW::ConcatN(__in_nz_opt LPCWSTR szSrcW, __in SIZE_T nSrcLen)
{
  if (nSrcLen == 0)
    return TRUE;
  if (szSrcW == NULL)
    return FALSE;
  if (nLen+nSrcLen+1 < nLen)
    return FALSE; //overflow
  if (EnsureBuffer(nLen+nSrcLen+1) == FALSE)
    return FALSE;
  MemCopy(szStrW+nLen, szSrcW, nSrcLen*sizeof(WCHAR));
  nLen += nSrcLen;
  szStrW[nLen] = 0;
  return TRUE;
}

BOOL CStringW::Copy(__in PCMX_UNICODE_STRING pUSStr)
{
  Empty();
  return Concat(pUSStr);
}

BOOL CStringW::Concat(__in PCMX_UNICODE_STRING pUSStr)
{
  if (pUSStr != NULL && pUSStr->Buffer != NULL && pUSStr->Length != 0)
    return ConcatN(pUSStr->Buffer, (SIZE_T)(pUSStr->Length)/sizeof(WCHAR));
  return TRUE;
}

BOOL CStringW::Copy(__in LONG nSrc)
{
  Empty();
  return Concat(nSrc);
}

BOOL CStringW::Concat(__in LONG nSrc)
{
  return Concat((LONGLONG)nSrc);
}

BOOL CStringW::Copy(__in ULONG nSrc)
{
  Empty();
  return Concat(nSrc);
}

BOOL CStringW::Concat(__in ULONG nSrc)
{
  return Concat((ULONGLONG)nSrc);
}

BOOL CStringW::Copy(__in LONGLONG nSrc)
{
  Empty();
  return Concat(nSrc);
}

BOOL CStringW::Concat(__in LONGLONG nSrc)
{
  WCHAR szTempW[128];

  mx_swprintf_s(szTempW, MX_ARRAYLEN(szTempW), L"%I64d", nSrc);
  return Concat(szTempW);
}

BOOL CStringW::Copy(__in ULONGLONG nSrc)
{
  Empty();
  return Concat(nSrc);
}

BOOL CStringW::Concat(__in ULONGLONG nSrc)
{
  WCHAR szTempW[128];

  mx_swprintf_s(szTempW, MX_ARRAYLEN(szTempW), L"%I64u", nSrc);
  return Concat(szTempW);
}

BOOL CStringW::Format(__in_z LPCSTR szFormatA, ...)
{
  va_list argptr;
  BOOL b;

  Empty();
  if (szFormatA == NULL)
    return FALSE;
  va_start(argptr, szFormatA);
  b = FormatV(szFormatA, argptr);
  va_end(argptr);
  return b;
}

BOOL CStringW::Format(__in_z LPCWSTR szFormatW, ...)
{
  va_list argptr;
  BOOL b;

  Empty();
  if (szFormatW == NULL)
    return FALSE;
  va_start(argptr, szFormatW);
  b = FormatV(szFormatW, argptr);
  va_end(argptr);
  return b;
}

BOOL CStringW::FormatV(__in_z LPCSTR szFormatA, __in va_list argptr)
{
  Empty();
  return AppendFormatV(szFormatA, argptr);
}

BOOL CStringW::FormatV(__in_z LPCWSTR szFormatW, __in va_list argptr)
{
  Empty();
  return AppendFormatV(szFormatW, argptr);
}

BOOL CStringW::AppendFormat(__in_z LPCSTR szFormatA, ...)
{
  va_list argptr;
  BOOL b;

  if (szFormatA == NULL)
    return FALSE;
  va_start(argptr, szFormatA);
  b = AppendFormatV(szFormatA, argptr);
  va_end(argptr);
  return b;
}

BOOL CStringW::AppendFormat(__in_z LPCWSTR szFormatW, ...)
{
  va_list argptr;
  BOOL b;

  if (szFormatW == NULL)
    return FALSE;
  va_start(argptr, szFormatW);
  b = AppendFormatV(szFormatW, argptr);
  va_end(argptr);
  return b;
}

BOOL CStringW::AppendFormatV(__in_z LPCSTR szFormatA, __in va_list argptr)
{
  CHAR szTempBufA[512], *szTempA;
  int nChars, nBufSize;
  BOOL b;

  if (szFormatA == NULL)
    return FALSE;
  nChars = mx_vsnprintf(szTempBufA, 512, szFormatA, argptr);
  if (nChars > 0)
  {
    if (nChars < 510)
      return ConcatN(szTempBufA, (size_t)nChars);
    nBufSize = nChars*2;
    while (1)
    {
      szTempA = (LPSTR)MX_MALLOC((SIZE_T)(nBufSize+1) * sizeof(CHAR));
      if (szTempA == NULL)
        return FALSE;
      nChars = mx_vsnprintf(szTempA, (size_t)nBufSize, szFormatA, argptr);
      if (nChars < nBufSize-2)
        break;
      nBufSize += 4096;
      MX_FREE(szTempA);
    }
    b = ConcatN(szTempA, (size_t)nChars);
    MX_FREE(szTempA);
    if (b == FALSE)
      return FALSE;
  }
  return TRUE;
}

BOOL CStringW::AppendFormatV(__in_z LPCWSTR szFormatW, __in va_list argptr)
{
  int nChars;
  SIZE_T nBufSize;

  if (szFormatW == NULL)
    return FALSE;
  nBufSize = 512;
  while (1)
  {
    if (EnsureBuffer(nLen+nBufSize+1) == FALSE)
      return FALSE;
    nChars = mx_vsnwprintf(szStrW+nLen, (SIZE_T)nBufSize, szFormatW, argptr);
    if ((SIZE_T)nChars < nBufSize-2)
      break;
    nBufSize += 4096;
  }
  nLen += (SIZE_T)nChars;
  szStrW[nLen] = 0;
  return TRUE;
}

BOOL CStringW::Insert(__in_z_opt LPCWSTR szSrcW, __in SIZE_T nInsertPosition)
{
  SIZE_T nSrcLen;

  nSrcLen = 0;
  if (szSrcW != NULL)
  {
    while (szSrcW[nSrcLen] != 0)
      nSrcLen++;
  }
  return InsertN(szSrcW, nInsertPosition, nSrcLen);
}

BOOL CStringW::InsertN(__in_nz_opt LPCWSTR szSrcW, __in SIZE_T nInsertPosition, __in SIZE_T nSrcLen)
{
  LPWSTR sW;

  if (nSrcLen == 0)
    return TRUE;
  if (szSrcW == NULL)
    return FALSE;
  if (nLen+nSrcLen+1 < nLen)
    return FALSE; //overflow
  if (EnsureBuffer(nLen+nSrcLen+1) == FALSE)
    return FALSE;
  if (nInsertPosition > nLen)
    nInsertPosition = nLen;
  sW = szStrW + nInsertPosition;
  MemMove(sW+nSrcLen, sW, (nLen-nInsertPosition)*sizeof(WCHAR));
  MemCopy(sW, szSrcW, nSrcLen*sizeof(WCHAR));
  nLen += nSrcLen;
  szStrW[nLen] = 0;
  return TRUE;
}

VOID CStringW::Delete(__in SIZE_T nStartChar, __in SIZE_T nChars)
{
  LPWSTR sW;
  SIZE_T k;

  if (nLen > 0)
  {
    if (nStartChar > nLen)
      nStartChar = nLen;
    if (nStartChar+nChars > nLen || nStartChar+nChars < nStartChar) //overflow
      nChars = nLen-nStartChar;
    k = nLen - nChars - nStartChar;
    sW = szStrW + nStartChar;
    nLen -= nChars;
    MemMove(sW, sW+nChars, k*sizeof(WCHAR));
    szStrW[nLen] = 0;
  }
  return;
}

BOOL CStringW::StartsWith(__in_z LPCWSTR _szStrW, __in_opt BOOL bCaseInsensitive)
{
  SIZE_T nSrcLen;

  nSrcLen = StrLenW(_szStrW);
  if (nSrcLen == 0 || nLen < nSrcLen)
    return FALSE;
  return (StrNCompareW(szStrW, _szStrW, nSrcLen, bCaseInsensitive) == 0) ? TRUE : FALSE;
}

BOOL CStringW::EndsWith(__in_z LPCWSTR _szStrW, __in_opt BOOL bCaseInsensitive)
{
  SIZE_T nSrcLen;

  nSrcLen = StrLenW(_szStrW);
  if (nSrcLen == 0 || nLen < nSrcLen)
    return FALSE;
  return (StrNCompareW(szStrW+(nLen-nSrcLen), _szStrW, nSrcLen, bCaseInsensitive) == 0) ? TRUE : FALSE;
}

LPCWSTR CStringW::Contains(__in_z LPCWSTR _szStrW, __in_opt BOOL bCaseInsensitive)
{
  if (_szStrW == NULL || *_szStrW == 0)
    return NULL;
  return MX::StrFindW(szStrW, _szStrW, FALSE, bCaseInsensitive);
}

VOID CStringW::Attach(__in_z LPWSTR szSrcW)
{
  Empty();
  szStrW = szSrcW;
  if (szSrcW != NULL)
  {
    nLen = 0;
    while (szStrW[nLen] != 0)
      nLen++;
    nSize = nLen + 1;
  }
  return;
}

LPWSTR CStringW::Detach()
{
  LPWSTR szRetW;

  szRetW = szStrW;
  szStrW = NULL;
  nSize = nLen = 0;
  return szRetW;
}

BOOL CStringW::EnsureBuffer(__in SIZE_T nChars)
{
  LPWSTR szNewStrW;

  nChars++;
  if (nChars >= nSize)
  {
    if (nSize > 0)
      nChars = X_ALIGN(nChars, 256);
    szNewStrW = (LPWSTR)MX_MALLOC(nChars*sizeof(WCHAR));
    if (szNewStrW == NULL)
      return FALSE;
    MemCopy(szNewStrW, szStrW, nLen*sizeof(WCHAR));
    szNewStrW[nLen] = 0;
    MX_FREE(szStrW);
    szStrW = szNewStrW;
    nSize = nChars;
  }
  return TRUE;
}

WCHAR& CStringW::operator[](__in SIZE_T nIndex)
{
  return szStrW[nIndex];
}

LPSTR CStringW::ToAnsi()
{
  return Wide2Ansi(szStrW, nLen);
}

LPSTR CStringW::Wide2Ansi(__in_z LPCWSTR szStrW, __in SIZE_T nSrcLen)
{
  CStringA cStrTempA;

  if (nSrcLen == 0)
    szStrW = L"";
  else if (szStrW == NULL)
    return NULL;
  if (cStrTempA.CopyN(szStrW, nSrcLen) == FALSE)
    return NULL;
  return cStrTempA.Detach();
}

} //namespace MX

//-----------------------------------------------------------

static VOID InitializeTables()
{
  static LONG volatile nMutex = 0;

  if (nInitialized == 0)
  {
    MX::CFastLock cLock(&nMutex);
    CHAR chA[4];
    WCHAR chW[4];
    SIZE_T i, j;
    MX_ANSI_STRING sAnsiStr;
    MX_UNICODE_STRING sUnicodeStr;

    if (nInitialized == 0)
    {
      for (i=1; i<256; i++)
      {
        chA[0] = (CHAR)i;
        sAnsiStr.Buffer = chA;
        sAnsiStr.Length = sAnsiStr.MaximumLength = 1;
        sUnicodeStr.Buffer = chW;
        sUnicodeStr.Length = 0;
        sUnicodeStr.MaximumLength = (USHORT)sizeof(chW);
        ::MxRtlAnsiStringToUnicodeString(&sUnicodeStr, &sAnsiStr, FALSE);
        aToUnicodeChar[i] = (sUnicodeStr.Length == 2) ? chW[0] : (WCHAR)i;
        for (j=0; j<(SIZE_T)(sUnicodeStr.Length) / sizeof(WCHAR); j++)
          chW[j] = ::MxRtlDowncaseUnicodeChar(chW[j]);
        sAnsiStr.Length = 0;
        sAnsiStr.MaximumLength = (USHORT)sizeof(chA);
        ::MxRtlUnicodeStringToAnsiString(&sAnsiStr, &sUnicodeStr, FALSE);
        aToLowerChar[i] = (sAnsiStr.Length == 1) ? chA[0] : (CHAR)i;
      }
    }
  }
  return;
}
