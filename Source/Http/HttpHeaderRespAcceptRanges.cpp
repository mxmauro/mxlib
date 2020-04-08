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

HRESULT CHttpHeaderRespAcceptRanges::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  eRange _nRange = RangeUnsupported;
  LPCSTR szValueEndA, szStartA;
  BOOL bGotItem = FALSE;

  if (szValueA == NULL)
    return E_POINTER;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  szValueEndA = szValueA + nValueLen;

  //parse range type
  bGotItem = FALSE;
  do
  {
    //skip spaces
    szValueA = SkipSpaces(szValueA, szValueEndA);
    if (szValueA >= szValueEndA)
      break;

    //get range type
    szValueA = GetToken(szStartA = szValueA, szValueEndA);
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
  nRange = _nRange;
  return S_OK;
}

HRESULT CHttpHeaderRespAcceptRanges::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
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

  //done
  nRange = _nRange;
  return S_OK;
}

CHttpHeaderRespAcceptRanges::eRange CHttpHeaderRespAcceptRanges::GetRange() const
{
  return nRange;
}

} //namespace MX
