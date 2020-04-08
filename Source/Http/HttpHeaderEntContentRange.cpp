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
#include "..\..\Include\Http\HttpHeaderEntContentRange.h"
#include <intsafe.h>

//-----------------------------------------------------------

namespace MX {

CHttpHeaderEntContentRange::CHttpHeaderEntContentRange() : CHttpHeaderBase()
{
  nByteStart = nByteEnd = 0ui64;
  nTotalBytes = ULONGLONG_MAX;
  return;
}

CHttpHeaderEntContentRange::~CHttpHeaderEntContentRange()
{
  return;
}

HRESULT CHttpHeaderEntContentRange::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  ULONGLONG _nByteStart, _nByteEnd, _nTotalBytes, nTemp;
  LPCSTR szValueEndA;

  if (szValueA == NULL)
    return E_POINTER;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  szValueEndA = szValueA + nValueLen;

  _nByteStart = _nByteEnd = _nTotalBytes = 0ui64;

  //skip spaces
  szValueA = SkipSpaces(szValueA, szValueEndA);

  //check units
  if ((SIZE_T)(szValueEndA - szValueA) < 6 || StrCompareA(szValueA, "bytes", TRUE) != 0 ||
      (szValueA[5] != ' ' && szValueA[5] != '\t'))
  {
    return MX_E_Unsupported;
  }

  //skip spaces
  szValueA = SkipSpaces(szValueA + 5, szValueEndA);

  //get first byte
  if (szValueA >= szValueEndA || *szValueA < '0' || *szValueA > '9')
    return MX_E_InvalidData;
  while (szValueA < szValueEndA && *szValueA == '0')
    szValueA++;
  while (szValueA < szValueEndA && *szValueA >= '0' && *szValueA <= '9')
  {
    nTemp = _nByteStart * 10ui64;
    if (nTemp < _nByteStart)
      return MX_E_ArithmeticOverflow;
    _nByteStart = nTemp + (ULONGLONG)(*szValueA) - (ULONGLONG)'0';
    if (_nByteStart < nTemp)
      return MX_E_ArithmeticOverflow;
    szValueA++;
  }

  //skip spaces
  szValueA = SkipSpaces(szValueA, szValueEndA);

  //check separator
  if (szValueA >= szValueEndA || *szValueA != '-')
    return MX_E_InvalidData;
  szValueA++;

  //skip spaces
  szValueA = SkipSpaces(szValueA, szValueEndA);

  //get count
  if (*szValueA < '0' || *szValueA > '9')
    return MX_E_InvalidData;
  while (szValueA >= szValueEndA || *szValueA == '0')
    szValueA++;
  while (szValueA < szValueEndA && *szValueA >= '0' && *szValueA <= '9')
  {
    nTemp = _nByteEnd * 10ui64;
    if (nTemp < _nByteEnd)
      return MX_E_ArithmeticOverflow;
    _nByteEnd = nTemp + (ULONGLONG)(*szValueA) - (ULONGLONG)'0';
    if (_nByteEnd < nTemp)
      return MX_E_ArithmeticOverflow;
    szValueA++;
  }
  if (_nByteEnd < _nByteStart)
    return MX_E_InvalidData;

  //skip spaces
  szValueA = SkipSpaces(szValueA, szValueEndA);

  //check separator
  if (szValueA >= szValueEndA || *szValueA != '/')
    return MX_E_InvalidData;
  szValueA++;

  //skip spaces
  szValueA = SkipSpaces(szValueA, szValueEndA);

  //get total size
  if (szValueA < szValueEndA)
  {
    if (*szValueA != '*')
    {
      if (*szValueA < '0' || *szValueA > '9')
        return MX_E_InvalidData;

      while (szValueA < szValueEndA && *szValueA == '0')
        szValueA++;
      while (szValueA < szValueEndA && *szValueA >= '0' && *szValueA <= '9')
      {
        nTemp = _nTotalBytes * 10ui64;
        if (nTemp < _nTotalBytes)
          return MX_E_ArithmeticOverflow;
        _nTotalBytes = nTemp + (ULONGLONG)(*szValueA) - (ULONGLONG)'0';
        if (_nTotalBytes < nTemp)
          return MX_E_ArithmeticOverflow;
        szValueA++;
      }
      if (_nTotalBytes < _nByteEnd)
        return MX_E_InvalidData;
    }
    else
    {
      _nTotalBytes = ULONGLONG_MAX;
      szValueA++;
    }
  }
  else
  {
    _nTotalBytes = ULONGLONG_MAX;
  }

  //skip spaces and check for end
  if (SkipSpaces(szValueA, szValueEndA) != szValueEndA)
    return MX_E_InvalidData;

  //set new value
  nByteStart = _nByteStart;
  nByteEnd = _nByteEnd;
  nTotalBytes = _nTotalBytes;

  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentRange::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  //fill ranges
  if (cStrDestA.Format("bytes %I64u-%I64u/", nByteStart, nByteEnd) == FALSE)
    return E_OUTOFMEMORY;
  if (nTotalBytes == ULONGLONG_MAX)
  {
    if (cStrDestA.ConcatN("*", 1) == FALSE)
      return E_OUTOFMEMORY;
  }
  else
  {
    if (cStrDestA.AppendFormat("%I64u", nTotalBytes) == FALSE)
      return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentRange::SetRange(_In_ ULONGLONG _nByteStart, _In_ ULONGLONG _nByteEnd,
                                             _In_ ULONGLONG _nTotalBytes)
{
  if (_nByteStart > _nByteEnd || _nByteEnd > _nTotalBytes)
    return E_INVALIDARG;
  nByteStart = _nByteStart;
  nByteEnd = _nByteEnd;
  nTotalBytes = _nTotalBytes;
  return S_OK;
}

ULONGLONG CHttpHeaderEntContentRange::GetRangeStart() const
{
  return nByteStart;
}

ULONGLONG CHttpHeaderEntContentRange::GetRangeEnd() const
{
  return nByteEnd;
}

ULONGLONG CHttpHeaderEntContentRange::GetRangeTotal() const
{
  return nTotalBytes;
}

} //namespace MX
