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
#include "..\..\Include\Http\HttpBodyParserUrlEncodedForm.h"
#include "..\..\Include\Http\Url.h"
#include "..\..\Include\Strings\Utf8.h"

//-----------------------------------------------------------

namespace MX {

CHttpBodyParserUrlEncodedForm::CHttpBodyParserUrlEncodedForm(_In_ DWORD _dwMaxFieldSize) : CHttpBodyParserFormBase()
{
  dwMaxFieldSize = _dwMaxFieldSize;
  if (dwMaxFieldSize < 32768)
    dwMaxFieldSize = 32768;
  sParser.nState = StateNameStart;
  nCurrContentSize = 0;
  return;
}

CHttpBodyParserUrlEncodedForm::~CHttpBodyParserUrlEncodedForm()
{
  return;
}

HRESULT CHttpBodyParserUrlEncodedForm::Initialize(_In_ CHttpCommon &cHttpCmn)
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
  if (sParser.nState == StateDone)
    return S_OK;
  if (sParser.nState == StateError)
    return MX_E_InvalidData;

  //end of parsing?
  if (lpData == NULL)
  {
    switch (sParser.nState)
    {
      case StateNameStart:
        break;

      case StateName:
        hRes = CUrl::Decode(cStrTempA, (LPCSTR)(sParser.cStrCurrA), sParser.cStrCurrA.GetLength());
        if (SUCCEEDED(hRes))
          hRes = Utf8_Decode(sParser.cStrCurrFieldNameW, (LPCSTR)cStrTempA, cStrTempA.GetLength());
        if (FAILED(hRes))
          goto done;
        break;

      case StateValue:
        hRes = CUrl::Decode(cStrTempA, (LPCSTR)(sParser.cStrCurrA), sParser.cStrCurrA.GetLength());
        if (SUCCEEDED(hRes))
          hRes = Utf8_Decode(cStrTempW, (LPCSTR)cStrTempA, cStrTempA.GetLength());
        if (SUCCEEDED(hRes))
          hRes = AddField((LPCWSTR)(sParser.cStrCurrFieldNameW), (LPCWSTR)cStrTempW);
        if (FAILED(hRes))
          goto done;
        break;
    }
    sParser.nState = StateDone;
    return S_OK;
  }

  //begin
  hRes = S_OK;
  //check if size is greater than max size or overflow
  if (nCurrContentSize+nDataSize < nCurrContentSize || nCurrContentSize+nDataSize > (SIZE_T)dwMaxFieldSize)
  {
    MarkEntityAsTooLarge();
  }

  //process data
  for (szDataA=(LPCSTR)lpData; szDataA!=(LPCSTR)lpData+nDataSize; szDataA++)
  {
    switch (sParser.nState)
    {
      case StateNameStart:
        if (*szDataA == '&')
          break; //ignore multiple '&' separators
        if (*szDataA == '=')
        {
err_invalid_data:
          hRes = MX_E_InvalidData;
          goto done;
        }
        sParser.nState = StateName;
        //fall into 'StateName'

      case StateName:
        if (*szDataA == '=' || *szDataA == '&')
        {
          if (IsEntityTooLarge() == FALSE)
          {
            hRes = CUrl::Decode(cStrTempA, (LPCSTR)(sParser.cStrCurrA), sParser.cStrCurrA.GetLength());
            if (SUCCEEDED(hRes))
              hRes = Utf8_Decode(sParser.cStrCurrFieldNameW, (LPCSTR)cStrTempA, cStrTempA.GetLength());
            if (FAILED(hRes))
              goto done;
          }
          sParser.cStrCurrA.Empty();
          if (*szDataA == '&')
          {
            BACKWARD_CHAR();
          }
          sParser.nState = StateValue;
          break;
        }
        if (*((unsigned char*)szDataA) < 32 || (*szDataA & 0x80) != 0)
          goto err_invalid_data;
        //add character to name
        if (IsEntityTooLarge() == FALSE)
        {
          if (sParser.cStrCurrA.ConcatN(szDataA, 1) == FALSE)
          {
            hRes = E_OUTOFMEMORY;
            goto done;
          }
        }
        break;

      case StateValue:
        if (*szDataA == '&')
        {
          if (IsEntityTooLarge() == FALSE)
          {
            hRes = CUrl::Decode(cStrTempA, (LPCSTR)(sParser.cStrCurrA), sParser.cStrCurrA.GetLength());
            if (SUCCEEDED(hRes))
              hRes = Utf8_Decode(cStrTempW, (LPCSTR)cStrTempA, cStrTempA.GetLength());
            if (SUCCEEDED(hRes))
              hRes = AddField((LPCWSTR)(sParser.cStrCurrFieldNameW), (LPCWSTR)cStrTempW);
            if (FAILED(hRes))
              goto done;
          }
          sParser.nState = StateNameStart;
          sParser.cStrCurrFieldNameW.Empty();
          sParser.cStrCurrA.Empty();
          break;
        }
          if (*((unsigned char*)szDataA) < 32 || (*szDataA & 0x80) != 0)
            goto err_invalid_data;
          //add character to value
          if (IsEntityTooLarge() == FALSE)
          {
            if (sParser.cStrCurrA.ConcatN(szDataA, 1) == FALSE)
            {
              hRes = E_OUTOFMEMORY;
              goto done;
            }
          }
          break;

        default:
          MX_ASSERT(FALSE);
          break;
      }
  }

done:
  if (FAILED(hRes))
    sParser.nState = StateError;
  return hRes;
}
#undef BACKWARD_CHAR

} //namespace MX
