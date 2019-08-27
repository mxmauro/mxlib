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
#include "..\..\Include\Http\HttpHeaderBase.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderBase::CHttpHeaderBase() : CBaseMemObj()
{
  return;
}

CHttpHeaderBase::~CHttpHeaderBase()
{
  return;
}

#define CHECK_AND_CREATE_HEADER(_hdr)                                                    \
    if (StrCompareA(szHeaderNameA, CHttpHeader##_hdr::GetHeaderNameStatic(), TRUE) == 0) \
    {                                                                                    \
      *lplpHeader = MX_DEBUG_NEW CHttpHeader##_hdr();                                    \
      return (*lplpHeader != NULL) ? S_OK : E_OUTOFMEMORY;                               \
    }
HRESULT CHttpHeaderBase::Create(_In_ LPCSTR szHeaderNameA, _In_ BOOL bIsRequest, _Out_ CHttpHeaderBase **lplpHeader)
{
  CHttpHeaderGeneric *lpGenericHdr;
  HRESULT hRes;

  if (lplpHeader == NULL)
    return E_POINTER;
  *lplpHeader = NULL;
  if (szHeaderNameA == NULL)
    return E_POINTER;
  if (*szHeaderNameA == 0)
    return E_INVALIDARG;
  //create header
  if (bIsRequest != FALSE)
  {
    CHECK_AND_CREATE_HEADER(ReqAccept);
    CHECK_AND_CREATE_HEADER(ReqAcceptCharset);
    CHECK_AND_CREATE_HEADER(ReqAcceptEncoding);
    CHECK_AND_CREATE_HEADER(ReqAcceptLanguage);
    CHECK_AND_CREATE_HEADER(ReqCacheControl);
    CHECK_AND_CREATE_HEADER(ReqExpect);
    CHECK_AND_CREATE_HEADER(ReqHost);
    CHECK_AND_CREATE_HEADER(ReqIfMatch);
    CHECK_AND_CREATE_HEADER(ReqIfNoneMatch);
    CHECK_AND_CREATE_HEADER(ReqIfModifiedSince);
    CHECK_AND_CREATE_HEADER(ReqIfUnmodifiedSince);
    CHECK_AND_CREATE_HEADER(ReqRange);
    CHECK_AND_CREATE_HEADER(ReqReferer);
    CHECK_AND_CREATE_HEADER(ReqSecWebSocketKey);
    CHECK_AND_CREATE_HEADER(ReqSecWebSocketProtocol);
    CHECK_AND_CREATE_HEADER(ReqSecWebSocketVersion);
  }
  else
  {
    CHECK_AND_CREATE_HEADER(RespAcceptRanges);
    CHECK_AND_CREATE_HEADER(RespAge);
    CHECK_AND_CREATE_HEADER(RespCacheControl);
    CHECK_AND_CREATE_HEADER(RespETag);
    CHECK_AND_CREATE_HEADER(RespLocation);
    CHECK_AND_CREATE_HEADER(RespRetryAfter);
    CHECK_AND_CREATE_HEADER(RespWwwAuthenticate);
    CHECK_AND_CREATE_HEADER(RespProxyAuthenticate);
    CHECK_AND_CREATE_HEADER(RespSecWebSocketAccept);
    CHECK_AND_CREATE_HEADER(RespSecWebSocketProtocol);
    CHECK_AND_CREATE_HEADER(RespSecWebSocketVersion);
  }
  //----
  CHECK_AND_CREATE_HEADER(EntAllow);
  CHECK_AND_CREATE_HEADER(EntContentDisposition);
  CHECK_AND_CREATE_HEADER(EntContentEncoding);
  CHECK_AND_CREATE_HEADER(EntContentLanguage);
  CHECK_AND_CREATE_HEADER(EntContentLength);
  CHECK_AND_CREATE_HEADER(EntContentRange);
  CHECK_AND_CREATE_HEADER(EntContentType);
  CHECK_AND_CREATE_HEADER(EntExpires);
  CHECK_AND_CREATE_HEADER(EntLastModified);
  //----
  CHECK_AND_CREATE_HEADER(GenConnection);
  CHECK_AND_CREATE_HEADER(GenDate);
  CHECK_AND_CREATE_HEADER(GenTransferEncoding);
  CHECK_AND_CREATE_HEADER(GenSecWebSocketExtensions);
  CHECK_AND_CREATE_HEADER(GenUpgrade);
  //else create a generic header
  lpGenericHdr = MX_DEBUG_NEW CHttpHeaderGeneric();
  if (lpGenericHdr == NULL)
    return E_OUTOFMEMORY;
  hRes = lpGenericHdr->SetHeaderName(szHeaderNameA);
  if (SUCCEEDED(hRes))
    *lplpHeader = lpGenericHdr;
  else
    delete lpGenericHdr;
  return hRes;
}
#undef CHECK_AND_CREATE_HEADER

LPCSTR CHttpHeaderBase::SkipSpaces(_In_z_ LPCSTR sA, _In_opt_ SIZE_T nMaxLen)
{
  if (nMaxLen == (SIZE_T)-1)
  {
    while (*sA == ' ' || *sA == '\t')
      sA++;
  }
  else while (nMaxLen > 0 && (*sA == ' ' || *sA == '\t'))
  {
    nMaxLen--;
    sA++;
  }
  return sA;
}

LPCSTR CHttpHeaderBase::SkipUntil(_In_z_ LPCSTR sA, _In_opt_z_ LPCSTR szStopCharsA, _In_opt_ SIZE_T nMaxLen)
{
  while (nMaxLen > 0 && *sA != 0)
  {
    if (szStopCharsA != NULL && StrChrA(szStopCharsA, *sA) != NULL)
      break;
    sA++;
    if (nMaxLen != (SIZE_T)-1)
      nMaxLen--;
  }
  return sA;
}

LPCSTR CHttpHeaderBase::GetToken(_In_z_ LPCSTR sA, _In_opt_ SIZE_T nMaxLen)
{
  static LPCSTR szSeparatorsA = "()<>@,;:\\\"/[]?={}";

  while (nMaxLen > 0 && *sA != 0)
  {
    if (*((UCHAR*)sA) < 0x21 || *((UCHAR*)sA) > 0x7E)
      break;
    if (StrChrA(szSeparatorsA, *sA) != NULL)
      break;
    sA++;
    if (nMaxLen != (SIZE_T)-1)
      nMaxLen--;
  }
  return sA;
}

HRESULT CHttpHeaderBase::GetQuotedString(_Out_ CStringA &cStrA, _Inout_ LPCSTR &sA)
{
  LPCSTR szStartA;

  cStrA.Empty();

  if (*sA++ != '"')
    return E_INVALIDARG;
  while (*sA != '"')
  {
    szStartA = sA;
    while (*((UCHAR*)sA) >= 0x20 && *sA != '\\' && *sA != '"')
      sA++;
    if (sA > szStartA)
    {
      if (cStrA.ConcatN(szStartA, (SIZE_T)(sA - szStartA)) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (*((UCHAR*)sA) < 0x20)
      return E_INVALIDARG;
    if (*sA == '\\')
    {
      sA++;
      if (*((UCHAR*)sA) < 0x20)
        return E_INVALIDARG;
      if (cStrA.ConcatN(sA, 1) == FALSE)
        return E_OUTOFMEMORY;
      sA++;
    }
  }
  if (*sA == '"')
    sA++;
  return S_OK;
}

HRESULT CHttpHeaderBase::GetParamNameAndValue(_Out_ CStringA &cStrTokenA, _Out_ CStringW &cStrValueW,
                                              _Inout_ LPCSTR &sA, _Out_opt_ LPBOOL lpbExtendedParam)
{
  CStringA cStrValueA;
  LPCSTR szStartA;
  BOOL bExtendedParam = FALSE;
  HRESULT hRes;

  cStrTokenA.Empty();
  cStrValueW.Empty();
  if (lpbExtendedParam != NULL)
    *lpbExtendedParam = FALSE;

  //get token
  sA = GetToken(szStartA = sA);
  if (sA == szStartA)
    return MX_E_InvalidData;
  if (*(sA - 1) != '*')
  {
    if (cStrTokenA.CopyN(szStartA, (SIZE_T)(sA - szStartA)) == FALSE)
      return E_OUTOFMEMORY;
  }
  else
  {
    bExtendedParam = TRUE;
    if (cStrTokenA.CopyN(szStartA, (SIZE_T)(sA - szStartA) - 1) == FALSE)
      return E_OUTOFMEMORY;
  }
  //skip spaces
  sA = SkipSpaces(sA);
  //is equal sign?
  if (*sA != '=')
  {
    return (*sA == 0 || *sA == ',' || *sA == ';') ? MX_E_NoData : MX_E_InvalidData;
  }
  //skip spaces
  sA = SkipSpaces(sA + 1);
  //parse value
  if (bExtendedParam != FALSE)
  {
    int nType = 0;

    if (MX::StrNCompareA(sA, "UTF-8'", 6, TRUE) == 0)
    {
      sA += 6;
      nType = 1;
    }
    else if (MX::StrNCompareA(sA, "ISO-8859-1'", 11, TRUE) == 0)
    {
      sA += 11;
      nType = 2;
    }
    if (nType != 0)
    {
      while (*sA != 0 && *sA != '\'')
        sA++;
      if (*sA != '\'')
        return MX_E_InvalidData;
      sA++;
      //get value
      while (*sA != 0)
      {
        BYTE chA, i;

        szStartA = sA;
        while (*sA >= 32 && *sA <= 126 && *sA != '%')
          sA++;
        if (sA > szStartA)
        {
          if (cStrValueA.CopyN(szStartA, (SIZE_T)(sA - szStartA)) == FALSE)
            return E_OUTOFMEMORY;
        }
        if (*sA != '%')
          break;
        sA++;
        //pct-encoded char
        chA = 0;
        for (i = 0; i < 2; i++)
        {
          if (*sA >= '0' && *sA <= '9')
            chA = (chA << 4) + (BYTE)(*sA - '0');
          else if (*sA >= 'A' && *sA <= 'A')
            chA = (chA << 4) + (BYTE)(*sA - 'A' + 10);
          else if (*sA >= 'a' && *sA <= 'f')
            chA = (chA << 4) + (BYTE)(*sA - 'a' + 10);
          else
            return MX_E_InvalidData;
        }
        if (cStrValueA.CopyN((LPCSTR)&chA, 1) == FALSE)
          return E_OUTOFMEMORY;
      }
      if (nType == 1)
      {
        hRes = Utf8_Decode(cStrValueW, (LPCSTR)cStrValueA, cStrValueA.GetLength());
        if (FAILED(hRes))
          return (hRes == E_OUTOFMEMORY) ? hRes : MX_E_InvalidData;
      }
      else
      {
        if (CHttpCommon::FromISO_8859_1(cStrValueW, (LPCSTR)cStrValueA, cStrValueA.GetLength()) == FALSE)
          return E_OUTOFMEMORY;
      }
      goto done;
    }
  }
  //try quoted string
  if (*sA == '"')
  {
    hRes = GetQuotedString(cStrValueA, sA);
    if (FAILED(hRes))
      return (hRes == E_OUTOFMEMORY) ? hRes : MX_E_InvalidData;
    if (CHttpCommon::FromISO_8859_1(cStrValueW, (LPCSTR)cStrValueA, cStrValueA.GetLength()) == FALSE)
      return E_OUTOFMEMORY;
  }
  else
  {
    //try token
    sA = GetToken(szStartA = sA);
    if (cStrValueA.CopyN(szStartA, (SIZE_T)(sA - szStartA)) == FALSE ||
        CHttpCommon::FromISO_8859_1(cStrValueW, (LPCSTR)cStrValueA, cStrValueA.GetLength()) == FALSE)
    {
      return E_OUTOFMEMORY;
    }
  }
done:
  sA = SkipSpaces(sA);
  if (lpbExtendedParam != NULL)
    *lpbExtendedParam = bExtendedParam;
  return S_OK;
}

BOOL CHttpHeaderBase::RawISO_8859_1_to_UTF8(_Out_ CStringW &cStrDestW, _In_ LPCWSTR szSrcW, _In_ SIZE_T nSrcLen)
{
  CHAR szInputA[4];
  WCHAR szDestW[2];
  LPCWSTR szSrcEndW = szSrcW + nSrcLen;
  int len;
  SIZE_T nInputLen;

  cStrDestW.Empty();

  nInputLen = 0;
  while (szSrcW < szSrcEndW || nInputLen > 0)
  {
    while (nInputLen < 4 && szSrcW < szSrcEndW)
      szInputA[nInputLen++] = (CHAR)(UCHAR)(*szSrcW++);

    len = Utf8_DecodeChar(szDestW, szInputA, nInputLen);
    if (len > 0)
    {
      if (cStrDestW.ConcatN(szDestW, (szDestW[1] != 0) ? 2 : 1) == FALSE)
        return FALSE;
    }
    else
    {
      len = 1; //if no output, eat the front character
    }
    //eat "len" characters from input
    MemMove(szInputA, szInputA + (SIZE_T)len, nInputLen - (SIZE_T)len);
    nInputLen -= (SIZE_T)len;
  }
  return TRUE;
}

CHttpHeaderBase::eBrowser CHttpHeaderBase::GetBrowserFromUserAgent(_In_ LPCSTR szUserAgentA,
                                                                   _In_opt_ SIZE_T nUserAgentLen)
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

} //namespace MX
