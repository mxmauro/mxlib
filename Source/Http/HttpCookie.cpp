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

static VOID SkipBlanks(_Inout_ LPCSTR &szSrcA, _Inout_ SIZE_T &nSrcLen);
static HRESULT GetPairA(_Inout_ LPCSTR &szSrcA, _Inout_ SIZE_T &nSrcLen, _In_ BOOL bAdv,
                        _Inout_ MX::CStringA &cStrNameA, _Inout_ MX::CStringA &cStrValueA);

//-----------------------------------------------------------

namespace MX {

CHttpCookie::CHttpCookie() : CBaseMemObj()
{
  nFlags = 0;
  return;
}

CHttpCookie::CHttpCookie(_In_ const CHttpCookie& cSrc) throw(...) : CBaseMemObj()
{
  nFlags = 0;
  operator=(cSrc);
  return;
}

CHttpCookie::~CHttpCookie()
{
  return;
}

CHttpCookie& CHttpCookie::operator=(_In_ const CHttpCookie& cSrc) throw(...)
{
  if (&cSrc != this)
  {
    MX::CStringA cStrTempNameA, cStrTempValueA, cStrTempDomainA, cStrTempPathA;

    if (cStrTempNameA.Copy(cSrc.GetName()) == FALSE)
      throw (LONG)E_OUTOFMEMORY;
    if (cStrTempValueA.Copy(cSrc.GetValue()) == FALSE)
      throw (LONG)E_OUTOFMEMORY;
    if (cStrTempDomainA.Copy(cSrc.GetDomain()) == FALSE)
      throw (LONG)E_OUTOFMEMORY;
    if (cStrTempPathA.Copy(cSrc.GetPath()) == FALSE)
      throw (LONG)E_OUTOFMEMORY;

    nFlags = cSrc.nFlags;
    cStrNameA.Attach(cStrTempNameA.Detach());
    cStrValueA.Attach(cStrTempValueA.Detach());
    cStrDomainA.Attach(cStrTempDomainA.Detach());
    cStrPathA.Attach(cStrTempPathA.Detach());
    cExpiresDt = *(cSrc.GetExpireDate());
  }
  return *this;
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

HRESULT CHttpCookie::SetName(_In_z_ LPCSTR szNameA)
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

HRESULT CHttpCookie::SetName(_In_z_ LPCWSTR szNameW)
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

HRESULT CHttpCookie::GetName(_Inout_ CStringW &cStrDestW)
{
  return Utf8_Decode(cStrDestW, GetName());
}

HRESULT CHttpCookie::SetValue(_In_z_ LPCSTR szValueA)
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

HRESULT CHttpCookie::SetValue(_In_z_ LPCWSTR szValueW)
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

HRESULT CHttpCookie::GetValue(_Inout_ CStringW &cStrDestW)
{
  return Utf8_Decode(cStrDestW, GetValue());
}

HRESULT CHttpCookie::SetDomain(_In_z_ LPCSTR szDomainA)
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

HRESULT CHttpCookie::SetDomain(_In_z_ LPCWSTR szDomainW)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (szDomainW == NULL)
    return E_POINTER;
  hRes = Punycode_Encode(cStrTempA, szDomainW);
  if (SUCCEEDED(hRes))
    hRes = SetDomain((LPCSTR)cStrTempA);
  return hRes;
}

LPCSTR CHttpCookie::GetDomain() const
{
  return (LPSTR)cStrDomainA;
}

HRESULT CHttpCookie::GetDomain(_Inout_ CStringW &cStrDestW)
{
  HRESULT hRes;

  hRes = Punycode_Decode(cStrDestW, GetDomain());
  if (FAILED(hRes) && hRes != E_OUTOFMEMORY)
    hRes = MX::Utf8_Decode(cStrDestW, GetDomain());
  if (FAILED(hRes) && hRes != E_OUTOFMEMORY)
  {
    if (cStrDestW.Copy(GetDomain()) == FALSE)
      hRes = E_OUTOFMEMORY;
  }
  if (FAILED(hRes))
    cStrDestW.Empty();
  return hRes;
}

HRESULT CHttpCookie::SetPath(_In_z_ LPCSTR szPathA)
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

HRESULT CHttpCookie::SetPath(_In_z_ LPCWSTR szPathW)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (szPathW == NULL)
    return E_POINTER;
  hRes = Utf8_Encode(cStrTempA, szPathW);
  if (SUCCEEDED(hRes))
    hRes = SetPath((LPCSTR)cStrTempA);
  return hRes;
}

LPCSTR CHttpCookie::GetPath() const
{
  return (LPSTR)cStrPathA;
}

HRESULT CHttpCookie::GetPath(_Inout_ CStringW &cStrDestW)
{
  return Utf8_Decode(cStrDestW, GetPath());
}

VOID CHttpCookie::SetExpireDate(_In_opt_ const CDateTime *lpDate)
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

VOID CHttpCookie::SetSecureFlag(_In_ BOOL bIsSecure)
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

VOID CHttpCookie::SetHttpOnlyFlag(_In_ BOOL bIsHttpOnly)
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

