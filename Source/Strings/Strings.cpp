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
#include "..\..\Include\Strings\Strings.h"
#include "..\..\Include\WaitableObjects.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

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

static __declspec(thread) int strtodbl_error = 0;
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
static void dblconv_invalid_parameter(const wchar_t * expression, const wchar_t * function, const wchar_t * file,
                                      unsigned int line, uintptr_t pReserved);

//-----------------------------------------------------------

namespace MX {

SIZE_T StrLenA(_In_opt_z_ LPCSTR szSrcA)
{
  SIZE_T nLen = 0;

  if (szSrcA != NULL)
  {
    for (; szSrcA[nLen] != 0; nLen++);
  }
  return nLen;
}

SIZE_T StrLenW(_In_opt_z_ LPCWSTR szSrcW)
{
  SIZE_T nLen = 0;

  if (szSrcW != NULL)
  {
    for (; szSrcW[nLen] != 0; nLen++);
  }
  return nLen;
}

int StrCompareA(_In_z_ LPCSTR szSrcA1, _In_z_ LPCSTR szSrcA2, _In_opt_ BOOL bCaseInsensitive)
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

int StrCompareW(_In_z_ LPCWSTR szSrcW1, _In_z_ LPCWSTR szSrcW2, _In_opt_ BOOL bCaseInsensitive)
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

int StrCompareAW(_In_z_ LPCSTR szSrcA1, _In_z_ LPCWSTR szSrcW2, _In_opt_ BOOL bCaseInsensitive)
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

int StrNCompareA(_In_z_ LPCSTR szSrcA1, _In_z_ LPCSTR szSrcA2, _In_ SIZE_T nLen, _In_opt_ BOOL bCaseInsensitive)
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

int StrNCompareW(_In_z_ LPCWSTR szSrcW1, _In_z_ LPCWSTR szSrcW2, _In_ SIZE_T nLen, _In_opt_ BOOL bCaseInsensitive)
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

int StrNCompareAW(_In_z_ LPCSTR szSrcA1, _In_z_ LPCWSTR szSrcW2, _In_ SIZE_T nLen,
                  _In_opt_ BOOL bCaseInsensitive)
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
      szTempBufW[i] = aToUnicodeChar[((UCHAR*)szSrcA1)[i]];
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

LPCSTR StrChrA(_In_z_ LPCSTR szSrcA, _In_ CHAR chA, _In_opt_ BOOL bReverse)
{
  return StrNChrA(szSrcA, chA, StrLenA(szSrcA), bReverse);
}

LPCWSTR StrChrW(_In_z_ LPCWSTR szSrcW, _In_ WCHAR chW, _In_opt_ BOOL bReverse)
{
  return StrNChrW(szSrcW, chW, StrLenW(szSrcW), bReverse);
}

LPCSTR StrNChrA(_In_z_ LPCSTR szSrcA, _In_ CHAR chA, _In_ SIZE_T nLen, _In_opt_ BOOL bReverse)
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

LPCWSTR StrNChrW(_In_z_ LPCWSTR szSrcW, _In_ WCHAR chW, _In_ SIZE_T nLen, _In_opt_ BOOL bReverse)
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

LPCSTR StrFindA(_In_z_ LPCSTR szSrcA, _In_z_ LPCSTR szToFindA, _In_opt_ BOOL bReverse, _In_opt_ BOOL bCaseInsensitive)
{
  return StrNFindA(szSrcA, szToFindA, StrLenA(szSrcA), bReverse, bCaseInsensitive);
}

LPCWSTR StrFindW(_In_z_ LPCWSTR szSrcW, _In_z_ LPCWSTR szToFindW, _In_opt_ BOOL bReverse,
                 _In_opt_ BOOL bCaseInsensitive)
{
  return StrNFindW(szSrcW, szToFindW, StrLenW(szSrcW), bReverse, bCaseInsensitive);
}

LPCSTR StrNFindA(_In_z_ LPCSTR szSrcA, _In_z_ LPCSTR szToFindA, _In_ SIZE_T nLen, _In_opt_ BOOL bReverse,
                 _In_opt_ BOOL bCaseInsensitive)
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

LPCWSTR StrNFindW(_In_z_ LPCWSTR szSrcW, _In_z_ LPCWSTR szToFindW, _In_ SIZE_T nLen, _In_opt_ BOOL bReverse,
                  _In_opt_ BOOL bCaseInsensitive)
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

CHAR CharToLowerA(_In_ CHAR chA)
{
  return aToLowerChar[chA];
}

CHAR CharToUpperA(_In_ CHAR chA)
{
  return ::MxRtlUpperChar(chA);
}

WCHAR CharToLowerW(_In_ WCHAR chW)
{
  return ::MxRtlDowncaseUnicodeChar(chW);
}

WCHAR CharToUpperW(_In_ WCHAR chW)
{
  return ::MxRtlUpcaseUnicodeChar(chW);
}

VOID StrToLowerA(_Inout_z_ LPSTR szSrcA)
{
  StrNToLowerA(szSrcA, StrLenA(szSrcA));
  return;
}

VOID StrNToLowerA(_Inout_updates_(nLen) LPSTR szSrcA, _In_ SIZE_T nLen)
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

VOID StrToLowerW(_Inout_z_ LPWSTR szSrcW)
{
  StrNToLowerW(szSrcW, StrLenW(szSrcW));
  return;
}

VOID StrNToLowerW(_Inout_updates_(nLen) LPWSTR szSrcW, _In_ SIZE_T nLen)
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

VOID StrToUpperA(_Inout_z_ LPSTR szSrcA)
{
  StrNToUpperA(szSrcA, StrLenA(szSrcA));
  return;
}

VOID StrNToUpperA(_Inout_updates_(nLen) LPSTR szSrcA, _In_ SIZE_T nLen)
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

VOID StrToUpperW(_Inout_z_ LPWSTR szSrcW)
{
  StrNToUpperW(szSrcW, StrLenW(szSrcW));
  return;
}

VOID StrNToUpperW(_Inout_updates_(nLen) LPWSTR szSrcW, _In_ SIZE_T nLen)
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

HRESULT StrToDoubleA(_In_ LPCSTR szSrcA, _In_ SIZE_T nLen, double *lpnValue)
{
  CStringA cStrTempA;
  _invalid_parameter_handler lpOldHandler;
  _CRT_DOUBLE dblval;
  HRESULT hRes;

  if (lpnValue == NULL)
    return E_POINTER;
  *lpnValue = 0.0;

  if (nLen == 0)
    return S_OK;
  if (szSrcA == NULL)
    return (nLen == (SIZE_T)-1) ? S_OK : E_POINTER;

  if (nLen != (SIZE_T)-1)
  {
    if (cStrTempA.CopyN(szSrcA, nLen) == FALSE)
      return E_OUTOFMEMORY;
    szSrcA = (LPCSTR)cStrTempA;
  }

  lpOldHandler = _set_thread_local_invalid_parameter_handler(&dblconv_invalid_parameter);
  strtodbl_error = 0;

  hRes = S_OK;
  switch (_atodbl(&dblval, (char*)szSrcA))
  {
    case _OVERFLOW:
      hRes = MX_E_ArithmeticOverflow;
      break;
    case _UNDERFLOW:
      hRes = MX_E_ArithmeticUnderflow;
      break;
    default:
      if (strtodbl_error == 0)
        *lpnValue = dblval.x;
      else
        hRes = E_INVALIDARG;
      break;
  }

  _set_thread_local_invalid_parameter_handler(lpOldHandler);
  //done
  return hRes;
}

HRESULT StrToDoubleW(_In_ LPCWSTR szSrcW, _In_ SIZE_T nLen, double *lpnValue)
{
  CStringA cStrTempA;

  if (lpnValue == NULL)
    return E_POINTER;
  if (nLen == 0)
  {
    *lpnValue = 0.0;
    return S_OK;
  }
  if (nLen == (SIZE_T)-1)
    nLen = StrLenW(szSrcW);
  if (cStrTempA.CopyN(szSrcW, nLen) == FALSE)
  {
    *lpnValue = 0.0;
    return E_OUTOFMEMORY;
  }
  return StrToDoubleA((LPCSTR)cStrTempA, (SIZE_T)-1, lpnValue);
}

//-----------------------------------------------------------

CStringA::CStringA() : CBaseMemObj(), CNonCopyableObj()
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
    szStrA[nSize - 1] = 0;
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

BOOL CStringA::Copy(_In_opt_z_ LPCSTR szSrcA)
{
  Empty();
  return Concat(szSrcA);
}

BOOL CStringA::CopyN(_In_reads_or_z_opt_(nSrcLen) LPCSTR szSrcA, _In_ SIZE_T nSrcLen)
{
  Empty();
  return ConcatN(szSrcA, nSrcLen);
}

BOOL CStringA::Concat(_In_opt_z_ LPCSTR szSrcA)
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

BOOL CStringA::ConcatN(_In_reads_or_z_opt_(nSrcLen) LPCSTR szSrcA, _In_ SIZE_T nSrcLen)
{
  if (nSrcLen == 0)
    return TRUE;
  if (szSrcA == NULL)
    return FALSE;
  if (nLen + nSrcLen + 1 < nLen)
    return FALSE; //overflow
  if (EnsureBuffer(nLen + nSrcLen + 1) == FALSE)
    return FALSE;
  MxMemCopy(szStrA + nLen, szSrcA, nSrcLen);
  nLen += nSrcLen;
  szStrA[nLen] = 0;
  return TRUE;
}

BOOL CStringA::Copy(_In_opt_z_ LPCWSTR szSrcW)
{
  Empty();
  return Concat(szSrcW);
}

BOOL CStringA::CopyN(_In_reads_or_z_opt_(nSrcLen) LPCWSTR szSrcW, _In_ SIZE_T nSrcLen)
{
  Empty();
  return ConcatN(szSrcW, nSrcLen);
}

BOOL CStringA::Concat(_In_opt_z_ LPCWSTR szSrcW)
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

BOOL CStringA::ConcatN(_In_reads_or_z_opt_(nSrcLen) LPCWSTR szSrcW, _In_ SIZE_T nSrcLen)
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
  for (k = nSrcLen; k > 0; k -= nThisRoundLen)
  {
    nThisRoundLen = (k > 16384) ? 16384 : k;
    if (nThisRoundLen > 1 && usStr.Buffer[nThisRoundLen-1] >= 0xD800 && usStr.Buffer[nThisRoundLen-1] <= 0xDBFF)
      nThisRoundLen--; //don't split surrogate pairs
    usStr.Length = usStr.MaximumLength = (USHORT)nThisRoundLen * sizeof(WCHAR);
    nAnsiLen += (SIZE_T)::MxRtlUnicodeStringToAnsiSize(&usStr) - 1; //remove NUL char terminator
    usStr.Buffer += nThisRoundLen;
  }
  if (nLen + nAnsiLen + 1 < nLen)
    return FALSE; //overflow
  if (EnsureBuffer(nLen + nAnsiLen + 1) == FALSE)
    return FALSE;
  usStr.Buffer = (PWSTR)szSrcW;
  for (k = nSrcLen; k > 0; k -= nThisRoundLen)
  {
    nThisRoundLen = (k > 16384) ? 16384 : k;
    if (nThisRoundLen > 1 && usStr.Buffer[nThisRoundLen-1] >= 0xD800 && usStr.Buffer[nThisRoundLen-1] <= 0xDBFF)
      nThisRoundLen--; //don't split surrogate pairs
    usStr.Length = (USHORT)(nThisRoundLen * sizeof(WCHAR));
    asTemp.Buffer = (PSTR)(szStrA + nLen);
    asTemp.MaximumLength = 32768;
    ::MxRtlUnicodeStringToAnsiString(&asTemp, &usStr, FALSE);
    usStr.Buffer += nThisRoundLen;
    nLen += (SIZE_T)(asTemp.Length);
  }
  szStrA[nLen] = 0;
  return TRUE;
}

