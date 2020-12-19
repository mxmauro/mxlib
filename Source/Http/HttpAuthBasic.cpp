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
#include "..\..\Include\Http\HttpAuthBasic.h"
#include "..\..\Include\Crypto\Base64.h"
#include "..\..\Include\Strings\Utf8.h"

//-----------------------------------------------------------

static HRESULT Encode(_Inout_ MX::CStringA &cStrA, _In_z_ LPCWSTR szValueW, _In_ BOOL bAppend, _In_ BOOL bIsUTF8);

//-----------------------------------------------------------

namespace MX {

CHttpAuthBasic::CHttpAuthBasic() : CHttpAuthBase(), CNonCopyableObj()
{
  bCharsetIsUtf8 = FALSE;
  return;
}

CHttpAuthBasic::~CHttpAuthBasic()
{
  return;
}

HRESULT CHttpAuthBasic::Parse(_In_ CHttpHeaderRespWwwProxyAuthenticateCommon *lpHeader)
{
  DWORD dwFields = 0;
  SIZE_T i, nCount;

  if (lpHeader == NULL)
    return E_POINTER;
  if (StrCompareA(lpHeader->GetScheme(), GetScheme(), TRUE) != 0)
    return MX_E_InvalidData;

  //parse parameters
  nCount = lpHeader->GetParamsCount();
  for (i = 0; i < nCount; i++)
  {
    LPCSTR szNameA = lpHeader->GetParamName(i);
    LPCWSTR szValueW = lpHeader->GetParamValue(i);

    if (StrCompareA(szNameA, "realm") == 0)
    {
      if ((dwFields & 1) != 0)
        return MX_E_InvalidData;
      dwFields |= 1;

      //copy value
      if (cStrRealmW.Copy(szValueW) == FALSE)
        return E_OUTOFMEMORY;
    }
    else if (StrCompareA(szNameA, "charset") == 0)
    {
      if ((dwFields & 2) != 0)
        return MX_E_InvalidData;
      dwFields |= 2;

      //parse value
      if (StrCompareW(szValueW, L"utf-8", TRUE) == 0 || StrCompareW(szValueW, L"utf8", TRUE) == 0)
      {
        bCharsetIsUtf8 = TRUE;
      }
      else if (StrCompareW(szValueW, L"iso-8859-1", TRUE) != 0)
      {
        return E_NOTIMPL;
      }
    }
  }

  //some checks
  if (cStrRealmW.IsEmpty() != FALSE)
    return MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpAuthBasic::GenerateResponse(_Out_ CStringA &cStrDestA, _In_z_ LPCWSTR szUserNameW,
                                         _In_z_ LPCWSTR szPasswordW)
{
  CSecureStringA cStrTempA;
  HRESULT hRes;

  cStrDestA.Empty();
  if (szUserNameW == NULL || szPasswordW == NULL)
    return E_POINTER;
  if (*szUserNameW == 0 || StrChrW(szUserNameW, L':') != NULL)
    return E_INVALIDARG;

  //encoded(username):encoded(password)
  hRes = Encode(cStrTempA, szUserNameW, FALSE, bCharsetIsUtf8);
  if (SUCCEEDED(hRes))
  {
    if (cStrTempA.ConcatN(":", 1) != FALSE)
      hRes = Encode(cStrTempA, szPasswordW, TRUE, bCharsetIsUtf8);
    else
      hRes = E_OUTOFMEMORY;
  }

  //convert to base64
  if (SUCCEEDED(hRes))
  {
    CBase64Encoder cBase64Enc;

    hRes = cBase64Enc.Begin(cBase64Enc.GetRequiredSpace(cStrTempA.GetLength()));
    if (SUCCEEDED(hRes))
      hRes = cBase64Enc.Process((LPSTR)cStrTempA, cStrTempA.GetLength());
    if (SUCCEEDED(hRes))
      hRes = cBase64Enc.End();
    //final string
    if (SUCCEEDED(hRes))
    {
      if (cStrDestA.CopyN("Basic ", 6) == FALSE ||
          cStrDestA.ConcatN(cBase64Enc.GetBuffer(), cBase64Enc.GetOutputLength()) == FALSE ||
          cStrDestA.ConcatN("\r\n", 2) == FALSE)
      {
        hRes = E_OUTOFMEMORY;
      }
    }
  }

  //done
  if (FAILED(hRes))
    cStrDestA.Empty();
  return hRes;
}

} //namespace MX

//-----------------------------------------------------------

static HRESULT Encode(_Inout_ MX::CStringA &cStrA, _In_z_ LPCWSTR szValueW, _In_ BOOL bAppend, _In_ BOOL bIsUTF8)
{
  SIZE_T nValueLen = MX::StrLenW(szValueW);
  int nDestChars;

  if (bAppend == FALSE)
    cStrA.Empty();
  if (bIsUTF8 != FALSE)
    return MX::Utf8_Encode(cStrA, szValueW, nValueLen, bAppend);

  nDestChars = ::WideCharToMultiByte(28591, WC_COMPOSITECHECK, szValueW, (int)nValueLen, NULL, 0, NULL, NULL);
  if (nDestChars > 0)
  {
    int nBufSize = nDestChars;

    if (cStrA.EnsureBuffer(cStrA.GetLength() + nBufSize) == FALSE)
      return E_OUTOFMEMORY;
    nDestChars = ::WideCharToMultiByte(28591, WC_COMPOSITECHECK, szValueW, (int)nValueLen,
                                       (LPSTR)cStrA + cStrA.GetLength(), nBufSize, NULL, NULL);
    if (nDestChars < 0)
      nDestChars = 0;
    ((LPSTR)cStrA)[cStrA.GetLength() + (SIZE_T)nDestChars] = 0;
    cStrA.Refresh();
  }

  //done
  return S_OK;
}
