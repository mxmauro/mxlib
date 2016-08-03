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
#include "..\..\Include\Http\Url.h"
#include "..\..\Include\Http\EMail.h"
#include "..\..\Include\Http\punycode.h"
#include "..\..\Include\Comm\Sockets.h"
#include "..\..\Include\AutoPtr.h"
#include "..\..\Include\Strings\Utf8.h"
#include <wchar.h>

//-----------------------------------------------------------

static struct tagValidSchemes {
  LPCWSTR szNameW;
  MX::CUrl::eScheme nCode;
  int nDefaultPort;
} aValidSchemes[] = {
  { L"mailto",      MX::CUrl::SchemeMailTo,      -1 },
  { L"news",        MX::CUrl::SchemeNews,        -1 },
  { L"http",        MX::CUrl::SchemeHttp,        80 },
  { L"https",       MX::CUrl::SchemeHttps,      443 },
  { L"ftp",         MX::CUrl::SchemeFtp,         21 },
  { L"file",        MX::CUrl::SchemeFile,        -1 },
  { L"resource",    MX::CUrl::SchemeResource,    -1 },
  { L"nntp",        MX::CUrl::SchemeNntp,        -1 },
  { L"gopher",      MX::CUrl::SchemeGopher,      -1 },
  { NULL,           MX::CUrl::SchemeUnknown,     -1 }
};

//-----------------------------------------------------------

static const LPCSTR szHexaNumA = "0123456789ABCDEF";
static const LPCWSTR szHexaNumW = L"0123456789ABCDEF";

//-----------------------------------------------------------

static MX::CUrl::eScheme Scheme2Enum(__in_z LPCWSTR szSchemeW);
static HRESULT ValidateHostAddress(__inout MX::CStringW &cStrHostW);
static CHAR DecodePct(__in_z LPCSTR szStrA);
static WCHAR DecodePct(__in_z LPCWSTR szStrW);
static WCHAR DecodePctU(__in_z LPCSTR szStrA);
static WCHAR DecodePctU(__in_z LPCWSTR szStrW);
static HRESULT NormalizePath(__inout MX::CStringW &cStrPathW, __in_z LPCWSTR szSchemeW);
static HRESULT ReducePath(__inout MX::CStringW &cStrPathW, __in_z LPCWSTR szSchemeW);
static HRESULT ToStringEncode(__inout MX::CStringA &cStrDestA, __in_z LPCWSTR szStrW, __in SIZE_T nLen,
                              __in_z LPCSTR szAllowedCharsA);
static HRESULT ToStringEncode(__inout MX::CStringW &cStrDestW, __in_z LPCWSTR szStrW, __in SIZE_T nLen,
                              __in_z LPCWSTR szAllowedCharsW);
static SIZE_T FindChar(__in_z LPCSTR szStrA, __in SIZE_T nSrcLen, __in_z LPCSTR szToFindA,
                       __in_z_opt LPCSTR szStopCharA=NULL);
static SIZE_T FindChar(__in_z LPCWSTR szStrW, __in SIZE_T nSrcLen, __in_z LPCWSTR szToFindW,
                       __in_z_opt LPCWSTR szStopCharW=NULL);
static BOOL IsLocalHost(__in_z LPCSTR szHostA);
static BOOL IsLocalHost(__in_z LPCWSTR szHostW);
static BOOL HasDisallowedEscapedSequences(__in_z LPCSTR szSrcA, __in SIZE_T nSrcLen);
static BOOL HasDisallowedEscapedSequences(__in_z LPCWSTR szSrcW, __in SIZE_T nSrcLen);

static __inline BOOL IsAlpha(__in CHAR chA)
{
  return ((chA >= 'A' && chA <= 'Z') || (chA >= 'a' && chA <= 'z')) ? TRUE : FALSE;
}

static __inline BOOL IsAlpha(__in WCHAR chW)
{
  return ((chW >= L'A' && chW <= L'Z') || (chW >= L'a' && chW <= L'z')) ? TRUE : FALSE;
}

static __inline int IsHexaDigit(__in CHAR chA)
{
  if (chA >= '0' && chA <= '9')
    return (int)(chA - '0');
  if (chA >= 'A' && chA <= 'F')
    return (int)(chA - 'A') + 10;
  if (chA >= 'a' && chA <= 'f')
    return (int)(chA - 'a') + 10;
  return -1;
}

static __inline int IsHexaDigit(__in WCHAR chW)
{
  if (chW >= L'0' && chW <= L'9')
    return (int)(chW - L'0');
  if (chW >= L'A' && chW <= L'F')
    return (int)(chW - L'A') + 10;
  if (chW >= L'a' && chW <= L'f')
    return (int)(chW - L'a') + 10;
  return -1;
}

static __inline BOOL IsSlash(__in CHAR chA)
{
  return (chA == '/' || chA == '\\') ? TRUE : FALSE;
}

static __inline BOOL IsSlash(__in WCHAR chW)
{
  return (chW == L'/' || chW == L'\\') ? TRUE : FALSE;
}

static __inline BOOL IsUnreservedChar(__in CHAR chA)
{
  return ((chA >='A' && chA <= 'Z') || (chA >='a' && chA <= 'z') ||
          (chA >='0' && chA <= '9') || MX::StrChrA("-_.~", chA) != NULL) ? TRUE : FALSE;
}

static __inline BOOL IsUnreservedChar(__in WCHAR chW)
{
  return (chW < 0x80) ? IsUnreservedChar((CHAR)(UCHAR)chW) : FALSE;
}

//-----------------------------------------------------------

