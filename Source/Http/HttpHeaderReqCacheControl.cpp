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
#include "..\..\Include\Http\HttpHeaderReqCacheControl.h"
#include <stdlib.h>
#include <intsafe.h>
#include "..\..\Include\AutoPtr.h"

//-----------------------------------------------------------

static HRESULT GetUInt64(_In_z_ LPCWSTR szValueW, _Out_ ULONGLONG &nValue);

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
  return;
}

HRESULT CHttpHeaderReqCacheControl::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  LPCSTR szValueEndA;
  CStringA cStrTokenA;
  CStringW cStrValueW;
  ULONGLONG nValue;
  ULONG nHasItem;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  szValueEndA = szValueA + nValueLen;

  //parse
  nHasItem = 0;
  do
  {
    //skip spaces
    szValueA = SkipSpaces(szValueA, szValueEndA);
    if (szValueA >= szValueEndA)
      break;
    if (*szValueA == ',')
      goto skip_null_listitem;

    //get parameter
    hRes = GetParamNameAndValue(TRUE, cStrTokenA, cStrValueW, szValueA, szValueEndA);
    if (FAILED(hRes) && hRes != MX_E_NoData)
      return hRes;

    //set item value
    if (StrCompareA((LPCSTR)cStrTokenA, "max-age", TRUE) == 0)
    {
      if ((nHasItem & 0x0001) != 0)
        return MX_E_InvalidData;
      nHasItem |= 0x0001;

      //parse delta seconds
      hRes = GetUInt64((LPCWSTR)cStrValueW, nValue);
      if (SUCCEEDED(hRes))
        hRes = SetMaxAge(nValue);
    }
    else if (StrCompareA((LPCSTR)cStrTokenA, "no-cache", TRUE) == 0)
    {
      if ((nHasItem & 0x0002) != 0)
        return MX_E_InvalidData;
      nHasItem |= 0x0002;

      //set value
      hRes = SetNoCache(TRUE);
    }
    else if (StrCompareA((LPCSTR)cStrTokenA, "no-store", TRUE) == 0)
    {
      if ((nHasItem & 0x0004) != 0)
        return MX_E_InvalidData;
      nHasItem |= 0x0004;

      //set value
      hRes = SetNoStore(TRUE);
    }
    else if (StrCompareA((LPCSTR)cStrTokenA, "max-stale", TRUE) == 0)
    {
      if ((nHasItem & 0x0008) != 0)
        return MX_E_InvalidData;
      nHasItem |= 0x0008;

      //parse delta seconds
      hRes = GetUInt64((LPCWSTR)cStrValueW, nValue);
      if (SUCCEEDED(hRes))
        hRes = SetMaxStale(nValue);
    }
    else if (StrCompareA((LPCSTR)cStrTokenA, "min-fresh", TRUE) == 0)
    {
      if ((nHasItem & 0x0010) != 0)
        return MX_E_InvalidData;
      nHasItem |= 0x0010;

      //parse delta seconds
      hRes = GetUInt64((LPCWSTR)cStrValueW, nValue);
      if (SUCCEEDED(hRes))
        hRes = SetMinFresh(nValue);
    }
    else if (StrCompareA((LPCSTR)cStrTokenA, "no-transform", TRUE) == 0)
    {
      if ((nHasItem & 0x0020) != 0)
        return MX_E_InvalidData;
      nHasItem |= 0x0020;

      //set value
      hRes = SetNoTransform(TRUE);
    }
    else if (StrCompareA((LPCSTR)cStrTokenA, "only-if-cached", TRUE) == 0)
    {
      if ((nHasItem & 0x0040) != 0)
        return MX_E_InvalidData;
      nHasItem |= 0x0040;

      //set value
      hRes = SetOnlyIfCached(TRUE);
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
    szValueA = SkipSpaces(szValueA, szValueEndA);

skip_null_listitem:
    //check for separator or end
    if (szValueA < szValueEndA)
    {
      if (*szValueA == ',')
        szValueA++;
      else
        return MX_E_InvalidData;
    }
  }
  while (szValueA < szValueEndA);

  //do we got one?
  if (nHasItem == 0)
    return MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpHeaderReqCacheControl::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  CStringA cStrTempA;
  SIZE_T i, nCount;

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
  nCount = aExtensionsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (Http::BuildQuotedString(cStrTempA, aExtensionsList[i]->szValueW, StrLenW(aExtensionsList[i]->szValueW),
                                FALSE) == FALSE)
    {
      return E_OUTOFMEMORY;
    }
    if (cStrDestA.AppendFormat(",%s=%s", aExtensionsList[i]->szNameA, (LPCSTR)cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
  }
  if (cStrDestA.IsEmpty() == FALSE)
    cStrDestA.Delete(0, 1); //delete initial comma

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
  LPCSTR szStartA;
  SIZE_T nNameLen, nValueLen;

  if (szNameA == NULL)
    return E_POINTER;
  if (szValueW == NULL)
    szValueW = L"";

  nNameLen = StrLenA(szNameA);

  //get token
  szNameA = GetToken(szStartA = szNameA, szNameA + nNameLen);
  if (szStartA == szNameA || szNameA != szStartA + nNameLen)
    return MX_E_InvalidData;

  //get name and value length
  nValueLen = (StrLenW(szValueW) + 1) * sizeof(WCHAR);

  //create new item
  cNewExtension.Attach((LPEXTENSION)MX_MALLOC(sizeof(EXTENSION) + nNameLen + nValueLen));
  if (!cNewExtension)
    return E_OUTOFMEMORY;
  ::MxMemCopy(cNewExtension->szNameA, szStartA, nNameLen);
  cNewExtension->szNameA[nNameLen] = 0;
  cNewExtension->szValueW = (LPWSTR)((LPBYTE)cNewExtension.Get() + nNameLen);
  ::MxMemCopy(cNewExtension->szValueW, szValueW, nValueLen);

  //add to list
  if (aExtensionsList.AddElement(cNewExtension.Get()) == FALSE)
    return E_OUTOFMEMORY;

  //done
  cNewExtension.Detach();
  return S_OK;
}

SIZE_T CHttpHeaderReqCacheControl::GetExtensionsCount() const
{
  return aExtensionsList.GetCount();
}

LPCSTR CHttpHeaderReqCacheControl::GetExtensionName(_In_ SIZE_T nIndex) const
{
  return (nIndex < aExtensionsList.GetCount()) ? aExtensionsList[nIndex]->szNameA : NULL;
}

LPCWSTR CHttpHeaderReqCacheControl::GetExtensionValue(_In_ SIZE_T nIndex) const
{
  return (nIndex < aExtensionsList.GetCount()) ? aExtensionsList[nIndex]->szValueW : NULL;
}

LPCWSTR CHttpHeaderReqCacheControl::GetExtensionValue(_In_z_ LPCSTR szNameA) const
{
  SIZE_T i, nCount;

  if (szNameA != NULL && szNameA[0] != 0)
  {
    nCount = aExtensionsList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      if (StrCompareA(aExtensionsList[i]->szNameA, szNameA, TRUE) == 0)
        return aExtensionsList[i]->szValueW;
    }
  }
  return NULL;
}

