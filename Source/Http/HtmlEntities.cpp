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
#include "..\..\Include\Http\HtmlEntities.h"
#include "..\..\Include\Strings\Utf8.h"
#include <search.h>

//-----------------------------------------------------------

typedef struct {
  LPCSTR szNameA;
  WCHAR chCharCodeW;
} HTMLENTITY_DEF;

//-----------------------------------------------------------

static HTMLENTITY_DEF aHtmlEntities[] = {
  { "quot",       34 },
  { "amp",        38 },
  { "apos",       39 },
  { "lt",         60 },
  { "gt",         62 },
  { "nbsp",      160 },
  { "iexcl",     161 },
  { "cent",      162 },
  { "pound",     163 },
  { "curren",    164 },
  { "yen",       165 },
  { "brvbar",    166 },
  { "sect",      167 },
  { "uml",       168 },
  { "copy",      169 },
  { "ordf",      170 },
  { "laquo",     171 },
  { "not",       172 },
  { "shy",       173 },
  { "reg",       174 },
  { "macr",      175 },
  { "deg",       176 },
  { "plusmn",    177 },
  { "sup2",      178 },
  { "sup3",      179 },
  { "acute",     180 },
  { "micro",     181 },
  { "para",      182 },
  { "middot",    183 },
  { "cedil",     184 },
  { "sup1",      185 },
  { "ordm",      186 },
  { "raquo",     187 },
  { "frac14",    188 },
  { "frac12",    189 },
  { "frac34",    190 },
  { "iquest",    191 },
  { "Agrave",    192 },
  { "Aacute",    193 },
  { "Acirc",     194 },
  { "Atilde",    195 },
  { "Auml",      196 },
  { "Aring",     197 },
  { "AElig",     198 },
  { "Ccedil",    199 },
  { "Egrave",    200 },
  { "Eacute",    201 },
  { "Ecirc",     202 },
  { "Euml",      203 },
  { "Igrave",    204 },
  { "Iacute",    205 },
  { "Icirc",     206 },
  { "Iuml",      207 },
  { "ETH",       208 },
  { "Ntilde",    209 },
  { "Ograve",    210 },
  { "Oacute",    211 },
  { "Ocirc",     212 },
  { "Otilde",    213 },
  { "Ouml",      214 },
  { "times",     215 },
  { "Oslash",    216 },
  { "Ugrave",    217 },
  { "Uacute",    218 },
  { "Ucirc",     219 },
  { "Uuml",      220 },
  { "Yacute",    221 },
  { "THORN",     222 },
  { "szlig",     223 },
  { "agrave",    224 },
  { "aacute",    225 },
  { "acirc",     226 },
  { "atilde",    227 },
  { "auml",      228 },
  { "aring",     229 },
  { "aelig",     230 },
  { "ccedil",    231 },
  { "egrave",    232 },
  { "eacute",    233 },
  { "ecirc",     234 },
  { "euml",      235 },
  { "igrave",    236 },
  { "iacute",    237 },
  { "icirc",     238 },
  { "iuml",      239 },
  { "eth",       240 },
  { "ntilde",    241 },
  { "ograve",    242 },
  { "oacute",    243 },
  { "ocirc",     244 },
  { "otilde",    245 },
  { "ouml",      246 },
  { "divide",    247 },
  { "oslash",    248 },
  { "ugrave",    249 },
  { "uacute",    250 },
  { "ucirc",     251 },
  { "uuml",      252 },
  { "yacute",    253 },
  { "thorn",     254 },
  { "yuml",      255 },
  { "OElig",     338 },
  { "oelig",     339 },
  { "Scaron",    352 },
  { "scaron",    353 },
  { "Yuml",      376 },
  { "fnof",      402 },
  { "circ",      710 },
  { "tilde",     732 },
  { "Alpha",     913 },
  { "Beta",      914 },
  { "Gamma",     915 },
  { "Delta",     916 },
  { "Epsilon",   917 },
  { "Zeta",      918 },
  { "Eta",       919 },
  { "Theta",     920 },
  { "Iota",      921 },
  { "Kappa",     922 },
  { "Lambda",    923 },
  { "Mu",        924 },
  { "Nu",        925 },
  { "Xi",        926 },
  { "Omicron",   927 },
  { "Pi",        928 },
  { "Rho",       929 },
  { "Sigma",     931 },
  { "Tau",       932 },
  { "Upsilon",   933 },
  { "Phi",       934 },
  { "Chi",       935 },
  { "Psi",       936 },
  { "Omega",     937 },
  { "alpha",     945 },
  { "beta",      946 },
  { "gamma",     947 },
  { "delta",     948 },
  { "epsilon",   949 },
  { "zeta",      950 },
  { "eta",       951 },
  { "theta",     952 },
  { "iota",      953 },
  { "kappa",     954 },
  { "lambda",    955 },
  { "mu",        956 },
  { "nu",        957 },
  { "xi",        958 },
  { "omicron",   959 },
  { "pi",        960 },
  { "rho",       961 },
  { "sigmaf",    962 },
  { "sigma",     963 },
  { "tau",       964 },
  { "upsilon",   965 },
  { "phi",       966 },
  { "chi",       967 },
  { "psi",       968 },
  { "omega",     969 },
  { "thetasym",  977 },
  { "upsih",     978 },
  { "piv",       982 },
  { "ensp",     8194 },
  { "emsp",     8195 },
  { "thinsp",   8201 },
  { "zwnj",     8204 },
  { "zwj",      8205 },
  { "lrm",      8206 },
  { "rlm",      8207 },
  { "ndash",    8211 },
  { "mdash",    8212 },
  { "lsquo",    8216 },
  { "rsquo",    8217 },
  { "sbquo",    8218 },
  { "ldquo",    8220 },
  { "rdquo",    8221 },
  { "bdquo",    8222 },
  { "dagger",   8224 },
  { "Dagger",   8225 },
  { "bull",     8226 },
  { "hellip",   8230 },
  { "permil",   8240 },
  { "prime",    8242 },
  { "Prime",    8243 },
  { "lsaquo",   8249 },
  { "rsaquo",   8250 },
  { "oline",    8254 },
  { "frasl",    8260 },
  { "euro",     8364 },
  { "image",    8465 },
  { "weierp",   8472 },
  { "real",     8476 },
  { "trade",    8482 },
  { "alefsym",  8501 },
  { "larr",     8592 },
  { "uarr",     8593 },
  { "rarr",     8594 },
  { "darr",     8595 },
  { "harr",     8596 },
  { "crarr",    8629 },
  { "lArr",     8656 },
  { "uArr",     8657 },
  { "rArr",     8658 },
  { "dArr",     8659 },
  { "hArr",     8660 },
  { "forall",   8704 },
  { "part",     8706 },
  { "exist",    8707 },
  { "empty",    8709 },
  { "nabla",    8711 },
  { "isin",     8712 },
  { "notin",    8713 },
  { "ni",       8715 },
  { "prod",     8719 },
  { "sum",      8721 },
  { "minus",    8722 },
  { "lowast",   8727 },
  { "radic",    8730 },
  { "prop",     8733 },
  { "infin",    8734 },
  { "ang",      8736 },
  { "and",      8743 },
  { "or",       8744 },
  { "cap",      8745 },
  { "cup",      8746 },
  { "int",      8747 },
  { "there4",   8756 },
  { "sim",      8764 },
  { "cong",     8773 },
  { "asymp",    8776 },
  { "ne",       8800 },
  { "equiv",    8801 },
  { "le",       8804 },
  { "ge",       8805 },
  { "sub",      8834 },
  { "sup",      8835 },
  { "nsub",     8836 },
  { "sube",     8838 },
  { "supe",     8839 },
  { "oplus",    8853 },
  { "otimes",   8855 },
  { "perp",     8869 },
  { "sdot",     8901 },
  { "lceil",    8968 },
  { "rceil",    8969 },
  { "lfloor",   8970 },
  { "rfloor",   8971 },
  { "lang",     9001 },
  { "rang",     9002 },
  { "loz",      9674 },
  { "spades",   9824 },
  { "clubs",    9827 },
  { "hearts",   9829 },
  { "diams",    9830 }
};

