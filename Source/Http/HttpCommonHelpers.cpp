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
#include "..\..\Include\Http\HttpCommon.h"

//-----------------------------------------------------------

static LPCSTR GetMimeTypeFromExtension(_In_z_ LPCSTR szExtA);
static CHAR UTF16_to_ISO_8859_1(_In_ WCHAR chW);

//-----------------------------------------------------------

namespace MX {

BOOL CHttpCommon::IsValidNameChar(_In_ CHAR chA)
{
  static LPCSTR szInvalidA = "()<>@,;:\\\"/[]?={}";

  if (chA < 0x21 || chA > 0x7E)
    return FALSE;
  if (StrChrA(szInvalidA, chA) != NULL)
    return FALSE;
  return TRUE;
}

/*
//like Google Chrome and IE
BOOL CHttpCommon::EncodeQuotedString(_Inout_ CStringA &cStrA)
{
  SIZE_T nOffset;
  LPSTR sA, szToInsertA;

  sA = (LPSTR)cStrA;
  szToInsertA = NULL;
  while (*sA != 0)
  {
    switch (*sA)
    {
      case '\n':
        szToInsertA = "%0A";
        break;
      case '\r':
        szToInsertA = "%0D";
        break;
      case '"':
        szToInsertA = "%22";
        break;
      case '%':
        szToInsertA = "%25";
        break;
    }
    if (szToInsertA == NULL)
    {
      sA++;
    }
    else
    {
      nOffset = (SIZE_T)(sA - (LPSTR)cStrA);
      cStrA.Delete(nOffset, 1);
      if (cStrA.Insert(szToInsertA, nOffset) == FALSE)
        return FALSE;
      sA = (LPSTR)cStrA + nOffset + StrLenA(szToInsertA);
      szToInsertA = NULL;
    }
  }
  return TRUE;
}
*/

BOOL CHttpCommon::FromISO_8859_1(_Out_ CStringW &cStrDestW, _In_ LPCSTR szStrA, _In_ SIZE_T nStrLen)
{
  WCHAR chW;
  LPCSTR szStrEndA = szStrA + nStrLen;

  cStrDestW.Empty();
  if (cStrDestW.EnsureBuffer(nStrLen) == FALSE)
    return FALSE;
  while (szStrA < szStrEndA)
  {
    chW = (WCHAR)(UCHAR)(*szStrA);
    if (cStrDestW.ConcatN(&chW, 1) == FALSE)
      return FALSE;
    szStrA++;
  }
  return TRUE;
}

BOOL CHttpCommon::ToISO_8859_1(_Out_ CStringA &cStrDestA, _In_ LPCWSTR szStrW, _In_ SIZE_T nStrLen)
{
  CHAR chA;
  LPCWSTR szStrEndW = szStrW + nStrLen;

  cStrDestA.Empty();
  if (cStrDestA.EnsureBuffer(nStrLen) == FALSE)
    return FALSE;
  while (szStrW < szStrEndW)
  {
    if ((chA = UTF16_to_ISO_8859_1(*szStrW)) != '?')
    {
      if (cStrDestA.ConcatN(&chA, 1) == FALSE)
        return FALSE;
    }
    else if (*szStrW == L'?')
    {
      if (cStrDestA.ConcatN("?", 1) == FALSE)
        return FALSE;
    }
    szStrW++;
  }
  return TRUE;
}

BOOL CHttpCommon::BuildQuotedString(_Out_ CStringA &cStrA, _In_ LPCSTR szStrA, _In_ SIZE_T nStrLen,
                                    _In_ BOOL bDontQuoteIfToken)
{
  static LPCSTR szSeparatorsA = "()<>@,;:\\\"/[]?={}";
  LPCSTR szStrEndA = szStrA + nStrLen;
  LPCSTR szStartA, szStrA_Copy = szStrA;

  cStrA.Empty();

  if (bDontQuoteIfToken != FALSE)
  {
    while (szStrA < szStrEndA)
    {
      if (*((UCHAR*)szStrA) < 0x21 || *((UCHAR*)szStrA) > 0x7E)
        break;
      if (StrChrA(szSeparatorsA, *szStrA) != NULL)
        break;
      szStrA++;
    }
    if (szStrA == szStrEndA)
    {
      //it is a token
      return cStrA.CopyN(szStrA_Copy, nStrLen);
    }
    //restore pointer
    szStrA = szStrA_Copy;
  }
  //quotize
  while (szStrA < szStrEndA)
  {
    szStartA = szStrA;
    while (szStrA < szStrEndA && *szStrA != '"' && *szStrA != '\\')
      szStrA++;
    if (szStrA > szStartA)
    {
      if (cStrA.ConcatN(szStartA, (SIZE_T)(szStrA - szStartA)) == FALSE)
        return FALSE;
    }
    if (szStrA < szStrEndA)
    {
      if (cStrA.ConcatN("\\", 1) == FALSE || cStrA.ConcatN(szStrA, 1) == FALSE)
        return FALSE;
      szStrA++;
    }
  }
  //done
  return TRUE;
}

BOOL CHttpCommon::BuildQuotedString(_Out_ CStringA &cStrA, _In_ LPCWSTR szStrW, _In_ SIZE_T nStrLen,
                                    _In_ BOOL bDontQuoteIfToken)
{
  static LPCWSTR szSeparatorsW = L"()<>@,;:\\\"/[]?={}";
  CStringA cStrTempA;
  LPCWSTR szStrEndW = szStrW + nStrLen;
  LPCWSTR szStartW, szStrW_Copy = szStrW;

  cStrA.Empty();

  if (bDontQuoteIfToken != FALSE)
  {
    while (szStrW < szStrEndW)
    {
      if (*szStrW < 0x21 || *szStrW > 0x7E)
        break;
      if (StrChrW(szSeparatorsW, *szStrW) != NULL)
        break;
      szStrW++;
    }
    if (szStrW == szStrEndW)
    {
      //it is a token
      return ToISO_8859_1(cStrA, szStrW_Copy, nStrLen);
    }
    //restore pointer
    szStrW = szStrW_Copy;
  }
  //quotize
  while (szStrW < szStrEndW)
  {
    szStartW = szStrW;
    while (szStrW < szStrEndW && *szStrW != L'"' && *szStrW != L'\\')
      szStrW++;
    if (szStrW > szStartW)
    {
      if (ToISO_8859_1(cStrTempA, szStartW, (SIZE_T)(szStrW - szStartW)) == FALSE ||
          cStrA.ConcatN((LPCSTR)cStrTempA, cStrTempA.GetLength()) == FALSE)
      {
        return FALSE;
      }
    }
    if (szStrW < szStrEndW)
    {
      CHAR chA[2];

      chA[0] = '\\';
      chA[1] = (CHAR)(UCHAR)(*szStrW++);
      if (cStrA.ConcatN(chA, 2) == FALSE)
        return FALSE;
    }
  }
  //done
  return TRUE;
}

BOOL CHttpCommon::BuildExtendedValueString(_Out_ CStringA &cStrA, _In_ LPCWSTR szStrW, _In_ SIZE_T nStrLen)
{
  static const LPCSTR szHexaA = "0123456789ABCDEF";
  LPCSTR sA;
  CHAR szToInsertA[3];
  SIZE_T nOffset;

  if (FAILED(Utf8_Encode(cStrA, szStrW, nStrLen)))
    return FALSE;
  szToInsertA[0] = '%';
  sA = (LPCSTR)cStrA;
  while (*sA != 0)
  {
    if (*sA <= 0x21 || *sA > 0x7E || *sA == '%' || *sA == '"' || *sA == '\'')
    {
      szToInsertA[1] = szHexaA[(*((UCHAR*)sA)) >> 4];
      szToInsertA[2] = szHexaA[(*((UCHAR*)sA)) & 15];
      nOffset = (SIZE_T)(sA - (LPSTR)cStrA);
      cStrA.Delete(nOffset, 1);
      if (cStrA.InsertN(szToInsertA, nOffset, 3) == FALSE)
        return FALSE;
      sA = (LPSTR)cStrA + nOffset + 3;
    }
    else
    {
      sA++;
    }
  }
  //done
  return cStrA.InsertN("UTF-8''", 0, 7);
}

LPCSTR CHttpCommon::GetMimeType(_In_z_ LPCSTR szFileNameA)
{
  LPCSTR szExtA;

  if (szFileNameA == NULL || *szFileNameA == 0)
    return NULL;
  szExtA = szFileNameA + StrLenA(szFileNameA);
  while (szExtA > szFileNameA && *(szExtA-1) != '.' && *(szExtA-1) != '\\' && *(szExtA-1) != '/')
    szExtA--;
  return GetMimeTypeFromExtension((szExtA > szFileNameA && *(szExtA-1) == '.') ? (szExtA-1) : "");
}

LPCSTR CHttpCommon::GetMimeType(_In_z_ LPCWSTR szFileNameW)
{
  LPCWSTR szExtW;
  CHAR szExtA[8];
  SIZE_T i;

  if (szFileNameW == NULL || *szFileNameW == 0)
    return NULL;
  szExtW = szFileNameW + StrLenW(szFileNameW);
  while (szExtW > szFileNameW && *(szExtW-1) != L'.' && *(szExtW-1) != L'\\' && *(szExtW-1) != L'/')
    szExtW--;
  if (szExtW > szFileNameW && *(szExtW-1) == L'.')
  {
    szExtW--;
    for (i=0; i<MX_ARRAYLEN(szExtA) && szExtW[i] != 0; i++)
    {
      if (szExtW[i] > 127)
        break;
      szExtA[i] = (CHAR)(UCHAR)szExtW[i];
    }
    if (i < MX_ARRAYLEN(szExtA) && szExtW[i] == 0)
    {
      szExtA[i] = 0;
      return GetMimeTypeFromExtension(szExtA);
    }
  }
  return GetMimeTypeFromExtension(".");
}

HRESULT CHttpCommon::ParseDate(_Out_ CDateTime &cDt, _In_z_ LPCSTR szDateTimeA)
{
  HRESULT hRes;

  cDt.Clear();
  if (szDateTimeA == NULL)
    return E_POINTER;
  if (*szDateTimeA == 0)
    return MX_E_InvalidData;
  //parse date
  hRes = cDt.SetFromString(szDateTimeA, "%a, %d %b %Y %H:%M:%S %z");
  if (FAILED(hRes))
    hRes = cDt.SetFromString(szDateTimeA, "%A, %d-%b-%Y %H:%M:%S %z");
  if (FAILED(hRes))
    hRes = cDt.SetFromString(szDateTimeA, "%Y.%m.%dT%H:%M");
  if (FAILED(hRes))
    hRes = cDt.SetFromString(szDateTimeA, "%a, %b %d %H:%M:%S %Y %z");
  if (hRes == E_FAIL)
    hRes = MX_E_InvalidData;
  //done
  if (FAILED(hRes))
    cDt.Clear();
  return hRes;
}

HRESULT CHttpCommon::ParseDate(_Out_ CDateTime &cDt, _In_z_ LPCWSTR szDateTimeW)
{
  HRESULT hRes;

  cDt.Clear();
  if (szDateTimeW == NULL)
    return E_POINTER;
  if (*szDateTimeW == 0)
    return MX_E_InvalidData;
  //parse date
  hRes = cDt.SetFromString(szDateTimeW, L"%a, %d %b %Y %H:%M:%S %z");
  if (FAILED(hRes))
    hRes = cDt.SetFromString(szDateTimeW, L"%A, %d-%b-%Y %H:%M:%S %z");
  if (FAILED(hRes))
    hRes = cDt.SetFromString(szDateTimeW, L"%Y.%m.%dT%H:%M");
  if (FAILED(hRes))
    hRes = cDt.SetFromString(szDateTimeW, L"%a, %b %d %H:%M:%S %Y %z");
  if (hRes == E_FAIL)
    hRes = MX_E_InvalidData;
  //done
  if (FAILED(hRes))
    cDt.Clear();
  return hRes;
}

HRESULT CHttpCommon::_GetTempPath(_Out_ MX::CStringW &cStrPathW)
{
  SIZE_T nLen, nThisLen=0;

  for (nLen=2048; nLen<=65536; nLen<<=1)
  {
    if (cStrPathW.EnsureBuffer(nLen) == FALSE)
      return E_OUTOFMEMORY;
    nThisLen = (SIZE_T)::GetTempPathW((DWORD)nLen, (LPWSTR)cStrPathW);
    if (nThisLen < nLen - 4)
      break;
  }
  if (nLen > 65536)
    return ERROR_BUFFER_ALL_ZEROS;
  ((LPWSTR)cStrPathW)[nThisLen] = 0;
  cStrPathW.Refresh();
  if (nThisLen > 0 && ((LPWSTR)cStrPathW)[nThisLen - 1] != L'\\')
  {
    if (cStrPathW.Concat(L"\\") == FALSE)
      return E_OUTOFMEMORY;
  }
  return S_OK;
}

} //namespace MX

