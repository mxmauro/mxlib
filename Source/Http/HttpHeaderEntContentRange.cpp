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

HRESULT CHttpHeaderEntContentRange::Parse(_In_z_ LPCSTR szValueA)
{
  ULONGLONG _nByteStart, _nByteEnd, _nTotalBytes, nTemp;

  if (szValueA == NULL)
    return E_POINTER;
  _nByteStart = _nByteEnd = _nTotalBytes = 0ui64;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //check units
  if (StrCompareA(szValueA, "bytes", TRUE) != 0 || (szValueA[5] != ' ' && szValueA[5] != '\t'))
    return MX_E_Unsupported;
  //skip spaces
  szValueA = SkipSpaces(szValueA + 5);
  //get first byte
  if (*szValueA < '0' || *szValueA > '9')
    return MX_E_InvalidData;
  while (*szValueA == '0')
    szValueA++;
  while (*szValueA >= '0' && *szValueA <= '9')
  {
    nTemp = _nByteStart * 10ui64;
    if (nTemp < _nByteStart)
      return MX_E_ArithmeticOverflow;
    _nByteStart = nTemp + (ULONGLONG)(*szValueA - '0');
    if (_nByteStart < nTemp)
      return MX_E_ArithmeticOverflow;
    szValueA++;
  }
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //check separator
  if (*szValueA++ != '-')
    return MX_E_InvalidData;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //get count
  if (*szValueA < '0' || *szValueA > '9')
    return MX_E_InvalidData;
  while (*szValueA == '0')
    szValueA++;
  while (*szValueA >= '0' && *szValueA <= '9')
  {
    nTemp = _nByteEnd * 10ui64;
    if (nTemp < _nByteEnd)
      return MX_E_ArithmeticOverflow;
    _nByteEnd = nTemp + (ULONGLONG)(*szValueA - '0');
    if (_nByteEnd < nTemp)
      return MX_E_ArithmeticOverflow;
    szValueA++;
  }
  if (_nByteEnd < _nByteStart)
    return MX_E_InvalidData;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //check separator
  if (*szValueA++ != '/')
    return MX_E_InvalidData;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //get total size
  if (*szValueA != '*')
  {
    if (*szValueA < '0' || *szValueA > '9')
      return MX_E_InvalidData;
    while (*szValueA == '0')
      szValueA++;
    while (*szValueA >= '0' && *szValueA <= '9')
    {
      nTemp = _nTotalBytes * 10ui64;
      if (nTemp < _nTotalBytes)
        return MX_E_ArithmeticOverflow;
      _nTotalBytes = nTemp + (ULONGLONG)(*szValueA - '0');
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
  //skip spaces and check for end
  if (*SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;
  //set new value
  nByteStart = _nByteStart;
  nByteEnd = _nByteEnd;
  nTotalBytes = _nTotalBytes;
  //done
  return S_OK;
}

HRESULT CHttpHeaderEntContentRange::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
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
