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
#include "..\..\Include\AutoPtr.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderBase::CHttpHeaderBase() : TRefCounted<CBaseMemObj>()
{
  return;
}

CHttpHeaderBase::~CHttpHeaderBase()
{
  return;
}

#define CHECK_AND_CREATE_HEADER(_hdr)                                                                    \
    szNameA = CHttpHeader##_hdr::GetHeaderNameStatic();                                                  \
    if (StrNCompareA(szHeaderNameA, szNameA, nHeaderNameLen, TRUE) == 0 && szNameA[nHeaderNameLen] == 0) \
    {                                                                                                    \
      *lplpHeader = MX_DEBUG_NEW CHttpHeader##_hdr();                                                    \
      return (*lplpHeader != NULL) ? S_OK : E_OUTOFMEMORY;                                               \
    }
HRESULT CHttpHeaderBase::Create(_In_ LPCSTR szHeaderNameA, _In_ BOOL bIsRequest, _Out_ CHttpHeaderBase **lplpHeader,
                                _In_opt_ SIZE_T nHeaderNameLen)
{
  TAutoRefCounted<CHttpHeaderGeneric> cHeaderGeneric;
  LPCSTR szNameA;
  HRESULT hRes;

  if (lplpHeader == NULL)
    return E_POINTER;
  *lplpHeader = NULL;
  if (szHeaderNameA == NULL)
    return E_POINTER;
  if (nHeaderNameLen == (SIZE_T)-1)
    nHeaderNameLen = StrLenA(szHeaderNameA);
  if (nHeaderNameLen == 0)
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
  cHeaderGeneric.Attach(MX_DEBUG_NEW CHttpHeaderGeneric());
  if (!cHeaderGeneric)
    return E_OUTOFMEMORY;
  hRes = cHeaderGeneric->SetHeaderName(szHeaderNameA, nHeaderNameLen);
  if (SUCCEEDED(hRes))
    *lplpHeader = cHeaderGeneric.Detach();
  return hRes;
}
#undef CHECK_AND_CREATE_HEADER

HRESULT CHttpHeaderBase::Parse(_In_z_ LPCWSTR szValueW, _In_opt_ SIZE_T nValueLen)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenW(szValueW);
  hRes = Utf8_Encode(cStrTempA, szValueW, nValueLen);
  if (FAILED(hRes))
    return hRes;
  return Parse((LPCSTR)cStrTempA, cStrTempA.GetLength());
}

HRESULT CHttpHeaderBase::Merge(_In_ CHttpHeaderBase *lpHeader)
{
  return (lpHeader == NULL) ? E_POINTER : MX_E_Unsupported;
}

LPCSTR CHttpHeaderBase::SkipSpaces(_In_ LPCSTR sA, _In_ LPCSTR szEndA)
{
  while (sA < szEndA && (*sA == ' ' || *sA == '\t'))
    sA++;
  return sA;
}

LPCSTR CHttpHeaderBase::SkipUntil(_In_ LPCSTR sA, _In_ LPCSTR szEndA, _In_opt_z_ LPCSTR szStopCharsA)
{
  while (sA < szEndA)
  {
    if (szStopCharsA != NULL && StrChrA(szStopCharsA, *sA) != NULL)
      break;
    sA++;
  }
  return sA;
}

LPCSTR CHttpHeaderBase::GetToken(_In_ LPCSTR sA, _In_ LPCSTR szEndA)
{
  static LPCSTR szSeparatorsA = "()<>@,;:\\\"/[]?={}";

  while (sA < szEndA)
  {
    if (*((UCHAR*)sA) < 0x21 || *((UCHAR*)sA) > 0x7E || StrChrA(szSeparatorsA, *sA) != NULL)
      break;
    sA++;
  }
  return sA;
}

