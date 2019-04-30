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

SIZE_T StrLenA(_In_opt_z_ LPCSTR szSrcA);
SIZE_T StrLenW(_In_opt_z_ LPCWSTR szSrcW);

int StrCompareA(_In_z_ LPCSTR szSrcA1, _In_z_ LPCSTR szSrcA2, _In_opt_ BOOL bCaseInsensitive=FALSE);
int StrCompareW(_In_z_ LPCWSTR szSrcW1, _In_z_ LPCWSTR szSrcW2, _In_opt_ BOOL bCaseInsensitive=FALSE);
int StrNCompareA(_In_z_ LPCSTR szSrcA1, _In_z_ LPCSTR szSrcA2, _In_ SIZE_T nLen,
                 _In_opt_ BOOL bCaseInsensitive=FALSE);
int StrNCompareW(_In_z_ LPCWSTR szSrcW1, _In_z_ LPCWSTR szSrcW2, _In_ SIZE_T nLen,
                 _In_opt_ BOOL bCaseInsensitive=FALSE);
int StrCompareAW(_In_z_ LPCSTR szSrcA1, _In_z_ LPCWSTR szSrcW2, _In_opt_ BOOL bCaseInsensitive=FALSE);
int StrNCompareAW(_In_z_ LPCSTR szSrcA1, _In_z_ LPCWSTR szSrcW2, _In_ SIZE_T nLen,
                  _In_opt_ BOOL bCaseInsensitive=FALSE);

LPCSTR StrChrA(_In_z_ LPCSTR szSrcA, _In_ CHAR chA, _In_opt_ BOOL bReverse=FALSE);
LPCWSTR StrChrW(_In_z_ LPCWSTR szSrcW, _In_ WCHAR chW, _In_opt_ BOOL bReverse=FALSE);
LPCSTR StrNChrA(_In_z_ LPCSTR szSrcA, _In_ CHAR chA, _In_ SIZE_T nLen, _In_opt_ BOOL bReverse=FALSE);
LPCWSTR StrNChrW(_In_z_ LPCWSTR szSrcW, _In_ WCHAR chW, _In_ SIZE_T nLen, _In_opt_ BOOL bReverse=FALSE);

LPCSTR StrFindA(_In_z_ LPCSTR szSrcA, _In_z_ LPCSTR szToFindA, _In_opt_ BOOL bReverse=FALSE,
                _In_opt_ BOOL bCaseInsensitive=FALSE);
LPCWSTR StrFindW(_In_z_ LPCWSTR szSrcW, _In_z_ LPCWSTR szToFindW, _In_opt_ BOOL bReverse=FALSE,
                 _In_opt_ BOOL bCaseInsensitive=FALSE);
LPCSTR StrNFindA(_In_z_ LPCSTR szSrcA, _In_z_ LPCSTR szToFindA, _In_ SIZE_T nLen, _In_opt_ BOOL bReverse=FALSE,
                 _In_opt_ BOOL bCaseInsensitive=FALSE);
LPCWSTR StrNFindW(_In_z_ LPCWSTR szSrcW, _In_z_ LPCWSTR szToFindW, _In_ SIZE_T nLen, _In_opt_ BOOL bReverse=FALSE,
                  _In_opt_ BOOL bCaseInsensitive=FALSE);

CHAR CharToLowerA(_In_ CHAR chA);
CHAR CharToUpperA(_In_ CHAR chA);
WCHAR CharToLowerW(_In_ WCHAR chW);
WCHAR CharToUpperW(_In_ WCHAR chW);

VOID StrToLowerA(_Inout_z_ LPSTR szSrcA);
VOID StrNToLowerA(_Inout_updates_(nLen) LPSTR szSrcA, _In_ SIZE_T nLen);
VOID StrToLowerW(_Inout_z_ LPWSTR szSrcW);
VOID StrNToLowerW(_Inout_updates_(nLen) LPWSTR szSrcW, _In_ SIZE_T nLen);
VOID StrToUpperA(_Inout_z_ LPSTR szSrcA);
VOID StrNToUpperA(_Inout_updates_(nLen) LPSTR szSrcA, _In_ SIZE_T nLen);
VOID StrToUpperW(_Inout_z_ LPWSTR szSrcW);
VOID StrNToUpperW(_Inout_updates_(nLen) LPWSTR szSrcW, _In_ SIZE_T nLen);

