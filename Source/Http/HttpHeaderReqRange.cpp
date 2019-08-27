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

HRESULT CHttpHeaderReqRange::Parse(_In_z_ LPCSTR szValueA)
{
  RANGESET sRangeSet;
  ULONGLONG nTemp;

  if (szValueA == NULL)
    return E_POINTER;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //check units
  if (StrNCompareA(szValueA, "bytes", 5, TRUE) != 0)
    return MX_E_InvalidData;
  //skip spaces
  szValueA = SkipSpaces(szValueA + 5);
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

HRESULT CHttpHeaderReqRange::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  SIZE_T i, nCount;

  cStrDestA.Empty();
  nCount = cRangeSetsList.GetCount();
  if (nCount == 0)
    return S_OK;
  //fill ranges
  if (cStrDestA.CopyN("bytes", 5) == FALSE)
    return E_OUTOFMEMORY;
  for (i = 0; i < nCount; i++)
  {
    if (cStrDestA.AppendFormat("%c%I64u-%I64u", ((i == 0) ? '=' : ','), cRangeSetsList[i].nByteStart,
                               cRangeSetsList[i].nByteEnd) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderReqRange::AddRangeSet(_In_ ULONGLONG nByteStart, _In_ ULONGLONG nByteEnd)
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

ULONGLONG CHttpHeaderReqRange::GetRangeSetStart(_In_ SIZE_T nIndex) const
{
  return (nIndex < cRangeSetsList.GetCount()) ? cRangeSetsList[nIndex].nByteStart : ULONGLONG_MAX;
}

ULONGLONG CHttpHeaderReqRange::GetRangeSetEnd(_In_ SIZE_T nIndex) const
{
  return (nIndex < cRangeSetsList.GetCount()) ? cRangeSetsList[nIndex].nByteEnd : 0ui64;
}

} //namespace MX