//-----------------------------------------------------------

static LPCSTR GetMimeTypeFromExtension(_In_z_ LPCSTR szExtA)
{
  if (*szExtA == '.')
  {
    switch (szExtA[1])
    {
      case 'c':
      case 'C':
        if (MX::StrCompareA(szExtA+2, "ss", TRUE) == 0)
          return "text/css";
        break;

      case 'g':
      case 'G':
        if (MX::StrCompareA(szExtA+2, "if", TRUE) == 0)
          return "image/gif";
        break;

      case 'j':
      case 'J':
        if (MX::StrCompareA(szExtA+2, "s", TRUE) == 0)
          return "application/javascript";
        if (MX::StrCompareA(szExtA+2, "peg", TRUE) == 0 ||
            MX::StrCompareA(szExtA+2, "pg", TRUE) == 0 ||
            MX::StrCompareA(szExtA+2, "pe", TRUE) == 0)
          return "image/jpeg";
        break;

      case 'p':
      case 'P':
        if (MX::StrCompareA(szExtA+2, "ng", TRUE) == 0)
          return "image/png";
        break;

      case 'm':
      case 'M':
        if (MX::StrCompareA(szExtA+2, "pga", TRUE) == 0 ||
            MX::StrCompareA(szExtA+2, "p2", TRUE) == 0 ||
            MX::StrCompareA(szExtA+2, "p3", TRUE) == 0)
          return "audio/mpeg";
        //----
        if (MX::StrCompareA(szExtA+2, "idi", TRUE) == 0)
          return "audio/midi";
        //----
        if (MX::StrCompareA(szExtA+2, "ov", TRUE) == 0)
          return "video/x-quicktime";
        //----
        if (MX::StrCompareA(szExtA+2, "peg", TRUE) == 0 ||
            MX::StrCompareA(szExtA+2, "pg", TRUE) == 0 ||
            MX::StrCompareA(szExtA+2, "pe", TRUE) == 0)
          return "video/mpeg";
        break;

      case 'k':
      case 'K':
        if (MX::StrCompareA(szExtA+2, "ar", TRUE) == 0)
          return "audio/midi";
        break;

      case 'a':
      case 'A':
        if (MX::StrCompareA(szExtA+2, "u", TRUE) == 0)
          return "audio/basic";
        //----
        if (MX::StrCompareA(szExtA+2, "vi", TRUE) == 0)
          return "video/x-msvideo";
        break;

      case 's':
      case 'S':
        if (MX::StrCompareA(szExtA+2, "nd", TRUE) == 0)
          return "audio/basic";
        break;

      case 'r':
      case 'R':
        if (MX::StrCompareA(szExtA+2, "a", TRUE) == 0)
          return "audio/x-realaudio";
        //----
        if (MX::StrCompareA(szExtA+2, "tf", TRUE) == 0)
          return "text/rtf";
        break;

      case 'w':
      case 'W':
        if (MX::StrCompareA(szExtA+2, "av", TRUE) == 0)
          return "audio/x-wav";
        break;

      case 'q':
      case 'Q':
        if (MX::StrCompareA(szExtA+2, "t", TRUE) == 0)
          return "video/x-quicktime";
        break;

      case 'h':
      case 'H':
        if (MX::StrCompareA(szExtA+2, "tm", TRUE) == 0 ||
            MX::StrCompareA(szExtA+2, "tml", TRUE) == 0)
          return "text/html";
        break;

      case 'x':
      case 'X':
        if (MX::StrCompareA(szExtA+2, "ml", TRUE) == 0 ||
            MX::StrCompareA(szExtA+2, "sl", TRUE) == 0)
          return "text/xml";
        break;
    }
  }
  return "application/octet-stream";
}

