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

#define CHECK_AND_CREATE_HEADER(_hdr)                                              \
    if (StrCompareA(szHeaderNameA, CHttpHeader##_hdr::GetNameStatic(), TRUE) == 0) \
    {                                                                              \
      *lplpHeader = MX_DEBUG_NEW CHttpHeader##_hdr();                              \
      return (*lplpHeader != NULL) ? S_OK : E_OUTOFMEMORY;                         \
    }
HRESULT CHttpHeaderBase::Create(__in LPCSTR szHeaderNameA, __in BOOL bIsRequest, __out CHttpHeaderBase **lplpHeader)
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
  }
  else
  {
    CHECK_AND_CREATE_HEADER(RespAcceptRanges);
    CHECK_AND_CREATE_HEADER(RespAge);
    CHECK_AND_CREATE_HEADER(RespCacheControl);
    CHECK_AND_CREATE_HEADER(RespETag);
    CHECK_AND_CREATE_HEADER(RespLocation);
    CHECK_AND_CREATE_HEADER(RespRetryAfter);
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
  CHECK_AND_CREATE_HEADER(GenUpgrade);
  //else create a generic header
  lpGenericHdr = MX_DEBUG_NEW CHttpHeaderGeneric();
  if (lpGenericHdr == NULL)
    return E_OUTOFMEMORY;
  hRes = lpGenericHdr->SetName(szHeaderNameA);
  if (SUCCEEDED(hRes))
    *lplpHeader = lpGenericHdr;
  else
    delete lpGenericHdr;
  return hRes;
}
#undef CHECK_AND_CREATE_HEADER

LPCSTR CHttpHeaderBase::SkipSpaces(__in_z LPCSTR sA)
{
  while (*sA == ' ' || *sA == '\t')
    sA++;
  return sA;
}

LPCWSTR CHttpHeaderBase::SkipSpaces(__in_z LPCWSTR sW)
{
  while (*sW == L' ' || *sW == L'\t')
    sW++;
  return sW;
}

LPCSTR CHttpHeaderBase::GetToken(__in_z LPCSTR sA, __in_z_opt LPCSTR szStopCharsA)
{
  while (*sA != 0)
  {
    if (CHttpCommon::IsValidNameChar(*sA) == FALSE)
      break;
    if (szStopCharsA !=NULL && StrChrA(szStopCharsA, *sA) != NULL)
      break;
    sA++;
  }
  return sA;
}

LPCWSTR CHttpHeaderBase::GetToken(__in_z LPCWSTR sW, __in_z_opt LPCWSTR szStopCharsW)
{
  while (*sW != 0)
  {
    if (*sW < 31 || *sW > 127)
      break;
    if (CHttpCommon::IsValidNameChar((CHAR)*sW) == FALSE)
      break;
    if (szStopCharsW !=NULL && StrChrW(szStopCharsW, *sW) != NULL)
      break;
    sW++;
  }
  return sW;
}


LPCSTR CHttpHeaderBase::Advance(__in_z LPCSTR sA, __in_z_opt LPCSTR szStopCharsA)
{
  while (*sA != 0)
  {
    if (szStopCharsA !=NULL && StrChrA(szStopCharsA, *sA) != NULL)
      break;
    sA++;
  }
  return sA;
}

LPCWSTR CHttpHeaderBase::Advance(__in_z LPCWSTR sW, __in_z_opt LPCWSTR szStopCharsW)
{
  while (*sW != 0)
  {
    if (szStopCharsW !=NULL && StrChrW(szStopCharsW, *sW) != NULL)
      break;
    sW++;
  }
  return sW;
}

} //namespace MX
