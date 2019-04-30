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

static HRESULT GetUInt64(_In_z_ LPCWSTR szValueW, _Out_ ULONGLONG &nValue);

//-----------------------------------------------------------

namespace MX {

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
  return;
}

HRESULT CHttpHeaderRespCacheControl::Parse(_In_z_ LPCSTR szValueA)
{
  CStringA cStrTokenA;
  CStringW cStrValueW;
  ULONGLONG nValue;
  ULONG nHasItem;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //parse
  nHasItem = 0;
  do
  {
    //skip spaces
    szValueA = SkipSpaces(szValueA);
    if (*szValueA == 0)
      break;
    if (*szValueA == ',')
      goto skip_null_listitem;

    //get parameter
    hRes = GetParamNameAndValue(cStrTokenA, cStrValueW, szValueA);
    if (FAILED(hRes) && hRes != MX_E_NoData)
      return hRes;

    //add parameter
    if (StrCompareA((LPCSTR)cStrTokenA, "public", TRUE) == 0)
    {
      if (cStrValueW.IsEmpty() == FALSE || (nHasItem & 0x0001) != 0)
        return MX_E_InvalidData;
      nHasItem |= 0x0001;
      //set value
      hRes = SetPublic(TRUE);
    }
    else if (StrCompareA((LPCSTR)cStrTokenA, "max-age", TRUE) == 0)
    {
      if ((nHasItem & 0x0002) != 0)
        return MX_E_InvalidData;
      nHasItem |= 0x0002;
      //parse delta seconds
      hRes = GetUInt64((LPCWSTR)cStrValueW, nValue);
      if (SUCCEEDED(hRes))
        hRes = SetMaxAge(nValue);
    }
    else if (StrCompareA((LPCSTR)cStrTokenA, "private", TRUE) == 0)
    {
      if ((nHasItem & 0x0004) != 0)
        return MX_E_InvalidData;
      nHasItem |= 0x0004;
      //set value
      hRes = SetPrivate(TRUE);
      //parse fields
      if (SUCCEEDED(hRes) && cStrValueW.IsEmpty() == FALSE)
      {
        LPCWSTR szStartW, szFieldW = (LPCWSTR)cStrValueW;
        CStringA cStrTempA;

        while (SUCCEEDED(hRes) && *szFieldW != 0)
        {
          //skip spaces
          while (*szFieldW == L' ' || *szFieldW == L'\t')
            szFieldW++;
          if (*szFieldW == 0)
            break;

          //add field
          szStartW = szFieldW;
          while (*szFieldW >= 0x21 && *szFieldW <= 0x7E && *szFieldW != L',')
            szFieldW++;
          if (szFieldW > szStartW)
          {
            if (cStrTempA.CopyN(szStartW, (SIZE_T)(szFieldW - szStartW)) == FALSE)
              hRes = AddPrivateField((LPCSTR)cStrTempA);
            else
              hRes = E_OUTOFMEMORY;
            if (FAILED(hRes))
              break;
          }

          //skip spaces
          while (*szFieldW == L' ' || *szFieldW == L'\t')
            szFieldW++;
          //check for separator or end
          if (*szFieldW == ',')
            szFieldW++;
          else if (*szFieldW != 0)
            return MX_E_InvalidData;
        }
      }
    }
    else if (StrCompareA((LPCSTR)cStrTokenA, "s-maxage", TRUE) == 0)
    {
      if ((nHasItem & 0x0008) != 0)
        return MX_E_InvalidData;
      nHasItem |= 0x0008;
      //parse delta seconds
      hRes = GetUInt64((LPCWSTR)cStrValueW, nValue);
      if (SUCCEEDED(hRes))
        hRes = SetSharedMaxAge(nValue);
    }
    else if (StrCompareA((LPCSTR)cStrTokenA, "no-cache", TRUE) == 0)
    {
      if ((nHasItem & 0x0010) != 0)
        return MX_E_InvalidData;
      nHasItem |= 0x0010;
      //set value
      hRes = SetNoCache(TRUE);
      //parse fields
      if (SUCCEEDED(hRes) && cStrValueW.IsEmpty() == FALSE)
      {
        LPCWSTR szStartW, szFieldW = (LPCWSTR)cStrValueW;
        CStringA cStrTempA;

        while (SUCCEEDED(hRes) && *szFieldW != 0)
        {
          //skip spaces
          while (*szFieldW == L' ' || *szFieldW == L'\t')
            szFieldW++;
          if (*szFieldW == 0)
            break;

          //add field
          szStartW = szFieldW;
          while (*szFieldW >= 0x21 && *szFieldW <= 0x7E && *szFieldW != L',')
            szFieldW++;
          if (szFieldW > szStartW)
          {
            if (cStrTempA.CopyN(szStartW, (SIZE_T)(szFieldW - szStartW)) == FALSE)
              hRes = AddNoCacheField((LPCSTR)cStrTempA);
            else
              hRes = E_OUTOFMEMORY;
            if (FAILED(hRes))
              break;
          }

          //skip spaces
          while (*szFieldW == L' ' || *szFieldW == L'\t')
            szFieldW++;
          //check for separator or end
          if (*szFieldW == ',')
            szFieldW++;
          else if (*szFieldW != 0)
            return MX_E_InvalidData;
        }
      }
    }
    else if (StrCompareA((LPCSTR)cStrTokenA, "no-store", TRUE) == 0)
    {
      if ((nHasItem & 0x0020) != 0)
        return MX_E_InvalidData;
      nHasItem |= 0x0020;
      //set value
      hRes = SetNoStore(TRUE);
    }
    else if (StrCompareA((LPCSTR)cStrTokenA, "no-transform", TRUE) == 0)
    {
      if ((nHasItem & 0x0040) != 0)
        return MX_E_InvalidData;
      nHasItem |= 0x0040;
      //set value
      hRes = SetNoTransform(TRUE);
    }
    else if (StrCompareA((LPCSTR)cStrTokenA, "must-revalidate", TRUE) == 0)
    {
      if ((nHasItem & 0x0080) != 0)
        return MX_E_InvalidData;
      nHasItem |= 0x0080;
      //set value
      hRes = SetMustRevalidate(TRUE);
    }
    else if (StrCompareA((LPCSTR)cStrTokenA, "proxy-revalidate", TRUE) == 0)
    {
      if ((nHasItem & 0x0100) != 0)
        return MX_E_InvalidData;
      nHasItem |= 0x0100;
      //set value
      hRes = SetProxyRevalidate(TRUE);
    }
    else
    {
      nHasItem |= 0x1000;
      //parse cache extension
      hRes = AddExtension((LPCSTR)cStrTokenA, (LPCWSTR)cStrValueW);
    }
    if (FAILED(hRes))
      return hRes;

    //skip spaces
    szValueA = SkipSpaces(szValueA);

skip_null_listitem:
    //check for separator or end
    if (*szValueA == ',')
      szValueA++;
    else if (*szValueA != 0)
      return MX_E_InvalidData;
  }
  while (*szValueA != 0);
  //done
  return (nHasItem != 0) ? S_OK : MX_E_InvalidData;
}