HRESULT CHttpHeaderBase::GetQuotedString(_Out_ CStringA &cStrA, _Inout_ LPCSTR &sA, _In_ LPCSTR szEndA)
{
  LPCSTR szStartA;

  cStrA.Empty();

  if (sA >= szEndA || *sA != '"')
    return E_INVALIDARG;
  sA++;
  while (sA < szEndA && *sA != '"')
  {
    szStartA = sA;
    while (sA < szEndA && (*((UCHAR*)sA) >= 0x20 || *sA == '\t' || *sA == '\r' || *sA == '\n') &&
           *sA != '\\' && *sA != '"')
    {
      sA++;
    }
    if (sA > szStartA)
    {
      if (cStrA.ConcatN(szStartA, (SIZE_T)(sA - szStartA)) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (sA >= szEndA)
      return E_INVALIDARG;
    if (*sA == '\\')
    {
      sA++;
      if (sA >= szEndA || *((UCHAR*)sA) == 0) //we don't support embedding \0
        return E_INVALIDARG;
      if (cStrA.ConcatN(sA, 1) == FALSE)
        return E_OUTOFMEMORY;
      sA++;
    }
  }
  if (sA >= szEndA || *sA != '"')
    return E_INVALIDARG;
  sA++;
  return S_OK;
}

HRESULT CHttpHeaderBase::GetParamNameAndValue(_In_ BOOL bUseUtf8AsDefaultCharset, _Out_ CStringA &cStrTokenA,
                                              _Out_ CStringW &cStrValueW, _Inout_ LPCSTR &sA, _In_ LPCSTR szEndA,
                                              _Out_opt_ LPBOOL lpbExtendedParam)
{
  CStringA cStrValueA;
  LPCSTR szStartA;
  BOOL bUsingUTF8;
  BOOL bExtendedParam = FALSE;
  HRESULT hRes;

  cStrTokenA.Empty();
  cStrValueW.Empty();
  if (lpbExtendedParam != NULL)
    *lpbExtendedParam = FALSE;

  bUsingUTF8 = bUseUtf8AsDefaultCharset;

  //get token
  sA = GetToken(szStartA = sA, szEndA);
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
  sA = SkipSpaces(sA, szEndA);

  //end?
  if (sA >= szEndA)
    return MX_E_NoData;

  //is equal sign?
  if (*sA != '=')
    return (*sA == ',' || *sA == ';') ? MX_E_NoData : MX_E_InvalidData;

  //skip spaces
  sA = SkipSpaces(sA + 1, szEndA);
  //parse value
  if (bExtendedParam != FALSE)
  {
    if ((SIZE_T)(szEndA - sA) >= 6 && MX::StrNCompareA(sA, "UTF-8'", 6, TRUE) == 0)
    {
      sA += 6;
      bUsingUTF8 = TRUE;
    }
    else if ((SIZE_T)(szEndA - sA) >= 11 && MX::StrNCompareA(sA, "ISO-8859-1'", 11, TRUE) == 0)
    {
      sA += 11;
      bUsingUTF8 = FALSE;
    }

    if (sA < szEndA && *sA == '\'')
    {
      //skip language
      while (sA < szEndA && *sA != '\'')
        sA++;
      //must follow an apostrophe
      if (sA >= szEndA || *sA != '\'')
        return MX_E_InvalidData;
      sA++;

      //now get value
      while (sA < szEndA)
      {
        BYTE chA, i;

        szStartA = sA;
        while (sA < szEndA && *sA >= 32 && *sA <= 126 && *sA != '%')
          sA++;
        if (sA > szStartA)
        {
          if (cStrValueA.CopyN(szStartA, (SIZE_T)(sA - szStartA)) == FALSE)
            return E_OUTOFMEMORY;
        }
        if (sA < szEndA || *sA != '%')
          break;
        sA++;
        //pct-encoded char
        if ((SIZE_T)(szEndA - sA) < 2)
          return MX_E_InvalidData;
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
          sA++;
        }
        if (cStrValueA.CopyN((LPCSTR)&chA, 1) == FALSE)
          return E_OUTOFMEMORY;
      }

      goto convert_harset;
    }
  }

  //try quoted string
  if (sA < szEndA && *sA == '"')
  {
    hRes = GetQuotedString(cStrValueA, sA, szEndA);
    if (FAILED(hRes))
      return (hRes == E_OUTOFMEMORY) ? hRes : MX_E_InvalidData;
  }
  else
  {
    //try token
    sA = GetToken(szStartA = sA, szEndA);
    if (cStrValueA.CopyN(szStartA, (SIZE_T)(sA - szStartA)) == FALSE)
      return E_OUTOFMEMORY;
  }

convert_harset:
  if (bUsingUTF8 != FALSE)
  {
    hRes = Utf8_Decode(cStrValueW, (LPCSTR)cStrValueA, cStrValueA.GetLength());
    if (FAILED(hRes))
      return (hRes == E_OUTOFMEMORY) ? hRes : MX_E_InvalidData;
  }
  else
  {
    if (Http::FromISO_8859_1(cStrValueW, (LPCSTR)cStrValueA, cStrValueA.GetLength()) == FALSE)
      return E_OUTOFMEMORY;
  }

  sA = SkipSpaces(sA, szEndA);
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
    {
      szInputA[nInputLen++] = (CHAR)(UCHAR)(*szSrcW++);
    }

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
    MxMemMove(szInputA, szInputA + (SIZE_T)len, nInputLen - (SIZE_T)len);
    nInputLen -= (SIZE_T)len;
  }
  return TRUE;
}


