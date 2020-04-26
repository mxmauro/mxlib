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
#include "..\..\Include\Http\HttpHeaderEntContentDisposition.h"
#include <intsafe.h>
#include "..\..\Include\AutoPtr.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderEntContentDisposition::CHttpHeaderEntContentDisposition() : CHttpHeaderBase()
{
  nSize = ULONGLONG_MAX;
  bHasFileName = FALSE;
  return;
}

CHttpHeaderEntContentDisposition::~CHttpHeaderEntContentDisposition()
{
  aParamsList.RemoveAllElements();
  return;
}

HRESULT CHttpHeaderEntContentDisposition::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  LPCSTR szValueEndA, szStartA;
  CStringA cStrTokenA;
  CStringW cStrValueW;
  CDateTime cDt;
  ULONG nHasItem;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  szValueEndA = szValueA + nValueLen;

  //skip spaces
  szValueA = SkipSpaces(szValueA, szValueEndA);

  //type
  szValueA = SkipUntil(szStartA = szValueA, szValueEndA, ";, \t");
  if (szValueA == szStartA)
    return MX_E_InvalidData;
  hRes = SetType(szStartA, (SIZE_T)(szValueA - szStartA));
  if (FAILED(hRes))
    return hRes;
  //skip spaces
  szValueA = SkipSpaces(szValueA, szValueEndA);
  //parameters
  nHasItem = 0;
  if (szValueA < szValueEndA && *szValueA == ';')
  {
    szValueA++;
    do
    {
      BOOL bExtendedParam;

      //skip spaces
      szValueA = SkipSpaces(szValueA, szValueEndA);
      if (szValueA >= szValueEndA)
        break;

      //get parameter
      hRes = GetParamNameAndValue(cStrTokenA, cStrValueW, szValueA, szValueEndA , &bExtendedParam);
      if (FAILED(hRes))
        return (hRes == MX_E_NoData) ? MX_E_InvalidData : hRes;

      //add parameter
      if (StrCompareA((LPCSTR)cStrTokenA, "name", TRUE) == 0)
      {
        if (bExtendedParam == FALSE)
        {
          if ((nHasItem & 0x0001) != 0)
            return MX_E_InvalidData;
          nHasItem |= 0x0001;
          //extended filename set?
          if ((nHasItem & 0x0010) == 0)
          {
            CStringW cStrNameW;

            //the returned value is ISO-8859-1, convert to UTF-8
            if (RawISO_8859_1_to_UTF8(cStrNameW, (LPCWSTR)cStrValueW, cStrValueW.GetLength()) == FALSE)
              return E_OUTOFMEMORY;
            //set name
            hRes = SetName((LPCWSTR)cStrNameW);
          }
          else
          {
            hRes = S_OK; //ignore this filename if the extended filename was set
          }
        }
        else
        {
          if ((nHasItem & 0x0010) != 0)
            return MX_E_InvalidData;
          nHasItem |= 0x0010;
          //set name
          hRes = SetName((LPCWSTR)cStrValueW);
        }
      }
      else if (StrCompareA((LPCSTR)cStrTokenA, "filename", TRUE) == 0)
      {
        if (bExtendedParam == FALSE)
        {
          if ((nHasItem & 0x0002) != 0)
            return MX_E_InvalidData;
          nHasItem |= 0x0002;
          //extended filename set?
          if ((nHasItem & 0x0020) == 0)
          {
            CStringW cStrFileNameW;

            //the returned value is ISO-8859-1, convert to UTF-8
            if (RawISO_8859_1_to_UTF8(cStrFileNameW, (LPCWSTR)cStrValueW, cStrValueW.GetLength()) == FALSE)
              return E_OUTOFMEMORY;
            //set filename
            hRes = SetFileName((LPCWSTR)cStrFileNameW);
          }
          else
          {
            hRes = S_OK; //ignore this filename if the extended filename was set
          }
        }
        else
        {
          if ((nHasItem & 0x0020) != 0)
            return MX_E_InvalidData;
          nHasItem |= 0x0020;
          //set filename
          hRes = SetFileName((LPCWSTR)cStrValueW);
        }
      }
      else if (StrCompareA((LPCSTR)cStrTokenA, "creation-date", TRUE) == 0)
      {
        if ((nHasItem & 0x0004) != 0)
          return MX_E_InvalidData;
        nHasItem |= 0x0004;
        //set creation date
        hRes = Http::ParseDate(cDt, (LPCWSTR)cStrValueW);
        if (SUCCEEDED(hRes))
          hRes = SetCreationDate(cDt);
      }
      else if (StrCompareA((LPCSTR)cStrTokenA, "modification-date", TRUE) == 0)
      {
        if ((nHasItem & 0x0008) != 0)
          return MX_E_InvalidData;
        nHasItem |= 0x0008;
        //set creation date
        hRes = Http::ParseDate(cDt, (LPCWSTR)cStrValueW);
        if (SUCCEEDED(hRes))
          hRes = SetModificationDate(cDt);
      }
      else if (StrCompareA((LPCSTR)cStrTokenA, "read-date", TRUE) == 0)
      {
        if ((nHasItem & 0x0040) != 0)
          return MX_E_InvalidData;
        nHasItem |= 0x0040;
        //set creation date
        hRes = Http::ParseDate(cDt, (LPCWSTR)cStrValueW);
        if (SUCCEEDED(hRes))
          hRes = SetReadDate(cDt);
      }
      else if (StrCompareA((LPCSTR)cStrTokenA, "size", TRUE) == 0)
      {
        ULONGLONG nTemp, nSize;
        LPCWSTR sW = (LPCWSTR)cStrValueW;

        if ((nHasItem & 0x0080) != 0)
          return MX_E_InvalidData;
        nHasItem |= 0x0080;
        //parse value
        if (*sW < L'0' || *sW > L'9')
          return MX_E_InvalidData;
        while (*sW == L'0')
          sW++;
        nSize = 0ui64;
        while (*sW >= L'0' && *sW <= L'9')
        {
          nTemp = nSize * 10ui64;
          if (nTemp < nSize)
            return MX_E_ArithmeticOverflow;
          nSize = nTemp + (ULONGLONG)(*sW - '0');
          if (nSize < nTemp)
            return MX_E_ArithmeticOverflow;
          sW++;
        }
        hRes = SetSize(nSize);
      }
      else
      {
        //add parameter
        hRes = AddParam((LPCSTR)cStrTokenA, (LPCWSTR)cStrValueW);
      }
      if (FAILED(hRes))
        return hRes;

      //check for separator or end
      if (szValueA < szValueEndA)
      {
        if (*szValueA == ';')
          szValueA++;
        else
          return MX_E_InvalidData;
      }
    }
    while (szValueA < szValueEndA);
  }
  //check for separator or end
  if (SkipSpaces(szValueA, szValueEndA) != szValueEndA)
    return MX_E_InvalidData;
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentDisposition::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  SIZE_T i, nCount;
  CStringA cStrTempA;
  HRESULT hRes;

  if (cStrTypeA.IsEmpty() != FALSE)
  {
    cStrDestA.Empty();
    return S_OK;
  }
  if (cStrDestA.Copy((LPCSTR)cStrTypeA) == FALSE)
    return E_OUTOFMEMORY;

  //name
  if (cStrNameW.IsEmpty() == FALSE)
  {
    CStringA cStrTempA_2;

    if (nBrowser == Http::BrowserIE || nBrowser == Http::BrowserIE6 || nBrowser == Http::BrowserSafari)
    {
      if (Http::BuildExtendedValueString(cStrTempA, (LPCWSTR)cStrNameW, cStrFileNameW.GetLength()) == FALSE)
        return E_OUTOFMEMORY;
      //remove UTF-8'' prefix
      if (cStrDestA.AppendFormat("; name=\"%s\"", (LPCSTR)cStrTempA + 7) == FALSE)
        return E_OUTOFMEMORY;
    }
    else
    {
      if (Http::BuildQuotedString(cStrTempA, (LPCWSTR)cStrNameW, cStrNameW.GetLength(), FALSE) == FALSE ||
          Http::BuildExtendedValueString(cStrTempA_2, (LPCWSTR)cStrNameW, cStrNameW.GetLength()) == FALSE ||
          cStrDestA.AppendFormat("; name=%s; name*=%s", (LPCSTR)cStrTempA, (LPCSTR)cStrTempA_2) == FALSE)
      {
        return E_OUTOFMEMORY;
      }
    }
  }

  //filename
  if (cStrFileNameW.IsEmpty() == FALSE)
  {
    CStringA cStrTempA_2;

    //NOTE: If the following code does not work, try a review of
    //      https://fastmail.blog/2011/06/24/download-non-english-filenames/
    if (nBrowser == Http::BrowserIE || nBrowser == Http::BrowserIE6 || nBrowser == Http::BrowserSafari)
    {
      if (Http::BuildExtendedValueString(cStrTempA, (LPCWSTR)cStrFileNameW, cStrFileNameW.GetLength()) == FALSE)
        return E_OUTOFMEMORY;
      //remove UTF-8'' prefix
      if (cStrDestA.AppendFormat("; filename=\"%s\"", (LPCSTR)cStrTempA + 7) == FALSE)
        return E_OUTOFMEMORY;
    }
    else
    {
      if (Http::BuildQuotedString(cStrTempA, (LPCWSTR)cStrFileNameW, cStrFileNameW.GetLength(), FALSE) == FALSE ||
          Http::BuildExtendedValueString(cStrTempA_2, (LPCWSTR)cStrFileNameW, cStrFileNameW.GetLength()) == FALSE ||
          cStrDestA.AppendFormat("; filename=%s; filename*=%s", (LPCSTR)cStrTempA, (LPCSTR)cStrTempA_2) == FALSE)
      {
        return E_OUTOFMEMORY;
      }
    }
  }

  //creation date
  if (cCreationDt.GetTicks() != 0)
  {
    hRes = cCreationDt.Format(cStrTempA, "%a, %d %b %Y %H:%M:%S %z");
    if (FAILED(hRes))
      return hRes;
    if (cStrDestA.AppendFormat("; creation-date=\"%s\"", (LPSTR)cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
  }
  //modification date
  if (cModificationDt.GetTicks() != 0)
  {
    hRes = cModificationDt.Format(cStrTempA, "%a, %d %b %Y %H:%M:%S %z");
    if (FAILED(hRes))
      return hRes;
    if (cStrDestA.AppendFormat("; modification-date=\"%s\"", (LPSTR)cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
  }

  //read date
  if (cReadDt.GetTicks() != 0)
  {
    hRes = cReadDt.Format(cStrTempA, "%a, %d %b %Y %H:%M:%S %z");
    if (FAILED(hRes))
      return hRes;
    if (cStrDestA.AppendFormat("; read-date=\"%s\"", (LPSTR)cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
  }

  //size
  if (nSize != ULONGLONG_MAX)
  {
    if (cStrDestA.AppendFormat("; size=%I64u", nSize) == FALSE)
      return E_OUTOFMEMORY;
  }

  //parameters
  nCount = aParamsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (Http::BuildQuotedString(cStrTempA, aParamsList[i]->szValueW, StrLenW(aParamsList[i]->szValueW), FALSE) == FALSE)
      return E_OUTOFMEMORY;
    if (cStrDestA.AppendFormat(";%s=%s", aParamsList[i]->szNameA, (LPCSTR)cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentDisposition::SetType(_In_z_ LPCSTR szTypeA, _In_ SIZE_T nTypeLen)
{
  LPCSTR szStartA;

  if (nTypeLen == (SIZE_T)-1)
    nTypeLen = StrLenA(szTypeA);

  if (nTypeLen == 0)
    return MX_E_InvalidData;
  if (szTypeA == NULL)
    return E_POINTER;

  //get token
  szStartA = szTypeA;
  szTypeA = GetToken(szTypeA, szTypeA + nTypeLen);
  if ((SIZE_T)(szTypeA - szStartA) != nTypeLen)
    return MX_E_InvalidData;

  //set new value
  if (cStrTypeA.CopyN(szStartA, nTypeLen) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCSTR CHttpHeaderEntContentDisposition::GetType() const
{
  return (LPCSTR)cStrTypeA;
}

HRESULT CHttpHeaderEntContentDisposition::SetName(_In_opt_z_ LPCWSTR szNameW, _In_ SIZE_T nNameLen)
{
  if (nNameLen == (SIZE_T)-1)
    nNameLen = StrLenW(szNameW);

  //set new value
  if (nNameLen > 0)
  {
    if (szNameW == NULL)
      return E_POINTER;
    if (cStrNameW.CopyN(szNameW, nNameLen) == FALSE)
      return E_OUTOFMEMORY;
  }
  else
  {
    cStrNameW.Empty();
  }
  //done
  return S_OK;
}

LPCWSTR CHttpHeaderEntContentDisposition::GetName() const
{
  return (LPCWSTR)cStrNameW;
}

HRESULT CHttpHeaderEntContentDisposition::SetFileName(_In_opt_z_ LPCWSTR szFileNameW, _In_ SIZE_T nFileNameLen)
{
  if (nFileNameLen == (SIZE_T)-1)
    nFileNameLen = StrLenW(szFileNameW);

  //set new value
  if (nFileNameLen > 0)
  {
    if (szFileNameW == NULL)
      return E_POINTER;
    if (cStrFileNameW.CopyN(szFileNameW, nFileNameLen) == FALSE)
      return E_OUTOFMEMORY;
    bHasFileName = TRUE;
  }
  else
  {
    cStrFileNameW.Empty();
    bHasFileName = (szFileNameW != NULL) ? TRUE : FALSE;
  }
  //done
  return S_OK;
}

LPCWSTR CHttpHeaderEntContentDisposition::GetFileName() const
{
  return (LPCWSTR)cStrFileNameW;
}

BOOL CHttpHeaderEntContentDisposition::HasFileName() const
{
  return bHasFileName;
}

HRESULT CHttpHeaderEntContentDisposition::SetCreationDate(_In_ CDateTime &cDt)
{
  cCreationDt = cDt;
  //done
  return S_OK;
}

CDateTime CHttpHeaderEntContentDisposition::GetCreationDate() const
{
  return cCreationDt;
}

HRESULT CHttpHeaderEntContentDisposition::SetModificationDate(_In_ CDateTime &cDt)
{
  cModificationDt = cDt;
  //done
  return S_OK;
}

CDateTime CHttpHeaderEntContentDisposition::GetModificationDate() const
{
  return cModificationDt;
}

HRESULT CHttpHeaderEntContentDisposition::SetReadDate(_In_ CDateTime &cDt)
{
  cReadDt = cDt;
  //done
  return S_OK;
}

CDateTime CHttpHeaderEntContentDisposition::GetReadDate() const
{
  return cReadDt;
}

HRESULT CHttpHeaderEntContentDisposition::SetSize(_In_ ULONGLONG _nSize)
{
  nSize = _nSize;
  //done
  return S_OK;
}

ULONGLONG CHttpHeaderEntContentDisposition::GetSize() const
{
  return nSize;
}

HRESULT CHttpHeaderEntContentDisposition::AddParam(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW)
{
  TAutoFreePtr<PARAMETER> cNewParam;
  LPCSTR szStartA;
  SIZE_T nNameLen, nValueLen;

  if (szNameA == NULL)
    return E_POINTER;
  if (szValueW == NULL)
    szValueW = L"";

  nNameLen = StrLenA(szNameA);

  //get token
  szNameA = CHttpHeaderBase::GetToken(szStartA = szNameA, szNameA + nNameLen);
  if (szStartA == szNameA || szNameA != szStartA + nNameLen)
    return MX_E_InvalidData;

  //get value length
  nValueLen = (StrLenW(szValueW) + 1) * sizeof(WCHAR);

  //create new item
  cNewParam.Attach((LPPARAMETER)MX_MALLOC(sizeof(PARAMETER) + nNameLen + nValueLen));
  if (!cNewParam)
    return E_OUTOFMEMORY;
  MxMemCopy(cNewParam->szNameA, szStartA, nNameLen);
  cNewParam->szNameA[nNameLen] = 0;
  cNewParam->szValueW = (LPWSTR)((LPBYTE)(cNewParam->szNameA) + (nNameLen + 1));
  MxMemCopy(cNewParam->szValueW, szValueW, nValueLen);

  //add to list
  if (aParamsList.AddElement(cNewParam.Get()) == FALSE)
    return E_OUTOFMEMORY;
  cNewParam.Detach();

  //done
  return S_OK;
}

SIZE_T CHttpHeaderEntContentDisposition::GetParamsCount() const
{
  return aParamsList.GetCount();
}

LPCSTR CHttpHeaderEntContentDisposition::GetParamName(_In_ SIZE_T nIndex) const
{
  return (nIndex < aParamsList.GetCount()) ? aParamsList[nIndex]->szNameA : NULL;
}

LPCWSTR CHttpHeaderEntContentDisposition::GetParamValue(_In_ SIZE_T nIndex) const
{
  return (nIndex < aParamsList.GetCount()) ? aParamsList[nIndex]->szValueW : NULL;
}

LPCWSTR CHttpHeaderEntContentDisposition::GetParamValue(_In_z_ LPCSTR szNameA) const
{
  SIZE_T i, nCount;

  if (szNameA != NULL && *szNameA != 0)
  {
    nCount = aParamsList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      if (StrCompareA(aParamsList[i]->szNameA, szNameA, TRUE) == 0)
        return aParamsList[i]->szValueW;
    }
  }
  return NULL;
}

} //namespace MX
