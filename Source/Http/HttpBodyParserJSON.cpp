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
#include "..\..\Include\Http\HttpBodyParserJSON.h"
#include "..\..\Include\AutoPtr.h"

//-----------------------------------------------------------

namespace MX {

CHttpBodyParserJSON::CHttpBodyParserJSON() : CHttpBodyParserBase()
{
  nState = StateData;
  return;
}

CHttpBodyParserJSON::~CHttpBodyParserJSON()
{
  return;
}

HRESULT CHttpBodyParserJSON::Initialize(_In_ MX::Internals::CHttpParser &cHttpParser)
{
  //done
  return S_OK;
}

HRESULT CHttpBodyParserJSON::Parse(_In_opt_ LPCVOID lpData, _In_opt_ SIZE_T nDataSize)
{
  if (lpData == NULL && nDataSize > 0)
    return E_POINTER;

  //end of parsing?
  if (lpData != NULL)
  {
    if (cStrTempA.ConcatN((LPCSTR)lpData, nDataSize) != FALSE)
      return S_OK;
    nState = StateError;
    return E_OUTOFMEMORY;
  }

  if (nState == StateDone)
  {
    nState = StateError;
    return MX_E_InvalidData;
  }

  try
  {
    if (d.ParseInsitu<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseNanAndInfFlag |
                      rapidjson::kParseFullPrecisionFlag | rapidjson::kParseTrailingCommasFlag |
                      rapidjson::kParseEscapedApostropheFlag>((LPSTR)cStrTempA).HasParseError() != false)
    {
      nState = StateError;
      return MX_E_InvalidData;
    }
  }
  catch (std::bad_alloc &)
  {
    nState = StateError;
    return E_OUTOFMEMORY;
  }
  catch (...)
  {
    nState = StateError;
    return MX_E_InvalidData;
  }

  //success
  nState = StateDone;
  return S_OK;
}

} //namespace MX