HRESULT StrToDoubleA(_In_ LPCSTR szSrcA, _In_ SIZE_T nLen, double *lpnValue);
HRESULT StrToDoubleW(_In_ LPCWSTR szSrcW, _In_ SIZE_T nLen, double *lpnValue);

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

  virtual BOOL Copy(_In_opt_z_ LPCSTR szSrcA);
  virtual BOOL CopyN(_In_reads_or_z_opt_(nSrcLen) LPCSTR szSrcA, _In_ SIZE_T nSrcLen);
  virtual BOOL Concat(_In_opt_z_ LPCSTR szSrcA);
  virtual BOOL ConcatN(_In_reads_or_z_opt_(nSrcLen) LPCSTR szSrcA, _In_ SIZE_T nSrcLen);

  virtual BOOL Copy(_In_opt_z_ LPCWSTR szSrcW);
  virtual BOOL CopyN(_In_reads_or_z_opt_(nSrcLen) LPCWSTR szSrcW, _In_ SIZE_T nSrcLen);
  virtual BOOL Concat(_In_opt_z_ LPCWSTR szSrcW);
  virtual BOOL ConcatN(_In_reads_or_z_opt_(nSrcLen) LPCWSTR szSrcW, _In_ SIZE_T nSrcLen);

  virtual BOOL Copy(_In_ PCMX_ANSI_STRING pASStr);
  virtual BOOL Concat(_In_ PCMX_ANSI_STRING pASStr);

  virtual BOOL Copy(_In_ LONG nSrc);
  virtual BOOL Concat(_In_ LONG nSrc);
  virtual BOOL Copy(_In_ ULONG nSrc);
  virtual BOOL Concat(_In_ ULONG nSrc);

  virtual BOOL Copy(_In_ LONGLONG nSrc);
  virtual BOOL Concat(_In_ LONGLONG nSrc);
  virtual BOOL Copy(_In_ ULONGLONG nSrc);
  virtual BOOL Concat(_In_ ULONGLONG nSrc);

  virtual BOOL Format(_In_z_ _Printf_format_string_ LPCSTR szFormatA, ...);
  virtual BOOL Format(_In_z_ _Printf_format_string_ LPCWSTR szFormatW, ...);
  virtual BOOL FormatV(_In_z_ _Printf_format_string_params_(1) LPCSTR szFormatA, _In_ va_list argptr);
  virtual BOOL FormatV(_In_z_ _Printf_format_string_params_(1) LPCWSTR szFormatW, _In_ va_list argptr);

  virtual BOOL AppendFormat(_In_z_ _Printf_format_string_ LPCSTR szFormatA, ...);
  virtual BOOL AppendFormat(_In_z_ _Printf_format_string_ LPCWSTR szFormatW, ...);
  virtual BOOL AppendFormatV(_In_z_ _Printf_format_string_params_(1) LPCSTR szFormatA, _In_ va_list argptr);
  virtual BOOL AppendFormatV(_In_z_ _Printf_format_string_params_(1) LPCWSTR szFormatW, _In_ va_list argptr);

  virtual BOOL Insert(_In_opt_z_ LPCSTR szSrcA, _In_ SIZE_T nInsertPosition);
  virtual BOOL InsertN(_In_reads_or_z_opt_(nSrcLen) LPCSTR szSrcA, _In_ SIZE_T nInsertPosition, _In_ SIZE_T nSrcLen);
  virtual VOID Delete(_In_ SIZE_T nStartChar, _In_ SIZE_T nChars);

  virtual BOOL StartsWith(_In_z_ LPCSTR szStrA, _In_opt_ BOOL bCaseInsensitive=TRUE);
  virtual BOOL EndsWith(_In_z_ LPCSTR szStrA, _In_opt_ BOOL bCaseInsensitive=TRUE);
  virtual LPCSTR Contains(_In_z_ LPCSTR szStrA, _In_opt_ BOOL bCaseInsensitive=TRUE);

  virtual VOID Attach(_In_z_ LPSTR szSrcA);
  virtual LPSTR Detach();

  virtual BOOL EnsureBuffer(_In_ SIZE_T nChars);

  virtual CHAR operator[](_In_ SIZE_T nIndex) const
    {
    return (szStrA != NULL) ? szStrA[nIndex] : 0;
    };
  virtual CHAR& operator[](_In_ SIZE_T nIndex);

  LPWSTR ToWide();
  static LPWSTR Ansi2Wide(_In_z_ LPCSTR szStrA, _In_ SIZE_T nSrcLen);

protected:
  LPSTR szStrA;
  SIZE_T nSize, nLen;
};

//--------

class CSecureStringA : public CStringA
{
  MX_DISABLE_COPY_CONSTRUCTOR(CSecureStringA);
public:
  CSecureStringA() : CStringA()
    { };

  VOID Empty();

  BOOL Concat(_In_ LONGLONG nSrc);
  BOOL Concat(_In_ ULONGLONG nSrc);

  BOOL AppendFormatV(_In_z_ _Printf_format_string_params_(1) LPCWSTR szFormatW, _In_ va_list argptr);
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

