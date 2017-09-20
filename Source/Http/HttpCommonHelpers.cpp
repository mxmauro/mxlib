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
#include <search.h>

//-----------------------------------------------------------

typedef struct {
  LPCSTR szNameA;
  WCHAR chCharCodeW;
} HTMLENTITY_DEF;

#define DEFINE_ENTITY(_name, _char) { _name, _char }
  static HTMLENTITY_DEF aHtmlEntities[] = {
    #include "HtmlEntities.inl"
  };
#undef DEFINE_ENTITY

//-----------------------------------------------------------

static LPCSTR GetMimeTypeFromExtension(__in_z LPCSTR szExtA);
static int HtmlEntities_Compare(void *, const void *key, const void *datum);

//-----------------------------------------------------------

namespace MX {

BOOL CHttpCommon::IsValidNameChar(__in CHAR chA)
{
  static LPCSTR szInvalidA = "()<>@,;:\\\"/[]?={}";
  int i;

  if (chA < 0x21 && chA > 0x7E)
    return FALSE;
  for (i=0; szInvalidA[i]!=0; i++)
  {
    if (chA == szInvalidA[i])
      return FALSE;
  }
  return TRUE;
}

//like Google Chrome and IE
BOOL CHttpCommon::EncodeQuotedString(__inout CStringA &cStrA)
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

LPCSTR CHttpCommon::GetMimeType(__in_z LPCSTR szFileNameA)
{
  LPCSTR szExtA;

  if (szFileNameA == NULL || *szFileNameA == 0)
    return NULL;
  szExtA = szFileNameA + StrLenA(szFileNameA);
  while (szExtA > szFileNameA && *(szExtA-1) != '.' && *(szExtA-1) != '\\' && *(szExtA-1) != '/')
    szExtA--;
  return GetMimeTypeFromExtension((szExtA > szFileNameA && *(szExtA-1) == '.') ? (szExtA-1) : "");
}

LPCSTR CHttpCommon::GetMimeType(__in_z LPCWSTR szFileNameW)
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

HRESULT CHttpCommon::ParseDate(__out CDateTime &cDt, __in_z LPCSTR szDateTimeA)
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
    hRes = cDt.SetFromString(szDateTimeA, "%A, %d-%b-%y %H:%M:%S %z");
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

LPCSTR CHttpCommon::GetHtmlEntity(__in WCHAR chW)
{
  HTMLENTITY_DEF *lpDef;

  lpDef = (HTMLENTITY_DEF*)bsearch_s(&chW, aHtmlEntities, MX_ARRAYLEN(aHtmlEntities), sizeof(HTMLENTITY_DEF),
                                     &HtmlEntities_Compare, NULL);
  return (lpDef != NULL) ? lpDef->szNameA : NULL;
}

HRESULT CHttpCommon::ToHtmlEntities(__inout CStringA &cStrA)
{
  CStringW cStrTempW;
  HRESULT hRes;

  hRes = Utf8_Decode(cStrTempW, (LPCSTR)cStrA, cStrA.GetLength());
  if (SUCCEEDED(hRes))
    hRes = ToHtmlEntities(cStrTempW);
  if (SUCCEEDED(hRes))
    hRes = MX::Utf8_Encode(cStrA, (LPCWSTR)cStrTempW, cStrTempW.GetLength());
  return hRes;
}

HRESULT CHttpCommon::ToHtmlEntities(__inout CStringW &cStrW)
{
  static const LPCWSTR szHexaNumW = L"0123456789ABCDEF";
  WCHAR szTempBufW[40];
  LPCSTR szBufA;
  LPCWSTR sW, szReplacementW;
  SIZE_T i, nOfs;

  sW = (LPCWSTR)cStrW;
  while (*sW != 0)
  {
    szReplacementW = NULL;
    szBufA = MX::CHttpCommon::GetHtmlEntity(*sW);
    if (szBufA != NULL)
    {
      //quick convert
      szTempBufW[0] = L'&';
      for (i=0; szBufA[i]!=0 && i<32; i++)
        szTempBufW[i+1] = (WCHAR)(UCHAR)szBufA[i];
      szTempBufW[i+1] = ';';
      szTempBufW[i+2] = 0;
      szReplacementW = szTempBufW;
    }
    else if (*sW < 32)
    {
      szTempBufW[0] = L'&';
      szTempBufW[1] = L'#';
      szTempBufW[2] = L'x';
      szTempBufW[3] = szHexaNumW[((ULONG)(*sW) >> 4) & 0x0F];
      szTempBufW[4] = szHexaNumW[(ULONG)(*sW)       & 0x0F];
      szTempBufW[5] = L';';
      szTempBufW[6] = 0;
      szReplacementW = szTempBufW;
    }
    if (szReplacementW != NULL)
    {
      nOfs = (SIZE_T)(sW - (LPCWSTR)cStrW);
      cStrW.Delete(nOfs, 1);
      if (cStrW.Insert(szReplacementW, nOfs) == FALSE)
        return E_OUTOFMEMORY;
      sW = (LPCWSTR)cStrW + (nOfs + MX::StrLenW(szReplacementW));
    }
    else
    {
      sW++;
    }
  }
  //done
  return S_OK;
}

WCHAR DecodeHtmlEntity(__inout LPCSTR &szStrA)
{
  if (szStrA == NULL || *szStrA != '&')
    return 0;
  if (szStrA[1] == '#')
  {
    SIZE_T nOfs = 2, nBase = 10, nDigit;
    WCHAR chW = 0, chTempW;

    if (szStrA[2] == 'x' || szStrA[2] == 'X')
    {
      nBase = 16;
      nOfs++;
    }
    while (szStrA[nOfs] != 0 && szStrA[nOfs] != ';')
    {
      if (szStrA[nOfs] >= '0' && szStrA[nOfs] <= '9')
        nDigit = (SIZE_T)(szStrA[nOfs] - '0');
      else if (szStrA[nOfs] >= 'A' && szStrA[nOfs] <= 'F')
        nDigit = (SIZE_T)(szStrA[nOfs] - 'A') + 10;
      else if (szStrA[nOfs] >= 'a' && szStrA[nOfs] <= 'f')
        nDigit = (SIZE_T)(szStrA[nOfs] - 'a') + 10;
      else
        return 0; //invalid digit
      if (nDigit >= nBase)
        return 0; //invalid digit
      //check overflow
      chTempW = chW * (WCHAR)nBase;
      if (chTempW < chW)
        return 0;
      chW = chTempW + (WCHAR)nDigit;
      if (chW < chTempW)
        return 0;
      //advance
      nOfs++;
    }
    //has value?
    if (szStrA[nOfs-1] != '#' && szStrA[nOfs-1] != 'x' && szStrA[nOfs-1] != 'X')
    {
      szStrA += nOfs;
      if (*szStrA == ';')
        szStrA++;
      return chW;
    }
  }
  else
  {
    LPCSTR sA;
    SIZE_T i, j;

    for (i=0; i<sizeof(aHtmlEntities) / sizeof(aHtmlEntities[0]); i++)
    {
      sA = aHtmlEntities[i].szNameA;
      for (j=0; sA[j] != 0 && szStrA[j+1] == sA[j]; j++);
      if (sA[j] == 0 && szStrA[j+1] == ';')
      {
        szStrA += j + 2;
        return aHtmlEntities[i].chCharCodeW;
      }
    }
  }
  return 0;
}

WCHAR DecodeHtmlEntity(__inout LPCWSTR &szStrW)
{
  if (szStrW == NULL || *szStrW != L'&')
    return 0;
  if (szStrW[1] == L'#')
  {
    SIZE_T nOfs = 2, nBase = 10, nDigit;
    WCHAR chW = 0, chTempW;

    if (szStrW[2] == L'x' || szStrW[2] == L'X')
    {
      nBase = 16;
      nOfs++;
    }
    while (szStrW[nOfs] != 0 && szStrW[nOfs] != L';')
    {
      if (szStrW[nOfs] >= L'0' && szStrW[nOfs] <= L'9')
        nDigit = (SIZE_T)(szStrW[nOfs] - L'0');
      else if (szStrW[nOfs] >= L'A' && szStrW[nOfs] <= L'F')
        nDigit = (SIZE_T)(szStrW[nOfs] - L'A') + 10;
      else if (szStrW[nOfs] >= L'a' && szStrW[nOfs] <= L'f')
        nDigit = (SIZE_T)(szStrW[nOfs] - L'a') + 10;
      else
        return 0; //invalid digit
      if (nDigit >= nBase)
        return 0; //invalid digit
      //check overflow
      chTempW = chW * (WCHAR)nBase;
      if (chTempW < chW)
        return 0;
      chW = chTempW + (WCHAR)nDigit;
      if (chW < chTempW)
        return 0;
      //advance
      nOfs++;
    }
    //has value?
    if (szStrW[nOfs-1] != L'#' && szStrW[nOfs-1] != L'x' && szStrW[nOfs-1] != L'X')
    {
      szStrW += nOfs;
      if (*szStrW == L';')
        szStrW++;
      return chW;
    }
  }
  else
  {
    LPCSTR sA;
    SIZE_T i, j;

    for (i=0; i<sizeof(aHtmlEntities) / sizeof(aHtmlEntities[0]); i++)
    {
      sA = aHtmlEntities[i].szNameA;
      for (j=0; sA[j] != 0 && szStrW[j+1] == (WCHAR)((UCHAR)sA[j]); j++);
      if (sA[j] == 0 && szStrW[j+1] == L';')
      {
        szStrW += j + 2;
        return aHtmlEntities[i].chCharCodeW;
      }
    }
  }
  return 0;
}

} //namespace MX

//-----------------------------------------------------------

static LPCSTR GetMimeTypeFromExtension(__in_z LPCSTR szExtA)
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

static int HtmlEntities_Compare(void *, const void *key, const void *datum)
{
  if (*((WCHAR*)key) < ((HTMLENTITY_DEF*)datum)->chCharCodeW)
    return -1;
  if (*((WCHAR*)key) > ((HTMLENTITY_DEF*)datum)->chCharCodeW)
    return 1;
  return 0;
}
