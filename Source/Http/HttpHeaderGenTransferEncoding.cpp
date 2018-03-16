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
#include "..\..\Include\Http\HttpHeaderGenTransferEncoding.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderGenTransferEncoding::CHttpHeaderGenTransferEncoding() : CHttpHeaderBase()
{
  nEncoding = EncodingUnsupported;
  return;
}

CHttpHeaderGenTransferEncoding::~CHttpHeaderGenTransferEncoding()
{
  return;
}

HRESULT CHttpHeaderGenTransferEncoding::Parse(_In_z_ LPCSTR szValueA)
{
  eEncoding _nEncoding = EncodingUnsupported;

  if (szValueA == NULL)
    return E_POINTER;
  nEncoding = EncodingUnsupported;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //check Encoding
  if (StrNCompareA(szValueA, "identity", 8, TRUE) == 0)
  {
    _nEncoding = EncodingIdentity;
    szValueA += 8;
  }
  else if (StrNCompareA(szValueA, "chunked", 7, TRUE) == 0)
  {
    _nEncoding = EncodingChunked;
    szValueA += 7;
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

HRESULT CHttpHeaderGenTransferEncoding::Build(_Inout_ CStringA &cStrDestA)
{
  switch (nEncoding)
  {
    case EncodingIdentity:
      if (cStrDestA.Copy("identity") == FALSE)
        return E_OUTOFMEMORY;
      return S_OK;
    case EncodingChunked:
      if (cStrDestA.Copy("chunked") == FALSE)
        return E_OUTOFMEMORY;
      return S_OK;
  }
  cStrDestA.Empty();
  return MX_E_Unsupported;
}

HRESULT CHttpHeaderGenTransferEncoding::SetEncoding(_In_ eEncoding _nEncoding)
{
  if (_nEncoding != EncodingChunked && _nEncoding != EncodingIdentity && _nEncoding != EncodingUnsupported)
    return E_INVALIDARG;
  nEncoding = _nEncoding;
  return S_OK;
}

CHttpHeaderGenTransferEncoding::eEncoding CHttpHeaderGenTransferEncoding::GetEncoding() const
{
  return nEncoding;
}

} //namespace MX