  virtual BOOL Copy(_In_opt_z_ LPCSTR szSrcA);
  virtual BOOL CopyN(_In_reads_or_z_opt_(nSrcLen) LPCSTR szSrcA, _In_ SIZE_T nSrcLen);
  virtual BOOL Concat(_In_opt_z_ LPCSTR szSrcA);
  virtual BOOL ConcatN(_In_reads_or_z_opt_(nSrcLen) LPCSTR szSrcA, _In_ SIZE_T nSrcLen);

  virtual BOOL Copy(_In_opt_z_ LPCWSTR szSrcW);
  virtual BOOL CopyN(_In_reads_or_z_opt_(nSrcLen) LPCWSTR szSrcW, _In_ SIZE_T nSrcLen);
  virtual BOOL Concat(_In_opt_z_ LPCWSTR szSrcW);
  virtual BOOL ConcatN(_In_reads_or_z_opt_(nSrcLen) LPCWSTR szSrcW, _In_ SIZE_T nSrcLen);

  virtual BOOL Copy(_In_ PCMX_UNICODE_STRING pUSStr);
  virtual BOOL Concat(_In_ PCMX_UNICODE_STRING pUSStr);

  virtual BOOL Copy(_In_ LONG nSrc);
  virtual BOOL Concat(_In_ LONG nSrc);
  virtual BOOL Copy(_In_ ULONG nSrc);
  virtual BOOL Concat(_In_ ULONG nSrc);

  virtual BOOL Copy(_In_ LONGLONG nSrc);
  virtual BOOL Concat(_In_ LONGLONG nSrc);
  virtual BOOL Copy(_In_ ULONGLONG nSrc);
  virtual BOOL Concat(_In_ ULONGLONG nSrc);

  virtual BOOL Format(_In_z_ _Printf_format_string_ LPCSTR szFormatA, ...);
  virtual BOOL Format(_In_z_ _Printf_format_string_ LPCWSTR szFormatW, ...);
  virtual BOOL FormatV(_In_z_ _Printf_format_string_params_(1) LPCSTR szFormatA, _In_ va_list argptr);
  virtual BOOL FormatV(_In_z_ _Printf_format_string_params_(1) LPCWSTR szFormatW, _In_ va_list argptr);

  virtual BOOL AppendFormat(_In_z_ _Printf_format_string_ LPCSTR szFormatA, ...);
  virtual BOOL AppendFormat(_In_z_ _Printf_format_string_ LPCWSTR szFormatW, ...);
  virtual BOOL AppendFormatV(_In_z_ _Printf_format_string_params_(1) LPCSTR szFormatA, _In_ va_list argptr);
  virtual BOOL AppendFormatV(_In_z_ _Printf_format_string_params_(1) LPCWSTR szFormatW, _In_ va_list argptr);

  virtual BOOL Insert(_In_opt_z_ LPCWSTR szSrcW, _In_ SIZE_T nInsertPosition);
  virtual BOOL InsertN(_In_reads_or_z_opt_(nSrcLen) LPCWSTR szSrcW, _In_ SIZE_T nInsertPosition, _In_ SIZE_T nSrcLen);
  virtual VOID Delete(_In_ SIZE_T nStartChar, _In_ SIZE_T nChars);

  virtual BOOL StartsWith(_In_z_ LPCWSTR szStrW, _In_opt_ BOOL bCaseInsensitive=TRUE);
  virtual BOOL EndsWith(_In_z_ LPCWSTR szStrW, _In_opt_ BOOL bCaseInsensitive=TRUE);
  virtual LPCWSTR Contains(_In_z_ LPCWSTR szStrW, _In_opt_ BOOL bCaseInsensitive=TRUE);

  virtual VOID Attach(_In_z_ LPWSTR szSrcW);
  virtual LPWSTR Detach();

  virtual BOOL EnsureBuffer(_In_ SIZE_T nChars);

  WCHAR operator[](_In_ SIZE_T nIndex) const
    {
    return (szStrW != NULL) ? szStrW[nIndex] : 0;
    };
  WCHAR& operator[](_In_ SIZE_T nIndex);

  LPSTR ToAnsi();
  static LPSTR Wide2Ansi(_In_z_ LPCWSTR szStrW, _In_ SIZE_T nSrcLen);

protected:
  LPWSTR szStrW;
  SIZE_T nSize, nLen;
};

//--------

class CSecureStringW : public CStringW
{
  MX_DISABLE_COPY_CONSTRUCTOR(CSecureStringW);
public:
  CSecureStringW() : CStringW()
    { };

  VOID Empty();

  BOOL Concat(_In_ LONGLONG nSrc);
  BOOL Concat(_In_ ULONGLONG nSrc);

  BOOL AppendFormatV(_In_z_ _Printf_format_string_params_(1) LPCSTR szFormatA, _In_ va_list argptr);
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_STRINGS_H