BOOL CStringA::Copy(_In_ PCMX_ANSI_STRING pASStr)
{
  Empty();
  return Concat(pASStr);
}

BOOL CStringA::Concat(_In_ PCMX_ANSI_STRING pASStr)
{
  if (pASStr != NULL && pASStr->Buffer != NULL && pASStr->Length != 0)
    return ConcatN(pASStr->Buffer, (SIZE_T)(pASStr->Length));
  return TRUE;
}

BOOL CStringA::Copy(_In_ LONG nSrc)
{
  Empty();
  return Concat(nSrc);
}

BOOL CStringA::Concat(_In_ LONG nSrc)
{
  return Concat((LONGLONG)nSrc);
}

BOOL CStringA::Copy(_In_ ULONG nSrc)
{
  Empty();
  return Concat(nSrc);
}

BOOL CStringA::Concat(_In_ ULONG nSrc)
{
  return Concat((ULONGLONG)nSrc);
}

BOOL CStringA::Copy(_In_ LONGLONG nSrc)
{
  Empty();
  return Concat(nSrc);
}

BOOL CStringA::Concat(_In_ LONGLONG nSrc)
{
  CHAR szTempA[128];

  mx_sprintf_s(szTempA, MX_ARRAYLEN(szTempA), "%I64d", nSrc);
  return Concat(szTempA);
}