namespace MX {

CUrl::CUrl() : CBaseMemObj()
{
  nPort = -1;
  return;
}

CUrl::~CUrl()
{
  Reset();
  return;
}

VOID CUrl::Reset()
{
  cStrSchemeW.Empty();
  cStrHostW.Empty();
  cStrPathW.Empty();
  ResetQueryStrings();
  cStrUserInfoW.Empty();
  cStrFragmentW.Empty();
  nPort = -1;
  return;
}

HRESULT CUrl::SetScheme(__in_z LPCSTR szSchemeA)
{
  CStringW cStrTempW;

  if (szSchemeA == NULL || *szSchemeA == 0)
  {
    cStrSchemeW.Empty();
    return S_OK;
  }
  if (cStrTempW.Copy(szSchemeA) == FALSE)
    return E_OUTOFMEMORY;
  return SetScheme((LPWSTR)cStrTempW);
}

HRESULT CUrl::SetScheme(__in_z LPCWSTR szSchemeW)
{
  LPCWSTR sW;

  if (szSchemeW == NULL || *szSchemeW == 0)
  {
    cStrSchemeW.Empty();
    return S_OK;
  }
  sW = szSchemeW;
  if (IsAlpha(*sW++) == FALSE)
    return MX_E_InvalidData;
  while (*sW != 0)
  {
    if (IsAlpha(*sW) == FALSE && (*sW < L'0' || *sW > L'9') && *sW != L'.' && *sW != L'+' && *sW != L'-')
      return MX_E_InvalidData;
    sW++;
  }
  //set new value
  if (cStrSchemeW.Copy(szSchemeW) == FALSE)
    return E_OUTOFMEMORY;
  StrToLowerW((LPWSTR)cStrSchemeW);
  //done
  return S_OK;
}

LPCWSTR CUrl::GetScheme() const
{
  return (LPCWSTR)cStrSchemeW;
}

HRESULT CUrl::GetScheme(__inout CStringA &cStrDestA)
{
  return (cStrDestA.Copy((LPCWSTR)cStrSchemeW) != FALSE) ? S_OK : E_OUTOFMEMORY;
}

CUrl::eScheme CUrl::GetSchemeCode() const
{
  return Scheme2Enum(GetScheme());
}

HRESULT CUrl::SetHost(__in_z LPCSTR szHostA, __in_opt SIZE_T nHostLen)
{
  CStringA cStrTempA;
  CStringW cStrTempW;
  HRESULT hRes;

  if (nHostLen == (SIZE_T)-1)
    nHostLen = StrLenA(szHostA);
  if (nHostLen == 0)
  {
    cStrHostW.Empty();
    return S_OK;
  }
  if (szHostA == NULL)
    return E_POINTER;
  //is an IP address
  if (CHostResolver::IsValidIPV4(szHostA, nHostLen) != FALSE || CHostResolver::IsValidIPV6(szHostA, nHostLen) != FALSE)
  {
    if (cStrTempW.CopyN(szHostA, nHostLen) == FALSE)
      return E_OUTOFMEMORY;
  }
  else if (nHostLen > 2 && szHostA[0] == '[' && szHostA[nHostLen-1] == ']' &&
           CHostResolver::IsValidIPV6(szHostA+1, nHostLen-2) != FALSE)
  {
    if (cStrTempW.CopyN(L"[", 1) == FALSE ||
        cStrTempW.ConcatN(szHostA, nHostLen) == FALSE ||
        cStrTempW.ConcatN(L"]", 1) == FALSE)
      return E_OUTOFMEMORY;
  }
  else
  {
    //convert to punycode
    hRes = Decode(cStrTempA, szHostA, nHostLen);
    if (SUCCEEDED(hRes))
      hRes = Punycode_Decode(cStrTempW, (LPCSTR)cStrTempA, cStrTempA.GetLength());
    if (FAILED(hRes))
      return hRes;
    if (IsValidHostAddress((LPCWSTR)cStrTempW, cStrTempW.GetLength()) == FALSE)
      return MX_E_InvalidData;
    if (cStrTempW.CopyN(szHostA, nHostLen) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  cStrHostW.Attach(cStrTempW.Detach());
  return S_OK;
}

HRESULT CUrl::SetHost(__in_z LPCWSTR szHostW, __in_opt SIZE_T nHostLen)
{
  CStringW cStrTempW;
  HRESULT hRes;

  if (nHostLen == (SIZE_T)-1)
    nHostLen = StrLenW(szHostW);
  if (nHostLen == 0)
  {
    cStrHostW.Empty();
    return S_OK;
  }
  if (szHostW == NULL)
    return E_POINTER;
  if (cStrTempW.CopyN(szHostW, nHostLen) == FALSE)
    return E_OUTOFMEMORY;
  hRes = ValidateHostAddress(cStrTempW);
  if (FAILED(hRes))
    return hRes;
  //done
  cStrHostW.Attach(cStrTempW.Detach());
  return S_OK;
}

LPCWSTR CUrl::GetHost() const
{
  return (LPCWSTR)cStrHostW;
}

HRESULT CUrl::GetHost(__inout CStringA &cStrDestA)
{
  HRESULT hRes;

  hRes = Punycode_Encode(cStrDestA, (LPWSTR)cStrHostW, cStrHostW.GetLength());
  if (FAILED(hRes))
    cStrDestA.Empty();
  return hRes;
}

HRESULT CUrl::SetPort(__in int _nPort)
{
  if (_nPort != -1 && (_nPort < 1 || _nPort > 65535))
    return E_INVALIDARG;
  nPort = _nPort;
  return S_OK;
}

int CUrl::GetPort() const
{
  return nPort;
}

HRESULT CUrl::SetPath(__in_z LPCSTR szPathA, __in_opt SIZE_T nPathLen)
{
  CStringA cStrTempA;
  CStringW cStrTempW;
  HRESULT hRes;

  if (nPathLen == (SIZE_T)-1)
    nPathLen = StrLenA(szPathA);
  if (nPathLen == 0)
  {
    cStrPathW.Empty();
    return S_OK;
  }
  if (szPathA == NULL)
    return E_POINTER;
  //disallow strings containing %00, %2F, %5C, %u0000, %u002F and %u005C
  if (HasDisallowedEscapedSequences(szPathA, nPathLen) != FALSE)
    return MX_E_InvalidData;
  //decode percent-encoded characters
  hRes = Decode(cStrTempA, szPathA, nPathLen);
  if (SUCCEEDED(hRes))
    hRes = Utf8_Decode(cStrTempW, (LPCSTR)cStrTempA, cStrTempA.GetLength());
  //set path
  if (SUCCEEDED(hRes))
    hRes = SetPath((LPCWSTR)cStrTempW, cStrTempW.GetLength());
  //done
  return hRes;
}

HRESULT CUrl::SetPath(__in_z LPCWSTR szPathW, __in_opt SIZE_T nPathLen)
{
  CStringW cStrTempW;
  HRESULT hRes;

  if (nPathLen == (SIZE_T)-1)
    nPathLen = StrLenW(szPathW);
  if (nPathLen == 0)
  {
    cStrPathW.Empty();
    return S_OK;
  }
  if (szPathW == NULL)
    return E_POINTER;
  //parse path
  if (cStrTempW.CopyN(szPathW, nPathLen) == FALSE)
    return E_OUTOFMEMORY;
  hRes = NormalizePath(cStrTempW, (LPWSTR)cStrSchemeW);
  if (FAILED(hRes))
    return hRes;
  //done
  cStrPathW.Attach(cStrTempW.Detach());
  return S_OK;
}

LPCWSTR CUrl::GetPath() const
{
  return (LPWSTR)cStrPathW;
}

HRESULT CUrl::ShrinkPath()
{
  return ReducePath(cStrPathW, (LPCWSTR)cStrSchemeW);
}

VOID CUrl::ResetQueryStrings()
{
  cQueryStringsList.RemoveAllElements();
  return;
}

HRESULT CUrl::AddQueryString(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA, __in_opt SIZE_T nValueLen,
                             __in_opt SIZE_T nNameLen)
{
  CStringA cStrTempA;
  CStringW cStrTempNameW, cStrTempValueW;
  HRESULT hRes;

  if (nNameLen == (SIZE_T)-1)
    nNameLen = StrLenA(szNameA);
  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  if (nNameLen == 0)
    return E_INVALIDARG;
  if (szNameA == NULL)
    return E_POINTER;
  //decode percent-encoded characters
  hRes = Decode(cStrTempA, szNameA, nNameLen);
  if (SUCCEEDED(hRes))
    hRes = Utf8_Decode(cStrTempNameW, (LPCSTR)cStrTempA, cStrTempA.GetLength());
  if (SUCCEEDED(hRes))
    hRes = Decode(cStrTempA, szValueA, nValueLen);
  if (SUCCEEDED(hRes))
    hRes = Utf8_Decode(cStrTempValueW, (LPCSTR)cStrTempA, cStrTempA.GetLength());
  //add string
  if (SUCCEEDED(hRes))
  {
    hRes= AddQueryString((LPCWSTR)cStrTempNameW, (LPCWSTR)cStrTempValueW, cStrTempValueW.GetLength(),
                         cStrTempNameW.GetLength());
  }
  //done
  return hRes;
}

HRESULT CUrl::AddQueryString(__in_z LPCWSTR szNameW, __in_z LPCWSTR szValueW, __in_opt SIZE_T nValueLen,
                             __in_opt SIZE_T nNameLen)
{
  TAutoFreePtr<QUERYSTRINGITEM> cNewItem;
  SIZE_T nLen;

  if (nNameLen == (SIZE_T)-1)
    nNameLen = StrLenW(szNameW);
  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenW(szValueW);
  if (nNameLen == 0)
    return E_INVALIDARG;
  if (szNameW == NULL)
    return E_POINTER;
  //create new item
  nLen = nNameLen + nValueLen;
  if (nLen < nNameLen)
    return E_OUTOFMEMORY;
  if (nLen*sizeof(WCHAR) < nLen)
    return E_OUTOFMEMORY;
  nLen *= sizeof(WCHAR);
  if (nLen + sizeof(QUERYSTRINGITEM) + sizeof(WCHAR) < nLen)
    return E_OUTOFMEMORY;
  nLen += sizeof(QUERYSTRINGITEM) + sizeof(WCHAR);
  cNewItem.Attach((LPQUERYSTRINGITEM)MX_MALLOC(nLen));
  if (!cNewItem)
    return E_OUTOFMEMORY;
  cNewItem->szValueW = cNewItem->szNameW + (nNameLen + 1);
  MemCopy(cNewItem->szNameW, szNameW, nNameLen * sizeof(WCHAR));
  cNewItem->szNameW[nNameLen] = 0;
  MemCopy(cNewItem->szValueW, szValueW, nValueLen * sizeof(WCHAR));
  cNewItem->szValueW[nValueLen] = 0;
  //add to list
  if (cQueryStringsList.AddElement(cNewItem.Get()) == FALSE)
    return E_OUTOFMEMORY;
  //done
  cNewItem.Detach();
  return S_OK;
}

HRESULT CUrl::RemoveQueryString(__in SIZE_T nIndex)
{
  if (nIndex >= cQueryStringsList.GetCount())
    return E_INVALIDARG;
  cQueryStringsList.RemoveElementAt(nIndex);
  //done
  return S_OK;
}

SIZE_T CUrl::GetQueryStringCount() const
{
  return cQueryStringsList.GetCount();
}

LPCWSTR CUrl::GetQueryStringName(__in SIZE_T nIndex) const
{
  return (nIndex < cQueryStringsList.GetCount()) ? cQueryStringsList[nIndex]->szNameW : NULL;
}

LPCWSTR CUrl::GetQueryStringValue(__in SIZE_T nIndex) const
{
  return (nIndex < cQueryStringsList.GetCount()) ? cQueryStringsList[nIndex]->szValueW : NULL;
}

LPCWSTR CUrl::GetQueryStringValue(__in_z LPCWSTR szNameW) const
{
  SIZE_T i, nCount;

  if (szNameW != NULL && szNameW[0] != 0)
  {
    nCount = cQueryStringsList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (StrNCompareW(cQueryStringsList[i]->szNameW, szNameW, TRUE) == 0)
        return cQueryStringsList[i]->szValueW;
    }
  }
  return NULL;
}

HRESULT CUrl::SetFragment(__in_z LPCSTR szFragmentA, __in_opt SIZE_T nFragmentLen)
{
  CStringA cStrTempA;
  CStringW cStrTempW;
  HRESULT hRes;

  if (nFragmentLen == (SIZE_T)-1)
    nFragmentLen = StrLenA(szFragmentA);
  if (nFragmentLen == 0)
  {
    cStrFragmentW.Empty();
    return S_OK;
  }
  if (szFragmentA == NULL)
    return E_POINTER;
  //decode percent-encoded characters
  hRes = Decode(cStrTempA, szFragmentA, nFragmentLen);
  if (SUCCEEDED(hRes))
    hRes = Utf8_Decode(cStrTempW, (LPCSTR)cStrTempA, cStrTempA.GetLength());
  if (SUCCEEDED(hRes))
    hRes = SetFragment((LPCWSTR)cStrTempW, cStrTempW.GetLength());
  //done
  return hRes;
}

HRESULT CUrl::SetFragment(__in_z LPCWSTR szFragmentW, __in_opt SIZE_T nFragmentLen)
{
  CStringW cStrTempW;

  if (nFragmentLen == (SIZE_T)-1)
    nFragmentLen = StrLenW(szFragmentW);
  if (nFragmentLen == 0)
  {
    cStrFragmentW.Empty();
    return S_OK;
  }
  //set new value
  if (szFragmentW == NULL)
    return E_POINTER;
  if (cStrTempW.CopyN(szFragmentW, nFragmentLen) == FALSE)
    return E_OUTOFMEMORY;
  //done
  cStrFragmentW.Attach(cStrTempW.Detach());
  return S_OK;
}

LPCWSTR CUrl::GetFragment() const
{
  return (LPCWSTR)cStrFragmentW;
}

HRESULT CUrl::SetUserInfo(__in_z LPCSTR szUserInfoA, __in_opt SIZE_T nUserInfoLen)
{
  CStringA cStrTempA;
  CStringW cStrTempW;
  HRESULT hRes;

  if (nUserInfoLen == (SIZE_T)-1)
    nUserInfoLen = StrLenA(szUserInfoA);
  if (nUserInfoLen == 0)
  {
    cStrFragmentW.Empty();
    return S_OK;
  }
  if (szUserInfoA == NULL)
    return E_POINTER;
  //decode percent-encoded characters
  hRes = Decode(cStrTempA, szUserInfoA, nUserInfoLen);
  if (SUCCEEDED(hRes))
    hRes = Utf8_Decode(cStrTempW, (LPCSTR)cStrTempA, cStrTempA.GetLength());
  if (SUCCEEDED(hRes))
    hRes = SetUserInfo((LPCWSTR)cStrTempW, cStrTempW.GetLength());
  //done
  return hRes;
}

HRESULT CUrl::SetUserInfo(__in_z LPCWSTR szUserInfoW, __in_opt SIZE_T nUserInfoLen)
{
  CStringW cStrTempW;

  if (nUserInfoLen == (SIZE_T)-1)
    nUserInfoLen = StrLenW(szUserInfoW);
  if (nUserInfoLen == 0)
  {
    cStrUserInfoW.Empty();
    return S_OK;
  }
  //set new value
  if (szUserInfoW == NULL)
    return E_POINTER;
  if (cStrTempW.CopyN(szUserInfoW, nUserInfoLen) == FALSE)
    return E_OUTOFMEMORY;
  //done
  cStrFragmentW.Attach(cStrTempW.Detach());
  return S_OK;
}

LPCWSTR CUrl::GetUserInfo() const
{
  return (LPCWSTR)cStrUserInfoW;
}

HRESULT CUrl::ToString(__inout CStringA &cStrDestA, __in int nFlags)
{
  eScheme nSchemeType;
  CStringA cStrTempA;
  CStringW cStrTempW;
  SIZE_T i, nCount;
  LPCWSTR sW;
  HRESULT hRes;

  cStrDestA.Empty();
  nSchemeType = Scheme2Enum((LPCWSTR)cStrSchemeW);
  switch (nSchemeType)
  {
    case CUrl::SchemeNone:
      //no scheme, check if path is relative else assume file
      sW = (LPCWSTR)cStrPathW;
      //count the number of slashes
      for (i=0; sW[i]=='/'; i++);
      //check 4 drive letter specification
      if (IsAlpha(sW[i]) != FALSE && (sW[i+1] == L':' || sW[i+1] == L'|'))
      {
        //drive letter?
        if ((nFlags & ToStringAddScheme) != 0)
        {
          if (cStrDestA.Concat("file://") == FALSE)
          {
err_nomem:  hRes = E_OUTOFMEMORY;
            goto done;
          }
        }
      }
      else if (i > 0)
      {
        if ((nFlags & ToStringAddScheme) != 0)
        {
          if (cStrDestA.Concat("file://") == FALSE)
            goto err_nomem;
        }
      }
      if ((nFlags & ToStringAddPath) != 0)
      {
        if (cStrPathW.IsEmpty() != FALSE)
        {
          if (cStrDestA.Concat("/") == FALSE)
            goto err_nomem;
        }
        else
        {
          if (cStrTempW.Copy((LPCWSTR)cStrPathW) == FALSE)
            goto err_nomem;
          if ((nFlags & ToStringShrinkPath) == 0)
            hRes = NormalizePath(cStrTempW, (LPCWSTR)cStrSchemeW);
          else
            hRes = ReducePath(cStrTempW, (LPCWSTR)cStrSchemeW);
          if (SUCCEEDED(hRes))
            hRes = ToStringEncode(cStrDestA, (LPCWSTR)cStrTempW, cStrTempW.GetLength(), "/@:");
          if (FAILED(hRes))
            goto done;
        }
      }
      break;

    case CUrl::SchemeMailTo:
    case CUrl::SchemeNews:
      if ((nFlags & ToStringAddScheme) != 0)
      {
        if (cStrDestA.Concat((nSchemeType == CUrl::SchemeMailTo) ? "mailto:" : "news:") == FALSE)
          goto err_nomem;
      }
      if ((nFlags & ToStringAddPath) != 0)
      {
        hRes = ToStringEncode(cStrDestA, (LPCWSTR)cStrTempW, cStrTempW.GetLength(), "/@:");
        if (FAILED(hRes))
          goto done;
      }
      break;

    default:
      if ((nFlags & ToStringAddScheme) != 0)
      {
        if (cStrDestA.Concat((LPCWSTR)cStrSchemeW) == FALSE ||
            cStrDestA.Concat("://") == FALSE)
          goto err_nomem;
      }
      //add host
      if ((nFlags & ToStringAddHostPort) != 0 && cStrHostW.IsEmpty() == FALSE &&
          nSchemeType != CUrl::SchemeFile)
      {
        if ((nFlags & ToStringAddUserInfo) != 0 && cStrUserInfoW.IsEmpty() == FALSE)
        {
          hRes = ToStringEncode(cStrDestA, (LPCWSTR)cStrTempW, cStrTempW.GetLength(), "!$&'()*+,;=:");
          if (FAILED(hRes))
            goto done;
          if (cStrDestA.Concat("@") == FALSE)
            goto err_nomem;
        }
        //use idna
        hRes = Punycode_Encode(cStrTempA, (LPWSTR)cStrHostW, cStrHostW.GetLength());
        if (FAILED(hRes))
          goto done;
        if (cStrDestA.Concat((LPCSTR)cStrTempA) == FALSE)
          goto err_nomem;
        //add port
        if (nPort >= 0)
        {
          for (i=0; aValidSchemes[i].szNameW!=NULL; i++)
          {
            if (aValidSchemes[i].nCode == nSchemeType)
              break;
          }
          if (aValidSchemes[i].szNameW == NULL || aValidSchemes[i].nDefaultPort != nPort)
          {
            if (cStrDestA.AppendFormat(":%ld", nPort) == FALSE)
              goto err_nomem;
          }
        }
      }
      //add path
      if ((nFlags & ToStringAddPath) != 0)
      {
        if (cStrPathW.IsEmpty() != FALSE)
        {
          if (cStrDestA.Concat("/") == FALSE)
            goto err_nomem;
        }
        else
        {
          if (cStrTempW.Copy((LPCWSTR)cStrPathW) == FALSE)
            goto err_nomem;
          if ((nFlags & ToStringShrinkPath) == 0)
            hRes = NormalizePath(cStrTempW, (LPCWSTR)cStrSchemeW);
          else
            hRes = ReducePath(cStrTempW, (LPCWSTR)cStrSchemeW);
          if (SUCCEEDED(hRes))
            hRes = ToStringEncode(cStrDestA, (LPCWSTR)cStrTempW, cStrTempW.GetLength(), "/");
          if (FAILED(hRes))
            goto done;
        }
      }
      break;
  }
  //query strings
  if ((nFlags & ToStringAddQueryStrings) != 0)
  {
    nCount = cQueryStringsList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (cStrDestA.Concat((i == 0) ? "?" : "&") == FALSE)
        goto err_nomem;
      hRes = ToStringEncode(cStrDestA, cQueryStringsList[i]->szNameW, StrLenW(cQueryStringsList[i]->szNameW), NULL);
      if (FAILED(hRes))
        goto done;
      if (cQueryStringsList[i]->szValueW[0] != 0)
      {
        if (cStrDestA.Concat("=") == FALSE)
          goto err_nomem;
        hRes = ToStringEncode(cStrDestA, cQueryStringsList[i]->szValueW, StrLenW(cQueryStringsList[i]->szValueW), NULL);
        if (FAILED(hRes))
          goto done;
      }
    }
  }
  //fragment
  if ((nFlags & ToStringAddFragment) != 0 && cStrFragmentW.IsEmpty() == FALSE)
  {
    if (cStrDestA.Concat("#") == FALSE)
      goto err_nomem;
    hRes = ToStringEncode(cStrDestA, cQueryStringsList[i]->szValueW, StrLenW(cQueryStringsList[i]->szValueW), NULL);
    if (FAILED(hRes))
      goto done;
  }
  //done
  hRes = S_OK;

done:
  if (FAILED(hRes))
    cStrDestA.Empty();
  return hRes;
}

HRESULT CUrl::ToString(__inout CStringW &cStrDestW, __in int nFlags)
{
  eScheme nSchemeType;
  CStringA cStrTempA;
  CStringW cStrTempW;
  SIZE_T i, nCount;
  LPCWSTR sW;
  HRESULT hRes;

  cStrDestW.Empty();
  nSchemeType = Scheme2Enum((LPCWSTR)cStrSchemeW);
  switch (nSchemeType)
  {
    case CUrl::SchemeNone:
      //no scheme, check if path is relative else assume file
      sW = (LPCWSTR)cStrPathW;
      //count the number of slashes
      for (i=0; sW[i]=='/'; i++);
      //check 4 drive letter specification
      if (IsAlpha(sW[i]) != FALSE && (sW[i+1] == L':' || sW[i+1] == L'|'))
      {
        //drive letter?
        if ((nFlags & ToStringAddScheme) != 0)
        {
          if (cStrDestW.Concat(L"file://") == FALSE)
          {
err_nomem:  hRes = E_OUTOFMEMORY;
            goto done;
          }
        }
      }
      else if (i > 0)
      {
        if ((nFlags & ToStringAddScheme) != 0)
        {
          if (cStrDestW.Concat(L"file://") == FALSE)
            goto err_nomem;
        }
      }
      if ((nFlags & ToStringAddPath) != 0)
      {
        if (cStrPathW.IsEmpty() != FALSE)
        {
          if (cStrDestW.Concat(L"/") == FALSE)
            goto err_nomem;
        }
        else
        {
          if (cStrTempW.Copy((LPCWSTR)cStrPathW) == FALSE)
            goto err_nomem;
          if ((nFlags & ToStringShrinkPath) == 0)
            hRes = NormalizePath(cStrTempW, (LPCWSTR)cStrSchemeW);
          else
            hRes = ReducePath(cStrTempW, (LPCWSTR)cStrSchemeW);
          if (SUCCEEDED(hRes))
            hRes = ToStringEncode(cStrDestW, (LPCWSTR)cStrTempW, cStrTempW.GetLength(), L"/@:");
          if (FAILED(hRes))
            goto done;
        }
      }
      break;

    case CUrl::SchemeMailTo:
    case CUrl::SchemeNews:
      if ((nFlags & ToStringAddScheme) != 0)
      {
        if (cStrDestW.Concat((nSchemeType == CUrl::SchemeMailTo) ? L"mailto:" : L"news:") == FALSE)
          goto err_nomem;
      }
      if ((nFlags & ToStringAddPath) != 0)
      {
        hRes = ToStringEncode(cStrDestW, (LPCWSTR)cStrTempW, cStrTempW.GetLength(), L"/@:");
        if (FAILED(hRes))
          goto done;
      }
      break;

    default:
      if ((nFlags & ToStringAddScheme) != 0)
      {
        if (cStrDestW.Concat((LPCWSTR)cStrSchemeW) == FALSE ||
            cStrDestW.Concat(L"://") == FALSE)
            goto err_nomem;
      }
      //add host
      if ((nFlags & ToStringAddHostPort) != 0 && cStrHostW.IsEmpty() == FALSE &&
          nSchemeType != CUrl::SchemeFile)
      {
        if ((nFlags & ToStringAddUserInfo) != 0 && cStrUserInfoW.IsEmpty() == FALSE)
        {
          hRes = ToStringEncode(cStrDestW, (LPCWSTR)cStrTempW, cStrTempW.GetLength(), L"!$&'()*+,;=:");
          if (FAILED(hRes))
            goto done;
          if (cStrDestW.Concat(L"@") == FALSE)
            goto err_nomem;
        }
        //add host
        if (cStrDestW.Concat((LPCWSTR)cStrHostW) == FALSE)
          goto err_nomem;
        //add port
        if (nPort >= 0)
        {
          for (i=0; aValidSchemes[i].szNameW!=NULL; i++)
          {
            if (aValidSchemes[i].nCode == nSchemeType)
              break;
          }
          if (aValidSchemes[i].szNameW == NULL || aValidSchemes[i].nDefaultPort != nPort)
          {
            if (cStrDestW.AppendFormat(L":%ld", nPort) == FALSE)
              goto err_nomem;
          }
        }
      }
      //add path
      if ((nFlags & ToStringAddPath) != 0)
      {
        if (cStrPathW.IsEmpty() != FALSE)
        {
          if (cStrDestW.Concat(L"/") == FALSE)
            goto err_nomem;
        }
        else
        {
          if (cStrTempW.Copy((LPCWSTR)cStrPathW) == FALSE)
            goto err_nomem;
          if ((nFlags & ToStringShrinkPath) == 0)
            hRes = NormalizePath(cStrTempW, (LPCWSTR)cStrSchemeW);
          else
            hRes = ReducePath(cStrTempW, (LPCWSTR)cStrSchemeW);
          if (SUCCEEDED(hRes))
            hRes = ToStringEncode(cStrDestW, (LPCWSTR)cStrTempW, cStrTempW.GetLength(), L"/");
          if (FAILED(hRes))
            goto done;
        }
      }
      break;
  }
  //query strings
  if ((nFlags & ToStringAddQueryStrings) != 0)
  {
    nCount = cQueryStringsList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (cStrDestW.Concat((i == 0) ? L"?" : L"&") == FALSE)
        goto err_nomem;
      hRes = ToStringEncode(cStrDestW, cQueryStringsList[i]->szNameW, StrLenW(cQueryStringsList[i]->szNameW), NULL);
      if (FAILED(hRes))
        goto done;
      if (cQueryStringsList[i]->szValueW[0] != 0)
      {
        if (cStrDestW.Concat(L"=") == FALSE)
          goto err_nomem;
        hRes = ToStringEncode(cStrDestW, cQueryStringsList[i]->szValueW, StrLenW(cQueryStringsList[i]->szValueW), NULL);
        if (FAILED(hRes))
          goto done;
      }
    }
  }
  //fragment
  if ((nFlags & ToStringAddFragment) != 0 && cStrFragmentW.IsEmpty() == FALSE)
  {
    if (cStrDestW.Concat(L"#") == FALSE)
      goto err_nomem;
    hRes = ToStringEncode(cStrDestW, cQueryStringsList[i]->szValueW, StrLenW(cQueryStringsList[i]->szValueW), NULL);
    if (FAILED(hRes))
      goto done;
  }
  //done
  hRes = S_OK;

done:
  if (FAILED(hRes))
    cStrDestW.Empty();
  return hRes;
}

