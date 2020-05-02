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
#include "..\..\Include\Http\HttpUtils.h"
#include "..\..\Include\Strings\Utf8.h"

//-----------------------------------------------------------

#define MAX_VERB_LENGTH                                   24
static struct {
  LPCSTR szNameA;
  SIZE_T nNameLen;
} sVerbs[] = {
  { "CHECKOUT", 8 }, { "CONNECT",      7 }, { "COPY",   4 }, { "DELETE",   6 },
  { "GET",      3 }, { "HEAD",         4 }, { "LOCK",   4 }, { "MKCOL",    5 },
  { "MOVE",     4 }, { "MKACTIVITY",  10 }, { "MERGE",  5 }, { "M-SEARCH", 7 },
  { "NOTIFY",   6 }, { "OPTIONS",      7 }, { "PATCH",  5 }, { "POST",     4 },
  { "PROPFIND", 8 }, { "PROPPATCH",    9 }, { "PUT",    3 }, { "PURGE",    5 },
  { "REPORT",   6 }, { "SUBSCRIBE",    9 }, { "SEARCH", 6 }, { "TRACE",    5 },
  { "UNLOCK",   6 }, { "UNSUBSCRIBE", 11 }
};

static const LPCSTR szDefaultMimeTypeA = "application/octet-stream";

//-----------------------------------------------------------

static LPCSTR GetMimeTypeFromExtensionA(_In_z_ LPCSTR szExtA);
static LPCSTR GetMimeTypeFromExtensionW(_In_z_ LPCWSTR szExtW);
static CHAR UTF16_to_ISO_8859_1(_In_ WCHAR chW);

//-----------------------------------------------------------