BOOL CStringA::Copy(_In_ ULONGLONG nSrc)
{
  Empty();
  return Concat(nSrc);
}

BOOL CStringA::Concat(_In_ ULONGLONG nSrc)
{
  CHAR szTempA[128];

  mx_sprintf_s(szTempA, MX_ARRAYLEN(szTempA), "%I64u", nSrc);
  return Concat(szTempA);
}

BOOL CStringA::Format(_In_z_ _Printf_format_string_ LPCSTR szFormatA, ...)
{
  va_list argptr;
  BOOL bRet;

  Empty();
  if (szFormatA == NULL)
    return FALSE;
  va_start(argptr, szFormatA);
  bRet = FormatV(szFormatA, argptr);
  va_end(argptr);
  return bRet;
}

BOOL CStringA::Format(_In_z_ _Printf_format_string_ LPCWSTR szFormatW, ...)
{
  va_list argptr;
  BOOL bRet;

  Empty();
  if (szFormatW == NULL)
    return FALSE;
  va_start(argptr, szFormatW);
  bRet = FormatV(szFormatW, argptr);
  va_end(argptr);
  return bRet;
}

BOOL CStringA::FormatV(_In_z_ _Printf_format_string_params_(1) LPCSTR szFormatA, _In_ va_list argptr)
{
  Empty();
  return AppendFormatV(szFormatA, argptr);
}

BOOL CStringA::FormatV(_In_z_ _Printf_format_string_params_(1) LPCWSTR szFormatW, _In_ va_list argptr)
{
  Empty();
  return AppendFormatV(szFormatW, argptr);
}

