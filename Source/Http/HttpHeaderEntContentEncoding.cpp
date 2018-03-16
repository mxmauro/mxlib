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
#include "..\..\Include\Http\HttpHeaderEntContentEncoding.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderEntContentEncoding::CHttpHeaderEntContentEncoding() : CHttpHeaderBase()
{
  nEncoding = EncodingUnsupported;
  return;
}

CHttpHeaderEntContentEncoding::~CHttpHeaderEntContentEncoding()
{
  return;
}

HRESULT CHttpHeaderEntContentEncoding::Parse(_In_z_ LPCSTR szValueA)
{
  eEncoding _nEncoding = EncodingUnsupported;

  if (szValueA == NULL)
    return E_POINTER;
  nEncoding = EncodingUnsupported;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //check encoding
  if (StrNCompareA(szValueA, "x-gzip", 6, TRUE) == 0)
  {
    _nEncoding = EncodingGZip;
    szValueA += 6;
  }
  else if (StrNCompareA(szValueA, "gzip", 4, TRUE) == 0)
  {
    _nEncoding = EncodingGZip;
    szValueA += 4;
  }
  else if (StrNCompareA(szValueA, "deflate", 7, TRUE) == 0)
  {
    _nEncoding = EncodingDeflate;
    szValueA += 7;
  }
  else if (StrNCompareA(szValueA, "7bit", 4, TRUE) == 0 || StrNCompareA(szValueA, "8bit", 4, TRUE) == 0)
  {
    szValueA += 4;
  }
  else if (StrNCompareA(szValueA, "binary", 6, TRUE) == 0)
  {
    szValueA += 6;
  }
  else if (StrNCompareA(szValueA, "identity", 8, TRUE) == 0)
  {
    szValueA += 8;
  }
  else
  {
    return MX_E_Unsupported;
  }
  //skip spaces and check for end
  if (*SkipSpaces(szValueA) != 0)
    return MX_E_InvalidData;
  //done
  nEncoding = _nEncoding;
  return S_OK;
}

HRESULT CHttpHeaderEntContentEncoding::Build(_Inout_ CStringA &cStrDestA)
{
  switch (nEncoding)
  {
    case EncodingIdentity:
      if (cStrDestA.Copy("identity") == FALSE)
        return E_OUTOFMEMORY;
      return S_OK;
    case EncodingGZip:
      if (cStrDestA.Copy("gzip") == FALSE)
        return E_OUTOFMEMORY;
      return S_OK;
    case EncodingDeflate:
      if (cStrDestA.Copy("deflate") == FALSE)
        return E_OUTOFMEMORY;
      return S_OK;
  }
  cStrDestA.Empty();
  return MX_E_Unsupported;
}

HRESULT CHttpHeaderEntContentEncoding::SetEncoding(_In_ eEncoding _nEncoding)
{
  if (_nEncoding != EncodingIdentity && _nEncoding != EncodingGZip &&
      _nEncoding != EncodingDeflate && _nEncoding != EncodingUnsupported)
  {
    return E_INVALIDARG;
  }
  nEncoding = _nEncoding;
  return S_OK;
}

CHttpHeaderEntContentEncoding::eEncoding CHttpHeaderEntContentEncoding::GetEncoding() const
{
  return nEncoding;
}

} //namespace MX
