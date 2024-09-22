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
#include "..\..\Include\Http\HttpAuthBearer.h"
/*
#include "..\..\Include\Http\HttpCommon.h"
#include "..\..\Include\Crypto\Base64.h"
#include "..\..\Include\AutoPtr.h"
#include "..\..\Include\Strings\Utf8.h"
#include "..\..\Include\Crypto\MessageDigest.h"
#include "..\..\Include\FnvHash.h"
*/

/*
WWW-Authenticate: Bearer scope="{scope}", realm="example",
                       error="invalid_token",
                       error_description="The access token expired"

Authorization: Bearer mF_9.B5f-4.1JqM

scope a space separated list of scope items, items chars must be inside this set %x21 / %x23-5B / %x5D-7E
*/

//-----------------------------------------------------------

namespace MX {

CHttpAuthBearer::CHttpAuthBearer() : CHttpAuthBase(), CNonCopyableObj()
{
  return;
}

CHttpAuthBearer::~CHttpAuthBearer()
{
  return;
}

HRESULT CHttpAuthBearer::Parse(_In_ CHttpHeaderRespWwwProxyAuthenticateCommon *lpHeader)
{
  DWORD dwFields = 0;
  SIZE_T i, nCount;
  MX::CStringW cStrTempW;

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

    //process pair
    if (StrCompareA(szNameA, "scope") == 0)
    {
      LPCWSTR szScopeStartW;

      if ((dwFields & 1) != 0)
        return MX_E_InvalidData;
      dwFields |= 1;

      //parse scope
      for (;;)
      {
        while (*szValueW == L' ' || *szValueW == L'\t')
          szValueW++;
        if (*szValueW == 0)
          break;

        szScopeStartW = szValueW;
        while (*szValueW >= 0x21 && *szValueW <= 0x7F && *szValueW != L'"' && *szValueW != L'\\')
          szValueW++;

        if (*szValueW != 0 && *szValueW != L' ' && *szValueW != '\t')
          return MX_E_InvalidData;

        if (cStrTempW.CopyN(szScopeStartW, (SIZE_T)(szValueW - szScopeStartW)) == FALSE)
          return E_OUTOFMEMORY;

        if (aScopesList.AddElement((LPCWSTR)cStrTempW) == FALSE)
          return E_OUTOFMEMORY;
        cStrTempW.Detach();
      }
    }
    else if (StrCompareA(szNameA, "realm") == 0)
    {
      if ((dwFields & 2) != 0)
        return MX_E_InvalidData;
      dwFields |= 2;

      //copy value
      if (cStrRealmW.Copy(szValueW) == FALSE)
        return E_OUTOFMEMORY;
    }
    else if (StrCompareA(szNameA, "error") == 0)
    {
      if ((dwFields & 4) != 0)
        return MX_E_InvalidData;
      dwFields |= 4;

      //copy value
      if (cStrErrorW.Copy(szValueW) == FALSE)
        return E_OUTOFMEMORY;
    }
    else if (StrCompareA(szNameA, "error_description") == 0)
    {
      if ((dwFields & 8) != 0)
        return MX_E_InvalidData;
      dwFields |= 8;

      //copy value
      if (cStrErrorDescriptionW.Copy(szValueW) == FALSE)
        return E_OUTOFMEMORY;
    }
  }

  //some checks
  if (cStrRealmW.IsEmpty() != FALSE)
    return MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpAuthBearer::GenerateResponse(_Out_ CStringA &cStrDestA, _In_z_ LPCSTR szAccessTokenA)
{
  HRESULT hRes;

  cStrDestA.Empty();
  if (szAccessTokenA == NULL)
    return E_POINTER;
  if (*szAccessTokenA == 0)
    return E_INVALIDARG;

  hRes = S_OK;

  //build response
  if (cStrDestA.Format("Bearer %s", szAccessTokenA) == FALSE)
    hRes = E_OUTOFMEMORY;

  //done
  if (FAILED(hRes))
    cStrDestA.Empty();
  return hRes;
}

} //namespace MX