BOOL CStringA::AppendFormat(_In_z_ _Printf_format_string_ LPCSTR szFormatA, ...)
{
  va_list argptr;
  BOOL bRet;

  if (szFormatA == NULL)
    return FALSE;
  va_start(argptr, szFormatA);
  bRet = AppendFormatV(szFormatA, argptr);
  va_end(argptr);
  return bRet;
}

BOOL CStringA::AppendFormat(_In_z_ _Printf_format_string_ LPCWSTR szFormatW, ...)
{
  va_list argptr;
  BOOL bRet;

  if (szFormatW == NULL)
    return FALSE;
  va_start(argptr, szFormatW);
  bRet = AppendFormatV(szFormatW, argptr);
  va_end(argptr);
  return bRet;
}

BOOL CStringA::AppendFormatV(_In_z_ _Printf_format_string_params_(1) LPCSTR szFormatA, _In_ va_list argptr)
{
  int nChars;
  SIZE_T nBufSize;

  if (szFormatA == NULL)
    return FALSE;
  nBufSize = 512;
  while (1)
  {
    if (EnsureBuffer(nLen + nBufSize+1) == FALSE)
      return FALSE;
    nChars = mx_vsnprintf(szStrA + nLen, (size_t)nBufSize, szFormatA, argptr);
    if ((SIZE_T)nChars < nBufSize - 2)
      break;
    nBufSize += 4096;
  }
  nLen += (SIZE_T)nChars;
  szStrA[nLen] = 0;
  return TRUE;
}

BOOL CStringA::AppendFormatV(_In_z_ _Printf_format_string_params_(1) LPCWSTR szFormatW, _In_ va_list argptr)
{
  WCHAR szTempBufW[512], *szTempW;
  int nChars, nBufSize;
  BOOL bRet;

  if (szFormatW == NULL)
    return FALSE;
  nChars = mx_vsnwprintf(szTempBufW, 512, szFormatW, argptr);
  if (nChars > 0)
  {
    if (nChars < 510)
    {
      bRet = ConcatN(szTempBufW, (size_t)nChars);
    }
    else
    {
      nBufSize = nChars * 2;
      while (1)
      {
        szTempW = (LPWSTR)MX_MALLOC((SIZE_T)(nBufSize + 1) * sizeof(WCHAR));
        if (szTempW == NULL)
          return FALSE;
        nChars = mx_vsnwprintf(szTempW, (SIZE_T)nBufSize, szFormatW, argptr);
        if (nChars < nBufSize - 2)
          break;
        nBufSize += 4096;
        MX_FREE(szTempW);
      }
      bRet = ConcatN(szTempW, (SIZE_T)nChars);
      MX_FREE(szTempW);
    }
    if (bRet == FALSE)
      return FALSE;
  }
  return TRUE;
}

BOOL CStringA::Insert(_In_opt_z_ LPCSTR szSrcA, _In_ SIZE_T nInsertPosition)
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

BOOL CStringA::InsertN(_In_reads_or_z_opt_(nSrcLen) LPCSTR szSrcA, _In_ SIZE_T nInsertPosition, _In_ SIZE_T nSrcLen)
{
  LPSTR sA;

  if (nSrcLen == 0)
    return TRUE;
  if (szSrcA == NULL)
    return FALSE;
  if (nLen + nSrcLen + 1 < nLen)
    return FALSE; //overflow
  if (EnsureBuffer(nLen + nSrcLen + 1) == FALSE)
    return FALSE;
  if (nInsertPosition > nLen)
    nInsertPosition = nLen;
  sA = szStrA + nInsertPosition;
  MxMemMove(sA+nSrcLen, sA, (nLen - nInsertPosition));
  MxMemCopy(sA, szSrcA, nSrcLen);
  nLen += nSrcLen;
  szStrA[nLen] = 0;
  return TRUE;
}

VOID CStringA::Delete(_In_ SIZE_T nStartChar, _In_ SIZE_T nChars)
{
  LPSTR sA;
  SIZE_T k;

  if (nLen > 0)
  {
    if (nStartChar >= nLen)
      return;
    if (nStartChar + nChars > nLen || nStartChar + nChars < nStartChar) //overflow
      nChars = nLen - nStartChar;
    k = nLen - nChars - nStartChar;
    sA = szStrA + nStartChar;
    nLen -= nChars;
    MxMemMove(sA, sA + nChars, k);
    szStrA[nLen] = 0;
  }
  return;
}

BOOL CStringA::StartsWith(_In_z_ LPCSTR _szStrA, _In_opt_ BOOL bCaseInsensitive)
{
  SIZE_T nSrcLen;

  nSrcLen = StrLenA(_szStrA);
  if (nSrcLen == 0 || nLen < nSrcLen)
    return FALSE;
  return (StrNCompareA(szStrA, _szStrA, nSrcLen, bCaseInsensitive) == 0) ? TRUE : FALSE;
}

BOOL CStringA::EndsWith(_In_z_ LPCSTR _szStrA, _In_opt_ BOOL bCaseInsensitive)
{
  SIZE_T nSrcLen;

  nSrcLen = StrLenA(_szStrA);
  if (nSrcLen == 0 || nLen < nSrcLen)
    return FALSE;
  return (StrNCompareA(szStrA + (nLen - nSrcLen), _szStrA, nSrcLen, bCaseInsensitive) == 0) ? TRUE : FALSE;
}

