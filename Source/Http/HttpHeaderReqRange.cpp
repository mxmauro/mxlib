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
#include "..\..\Include\Http\HttpHeaderReqRange.h"
#include <intsafe.h>

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqRange::CHttpHeaderReqRange() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderReqRange::~CHttpHeaderReqRange()
{
  cRangeSetsList.RemoveAllElements();
  return;
}

HRESULT CHttpHeaderReqRange::Parse(__in_z LPCSTR szValueA)
{
  RANGESET sRangeSet;
  ULONGLONG nTemp;

  if (szValueA == NULL)
    return E_POINTER;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //check units
  if (StrCompareA(szValueA, "bytes", TRUE) != 0)
    return MX_E_InvalidData;
  //skip spaces
  szValueA = SkipSpaces(szValueA+5);
  //check equal sign
  if (*szValueA++ != '=')
    return MX_E_InvalidData;
  //parse sets
  do
  {
    //skip spaces
    szValueA = SkipSpaces(szValueA);
    if (*szValueA == 0)
      break;
    sRangeSet.nByteStart = sRangeSet.nByteEnd = 0ui64;
    //skip spaces
    szValueA = SkipSpaces(szValueA);
    //get starting byte
    if (*szValueA != '-')
    {
      if (*szValueA < '0' || *szValueA > '9')
        return MX_E_InvalidData;
      while (*szValueA >= '0' && *szValueA <= '9')
      {
        nTemp = sRangeSet.nByteStart * 10ui64;
        if (nTemp < sRangeSet.nByteStart)
          return MX_E_ArithmeticOverflow;
        sRangeSet.nByteStart = nTemp + (ULONGLONG)(*szValueA - '0');
        if (sRangeSet.nByteStart < nTemp)
          return MX_E_ArithmeticOverflow;
        szValueA++;
      }
      //skip spaces
      szValueA = SkipSpaces(szValueA);
    }
    //check range separator
    if (*szValueA++ != '-')
      return MX_E_InvalidData;
    //skip spaces
    szValueA = SkipSpaces(szValueA);
    //get ending byte
    if (*szValueA >= '0' && *szValueA <= '9')
    {
      while (*szValueA >= '0' && *szValueA <= '9')
      {
        nTemp = sRangeSet.nByteEnd * 10ui64;
        if (nTemp < sRangeSet.nByteEnd)
          return MX_E_ArithmeticOverflow;
        sRangeSet.nByteEnd = nTemp + (ULONGLONG)(*szValueA - '0');
        if (sRangeSet.nByteEnd < nTemp)
          return MX_E_ArithmeticOverflow;
        szValueA++;
      }
    }
    else
    {
      sRangeSet.nByteEnd = ULONGLONG_MAX;
    }
    //check values
    if (sRangeSet.nByteStart > sRangeSet.nByteEnd)
      return MX_E_InvalidData;
    //add range
    if (cRangeSetsList.AddElement(&sRangeSet) == FALSE)
      return E_OUTOFMEMORY;
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
  return S_OK;
}

HRESULT CHttpHeaderReqRange::Build(__inout CStringA &cStrDestA)
{
  SIZE_T i, nCount;

  cStrDestA.Empty();
  nCount = cRangeSetsList.GetCount();
  if (nCount == 0)
    return S_OK;
  //fill ranges
  if (cStrDestA.CopyN("bytes", 5) == FALSE)
    return E_OUTOFMEMORY;
  for (i=0; i<nCount; i++)
  {
    if (cStrDestA.AppendFormat("%c%I64u-%I64u", ((i == 0) ? '=' : ','), cRangeSetsList[i].nByteStart,
                               cRangeSetsList[i].nByteEnd) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderReqRange::AddRangeSet(__in ULONGLONG nByteStart, __in ULONGLONG nByteEnd)
{
  RANGESET sNewRangeSet;

  if (nByteStart > nByteEnd)
    return E_INVALIDARG;
  sNewRangeSet.nByteStart = nByteStart;
  sNewRangeSet.nByteEnd = nByteEnd;
  if (cRangeSetsList.AddElement(&sNewRangeSet) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

SIZE_T CHttpHeaderReqRange::GetRangeSetsCount() const
{
  return cRangeSetsList.GetCount();
}

ULONGLONG CHttpHeaderReqRange::GetRangeSetStart(__in SIZE_T nIndex) const
{
  return (nIndex < cRangeSetsList.GetCount()) ? cRangeSetsList[nIndex].nByteStart : ULONGLONG_MAX;
}

ULONGLONG CHttpHeaderReqRange::GetRangeSetEnd(__in SIZE_T nIndex) const
{
  return (nIndex < cRangeSetsList.GetCount()) ? cRangeSetsList[nIndex].nByteEnd : 0ui64;
}

} //namespace MX
