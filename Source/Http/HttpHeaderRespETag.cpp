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

HRESULT CHttpHeaderRespETag::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  LPCSTR szValueEndA, szStartA;
  CStringA cStrTempA;
  BOOL _bIsWeak;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  szValueEndA = szValueA + nValueLen;

  //skip spaces
  szValueA = SkipSpaces(szValueA, szValueEndA);

  //is weak?
  _bIsWeak = FALSE;
  if ((SIZE_T)(szValueEndA - szValueA) >= 2 && szValueA[0] == 'W' && szValueA[1] == '/')
  {
    szValueA += 2;
    _bIsWeak = TRUE;
  }

  //get entity
  if (szValueA < szValueEndA && *szValueA != '"')
  {
    szValueA = SkipUntil(szStartA = szValueA, szValueEndA, " \t");
    if (szValueA == szStartA)
      return MX_E_InvalidData;
    if (cStrTempA.CopyN(szStartA, (SIZE_T)(szValueA - szStartA)) == FALSE)
      return E_OUTOFMEMORY;
  }
  else
  {
    hRes = GetQuotedString(cStrTempA, szValueA, szValueEndA);
    if (FAILED(hRes))
      return hRes;
    if (cStrTempA.IsEmpty() != FALSE)
      return MX_E_InvalidData;
  }

  //skip spaces and check for end
  if (SkipSpaces(szValueA, szValueEndA) != szValueEndA)
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

HRESULT CHttpHeaderRespETag::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
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
    if (Http::BuildQuotedString(cStrTempA, (LPCSTR)cStrTagA, cStrTagA.GetLength(), FALSE) == FALSE ||
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