HRESULT CHttpHeaderReqCacheControl::Merge(_In_ CHttpHeaderBase *_lpHeader)
{
  CHttpHeaderReqCacheControl *lpHeader = reinterpret_cast<CHttpHeaderReqCacheControl*>(_lpHeader);
  SIZE_T i, nCount, nThisIndex, nThisCount;

  if (lpHeader->bNoCache != FALSE)
  {
    bNoCache = TRUE;
  }

  if (lpHeader->bNoStore != FALSE)
  {
    bNoStore = TRUE;
  }

  if (lpHeader->nMaxAge != ULONGLONG_MAX)
  {
    nMaxAge = lpHeader->nMaxAge;
  }

  if (lpHeader->nMaxStale != ULONGLONG_MAX)
  {
    nMaxStale = lpHeader->nMaxStale;
  }

  if (lpHeader->nMinFresh != ULONGLONG_MAX)
  {
    nMinFresh = lpHeader->nMinFresh;
  }

  if (lpHeader->bNoTransform != FALSE)
  {
    bNoTransform = TRUE;
  }

  if (lpHeader->bOnlyIfCached != FALSE)
  {
    bOnlyIfCached = TRUE;
  }

  nCount = lpHeader->aExtensionsList.GetCount();
  nThisCount = aExtensionsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    LPEXTENSION lpExtension = lpHeader->aExtensionsList.GetElementAt(i);
    TAutoFreePtr<EXTENSION> cNewExtension;
    SIZE_T nNameLen, nValueLen;

    for (nThisIndex = 0; nThisIndex < nThisCount; nThisIndex++)
    {
      LPEXTENSION lpThisExtension = aExtensionsList.GetElementAt(nThisIndex);

      if (StrCompareA(lpExtension->szNameA, lpThisExtension->szNameA, TRUE) == 0)
        break;
    }
    if (nThisIndex < nThisCount)
    {
      //remove the old
      aExtensionsList.RemoveElementAt(nThisIndex);
    }

    //add the new extension

    //get name value length
    nNameLen = StrLenA(lpExtension->szNameA);
    nValueLen = (StrLenW(lpExtension->szValueW) + 1) * sizeof(WCHAR);

    //create new item
    cNewExtension.Attach((LPEXTENSION)MX_MALLOC(sizeof(EXTENSION) + nNameLen + nValueLen));
    if (!cNewExtension)
      return E_OUTOFMEMORY;
    ::MxMemCopy(cNewExtension->szNameA, lpExtension->szNameA, nNameLen);
    cNewExtension->szNameA[nNameLen] = 0;
    cNewExtension->szValueW = (LPWSTR)((LPBYTE)cNewExtension.Get() + nNameLen);
    ::MxMemCopy(cNewExtension->szValueW, lpExtension->szValueW, nValueLen);

    //add to list
    if (aExtensionsList.AddElement(cNewExtension.Get()) == FALSE)
      return E_OUTOFMEMORY;
    cNewExtension.Detach();
    nThisCount++;
  }

  //done
  return S_OK;
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