HRESULT CHttpHeaderRespCacheControl::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  SIZE_T i, nCount;
  CStringA cStrTempA;

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
    for (i = 0; i < nCount; i++)
    {
      if (cStrDestA.Concat((i == 0) ? "=\"" : ",") == FALSE)
        return E_OUTOFMEMORY;
      if (cStrDestA.Concat(cPrivateFieldsList.GetElementAt(i)) == FALSE)
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
    for (i = 0; i < nCount; i++)
    {
      if (cStrDestA.Concat((i == 0) ? "=\"" : ",") == FALSE)
        return E_OUTOFMEMORY;
      if (cStrDestA.Concat(cNoCacheFieldsList.GetElementAt(i)) == FALSE)
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
  for (i = 0; i < nCount; i++)
  {
    if (CHttpCommon::BuildQuotedString(cStrTempA, cExtensionsList[i]->szValueW, StrLenW(cExtensionsList[i]->szValueW),
                                       FALSE) == FALSE)
    {
      return E_OUTOFMEMORY;
    }
    if (cStrDestA.AppendFormat(",%s=%s", cExtensionsList[i]->szNameA, (LPCSTR)cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
  }
  if (cStrTempA.IsEmpty() == FALSE)
    cStrTempA.Delete(0, 1); //delete initial comma
  //done
  return S_OK;
}

HRESULT CHttpHeaderRespCacheControl::SetPublic(_In_ BOOL _bPublic)
{
  bPublic = _bPublic;
  return S_OK;
}

BOOL CHttpHeaderRespCacheControl::GetPublic() const
{
  return bPublic;
}

HRESULT CHttpHeaderRespCacheControl::SetPrivate(_In_ BOOL _bPrivate)
{
  bPrivate = _bPrivate;
  return S_OK;
}

BOOL CHttpHeaderRespCacheControl::GetPrivate() const
{
  return bPrivate;
}

HRESULT CHttpHeaderRespCacheControl::AddPrivateField(_In_z_ LPCSTR szFieldA, _In_ SIZE_T nFieldLen)
{
  LPCSTR szStartA;
  CStringA cStrTempA;

  if (nFieldLen == (SIZE_T)-1)
    nFieldLen = StrLenA(szFieldA);
  if (nFieldLen == 0)
    return MX_E_InvalidData;
  if (szFieldA == NULL)
    return E_POINTER;
  //get token
  szFieldA = CHttpHeaderBase::GetToken(szStartA = szFieldA, nFieldLen);
  if (szStartA == szFieldA)
    return MX_E_InvalidData;
  //check for end
  if (szFieldA != szStartA + nFieldLen)
    return MX_E_InvalidData;
  //set new value
  if (cStrTempA.CopyN(szStartA, nFieldLen) == FALSE)
    return E_OUTOFMEMORY;
  if (cPrivateFieldsList.AddElement((LPSTR)cStrTempA) == FALSE)
    return E_OUTOFMEMORY;
  cStrTempA.Detach();
  //done
  return S_OK;
}

SIZE_T CHttpHeaderRespCacheControl::GetPrivateFieldsCount() const
{
  return cPrivateFieldsList.GetCount();
}

LPCSTR CHttpHeaderRespCacheControl::GetPrivateField(_In_ SIZE_T nIndex) const
{
  return (nIndex < cPrivateFieldsList.GetCount()) ? cPrivateFieldsList.GetElementAt(nIndex) : NULL;
}

BOOL CHttpHeaderRespCacheControl::HasPrivateField(_In_z_ LPCSTR szFieldA) const
{
  SIZE_T i, nCount;

  if (szFieldA != NULL && *szFieldA != 0)
  {
    nCount = cPrivateFieldsList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      if (StrCompareA(cPrivateFieldsList.GetElementAt(i), szFieldA, TRUE) == 0)
        return TRUE;
    }
  }
  return FALSE;
}

HRESULT CHttpHeaderRespCacheControl::SetNoCache(_In_ BOOL _bNoCache)
{
  bNoCache = _bNoCache;
  return S_OK;
}

BOOL CHttpHeaderRespCacheControl::GetNoCache() const
{
  return bNoCache;
}

HRESULT CHttpHeaderRespCacheControl::AddNoCacheField(_In_z_ LPCSTR szFieldA, _In_ SIZE_T nFieldLen)
{
  LPCSTR szStartA;
  CStringA cStrTempA;

  if (nFieldLen == (SIZE_T)-1)
    nFieldLen = StrLenA(szFieldA);
  if (nFieldLen == 0)
    return MX_E_InvalidData;
  if (szFieldA == NULL)
    return E_POINTER;
  //get token
  szFieldA = GetToken(szStartA = szFieldA, nFieldLen);
  if (szStartA == szFieldA)
    return MX_E_InvalidData;
  //check for end
  if (szFieldA != szStartA + nFieldLen)
    return MX_E_InvalidData;
  //set new value
  if (cStrTempA.CopyN(szStartA, nFieldLen) == FALSE)
    return E_OUTOFMEMORY;
  if (cNoCacheFieldsList.AddElement((LPSTR)cStrTempA) == FALSE)
    return E_OUTOFMEMORY;
  cStrTempA.Detach();
  //done
  return S_OK;
}

SIZE_T CHttpHeaderRespCacheControl::GetNoCacheFieldsCount() const
{
  return cNoCacheFieldsList.GetCount();
}

LPCSTR CHttpHeaderRespCacheControl::GetNoCacheField(_In_ SIZE_T nIndex) const
{
  return (nIndex < cNoCacheFieldsList.GetCount()) ? cNoCacheFieldsList.GetElementAt(nIndex) : NULL;
}

BOOL CHttpHeaderRespCacheControl::HasNoCacheField(_In_z_ LPCSTR szFieldA) const
{
  SIZE_T i, nCount;

  if (szFieldA != NULL && *szFieldA != 0)
  {
    nCount = cNoCacheFieldsList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      if (StrCompareA(cNoCacheFieldsList.GetElementAt(i), szFieldA, TRUE) == 0)
        return TRUE;
    }
  }
  return FALSE;
}

HRESULT CHttpHeaderRespCacheControl::SetNoStore(_In_ BOOL _bNoStore)
{
  bNoStore = _bNoStore;
  return S_OK;
}

BOOL CHttpHeaderRespCacheControl::GetNoStore() const
{
  return bNoStore;
}

HRESULT CHttpHeaderRespCacheControl::SetNoTransform(_In_ BOOL _bNoTransform)
{
  bNoTransform = _bNoTransform;
  return S_OK;
}

BOOL CHttpHeaderRespCacheControl::GetNoTransform() const
{
  return bNoTransform;
}

HRESULT CHttpHeaderRespCacheControl::SetMustRevalidate(_In_ BOOL _bMustRevalidate)
{
  bMustRevalidate = _bMustRevalidate;
  return S_OK;
}

BOOL CHttpHeaderRespCacheControl::GetMustRevalidate() const
{
  return bMustRevalidate;
}

HRESULT CHttpHeaderRespCacheControl::SetProxyRevalidate(_In_ BOOL _bProxyRevalidate)
{
  bProxyRevalidate = _bProxyRevalidate;
  return S_OK;
}

BOOL CHttpHeaderRespCacheControl::GetProxyRevalidate() const
{
  return bProxyRevalidate;
}

HRESULT CHttpHeaderRespCacheControl::SetMaxAge(_In_ ULONGLONG _nMaxAge)
{
  nMaxAge = _nMaxAge;
  return S_OK;
}

ULONGLONG CHttpHeaderRespCacheControl::GetMaxAge() const
{
  return nMaxAge;
}

HRESULT CHttpHeaderRespCacheControl::SetSharedMaxAge(_In_ ULONGLONG _nSharedMaxAge)
{
  nSMaxAge = _nSharedMaxAge;
  return S_OK;
}

ULONGLONG CHttpHeaderRespCacheControl::GetSharedMaxAge() const
{
  return nSMaxAge;
}

HRESULT CHttpHeaderRespCacheControl::AddExtension(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW)
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

LPCSTR CHttpHeaderRespCacheControl::GetExtensionName(_In_ SIZE_T nIndex) const
{
  return (nIndex < cExtensionsList.GetCount()) ? cExtensionsList[nIndex]->szNameA : NULL;
}

LPCWSTR CHttpHeaderRespCacheControl::GetExtensionValue(_In_ SIZE_T nIndex) const
{
  return (nIndex < cExtensionsList.GetCount()) ? cExtensionsList[nIndex]->szValueW : NULL;
}

LPCWSTR CHttpHeaderRespCacheControl::GetExtensionValue(_In_z_ LPCSTR szNameA) const
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

static HRESULT GetUInt64(_In_z_ LPCWSTR szValueW, _Out_ ULONGLONG &nValue)
{
  ULONGLONG nTemp;

  nValue = 0ui64;
  if (*szValueW < L'0' || *szValueW > L'9')
    return MX_E_InvalidData;
  while (*szValueW == L'0')
    szValueW++;
  while (*szValueW >= L'0' && *szValueW <= L'9')
  {
    nTemp = nValue * 10ui64;
    if (nTemp < nValue)
      return MX_E_ArithmeticOverflow;
    nValue = nTemp + (ULONGLONG)(*szValueW - L'0');
    if (nValue < nTemp)
      return MX_E_ArithmeticOverflow;
    szValueW++;
  }
  return (*szValueW == 0) ? S_OK : MX_E_InvalidData;
}