//-----------------------------------------------------------

static int HtmlEntities_Compare(void *, const void *key, const void *datum);
static BOOL AddString(_Inout_ MX::CStringA &cStrA, _In_ LPCSTR szStartA, _In_ LPCSTR szCurrentA);
static BOOL AddString(_Inout_ MX::CStringW &cStrW, _In_ LPCWSTR szStartW, _In_ LPCWSTR szCurrentW);

//-----------------------------------------------------------

namespace MX {

namespace HtmlEntities {

LPCSTR Get(_In_ WCHAR chW)
{
  HTMLENTITY_DEF *lpDef;

#pragma warning(suppress: 6387)
  lpDef = (HTMLENTITY_DEF*)bsearch_s(&chW, aHtmlEntities, MX_ARRAYLEN(aHtmlEntities), sizeof(HTMLENTITY_DEF),
                                     &HtmlEntities_Compare, NULL);
  return (lpDef != NULL) ? lpDef->szNameA : NULL;
}

HRESULT ConvertTo(_Out_ CStringA &cStrA, _In_ LPCSTR szStrA, _In_ SIZE_T nStrLen)
{
  static const LPCSTR szHexaNumA = "0123456789ABCDEF";
  WCHAR chW[2];
  LPCSTR szStartA, szEndA, szEntityA;
  CHAR szTempBufA[8];
  int nChars;

  cStrA.Empty();

  if (nStrLen == (SIZE_T)-1)
    nStrLen = StrLenA(szStrA);
  if (szStrA == NULL && nStrLen > 0)
    return E_POINTER;
  szEndA = szStrA + nStrLen;

  //preallocate some buffer
  if (cStrA.EnsureBuffer(nStrLen + 1) == FALSE)
    return E_OUTOFMEMORY;

  szStartA = szStrA;
  while (szStrA < szEndA)
  {
    if ((*((LPBYTE)szStrA)) < 32)
    {
      if (AddString(cStrA, szStartA, szStrA) == FALSE)
        return E_OUTOFMEMORY;

      //add entity
      szTempBufA[0] = L'&';
      szTempBufA[1] = L'#';
      szTempBufA[2] = L'x';
      szTempBufA[3] = szHexaNumA[((*((LPBYTE)szStrA)) >> 4) & 0x0F];
      szTempBufA[4] = szHexaNumA[(*((LPBYTE)szStrA)) & 0x0F];
      szTempBufA[5] = L';';
      if (cStrA.ConcatN(szTempBufA, 6) == FALSE)
        return E_OUTOFMEMORY;

      //advance pointer
      szStrA++;
      szStartA = szStrA;
      continue;
    }

    nChars = Utf8_DecodeChar(chW, szStrA, (SIZE_T)(szEndA - szStrA));
    if (nChars > 0 && chW[1] == 0)
    {
      //decoded a non-surrogate pair
      szEntityA = Get(chW[0]);
      if (szEntityA != NULL)
      {
        if (AddString(cStrA, szStartA, szStrA) == FALSE)
          return E_OUTOFMEMORY;

        //add entity
        if (cStrA.ConcatN("&", 1) == FALSE ||
            cStrA.Concat(szEntityA) == FALSE ||
            cStrA.ConcatN(";", 1) == FALSE)
        {
          return E_OUTOFMEMORY;
        }

        //advance pointer
        szStrA += (SIZE_T)nChars;
        szStartA = szStrA;
        continue;
      }
    }

    //advance pointer
    szStrA += (nChars > 0) ? (SIZE_T)nChars : 1;
  }

  //flush remaining
  if (AddString(cStrA, szStartA, szStrA) == FALSE)
    return E_OUTOFMEMORY;

  //done
  return S_OK;
}

HRESULT ConvertTo(_Out_ CStringW &cStrW, _In_ LPCWSTR szStrW, _In_ SIZE_T nStrLen)
{
  static const LPCWSTR szHexaNumW = L"0123456789ABCDEF";
  WCHAR szTempBufW[32];
  LPCSTR szEntityA;
  LPCWSTR szStartW, szEndW;
  SIZE_T i;

  cStrW.Empty();

  if (nStrLen == (SIZE_T)-1)
    nStrLen = StrLenW(szStrW);
  if (szStrW == NULL && nStrLen > 0)
    return E_POINTER;
  szEndW = szStrW + nStrLen;

  //preallocate some buffer
  if (cStrW.EnsureBuffer(nStrLen + 1) == FALSE)
    return E_OUTOFMEMORY;

  szStartW = szStrW;
  while (szStrW < szEndW)
  {
    if ((*szStrW) < 32)
    {
      if (AddString(cStrW, szStartW, szStrW) == FALSE)
        return E_OUTOFMEMORY;

      //add entity
      szTempBufW[0] = L'&';
      szTempBufW[1] = L'#';
      szTempBufW[2] = L'x';
      szTempBufW[3] = szHexaNumW[((*szStrW) >> 4) & 0x0F];
      szTempBufW[4] = szHexaNumW[(*szStrW) & 0x0F];
      szTempBufW[5] = L';';
      if (cStrW.ConcatN(szTempBufW, 6) == FALSE)
        return E_OUTOFMEMORY;

      //advance pointer
      szStrW++;
      szStartW = szStrW;
      continue;
    }

    szEntityA = Get(*szStrW);
    if (szEntityA != NULL)
    {
      if (AddString(cStrW, szStartW, szStrW) == FALSE)
        return E_OUTOFMEMORY;

      //quick convert
      for (i = 0; szEntityA[i] != 0 && i < 32; i++)
        szTempBufW[i] = (WCHAR)(UCHAR)szEntityA[i];

      //add entity
      if (cStrW.ConcatN("&", 1) == FALSE ||
          cStrW.ConcatN(szTempBufW, i) == FALSE ||
          cStrW.ConcatN(";", 1) == FALSE)
      {
        return E_OUTOFMEMORY;
      }

      //advance pointer
      szStrW++;
      szStartW = szStrW;
      continue;
    }

    //advance pointer
    szStrW++;
  }

  //flush remaining
  if (AddString(cStrW, szStartW, szStrW) == FALSE)
    return E_OUTOFMEMORY;

  //done
  return S_OK;
}

HRESULT ConvertFrom(_Out_ CStringA &cStrA, _In_ LPCSTR szStrA, _In_ SIZE_T nStrLen)
{
  LPCSTR szStartA, szEndA, szDecodeEndA;
  CHAR szBufA[8];
  int r;
  WCHAR chW;

  cStrA.Empty();

  if (nStrLen == (SIZE_T)-1)
    nStrLen = StrLenA(szStrA);
  if (szStrA == NULL && nStrLen > 0)
    return E_POINTER;
  szEndA = szStrA + nStrLen;

  //preallocate some buffer
  if (cStrA.EnsureBuffer(nStrLen + 1) == FALSE)
    return E_OUTOFMEMORY;

  szStartA = szStrA;
  while (szStrA < szEndA)
  {
    chW = Decode(szStrA, (SIZE_T)(szEndA - szStrA), &szDecodeEndA);
    if (chW != 0)
    {
      if (AddString(cStrA, szStartA, szStrA) == FALSE)
        return E_OUTOFMEMORY;

      //add character
      r = Utf8_EncodeChar(szBufA, chW);
      if (r < 0)
        r = 0;
      if (r > 0)
      {
        if (cStrA.ConcatN(szBufA, (SIZE_T)r) == FALSE)
          return E_OUTOFMEMORY;
      }

      //advance pointer
      szStrA = szStrA = szDecodeEndA;
    }
    else
    {
      //advance pointer
      szStrA++;
    }
  }

  //flush remaining
  if (AddString(cStrA, szStartA, szStrA) == FALSE)
    return E_OUTOFMEMORY;

  //done
  return S_OK;
}

HRESULT ConvertFrom(_Out_ CStringW &cStrW, _In_ LPCWSTR szStrW, _In_ SIZE_T nStrLen)
{
  LPCWSTR szStartW, szEndW, szDecodeEndW;
  WCHAR chW;

  cStrW.Empty();

  if (nStrLen == (SIZE_T)-1)
    nStrLen = StrLenW(szStrW);
  if (szStrW == NULL && nStrLen > 0)
    return E_POINTER;
  szEndW = szStrW + nStrLen;

  //preallocate some buffer
  if (cStrW.EnsureBuffer(nStrLen + 1) == FALSE)
    return E_OUTOFMEMORY;

  szStartW = szStrW;
  while (szStrW < szEndW)
  {
    chW = Decode(szStrW, (SIZE_T)(szEndW - szStrW), &szDecodeEndW);
    if (chW != 0)
    {
      if (AddString(cStrW, szStartW, szStrW) == FALSE)
        return E_OUTOFMEMORY;

      if (cStrW.ConcatN(&chW, 1) == FALSE)
        return E_OUTOFMEMORY;

      //advance pointer
      szStrW = szStartW = szDecodeEndW;
    }
    else
    {
      //advance pointer
      szStrW++;
    }
  }

  //flush remaining
  if (AddString(cStrW, szStartW, szStrW) == FALSE)
    return E_OUTOFMEMORY;

  //done
  return S_OK;
}

WCHAR Decode(_In_ LPCSTR szStrA, _In_ SIZE_T nStrLen, _Out_opt_ LPCSTR *lpszAfterEntityA)
{
  if (lpszAfterEntityA != NULL)
    *lpszAfterEntityA = szStrA;
  if (szStrA == NULL || nStrLen == 0 || *szStrA != '&')
    return 0;
  if ((--nStrLen) == 0)
    return 0;

  szStrA++;
  if (*szStrA == '#')
  {
    SIZE_T nBase = 10, nDigit;
    WCHAR chW = 0, chTempW;

    if (nStrLen >= 2 && (szStrA[1] == 'x' || szStrA[1] == 'X'))
    {
      nBase = 16;
      szStrA += 2;
      nStrLen -= 2;
    }
    else
    {
      szStrA++;
      nStrLen--;
    }

    while (nStrLen > 0 && *szStrA != 0 && *szStrA != ';')
    {
      if (*szStrA >= '0' && *szStrA <= '9')
        nDigit = (SIZE_T)(*szStrA) - (SIZE_T)'0';
      else if (*szStrA >= 'A' && *szStrA <= 'F')
        nDigit = (SIZE_T)(*szStrA) - (SIZE_T)'A' + 10;
      else if (*szStrA >= 'a' && *szStrA <= 'f')
        nDigit = (SIZE_T)(*szStrA) - (SIZE_T)'a' + 10;
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
      szStrA++;
      nStrLen--;
    }

    //has value?
    if (nStrLen > 0 && *szStrA == ';')
    {
      if (lpszAfterEntityA != NULL)
        *lpszAfterEntityA = szStrA + 1;
      return chW;
    }
  }
  else
  {
    LPCSTR sA;
    SIZE_T i, j;

    for (i = 0; i<MX_ARRAYLEN(aHtmlEntities); i++)
    {
      sA = aHtmlEntities[i].szNameA;
      for (j = 0; j < nStrLen && sA[j] != 0 && szStrA[j] == sA[j]; j++);

      if (sA[j] == 0 && nStrLen - 1 > j && szStrA[j] == ';')
      {
        if (lpszAfterEntityA != NULL)
          *lpszAfterEntityA = szStrA + (j + 1);
        return aHtmlEntities[i].chCharCodeW;
      }
    }
  }

  //invalid entity
  return 0;
}

WCHAR Decode(_In_ LPCWSTR szStrW, _In_ SIZE_T nStrLen, _Out_opt_ LPCWSTR *lpszAfterEntityW)
{
  if (lpszAfterEntityW != NULL)
    *lpszAfterEntityW = szStrW;
  if (szStrW == NULL || nStrLen == 0 || *szStrW != L'&')
    return 0;
  if ((--nStrLen) == 0)
    return 0;

  szStrW++;
  if (*szStrW == L'#')
  {
    SIZE_T nBase = 10, nDigit;
    WCHAR chW = 0, chTempW;

    if (nStrLen >= 2 && (szStrW[1] == L'x' || szStrW[1] == L'X'))
    {
      nBase = 16;
      szStrW += 2;
      nStrLen -= 2;
    }
    else
    {
      szStrW++;
      nStrLen--;
    }

    while (nStrLen > 0 && *szStrW != 0 && *szStrW != L';')
    {
      if (*szStrW >= L'0' && *szStrW <= L'9')
        nDigit = (SIZE_T)(*szStrW - L'0');
      else if (*szStrW >= L'A' && *szStrW <= L'F')
        nDigit = (SIZE_T)(*szStrW - L'A') + 10;
      else if (*szStrW >= L'a' && *szStrW <= L'f')
        nDigit = (SIZE_T)(*szStrW - L'a') + 10;
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
      szStrW++;
      nStrLen--;
    }

    //has value?
    if (nStrLen > 0 && *szStrW == L';')
    {
      if (lpszAfterEntityW != NULL)
        *lpszAfterEntityW = szStrW + 1;
      return chW;
    }
  }
  else
  {
    LPCSTR sA;
    SIZE_T i, j;

    for (i = 0; i < MX_ARRAYLEN(aHtmlEntities); i++)
    {
      sA = aHtmlEntities[i].szNameA;
      for (j=0; j < nStrLen && sA[j] != 0 && szStrW[j] == (WCHAR)((UCHAR)sA[j]); j++);

      if (sA[j] == 0 && nStrLen - 1 > j && szStrW[j] == L';')
      {
        if (lpszAfterEntityW != NULL)
          *lpszAfterEntityW = szStrW + (j + 1);
        return aHtmlEntities[i].chCharCodeW;
      }
    }
  }

  //invalid entity
  return 0;
}

} //namespace HtmlEntities

} //namespace MX

//-----------------------------------------------------------

static int HtmlEntities_Compare(void *, const void *key, const void *datum)
{
  if (*((WCHAR*)key) < ((HTMLENTITY_DEF*)datum)->chCharCodeW)
    return -1;
  if (*((WCHAR*)key) > ((HTMLENTITY_DEF*)datum)->chCharCodeW)
    return 1;
  return 0;
}

static BOOL AddString(_Inout_ MX::CStringA &cStrA, _In_ LPCSTR szStartA, _In_ LPCSTR szCurrentA)
{
  if (szCurrentA > szStartA)
  {
    if (cStrA.ConcatN(szStartA, (SIZE_T)(szCurrentA - szStartA)) == FALSE)
      return FALSE;
  }
  return TRUE;
}

static BOOL AddString(_Inout_ MX::CStringW &cStrW, _In_ LPCWSTR szStartW, _In_ LPCWSTR szCurrentW)
{
  if (szCurrentW > szStartW)
  {
    if (cStrW.ConcatN(szStartW, (SIZE_T)(szCurrentW - szStartW)) == FALSE)
      return FALSE;
  }
  return TRUE;
}