LPCSTR CStringA::Contains(_In_z_ LPCSTR _szStrA, _In_opt_ BOOL bCaseInsensitive)
{
  if (_szStrA == NULL || *_szStrA == 0)
    return NULL;
  return MX::StrFindA(szStrA, _szStrA, FALSE, bCaseInsensitive);
}

VOID CStringA::Attach(_In_z_ LPSTR szSrcA)
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

BOOL CStringA::EnsureBuffer(_In_ SIZE_T nChars)
{
  LPSTR szNewStrA;
  SIZE_T nOrigLen;

  nChars++;
  if (nChars >= nSize)
  {
    nOrigLen = nLen;
    if (nSize > 0)
      nChars = X_ALIGN(nChars, 256);
    szNewStrA = (LPSTR)MX_MALLOC(nChars);
    if (szNewStrA == NULL)
      return FALSE;
    MxMemCopy(szNewStrA, szStrA, nLen);
    Empty(); //<<--- to secure delete on derived classes
    szStrA = szNewStrA;
    nSize = nChars;
    szStrA[nLen = nOrigLen] = 0;
  }
  return TRUE;
}

CHAR& CStringA::operator[](_In_ SIZE_T nIndex)
{
  return szStrA[nIndex];
}

LPWSTR CStringA::ToWide()
{
  return Ansi2Wide(szStrA, nLen);
}

LPWSTR CStringA::Ansi2Wide(_In_z_ LPCSTR szStrA, _In_ SIZE_T nSrcLen)
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

//--------

VOID CSecureStringA::Empty()
{
  if (szStrA != NULL && nSize > 0)
    ::MxMemSet(szStrA, '*', nSize);
  CStringA::Empty();
  return;
}

BOOL CSecureStringA::Concat(_In_ LONGLONG nSrc)
{
  CHAR szTempA[128];
  BOOL bRet;

  mx_sprintf_s(szTempA, MX_ARRAYLEN(szTempA), "%I64d", nSrc);
  bRet = CStringA::Concat(szTempA);
  ::MxMemSet(szTempA, 0, sizeof(szTempA));
  return bRet;
}

BOOL CSecureStringA::Concat(_In_ ULONGLONG nSrc)
{
  CHAR szTempA[128];
  BOOL bRet;

  mx_sprintf_s(szTempA, MX_ARRAYLEN(szTempA), "%I64u", nSrc);
  bRet = CStringA::Concat(szTempA);
  ::MxMemSet(szTempA, 0, sizeof(szTempA));
  return bRet;
}

BOOL CSecureStringA::AppendFormatV(_In_z_ _Printf_format_string_params_(1) LPCWSTR szFormatW, _In_ va_list argptr)
{
  WCHAR szTempBufW[512], *szTempW;
  int nChars, nBufSize;
  BOOL bRet;

  if (szFormatW == NULL)
    return FALSE;
  nChars = mx_vsnwprintf(szTempBufW, 512, szFormatW, argptr);
  if (nChars > 0)
  {
    if (nChars < 510)
    {
      bRet = ConcatN(szTempBufW, (size_t)nChars);
      ::MxMemSet(szTempBufW, 0, sizeof(szTempBufW));
    }
    else
    {
      ::MxMemSet(szTempBufW, 0, sizeof(szTempBufW));
      nBufSize = nChars * 2;
      while (1)
      {
        szTempW = (LPWSTR)MX_MALLOC((SIZE_T)(nBufSize + 1) * sizeof(WCHAR));
        if (szTempW == NULL)
          return FALSE;
        nChars = mx_vsnwprintf(szTempW, (SIZE_T)nBufSize, szFormatW, argptr);
        if (nChars < nBufSize - 2)
          break;
        ::MxMemSet(szTempW, 0, (SIZE_T)(nBufSize + 1) * sizeof(WCHAR));
        nBufSize += 4096;
        MX_FREE(szTempW);
      }
      bRet = ConcatN(szTempW, (SIZE_T)nChars);
      ::MxMemSet(szTempW, 0, (SIZE_T)(nBufSize + 1) * sizeof(WCHAR));
      MX_FREE(szTempW);
    }
    ::MxMemSet(szTempBufW, 0, sizeof(szTempBufW));
    if (bRet == FALSE)
      return FALSE;
  }
  return TRUE;
}

//-----------------------------------------------------------

CStringW::CStringW() : CBaseMemObj(), CNonCopyableObj()
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
    szStrW[nSize - 1] = 0;
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

BOOL CStringW::Copy(_In_opt_z_ LPCSTR szSrcA)
{
  Empty();
  return Concat(szSrcA);
}

BOOL CStringW::CopyN(_In_reads_or_z_opt_(nSrcLen) LPCSTR szSrcA, _In_ SIZE_T nSrcLen)
{
  Empty();
  return ConcatN(szSrcA, nSrcLen);
}

