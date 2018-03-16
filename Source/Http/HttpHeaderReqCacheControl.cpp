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
#include "..\..\Include\Http\HttpHeaderReqCacheControl.h"
#include <stdlib.h>

//-----------------------------------------------------------

typedef enum {
  ValueTypeNone, ValueTypeToken, ValueTypeQuotedString
} eValueType;

//-----------------------------------------------------------

static HRESULT GetUInt64(_In_z_ LPCSTR szValueA, _Out_ ULONGLONG &nValue);

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqCacheControl::CHttpHeaderReqCacheControl() : CHttpHeaderBase()
{
  bNoCache = FALSE;
  bNoStore = FALSE;
  nMaxAge = ULONGLONG_MAX;
  nMaxStale = ULONGLONG_MAX;
  nMinFresh = ULONGLONG_MAX;
  bNoTransform = FALSE;
  bOnlyIfCached = FALSE;
  return;
}

CHttpHeaderReqCacheControl::~CHttpHeaderReqCacheControl()
{
  cExtensionsList.RemoveAllElements();
  return;
}

HRESULT CHttpHeaderReqCacheControl::Parse(_In_z_ LPCSTR szValueA)
{
  LPCSTR szNameStartA, szNameEndA, szStartA;
  CStringA cStrTempA;
  CStringW cStrTempW;
  ULONGLONG nValue;
  ULONG nGotItems;
  eValueType nValueType;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //parse
  nGotItems = 0;
  do
  {
    //skip spaces
    szValueA = SkipSpaces(szValueA);
    if (*szValueA == 0)
      break;
    //token
    szNameEndA = szValueA = Advance(szNameStartA = szValueA, "=, \t");
    if (szNameStartA == szNameEndA)
      return MX_E_InvalidData;
    //skip spaces
    szValueA = SkipSpaces(szValueA);
    //value
    nValueType = ValueTypeNone;
    cStrTempA.Empty();
    if (*szValueA == '=')
    {
      //skip spaces
      szValueA = SkipSpaces(szValueA+1);
      //parse value
      if (*szValueA == '"')
      {
        nValueType = ValueTypeQuotedString;
        szValueA = Advance(szStartA = ++szValueA, "\"");
        if (*szValueA != '"')
          return MX_E_InvalidData;
        if (cStrTempA.CopyN(szStartA, (SIZE_T)(szValueA-szStartA)) == FALSE)
          return E_OUTOFMEMORY;
        szValueA++;
      }
      else
      {
        nValueType = ValueTypeToken;
        szValueA = Advance(szStartA = szValueA, " \t,");
        if (cStrTempA.CopyN(szStartA, (SIZE_T)(szValueA-szStartA)) == FALSE)
          return E_OUTOFMEMORY;
      }
    }
    //set item value
    hRes = S_FALSE;
    switch ((SIZE_T)(szNameEndA-szNameStartA))
    {
      case 7:
        if (StrNCompareA(szNameStartA, "max-age", 7, TRUE) == 0)
        {
          if (nValueType != ValueTypeToken || (nGotItems & 0x0001) != 0)
            return MX_E_InvalidData; //no value provided or token already specified
          nGotItems |= 0x0001;
          //parse delta seconds
          hRes = GetUInt64((LPSTR)cStrTempA, nValue);
          if (SUCCEEDED(hRes))
            hRes = SetMaxAge(nValue);
          break;
        }
        break;

      case 8:
        if (StrNCompareA(szNameStartA, "no-cache", 8, TRUE) == 0)
        {
          if (nValueType != ValueTypeNone || (nGotItems & 0x0002) != 0)
            return MX_E_InvalidData; //value provided or token already specified
          nGotItems |= 0x0002;
          //set value
          hRes = SetNoCache(TRUE);
          break;
        }
        if (StrNCompareA(szNameStartA, "no-store", 8, TRUE) == 0)
        {
          if (nValueType != ValueTypeNone || (nGotItems & 0x0004) != 0)
            return MX_E_InvalidData; //value provided or token already specified
          nGotItems |= 0x0004;
          //set value
          hRes = SetNoStore(TRUE);
          break;
        }
        break;

      case 9:
        if (StrNCompareA(szNameStartA, "max-stale", 9, TRUE) == 0)
        {
          if (nValueType != ValueTypeToken || (nGotItems & 0x0008) != 0)
            return MX_E_InvalidData; //no value provided or token already specified
          nGotItems |= 0x0008;
          //parse delta seconds
          hRes = GetUInt64((LPSTR)cStrTempA, nValue);
          if (SUCCEEDED(hRes))
            hRes = SetMaxStale(nValue);
          break;
        }
        if (StrNCompareA(szNameStartA, "min-fresh", 9, TRUE) == 0)
        {
          if (nValueType != ValueTypeToken || (nGotItems & 0x0010) != 0)
            return MX_E_InvalidData; //no value provided or token already specified
          nGotItems |= 0x0010;
          //parse delta seconds
          hRes = GetUInt64((LPSTR)cStrTempA, nValue);
          if (SUCCEEDED(hRes))
            hRes = SetMinFresh(nValue);
          break;
        }
        break;

      case 12:
        if (StrNCompareA(szNameStartA, "no-transform", 12, TRUE) == 0)
        {
          if (nValueType != ValueTypeNone || (nGotItems & 0x0020) != 0)
            return MX_E_InvalidData; //value provided or token already specified
          nGotItems |= 0x0020;
          //set value
          hRes = SetNoTransform(TRUE);
          break;
        }
        break;

      case 14:
        if (StrNCompareA(szNameStartA, "only-if-cached", 14, TRUE) == 0)
        {
          if (nValueType != ValueTypeNone || (nGotItems & 0x0040) != 0)
            return MX_E_InvalidData; //value provided or token already specified
          nGotItems |= 0x0040;
          //set value
          hRes = SetOnlyIfCached(TRUE);
          break;
        }
        break;
    }
    if (hRes == S_FALSE)
    {
      nGotItems |= 0x1000;
      //parse cache extension
      cStrTempW.Empty();
      switch (nValueType)
      {
        case ValueTypeToken:
          if (cStrTempW.Copy((LPSTR)cStrTempA) == FALSE)
            return E_OUTOFMEMORY;
          break;

        case ValueTypeQuotedString:
          hRes = Utf8_Decode(cStrTempW, (LPSTR)cStrTempA);
          if (FAILED(hRes))
            return hRes;
          break;
      }
      if (cStrTempA.CopyN(szNameStartA, (SIZE_T)(szNameEndA-szNameStartA)) == FALSE)
        return E_OUTOFMEMORY;
      hRes = AddExtension((LPSTR)cStrTempA, (LPWSTR)cStrTempW);
    }
    if (FAILED(hRes))
      return hRes;
    //skip spaces
    szValueA = SkipSpaces(szValueA);
    //check for separator or end
    if (*szValueA == ',')
      szValueA++;
    else if (*szValueA != 0)
      return MX_E_InvalidData;
  }
  while (*szValueA != 0);
  //done
  return (nGotItems != 0) ? S_OK : MX_E_InvalidData;
}

