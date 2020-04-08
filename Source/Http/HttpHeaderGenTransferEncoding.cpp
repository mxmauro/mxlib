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

HRESULT CHttpHeaderGenTransferEncoding::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  eEncoding _nEncoding = EncodingUnsupported;
  LPCSTR szValueEndA, szStartA;
  BOOL bGotItem;

  if (szValueA == NULL)
    return E_POINTER;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  szValueEndA = szValueA + nValueLen;

  //parse encodings
  bGotItem = FALSE;
  do
  {
    //skip spaces
    szValueA = SkipSpaces(szValueA, szValueEndA);
    if (szValueA >= szValueEndA)
      break;

    //get encoding
    szValueA = GetToken(szStartA = szValueA, szValueEndA);
    if (szValueA == szStartA)
      goto skip_null_listitem;

    if (bGotItem != FALSE)
      return MX_E_Unsupported; //only one encoding is supported
    bGotItem = TRUE;

    //check encoding
    switch ((SIZE_T)(szValueA - szStartA))
    {
      case 7:
        if (StrNCompareA(szStartA, "chunked", 7, TRUE) == 0)
          _nEncoding = EncodingChunked;
        else
          return MX_E_Unsupported;
        break;

      case 8:
        if (StrNCompareA(szStartA, "identity", 8, TRUE) == 0)
          _nEncoding = EncodingIdentity;
        else
          return MX_E_Unsupported;
        break;

      default:
        return MX_E_Unsupported;
    }

skip_null_listitem:
    //skip spaces
    szValueA = SkipSpaces(szValueA, szValueEndA);

    //check for separator or end
    if (szValueA < szValueEndA)
    {
      if (*szValueA == ',')
        szValueA++;
      else
        return MX_E_InvalidData;
    }
  }
  while (szValueA < szValueEndA);

  //do we got one?
  if (bGotItem == FALSE)
    return MX_E_InvalidData;

  //done
  nEncoding = _nEncoding;
  return S_OK;
}

HRESULT CHttpHeaderGenTransferEncoding::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
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