BOOL CStringW::Concat(_In_opt_z_ LPCSTR szSrcA)
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

BOOL CStringW::ConcatN(_In_reads_or_z_opt_(nSrcLen) LPCSTR szSrcA, _In_ SIZE_T nSrcLen)
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
  for (k = nSrcLen; k > 0; k -= (SIZE_T)(asStr.Length))
  {
    asStr.Length = asStr.MaximumLength = (USHORT)((k > 16384) ? 16384 : k);
    nWideLen += ((SIZE_T)::MxRtlAnsiStringToUnicodeSize(&asStr) / sizeof(WCHAR)) - 1; //remove NUL char terminator
    asStr.Buffer += (SIZE_T)(asStr.Length);
  }
  if (nLen + nWideLen + 1 < nLen)
    return FALSE; //overflow
  if (EnsureBuffer(nLen + nWideLen + 1) == FALSE)
    return FALSE;
  asStr.Buffer = (PSTR)szSrcA;
  for (k = nSrcLen; k > 0; k -= (SIZE_T)(asStr.Length))
  {
    asStr.Length = (USHORT)((k > 16384) ? 16384 : k); 
    usTemp.Buffer = (PWSTR)(szStrW + nLen);
    usTemp.MaximumLength = 32768;
    ::MxRtlAnsiStringToUnicodeString(&usTemp, &asStr, FALSE);
    asStr.Buffer += (SIZE_T)(asStr.Length);
    nLen += (SIZE_T)(usTemp.Length) / sizeof(WCHAR);
  }
  szStrW[nLen] = 0;
  return TRUE;
}

BOOL CStringW::Copy(_In_opt_z_ LPCWSTR szSrcW)
{
  Empty();
  return Concat(szSrcW);
}

BOOL CStringW::CopyN(_In_reads_or_z_opt_(nSrcLen) LPCWSTR szSrcW, _In_ SIZE_T nSrcLen)
{
  Empty();
  return ConcatN(szSrcW, nSrcLen);
}

BOOL CStringW::Concat(_In_opt_z_ LPCWSTR szSrcW)
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

BOOL CStringW::ConcatN(_In_reads_or_z_opt_(nSrcLen) LPCWSTR szSrcW, _In_ SIZE_T nSrcLen)
{
  if (nSrcLen == 0)
    return TRUE;
  if (szSrcW == NULL)
    return FALSE;
  if (nLen + nSrcLen + 1 < nLen)
    return FALSE; //overflow
  if (EnsureBuffer(nLen + nSrcLen + 1) == FALSE)
    return FALSE;
  MxMemCopy(szStrW + nLen, szSrcW, nSrcLen * sizeof(WCHAR));
  nLen += nSrcLen;
  szStrW[nLen] = 0;
  return TRUE;
}

BOOL CStringW::Copy(_In_ PCMX_UNICODE_STRING pUSStr)
{
  Empty();
  return Concat(pUSStr);
}

BOOL CStringW::Concat(_In_ PCMX_UNICODE_STRING pUSStr)
{
  if (pUSStr != NULL && pUSStr->Buffer != NULL && pUSStr->Length != 0)
    return ConcatN(pUSStr->Buffer, (SIZE_T)(pUSStr->Length) / sizeof(WCHAR));
  return TRUE;
}

BOOL CStringW::Copy(_In_ LONG nSrc)
{
  Empty();
  return Concat(nSrc);
}

BOOL CStringW::Concat(_In_ LONG nSrc)
{
  return Concat((LONGLONG)nSrc);
}

BOOL CStringW::Copy(_In_ ULONG nSrc)
{
  Empty();
  return Concat(nSrc);
}

BOOL CStringW::Concat(_In_ ULONG nSrc)
{
  return Concat((ULONGLONG)nSrc);
}

BOOL CStringW::Copy(_In_ LONGLONG nSrc)
{
  Empty();
  return Concat(nSrc);
}

BOOL CStringW::Concat(_In_ LONGLONG nSrc)
{
  WCHAR szTempW[128];

  mx_swprintf_s(szTempW, MX_ARRAYLEN(szTempW), L"%I64d", nSrc);
  return Concat(szTempW);
}

BOOL CStringW::Copy(_In_ ULONGLONG nSrc)
{
  Empty();
  return Concat(nSrc);
}

BOOL CStringW::Concat(_In_ ULONGLONG nSrc)
{
  WCHAR szTempW[128];

  mx_swprintf_s(szTempW, MX_ARRAYLEN(szTempW), L"%I64u", nSrc);
  return Concat(szTempW);
}

BOOL CStringW::Format(_In_z_ _Printf_format_string_ LPCSTR szFormatA, ...)
{
  va_list argptr;
  BOOL bRet;

  Empty();
  if (szFormatA == NULL)
    return FALSE;
  va_start(argptr, szFormatA);
  bRet = FormatV(szFormatA, argptr);
  va_end(argptr);
  return bRet;
}