namespace MX {

namespace Http {

eBrowser GetBrowserFromUserAgent(_In_ LPCSTR szUserAgentA, _In_opt_ SIZE_T nUserAgentLen)
{
  LPCSTR sA;

  if (nUserAgentLen == (SIZE_T)-1)
    nUserAgentLen = StrLenA(szUserAgentA);
  if (szUserAgentA == NULL || nUserAgentLen == 0)
    return BrowserOther;

  sA = StrNFindA(szUserAgentA, "MSIE ", nUserAgentLen);
  if (sA != NULL)
  {
    SIZE_T nOffset = (SIZE_T)(sA - szUserAgentA);

    if (nOffset + 6 < nUserAgentLen && sA[6] == '.')
    {
      switch (sA[5])
      {
        case '4':
        case '5':
          return BrowserIE6;

        case '6':
          if (nOffset + 8 < nUserAgentLen &&
              StrNCompareA(sA + 8, "SV1", nUserAgentLen - (nOffset + 8)) != NULL)
          {
            return BrowserIE6;
          }
          break;
      }
    }
    return BrowserIE;
  }

  sA = StrNFindA(szUserAgentA, "Opera ", nUserAgentLen);
  if (sA != NULL)
    return BrowserOpera;

  sA = StrNFindA(szUserAgentA, "Gecko/", nUserAgentLen);
  if (sA != NULL)
    return BrowserGecko;

  sA = StrNFindA(szUserAgentA, "Chrome/", nUserAgentLen);
  if (sA != NULL)
    return BrowserChrome;

  sA = StrNFindA(szUserAgentA, "Safari/", nUserAgentLen);
  if (sA != NULL)
    return BrowserSafari;
  sA = StrNFindA(szUserAgentA, "Mac OS X", nUserAgentLen);
  if (sA != NULL)
    return BrowserSafari;

  sA = StrNFindA(szUserAgentA, "Konqueror", nUserAgentLen);
  if (sA != NULL)
    return BrowserKonqueror;

  return BrowserOther;
}

LPCSTR IsValidVerb(_In_ LPCSTR szStrA, _In_ SIZE_T nStrLen)
{
  if (szStrA != NULL && nStrLen > 0)
  {
    for (SIZE_T nMethod = 0; nMethod < MX_ARRAYLEN(sVerbs); nMethod++)
    {
      if (nStrLen == sVerbs[nMethod].nNameLen && StrNCompareA(szStrA, sVerbs[nMethod].szNameA, nStrLen, FALSE) == 0)
        return sVerbs[nMethod].szNameA;
    }
  }
  return NULL;
}

BOOL IsValidNameChar(_In_ CHAR chA)
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
BOOL EncodeQuotedString(_Inout_ CStringA &cStrA)
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

BOOL FromISO_8859_1(_Out_ CStringW &cStrDestW, _In_ LPCSTR szStrA, _In_ SIZE_T nStrLen)
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

BOOL ToISO_8859_1(_Out_ CStringA &cStrDestA, _In_ LPCWSTR szStrW, _In_ SIZE_T nStrLen)
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

BOOL BuildQuotedString(_Out_ CStringA &cStrA, _In_ LPCSTR szStrA, _In_ SIZE_T nStrLen, _In_ BOOL bDontQuoteIfToken)
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

BOOL BuildQuotedString(_Out_ CStringA &cStrA, _In_ LPCWSTR szStrW, _In_ SIZE_T nStrLen, _In_ BOOL bDontQuoteIfToken)
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

BOOL BuildExtendedValueString(_Out_ CStringA &cStrA, _In_ LPCWSTR szStrW, _In_ SIZE_T nStrLen)
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

LPCSTR GetMimeType(_In_z_ LPCSTR szFileNameA)
{
  LPCSTR szExtA;

  if (szFileNameA == NULL || *szFileNameA == 0)
    return NULL;

  szExtA = szFileNameA + StrLenA(szFileNameA);
  while (szExtA > szFileNameA && *(szExtA - 1) != '.' && *(szExtA - 1) != '\\' && *(szExtA - 1) != '/')
  {
    szExtA--;
  }
  if (szExtA > szFileNameA && *(szExtA - 1) == '.')
    return GetMimeTypeFromExtensionA(szExtA);
  return szDefaultMimeTypeA;
}

LPCSTR GetMimeType(_In_z_ LPCWSTR szFileNameW)
{
  LPCWSTR szExtW;

  if (szFileNameW == NULL || *szFileNameW == 0)
    return NULL;

  szExtW = szFileNameW + StrLenW(szFileNameW);
  while (szExtW > szFileNameW && *(szExtW - 1) != L'.' && *(szExtW - 1) != L'\\' && *(szExtW - 1) != L'/')
  {
    szExtW--;
  }
  if (szExtW > szFileNameW && *(szExtW - 1) == L'.')
    return GetMimeTypeFromExtensionW(szExtW);
  return szDefaultMimeTypeA;
}

HRESULT ParseDate(_Out_ CDateTime &cDt, _In_z_ LPCSTR szDateTimeA, _In_opt_ SIZE_T nDateTimeLen)
{
  CStringA cStrTempA;
  HRESULT hRes;

  cDt.Clear();
  if (szDateTimeA == NULL)
    return E_POINTER;

  if (nDateTimeLen == (SIZE_T)-1)
  {
    if (StrLenA(szDateTimeA) == 0)
      return MX_E_InvalidData;
  }
  else if (nDateTimeLen > 0)
  {
    if (cStrTempA.CopyN(szDateTimeA, nDateTimeLen) == FALSE)
      return E_OUTOFMEMORY;
    szDateTimeA = (LPCSTR)cStrTempA;
  }
  else
  {
    return MX_E_InvalidData;
  }

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

HRESULT ParseDate(_Out_ CDateTime &cDt, _In_z_ LPCWSTR szDateTimeW, _In_opt_ SIZE_T nDateTimeLen)
{
  CStringW cStrTempW;
  HRESULT hRes;

  cDt.Clear();
  if (szDateTimeW == NULL)
    return E_POINTER;

  if (nDateTimeLen == (SIZE_T)-1)
  {
    if (StrLenW(szDateTimeW) == 0)
      return MX_E_InvalidData;
  }
  else if (nDateTimeLen > 0)
  {
    if (cStrTempW.CopyN(szDateTimeW, nDateTimeLen) == FALSE)
      return E_OUTOFMEMORY;
    szDateTimeW = (LPCWSTR)cStrTempW;
  }
  else
  {
    return MX_E_InvalidData;
  }

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

} //namespace Http

} //namespace MX

//-----------------------------------------------------------

static LPCSTR GetMimeTypeFromExtensionA(_In_z_ LPCSTR szExtA)
{
#include "HttpMimeFromExtension.h"
  CHAR szExtLcA[8];
  SIZE_T i;

  for (i = 0; i < MX_ARRAYLEN(szExtA) && szExtA[i] != 0; i++)
  {
    szExtLcA[i] = MX::CharToLowerA(szExtA[i]);
  }
  if (i > 0 && i < MX_ARRAYLEN(szExtLcA) && szExtA[i] == 0)
  {
    SIZE_T nBase = 0;
    SIZE_T n = MX_ARRAYLEN(aMimeTypes);

    szExtLcA[i] = 0;
    while (n > 0)
    {
      SIZE_T nMid = nBase + (n >> 1);

      int comp = MX::StrCompareA(szExtA, aMimeTypes[nMid].szExtensionA);
      if (comp == 0)
        return aMimeTypes[nMid].szMimeTypeA;
      if (comp > 0)
      {
        nBase = nMid + 1;
        n--;
      }
      n >>= 1;
    }
  }
  return szDefaultMimeTypeA;
}

static LPCSTR GetMimeTypeFromExtensionW(_In_z_ LPCWSTR szExtW)
{
  CHAR szExtA[8];
  SIZE_T i;

  for (i = 0; i < MX_ARRAYLEN(szExtA) && szExtW[i] != 0; i++)
  {
    if (szExtW[i] > 127)
      break;
    szExtA[i] = (CHAR)(UCHAR)szExtW[i];
  }
  if (i >= MX_ARRAYLEN(szExtA) || szExtW[i] != 0)
    return szDefaultMimeTypeA;
  szExtA[i] = 0;
  return GetMimeTypeFromExtensionA(szExtA);
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
