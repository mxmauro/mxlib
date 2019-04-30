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
#include "..\..\Include\Http\HttpHeaderRespETag.h"
#include <wtypes.h>

//-----------------------------------------------------------

namespace MX {

CHttpHeaderRespETag::CHttpHeaderRespETag() : CHttpHeaderBase()
{
  bIsWeak = FALSE;
  return;
}

CHttpHeaderRespETag::~CHttpHeaderRespETag()
{
  return;
}

HRESULT CHttpHeaderRespETag::Parse(_In_z_ LPCSTR szValueA)
{
  LPCSTR szStartA;
  CStringA cStrTempA;
  BOOL _bIsWeak;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //is weak?
  _bIsWeak = FALSE;
  if (szValueA[0] == 'W' && szValueA[1] == '/')
  {
    szValueA += 2;
    _bIsWeak = TRUE;
  }
  //get entity
  if (*szValueA != '"')
  {
    szValueA = SkipUntil(szStartA = szValueA, " \t");
    if (szValueA == szStartA)
      return MX_E_InvalidData;
    if (cStrTempA.CopyN(szStartA, (SIZE_T)(szValueA - szStartA)) == FALSE)
      return E_OUTOFMEMORY;
  }
  else
  {
    hRes = GetQuotedString(cStrTempA, szValueA);
    if (FAILED(hRes))
      return hRes;
    if (cStrTempA.IsEmpty() != FALSE)
      return MX_E_InvalidData;
  }
  //skip spaces and check for end
  if (*SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;
  //set entity name
  hRes = SetTag((LPCSTR)cStrTempA, cStrTempA.GetLength());
  if (FAILED(hRes))
    return hRes;
  //set entity weak
  SetWeak(_bIsWeak);
  //done
  return S_OK;
}

HRESULT CHttpHeaderRespETag::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  CStringA cStrTempA;

  cStrDestA.Empty();
  if (cStrTagA.IsEmpty() == FALSE)
  {
    //weak
    if (bIsWeak != FALSE)
    {
      if (cStrDestA.ConcatN("W/", 2) == FALSE)
        return E_OUTOFMEMORY;
    }
    //entity
    if (CHttpCommon::BuildQuotedString(cStrTempA, (LPCSTR)cStrTagA, cStrTagA.GetLength(), FALSE) == FALSE ||
        cStrDestA.ConcatN((LPCSTR)cStrTempA, cStrTempA.GetLength()) == FALSE)
    {
      return E_OUTOFMEMORY;
    }
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderRespETag::SetTag(_In_z_ LPCSTR szTagA, _In_ SIZE_T nTagLen)
{
  SIZE_T i;

  if (nTagLen == (SIZE_T)-1)
    nTagLen = StrLenA(szTagA);
  if (nTagLen == 0)
    return MX_E_InvalidData;
  if (szTagA == NULL)
    return E_POINTER;
  //check for invalid characters
  for (i = 0; i < nTagLen; i++)
  {
    if (((UCHAR)szTagA[i]) < 0x20 || ((UCHAR)szTagA[i]) > 0x7E || szTagA[i] == '"')
      return MX_E_InvalidData;
  }
  //check completed
  if (cStrTagA.CopyN(szTagA, nTagLen) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCSTR CHttpHeaderRespETag::GetTag() const
{
  return (LPCSTR)cStrTagA;
}

HRESULT CHttpHeaderRespETag::SetWeak(_In_ BOOL _bIsWeak)
{
  bIsWeak = _bIsWeak;
  return S_OK;
}

BOOL CHttpHeaderRespETag::GetWeak() const
{
  return bIsWeak;
}

} //namespace MX
