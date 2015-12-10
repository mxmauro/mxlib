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
#include "..\..\Include\Http\HttpCookie.h"
#include "..\..\Include\Strings\Utf8.h"
#include "..\..\Include\Http\Url.h"
#include "..\..\Include\Http\punycode.h"
#include "..\..\Include\Http\HttpCommon.h"
#include <stdlib.h>

//-----------------------------------------------------------

#define COOKIE_FLAG_EXPIRES_SET                      0x01
#define COOKIE_FLAG_ISSECURE                         0x02
#define COOKIE_FLAG_HTTPONLY                         0x04

//-----------------------------------------------------------

static const MX::CDateTime cZeroDt(1, 1, 1);

//-----------------------------------------------------------

static VOID SkipBlanks(__inout LPCSTR &szSrcA, __inout SIZE_T &nSrcLen);
static HRESULT GetPairA(__inout LPCSTR &szSrcA, __inout SIZE_T &nSrcLen, __in BOOL bAdv,
                        __inout MX::CStringA &cStrNameA, __inout MX::CStringA &cStrValueA);

//-----------------------------------------------------------

namespace MX {

CHttpCookie::CHttpCookie() : CBaseMemObj()
{
  nFlags = 0;
  return;
}

CHttpCookie::~CHttpCookie()
{
  return;
}

VOID CHttpCookie::Clear()
{
  cStrNameA.Empty();
  cStrValueA.Empty();
  cStrDomainA.Empty();
  cStrPathA.Empty();
  cExpiresDt.Clear();
  nFlags = 0;
  return;
}

HRESULT CHttpCookie::SetName(__in_z LPCSTR szNameA)
{
  CStringA cStrTempA;
  LPCSTR sA;

  if (szNameA == NULL)
    return E_POINTER;
  for (sA=szNameA; *sA!=0 && *sA!=';' && *sA!=',' && *((LPBYTE)sA)>32; sA++);
  //for (sA=szNameA; *sA!=0 && CHttpCommon::IsValidNameChar(*sA)!=FALSE; sA++);
  if (*sA != 0)
    return E_INVALIDARG;
  if (cStrTempA.Copy(szNameA) == FALSE)
    return E_OUTOFMEMORY;
  cStrNameA.Attach(cStrTempA.Detach());
  return S_OK;
}

HRESULT CHttpCookie::SetName(__in_z LPCWSTR szNameW)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (szNameW == NULL)
    return E_POINTER;
  hRes = Utf8_Encode(cStrTempA, szNameW);
  if (SUCCEEDED(hRes))
    hRes = SetName((LPCSTR)cStrTempA);
  //done
  return hRes;
}

LPCSTR CHttpCookie::GetName() const
{
  return (LPSTR)cStrNameA;
}

HRESULT CHttpCookie::GetName(__inout CStringW &cStrDestW)
{
  return Utf8_Decode(cStrDestW, GetName());
}

HRESULT CHttpCookie::SetValue(__in_z LPCSTR szValueA)
{
  CStringA cStrTempA;
  LPCSTR sA;

  if (szValueA == NULL)
    return E_POINTER;
  for (sA=szValueA; *sA!=0 && *sA!=';'; sA++);
  if (*sA != 0)
    return E_INVALIDARG;
  if (cStrTempA.Copy(szValueA) == FALSE)
    return E_OUTOFMEMORY;
  cStrValueA.Attach(cStrTempA.Detach());
  //done
  return S_OK;
}

HRESULT CHttpCookie::SetValue(__in_z LPCWSTR szValueW)
{
  CStringA cStrTempA;
  LPCWSTR sW;
  HRESULT hRes;

  if (szValueW == NULL)
    return E_POINTER;
  for (sW=szValueW; *sW!=0 && *sW!=L';'; sW++);
  if (*sW != 0)
    return E_INVALIDARG;
  hRes = Utf8_Encode(cStrTempA, szValueW);
  if (FAILED(hRes))
    return hRes;
  cStrValueA.Attach(cStrTempA.Detach());
  //done
  return S_OK;
}

