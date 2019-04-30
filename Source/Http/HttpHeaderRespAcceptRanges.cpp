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
#include "..\..\Include\Http\HttpHeaderRespAcceptRanges.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderRespAcceptRanges::CHttpHeaderRespAcceptRanges() : CHttpHeaderBase()
{
  nRange = RangeUnsupported;
  return;
}

CHttpHeaderRespAcceptRanges::~CHttpHeaderRespAcceptRanges()
{
  return;
}

HRESULT CHttpHeaderRespAcceptRanges::Parse(_In_z_ LPCSTR szValueA)
{
  eRange _nRange = RangeUnsupported;
  LPCSTR szStartA;
  BOOL bGotItem = FALSE;

  if (szValueA == NULL)
    return E_POINTER;
  //parse range type
  bGotItem = FALSE;
  do
  {
    //skip spaces
    szValueA = SkipSpaces(szValueA);
    if (*szValueA == 0)
      break;

    //get range type
    szValueA = GetToken(szStartA = szValueA);
    if (szValueA == szStartA)
      goto skip_null_listitem;

    if (bGotItem != FALSE)
      return MX_E_Unsupported; //only one range type is supported
    bGotItem = TRUE;

    //check range type
    switch ((SIZE_T)(szValueA - szStartA))
    {
      case 4:
        if (StrNCompareA(szStartA, "none", 4, TRUE) == 0)
          _nRange = RangeNone;
        else
          return MX_E_Unsupported;
        break;

      case 5:
        if (StrNCompareA(szStartA, "bytes", 5, TRUE) == 0)
          _nRange = RangeBytes;
        else
          return MX_E_Unsupported;
        break;

      default:
        return MX_E_Unsupported;
    }

skip_null_listitem:
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
  if (bGotItem == FALSE)
    return MX_E_InvalidData;
  nRange = _nRange;
  return S_OK;
}

HRESULT CHttpHeaderRespAcceptRanges::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  switch (nRange)
  {
    case RangeNone:
      if (cStrDestA.Copy("none") == FALSE)
        return E_OUTOFMEMORY;
      return S_OK;

    case RangeBytes:
      if (cStrDestA.Copy("bytes") == FALSE)
        return E_OUTOFMEMORY;
      return S_OK;
  }
  cStrDestA.Empty();
  return MX_E_Unsupported;
}

HRESULT CHttpHeaderRespAcceptRanges::SetRange(_In_ eRange _nRange)
{
  if (_nRange != RangeNone && _nRange != RangeBytes && _nRange != RangeUnsupported)
    return E_INVALIDARG;
  nRange = _nRange;
  return S_OK;
}

CHttpHeaderRespAcceptRanges::eRange CHttpHeaderRespAcceptRanges::GetRange() const
{
  return nRange;
}

} //namespace MX