HRESULT CHttpHeaderReqCacheControl::Build(_Inout_ CStringA &cStrDestA)
{
  SIZE_T i, nCount;
  LPCWSTR sW;
  CStringA cStrTempA;
  HRESULT hRes;

  cStrDestA.Empty();
  if (bNoCache != FALSE)
  {
    if (cStrDestA.Concat(",no-cache") == FALSE)
      return E_OUTOFMEMORY;
  }
  if (bNoStore != FALSE)
  {
    if (cStrDestA.Concat(",no-store") == FALSE)
      return E_OUTOFMEMORY;
  }
  if (nMaxAge != ULONGLONG_MAX)
  {
    if (cStrDestA.AppendFormat(",max-age=%I64u", nMaxAge) == FALSE)
      return E_OUTOFMEMORY;
  }
  if (nMaxStale != ULONGLONG_MAX)
  {
    if (cStrDestA.AppendFormat(",max-stale=%I64u", nMaxStale) == FALSE)
      return E_OUTOFMEMORY;
  }
  if (nMinFresh != ULONGLONG_MAX)
  {
    if (cStrDestA.AppendFormat(",min-fresh=%I64u", nMinFresh) == FALSE)
      return E_OUTOFMEMORY;
  }
  if (bNoTransform != FALSE)
  {
    if (cStrDestA.Concat(",no-transform") == FALSE)
      return E_OUTOFMEMORY;
  }
  if (bOnlyIfCached != FALSE)
  {
    if (cStrDestA.Concat(",only-if-cached") == FALSE)
      return E_OUTOFMEMORY;
  }
  //extensions
  nCount = cExtensionsList.GetCount();
  for (i=0; i<nCount; i++)
  {
    if (cStrDestA.AppendFormat(",%s=", cExtensionsList[i]->szNameA) == FALSE)
      return E_OUTOFMEMORY;
    sW = cExtensionsList[i]->szValueW;
    while (*sW != 0)
    {
      if (*sW < 33 || *sW > 126 || CHttpCommon::IsValidNameChar((CHAR)(*sW)) == FALSE)
        break;
      sW++;
    }
    if (*sW == 0)
    {
      if (cStrDestA.AppendFormat("%S", cExtensionsList[i]->szValueW) == FALSE)
        return E_OUTOFMEMORY;
    }
    else
    {
      hRes = Utf8_Encode(cStrTempA, cExtensionsList[i]->szValueW);
      if (FAILED(hRes))
        return hRes;
      if (CHttpCommon::EncodeQuotedString(cStrTempA) == FALSE)
        return E_OUTOFMEMORY;
      if (cStrDestA.AppendFormat("\"%s\"", (LPCSTR)cStrTempA) == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  if (cStrTempA.GetLength() > 0)
    cStrTempA.Delete(0, 1); //delete initial comma
  //done
  return S_OK;
}

HRESULT CHttpHeaderReqCacheControl::SetNoCache(_In_ BOOL _bNoCache)
{
  bNoCache = _bNoCache;
  return S_OK;
}

BOOL CHttpHeaderReqCacheControl::GetNoCache() const
{
  return bNoCache;
}

HRESULT CHttpHeaderReqCacheControl::SetNoStore(_In_ BOOL _bNoStore)
{
  bNoStore = _bNoStore;
  return S_OK;
}

BOOL CHttpHeaderReqCacheControl::GetNoStore() const
{
  return bNoStore;
}

HRESULT CHttpHeaderReqCacheControl::SetMaxAge(_In_ ULONGLONG _nMaxAge)
{
  nMaxAge = _nMaxAge;
  return S_OK;
}

ULONGLONG CHttpHeaderReqCacheControl::GetMaxAge() const
{
  return nMaxAge;
}

HRESULT CHttpHeaderReqCacheControl::SetMaxStale(_In_ ULONGLONG _nMaxStale)
{
  nMaxStale = _nMaxStale;
  return S_OK;
}

ULONGLONG CHttpHeaderReqCacheControl::GetMaxStale() const
{
  return nMaxStale;
}

HRESULT CHttpHeaderReqCacheControl::SetMinFresh(_In_ ULONGLONG _nMinFresh)
{
  nMinFresh = _nMinFresh;
  return S_OK;
}

ULONGLONG CHttpHeaderReqCacheControl::GetMinFresh() const
{
  return nMinFresh;
}

HRESULT CHttpHeaderReqCacheControl::SetNoTransform(_In_ BOOL _bNoTransform)
{
  bNoTransform = _bNoTransform;
  return S_OK;
}

BOOL CHttpHeaderReqCacheControl::GetNoTransform() const
{
  return bNoTransform;
}

HRESULT CHttpHeaderReqCacheControl::SetOnlyIfCached(_In_ BOOL _bOnlyIfCached)
{
  bOnlyIfCached = _bOnlyIfCached;
  return S_OK;
}

BOOL CHttpHeaderReqCacheControl::GetOnlyIfCached() const
{
  return bOnlyIfCached;
}

HRESULT CHttpHeaderReqCacheControl::AddExtension(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW)
{
  TAutoFreePtr<EXTENSION> cNewExtension;
  LPCSTR szStartA, szEndA;
  SIZE_T nNameLen, nValueLen;

  if (szNameA == NULL)
    return E_POINTER;
  if (szValueW == NULL)
    szValueW = L"";
  //skip spaces
  szNameA = CHttpHeaderBase::SkipSpaces(szNameA);
  //get token
  szNameA = CHttpHeaderBase::GetToken(szStartA = szNameA);
  if (szStartA == szNameA)
    return MX_E_InvalidData;
  szEndA = szNameA;
  //skip spaces and check for end
  if (*CHttpHeaderBase::SkipSpaces(szNameA) != 0)
    return MX_E_InvalidData;
  //get name and value length
  nNameLen = (SIZE_T)(szEndA-szStartA);
  nValueLen = (StrLenW(szValueW) + 1) * sizeof(WCHAR);
  //create new item
  cNewExtension.Attach((LPEXTENSION)MX_MALLOC(sizeof(EXTENSION) + nNameLen + nValueLen));
  if (!cNewExtension)
    return E_OUTOFMEMORY;
  MemCopy(cNewExtension->szNameA, szStartA, nNameLen);
  cNewExtension->szNameA[nNameLen] = 0;
  cNewExtension->szValueW = (LPWSTR)((LPBYTE)cNewExtension.Get() + nNameLen);
  MemCopy(cNewExtension->szValueW, szValueW, nValueLen);
  //add to list
  if (cExtensionsList.AddElement(cNewExtension.Get()) == FALSE)
    return E_OUTOFMEMORY;
  //done
  cNewExtension.Detach();
  return S_OK;
}

SIZE_T CHttpHeaderReqCacheControl::GetExtensionsCount() const
{
  return cExtensionsList.GetCount();
}

LPCSTR CHttpHeaderReqCacheControl::GetExtensionName(_In_ SIZE_T nIndex) const
{
  return (nIndex < cExtensionsList.GetCount()) ? cExtensionsList[nIndex]->szNameA : NULL;
}

LPCWSTR CHttpHeaderReqCacheControl::GetExtensionValue(_In_ SIZE_T nIndex) const
{
  return (nIndex < cExtensionsList.GetCount()) ? cExtensionsList[nIndex]->szValueW : NULL;
}

LPCWSTR CHttpHeaderReqCacheControl::GetExtensionValue(_In_z_ LPCSTR szNameA) const
{
  SIZE_T i, nCount;

  if (szNameA != NULL && szNameA[0] != 0)
  {
    nCount = cExtensionsList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (StrCompareA(cExtensionsList[i]->szNameA, szNameA, TRUE) == 0)
        return cExtensionsList[i]->szValueW;
    }
  }
  return NULL;
}

} //namespace MX

//-----------------------------------------------------------

static HRESULT GetUInt64(_In_z_ LPCSTR szValueA, _Out_ ULONGLONG &nValue)
{
  ULONGLONG nTemp;

  nValue = 0ui64;
  while (*szValueA == ' ' || *szValueA == '\t')
    szValueA++;
  if (*szValueA < '0' || *szValueA > '9')
    return MX_E_InvalidData;
  while (*szValueA >= '0' && *szValueA <= '9')
  {
    nTemp = nValue * 10ui64;
    if (nTemp < nValue)
      return MX_E_ArithmeticOverflow;
    nValue = nTemp + (ULONGLONG)(*szValueA - '0');
    if (nValue < nTemp)
      return MX_E_ArithmeticOverflow;
    szValueA++;
  }
  return S_OK;
}
