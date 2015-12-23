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
#ifndef _MX_STRINGS_H
#define _MX_STRINGS_H

#include "..\Defines.h"

//-----------------------------------------------------------

namespace MX {

SIZE_T StrLenA(__in_z_opt LPCSTR szSrcA);
SIZE_T StrLenW(__in_z_opt LPCWSTR szSrcW);

int StrCompareA(__in_z LPCSTR szSrcA1, __in_z LPCSTR szSrcA2, __in_opt BOOL bCaseInsensitive=FALSE);
int StrCompareW(__in_z LPCWSTR szSrcW1, __in_z LPCWSTR szSrcW2, __in_opt BOOL bCaseInsensitive=FALSE);
int StrNCompareA(__in_z LPCSTR szSrcA1, __in_z LPCSTR szSrcA2, __in SIZE_T nLen,
                 __in_opt BOOL bCaseInsensitive=FALSE);
int StrNCompareW(__in_z LPCWSTR szSrcW1, __in_z LPCWSTR szSrcW2, __in SIZE_T nLen,
                 __in_opt BOOL bCaseInsensitive=FALSE);
int StrCompareAW(__in_z LPCSTR szSrcA1, __in_z LPCWSTR szSrcW2, __in_opt BOOL bCaseInsensitive=FALSE);
int StrNCompareAW(__in_z LPCSTR szSrcA1, __in_z LPCWSTR szSrcW2, __in SIZE_T nLen,
                  __in_opt BOOL bCaseInsensitive=FALSE);

LPCSTR StrChrA(__in_z LPCSTR szSrcA, __in CHAR chA, __in_opt BOOL bReverse=FALSE);
LPCWSTR StrChrW(__in_z LPCWSTR szSrcW, __in WCHAR chW, __in_opt BOOL bReverse=FALSE);
LPCSTR StrNChrA(__in_z LPCSTR szSrcA, __in CHAR chA, __in SIZE_T nLen, __in_opt BOOL bReverse=FALSE);
LPCWSTR StrNChrW(__in_z LPCWSTR szSrcW, __in WCHAR chW, __in SIZE_T nLen, __in_opt BOOL bReverse=FALSE);

LPCSTR StrFindA(__in_z LPCSTR szSrcA, __in_z LPCSTR szToFindA, __in_opt BOOL bReverse=FALSE,
                __in_opt BOOL bCaseInsensitive=FALSE);
LPCWSTR StrFindW(__in_z LPCWSTR szSrcW, __in_z LPCWSTR szToFindW, __in_opt BOOL bReverse=FALSE,
                 __in_opt BOOL bCaseInsensitive=FALSE);
LPCSTR StrNFindA(__in_z LPCSTR szSrcA, __in_z LPCSTR szToFindA, __in SIZE_T nLen, __in_opt BOOL bReverse=FALSE,
                 __in_opt BOOL bCaseInsensitive=FALSE);
LPCWSTR StrNFindW(__in_z LPCWSTR szSrcW, __in_z LPCWSTR szToFindW, __in SIZE_T nLen, __in_opt BOOL bReverse=FALSE,
                  __in_opt BOOL bCaseInsensitive=FALSE);

CHAR CharToLowerA(__in CHAR chA);
CHAR CharToUpperA(__in CHAR chA);
WCHAR CharToLowerW(__in WCHAR chW);
WCHAR CharToUpperW(__in WCHAR chW);

VOID StrToLowerA(__inout_z LPSTR szSrcA);
VOID StrNToLowerA(__inout_z LPSTR szSrcA, __in SIZE_T nLen);
VOID StrToLowerW(__inout_z LPWSTR szSrcW);
VOID StrNToLowerW(__inout_z LPWSTR szSrcW, __in SIZE_T nLen);
VOID StrToUpperA(__inout_z LPSTR szSrcA);
VOID StrNToUpperA(__inout_z LPSTR szSrcA, __in SIZE_T nLen);
VOID StrToUpperW(__inout_z LPWSTR szSrcW);
VOID StrNToUpperW(__inout_z LPWSTR szSrcW, __in SIZE_T nLen);

//-----------------------------------------------------------

class CStringA : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CStringA);
public:
  CStringA();
  ~CStringA();

  virtual operator LPSTR() const
    {
    return (szStrA != NULL) ? szStrA : "";
    };
  virtual operator LPCSTR() const
    {
    return (szStrA != NULL) ? szStrA : "";
    };

  virtual VOID Empty();

  virtual VOID Refresh();

  virtual SIZE_T GetLength() const;

  virtual BOOL IsEmpty() const
    {
    return (szStrA != NULL && *szStrA != 0) ? FALSE : TRUE;
    };

  virtual BOOL Copy(__in_z_opt LPCSTR szSrcA);
  virtual BOOL CopyN(__in_nz_opt LPCSTR szSrcA, __in SIZE_T nSrcLen);
  virtual BOOL Concat(__in_z_opt LPCSTR szSrcA);
  virtual BOOL ConcatN(__in_nz_opt LPCSTR szSrcA, __in SIZE_T nSrcLen);

  virtual BOOL Copy(__in_z_opt LPCWSTR szSrcW);
  virtual BOOL CopyN(__in_nz_opt LPCWSTR szSrcW, __in SIZE_T nSrcLen);
  virtual BOOL Concat(__in_z_opt LPCWSTR szSrcW);
  virtual BOOL ConcatN(__in_nz_opt LPCWSTR szSrcW, __in SIZE_T nSrcLen);

  virtual BOOL Copy(__in PCMX_ANSI_STRING pASStr);
  virtual BOOL Concat(__in PCMX_ANSI_STRING pASStr);

  virtual BOOL Copy(__in LONG nSrc);
  virtual BOOL Concat(__in LONG nSrc);
  virtual BOOL Copy(__in ULONG nSrc);
  virtual BOOL Concat(__in ULONG nSrc);

  virtual BOOL Copy(__in LONGLONG nSrc);
  virtual BOOL Concat(__in LONGLONG nSrc);
  virtual BOOL Copy(__in ULONGLONG nSrc);
  virtual BOOL Concat(__in ULONGLONG nSrc);

  virtual BOOL Format(__in_z LPCSTR szFormatA, ...);
  virtual BOOL Format(__in_z LPCWSTR szFormatW, ...);
  virtual BOOL FormatV(__in_z LPCSTR szFormatA, __in va_list argptr);
  virtual BOOL FormatV(__in_z LPCWSTR szFormatW, __in va_list argptr);

  virtual BOOL AppendFormat(__in_z LPCSTR szFormatA, ...);
  virtual BOOL AppendFormat(__in_z LPCWSTR szFormatW, ...);
  virtual BOOL AppendFormatV(__in_z LPCSTR szFormatA, __in va_list argptr);
  virtual BOOL AppendFormatV(__in_z LPCWSTR szFormatW, __in va_list argptr);

  virtual BOOL Insert(__in_z_opt LPCSTR szSrcA, __in SIZE_T nInsertPosition);
  virtual BOOL InsertN(__in_nz_opt LPCSTR szSrcA, __in SIZE_T nInsertPosition, __in SIZE_T nSrcLen);
  virtual VOID Delete(__in SIZE_T nStartChar, __in SIZE_T nChars);

  virtual BOOL StartsWith(__in_z LPCSTR szStrA, __in_opt BOOL bCaseInsensitive=TRUE);
  virtual BOOL EndsWith(__in_z LPCSTR szStrA, __in_opt BOOL bCaseInsensitive=TRUE);
  virtual LPCSTR Contains(__in_z LPCSTR szStrA, __in_opt BOOL bCaseInsensitive=TRUE);

  virtual VOID Attach(__in_z LPSTR szSrcA);
  virtual LPSTR Detach();

  virtual BOOL EnsureBuffer(__in SIZE_T nChars);

  virtual CHAR operator[](__in SIZE_T nIndex) const
    {
    return (szStrA != NULL) ? szStrA[nIndex] : 0;
    };
  virtual CHAR& operator[](__in SIZE_T nIndex);

  LPWSTR ToWide();
  static LPWSTR Ansi2Wide(__in_z LPCSTR szStrA, __in SIZE_T nSrcLen);

