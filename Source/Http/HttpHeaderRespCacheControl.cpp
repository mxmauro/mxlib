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
#include "..\..\Include\Http\HttpHeaderRespCacheControl.h"
#include <stdlib.h>

//-----------------------------------------------------------

typedef enum {
  ValueTypeNone, ValueTypeToken, ValueTypeQuotedString
} eValueType;

//-----------------------------------------------------------

static HRESULT GetUInt64(__in_z LPCSTR szValueA, __out ULONGLONG &nValue);

//-----------------------------------------------------------

namespace MX
{

CHttpHeaderRespCacheControl::CHttpHeaderRespCacheControl() : CHttpHeaderBase()
{
  bPublic = FALSE;
  bPrivate = FALSE;
  bNoCache = FALSE;
  bNoStore = FALSE;
  bNoTransform = FALSE;
  bMustRevalidate = FALSE;
  bProxyRevalidate = FALSE;
  nMaxAge = ULONGLONG_MAX;
  nSMaxAge = ULONGLONG_MAX;
  return;
}

CHttpHeaderRespCacheControl::~CHttpHeaderRespCacheControl()
{
  cPrivateFieldsList.RemoveAllElements();
  cNoCacheFieldsList.RemoveAllElements();
  cExtensionsList.RemoveAllElements();
  return;
}

HRESULT CHttpHeaderRespCacheControl::Parse(__in_z LPCSTR szValueA)
{
  LPCSTR szNameStartA, szNameEndA, szStartA, szFieldA;
  CStringA cStrTempA, cStrFieldA;
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
      case 6:
        if (StrNCompareA(szNameStartA, "public", 6, TRUE) == 0)
        {
          if (nValueType != ValueTypeNone || (nGotItems & 0x0001) != 0)
            return MX_E_InvalidData; //value provided or token already specified
          nGotItems |= 0x0001;
          //set value
          hRes = SetPublic(TRUE);
          break;
        }
        break;

      case 7:
        if (StrNCompareA(szNameStartA, "max-age", 7, TRUE) == 0)
        {
          if (nValueType != ValueTypeToken || (nGotItems & 0x0002) != 0)
            return MX_E_InvalidData; //no value provided or token already specified
          nGotItems |= 0x0002;
          //parse delta seconds
          hRes = GetUInt64((LPSTR)cStrTempA, nValue);
          if (SUCCEEDED(hRes))
            hRes = SetMaxAge(nValue);
          break;
        }
        if (StrNCompareA(szNameStartA, "private", 7, TRUE) == 0)
        {
          if ((nValueType != ValueTypeNone && nValueType != ValueTypeQuotedString) || (nGotItems & 0x0004) != 0)
            return MX_E_InvalidData; //non quoted value provided or token already specified
          nGotItems |= 0x0004;
          //set value
          hRes = SetPrivate(TRUE);
          if (SUCCEEDED(hRes) && nValueType == ValueTypeQuotedString)
          {
            //parse fields
            szFieldA = (LPSTR)cStrTempA;
            while (SUCCEEDED(hRes) && *szFieldA != 0)
            {
              //skip spaces
              szFieldA = SkipSpaces(szFieldA);
              if (*szFieldA == 0)
                break;
              szFieldA = GetToken(szStartA = szFieldA, ",");
              if (szFieldA == szStartA)
              {
                hRes = MX_E_InvalidData;
                break;
              }
              //add field
              if (cStrTempA.CopyN(szStartA, (SIZE_T)(szFieldA-szStartA)) == FALSE)
              {
                hRes = E_OUTOFMEMORY;
                break;
              }
              hRes = AddPrivateField((LPSTR)cStrTempA);
              if (FAILED(hRes))
                break;
              //skip spaces
              szFieldA = SkipSpaces(szFieldA);
              //check for separator or end
              if (*szFieldA == ',')
                szFieldA++;
              else if (*szFieldA != 0)
                hRes = MX_E_InvalidData;
            }
          }
          break;
        }
        break;

      case 8:
        if (StrNCompareA(szNameStartA, "s-maxage", 8, TRUE) == 0)
        {
          if (nValueType != ValueTypeToken || (nGotItems & 0x0008) != 0)
            return MX_E_InvalidData; //no value provided or token already specified
          nGotItems |= 0x0008;
          //parse delta seconds
          hRes = GetUInt64((LPSTR)cStrTempA, nValue);
          if (SUCCEEDED(hRes))
            hRes = SetSharedMaxAge(nValue);
          break;
        }
        if (StrNCompareA(szNameStartA, "no-cache", 8, TRUE) == 0)
        {
          if ((nValueType != ValueTypeNone && nValueType != ValueTypeQuotedString) || (nGotItems & 0x0010) != 0)
            return MX_E_InvalidData; //non quoted value provided or token already specified
          nGotItems |= 0x0010;
          //set value
          hRes = SetNoCache(TRUE);
          if (SUCCEEDED(hRes) && nValueType == ValueTypeQuotedString)
          {
            //parse fields
            szFieldA = (LPSTR)cStrTempA;
            while (SUCCEEDED(hRes) && *szFieldA != 0)
            {
              //skip spaces
              szFieldA = SkipSpaces(szFieldA);
              if (*szFieldA == 0)
                break;
              szFieldA = GetToken(szStartA = szFieldA, ",");
              if (szFieldA == szStartA)
              {
                hRes = MX_E_InvalidData;
                break;
              }
              //add field
              if (cStrTempA.CopyN(szStartA, (SIZE_T)(szFieldA-szStartA)) == FALSE)
              {
                hRes = E_OUTOFMEMORY;
                break;
              }
              hRes = AddNoCacheField((LPSTR)cStrTempA);
              if (FAILED(hRes))
                break;
              //skip spaces
              szFieldA = SkipSpaces(szFieldA);
              //check for separator or end
              if (*szFieldA == ',')
                szFieldA++;
              else if (*szFieldA != 0)
                hRes = MX_E_InvalidData;
            }
          }
          break;
        }
        if (StrNCompareA(szNameStartA, "no-store", 8, TRUE) == 0)
        {
          if (nValueType != ValueTypeNone || (nGotItems & 0x0020) != 0)
            return MX_E_InvalidData; //value provided or token already specified
          nGotItems |= 0x0020;
          //set value
          hRes = SetNoStore(TRUE);
          break;
        }
        break;

      case 12:
        if (StrNCompareA(szNameStartA, "no-transform", 12, TRUE) == 0)
        {
          if (nValueType != ValueTypeNone || (nGotItems & 0x0040) != 0)
            return MX_E_InvalidData; //value provided or token already specified
          nGotItems |= 0x0040;
          //set value
          hRes = SetNoTransform(TRUE);
          break;
        }
        break;

      case 15:
        if (StrNCompareA(szNameStartA, "must-revalidate", 15, TRUE) == 0)
        {
          if (nValueType != ValueTypeNone || (nGotItems & 0x0080) != 0)
            return MX_E_InvalidData; //value provided or token already specified
          nGotItems |= 0x0080;
          //set value
          hRes = SetMustRevalidate(TRUE);
          break;
        }
        break;

      case 16:
        if (StrNCompareA(szNameStartA, "proxy-revalidate", 16, TRUE) == 0)
        {
          if (nValueType != ValueTypeNone || (nGotItems & 0x0100) != 0)
            return MX_E_InvalidData; //value provided or token already specified
          nGotItems |= 0x0100;
          //set value
          hRes = SetProxyRevalidate(TRUE);
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

HRESULT CHttpHeaderRespCacheControl::Build(__inout CStringA &cStrDestA)
{
  SIZE_T i, nCount;
  LPCWSTR sW;
  CStringA cStrTempA;
  HRESULT hRes;

  cStrDestA.Empty();
  if (bPublic != FALSE)
  {
    if (cStrDestA.Concat(",public") == FALSE)
      return E_OUTOFMEMORY;
  }
  if (bPrivate != FALSE)
  {
    if (cStrDestA.Concat(",private") == FALSE)
      return E_OUTOFMEMORY;
    nCount = cPrivateFieldsList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (cStrDestA.AppendFormat("%c%s", ((i == 0) ? '"' : ','), cPrivateFieldsList.GetElementAt(i)) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (nCount > 0)
    {
      if (cStrDestA.ConcatN("\"", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  if (bNoCache != FALSE)
  {
    if (cStrDestA.Concat(",no-cache") == FALSE)
      return E_OUTOFMEMORY;
    nCount = cNoCacheFieldsList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (cStrDestA.AppendFormat("%c%s", ((i == 0) ? '"' : ','), cNoCacheFieldsList.GetElementAt(i)) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (nCount > 0)
    {
      if (cStrDestA.ConcatN("\"", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  if (bMustRevalidate != FALSE)
  {
    if (cStrDestA.Concat(",must-revalidate") == FALSE)
      return E_OUTOFMEMORY;
  }
  if (bProxyRevalidate != FALSE)
  {
    if (cStrDestA.Concat(",proxy-revalidate") == FALSE)
      return E_OUTOFMEMORY;
  }
  if (bNoStore != FALSE)
  {
    if (cStrDestA.Concat(",no-store") == FALSE)
      return E_OUTOFMEMORY;
  }
  if (bNoTransform != FALSE)
  {
    if (cStrDestA.Concat(",no-transform") == FALSE)
      return E_OUTOFMEMORY;
  }
  if (nMaxAge != ULONGLONG_MAX)
  {
    if (cStrDestA.AppendFormat(",max-age=%I64u", nMaxAge) == FALSE)
      return E_OUTOFMEMORY;
  }
  if (nSMaxAge != ULONGLONG_MAX)
  {
    if (cStrDestA.AppendFormat(",s-maxage=%I64u", nSMaxAge) == FALSE)
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

HRESULT CHttpHeaderRespCacheControl::SetPublic(__in BOOL _bPublic)
{
  bPublic = _bPublic;
  return S_OK;
}

BOOL CHttpHeaderRespCacheControl::GetPublic() const
{
  return bPublic;
}

HRESULT CHttpHeaderRespCacheControl::SetPrivate(__in BOOL _bPrivate)
{
  bPrivate = _bPrivate;
  return S_OK;
}

BOOL CHttpHeaderRespCacheControl::GetPrivate() const
{
  return bPrivate;
}

HRESULT CHttpHeaderRespCacheControl::AddPrivateField(__in_z LPCSTR szFieldA)
{
  LPCSTR szStartA, szEndA;
  CStringA cStrTempA;

  if (szFieldA == NULL)
    return E_POINTER;
  //skip spaces
  szFieldA = CHttpHeaderBase::SkipSpaces(szFieldA);
  //get token
  szFieldA = CHttpHeaderBase::GetToken(szStartA = szFieldA);
  if (szStartA == szFieldA)
    return MX_E_InvalidData;
  szEndA = szFieldA;
  //skip spaces and check for end
  if (*CHttpHeaderBase::SkipSpaces(szFieldA) != 0)
    return MX_E_InvalidData;
  //set new value
  if (cStrTempA.CopyN(szStartA, (SIZE_T)(szEndA-szStartA)) == FALSE)
    return E_OUTOFMEMORY;
  if (cPrivateFieldsList.AddElement((LPSTR)cStrTempA) == FALSE)
    return E_OUTOFMEMORY;
  //done
  cStrTempA.Detach();
  return S_OK;
}

SIZE_T CHttpHeaderRespCacheControl::GetPrivateFieldsCount() const
{
  return cPrivateFieldsList.GetCount();
}

LPCSTR CHttpHeaderRespCacheControl::GetPrivateField(__in SIZE_T nIndex) const
{
  return (nIndex < cPrivateFieldsList.GetCount()) ? cPrivateFieldsList.GetElementAt(nIndex) : NULL;
}

BOOL CHttpHeaderRespCacheControl::HasPrivateField(__in_z LPCSTR szFieldA) const
{
  SIZE_T i, nCount;

  if (szFieldA != NULL && szFieldA[0] != 0)
  {
    nCount = cPrivateFieldsList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (StrCompareA(cPrivateFieldsList.GetElementAt(i), szFieldA, TRUE) == 0)
        return TRUE;
    }
  }
  return FALSE;
}

HRESULT CHttpHeaderRespCacheControl::SetNoCache(__in BOOL _bNoCache)
{
  bNoCache = _bNoCache;
  return S_OK;
}

BOOL CHttpHeaderRespCacheControl::GetNoCache() const
{
  return bNoCache;
}

HRESULT CHttpHeaderRespCacheControl::AddNoCacheField(__in_z LPCSTR szFieldA)
{
  LPCSTR szStartA, szEndA;
  CStringA cStrTempA;

  if (szFieldA == NULL)
    return E_POINTER;
  //skip spaces
  szFieldA = CHttpHeaderBase::SkipSpaces(szFieldA);
  //get token
  szFieldA = CHttpHeaderBase::GetToken(szStartA = szFieldA);
  if (szStartA == szFieldA)
    return MX_E_InvalidData;
  szEndA = szFieldA;
  //skip spaces and check for end
  if (*CHttpHeaderBase::SkipSpaces(szFieldA) != 0)
    return MX_E_InvalidData;
  //set new value
  if (cStrTempA.CopyN(szStartA, (SIZE_T)(szEndA-szStartA)) == FALSE)
    return E_OUTOFMEMORY;
  if (cNoCacheFieldsList.AddElement((LPSTR)cStrTempA) == FALSE)
    return E_OUTOFMEMORY;
  //done
  cStrTempA.Detach();
  return S_OK;
}

SIZE_T CHttpHeaderRespCacheControl::GetNoCacheFieldsCount() const
{
  return cNoCacheFieldsList.GetCount();
}

LPCSTR CHttpHeaderRespCacheControl::GetNoCacheField(__in SIZE_T nIndex) const
{
  return (nIndex < cNoCacheFieldsList.GetCount()) ? cNoCacheFieldsList.GetElementAt(nIndex) : NULL;
}

BOOL CHttpHeaderRespCacheControl::HasNoCacheField(__in_z LPCSTR szFieldA) const
{
  SIZE_T i, nCount;

  if (szFieldA != NULL && szFieldA[0] != 0)
  {
    nCount = cNoCacheFieldsList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (StrCompareA(cNoCacheFieldsList.GetElementAt(i), szFieldA, TRUE) == 0)
        return TRUE;
    }
  }
  return FALSE;
}

HRESULT CHttpHeaderRespCacheControl::SetNoStore(__in BOOL _bNoStore)
{
  bNoStore = _bNoStore;
  return S_OK;
}

BOOL CHttpHeaderRespCacheControl::GetNoStore() const
{
  return bNoStore;
}

HRESULT CHttpHeaderRespCacheControl::SetNoTransform(__in BOOL _bNoTransform)
{
  bNoTransform = _bNoTransform;
  return S_OK;
}

BOOL CHttpHeaderRespCacheControl::GetNoTransform() const
{
  return bNoTransform;
}

HRESULT CHttpHeaderRespCacheControl::SetMustRevalidate(__in BOOL _bMustRevalidate)
{
  bMustRevalidate = _bMustRevalidate;
  return S_OK;
}

BOOL CHttpHeaderRespCacheControl::GetMustRevalidate() const
{
  return bMustRevalidate;
}

HRESULT CHttpHeaderRespCacheControl::SetProxyRevalidate(__in BOOL _bProxyRevalidate)
{
  bProxyRevalidate = _bProxyRevalidate;
  return S_OK;
}

BOOL CHttpHeaderRespCacheControl::GetProxyRevalidate() const
{
  return bProxyRevalidate;
}

HRESULT CHttpHeaderRespCacheControl::SetMaxAge(__in ULONGLONG _nMaxAge)
{
  nMaxAge = _nMaxAge;
  return S_OK;
}

ULONGLONG CHttpHeaderRespCacheControl::GetMaxAge() const
{
  return nMaxAge;
}

HRESULT CHttpHeaderRespCacheControl::SetSharedMaxAge(__in ULONGLONG _nSharedMaxAge)
{
  nSMaxAge = _nSharedMaxAge;
  return S_OK;
}

ULONGLONG CHttpHeaderRespCacheControl::GetSharedMaxAge() const
{
  return nSMaxAge;
}

HRESULT CHttpHeaderRespCacheControl::AddExtension(__in_z LPCSTR szNameA, __in_z LPCWSTR szValueW)
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

SIZE_T CHttpHeaderRespCacheControl::GetExtensionsCount() const
{
  return cExtensionsList.GetCount();
}

LPCSTR CHttpHeaderRespCacheControl::GetExtensionName(__in SIZE_T nIndex) const
{
  return (nIndex < cExtensionsList.GetCount()) ? cExtensionsList[nIndex]->szNameA : NULL;
}

LPCWSTR CHttpHeaderRespCacheControl::GetExtensionValue(__in SIZE_T nIndex) const
{
  return (nIndex < cExtensionsList.GetCount()) ? cExtensionsList[nIndex]->szValueW : NULL;
}

LPCWSTR CHttpHeaderRespCacheControl::GetExtensionValue(__in_z LPCSTR szNameA) const
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

static HRESULT GetUInt64(__in_z LPCSTR szValueA, __out ULONGLONG &nValue)
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