LPCSTR CHttpCookie::GetValue() const
{
  return (LPSTR)cStrValueA;
}

HRESULT CHttpCookie::GetValue(__inout CStringW &cStrDestW)
{
  return Utf8_Decode(cStrDestW, GetValue());
}

HRESULT CHttpCookie::SetDomain(__in_z LPCSTR szDomainA)
{
  CStringA cStrTempA;
  LPCSTR sA;

  if (szDomainA == NULL)
    return E_POINTER;
  for (sA=szDomainA; *sA!=0; sA++)
  {
    if (*sA < 0x21 || *sA > 0x7E || *sA == '=' || *sA == ';' || *sA == ',')
      return E_INVALIDARG;
  }
  if (cStrTempA.Copy(szDomainA) == FALSE)
    return E_OUTOFMEMORY;
  cStrDomainA.Attach(cStrTempA.Detach());
  return S_OK;
}

HRESULT CHttpCookie::SetDomain(__in_z LPCWSTR szDomainW)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (szDomainW == NULL)
    return E_POINTER;
  hRes = Punycode_Encode(cStrTempA, szDomainW);
  if (SUCCEEDED(hRes))
    hRes = SetDomain((LPSTR)cStrTempA);
  return hRes;
}

LPCSTR CHttpCookie::GetDomain() const
{
  return (LPSTR)cStrDomainA;
}

HRESULT CHttpCookie::GetDomain(__inout CStringW &cStrDestW)
{
  return Punycode_Decode(cStrDestW, GetDomain());
}

HRESULT CHttpCookie::SetPath(__in_z LPCSTR szPathA)
{
  CStringA cStrTempA;
  LPCSTR sA;

  if (szPathA == NULL)
    return E_POINTER;
  for (sA=szPathA; *sA!=0; sA++)
  {
    if (*sA < 0x21 || *sA > 0x7E || strchr("=;, \t", *sA) != NULL)
      return E_INVALIDARG;
  }
  if (cStrTempA.Copy(szPathA) == FALSE)
    return E_OUTOFMEMORY;
  cStrPathA.Attach(cStrTempA.Detach());
  return S_OK;
}

HRESULT CHttpCookie::SetPath(__in_z LPCWSTR szPathW)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (szPathW == NULL)
    return E_POINTER;
  hRes = Utf8_Encode(cStrTempA, szPathW);
  if (SUCCEEDED(hRes))
    hRes = SetPath((LPSTR)cStrTempA);
  return hRes;
}

LPCSTR CHttpCookie::GetPath() const
{
  return (LPSTR)cStrPathA;
}

HRESULT CHttpCookie::GetPath(__inout CStringW &cStrDestW)
{
  return Utf8_Decode(cStrDestW, GetPath());
}

VOID CHttpCookie::SetExpireDate(__in const CDateTime *lpDate)
{
  if (lpDate != NULL)
  {
    cExpiresDt = *lpDate;
    nFlags |= COOKIE_FLAG_EXPIRES_SET;
  }
  else
  {
    nFlags &= ~COOKIE_FLAG_EXPIRES_SET;
  }
  return;
}

CDateTime* CHttpCookie::GetExpireDate() const
{
  return const_cast<CDateTime*>(((nFlags & COOKIE_FLAG_EXPIRES_SET) != 0) ? (&cExpiresDt) : (&cZeroDt));
}

VOID CHttpCookie::SetSecureFlag(__in BOOL bIsSecure)
{
  if (bIsSecure != FALSE)
    nFlags |= COOKIE_FLAG_ISSECURE;
  else
    nFlags &= ~COOKIE_FLAG_ISSECURE;
  return;
}

BOOL CHttpCookie::GetSecureFlag() const
{
  return ((nFlags & COOKIE_FLAG_ISSECURE) != 0) ? TRUE : FALSE;
}