BOOL CStringW::Format(_In_z_ _Printf_format_string_ LPCWSTR szFormatW, ...)
{
  va_list argptr;
  BOOL bRet;

  Empty();
  if (szFormatW == NULL)
    return FALSE;
  va_start(argptr, szFormatW);
  bRet = FormatV(szFormatW, argptr);
  va_end(argptr);
  return bRet;
}

BOOL CStringW::FormatV(_In_z_ _Printf_format_string_params_(1) LPCSTR szFormatA, _In_ va_list argptr)
{
  Empty();
  return AppendFormatV(szFormatA, argptr);
}

BOOL CStringW::FormatV(_In_z_ _Printf_format_string_params_(1) LPCWSTR szFormatW, _In_ va_list argptr)
{
  Empty();
  return AppendFormatV(szFormatW, argptr);
}

BOOL CStringW::AppendFormat(_In_z_ _Printf_format_string_ LPCSTR szFormatA, ...)
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

BOOL CStringW::AppendFormat(_In_z_ _Printf_format_string_ LPCWSTR szFormatW, ...)
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

BOOL CStringW::AppendFormatV(_In_z_ _Printf_format_string_params_(1) LPCSTR szFormatA, _In_ va_list argptr)
{
  CHAR szTempBufA[512], *szTempA;
  int nChars, nBufSize;
  BOOL bRet;

  if (szFormatA == NULL)
    return FALSE;
  nChars = mx_vsnprintf(szTempBufA, 512, szFormatA, argptr);
  if (nChars > 0)
  {
    if (nChars < 510)
    {
      bRet = ConcatN(szTempBufA, (size_t)nChars);
    }
    else
    {
      nBufSize = nChars * 2;
      while (1)
      {
        szTempA = (LPSTR)MX_MALLOC((SIZE_T)(nBufSize + 1) * sizeof(CHAR));
        if (szTempA == NULL)
          return FALSE;
        nChars = mx_vsnprintf(szTempA, (size_t)nBufSize, szFormatA, argptr);
        if (nChars < nBufSize - 2)
          break;
        nBufSize += 4096;
        MX_FREE(szTempA);
      }
      bRet = ConcatN(szTempA, (size_t)nChars);
      MX_FREE(szTempA);
    }
    if (bRet == FALSE)
      return FALSE;
  }
  return TRUE;
}

BOOL CStringW::AppendFormatV(_In_z_ _Printf_format_string_params_(1) LPCWSTR szFormatW, _In_ va_list argptr)
{
  int nChars;
  SIZE_T nBufSize;

  if (szFormatW == NULL)
    return FALSE;
  nBufSize = 512;
  while (1)
  {
    if (EnsureBuffer(nLen + nBufSize + 1) == FALSE)
      return FALSE;
    nChars = mx_vsnwprintf(szStrW + nLen, (SIZE_T)nBufSize, szFormatW, argptr);
    if ((SIZE_T)nChars < nBufSize - 2)
      break;
    nBufSize += 4096;
  }
  nLen += (SIZE_T)nChars;
  szStrW[nLen] = 0;
  return TRUE;
}

BOOL CStringW::Insert(_In_opt_z_ LPCWSTR szSrcW, _In_ SIZE_T nInsertPosition)
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

BOOL CStringW::InsertN(_In_reads_or_z_opt_(nSrcLen) LPCWSTR szSrcW, _In_ SIZE_T nInsertPosition, _In_ SIZE_T nSrcLen)
{
  LPWSTR sW;

  if (nSrcLen == 0)
    return TRUE;
  if (szSrcW == NULL)
    return FALSE;
  if (nLen + nSrcLen + 1 < nLen)
    return FALSE; //overflow
  if (EnsureBuffer(nLen + nSrcLen + 1) == FALSE)
    return FALSE;
  if (nInsertPosition > nLen)
    nInsertPosition = nLen;
  sW = szStrW + nInsertPosition;
  MxMemMove(sW+nSrcLen, sW, (nLen - nInsertPosition) * sizeof(WCHAR));
  MxMemCopy(sW, szSrcW, nSrcLen * sizeof(WCHAR));
  nLen += nSrcLen;
  szStrW[nLen] = 0;
  return TRUE;
}

VOID CStringW::Delete(_In_ SIZE_T nStartChar, _In_ SIZE_T nChars)
{
  LPWSTR sW;
  SIZE_T k;

  if (nLen > 0)
  {
    if (nStartChar > nLen)
      return;
    if (nStartChar + nChars > nLen || nStartChar + nChars < nStartChar) //overflow
      nChars = nLen - nStartChar;
    k = nLen - nChars - nStartChar;
    sW = szStrW + nStartChar;
    nLen -= nChars;
    MxMemMove(sW, sW + nChars, k * sizeof(WCHAR));
    szStrW[nLen] = 0;
  }
  return;
}

BOOL CStringW::StartsWith(_In_z_ LPCWSTR _szStrW, _In_opt_ BOOL bCaseInsensitive)
{
  SIZE_T nSrcLen;

  nSrcLen = StrLenW(_szStrW);
  if (nSrcLen == 0 || nLen < nSrcLen)
    return FALSE;
  return (StrNCompareW(szStrW, _szStrW, nSrcLen, bCaseInsensitive) == 0) ? TRUE : FALSE;
}