static CHAR UTF16_to_ISO_8859_1(_In_ WCHAR chW)
{
  if (chW < 0x0100)
    return (CHAR)(UCHAR)chW;
  if (chW < 0x0149)
    return "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIi??JjKk?LlLlLl??LlNnNnNn"[chW - 0x0100];
  if (chW < 0x014C)
    return '?';
  if (chW < 0x017F)
    return "OoOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZz"[chW - 0x014C];
  if (chW >= 0x01CD && chW < 0x01DC)
    return "AaIiOoUuUuUuUuUu"[chW - 0x01CD];
  if (chW >= 0x01E4 && chW < 0x01ED)
    return "GgGgKkOoOo"[chW - 0x01E4];
  switch (chW)
  {
    case 0x0180:
      return 'b';
    case 0x0189:
      return 'D';
    case 0x0191:
      return 'F';
    case 0x0192:
      return 'f';
    case 0x0197:
      return 'I';
    case 0x019A:
      return 'l';
    case 0x019F:
    case 0x01A0:
      return 'O';
    case 0x01A1:
      return 'o';
    case 0x01AB:
      return 't';
    case 0x01AE:
    case 0x2122:
      return 'T';
    case 0x01AF:
      return 'U';
    case 0x01B0:
      return 'u';
    case 0x01B6:
      return 'z';
    case 0x01DE:
      return 'A';
    case 0x01DF:
      return 'a';
    case 0x01F0:
      return 'j';
    case 0x0261:
      return 'g';

    case 0x02B9:
    case 0x030E:
    case 0x201C:
    case 0x201D:
    case 0x201E:
      return '"';

    case 0x02BC:
      return '\'';

    case 0x02C4:
    case 0x02C6:
    case 0x0302:
      return '^';

    case 0x02C8:
    case 0x2018:
    case 0x2019:
    case 0x2032:
      return '\'';

    case 0x02CB:
    case 0x0300:
    case 0x2035:
      return '`';

    case 0x02CD:
    case 0x0331:
    case 0x0332:
      return '_';

    case 0x02DC:
    case 0x0303:
      return '~';

    case 0x2000:
    case 0x2001:
    case 0x2002:
    case 0x2003:
    case 0x2004:
    case 0x2005:
    case 0x2006:
      return ' ';

    case 0x2010:
    case 0x2011:
    case 0x2013:
    case 0x2014:
      return '-';
      return '-';
    case 0x201A:
      return ',';
    case 0x2022:
    case 0x2026:
      return '.';
    case 0x2039:
      return '<';
    case 0x203A:
      return '>';
  }
  if (chW >= 0xFF01 && chW <= 0xFF5E)
  {
    return "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
           "abcdefghijklmnopqrstuvwxyz{|}~"[chW - 0xFF01];
  }
  return '?';
}