VOID CHttpCookie::SetHttpOnlyFlag(__in BOOL bIsHttpOnly)
{
  if (bIsHttpOnly != FALSE)
    nFlags |= COOKIE_FLAG_HTTPONLY;
  else
    nFlags &= ~COOKIE_FLAG_HTTPONLY;
  return;
}

BOOL CHttpCookie::GetHttpOnlyFlag() const
{
  return ((nFlags & COOKIE_FLAG_HTTPONLY) != 0) ? TRUE : FALSE;
}

HRESULT CHttpCookie::operator=(__in const CHttpCookie& cSrc)
{
  if (&cSrc != this)
  {
    if (cStrNameA.EnsureBuffer(StrLenA(cSrc.GetName()) + 1) == FALSE ||
        cStrValueA.EnsureBuffer(StrLenA(cSrc.GetValue()) + 1) == FALSE ||
        cStrDomainA.EnsureBuffer(StrLenA(cSrc.GetDomain()) + 1) == FALSE ||
        cStrPathA.EnsureBuffer(StrLenA(cSrc.GetPath()) + 1) == FALSE)
      return E_OUTOFMEMORY;
    cStrNameA.Copy(cSrc.GetName());
    cStrValueA.Copy(cSrc.GetValue());
    cStrDomainA.Copy(cSrc.GetDomain());
    cStrPathA.Copy(cSrc.GetPath());
    nFlags = cSrc.nFlags;
    cExpiresDt = *(cSrc.GetExpireDate());
  }
  return S_OK;
}

