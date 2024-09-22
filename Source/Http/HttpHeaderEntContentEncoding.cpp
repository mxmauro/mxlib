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
#include "..\..\Include\Http\HttpHeaderEntContentEncoding.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderEntContentEncoding::CHttpHeaderEntContentEncoding() : CHttpHeaderBase()
{
  nEncoding = eEncoding::Unsupported;
  return;
}

CHttpHeaderEntContentEncoding::~CHttpHeaderEntContentEncoding()
{
  return;
}

HRESULT CHttpHeaderEntContentEncoding::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  eEncoding _nEncoding = eEncoding::Unsupported;
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
      case 4:
        if (StrNCompareA(szStartA, "gzip", 4, TRUE) == 0)
          _nEncoding = eEncoding::GZip;
        else if (StrNCompareA(szStartA, "7bit", 4, TRUE) != 0 && StrNCompareA(szStartA, "8bit", 4, TRUE) != 0)
          return MX_E_Unsupported;
        break;

      case 6:
        if (StrNCompareA(szStartA, "x-gzip", 6, TRUE) == 0)
          _nEncoding = eEncoding::GZip;
        else if (StrNCompareA(szStartA, "binary", 6, TRUE) != 0)
          return MX_E_Unsupported;
        break;

      case 7:
        if (StrNCompareA(szStartA, "deflate", 7, TRUE) == 0)
          _nEncoding = eEncoding::Deflate;
        else
          return MX_E_Unsupported;
        break;

      case 8:
        if (StrNCompareA(szStartA, "identity", 8, TRUE) != 0)
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

HRESULT CHttpHeaderEntContentEncoding::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
{
  switch (nEncoding)
  {
    case eEncoding::Identity:
      if (cStrDestA.Copy("identity") == FALSE)
        return E_OUTOFMEMORY;
      return S_OK;

    case eEncoding::GZip:
      if (cStrDestA.Copy("gzip") == FALSE)
        return E_OUTOFMEMORY;
      return S_OK;

    case eEncoding::Deflate:
      if (cStrDestA.Copy("deflate") == FALSE)
        return E_OUTOFMEMORY;
      return S_OK;
  }

  cStrDestA.Empty();
  return MX_E_Unsupported;
}

HRESULT CHttpHeaderEntContentEncoding::SetEncoding(_In_ eEncoding _nEncoding)
{
  if (_nEncoding != eEncoding::Identity && _nEncoding != eEncoding::GZip &&
      _nEncoding != eEncoding::Deflate && _nEncoding != eEncoding::Unsupported)
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