HRESULT CUrl::ParseFromString(__in_z LPCSTR szUrlA, __in_opt SIZE_T nSrcLen)
{
  CStringA cStrTempA, cStrTempHostA, cStrTempUserInfoA;
  eScheme nSchemeType;
  LPCSTR sA;
  SIZE_T i, nPos;
  HRESULT hRes;

  //clean
  Reset();
  nSchemeType = CUrl::SchemeUnknown;
  //check parameters
  if (nSrcLen == (SIZE_T)-1)
    nSrcLen = StrLenA(szUrlA);
  if (szUrlA == NULL && nSrcLen > 0)
    return E_POINTER;
  //skip 'url:' prefix
  while (nSrcLen >= 4 && StrNCompareA(szUrlA, "URL:", TRUE, 4) == 0)
  {
    szUrlA += 4;
    nSrcLen -= 4;
  }
  //nothing to parse?
  if (nSrcLen == 0)
    return S_OK;
  //is scheme specified?
  nPos = FindChar(szUrlA, nSrcLen, ":", "@?#/\\");
  if (nPos == 0)
  {
err_fail:
    hRes = E_FAIL;
    goto done;
  }
  if (nPos == (SIZE_T)-1)
  {
    //no scheme detected, may be a file or a relative path
    nSchemeType = CUrl::SchemeNone;
    //count the number of slashes
    for (i=nSrcLen,sA=szUrlA; i>0 && IsSlash(*sA)!=FALSE; sA++,i--);
    i = nSrcLen - i; //'i' has the number of slashes
    if (i > 0)
    {
      //if at least one slash, assume a file so set scheme
      if (cStrSchemeW.Copy(L"file") == FALSE)
      {
err_nomem:
        hRes = E_OUTOFMEMORY;
        goto done;
      }
      nSchemeType = CUrl::SchemeFile;
    }
    //if there are more than two slashes in the front, leave only two
    if (i > 2)
    {
      szUrlA += (i-2);
      nSrcLen -= (i-2);
    }
    //copy the rest to the path
    goto copy_path;
  }

  //is a windows local file descriptor?
  if (nPos == 1)
  {
    //there must be a letter and an absolute path
    if (IsAlpha(*szUrlA) == FALSE || IsSlash(szUrlA[2]) == FALSE)
      goto err_fail;
    if (cStrSchemeW.Copy(L"file") == FALSE)
      goto err_nomem;
    nSchemeType = CUrl::SchemeFile;
    goto copy_path;
  }

  //it is a valid (?) scheme
  if (cStrTempA.CopyN(szUrlA, nPos) == FALSE)
    goto err_nomem;
  hRes = SetScheme((LPCSTR)cStrTempA);
  if (FAILED(hRes))
    goto done;
  szUrlA += nPos+1; //skip scheme
  nSrcLen -= nPos+1;
  nSchemeType = Scheme2Enum((LPCWSTR)cStrSchemeW);
  if (nSchemeType == CUrl::SchemeMailTo || nSchemeType == CUrl::SchemeNews)
  {
    //get mail/domain from szUrlW
    nPos = FindChar(szUrlA, nSrcLen, "?#", NULL);
    if (nPos == (SIZE_T)-1)
      nPos = nSrcLen;
    //copy mail/domain to path
    hRes = Decode(cStrTempA, szUrlA, nPos);
    if (SUCCEEDED(hRes))
      hRes = Utf8_Decode(cStrPathW, (LPCSTR)cStrTempA, cStrTempA.GetLength());
    if (FAILED(hRes))
      goto done;
    szUrlA += nPos;
    nSrcLen -= nPos;
    //do check validity
    if (IsValidEMailAddress((LPCWSTR)cStrPathW, cStrPathW.GetLength()) == FALSE)
    {
      if (nSchemeType != CUrl::SchemeNews)
        goto err_fail;
      hRes = ValidateHostAddress(cStrPathW);
      if (FAILED(hRes))
        goto done;
    }
    goto parse_params;
  }

  //not a mail nor news... skip double slash if present
  if (nSrcLen >= 2 && IsSlash(*szUrlA) != FALSE && IsSlash(szUrlA[1]) != FALSE)
  {
    szUrlA += 2;
    nSrcLen -= 2;
  }

  //get host
  nPos = FindChar(szUrlA, nSrcLen, "/\\", "?#");
  if (nPos == (SIZE_T)-1)
    nPos = nSrcLen;
  if (cStrTempHostA.CopyN(szUrlA, nPos) == FALSE)
    goto err_nomem;
  //check special case for file uri'sW where host name, if exists, is "localhost"
  switch (nSchemeType)
  {
    case CUrl::SchemeFile:
      if (cStrTempHostA.IsEmpty() == FALSE && IsLocalHost((LPCSTR)cStrTempHostA) != FALSE)
      {
        //strip
        szUrlA += nPos;
        nSrcLen -= nPos;
      }
      cStrTempHostA.Empty();
      //count the number of slashes
      for (i=nSrcLen,sA=szUrlA; i>0 && IsSlash(*sA)!=FALSE; sA++,i--);
      i = nSrcLen - i; //'i' has the number of slashes
      //if there are more than two slashes in the front, leave only two
      if (i > 2)
      {
        szUrlA += (i-2);
        nSrcLen -= (i-2);
      }
      break;

    default:
      //host item is valid (?)
      szUrlA += nPos;
      nSrcLen -= nPos;
      break;
  }

  //well, at this point we are able to copy the path
copy_path:
  nPos = FindChar(szUrlA, nSrcLen, "?#", NULL);
  if (nPos == (SIZE_T)-1)
    nPos = nSrcLen;
  if (cStrTempA.CopyN(szUrlA, nPos) == FALSE)
    goto err_nomem;
  szUrlA += nPos;
  nSrcLen -= nPos;
  for (sA=(LPCSTR)cStrTempA; *sA!=0; sA++)
  {
    if (*sA == '\\')
      *((LPSTR)sA) = '/';
  }
  if (cStrTempA.IsEmpty() == FALSE)
  {
    //check if first slash if present
    sA = (LPCSTR)cStrTempA;
    for (i=0; sA[i]=='/'; i++);
    //check 4 drive letter specification
    if (IsAlpha(sA[i]) != FALSE && (sA[i+1] == ':' || sA[i+1] == '|'))
    {
      CHAR chA = sA[i];

      //drive letter?
      cStrTempA.Delete(0, i);
      if (cStrTempA.Insert("/X:", 0) == FALSE)
        goto err_nomem;
      ((LPSTR)cStrTempA)[1] = chA;
    }
  }
  //add the rest
  hRes = SetPath((LPCSTR)cStrTempA, cStrTempA.GetLength());
  if (FAILED(hRes))
    goto done;
  //split host
  if (cStrTempHostA.IsEmpty() == FALSE)
  {
    //extract user info from host if any
    sA = (LPCSTR)cStrTempHostA;
    nPos = FindChar(sA, cStrTempHostA.GetLength(), "@", NULL);
    if (nPos == 0)
      goto err_fail;
    if (nPos != (SIZE_T)-1)
    {
      if (cStrTempUserInfoA.CopyN(sA, nPos) == FALSE)
        goto err_nomem;
      cStrTempHostA.Delete(0, nPos+1);
    }
    //extract port
    nPos = FindChar(sA, StrLenA(sA), ":", NULL);
    if (nPos == 0)
      goto err_fail;
    if (nPos != (SIZE_T)-1)
    {
      sA += nPos+1;
      nPort = 0;
      while (*sA >= '0' && *sA <= '9')
      {
        nPort = nPort * 10 + (int)(*sA - '0');
        if (nPort > 65535)
          goto err_fail;
        sA++;
      }
      if (*sA != 0 || nPort < 1)
        goto err_fail;
      cStrTempHostA.Delete(nPos, (SIZE_T)-1);
    }
  }

  //prepend slash into path if absolute uri
  if (cStrSchemeW.IsEmpty() == FALSE &&
      (cStrPathW.IsEmpty() != FALSE || ((LPCWSTR)cStrPathW)[0] != L'/'))
  {
    if (cStrPathW.Insert(L"/", 0) == FALSE)
      goto err_nomem;
  }
  //at this point, we have..
  // a) cStrTempHostW with the host
  // b) cStrTempUserInfoW with the user info
  hRes = SetHost((LPCSTR)cStrTempHostA);
  if (SUCCEEDED(hRes))
    hRes = SetUserInfo((LPCSTR)cStrTempUserInfoA, cStrTempUserInfoA.GetLength());
  if (FAILED(hRes))
    goto done;

  //are query parameters specified?
parse_params:
  if (nSrcLen > 0 && *szUrlA == '?')
  {
    //copy query parameters
    szUrlA++;
    nSrcLen--;
    while (nSrcLen > 0 && *szUrlA != 0)
    {
      nPos = FindChar(szUrlA, nSrcLen, "#&", NULL);
      if (nPos == (SIZE_T)-1)
        nPos = nSrcLen;
      if (nPos == 0)
      {
        if (*szUrlA == L'&')
        {
          szUrlA++;
          nSrcLen--;
          continue;
        }
        break; //fragment separator found
      }
      //find the = sign
      i = FindChar(szUrlA, nPos, "=", NULL);
      if (i == (SIZE_T)-1)
        i = nPos;
      if (i > 0)
      {
        //from 0 to 'i' we have the name
        //from 'i+1' to 'nPos' we have the value
        hRes = AddQueryString(szUrlA, szUrlA+(i+1), nPos-(i+1), i);
        if (FAILED(hRes))
          goto done;
      }
      szUrlA += nPos;
      nSrcLen -= nPos;
    }
  }

  //is fragment specified?
  if (nSrcLen > 0 && *szUrlA == '#')
  {
    //copy fragment
    hRes = SetFragment(szUrlA+1, nSrcLen-1);
    if (FAILED(hRes))
      goto done;
  }
  if (nSchemeType != CUrl::SchemeMailTo && nSchemeType != CUrl::SchemeNews)
  {
    hRes = NormalizePath(cStrPathW, (LPCWSTR)cStrSchemeW);
    if (FAILED(hRes))
      goto done;
  }

  //done
  hRes = S_OK;

done:
  if (FAILED(hRes))
    Reset();
  return hRes;
}

