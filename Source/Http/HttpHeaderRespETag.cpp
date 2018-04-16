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
  LPCSTR szStartA, szEndA;
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
    szValueA = Advance(szStartA = szValueA, " \t");
    if (szValueA == szStartA)
      return MX_E_InvalidData;
    szEndA = szValueA;
  }
  else
  {
    szStartA = ++szValueA;
    while (*szValueA != 0 && *szValueA != '"')
    {
      if (*szValueA == L'\\' && szValueA[1] != 0)
        szValueA++;
      szValueA++;
    }
    if (*szValueA != '"')
      return MX_E_InvalidData;
    szEndA = szValueA++;
  }
  //skip spaces and check for end
  if (*SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;
  //set entity name
  if (cStrTempA.CopyN(szStartA, (SIZE_T)(szEndA-szStartA)) == FALSE)
    return E_OUTOFMEMORY;
  hRes = SetEntity((LPSTR)cStrTempA);
  if (FAILED(hRes))
    return hRes;
  //set entity weak
  SetWeak(_bIsWeak);
  //done
  return S_OK;
}

HRESULT CHttpHeaderRespETag::Build(_Inout_ CStringA &cStrDestA)
{
  CStringA cStrTempA;
  HRESULT hRes;

  cStrDestA.Empty();
  if (cStrEntityW.IsEmpty() == FALSE)
  {
    //weak
    if (bIsWeak != FALSE)
    {
      if (cStrDestA.ConcatN("W/", 2) == FALSE)
        return E_OUTOFMEMORY;
    }
    //entity
    hRes = Utf8_Encode(cStrTempA, cStrEntityW);
    if (FAILED(hRes))
      return hRes;
    if (CHttpCommon::EncodeQuotedString(cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
    if (cStrDestA.AppendFormat("\"%s\"", (LPCSTR)cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderRespETag::SetEntity(_In_z_ LPCSTR szEntityA)
{
  MX::CStringW cStrTempW;
  HRESULT hRes;

  if (szEntityA == NULL)
    return E_POINTER;
  hRes = Utf8_Decode(cStrTempW, szEntityA);
  if (FAILED(hRes))
    return hRes;
  //done
  return SetEntity((LPCWSTR)cStrTempW);
}

HRESULT CHttpHeaderRespETag::SetEntity(_In_z_ LPCWSTR szEntityW)
{
  LPCWSTR sW;

  if (szEntityW == NULL)
    return E_POINTER;
  //check for invalid characters
  for (sW=szEntityW; *sW!=0; sW++)
  {
    if (*sW < 32 || *sW == L'"')
      return MX_E_InvalidData;
  }
  //check completed
  if (cStrEntityW.Copy(szEntityW) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCWSTR CHttpHeaderRespETag::GetEntity() const
{
  return (LPCWSTR)cStrEntityW;
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