private:
  LPSTR szStrA;
  SIZE_T nSize, nLen;
};

//-----------------------------------------------------------

class CStringW : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CStringW);
public:
  CStringW();
  ~CStringW();

  virtual operator LPWSTR() const
    {
    return (szStrW != NULL) ? szStrW : L"";
    };
  virtual operator LPCWSTR() const
    {
    return (szStrW != NULL) ? szStrW : L"";
    };

  virtual VOID Empty();

  virtual VOID Refresh();

  virtual SIZE_T GetLength() const;

  virtual BOOL IsEmpty() const
    {
    return (szStrW != NULL && *szStrW != 0) ? FALSE : TRUE;
    };

  virtual BOOL Copy(__in_z_opt LPCSTR szSrcA);
  virtual BOOL CopyN(__in_nz_opt LPCSTR szSrcA, __in SIZE_T nSrcLen);
  virtual BOOL Concat(__in_z_opt LPCSTR szSrcA);
  virtual BOOL ConcatN(__in_nz_opt LPCSTR szSrcA, __in SIZE_T nSrcLen);

  virtual BOOL Copy(__in_z_opt LPCWSTR szSrcW);
  virtual BOOL CopyN(__in_nz_opt LPCWSTR szSrcW, __in SIZE_T nSrcLen);
  virtual BOOL Concat(__in_z_opt LPCWSTR szSrcW);
  virtual BOOL ConcatN(__in_nz_opt LPCWSTR szSrcW, __in SIZE_T nSrcLen);

  virtual BOOL Copy(__in PCMX_UNICODE_STRING pUSStr);
  virtual BOOL Concat(__in PCMX_UNICODE_STRING pUSStr);

  virtual BOOL Copy(__in LONG nSrc);
  virtual BOOL Concat(__in LONG nSrc);
  virtual BOOL Copy(__in ULONG nSrc);
  virtual BOOL Concat(__in ULONG nSrc);

  virtual BOOL Copy(__in LONGLONG nSrc);
  virtual BOOL Concat(__in LONGLONG nSrc);
  virtual BOOL Copy(__in ULONGLONG nSrc);
  virtual BOOL Concat(__in ULONGLONG nSrc);

  virtual BOOL Format(__in_z LPCSTR szFormatA, ...);
  virtual BOOL Format(__in_z LPCWSTR szFormatW, ...);
  virtual BOOL FormatV(__in_z LPCSTR szFormatA, __in va_list argptr);
  virtual BOOL FormatV(__in_z LPCWSTR szFormatW, __in va_list argptr);

  virtual BOOL AppendFormat(__in_z LPCSTR szFormatA, ...);
  virtual BOOL AppendFormat(__in_z LPCWSTR szFormatW, ...);
  virtual BOOL AppendFormatV(__in_z LPCSTR szFormatA, __in va_list argptr);
  virtual BOOL AppendFormatV(__in_z LPCWSTR szFormatW, __in va_list argptr);

  virtual BOOL Insert(__in_z_opt LPCWSTR szSrcW, __in SIZE_T nInsertPosition);
  virtual BOOL InsertN(__in_nz_opt LPCWSTR szSrcW, __in SIZE_T nInsertPosition, __in SIZE_T nSrcLen);
  virtual VOID Delete(__in SIZE_T nStartChar, __in SIZE_T nChars);

  virtual BOOL StartsWith(__in_z LPCWSTR szStrW, __in_opt BOOL bCaseInsensitive=TRUE);
  virtual BOOL EndsWith(__in_z LPCWSTR szStrW, __in_opt BOOL bCaseInsensitive=TRUE);
  virtual LPCWSTR Contains(__in_z LPCWSTR szStrW, __in_opt BOOL bCaseInsensitive=TRUE);

  virtual VOID Attach(__in_z LPWSTR szSrcW);
  virtual LPWSTR Detach();

  virtual BOOL EnsureBuffer(__in SIZE_T nChars);

  WCHAR operator[](__in SIZE_T nIndex) const
    {
    return (szStrW != NULL) ? szStrW[nIndex] : 0;
    };
  WCHAR& operator[](__in SIZE_T nIndex);

  LPSTR ToAnsi();
  static LPSTR Wide2Ansi(__in_z LPCWSTR szStrW, __in SIZE_T nSrcLen);

private:
  LPWSTR szStrW;
  SIZE_T nSize, nLen;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_STRINGS_H