HRESULT CUrl::ParseFromString(__in_z LPCWSTR szUrlW, __in_opt SIZE_T nSrcLen)
{
  CStringW cStrTempW, cStrTempHostW, cStrTempUserInfoW;
  eScheme nSchemeType;
  LPCWSTR sW;
  SIZE_T i, nPos;
  HRESULT hRes;

  //clean
  Reset();
  //check parameters
  if (nSrcLen == (SIZE_T)-1)
    nSrcLen = StrLenW(szUrlW);
  if (szUrlW == NULL && nSrcLen > 0)
    return E_POINTER;
  //skip 'url:' prefix
  while (nSrcLen >= 4 && StrNCompareW(szUrlW, L"URL:", TRUE, 4) == 0)
  {
    szUrlW += 4;
    nSrcLen -= 4;
  }
  //nothing to parse?
  if (nSrcLen == 0)
    return S_OK;
  //is scheme specified?
  nPos = FindChar(szUrlW, nSrcLen, L":", L"@?#/\\");
  if (nPos == 0)
  {
err_fail:
    hRes = E_FAIL;
    goto done;
  }
  if (nPos == (SIZE_T)-1)
  {
    //no scheme detected, may be a file or a relative path
    //count the number of slashes
    for (i=nSrcLen, sW=szUrlW; i>0 && IsSlash(*sW)!=FALSE; sW++,i--);
    i = nSrcLen - i; //'i' has the number of slashes
    if (i > 0)
    {
      //if at least one slash, assume a file so set scheme
      if (cStrSchemeW.Copy(L"file") == FALSE)
      {
err_nomem:
        hRes = E_OUTOFMEMORY;
        goto done;
      }
    }
    //if there are more than two slashes in the front, leave only two
    if (i > 2)
    {
      szUrlW += (i-2);
      nSrcLen -= (i-2);
    }
    //copy the rest to the path
    goto copy_path;
  }

  //is a windows local file descriptor?
  if (nPos == 1)
  {
    //there must be a letter and an absolute path
    if (IsAlpha(*szUrlW) == FALSE || IsSlash(szUrlW[2]) == FALSE)
      goto err_fail;
    if (cStrSchemeW.Copy(L"file") == FALSE)
      goto err_nomem;
    goto copy_path;
  }

  //it is a valid (?) scheme
  if (cStrTempW.CopyN(szUrlW, nPos) == FALSE)
    goto err_nomem;
  hRes = SetScheme((LPCWSTR)cStrTempW);
  if (FAILED(hRes))
    goto done;
  szUrlW += nPos+1; //skip scheme
  nSrcLen -= nPos+1;
  nSchemeType = Scheme2Enum((LPCWSTR)cStrSchemeW);
  if (nSchemeType == CUrl::SchemeMailTo || nSchemeType == CUrl::SchemeNews)
  {
    //get mail/domain from szUrlW
    nPos = FindChar(szUrlW, nSrcLen, L"?#", NULL);
    if (nPos == (SIZE_T)-1)
      nPos = nSrcLen;
    //copy mail/domain to path
    if (cStrPathW.CopyN(szUrlW, nPos) == FALSE)
      goto err_nomem;
    szUrlW += nPos;
    nSrcLen -= nPos;
    //do check validity
    if (IsValidEMailAddress((LPCWSTR)cStrPathW, cStrPathW.GetLength()) == FALSE)
    {
      if (nSchemeType != CUrl::SchemeNews)
        goto err_fail;
      hRes = ValidateHostAddress(cStrPathW);
      if (FAILED(hRes))
        goto done;
    }
    goto parse_params;
  }

  //not a mail nor news... skip double slash if present
  if (nSrcLen >= 2 && IsSlash(*szUrlW) != FALSE && IsSlash(szUrlW[1]) != FALSE)
  {
    szUrlW += 2;
    nSrcLen -= 2;
  }

  //get host
  nPos = FindChar(szUrlW, nSrcLen, L"/\\", L"?#");
  if (nPos == (SIZE_T)-1)
    nPos = nSrcLen;
  if (cStrTempHostW.CopyN(szUrlW, nPos) == FALSE)
    goto err_nomem;
  //check special case for file uri'sW where host name, if exists, is "localhost"
  switch (nSchemeType)
  {
    case CUrl::SchemeFile:
      if (cStrTempHostW.IsEmpty() == FALSE && IsLocalHost((LPCWSTR)cStrTempHostW) != FALSE)
      {
        //strip
        szUrlW += nPos;
        nSrcLen -= nPos;
      }
      cStrTempHostW.Empty();
      //count the number of slashes
      for (i=nSrcLen, sW=szUrlW; i>0 && IsSlash(*sW)!=FALSE; sW++,i--);
      i = nSrcLen - i; //'i' has the number of slashes
      //if there are more than two slashes in the front, leave only two
      if (i > 2)
      {
        szUrlW += (i-2);
        nSrcLen -= (i-2);
      }
      break;

    default:
      //host item is valid (?)
      szUrlW += nPos;
      nSrcLen -= nPos;
      break;
  }

  //well, at this point we are able to copy the path
copy_path:
  nPos = FindChar(szUrlW, nSrcLen, L"?#", NULL);
  if (nPos == (SIZE_T)-1)
    nPos = nSrcLen;
  if (cStrTempW.CopyN(szUrlW, nPos) == FALSE)
    goto err_nomem;
  szUrlW += nPos;
  nSrcLen -= nPos;
  for (sW=(LPCWSTR)cStrTempW; *sW!=0; sW++)
  {
    if (*sW == L'\\')
      *((LPWSTR)sW) = L'/';
  }
  if (cStrTempW.IsEmpty() == FALSE)
  {
    //check if first slash if present
    sW = (LPCWSTR)cStrTempW;
    for (i=0; sW[i]==L'/'; i++);
    //check 4 drive letter specification
    if (IsAlpha(sW[i]) != FALSE && (sW[i+1] == L':' || sW[i+1] == L'|'))
    {
      WCHAR chW = sW[i];

      //drive letter?
      cStrTempW.Delete(0, i);
      if (cStrTempW.Insert(L"/X:", 0) == FALSE)
        goto err_nomem;
      ((LPWSTR)cStrTempW)[1] = chW;
    }
  }
  //add the rest
  hRes = SetPath((LPCWSTR)cStrTempW, cStrTempW.GetLength());
  if (SUCCEEDED(hRes))
    hRes = ShrinkPath();
  if (FAILED(hRes))
    goto done;
  //split host
  if (cStrTempHostW.IsEmpty() == FALSE)
  {
    //extract user info from host if any
    sW = (LPCWSTR)cStrTempHostW;
    nPos = FindChar(sW, cStrTempHostW.GetLength(), L"@", NULL);
    if (nPos == 0)
      goto err_fail;
    if (nPos != (SIZE_T)-1)
    {
      if (cStrTempUserInfoW.CopyN(sW, nPos) == FALSE)
        goto err_nomem;
      cStrTempHostW.Delete(0, nPos+1);
    }
    //extract port
    nPos = FindChar(sW, StrLenW(sW), L":", NULL);
    if (nPos == 0)
      goto err_fail;
    if (nPos != (SIZE_T)-1)
    {
      sW += nPos+1;
      nPort = 0;
      while (*sW >= L'0' && *sW <= L'9')
      {
        nPort = nPort * 10 + (int)(*sW - L'0');
        if (nPort > 65535)
          goto err_fail;
        sW++;
      }
      if (*sW != 0 || nPort < 1)
        goto err_fail;
      cStrTempHostW.Delete(nPos, (SIZE_T)-1);
    }
  }

  //prepend slash into path if absolute uri
  if (cStrSchemeW.IsEmpty() == FALSE &&
      (cStrPathW.IsEmpty() != FALSE || ((LPCWSTR)cStrPathW)[0] != L'/'))
  {
    if (cStrPathW.Insert(L"/", 0) == FALSE)
      goto err_nomem;
  }
  //at this point, we have..
  // a) cStrTempHostW with the host
  // b) cStrTempUserInfoW with the user info
  hRes = SetHost((LPCWSTR)cStrTempHostW);
  if (SUCCEEDED(hRes))
    hRes = SetUserInfo((LPCWSTR)cStrTempUserInfoW, cStrTempUserInfoW.GetLength());
  if (FAILED(hRes))
    goto done;

  //are query parameters specified?
parse_params:
  if (nSrcLen > 0 && *szUrlW == L'?')
  {
    //copy query parameters
    szUrlW++;
    nSrcLen--;
    while (nSrcLen > 0 && *szUrlW != 0)
    {
      nPos = FindChar(szUrlW, nSrcLen, L"#&", NULL);
      if (nPos == (SIZE_T)-1)
        nPos = nSrcLen;
      if (nPos == 0)
      {
        if (*szUrlW == L'&')
        {
          szUrlW++;
          nSrcLen--;
          continue;
        }
        break; //fragment separator found
      }
      //find the = sign
      i = FindChar(szUrlW, nPos, L"=", NULL);
      if (i == (SIZE_T)-1)
        i = nPos;
      if (i > 0)
      {
        //from 0 to 'i' we have the name
        //from 'i+1' to 'nPos' we have the value
        hRes = AddQueryString(szUrlW, szUrlW+(i+1), nPos-(i+1), i);
        if (FAILED(hRes))
          goto done;
      }
      szUrlW += nPos;
      nSrcLen -= nPos;
    }
  }

  //is fragment specified?
  if (nSrcLen > 0 && *szUrlW == L'#')
  {
    //copy fragment
    hRes = SetFragment(szUrlW+1, nSrcLen-1);
    if (FAILED(hRes))
      goto done;
  }
  if (nSchemeType != CUrl::SchemeMailTo && nSchemeType != CUrl::SchemeNews)
  {
    hRes = NormalizePath(cStrPathW, (LPCWSTR)cStrSchemeW);
    if (FAILED(hRes))
      goto done;
  }

  //done
  hRes = S_OK;

done:
  if (FAILED(hRes))
    Reset();
  return hRes;
}