//-----------------------------------------------------------

CHttpHeaderArray::CHttpHeaderArray(_In_ const CHttpHeaderArray &cSrc) throw(...) :
                  TArrayListWithRelease<CHttpHeaderBase*>()
{
  operator=(cSrc);
  return;
}

CHttpHeaderArray& CHttpHeaderArray::operator=(_In_ const CHttpHeaderArray &cSrc) throw(...)
{
  if (this != &cSrc)
  {
    TArrayListWithRelease<CHttpHeaderBase*> aNewList;
    SIZE_T i, nCount;

    nCount = cSrc.GetCount();
    for (i = 0; i < nCount; i++)
    {
      CHttpHeaderBase *lpHeader = cSrc.GetElementAt(i);

      if (AddElement(lpHeader) == FALSE)
        throw (LONG)E_OUTOFMEMORY;
      lpHeader->AddRef();
    }
    this->Transfer(aNewList);
  }
  return *this;
}

//NOTE: Returns -1 if not found
SIZE_T CHttpHeaderArray::Find(_In_z_ LPCSTR szNameA) const
{
  SIZE_T i, nCount;

  if (szNameA == NULL || *szNameA == 0)
    return (SIZE_T)-1;
  nCount = GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (StrCompareA(GetElementAt(i)->GetHeaderName(), szNameA, TRUE) == 0)
      return i;
  }
  return (SIZE_T)-1;
}

SIZE_T CHttpHeaderArray::Find(_In_z_ LPCWSTR szNameW) const
{
  SIZE_T i, nCount;

  if (szNameW == NULL || *szNameW == 0)
    return (SIZE_T)-1;
  nCount = GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (StrCompareAW(GetElementAt(i)->GetHeaderName(), szNameW, TRUE) == 0)
      return i;
  }
  return (SIZE_T)-1;
}

HRESULT CHttpHeaderArray::Merge(_In_ const CHttpHeaderArray &cSrc, _In_ BOOL bForceReplaceExisting)
{
  SIZE_T i, nCount;
  HRESULT hRes;

  hRes = S_OK;
  nCount = cSrc.GetCount();
  for (i = 0; SUCCEEDED(hRes) && i < nCount; i++)
  {
    hRes = Merge(cSrc.GetElementAt(i), bForceReplaceExisting);
    if (hRes == MX_E_AlreadyExists)
      hRes = S_OK;
  }
  return hRes;
}

HRESULT CHttpHeaderArray::Merge(_In_ CHttpHeaderBase *lpSrc, _In_ BOOL bForceReplaceExisting)
{
  TAutoDeletePtr<CHttpHeaderBase> cHeader;
  CHttpHeaderBase *lpHeader = NULL;
  SIZE_T nHeaderIndex = (SIZE_T)-1;
  CHttpHeaderBase::eDuplicateBehavior nDuplicateBehavior;
  HRESULT hRes;

  if (lpSrc == NULL)
    return E_POINTER;

  nDuplicateBehavior = CHttpHeaderBase::DuplicateBehaviorReplace;
  if (bForceReplaceExisting == FALSE)
  {
    SIZE_T i, nCount;

    nDuplicateBehavior = CHttpHeaderBase::DuplicateBehaviorAdd;
    nCount = GetCount();
    for (i = 0; i < nCount; i++)
    {
      lpHeader = GetElementAt(i);
      if (StrCompareA(lpHeader->GetHeaderName(), lpSrc->GetHeaderName(), TRUE) == 0)
      {
        nDuplicateBehavior = lpHeader->GetDuplicateBehavior();
        if (nDuplicateBehavior == CHttpHeaderBase::DuplicateBehaviorError)
          return MX_E_AlreadyExists;
        nHeaderIndex = i;
        break;
      }
    }
  }
  switch (nDuplicateBehavior)
  {
    case CHttpHeaderBase::DuplicateBehaviorReplace:
      if (AddElement(lpSrc) == FALSE)
        return E_OUTOFMEMORY;
      lpSrc->AddRef();

      if (nHeaderIndex != (SIZE_T)-1)
        RemoveElementAt(nHeaderIndex);
      break;

    case CHttpHeaderBase::DuplicateBehaviorMerge:
      hRes = lpHeader->Merge(lpSrc);
      if (FAILED(hRes))
        return hRes;
      break;

    default:
      if (AddElement(lpSrc) == FALSE)
        return E_OUTOFMEMORY;
      lpSrc->AddRef();
      break;
  }
  //done
  return S_OK;
}

} //namespace MX