HRESULT CHttpCookie::ToString(_Inout_ CStringA& cStrDestA, _In_ BOOL bAddAttributes)
{
  CDateTime cTempDt, *lpDt;
  HRESULT hRes;

  cStrDestA.Empty();
  if (cStrNameA.IsEmpty() != FALSE)
    return E_FAIL;
  lpDt = NULL;
  if (cStrDestA.Copy((LPCSTR)cStrNameA) == FALSE ||
      cStrDestA.Concat("=") == FALSE)
    return E_OUTOFMEMORY;
  if (cStrValueA.IsEmpty() == FALSE || bAddAttributes == FALSE)
  {
    if (cStrDestA.Concat((LPCSTR)cStrValueA) == FALSE)
      return E_OUTOFMEMORY;
  }
  else
  {
    if (cStrDestA.Concat("dummy") == FALSE)
      return E_OUTOFMEMORY;
    hRes = cTempDt.SetFromNow(FALSE);
    hRes = cTempDt.Add(-1, CDateTime::UnitsHours);
    lpDt = &cTempDt;
  }
  if (bAddAttributes != FALSE)
  {
    if (cStrPathA.IsEmpty() == FALSE)
    {
      if (cStrDestA.Concat("; Path=") == FALSE ||
          cStrDestA.Concat((LPSTR)cStrPathA) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrDomainA.IsEmpty() == FALSE)
    {
      if (cStrDestA.Concat("; Domain=") == FALSE ||
          cStrDestA.Concat((LPSTR)cStrDomainA) == FALSE)
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
    hRes = lpDt->AppendFormat(cStrDestA, "; %a, %d-%m-%Y %H:%m:%S %z");
    if (FAILED(hRes))
      return hRes;
  }
  if (bAddAttributes != FALSE)
  {
    if ((nFlags & COOKIE_FLAG_ISSECURE) != 0)
    {
      if (cStrDestA.Concat("; Secure") == FALSE)
        return E_OUTOFMEMORY;
    }
    if ((nFlags & COOKIE_FLAG_HTTPONLY) != 0)
    {
      if (cStrDestA.Concat("; HttpOnly") == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  return S_OK;
}

HRESULT CHttpCookie::DoesDomainMatch(_In_z_ LPCSTR szDomainToMatchA)
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

HRESULT CHttpCookie::DoesDomainMatch(_In_z_ LPCWSTR szDomainToMatchW)
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

HRESULT CHttpCookie::HasExpired(_In_opt_ const CDateTime *lpDate)
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

HRESULT CHttpCookie::ParseFromResponseHeader(_In_z_ LPCSTR szSrcA, _In_opt_ SIZE_T nSrcLen)
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

CHttpCookieArray::CHttpCookieArray(_In_ const CHttpCookieArray& cSrc) throw(...) : TArrayListWithDelete<CHttpCookie*>()
{
  operator=(cSrc);
  return;
}

CHttpCookieArray& CHttpCookieArray::operator=(_In_ const CHttpCookieArray& cSrc) throw(...)
{
  if (this != &cSrc)
  {
    TArrayListWithDelete<CHttpCookie*> aNewList;
    TAutoDeletePtr<CHttpCookie> cNewCookie;
    SIZE_T i, nCount;

    nCount = cSrc.GetCount();
    for (i = 0; i < nCount; i++)
    {
      cNewCookie.Attach(MX_DEBUG_NEW CHttpCookie());
      if (!cNewCookie)
        throw (LONG)E_OUTOFMEMORY;
      *(cNewCookie.Get()) = *(cSrc.GetElementAt(i));
      if (AddElement(cNewCookie.Get()) == FALSE)
        throw (LONG)E_OUTOFMEMORY;
      cNewCookie.Detach();
    }
    this->Transfer(aNewList);
  }
  return *this;
}

HRESULT CHttpCookieArray::ParseFromRequestHeader(_In_z_ LPCSTR szSrcA, _In_opt_ SIZE_T nSrcLen)
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

HRESULT CHttpCookieArray::Update(_In_ const CHttpCookieArray& cSrc, _In_ BOOL bReplaceExisting)
{
  SIZE_T i, nCount;
  HRESULT hRes;

  hRes = S_OK;
  nCount = cSrc.GetCount();
  for (i=0; SUCCEEDED(hRes) && i<nCount; i++)
    hRes = Update(*cSrc.GetElementAt(i), bReplaceExisting);
  return hRes;
}

HRESULT CHttpCookieArray::Update(_In_ const CHttpCookie& cSrc, _In_ BOOL bReplaceExisting)
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
  try
  {
    *(cNewCookie.Get()) = cSrc;
  }
  catch (LONG hr)
  {
    return hr;
  }
  if (AddElement(cNewCookie.Get()) == FALSE)
    return E_OUTOFMEMORY;
  cNewCookie.Detach();
  if (nIdx < nCount)
    RemoveElementAt(nIdx);
  //done
  return S_OK;
}

HRESULT CHttpCookieArray::RemoveExpiredAndInvalid(_In_opt_ const CDateTime *lpDate)
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

static VOID SkipBlanks(_Inout_ LPCSTR &szSrcA, _Inout_ SIZE_T &nSrcLen)
{
  while (nSrcLen > 0 && (*szSrcA == ' ' || *szSrcA == '\t'))
  {
    szSrcA++;
    nSrcLen--;
  }
  return;
}

static HRESULT GetPairA(_Inout_ LPCSTR &szSrcA, _Inout_ SIZE_T &nSrcLen, _In_ BOOL bAdv,
                        _Inout_ MX::CStringA &cStrNameA, _Inout_ MX::CStringA &cStrValueA)
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