HRESULT CUrl::operator=(__in const CUrl& cSrc)
{
  CStringW cStrSrcSchemeW, cStrSrcHostW, cStrSrcPathW, cStrSrcFragmentW, cStrSrcUserInfoW;
  TArrayListWithFree<LPQUERYSTRINGITEM> cNewQueryStringsList;
  TAutoFreePtr<QUERYSTRINGITEM> cNewItem;
  SIZE_T i, nCount, nNameLen, nValueLen;

  if (&cSrc != this)
  {
    if (cStrSrcSchemeW.Copy(cSrc.GetScheme()) == FALSE ||
        cStrSrcHostW.Copy(cSrc.GetHost()) == FALSE ||
        cStrSrcPathW.Copy(cSrc.GetPath()) == FALSE ||
        cStrSrcFragmentW.Copy(cSrc.GetFragment()) == FALSE ||
        cStrSrcUserInfoW.Copy(cSrc.GetUserInfo()) == FALSE)
      return E_OUTOFMEMORY;
    nCount = cSrc.cQueryStringsList.GetCount();
    for (i=0; i<nCount; i++)
    {
      nNameLen = StrLenW(cSrc.cQueryStringsList[i]->szNameW);
      nValueLen = StrLenW(cSrc.cQueryStringsList[i]->szValueW);
      cNewItem.Attach((LPQUERYSTRINGITEM)MX_MALLOC(sizeof(QUERYSTRINGITEM)+(nNameLen+nValueLen+1)*sizeof(WCHAR)));
      if (!cNewItem)
        return E_OUTOFMEMORY;
      cNewItem->szValueW = cNewItem->szNameW + (nNameLen + 1);
      MemCopy(cNewItem->szNameW, cSrc.cQueryStringsList[i]->szNameW, nNameLen * sizeof(WCHAR));
      cNewItem->szNameW[nNameLen] = 0;
      MemCopy(cNewItem->szValueW, cSrc.cQueryStringsList[i]->szValueW, nValueLen * sizeof(WCHAR));
      cNewItem->szValueW[nValueLen] = 0;
      if (cNewQueryStringsList.AddElement(cNewItem.Get()) == FALSE)
        return E_OUTOFMEMORY;
      cNewItem.Detach();
    }
    //done... move to the query strings list
    ResetQueryStrings();
    cQueryStringsList.Transfer(cNewQueryStringsList);
    cStrSchemeW.Attach(cStrSrcSchemeW.Detach());
    cStrHostW.Attach(cStrSrcHostW.Detach());
    cStrPathW.Attach(cStrSrcPathW.Detach());
    cStrFragmentW.Attach(cStrSrcFragmentW.Detach());
    cStrUserInfoW.Attach(cStrSrcUserInfoW.Detach());
    nPort = cSrc.GetPort();
  }
  return S_OK;
}

HRESULT CUrl::operator+=(__in const CUrl& cOtherUrl)
{
  return Merge(cOtherUrl);
}

