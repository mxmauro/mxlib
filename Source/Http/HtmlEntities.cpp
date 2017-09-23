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

//-----------------------------------------------------------

namespace MX {

namespace HtmlEntities {

LPCSTR Get(__in WCHAR chW)
{
  HTMLENTITY_DEF *lpDef;

  lpDef = (HTMLENTITY_DEF*)bsearch_s(&chW, aHtmlEntities, MX_ARRAYLEN(aHtmlEntities), sizeof(HTMLENTITY_DEF),
                                     &HtmlEntities_Compare, NULL);
  return (lpDef != NULL) ? lpDef->szNameA : NULL;
}

HRESULT ConvertTo(__inout CStringA &cStrA)
{
  static const LPCSTR szHexaNumA = "0123456789ABCDEF";
  CStringW cStrTempW;
  WCHAR chW[2];
  SIZE_T nOfs, nLen;
  union {
    LPSTR sA;
    LPBYTE lpB;
  };
  int nChars;

  sA = (LPSTR)cStrA;
  nOfs = 0;
  nLen = cStrA.GetLength();
  while (nOfs < nLen)
  {
    nChars = MX::Utf8_DecodeChar(chW, sA + nOfs, nLen - nOfs);
    if (nChars > 0 && chW[1] == 0)
    {
      //decoded a non-surrogate pair
      LPCSTR szEntityA = Get(chW[0]);

      if (szEntityA != NULL)
      {
        cStrA.Delete(nOfs, (SIZE_T)nChars);
        if (cStrA.InsertN("&;", nOfs, 2) == FALSE)
          return E_OUTOFMEMORY;
        if (cStrA.Insert(szEntityA, nOfs+1) == FALSE)
          return E_OUTOFMEMORY;
        nOfs += 2 + MX::StrLenA(szEntityA);
        sA = (LPSTR)cStrA;
        nLen = cStrA.GetLength();
      }
      else
      {
        nOfs += (SIZE_T)nChars;
      }
    }
    else if (*(((LPBYTE)sA) + nOfs) < 32)
    {
      CHAR szTempBufA[8];

      szTempBufA[0] = L'&';
      szTempBufA[1] = L'#';
      szTempBufA[2] = L'x';
      szTempBufA[3] = szHexaNumA[(lpB[nOfs] >> 4) & 0x0F];
      szTempBufA[4] = szHexaNumA[ lpB[nOfs]       & 0x0F];
      szTempBufA[5] = L';';
      cStrA.Delete(nOfs, 1);
      if (cStrA.InsertN(szTempBufA, nOfs, 6) == FALSE)
        return E_OUTOFMEMORY;
      nOfs += 6;
      sA = (LPSTR)cStrA;
      nLen = cStrA.GetLength();
    }
    else
    {
      nOfs++;
    }
  }
  //done
  return S_OK;
}

HRESULT ConvertTo(__inout CStringW &cStrW)
{
  static const LPCWSTR szHexaNumW = L"0123456789ABCDEF";
  WCHAR szTempBufW[32];
  LPWSTR sW;
  SIZE_T i, nOfs, nLen;

  sW = (LPWSTR)cStrW;
  nOfs = 0;
  nLen = cStrW.GetLength();
  while (nOfs < nLen)
  {
    LPCSTR szEntityA = Get(sW[nOfs]);
    if (szEntityA != NULL)
    {
      //quick convert
      for (i=0; szEntityA[i] != 0 && i < 32; i++)
        szTempBufW[i] = (WCHAR)(UCHAR)szEntityA[i];
      cStrW.Delete(nOfs, 1);
      if (cStrW.InsertN(L"&;", nOfs, 2) == FALSE)
        return E_OUTOFMEMORY;
      if (cStrW.InsertN(szTempBufW, nOfs+1, i) == FALSE)
        return E_OUTOFMEMORY;
      nOfs += 2 + i;
      sW = (LPWSTR)cStrW;
      nLen = cStrW.GetLength();
    }
    else if (sW[nOfs] < 32)
    {
      szTempBufW[0] = L'&';
      szTempBufW[1] = L'#';
      szTempBufW[2] = L'x';
      szTempBufW[3] = szHexaNumW[((ULONG)(sW[nOfs]) >> 4) & 0x0F];
      szTempBufW[4] = szHexaNumW[ (ULONG)(sW[nOfs])       & 0x0F];
      szTempBufW[5] = L';';
      cStrW.Delete(nOfs, 1);
      if (cStrW.InsertN(szTempBufW, nOfs, 6) == FALSE)
        return E_OUTOFMEMORY;
      nOfs += 6;
      sW = (LPWSTR)cStrW;
      nLen = cStrW.GetLength();
    }
    else
    {
      nOfs++;
    }
  }
  //done
  return S_OK;
}

HRESULT ConvertFrom(__inout CStringA &cStrA)
{
  LPCSTR sA, szEndA;
  CHAR szBufA[8];
  SIZE_T nOfs, nLen, nEntityLen;
  int r;
  WCHAR chW;

  sA = (LPCSTR)cStrA;
  nLen = cStrA.GetLength();
  while (*sA != 0)
  {
    chW = Decode(sA, nLen, &szEndA);
    if (chW != 0)
    {
      nOfs = (SIZE_T)(szEndA - (LPCSTR)cStrA);
      nEntityLen = (SIZE_T)(szEndA - sA);
      cStrA.Delete(nOfs, nEntityLen);

      r = Utf8_EncodeChar(szBufA, chW);
      if (r < 0)
        r = 0;
      if (r > 0)
      {
        if (cStrA.InsertN(szBufA, nOfs, (SIZE_T)r) == FALSE)
          return E_OUTOFMEMORY;
        nOfs += (SIZE_T)r;
      }
      sA = (LPCSTR)cStrA + nOfs;
      nLen = cStrA.GetLength();
    }
    else
    {
      sA++;
    }
  }
  //done
  return S_OK;
}

HRESULT ConvertFrom(__inout CStringW &cStrW)
{
  LPCWSTR sW, szEndW;
  SIZE_T nOfs, nLen, nEntityLen;
  WCHAR chW;

  sW = (LPCWSTR)cStrW;
  nLen = cStrW.GetLength();
  while (*sW != 0)
  {
    chW = Decode(sW, nLen, &szEndW);
    if (chW != 0)
    {
      nOfs = (SIZE_T)(szEndW - (LPCWSTR)cStrW);
      nEntityLen = (SIZE_T)(szEndW - sW);
      cStrW.Delete(nOfs, nEntityLen);
      if (cStrW.InsertN(&chW, nOfs, 1) == FALSE)
        return E_OUTOFMEMORY;
      sW = (LPCWSTR)cStrW + nOfs + 1;
      nLen = cStrW.GetLength();
    }
    else
    {
      sW++;
    }
  }
  //done
  return S_OK;
}

WCHAR Decode(__in LPCSTR szStrA, __in SIZE_T nStrLen, __out_opt LPCSTR *lpszAfterEntityA)
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
        nDigit = (SIZE_T)(*szStrA - '0');
      else if (*szStrA >= 'A' && *szStrA <= 'F')
        nDigit = (SIZE_T)(*szStrA - 'A') + 10;
      else if (*szStrA >= 'a' && *szStrA <= 'f')
        nDigit = (SIZE_T)(*szStrA - 'a') + 10;
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

    for (i=0; i<sizeof(aHtmlEntities) / sizeof(aHtmlEntities[0]); i++)
    {
      sA = aHtmlEntities[i].szNameA;
      for (j=0; j < nStrLen && sA[j] != 0 && szStrA[j] == sA[j]; j++);
      if (sA[j] == 0 && nStrLen-1 > j && szStrA[j] == ';')
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

WCHAR Decode(__in LPCWSTR szStrW, __in SIZE_T nStrLen, __out_opt LPCWSTR *lpszAfterEntityW)
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

    for (i=0; i<sizeof(aHtmlEntities) / sizeof(aHtmlEntities[0]); i++)
    {
      sA = aHtmlEntities[i].szNameA;
      for (j=0; j < nStrLen && sA[j] != 0 && szStrW[j] == (WCHAR)((UCHAR)sA[j]); j++);
      if (sA[j] == 0 && nStrLen-1 > j && szStrW[j] == L';')
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
