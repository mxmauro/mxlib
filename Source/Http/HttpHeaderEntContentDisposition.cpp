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
#include "..\..\Include\Http\HttpHeaderEntContentDisposition.h"

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
  cParamsList.RemoveAllElements();
  return;
}

HRESULT CHttpHeaderEntContentDisposition::Parse(__in_z LPCSTR szValueA)
{
  LPCSTR szStartA;
  CStringA cStrTempA, cStrTempA_2;
  CStringW cStrTempW;
  CDateTime cDt;
  ULONG nHasItem;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //mime type
  szValueA = Advance(szStartA = szValueA, ";, \t");
  if (cStrTempA.CopyN(szStartA, (SIZE_T)(szValueA-szStartA)) == FALSE)
    return E_OUTOFMEMORY;
  if (szValueA == szStartA)
    return MX_E_InvalidData;
  hRes = SetType((LPCSTR)cStrTempA);
  if (FAILED(hRes))
    return hRes;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //parameters
  nHasItem = 0;
  if (*szValueA == ';')
  {
    szValueA++;
    do
    {
      //skip spaces
      szValueA = SkipSpaces(szValueA);
      if (*szValueA == 0)
        break;
      //parse name
      szValueA = GetToken(szStartA = szValueA);
      if (cStrTempA.CopyN(szStartA, (SIZE_T)(szValueA-szStartA)) == FALSE)
        return E_OUTOFMEMORY;
      //skip spaces
      szValueA = SkipSpaces(szValueA);
      //is equal sign?
      if (*szValueA++ != '=')
        return MX_E_InvalidData;
      //skip spaces
      szValueA = SkipSpaces(szValueA);
      //parse value
      if (*szValueA == '"')
      {
        szValueA = Advance(szStartA = ++szValueA, "\"");
        if (*szValueA != '"')
          return MX_E_InvalidData;
        if (cStrTempA_2.CopyN(szStartA, (SIZE_T)(szValueA-szStartA)) == FALSE)
          return E_OUTOFMEMORY;
        szValueA++;
      }
      else
      {
        szValueA = Advance(szStartA = szValueA, " \t;,");
        if (cStrTempA_2.CopyN(szStartA, (SIZE_T)(szValueA-szStartA)) == FALSE)
          return E_OUTOFMEMORY;
      }
      //add parameter
      if (StrCompareA((LPSTR)cStrTempA, "filename", TRUE) == 0)
      {
        if ((nHasItem & 0x0001) != 0)
          return MX_E_InvalidData;
        nHasItem |= 0x0001;
        //set filename
        hRes = Utf8_Decode(cStrTempW, (LPSTR)cStrTempA_2);
        if (SUCCEEDED(hRes))
          hRes = SetFileName((LPWSTR)cStrTempW);
      }
      else if (StrCompareA((LPSTR)cStrTempA, "creation-date", TRUE) == 0)
      {
        if ((nHasItem & 0x0002) != 0)
          return MX_E_InvalidData;
        nHasItem |= 0x0002;
        //set creation date
        hRes = CHttpCommon::ParseDate(cDt, (LPSTR)cStrTempA_2);
        if (SUCCEEDED(hRes))
          hRes = SetCreationDate(cDt);
      }
      else if (StrCompareA((LPSTR)cStrTempA, "modification-date", TRUE) == 0)
      {
        if ((nHasItem & 0x0004) != 0)
          return MX_E_InvalidData;
        nHasItem |= 0x0004;
        //set creation date
        hRes = CHttpCommon::ParseDate(cDt, (LPSTR)cStrTempA_2);
        if (SUCCEEDED(hRes))
          hRes = SetModificationDate(cDt);
      }
      else if (StrCompareA((LPSTR)cStrTempA, "read-date", TRUE) == 0)
      {
        if ((nHasItem & 0x0008) != 0)
          return MX_E_InvalidData;
        nHasItem |= 0x0008;
        //set creation date
        hRes = CHttpCommon::ParseDate(cDt, (LPSTR)cStrTempA_2);
        if (SUCCEEDED(hRes))
          hRes = SetReadDate(cDt);
      }
      else if (StrCompareA((LPSTR)cStrTempA, "size", TRUE) == 0)
      {
        ULONGLONG nTemp, nSize;
        LPSTR sA = (LPSTR)cStrTempA_2;

        //parse value
        if (*sA < '0' || *sA > '9')
          return MX_E_InvalidData;
        while (*sA == '0')
          sA++;
        nSize = 0;
        while (*sA >= '0' && *sA <= '9')
        {
          nTemp = nSize * 10ui64;
          if (nTemp < nSize)
            return MX_E_ArithmeticOverflow;
          nSize = nTemp + (ULONGLONG)(*sA - '0');
          if (nSize < nTemp)
            return MX_E_ArithmeticOverflow;
          sA++;
        }
        hRes = SetSize(nSize);
      }
      else
      {
        //add parameter
        hRes = Utf8_Decode(cStrTempW, (LPSTR)cStrTempA_2);
        if (SUCCEEDED(hRes))
          hRes = AddParam((LPSTR)cStrTempA, (LPWSTR)cStrTempW);
      }
      if (FAILED(hRes))
        return hRes;
      //skip spaces
      szValueA = SkipSpaces(szValueA);
      //check for separator or end
      if (*szValueA == ';')
        szValueA++;
      else if (*szValueA != 0)
        return MX_E_InvalidData;
    }
    while (*szValueA != 0);
  }
  //check for separator or end
  if (*SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentDisposition::Build(__inout CStringA &cStrDestA)
{
  SIZE_T i, nCount;
  LPWSTR sW;
  CStringA cStrTempA;
  HRESULT hRes;

  if (cStrTypeA.IsEmpty() != FALSE)
  {
    cStrDestA.Empty();
    return S_OK;
  }
  if (cStrDestA.Copy((LPCSTR)cStrTypeA) == FALSE)
    return E_OUTOFMEMORY;
  //filename
  if (cStrFileNameW.IsEmpty() == FALSE)
  {
    hRes = Utf8_Encode(cStrTempA, (LPWSTR)cStrFileNameW);
    if (FAILED(hRes))
      return hRes;
    if (CHttpCommon::EncodeQuotedString(cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
    if (cStrDestA.AppendFormat(";filename=\"%s\"", (LPSTR)cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
  }
  //creation date
  if (cCreationDt.GetTicks() != 0)
  {
    hRes = cCreationDt.Format(cStrTempA, "%a, %d %b %Y %H:%M:%S %z");
    if (FAILED(hRes))
      return hRes;
    if (cStrDestA.AppendFormat(";creation-date=\"%s\"", (LPSTR)cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
  }
  //modification date
  if (cModificationDt.GetTicks() != 0)
  {
    hRes = cModificationDt.Format(cStrTempA, "%a, %d %b %Y %H:%M:%S %z");
    if (FAILED(hRes))
      return hRes;
    if (cStrDestA.AppendFormat(";modification-date=\"%s\"", (LPSTR)cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
  }
  //read date
  if (cReadDt.GetTicks() != 0)
  {
    hRes = cReadDt.Format(cStrTempA, "%a, %d %b %Y %H:%M:%S %z");
    if (FAILED(hRes))
      return hRes;
    if (cStrDestA.AppendFormat(";read-date=\"%s\"", (LPSTR)cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
  }
  //size
  if (nSize != ULONGLONG_MAX)
  {
    if (cStrDestA.AppendFormat(";size=%I64u", nSize) == FALSE)
      return E_OUTOFMEMORY;
  }
  //parameters
  nCount = cParamsList.GetCount();
  for (i=0; i<nCount; i++)
  {
    if (cStrDestA.AppendFormat(";%s=", cParamsList[i]->szNameA) == FALSE)
      return E_OUTOFMEMORY;
    sW = cParamsList[i]->szValueW;
    while (*sW != 0)
    {
      if (*sW < 33 || *sW > 126 || CHttpCommon::IsValidNameChar((CHAR)(*sW)) == FALSE)
        break;
      sW++;
    }
    if (*sW == 0)
    {
      if (cStrDestA.AppendFormat("%S", cParamsList[i]->szValueW) == FALSE)
        return E_OUTOFMEMORY;
    }
    else
    {
      hRes = Utf8_Encode(cStrTempA, cParamsList[i]->szValueW);
      if (FAILED(hRes))
        return hRes;
      if (CHttpCommon::EncodeQuotedString(cStrTempA) == FALSE)
        return E_OUTOFMEMORY;
      if (cStrDestA.AppendFormat("\"%s\"", (LPCSTR)cStrTempA) == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentDisposition::SetType(__in_z LPCSTR szTypeA)
{
  LPCSTR szStartA, szEndA;

  if (szTypeA == NULL)
    return E_POINTER;
  //skip spaces
  szTypeA = SkipSpaces(szTypeA);
  //get token
  szTypeA = GetToken(szStartA = szTypeA);
  switch ((SIZE_T)(szTypeA-szStartA))
  {
    case 0:
      return MX_E_InvalidData;
    case 2:
      if ((szStartA[0] == 'X' || szStartA[0] == 'x') && szStartA[1] == '-')
        return MX_E_InvalidData;
      break;
  }
  szEndA = szTypeA;
  //skip spaces and check for end
  if (*SkipSpaces(szTypeA) != 0)
    return MX_E_InvalidData;
  //set new value
  if (cStrTypeA.CopyN(szStartA, (SIZE_T)(szEndA-szStartA)) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCSTR CHttpHeaderEntContentDisposition::GetType() const
{
  return (LPCSTR)cStrTypeA;
}

HRESULT CHttpHeaderEntContentDisposition::SetFileName(__in_z LPCWSTR szFileNameW)
{
  //set new value
  if (szFileNameW != NULL && *szFileNameW != 0)
  {
    if (cStrFileNameW.Copy(szFileNameW) == FALSE)
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
  return (bHasFileName != FALSE) ? (LPCWSTR)cStrFileNameW : NULL;
}

HRESULT CHttpHeaderEntContentDisposition::SetCreationDate(__in CDateTime &cDt)
{
  cCreationDt = cDt;
  //done
  return S_OK;
}

CDateTime CHttpHeaderEntContentDisposition::GetCreationDate() const
{
  return cCreationDt;
}

HRESULT CHttpHeaderEntContentDisposition::SetModificationDate(__in CDateTime &cDt)
{
  cModificationDt = cDt;
  //done
  return S_OK;
}

CDateTime CHttpHeaderEntContentDisposition::GetModificationDate() const
{
  return cModificationDt;
}

HRESULT CHttpHeaderEntContentDisposition::SetReadDate(__in CDateTime &cDt)
{
  cReadDt = cDt;
  //done
  return S_OK;
}

CDateTime CHttpHeaderEntContentDisposition::GetReadDate() const
{
  return cReadDt;
}

HRESULT CHttpHeaderEntContentDisposition::SetSize(__in ULONGLONG _nSize)
{
  nSize = _nSize;
  //done
  return S_OK;
}

ULONGLONG CHttpHeaderEntContentDisposition::GetSize() const
{
  return nSize;
}

HRESULT CHttpHeaderEntContentDisposition::AddParam(__in_z LPCSTR szNameA, __in_z LPCWSTR szValueW)
{
  TAutoFreePtr<PARAMETER> cNewParam;
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
  cNewParam.Attach((LPPARAMETER)MX_MALLOC(sizeof(PARAMETER) + nNameLen + nValueLen));
  if (!cNewParam)
    return E_OUTOFMEMORY;
  MemCopy(cNewParam->szNameA, szStartA, nNameLen);
  cNewParam->szNameA[nNameLen] = 0;
  cNewParam->szValueW = (LPWSTR)((LPBYTE)(cNewParam->szNameA) + (nNameLen+1));
  MemCopy(cNewParam->szValueW, szValueW, nValueLen);
  //add to list
  if (cParamsList.AddElement(cNewParam.Get()) == FALSE)
    return E_OUTOFMEMORY;
  //done
  cNewParam.Detach();
  return S_OK;
}

SIZE_T CHttpHeaderEntContentDisposition::GetParamsCount() const
{
  return cParamsList.GetCount();
}

LPCSTR CHttpHeaderEntContentDisposition::GetParamName(__in SIZE_T nIndex) const
{
  return (nIndex < cParamsList.GetCount()) ? cParamsList[nIndex]->szNameA : NULL;
}

LPCWSTR CHttpHeaderEntContentDisposition::GetParamValue(__in SIZE_T nIndex) const
{
  return (nIndex < cParamsList.GetCount()) ? cParamsList[nIndex]->szValueW : NULL;
}

LPCWSTR CHttpHeaderEntContentDisposition::GetParamValue(__in_z LPCSTR szNameA) const
{
  SIZE_T i, nCount;

  if (szNameA != NULL && szNameA[0] != 0)
  {
    nCount = cParamsList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (StrCompareA(cParamsList[i]->szNameA, szNameA, TRUE) == 0)
        return cParamsList[i]->szValueW;
    }
  }
  return NULL;
}

} //namespace MX