BOOL CStringW::EndsWith(_In_z_ LPCWSTR _szStrW, _In_opt_ BOOL bCaseInsensitive)
{
  SIZE_T nSrcLen;

  nSrcLen = StrLenW(_szStrW);
  if (nSrcLen == 0 || nLen < nSrcLen)
    return FALSE;
  return (StrNCompareW(szStrW + (nLen - nSrcLen), _szStrW, nSrcLen, bCaseInsensitive) == 0) ? TRUE : FALSE;
}

LPCWSTR CStringW::Contains(_In_z_ LPCWSTR _szStrW, _In_opt_ BOOL bCaseInsensitive)
{
  if (_szStrW == NULL || *_szStrW == 0)
    return NULL;
  return MX::StrFindW(szStrW, _szStrW, FALSE, bCaseInsensitive);
}

VOID CStringW::Attach(_In_z_ LPWSTR szSrcW)
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

BOOL CStringW::EnsureBuffer(_In_ SIZE_T nChars)
{
  LPWSTR szNewStrW;
  SIZE_T nOrigLen;

  nChars++;
  if (nChars >= nSize)
  {
    nOrigLen = nLen;
    if (nSize > 0)
      nChars = X_ALIGN(nChars, 256);
    szNewStrW = (LPWSTR)MX_MALLOC(nChars*sizeof(WCHAR));
    if (szNewStrW == NULL)
      return FALSE;
    MxMemCopy(szNewStrW, szStrW, nLen*sizeof(WCHAR));
    Empty(); //<<--- to secure delete on derived classes
    szStrW = szNewStrW;
    nSize = nChars;
    szStrW[nLen = nOrigLen] = 0;
  }
  return TRUE;
}

WCHAR& CStringW::operator[](_In_ SIZE_T nIndex)
{
  return szStrW[nIndex];
}

LPSTR CStringW::ToAnsi()
{
  return Wide2Ansi(szStrW, nLen);
}

LPSTR CStringW::Wide2Ansi(_In_z_ LPCWSTR szStrW, _In_ SIZE_T nSrcLen)
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

//--------

VOID CSecureStringW::Empty()
{
  if (szStrW != NULL && nSize > 0)
    ::MxMemSet(szStrW, '*', nSize * 2);
  CStringW::Empty();
  return;
}

BOOL CSecureStringW::Concat(_In_ LONGLONG nSrc)
{
  WCHAR szTempW[128];
  BOOL bRet;

  mx_swprintf_s(szTempW, MX_ARRAYLEN(szTempW), L"%I64d", nSrc);
  bRet = CStringW::Concat(szTempW);
  ::MxMemSet(szTempW, 0, sizeof(szTempW));
  return bRet;
}

BOOL CSecureStringW::Concat(_In_ ULONGLONG nSrc)
{
  WCHAR szTempW[128];
  BOOL bRet;

  mx_swprintf_s(szTempW, MX_ARRAYLEN(szTempW), L"%I64u", nSrc);
  bRet = CStringW::Concat(szTempW);
  ::MxMemSet(szTempW, 0, sizeof(szTempW));
  return bRet;
}

BOOL CSecureStringW::AppendFormatV(_In_z_ _Printf_format_string_params_(1) LPCSTR szFormatA, _In_ va_list argptr)
{
  CHAR szTempBufA[512], *szTempA;
  int nChars, nBufSize;
  BOOL bRet;

  if (szFormatA == NULL)
    return FALSE;
  nChars = mx_vsnprintf(szTempBufA, 512, szFormatA, argptr);
  if (nChars > 0)
  {
    if (nChars < 510)
    {
      bRet = ConcatN(szTempBufA, (size_t)nChars);
      ::MxMemSet(szTempBufA, 0, sizeof(szTempBufA));
    }
    else
    {
      ::MxMemSet(szTempBufA, 0, sizeof(szTempBufA));
      nBufSize = nChars * 2;
      while (1)
      {
        szTempA = (LPSTR)MX_MALLOC((SIZE_T)(nBufSize + 1) * sizeof(CHAR));
        if (szTempA == NULL)
          return FALSE;
        nChars = mx_vsnprintf(szTempA, (size_t)nBufSize, szFormatA, argptr);
        if (nChars < nBufSize - 2)
          break;
        ::MxMemSet(szTempA, 0, (SIZE_T)(nBufSize + 1) * sizeof(CHAR));
        nBufSize += 4096;
        MX_FREE(szTempA);
      }
      bRet = ConcatN(szTempA, (size_t)nChars);
      ::MxMemSet(szTempA, 0, (SIZE_T)(nBufSize + 1) * sizeof(CHAR));
      MX_FREE(szTempA);
    }
    if (bRet == FALSE)
      return FALSE;
  }
  return TRUE;
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

static void dblconv_invalid_parameter(const wchar_t * expression, const wchar_t * function, const wchar_t * file,
                                      unsigned int line, uintptr_t pReserved)
{
  strtodbl_error = 1;
  return;
}