HRESULT CUrl::Merge(__in const CUrl& cOtherUrl)
{
  TAutoFreePtr<QUERYSTRINGITEM> cNewItem;
  CStringA cStrTempPathA, cStrTempSubPathA;
  struct tagPARTS {
    eScheme nType;
    LPCWSTR szHostW;
    LPCWSTR szPathW;
    SIZE_T nPathLength;
    BOOL bPathIsAbsolute;
    LPCWSTR szSubPathW;
    SIZE_T nSubPathLength;
  } sBase, sOther, *lpParts;
  LPCWSTR sW;
  SIZE_T i, k, nCount, nNameLen, nValueLen;
  HRESULT hRes;

  sBase.nType = GetSchemeCode();
  sOther.nType = cOtherUrl.GetSchemeCode();
  sBase.szHostW = GetHost();
  sOther.szHostW = cOtherUrl.GetHost();
  sBase.szPathW = GetPath();
  sOther.szPathW = cOtherUrl.GetPath();
  for (k=1; k<=2; k++)
  {
    lpParts = (k == 1) ? &sBase : &sOther;
    sW = lpParts->szPathW;
    //count the number of slashes
    for (i=0; sW[i]==L'/'; i++);
    //check 4 drive letter specification
    if (IsAlpha(sW[i]) != FALSE && (sW[i+1] == L':' || sW[i+1] == L'|'))
    {
      //drive letter?
      if (lpParts->nType == CUrl::SchemeNone)
        lpParts->nType = CUrl::SchemeFile;
      lpParts->bPathIsAbsolute = TRUE;
    }
    else if (i > 0)
    {
      if (lpParts->nType == CUrl::SchemeNone)
        lpParts->nType = CUrl::SchemeFile;
      lpParts->bPathIsAbsolute = TRUE;
    }
    else if (lpParts->nType == CUrl::SchemeNews ||
             lpParts->nType == CUrl::SchemeMailTo ||
             lpParts->nType == CUrl::SchemeUnknown)
    {
      lpParts->bPathIsAbsolute = TRUE;
    }
    else if (lpParts->szHostW[0] != 0)
    {
      if (lpParts->nType == CUrl::SchemeNone)
        lpParts->nType = CUrl::SchemeHttp;
      lpParts->bPathIsAbsolute = TRUE;
    }
    else
    {
      lpParts->bPathIsAbsolute = FALSE;
    }
    lpParts->nPathLength = StrLenW(lpParts->szPathW);
    lpParts->szSubPathW = NULL;
    lpParts->nSubPathLength = 0;
  }

  //RFC 2396 - Section 5.2 - Step 3 Paragraph II
  if (sOther.nType == sBase.nType)
  {
    sOther.nType = CUrl::SchemeNone;
  }
  else if (sOther.nType != CUrl::SchemeNone)
  {
    //if scheme changed... 'sOther' is absolute
    if (cStrSchemeW.Copy((LPCWSTR)(cOtherUrl.cStrSchemeW)) == FALSE ||
        cStrHostW.Copy((LPCWSTR)(cOtherUrl.cStrHostW)) == FALSE ||
        cStrPathW.CopyN(sOther.szPathW, sOther.nPathLength) == FALSE)
      return E_OUTOFMEMORY;
    hRes = ReducePath(cStrPathW, (LPCWSTR)cStrSchemeW);
    if (FAILED(hRes))
      return hRes;
    goto merge_copyqueryandfrag;
  }

  //RFC 2396 - Section 5.2 - Step 2
  if (sOther.nType == CUrl::SchemeNone && *sOther.szHostW == 0 && sOther.nPathLength == 0)
    goto merge_copyqueryandfrag;

  //RFC 2396 - Section 5.2 - Step 4 (check if host is different)
  if (sOther.szHostW[0] != 0)
  {
    if (sOther.nType != CUrl::SchemeNone)
    {
      if (cStrSchemeW.Copy((LPCWSTR)(cOtherUrl.cStrSchemeW)) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrHostW.Copy((LPCWSTR)(cOtherUrl.cStrHostW)) == FALSE ||
        cStrPathW.CopyN(sOther.szPathW, sOther.nPathLength) == FALSE)
      return E_OUTOFMEMORY;
    hRes = ReducePath(cStrPathW, (LPCWSTR)cStrSchemeW);
    if (FAILED(hRes))
      return hRes;
    goto merge_copyqueryandfrag;
  }

  //RFC 2396 - Section 5.2 - Step 5 (check if path is absolute)
  if (sOther.nPathLength > 0 && sOther.bPathIsAbsolute != FALSE)
  {
    if (cStrPathW.CopyN(sOther.szPathW, sOther.nPathLength) == FALSE)
      return E_OUTOFMEMORY;
  }
  else
  {
    //process relative path
    for (i=sBase.nPathLength; i>0 && sBase.szPathW[i-1] != L'/'; i--);
    if (i == 0)
      i = sBase.nPathLength;
    if (cStrPathW.IsEmpty() == FALSE)
      cStrPathW.Delete(i, (SIZE_T)-1);
    if (cStrPathW.ConcatN(L"/", 1) == FALSE ||
        cStrPathW.ConcatN(sOther.szPathW, sOther.nPathLength) == FALSE)
      return E_OUTOFMEMORY;
  }
  //reduce path
  hRes = ReducePath(cStrPathW, (LPCWSTR)cStrSchemeW);
  if (FAILED(hRes))
    return hRes;

merge_copyqueryandfrag:
  ResetQueryStrings();
  nCount = cOtherUrl.cQueryStringsList.GetCount();
  for (i=0; i<nCount; i++)
  {
    nNameLen = StrLenW(cOtherUrl.cQueryStringsList[i]->szNameW);
    nValueLen = StrLenW(cOtherUrl.cQueryStringsList[i]->szValueW);
    cNewItem.Attach((LPQUERYSTRINGITEM)MX_MALLOC(sizeof(QUERYSTRINGITEM)+(nNameLen+nValueLen+1)*sizeof(WCHAR)));
    if (!cNewItem)
      return E_OUTOFMEMORY;
    cNewItem->szValueW = cNewItem->szNameW + (nNameLen + 1);
    MemCopy(cNewItem->szNameW, cOtherUrl.cQueryStringsList[i]->szNameW, nNameLen * sizeof(WCHAR));
    cNewItem->szNameW[nNameLen] = 0;
    MemCopy(cNewItem->szValueW, cOtherUrl.cQueryStringsList[i]->szValueW, nValueLen * sizeof(WCHAR));
    cNewItem->szValueW[nValueLen] = 0;
    if (cQueryStringsList.AddElement(cNewItem.Get()) == FALSE)
      return E_OUTOFMEMORY;
    cNewItem.Detach();
  }
  if (cStrFragmentW.Copy((LPCWSTR)(cOtherUrl.cStrFragmentW)) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

HRESULT CUrl::Encode(__inout CStringA &cStrDestA, __in_z LPCSTR szUrlA, __in_opt SIZE_T nUrlLen,
                     __in_z_opt LPCSTR szAllowedCharsA, __in_opt BOOL bAppend)
{
  CHAR chA[3];
  LPCSTR szStartA;

  if (bAppend == FALSE)
    cStrDestA.Empty();
  if (nUrlLen == (SIZE_T)-1)
    nUrlLen = StrLenA(szUrlA);
  if (nUrlLen > 0 && szUrlA == NULL)
    return E_POINTER;
  while (nUrlLen > 0)
  {
    szStartA = szUrlA;
    while (nUrlLen > 0 &&
           (IsUnreservedChar(*szUrlA) != FALSE ||
           (szAllowedCharsA != NULL && StrChrA(szAllowedCharsA, *szUrlA) != NULL)))
    {
      szUrlA++;
      nUrlLen--;
    }
    if (szUrlA > szStartA)
    {
      if (cStrDestA.ConcatN(szStartA, (SIZE_T)(szUrlA-szStartA)) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (nUrlLen > 0)
    {
      //encode char
      chA[0] = '%';
      chA[1] = szHexaNumA[((*((UCHAR*)szUrlA)) >> 4) & 0x0F];
      chA[2] = szHexaNumA[( *((UCHAR*)szUrlA)      ) & 0x0F];
      if (cStrDestA.ConcatN(chA, 3) == FALSE)
        return E_OUTOFMEMORY;
      szUrlA++;
      nUrlLen--;
    }
  }
  //done
  return S_OK;
}

SIZE_T CUrl::GetEncodedLength(__in_z LPCSTR szUrlA, __in_opt SIZE_T nUrlLen, __in_z_opt LPCSTR szAllowedCharsA)
{
  SIZE_T nCount;

  nCount = 0;
  if (nUrlLen == (SIZE_T)-1)
    nUrlLen = StrLenA(szUrlA);
  if (nUrlLen > 0 && szUrlA == NULL)
    return 0;
  while (nUrlLen > 0)
  {
    while (nUrlLen > 0 &&
           (IsUnreservedChar(*szUrlA) != FALSE ||
            (szAllowedCharsA != NULL && StrChrA(szAllowedCharsA, *szUrlA) != NULL)))
    {
      szUrlA++;
      nUrlLen--;
      nCount++;
    }
    if (nUrlLen > 0)
    {
      //encode char
      szUrlA++;
      nUrlLen--;
      nCount += 3;
    }
  }
  //done
  return nCount;
}

HRESULT CUrl::Decode(__inout CStringA &cStrDestA, __in_z LPCSTR szUrlA, __in_opt SIZE_T nUrlLen, __in_opt BOOL bAppend)
{
  CStringA cStrTempA;
  CHAR chA;
  WCHAR chW;
  LPCSTR szStartA;
  HRESULT hRes;

  if (bAppend == FALSE)
    cStrDestA.Empty();
  if (nUrlLen == (SIZE_T)-1)
    nUrlLen = StrLenA(szUrlA);
  if (nUrlLen > 0 && szUrlA == NULL)
    return 0;
  while (nUrlLen > 0)
  {
    szStartA = szUrlA;
    while (nUrlLen > 0 && *szUrlA != '%' && *szUrlA != '+' && (*szUrlA & 0x80) == 0)
    {
      szUrlA++;
      nUrlLen--;
    }
    if (szUrlA > szStartA)
    {
      if (cStrDestA.ConcatN(szStartA, (SIZE_T)(szUrlA-szStartA)) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (nUrlLen > 0)
    {
      switch (*szUrlA)
      {
        case '+':
          if (cStrDestA.ConcatN(" ", 1) == FALSE)
            return E_OUTOFMEMORY;
          szUrlA++;
          nUrlLen--;
          break;

        case '%':
          if (nUrlLen >= 3 && (chA = DecodePct(szUrlA)) != 0)
          {
            if (cStrDestA.ConcatN(&chA, 1) == FALSE)
              return E_OUTOFMEMORY;
            szUrlA += 3;
            nUrlLen -= 3;
          }
          else if (nUrlLen >= 6 && (chW = DecodePctU(szUrlA)) != 0)
          {
            hRes = Utf8_Encode(cStrTempA, &chW, 1);
            if (FAILED(hRes))
              return hRes;
            if (cStrDestA.Concat((LPSTR)cStrTempA) == FALSE)
              return E_OUTOFMEMORY;
            szUrlA += 6;
            nUrlLen -= 6;
          }
          else
          {
            if (cStrDestA.ConcatN("%", 1) == FALSE)
              return E_OUTOFMEMORY;
            szUrlA++;
            nUrlLen--;
          }
          break;

        default:
          return MX_E_InvalidData;
      }
    }
  }
  //done
  return S_OK;
}

SIZE_T CUrl::GetDecodedLength(__in_z LPCSTR szUrlA, __in_opt SIZE_T nUrlLen)
{
  CHAR chA;
  WCHAR chW;
  SIZE_T nCount;

  nCount = 0;
  if (nUrlLen == (SIZE_T)-1)
    nUrlLen = StrLenA(szUrlA);
  if (nUrlLen > 0 && szUrlA == NULL)
    return 0;
  while (nUrlLen > 0)
  {
    if ((*szUrlA & 0x80) != 0)
      return 0;
    if (*szUrlA == '%')
    {
      if (nUrlLen >= 3)
      {
        chA = DecodePct(szUrlA);
        if (chA != 0)
        {
          szUrlA += 3;
          nUrlLen -= 3;
          nCount++;
          continue;
        }
      }
      if (nUrlLen >= 6)
      {
        chW = DecodePctU(szUrlA);
        if (chW != 0)
        {
          szUrlA += 6;
          nUrlLen -= 6;
          if (chW < 0x80)
            nCount++;
          else if (chW < 0x800)
            nCount += 2;
          else
            nCount += 3;
          continue;
        }
      }
    }

    szUrlA++;
    nUrlLen--;
    nCount++;
  }
  //done
  return nCount;
}

BOOL CUrl::IsValidHostAddress(__in_z LPCSTR szHostA, __in_opt SIZE_T nHostLen)
{
  SIZE_T i;

  if (nHostLen == (SIZE_T)-1)
    nHostLen = StrLenA(szHostA);
  if (nHostLen == 0)
    return FALSE;
  //check for IPv4
  if (CHostResolver::IsValidIPV4(szHostA, nHostLen) != FALSE)
    return S_OK;
  //check for IPv6
  if (CHostResolver::IsValidIPV6(szHostA, nHostLen) != FALSE)
    return TRUE;
  if (szHostA[0] == '[' && szHostA[nHostLen-1] == ']' && CHostResolver::IsValidIPV6(szHostA+1, nHostLen-2) != FALSE)
    return TRUE;
  //check for a valid name
  if (szHostA[0] == '.' || szHostA[nHostLen-1] == '.')
    return FALSE;
  for (i=0; i<nHostLen-1; i++)
  {
    if (szHostA[i] == '.' && szHostA[i+1] == '.')
      return FALSE;
    if (StrChrA(":/?#[]@!$&'()*+,;=\"", szHostA[i]) != NULL)
      return FALSE;
  }
  return TRUE;
}

BOOL CUrl::IsValidHostAddress(__in_z LPCWSTR szHostW, __in_opt SIZE_T nHostLen)
{
  SIZE_T i;

  if (nHostLen == (SIZE_T)-1)
    nHostLen = StrLenW(szHostW);
  if (nHostLen == 0)
    return FALSE;
  //check for IPv4
  if (CHostResolver::IsValidIPV4(szHostW, nHostLen) != FALSE)
    return S_OK;
  //check for IPv6
  if (CHostResolver::IsValidIPV6(szHostW, nHostLen) != FALSE)
    return TRUE;
  if (szHostW[0] == L'[' && szHostW[nHostLen-1] == L']' && CHostResolver::IsValidIPV6(szHostW+1, nHostLen-2) != FALSE)
    return TRUE;
  //check for a valid name
  if (szHostW[0] == L'.' || szHostW[nHostLen-1] == L'.')
    return FALSE;
  for (i=0; i<nHostLen-1; i++)
  {
    if (szHostW[i] == L'.' && szHostW[i+1] == L'.')
      return FALSE;
    if (StrChrW(L":/?#[]@!$&'()*+,;=\"", szHostW[i]) != NULL)
      return FALSE;
  }
  return TRUE;
}

} //namespace MX

//-----------------------------------------------------------

static MX::CUrl::eScheme Scheme2Enum(__in_z LPCWSTR szSchemeW)
{
  SIZE_T i;

  if (szSchemeW == NULL || *szSchemeW == 0)
    return MX::CUrl::SchemeNone;
  for (i=0; aValidSchemes[i].szNameW != NULL; i++)
  {
    if (MX::StrCompareW(szSchemeW, aValidSchemes[i].szNameW, TRUE) == 0)
      return aValidSchemes[i].nCode;
  }
  return MX::CUrl::SchemeUnknown;
}

static HRESULT ValidateHostAddress(__inout MX::CStringW &cStrHostW)
{
  LPCWSTR sW;
  SIZE_T i, nLen;

  nLen = cStrHostW.GetLength();
  if (nLen == 0)
    return MX_E_InvalidData;
  sW = (LPCWSTR)cStrHostW;
  //check for IPv4
  if (MX::CHostResolver::IsValidIPV4(sW, nLen) != FALSE)
    return S_OK;
  //check for IPv6
  if (MX::CHostResolver::IsValidIPV6(sW, nLen) != FALSE)
  {
    //insert brackets
    return (cStrHostW.Insert(L"[", 0) != FALSE &&
            cStrHostW.ConcatN(L"]", 1) != FALSE) ? S_OK : E_OUTOFMEMORY;
  }
  if (nLen > 2 && sW[0] == L'[' && sW[nLen-1] == L']' &&
      MX::CHostResolver::IsValidIPV6(sW+1, nLen-2) != FALSE)
    return S_OK;
  //check for a valid name
  if (sW[0] == L'.' || sW[nLen-1] == L'.')
    return MX_E_InvalidData;
  for (i=0; i<nLen-1; i++)
  {
    if (sW[0] == L'.' && sW[1] == L'.')
      return MX_E_InvalidData;
    if (MX::StrChrW(L":/?#[]@!$&'()*+,;=\"", *sW) != NULL)
      return MX_E_InvalidData;
  }
  return S_OK;
}

static CHAR DecodePct(__in_z LPCSTR szStrA)
{
  int nDigits[2];

  if (*szStrA != '%')
    return 0;
  nDigits[0] = IsHexaDigit(szStrA[1]);
  nDigits[1] = IsHexaDigit(szStrA[2]);
  if (nDigits[0] < 0 || nDigits[1] < 0)
    return 0;
  return (CHAR)(((SIZE_T)nDigits[0] << 4) | (SIZE_T)nDigits[1]);
}

static WCHAR DecodePct(__in_z LPCWSTR szStrW)
{
  int nDigits[2];

  if (*szStrW != L'%')
    return 0;
  nDigits[0] = IsHexaDigit(szStrW[1]);
  nDigits[1] = IsHexaDigit(szStrW[2]);
  if (nDigits[0] < 0 || nDigits[1] < 0)
    return 0;
  return (WCHAR)(((SIZE_T)nDigits[0] << 4) | (SIZE_T)nDigits[1]);
}

static WCHAR DecodePctU(__in_z LPCSTR szStrA)
{
  int nDigits[4];

  if (*szStrA != '%' || (szStrA[1] != 'u' && szStrA[1] != 'U'))
    return 0;
  nDigits[0] = IsHexaDigit(szStrA[2]);
  nDigits[1] = IsHexaDigit(szStrA[3]);
  nDigits[2] = IsHexaDigit(szStrA[4]);
  nDigits[3] = IsHexaDigit(szStrA[5]);
  if (nDigits[0] < 0 || nDigits[1] < 0 || nDigits[2] < 0 || nDigits[3] < 0)
    return 0;
  return (WCHAR)(((SIZE_T)nDigits[0] << 12) | ((SIZE_T)nDigits[1] << 8) |
                 ((SIZE_T)nDigits[2] <<  4) |  (SIZE_T)nDigits[3]);
}

static WCHAR DecodePctU(__in_z LPCWSTR szStrW)
{
  int nDigits[4];

  if (*szStrW != L'%' || (szStrW[1] != L'u' && szStrW[1] != L'U'))
    return 0;
  nDigits[0] = IsHexaDigit(szStrW[2]);
  nDigits[1] = IsHexaDigit(szStrW[3]);
  nDigits[2] = IsHexaDigit(szStrW[4]);
  nDigits[3] = IsHexaDigit(szStrW[5]);
  if (nDigits[0] < 0 || nDigits[1] < 0 || nDigits[2] < 0 || nDigits[3] < 0)
    return 0;
  return (WCHAR)(((SIZE_T)nDigits[0] << 12) | ((SIZE_T)nDigits[1] << 8) |
                 ((SIZE_T)nDigits[2] <<  4) |  (SIZE_T)nDigits[3]);
}

static HRESULT NormalizePath(__inout MX::CStringW &cStrPathW, __in_z LPCWSTR szSchemeW)
{
  MX::CUrl::eScheme nSchemeType;
  BOOL bCanBeFilename;
  LPWSTR sW;
  SIZE_T k, nOfs;

  if (cStrPathW.IsEmpty() != FALSE)
    return S_OK; //nothing to do
  //convert backslashes to slashes
  for (sW=(LPWSTR)cStrPathW; *sW != 0; sW++)
  {
    if (*sW == L'\\')
      *sW = L'/';
  }
  //get scheme and gather for filename
  sW = (LPWSTR)cStrPathW;
  nSchemeType = Scheme2Enum(szSchemeW);
  bCanBeFilename = (nSchemeType == MX::CUrl::SchemeNone || nSchemeType == MX::CUrl::SchemeFile) ? TRUE : FALSE;
  //count slashes at beginning
  for (k=0; sW[k]==L'/'; k++);
  //check if it is a filename
  if (bCanBeFilename != FALSE)
  {
    if (IsAlpha(sW[k]) != FALSE && (sW[k+1] == L':' || sW[k+1] == L'|'))
    {
      //windows path... adjust by leaving only one slash
      if (k == 0)
      {
        if (cStrPathW.Insert(L"/", 0) == FALSE)
          return E_OUTOFMEMORY;
        sW = (LPWSTR)cStrPathW;
      }
      else if (k > 1)
      {
        //leave only one
        cStrPathW.Delete(0, k-1);
      }
      //replace | with : if needed
      sW[2] = L':';
      //add root slash if not specified
      if (sW[3] != L'/')
      {
        if (cStrPathW.Insert(L"/", 3) == FALSE)
          return E_OUTOFMEMORY;
        sW = (LPWSTR)cStrPathW;
      }
      //at this point we have /X:/....
      nOfs = 3;
    }
    else if (k >= 2)
    {
      //may be a windows unc path... first leave only 2 slashes
      cStrPathW.Delete(0, k-2);
      //a real unc path... skip the server name
      nOfs = 2;
      while (sW[nOfs] != 0 && sW[nOfs] != L'/')
        nOfs++;
      //and add slash after it if needed
      if (sW[nOfs] != L'/')
      {
        if (cStrPathW.Insert(L"/", nOfs) == FALSE)
          return E_OUTOFMEMORY;
      }
    }
    else
    {
      //simple absolute or relative path
      nOfs = 0;
    }
  }
  else
  {
    //leave only 1 slash at the beginning
    if (k > 1)
      cStrPathW.Delete(0, k-1);
    nOfs = 0;
  }
  //at this point we must check for double slashes starting from 'nOfs' offset
  sW = (LPWSTR)cStrPathW;
  while (sW[nOfs] != 0)
  {
    if (sW[nOfs] == L'/' && sW[nOfs+1] == L'/')
    {
      cStrPathW.Delete(nOfs+1, 1);
      continue;
    }
    nOfs++;
  }
  return S_OK;
}

static HRESULT ReducePath(__inout MX::CStringW &cStrPathW, __in_z LPCWSTR szSchemeW)
{
  MX::CUrl::eScheme nSchemeType;
  BOOL bCanBeFilename, bValid, bPrevWasSlash;
  LPWSTR sW;
  SIZE_T k, l, nStartOfs;
  HRESULT hRes;

  if (cStrPathW.IsEmpty() != FALSE)
    return S_OK; //nothing to do
  sW = (LPWSTR)cStrPathW;
  //first normalize path
  hRes = NormalizePath(cStrPathW, szSchemeW);
  if (FAILED(hRes))
    return hRes;
  nSchemeType = Scheme2Enum(szSchemeW);
  bCanBeFilename = (nSchemeType == MX::CUrl::SchemeNone || nSchemeType == MX::CUrl::SchemeFile) ? TRUE : FALSE;
  sW = (LPWSTR)cStrPathW;
  nStartOfs = 0;
  if (bCanBeFilename != FALSE)
  {
    if (*sW == L'/')
    {
      if (IsAlpha(sW[1]) != FALSE && sW[2] == L':')
      {
        nStartOfs = 3;
      }
      else if (sW[1] == L'/')
      {
        for (nStartOfs=2; sW[nStartOfs] != L'/'; nStartOfs++);
      }
    }
  }
  //begin reducing
  bValid = TRUE;
  bPrevWasSlash = TRUE; //assume we are after or in a slash
  k = nStartOfs;
  while (sW[k] != 0)
  {
    if (sW[k] == L'/')
    {
      bPrevWasSlash = TRUE;
      k++;
      continue;
    }
    if (bPrevWasSlash == FALSE)
    {
      k++;
      continue;
    }
    //check for potential dot / dot-dot
    if (sW[k] == L'.')
    {
      if (sW[k+1] == L'/' || sW[k+1] == 0)
      {
        //dot-slash or dot-end... remove
        cStrPathW.Delete(k, (sW[k+1] == L'/') ? 2 : 1);
        continue;
      }
      if (sW[k+1] == L'.')
      {
        //dot-dot...
        if (sW[k+2] == L'/' || sW[k+2] == 0)
        {
          //dot-dot-slash or dot-dot-end... go back to previous slash
          //NOTE: "nnn/nnn/(.)./" <= we are pointing to the dot between ()
          l = (sW[k+2] == L'/') ? 3 : 2; //chars to remove from 'k'
          if (k == nStartOfs || k == nStartOfs+1)
          {
            //case: "(.)./" or "/(.)./
            bValid = FALSE;
          }
          else
          {
            //case "nnn/(.)./"
            MX_ASSERT(sW[k-1] == L'/');
            k--;  l++;
            while (k > nStartOfs && sW[k-1] != L'/')
            {
              k--;
              l++;
            }
          }
          cStrPathW.Delete(k, l);
          continue;
        }
        k++;
      }
    }
    bPrevWasSlash = FALSE;
    k++;
  }
  return (bValid != FALSE) ? S_OK : MX_E_InvalidData;
}

static HRESULT ToStringEncode(__inout MX::CStringA &cStrDestA, __in_z LPCWSTR szStrW, __in SIZE_T nLen,
                              __in_z LPCSTR szAllowedCharsA)
{
  CHAR chA[4], _chA[3];
  int i, nSize;

  while (nLen > 0)
  {
    if (*szStrW < 127)
    {
      chA[0] = (CHAR)(UCHAR)(*szStrW);
      if (IsUnreservedChar(chA[0]) != FALSE ||
          (szAllowedCharsA != NULL && MX::StrChrA(szAllowedCharsA, chA[0]) != NULL))
      {
        if (cStrDestA.ConcatN(chA, 1) == FALSE)
          return E_OUTOFMEMORY;
        szStrW++;
        nLen--;
        continue;
      }
    }
    //encode this char
    if (nLen >= 2 && *szStrW >= 0xD800 && *szStrW <= 0xDBFF)
    {
      nSize = MX::Utf8_EncodeChar(chA, *szStrW, szStrW[1]);
      szStrW += 2;
      nLen -= 2;
    }
    else
    {
      nSize = MX::Utf8_EncodeChar(chA, *szStrW);
      szStrW++;
      nLen--;
    }
    if (nSize <= 0)
      return MX_E_InvalidData;
    for (i=0; i<nSize; i++)
    {
      _chA[0] = '%';
      _chA[1] = szHexaNumA[((*((UCHAR*)chA[i])) >> 4) & 0x0F];
      _chA[2] = szHexaNumA[( *((UCHAR*)chA[i])      ) & 0x0F];
      if (cStrDestA.ConcatN(_chA, 3) == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  //done
  return S_OK;
}

static HRESULT ToStringEncode(__inout MX::CStringW &cStrDestW, __in_z LPCWSTR szStrW, __in SIZE_T nLen,
                              __in_z LPCWSTR szAllowedCharsW)
{
  WCHAR chW[3];
  CHAR chA[4];
  int i, nSize;

  while (nLen > 0)
  {
    if (IsUnreservedChar(*szStrW) != FALSE ||
        (szAllowedCharsW != NULL && MX::StrChrW(szAllowedCharsW, *szStrW) != NULL))
    {
      if (cStrDestW.ConcatN(szStrW, 1) == FALSE)
        return E_OUTOFMEMORY;
      szStrW++;
      nLen--;
      continue;
    }
    //encode this char
    if (nLen >= 2 && *szStrW >= 0xD800 && *szStrW <= 0xDBFF)
    {
      nSize = MX::Utf8_EncodeChar(chA, *szStrW, szStrW[1]);
      szStrW += 2;
      nLen -= 2;
    }
    else
    {
      nSize = MX::Utf8_EncodeChar(chA, *szStrW);
      szStrW++;
      nLen--;
    }
    if (nSize <= 0)
      return MX_E_InvalidData;
    for (i=0; i<nSize; i++)
    {
      chW[0] = L'%';
      chW[1] = szHexaNumW[((*((UCHAR*)chA[i])) >> 4) & 0x0F];
      chW[2] = szHexaNumW[( *((UCHAR*)chA[i])      ) & 0x0F];
      if (cStrDestW.ConcatN(chW, 3) == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  //done
  return S_OK;
}

static SIZE_T FindChar(__in_z LPCSTR szStrA, __in SIZE_T nSrcLen, __in_z LPCSTR szToFindA,
                       __in_z_opt LPCSTR szStopCharA)
{
  SIZE_T nCounter;

  MX_ASSERT(szToFindA != NULL);
  nCounter = 0;
  while (nSrcLen > 0 && *szStrA != 0)
  {
    if (MX::StrChrA(szToFindA, *szStrA) != NULL)
      return nCounter;
    if (szStopCharA != NULL && MX::StrChrA(szStopCharA, *szStrA) != NULL)
      return (SIZE_T)-1;
    szStrA++;
    nSrcLen--;
    nCounter++;
  }
  return (SIZE_T)-1;
}

static SIZE_T FindChar(__in_z LPCWSTR szStrW, __in SIZE_T nSrcLen, __in_z LPCWSTR szToFindW,
                       __in_z_opt LPCWSTR szStopCharW)
{
  SIZE_T nCounter;

  MX_ASSERT(szToFindW != NULL);
  nCounter = 0;
  while (nSrcLen > 0 && *szStrW != 0)
  {
    if (MX::StrChrW(szToFindW, *szStrW) != NULL)
      return nCounter;
    if (szStopCharW != NULL && MX::StrChrW(szStopCharW, *szStrW) != NULL)
      return (SIZE_T)-1;
    szStrW++;
    nSrcLen--;
    nCounter++;
  }
  return (SIZE_T)-1;
}

static BOOL IsLocalHost(__in_z LPCSTR szHostA)
{
  SOCKADDR_INET sAddr;

  if (MX::StrCompareA(szHostA, "localhost") == 0)
    return TRUE;
  if (MX::CHostResolver::IsValidIPV4(szHostA, (SIZE_T)-1, &sAddr) != FALSE)
  {
    if (sAddr.Ipv4.sin_addr.S_un.S_un_b.s_b1 == 127 && sAddr.Ipv4.sin_addr.S_un.S_un_b.s_b2 == 0 &&
        sAddr.Ipv4.sin_addr.S_un.S_un_b.s_b3 == 0 && sAddr.Ipv4.sin_addr.S_un.S_un_b.s_b4 == 1)
      return TRUE;
  }
  if (MX::CHostResolver::IsValidIPV6(szHostA, (SIZE_T)-1, &sAddr) != FALSE)
  {
    if (sAddr.Ipv6.sin6_addr.u.Word[0] == 0 && sAddr.Ipv6.sin6_addr.u.Word[1] == 0 &&
        sAddr.Ipv6.sin6_addr.u.Word[2] == 0 && sAddr.Ipv6.sin6_addr.u.Word[3] == 0 &&
        sAddr.Ipv6.sin6_addr.u.Word[4] == 0)
    {
      if (sAddr.Ipv6.sin6_addr.u.Word[5] == 0 && sAddr.Ipv6.sin6_addr.u.Word[6] == 0 &&
          sAddr.Ipv6.sin6_addr.u.Word[7] == 1)
        return TRUE;
      if ((sAddr.Ipv6.sin6_addr.u.Word[5] == 0 || sAddr.Ipv6.sin6_addr.u.Word[5] == 0xFFFF) &&
          sAddr.Ipv6.sin6_addr.u.Word[6] == 0x7F00 && sAddr.Ipv6.sin6_addr.u.Word[7] == 0x0001)
        return TRUE;
    }
  }
  return FALSE;
}

static BOOL IsLocalHost(__in_z LPCWSTR szHostW)
{
  SOCKADDR_INET sAddr;

  if (MX::StrCompareW(szHostW, L"localhost") == 0)
    return TRUE;
  if (MX::CHostResolver::IsValidIPV4(szHostW, (SIZE_T)-1, &sAddr) != FALSE)
  {
    if (sAddr.Ipv4.sin_addr.S_un.S_un_b.s_b1 == 127 && sAddr.Ipv4.sin_addr.S_un.S_un_b.s_b2 == 0 &&
        sAddr.Ipv4.sin_addr.S_un.S_un_b.s_b3 == 0 && sAddr.Ipv4.sin_addr.S_un.S_un_b.s_b4 == 1)
      return TRUE;
  }
  if (MX::CHostResolver::IsValidIPV6(szHostW, (SIZE_T)-1, &sAddr) != FALSE)
  {
    if (sAddr.Ipv6.sin6_addr.u.Word[0] == 0 && sAddr.Ipv6.sin6_addr.u.Word[1] == 0 &&
        sAddr.Ipv6.sin6_addr.u.Word[2] == 0 && sAddr.Ipv6.sin6_addr.u.Word[3] == 0 &&
        sAddr.Ipv6.sin6_addr.u.Word[4] == 0)
    {
      if (sAddr.Ipv6.sin6_addr.u.Word[5] == 0 && sAddr.Ipv6.sin6_addr.u.Word[6] == 0 &&
          sAddr.Ipv6.sin6_addr.u.Word[7] == 1)
        return TRUE;
      if ((sAddr.Ipv6.sin6_addr.u.Word[5] == 0 || sAddr.Ipv6.sin6_addr.u.Word[5] == 0xFFFF) &&
          sAddr.Ipv6.sin6_addr.u.Word[6] == 0x7F00 && sAddr.Ipv6.sin6_addr.u.Word[7] == 0x0001)
        return TRUE;
    }
  }
  return FALSE;
}

static BOOL HasDisallowedEscapedSequences(__in_z LPCSTR szSrcA, __in SIZE_T nSrcLen)
{
  return (MX::StrNFindA(szSrcA, "%00", nSrcLen) != NULL ||
          MX::StrNFindA(szSrcA, "%2F", nSrcLen) != NULL ||
          MX::StrNFindA(szSrcA, "%5C", nSrcLen) != NULL ||
          MX::StrNFindA(szSrcA, "%u0000", nSrcLen) != NULL ||
          MX::StrNFindA(szSrcA, "%u002F", nSrcLen) != NULL ||
          MX::StrNFindA(szSrcA, "%u005C", nSrcLen) != NULL) ? TRUE : FALSE;
}

static BOOL HasDisallowedEscapedSequences(__in_z LPCWSTR szSrcW, __in SIZE_T nSrcLen)
{
  return (MX::StrNFindW(szSrcW, L"%00", nSrcLen) != NULL ||
          MX::StrNFindW(szSrcW, L"%2F", nSrcLen) != NULL ||
          MX::StrNFindW(szSrcW, L"%5C", nSrcLen) != NULL ||
          MX::StrNFindW(szSrcW, L"%u0000", nSrcLen) != NULL ||
          MX::StrNFindW(szSrcW, L"%u002F", nSrcLen) != NULL ||
          MX::StrNFindW(szSrcW, L"%u005C", nSrcLen) != NULL) ? TRUE : FALSE;
}
