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
#include "..\..\Include\Http\HttpHeaderReqAccept.h"
#include <stdlib.h>

//-----------------------------------------------------------

//NOTE: UNDOCUMENTED!!!
extern "C" {
  extern _locale_tstruct __initiallocalestructinfo;
};

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqAccept::CHttpHeaderReqAccept() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderReqAccept::~CHttpHeaderReqAccept()
{
  cTypesList.RemoveAllElements();
  return;
}

HRESULT CHttpHeaderReqAccept::Parse(__in_z LPCSTR szValueA)
{
  CType *lpType;
  LPCSTR szStartA;
  CStringA cStrTempA, cStrTempA_2;
  CStringW cStrTempW;
  BOOL bGotItem;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //parse
  bGotItem = FALSE;
  do
  {
    //skip spaces
    szValueA = SkipSpaces(szValueA);
    if (*szValueA == 0)
      break;
    bGotItem = TRUE;
    //mime type
    szValueA = Advance(szStartA = szValueA, ";, \t");
    if (szValueA == szStartA)
      return MX_E_InvalidData;
    if (cStrTempA.CopyN(szStartA, (SIZE_T)(szValueA-szStartA)) == FALSE)
      return E_OUTOFMEMORY;
    hRes = AddType((LPCSTR)cStrTempA, &lpType);
    if (FAILED(hRes))
      return hRes;
    //skip spaces
    szValueA = SkipSpaces(szValueA);
    //parameters
    if (*szValueA == ';')
    {
      szValueA++;
      do
      {
        //skip spaces
        szValueA = SkipSpaces(szValueA);
        if (*szValueA == 0 || *szValueA == ',')
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
        if (StrCompareA((LPSTR)cStrTempA, "q", TRUE) == 0)
        {
          hRes = lpType->SetQ(_atof_l((LPSTR)cStrTempA_2, &__initiallocalestructinfo));
        }
        else
        {
          hRes = Utf8_Decode(cStrTempW, (LPSTR)cStrTempA_2);
          if (SUCCEEDED(hRes))
            hRes = lpType->AddParam((LPSTR)cStrTempA, (LPWSTR)cStrTempW);
        }
        if (FAILED(hRes))
          return hRes;
        //skip spaces
        szValueA = SkipSpaces(szValueA);
        //check for separator or end
        if (*szValueA == ';')
          szValueA++;
        else if (*szValueA != 0 && *szValueA != ',')
          return MX_E_InvalidData;
      }
      while (*szValueA != 0 && *szValueA != ',');
    }
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
  return (bGotItem != FALSE) ? S_OK : MX_E_InvalidData;
}

HRESULT CHttpHeaderReqAccept::Build(__inout CStringA &cStrDestA)
{
  SIZE_T i, nCount, nParamIdx, nParamsCount;
  CType *lpType;
  LPCWSTR sW;
  CStringA cStrTempA;
  HRESULT hRes;

  cStrDestA.Empty();
  nCount = cTypesList.GetCount();
  for (i=0; i<nCount; i++)
  {
    lpType = cTypesList.GetElementAt(i);
    if (cStrDestA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(",", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrDestA.Concat(lpType->GetType()) == FALSE)
      return E_OUTOFMEMORY;
    //q
    if (lpType->GetQ() < 1.0)
    {
      if (cStrDestA.AppendFormat(";q=%f", lpType->GetQ()) == FALSE)
        return E_OUTOFMEMORY;
    }
    //parameters
    nParamsCount = lpType->GetParamsCount();
    for (nParamIdx=0; nParamIdx<nParamsCount; nParamIdx++)
    {
      if (cStrDestA.AppendFormat(";%s=", lpType->GetParamName(nParamIdx)) == FALSE)
        return E_OUTOFMEMORY;
      sW = lpType->GetParamValue(nParamIdx);
      while (*sW != 0)
      {
        if (*sW < 33 || *sW > 126 || CHttpCommon::IsValidNameChar((CHAR)(*sW)) == FALSE)
          break;
        sW++;
      }
      if (*sW == 0)
      {
        if (cStrDestA.AppendFormat("%S", lpType->GetParamValue(nParamIdx)) == FALSE)
          return E_OUTOFMEMORY;
      }
      else
      {
        hRes = Utf8_Encode(cStrTempA, lpType->GetParamValue(nParamIdx));
        if (FAILED(hRes))
          return hRes;
        if (CHttpCommon::EncodeQuotedString(cStrTempA) == FALSE)
          return E_OUTOFMEMORY;
        if (cStrDestA.AppendFormat("\"%s\"", (LPCSTR)cStrTempA) == FALSE)
          return E_OUTOFMEMORY;
      }
    }
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderReqAccept::AddType(__in_z LPCSTR szTypeA, __out_opt CType **lplpType)
{
  TAutoDeletePtr<CType> cNewType;
  SIZE_T i, nCount;
  HRESULT hRes;

  if (lplpType != NULL)
    *lplpType = NULL;
  if (szTypeA == NULL)
    return E_POINTER;
  //create new type
  cNewType.Attach(MX_DEBUG_NEW CType());
  if (!cNewType)
    return E_OUTOFMEMORY;
  hRes = cNewType->SetType(szTypeA);
  if (FAILED(hRes))
    return hRes;
  //check if already exists in list
  nCount = cTypesList.GetCount();
  for (i=0; i<nCount; i++)
  {
    if (StrCompareA(cTypesList[i]->GetType(), cNewType->GetType()) == 0)
      return MX_E_AlreadyExists;
  }
  //add to list
  if (cTypesList.AddElement(cNewType.Get()) == FALSE)
    return E_OUTOFMEMORY;
  //done
  if (lplpType != NULL)
    *lplpType = cNewType.Detach();
  else
    cNewType.Detach();
  return S_OK;
}

SIZE_T CHttpHeaderReqAccept::GetTypesCount() const
{
  return cTypesList.GetCount();
}

CHttpHeaderReqAccept::CType* CHttpHeaderReqAccept::GetType(__in SIZE_T nIndex) const
{
  return (nIndex < cTypesList.GetCount()) ? cTypesList.GetElementAt(nIndex) : NULL;
}

CHttpHeaderReqAccept::CType* CHttpHeaderReqAccept::GetType(__in_z LPCSTR szTypeA) const
{
  SIZE_T i, nCount;

  if (szTypeA != NULL && szTypeA[0] != 0)
  {
    nCount = cTypesList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (StrCompareA(cTypesList[i]->GetType(), szTypeA, TRUE) == 0)
        return cTypesList.GetElementAt(i);
    }
  }
  return NULL;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CHttpHeaderReqAccept::CType::CType() : CBaseMemObj()
{
  q = 1.0;
  return;
}

CHttpHeaderReqAccept::CType::~CType()
{
  cParamsList.RemoveAllElements();
  return;
}

HRESULT CHttpHeaderReqAccept::CType::SetType(__in_z LPCSTR szTypeA)
{
  LPCSTR szStartA, szEndA;

  if (szTypeA == NULL)
    return E_POINTER;
  if (*szTypeA == 0 || *szTypeA == '/')
    return MX_E_InvalidData;
  //skip spaces
  szTypeA = CHttpHeaderBase::SkipSpaces(szTypeA);
  //mark start
  szStartA = szTypeA;
  //get mime type
  if (szTypeA[0] != '*')
    szTypeA = CHttpHeaderBase::GetToken(szTypeA, "/");
  else
    szTypeA++;
  //check slash separator
  if (*szTypeA++ != '/')
    return MX_E_InvalidData;
  if (szTypeA[0] != '*')
  {
    szTypeA = (szTypeA[0] != '*') ? CHttpHeaderBase::GetToken(szTypeA, "/ \t") : szTypeA++;
    if (*(szTypeA-1) == '/' || *szTypeA == '/') //no subtype?
      return MX_E_InvalidData;
  }
  else
  {
    szTypeA++;
  }
  szEndA = szTypeA;
  //skip spaces and check for end
  if (*CHttpHeaderBase::SkipSpaces(szTypeA) != 0)
    return MX_E_InvalidData;
  //set new value
  if (cStrTypeA.CopyN(szStartA, (SIZE_T)(szEndA-szStartA)) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCSTR CHttpHeaderReqAccept::CType::GetType() const
{
  return (LPCSTR)cStrTypeA;
}

HRESULT CHttpHeaderReqAccept::CType::SetQ(__in double _q)
{
  if (_q < -0.000001 || _q > 1.000001)
    return E_INVALIDARG;
  q = (_q > 0.000001) ? ((_q < 0.99999) ? _q : 1.0) : 0.0;
  return S_OK;
}

double CHttpHeaderReqAccept::CType::GetQ() const
{
  return q;
}

HRESULT CHttpHeaderReqAccept::CType::AddParam(__in_z LPCSTR szNameA, __in_z LPCWSTR szValueW)
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

SIZE_T CHttpHeaderReqAccept::CType::GetParamsCount() const
{
  return cParamsList.GetCount();
}

LPCSTR CHttpHeaderReqAccept::CType::GetParamName(__in SIZE_T nIndex) const
{
  return (nIndex < cParamsList.GetCount()) ? cParamsList[nIndex]->szNameA : NULL;
}

LPCWSTR CHttpHeaderReqAccept::CType::GetParamValue(__in SIZE_T nIndex) const
{
  return (nIndex < cParamsList.GetCount()) ? cParamsList[nIndex]->szValueW : NULL;
}

LPCWSTR CHttpHeaderReqAccept::CType::GetParamValue(__in_z LPCSTR szNameA) const
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