HRESULT CHttpCookie::ToString(__inout CStringA& cStrDestA, __in BOOL bAddAttributes)
{
  CDateTime cTempDt, *lpDt;
  HRESULT hRes;

  cStrDestA.Empty();
  if (cStrNameA.IsEmpty() != FALSE)
    return E_FAIL;
  lpDt = NULL;
  if (cStrDestA.Copy((LPSTR)cStrNameA) == FALSE ||
      cStrDestA.Concat("=") == FALSE)
    return E_OUTOFMEMORY;
  if (cStrValueA.IsEmpty() == FALSE || bAddAttributes == FALSE)
  {
    if (cStrDestA.Concat((LPSTR)cStrValueA) == FALSE ||
        cStrDestA.Concat("; ") == FALSE)
      return E_OUTOFMEMORY;
  }
  else
  {
    if (cStrDestA.Concat("dummy; ") == FALSE)
      return E_OUTOFMEMORY;
    hRes = cTempDt.SetFromNow(FALSE);
    hRes = cTempDt.Add(-1, CDateTime::UnitsHours);
    lpDt = &cTempDt;
  }
  if (bAddAttributes != FALSE)
  {
    if (cStrPathA.IsEmpty() == FALSE)
    {
      if (cStrDestA.Concat("Path=") == FALSE ||
          cStrDestA.Concat((LPSTR)cStrPathA) == FALSE ||
          cStrDestA.Concat("; ") == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrDomainA.IsEmpty() == FALSE)
    {
      if (cStrDestA.Concat("Domain=") == FALSE ||
          cStrDestA.Concat((LPSTR)cStrDomainA) == FALSE ||
          cStrDestA.Concat("; ") == FALSE)
        return E_OUTOFMEMORY;
    }
    if (lpDt == NULL && (nFlags & COOKIE_FLAG_EXPIRES_SET) != 0)
    {
      cTempDt = cExpiresDt;
      hRes = cTempDt.SetGmtOffset(0);
      if (FAILED(hRes))
        return hRes;
      lpDt = &cTempDt;
    }
  }
  if (lpDt != NULL)
  {
    hRes = lpDt->AppendFormat(cStrDestA, "%a, %d-%m-%Y %H:%m:%S %z");
    if (FAILED(hRes))
      return hRes;
  }
  if (bAddAttributes != FALSE)
  {
    if ((nFlags & COOKIE_FLAG_ISSECURE) != 0)
    {
      if (cStrDestA.Concat("Secure; ") == FALSE)
        return E_OUTOFMEMORY;
    }
    if ((nFlags & COOKIE_FLAG_HTTPONLY) != 0)
    {
      if (cStrDestA.Concat("HttpOnly; ") == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  //strip last "; "
  cStrDestA.Delete(cStrDestA.GetLength()-2, 2);
  return S_OK;
}

HRESULT CHttpCookie::DoesDomainMatch(__in_z LPCSTR szDomainToMatchA)
{
  SIZE_T nToMatchLen, nLength;
  LPCSTR szDomainA;

  if (szDomainToMatchA == NULL)
    return E_POINTER;
  if (*szDomainToMatchA == 0)
    return E_INVALIDARG;
  if (cStrDomainA.IsEmpty() != FALSE)
    return S_OK;
  szDomainA = (LPCSTR)cStrDomainA;
  if (*szDomainToMatchA == '.' && *szDomainA != '.')
    szDomainToMatchA++;
  nToMatchLen = StrLenA(szDomainToMatchA);
  nLength = StrLenA(szDomainA);
  if (nToMatchLen > nLength)
    return S_FALSE;
  szDomainA += (nLength - nToMatchLen);
  return (StrCompareA(szDomainToMatchA, szDomainA) == 0) ? S_OK : S_FALSE;
}

HRESULT CHttpCookie::DoesDomainMatch(__in_z LPCWSTR szDomainToMatchW)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (szDomainToMatchW == NULL)
    return E_POINTER;
  hRes = Punycode_Encode(cStrTempA, szDomainToMatchW);
  if (SUCCEEDED(hRes))
    hRes = DoesDomainMatch((LPSTR)cStrTempA);
  return hRes;
}

HRESULT CHttpCookie::HasExpired(__in_opt const CDateTime *lpDate)
{
  CDateTime cTempCmpDt, cTempExpDt, *lpThisDt;
  HRESULT hRes;

  if ((nFlags & COOKIE_FLAG_EXPIRES_SET) == 0)
    return S_FALSE;
  lpThisDt = &cExpiresDt;
  if (cExpiresDt.GetGmtOffset() != 0)
  {
    cTempExpDt = cExpiresDt;
    hRes = cTempExpDt.SetGmtOffset(0);
    if (FAILED(hRes))
      return hRes;
    lpThisDt = &cTempExpDt;
  }
  if (lpDate != NULL)
  {
    if (lpDate->GetGmtOffset() != 0)
    {
      cTempCmpDt = *lpDate;
      hRes = cTempCmpDt.SetGmtOffset(0);
      if (FAILED(hRes))
        return hRes;
      lpDate = &cTempCmpDt;
    }
  }
  else
  {
    hRes = cTempCmpDt.SetFromNow(FALSE);
    if (FAILED(hRes))
      return hRes;
    lpDate = &cTempCmpDt;
  }
  return ((*lpDate) >= cExpiresDt) ? S_FALSE : S_OK;
}

HRESULT CHttpCookie::ParseFromResponseHeader(__in_z LPCSTR szSrcA, __in_opt SIZE_T nSrcLen)
{
  CStringA cStrTempNameA, cStrTempValueA;
  HRESULT hRes;

  if (nSrcLen == (SIZE_T)-1)
    nSrcLen = (szSrcA != NULL) ? StrLenA(szSrcA) : 0;
  if (szSrcA == NULL && nSrcLen > 0)
    return E_POINTER;
  if (nSrcLen >= 11 && StrNCompareA(szSrcA, "Set-Cookie:", 11, TRUE) == 0)
  {
    szSrcA += 11;
    nSrcLen -= 11;
  }
  else if (nSrcLen >= 11 && StrNCompareA(szSrcA, "Set-Cookie2:", 12, TRUE) == 0)
  {
    szSrcA += 12;
    nSrcLen -= 12;
  }
  SkipBlanks(szSrcA, nSrcLen);
  Clear();
  //get name/value pair
  hRes = GetPairA(szSrcA, nSrcLen, FALSE, cStrTempNameA, cStrTempValueA);
  if (SUCCEEDED(hRes))
    hRes = SetName((LPCSTR)cStrTempNameA);
  if (SUCCEEDED(hRes))
    hRes = SetValue((LPCSTR)cStrTempValueA);
  if (FAILED(hRes))
    return hRes;
  while (nSrcLen > 0)
  {
    //get advanced values
    hRes = GetPairA(szSrcA, nSrcLen, TRUE, cStrTempNameA, cStrTempValueA);
    if (FAILED(hRes))
      return hRes;
    if (((LPSTR)cStrTempNameA)[0] == '$')
      cStrTempNameA.Delete(0, 1);
    if (cStrTempNameA.IsEmpty() != FALSE ||
        StrCompareA((LPSTR)cStrTempNameA, "Discard", TRUE) == 0 ||
        StrCompareA((LPSTR)cStrTempNameA, "Comment", TRUE) == 0 ||
        StrCompareA((LPSTR)cStrTempNameA, "CommentURL", TRUE) == 0 ||
        StrCompareA((LPSTR)cStrTempNameA, "Port", TRUE) == 0 ||
        StrCompareA((LPSTR)cStrTempNameA, "Version", TRUE) == 0)
    {
      //ignore value
    }
    else if (StrCompareA((LPSTR)cStrTempNameA, "Secure", TRUE) == 0)
    {
      SetSecureFlag(TRUE);
    }
    else if (StrCompareA((LPSTR)cStrTempNameA, "HttpOnly", TRUE) == 0)
    {
      SetHttpOnlyFlag(TRUE);
    }
    else if (StrCompareA((LPSTR)cStrTempNameA, "Max-Age", TRUE) == 0)
    {
      MX::CDateTime cExpireDt;
      LPSTR sA;
      ULONGLONG nMaxAge, nTemp;

      sA = (LPSTR)cStrTempValueA;
      nMaxAge = 0;
      while (*sA == ' ' || *sA == '\t')
        sA++;
      if (*sA < '0' || *sA > '9')
        return E_FAIL;
      while (*sA >= '0' && *sA <=' 9')
      {
        nTemp = nMaxAge * 10;
        if (nTemp < nMaxAge)
          return E_FAIL;
        nMaxAge = nTemp + (ULONGLONG)(*sA - '0');
        if (nMaxAge < nTemp)
          return E_FAIL;
        sA++;
      }
      while (*sA == ' ' || *sA == '\t')
        sA++;
      if ((nMaxAge & 0x8000000000000000ui64) != 0 || *sA != 0)
        return E_FAIL;
      hRes = cExpireDt.SetFromNow(FALSE);
      if (SUCCEEDED(hRes))
        hRes = cExpireDt.Add((LONGLONG)nMaxAge, MX::CDateTime::UnitsSeconds);
      if (FAILED(hRes))
        return hRes;
      SetExpireDate(&cExpireDt);
    }
    else if (StrCompareA((LPSTR)cStrTempNameA, "Expires", TRUE) == 0)
    {
      MX::CDateTime cExpireDt;

      hRes = CHttpCommon::ParseDate(cExpireDt, (LPSTR)cStrTempValueA);
      if (FAILED(hRes))
        return hRes;
      SetExpireDate(&cExpireDt);
    }
    else if (StrCompareA((LPSTR)cStrTempNameA, "Path", TRUE) == 0)
    {
      hRes = SetPath((LPSTR)cStrTempValueA);
      if (FAILED(hRes))
        return hRes;
    }
    else if (StrCompareA((LPSTR)cStrTempNameA, "Domain", TRUE) == 0)
    {
      hRes = SetDomain((LPSTR)cStrTempValueA);
      if (FAILED(hRes))
        return hRes;
    }
    else
    {
      return MX_E_InvalidData;
    }
  }
  return S_OK;
}

//-----------------------------------------------------------

HRESULT CHttpCookieArray::operator=(const CHttpCookieArray& cSrc)
{
  TAutoDeletePtr<CHttpCookie> cNewCookie;
  SIZE_T i, nCount, nOrigCount;
  HRESULT hRes;

  nOrigCount = GetCount();
  hRes = S_OK;
  nCount = cSrc.GetCount();
  for (i=0; SUCCEEDED(hRes) && i<nCount; i++)
  {
    cNewCookie.Attach(MX_DEBUG_NEW CHttpCookie());
    if (cNewCookie != NULL)
    {
      hRes = (*(cNewCookie.Get()) = *(cSrc.GetElementAt(i)));
      if (SUCCEEDED(hRes))
      {
        if (AddElement(cNewCookie.Get()) != FALSE)
          cNewCookie.Detach();
        else
          hRes = E_OUTOFMEMORY;
      }
    }
    else
    {
      hRes = E_OUTOFMEMORY;
    }
  }
  if (SUCCEEDED(hRes))
  {
    for (i=0; i<nCount; i++)
      RemoveElementAt(0);
  }
  else
  {
    nCount = GetCount();
    for (i=nOrigCount; i<nCount; i++)
      RemoveElementAt(nOrigCount);
  }
  return hRes;
}

HRESULT CHttpCookieArray::ParseFromRequestHeader(__in_z LPCSTR szSrcA, __in_opt SIZE_T nSrcLen)
{
  CStringA cStrNameA, cStrValueA;
  TAutoDeletePtr<CHttpCookie> cCookie;
  HRESULT hRes;

  if (nSrcLen == (SIZE_T)-1)
    nSrcLen = StrLenA(szSrcA);
  if (szSrcA == NULL && nSrcLen > 0)
    return E_POINTER;
  RemoveAllElements();
  while (nSrcLen > 0)
  {
    SkipBlanks(szSrcA, nSrcLen);
    //get name/value pair
    hRes = GetPairA(szSrcA, nSrcLen, FALSE, cStrNameA, cStrValueA);
    if (FAILED(hRes))
      return hRes;
    //skip $Version, $Path and so on....
    if (((LPSTR)cStrNameA)[0] == '$')
      continue;
    //create a new cookie
    cCookie.Attach(MX_DEBUG_NEW CHttpCookie());
    if (!cCookie)
      return E_OUTOFMEMORY;
    hRes = cCookie->SetName((LPSTR)cStrNameA);
    if (SUCCEEDED(hRes))
      hRes = cCookie->SetValue((LPSTR)cStrValueA);
    if (SUCCEEDED(hRes))
    {
      if (AddElement(cCookie) != FALSE)
        cCookie.Detach();
      else
        hRes = E_OUTOFMEMORY;
    }
    if (FAILED(hRes))
      return hRes;
  }
  return S_OK;
}

HRESULT CHttpCookieArray::Update(__in const CHttpCookieArray& cSrc, __in BOOL bReplaceExisting)
{
  SIZE_T i, nCount;
  HRESULT hRes;

  hRes = S_OK;
  nCount = cSrc.GetCount();
  for (i=0; SUCCEEDED(hRes) && i<nCount; i++)
    hRes = Update(*cSrc.GetElementAt(i), bReplaceExisting);
  return hRes;
}

HRESULT CHttpCookieArray::Update(__in const CHttpCookie& cSrc, __in BOOL bReplaceExisting)
{
  TAutoDeletePtr<CHttpCookie> cNewCookie;
  SIZE_T nIdx, nCount;
  HRESULT hRes;

  hRes = S_OK;
  nCount = GetCount();
  for (nIdx=0; nIdx<nCount; nIdx++)
  {
    if (StrCompareA(cSrc.GetName(), GetElementAt(nIdx)->GetName(), TRUE) == 0)
    {
      if (bReplaceExisting == FALSE)
        return S_OK;
      break;
    }
  }
  cNewCookie.Attach(MX_DEBUG_NEW CHttpCookie());
  if (!cNewCookie)
    return E_OUTOFMEMORY;
  hRes = (*(cNewCookie.Get()) = cSrc);
  if (FAILED(hRes))
    return hRes;
  if (AddElement(cNewCookie.Get()) == FALSE)
    return E_OUTOFMEMORY;
  cNewCookie.Detach();
  if (nIdx < nCount)
    RemoveElementAt(nIdx);
  //done
  return S_OK;
}

HRESULT CHttpCookieArray::RemoveExpiredAndInvalid(__in_opt const CDateTime *lpDate)
{
  CDateTime cTempDt;
  CHttpCookie *lpCookie;
  SIZE_T nCount;
  HRESULT hRes;

  if (lpDate == NULL)
  {
    hRes = cTempDt.SetFromNow(FALSE);
    if (FAILED(hRes))
      return hRes;
    lpDate = &cTempDt;
  }
  else if (lpDate->GetGmtOffset() != 0)
  {
    cTempDt = *lpDate;
    hRes = cTempDt.SetGmtOffset(0);
    if (FAILED(hRes))
      return hRes;
    lpDate = &cTempDt;
  }
  nCount = GetCount();
  while (nCount > 0)
  {
    lpCookie = GetElementAt(--nCount);
    if (lpCookie->GetName()[0] == 0 || lpCookie->GetValue()[0] == 0 ||
        lpCookie->HasExpired(lpDate) == S_OK)
      RemoveElementAt(nCount);
  }
  return S_OK;
}

} //namespace MX

//-----------------------------------------------------------

static VOID SkipBlanks(__inout LPCSTR &szSrcA, __inout SIZE_T &nSrcLen)
{
  while (nSrcLen > 0 && (*szSrcA == ' ' || *szSrcA == '\t'))
  {
    szSrcA++;
    nSrcLen--;
  }
  return;
}

static HRESULT GetPairA(__inout LPCSTR &szSrcA, __inout SIZE_T &nSrcLen, __in BOOL bAdv,
                        __inout MX::CStringA &cStrNameA, __inout MX::CStringA &cStrValueA)
{
  LPCSTR szStartA;

  cStrNameA.Empty();
  cStrValueA.Empty();
  SkipBlanks(szSrcA, nSrcLen);
  //get and store name
  szStartA = szSrcA;
  while (nSrcLen > 0 && *szSrcA != '=' && *szSrcA != ';')
  {
    szSrcA++;
    nSrcLen--;
  }
  if (szSrcA == szStartA)
    return MX_E_InvalidData;
  if (cStrNameA.CopyN(szStartA, (SIZE_T)(szSrcA - szStartA)) == FALSE)
    return E_OUTOFMEMORY;
  SkipBlanks(szSrcA, nSrcLen);
  //end of cookie?
  if (nSrcLen == 0)
    return S_OK;
  //end of cookie?
  if (*szSrcA == ';')
  {
    szSrcA++;
    nSrcLen--;
    return S_OK;
  }
  szSrcA++; //skip '='
  nSrcLen--;
  SkipBlanks(szSrcA, nSrcLen);
  if (nSrcLen > 0)
  {
    if (bAdv != FALSE || *szSrcA != '\"')
    {
      szStartA = szSrcA;
      while (nSrcLen > 0 && *szSrcA != ';')
      {
        szSrcA++;
        nSrcLen--;
      }
      if (cStrValueA.CopyN(szStartA, (SIZE_T)(szSrcA - szStartA)) == FALSE)
        return E_OUTOFMEMORY;
    }
    else
    {
      szStartA = ++szSrcA;
      nSrcLen--;
      while (nSrcLen > 0 && *szSrcA != '"')
      {
        szSrcA++;
        nSrcLen--;
      }
      if (nSrcLen == 0)
        return MX_E_InvalidData;
      if (cStrValueA.CopyN(szStartA, (SIZE_T)(szSrcA - szStartA)) == FALSE)
        return E_OUTOFMEMORY;
      //skip closing quotes
      szSrcA++;
      nSrcLen--;
    }
    SkipBlanks(szSrcA, nSrcLen);
  }
  //check end/final separator
  if (nSrcLen > 0)
  {
    if (*szSrcA != ';' && *szSrcA != ',')
      return MX_E_InvalidData;
    szSrcA++;
    nSrcLen--;
  }
  return S_OK;
}
