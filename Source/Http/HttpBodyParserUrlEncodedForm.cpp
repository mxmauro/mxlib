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
#include "..\..\Include\Http\HttpBodyParserUrlEncodedForm.h"
#include "..\..\Include\Http\Url.h"
#include "..\..\Include\Strings\Utf8.h"

//-----------------------------------------------------------

namespace MX {

CHttpBodyParserUrlEncodedForm::CHttpBodyParserUrlEncodedForm(_In_ DWORD _dwMaxFieldSize) : CHttpBodyParserFormBase(),
                                                                                           CNonCopyableObj()
{
  dwMaxFieldSize = _dwMaxFieldSize;
  if (dwMaxFieldSize < 32768)
    dwMaxFieldSize = 32768;
  return;
}

CHttpBodyParserUrlEncodedForm::~CHttpBodyParserUrlEncodedForm()
{
  return;
}

HRESULT CHttpBodyParserUrlEncodedForm::Initialize(_In_ Internals::CHttpParser &cHttpParser)
{
  return S_OK;
}

#define BACKWARD_CHAR()     szDataA--
HRESULT CHttpBodyParserUrlEncodedForm::Parse(_In_opt_ LPCVOID lpData, _In_opt_ SIZE_T nDataSize)
{
  CStringA cStrTempA;
  CStringW cStrTempW;
  LPCSTR szDataA;
  HRESULT hRes;

  if (lpData == NULL && nDataSize > 0)
    return E_POINTER;
  if (sParser.nState == eState::Done)
    return S_OK;
  if (sParser.nState == eState::Error)
    return MX_E_InvalidData;

  //end of parsing?
  if (lpData == NULL)
  {
    switch (sParser.nState)
    {
      case eState::NameStart:
        break;

      case eState::Name:
        hRes = CUrl::Decode(cStrTempA, (LPCSTR)(sParser.cStrCurrA), sParser.cStrCurrA.GetLength());
        if (SUCCEEDED(hRes))
          hRes = Utf8_Decode(sParser.cStrCurrFieldNameW, (LPCSTR)cStrTempA, cStrTempA.GetLength());
        if (FAILED(hRes))
          goto done;
        break;

      case eState::Value:
        hRes = CUrl::Decode(cStrTempA, (LPCSTR)(sParser.cStrCurrA), sParser.cStrCurrA.GetLength());
        if (SUCCEEDED(hRes))
          hRes = Utf8_Decode(cStrTempW, (LPCSTR)cStrTempA, cStrTempA.GetLength());
        if (SUCCEEDED(hRes))
          hRes = AddField((LPCWSTR)(sParser.cStrCurrFieldNameW), (LPCWSTR)cStrTempW);
        if (FAILED(hRes))
          goto done;
        break;
    }
    sParser.nState = eState::Done;
    return S_OK;
  }

  //check if size is greater than max size or overflow
  if (nCurrContentSize+nDataSize < nCurrContentSize || nCurrContentSize+nDataSize > (SIZE_T)dwMaxFieldSize)
    return MX_E_BadLength;

  //process data
  hRes = S_OK;
  for (szDataA = (LPCSTR)lpData; szDataA != (LPCSTR)lpData + nDataSize; szDataA++)
  {
    switch (sParser.nState)
    {
      case eState::NameStart:
        if (*szDataA == '&')
          break; //ignore multiple '&' separators
        if (*szDataA == '=')
        {
err_invalid_data:
          hRes = MX_E_InvalidData;
          goto done;
        }
        sParser.nState = eState::Name;
        //fall into 'StateName'

      case eState::Name:
        if (*szDataA == '=' || *szDataA == '&')
        {
          hRes = CUrl::Decode(cStrTempA, (LPCSTR)(sParser.cStrCurrA), sParser.cStrCurrA.GetLength());
          if (SUCCEEDED(hRes))
            hRes = Utf8_Decode(sParser.cStrCurrFieldNameW, (LPCSTR)cStrTempA, cStrTempA.GetLength());
          if (FAILED(hRes))
            goto done;
          sParser.cStrCurrA.Empty();
          if (*szDataA == '&')
          {
            BACKWARD_CHAR();
          }
          sParser.nState = eState::Value;
          break;
        }
        if (*((unsigned char*)szDataA) < 32 || (*szDataA & 0x80) != 0)
          goto err_invalid_data;
        //add character to name
        if (sParser.cStrCurrA.ConcatN(szDataA, 1) == FALSE)
        {
          hRes = E_OUTOFMEMORY;
          goto done;
        }
        break;

      case eState::Value:
        if (*szDataA == '&')
        {
          hRes = CUrl::Decode(cStrTempA, (LPCSTR)(sParser.cStrCurrA), sParser.cStrCurrA.GetLength());
          if (SUCCEEDED(hRes))
            hRes = Utf8_Decode(cStrTempW, (LPCSTR)cStrTempA, cStrTempA.GetLength());
          if (SUCCEEDED(hRes))
            hRes = AddField((LPCWSTR)(sParser.cStrCurrFieldNameW), (LPCWSTR)cStrTempW);
          if (FAILED(hRes))
            goto done;
          sParser.nState = eState::NameStart;
          sParser.cStrCurrFieldNameW.Empty();
          sParser.cStrCurrA.Empty();
          break;
        }
          if (*((unsigned char*)szDataA) < 32 || (*szDataA & 0x80) != 0)
            goto err_invalid_data;
          //add character to value
          if (sParser.cStrCurrA.ConcatN(szDataA, 1) == FALSE)
          {
            hRes = E_OUTOFMEMORY;
            goto done;
          }
          break;

        default:
          MX_ASSERT(FALSE);
          break;
      }
  }

done:
  if (FAILED(hRes))
    sParser.nState = eState::Error;
  return hRes;
}
#undef BACKWARD_CHAR

} //namespace MX
