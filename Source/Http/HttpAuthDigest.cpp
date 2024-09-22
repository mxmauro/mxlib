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
#include "..\..\Include\Http\HttpAuthDigest.h"
#include "..\..\Include\FnvHash.h"
#include "..\..\Include\Strings\Utf8.h"
#include <stdio.h>

//-----------------------------------------------------------

static HRESULT Encode(_Inout_ MX::CStringA &cStrA, _In_z_ LPCWSTR szValueW, _In_ BOOL bAppend, _In_ BOOL bIsUTF8);
static BOOL ConvertToHex(_Out_ MX::CStringA &cStrA, _In_ LPCVOID lpData, _In_ SIZE_T nDataLen);
static int ParseQOP(_In_opt_z_ LPCWSTR szValueW);

//-----------------------------------------------------------

namespace MX {

CHttpAuthDigest::CHttpAuthDigest() : CHttpAuthBase(), CNonCopyableObj()
{
  return;
}

CHttpAuthDigest::~CHttpAuthDigest()
{
  return;
}

HRESULT CHttpAuthDigest::Parse(_In_ CHttpHeaderRespWwwProxyAuthenticateCommon *lpHeader)
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

    //process pair
    if (StrCompareA(szNameA, "qop") == 0)
    {
      if ((dwFields & 1) != 0)
        return MX_E_InvalidData;
      dwFields |= 1;

      //parse value
      nQop = ParseQOP(szValueW);
      if (nQop < 0)
        return MX_E_InvalidData;
      if (nQop == 2)
        return MX_E_Unsupported;
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
    else if (StrCompareA(szNameA, "nonce") == 0)
    {
      if ((dwFields & 4) != 0)
        return MX_E_InvalidData;
      dwFields |= 4;

      //copy value
      if (cStrNonceW.Copy(szValueW) == FALSE)
        return E_OUTOFMEMORY;
    }
    else if (StrCompareA(szNameA, "stale") == 0)
    {
      if ((dwFields & 8) != 0)
        return MX_E_InvalidData;
      dwFields |= 8;

      //parse value
      if (StrCompareW(szValueW, L"true", TRUE) == 0 || StrCompareW(szValueW, L"1") == 0)
      {
        bStale = TRUE;
      }
      else if (StrCompareW(szValueW, L"false", TRUE) == 0 || StrCompareW(szValueW, L"0") == 0)
      {
        return MX_E_InvalidData;
      }
    }
    else if (StrCompareA(szNameA, "domain") == 0)
    {
      if ((dwFields & 16) != 0)
        return MX_E_InvalidData;
      dwFields |= 16;

      //copy value
      if (cStrDomainW.Copy(szValueW) == FALSE)
        return E_OUTOFMEMORY;
    }
    else if (StrCompareA(szNameA, "opaque") == 0)
    {
      if ((dwFields & 32) != 0)
        return MX_E_InvalidData;
      dwFields |= 32;

      //copy value
      if (cStrOpaqueW.Copy(szValueW) == FALSE)
        return E_OUTOFMEMORY;
    }
    else if (StrCompareA(szNameA, "charset") == 0)
    {
      if ((dwFields & 64) != 0)
        return MX_E_InvalidData;
      dwFields |= 64;

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
    else if (StrCompareA(szNameA, "userhash") == 0)
    {
      if ((dwFields & 128) != 0)
        return MX_E_InvalidData;
      dwFields |= 128;

      //parse value
      if (StrCompareW(szValueW, L"true", TRUE) == 0 || StrCompareW(szValueW, L"1") == 0)
      {
        bUserHash = TRUE;
      }
      else if (StrCompareW(szValueW, L"false", TRUE) == 0 || StrCompareW(szValueW, L"0") == 0)
      {
        return MX_E_InvalidData;
      }
    }
    else if (StrCompareA(szNameA, "algorithm") == 0)
    {
      CStringA cStrTempA;
      SIZE_T nAlgLen;

      if ((dwFields & 256) != 0)
        return MX_E_InvalidData;
      dwFields |= 256;

      //parse value
      nAlgLen = StrLenW(szValueW);
      if (nAlgLen >= 5 && StrCompareW(szValueW + nAlgLen - 5, L"-sess", TRUE) == 0)
      {
        bAlgorithmSession = TRUE;
        nAlgLen -= 5;
      }

      if (cStrTempA.CopyN(szValueW, nAlgLen) == FALSE)
        return E_OUTOFMEMORY;
      nAlgorithm = MX::CMessageDigest::GetAlgorithm((LPCSTR)cStrTempA);
      if (nAlgorithm == MX::CMessageDigest::eAlgorithm::Invalid)
        return MX_E_Unsupported;
    }
  }

  //some checks
  if (cStrRealmW.IsEmpty() != FALSE || cStrNonceW.IsEmpty() != FALSE)
    return MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpAuthDigest::GenerateResponse(_Out_ CStringA &cStrDestA, _In_z_ LPCWSTR szUserNameW,
                                          _In_z_ LPCWSTR szPasswordW, _In_z_ LPCSTR szMethodA,
                                                  _In_z_ LPCSTR szUriPathA)
{
  CMessageDigest cDigest;
  CSecureStringA cStrTempA, cStrHashKeyA[2], cStrResponseA, cStrUserNameA, cStrNonceA, cStrOpaqueA;
  CHAR szCNonceA[32], szNonceCountA[32];
  LONG _nNonceCount;
  BOOL bExtendedUserName = FALSE;
  HRESULT hRes;

  cStrDestA.Empty();
  if (szUserNameW == NULL || szPasswordW == NULL || szMethodA == NULL || szUriPathA == NULL)
    return E_POINTER;
  if (*szUserNameW == 0 || *szMethodA == 0 || *szUriPathA == 0)
    return E_INVALIDARG;

  //generate a cnonce if a session algorithm or if we have a qop
  szCNonceA[0] = 0;
  if (bAlgorithmSession != FALSE || (nQop & 1) != 0)
  {
    Fnv64_t nCNonce;
    union {
      LPVOID lp;
      DWORD dw;
    };

    nCNonce = fnv_64a_buf((LPCWSTR)cStrNonceW, cStrNonceW.GetLength(), FNV1A_64_INIT);
    nCNonce = fnv_64a_buf((LPCWSTR)cStrRealmW, cStrRealmW.GetLength() * 2, nCNonce);
    nCNonce = fnv_64a_buf((LPCWSTR)cStrDomainW, cStrDomainW.GetLength() * 2, nCNonce);
    nCNonce = fnv_64a_buf((LPCWSTR)cStrOpaqueW, cStrOpaqueW.GetLength(), nCNonce);

    nCNonce = fnv_64a_buf(&bStale, sizeof(bStale), nCNonce);
    nCNonce = fnv_64a_buf(&bAlgorithmSession, sizeof(bAlgorithmSession), nCNonce);
    nCNonce = fnv_64a_buf(&nAlgorithm, sizeof(nAlgorithm), nCNonce);
    nCNonce = fnv_64a_buf(&nQop, sizeof(nQop), nCNonce);
    lp = this;
    nCNonce = fnv_64a_buf(&lp, sizeof(lp), nCNonce);
#pragma warning(suppress : 28159)
    dw = ::GetTickCount();
    nCNonce = fnv_64a_buf(&dw, sizeof(dw), nCNonce);
    sprintf_s(szCNonceA, MX_ARRAYLEN(szCNonceA), "%16I64x", nCNonce);
  }

  //nonce count
  _nNonceCount = _InterlockedIncrement(&nNonceCount);
  sprintf_s(szNonceCountA, MX_ARRAYLEN(szNonceCountA), ":%08lx:", (ULONG)_nNonceCount);

  hRes = Encode(cStrNonceA, (LPCWSTR)cStrNonceW, FALSE, bCharsetIsUtf8);
  if (SUCCEEDED(hRes))
    hRes = Encode(cStrOpaqueA, (LPCWSTR)cStrOpaqueW, FALSE, bCharsetIsUtf8);

  //HA1 = DIGEST(encoded(username):realm:encoded(password))
  if (SUCCEEDED(hRes))
  {
    hRes = cDigest.BeginDigest((CMessageDigest::eAlgorithm)nAlgorithm);
    if (SUCCEEDED(hRes))
    {
      //username
      if (bUserHash != FALSE)
      {
        CMessageDigest cDigestUserHash;

        hRes = cDigestUserHash.BeginDigest((CMessageDigest::eAlgorithm)nAlgorithm);
        if (SUCCEEDED(hRes))
        {
          //add user name
          hRes = Encode(cStrTempA, szUserNameW, FALSE, bCharsetIsUtf8);

          //add realm
          if (SUCCEEDED(hRes) && cStrTempA.ConcatN(L":", 1) == FALSE)
            hRes = E_OUTOFMEMORY;
          if (SUCCEEDED(hRes))
            hRes = Encode(cStrTempA, (LPCWSTR)cStrRealmW, TRUE, bCharsetIsUtf8);

          if (SUCCEEDED(hRes))
          {
            hRes = cDigestUserHash.DigestStream((LPCSTR)cStrTempA, cStrTempA.GetLength());

            //add result hash
            if (SUCCEEDED(hRes))
            {
              hRes = cDigestUserHash.EndDigest();
              if (SUCCEEDED(hRes))
              {
                if (ConvertToHex(cStrUserNameA, cDigestUserHash.GetResult(), cDigestUserHash.GetResultSize()) == FALSE)
                  hRes = E_OUTOFMEMORY;
              }
            }
          }
        }
      }
      else
      {
        hRes = Encode(cStrTempA, szUserNameW, FALSE, bCharsetIsUtf8);
        if (SUCCEEDED(hRes))
        {
          hRes = CUrl::Encode(cStrUserNameA, (LPCSTR)cStrTempA, cStrTempA.GetLength(), "!#$&+^`{}");
          if (SUCCEEDED(hRes))
          {
            if (cStrUserNameA.GetLength() != cStrTempA.GetLength())
            {
              bExtendedUserName = TRUE;
              if (cStrUserNameA.Insert((bCharsetIsUtf8 != FALSE) ? "UTF-8''" : "ISO-8859-1''", 0) == FALSE)
                hRes = E_OUTOFMEMORY;
            }
            else
            {
              cStrUserNameA.Attach(cStrTempA.Detach());
            }
          }
        }
      }
      if (SUCCEEDED(hRes))
        hRes = cDigest.DigestStream((LPCSTR)cStrUserNameA, cStrUserNameA.GetLength());

      //realm
      if (SUCCEEDED(hRes))
      {
        hRes = cDigest.DigestStream(":", 1);
        if (SUCCEEDED(hRes))
        {
          hRes = Encode(cStrTempA, (LPCWSTR)cStrRealmW, FALSE, bCharsetIsUtf8);
          if (SUCCEEDED(hRes))
            hRes = cDigest.DigestStream((LPCSTR)cStrTempA, cStrTempA.GetLength());
        }
      }

      //password
      if (SUCCEEDED(hRes))
      {
        hRes = cDigest.DigestStream(":", 1);
        if (SUCCEEDED(hRes))
        {
          hRes = Encode(cStrTempA, szPasswordW, FALSE, bCharsetIsUtf8);
          if (SUCCEEDED(hRes))
            hRes = cDigest.DigestStream((LPCSTR)cStrTempA, cStrTempA.GetLength());
        }
      }

      //session-ize (HA1 = DIGEST(HA1:nonce:cnonce)
      if (SUCCEEDED(hRes))
      {
        if (bAlgorithmSession != FALSE)
        {
          CMessageDigest cDigestSession;

          hRes = cDigestSession.BeginDigest((CMessageDigest::eAlgorithm)nAlgorithm);
          if (SUCCEEDED(hRes))
          {
            //HA1
            if (ConvertToHex(cStrTempA, cDigest.GetResult(), cDigest.GetResultSize()) != FALSE)
              hRes = cDigestSession.DigestStream((LPCSTR)cStrTempA, cStrTempA.GetLength());
            else
              hRes = E_OUTOFMEMORY;

            //nonce
            if (SUCCEEDED(hRes))
            {
              hRes = cDigestSession.DigestStream(":", 1);
              if (SUCCEEDED(hRes))
                hRes = cDigestSession.DigestStream((LPCSTR)cStrNonceA, cStrNonceA.GetLength());
            }

            //cnonce
            if (SUCCEEDED(hRes))
            {
              hRes = cDigestSession.DigestStream(":", 1);
              if (SUCCEEDED(hRes))
                hRes = cDigestSession.DigestStream(szCNonceA, StrLenA(szCNonceA));
            }
            if (SUCCEEDED(hRes))
              hRes = cDigestSession.EndDigest();

            //convert result to hex
            if (SUCCEEDED(hRes))
            {
              if (ConvertToHex(cStrHashKeyA[0], cDigestSession.GetResult(), cDigestSession.GetResultSize()) == FALSE)
                hRes = E_OUTOFMEMORY;
            }
          }
        }
        else
        {
          if (SUCCEEDED(hRes))
            hRes = cDigest.EndDigest();

          //convert result to hex
          if (SUCCEEDED(hRes))
          {
            if (ConvertToHex(cStrHashKeyA[0], cDigest.GetResult(), cDigest.GetResultSize()) == FALSE)
              hRes = E_OUTOFMEMORY;
          }
        }
      }
    }
  }

  //IMPORTANT: "Auth-int" quality of protection is not supported
  //HA2 = DIGEST(method:path)
  if (SUCCEEDED(hRes))
  {
    hRes = cDigest.BeginDigest((CMessageDigest::eAlgorithm)nAlgorithm);
    if (SUCCEEDED(hRes))
    {
      //method
      hRes = cDigest.DigestStream(szMethodA, StrLenA(szMethodA));

      //uri
      if (SUCCEEDED(hRes))
      {
        hRes = cDigest.DigestStream(":", 1);
        if (SUCCEEDED(hRes))
          hRes = cDigest.DigestStream(szUriPathA, StrLenA(szUriPathA));
      }
    }
    if (SUCCEEDED(hRes))
      hRes = cDigest.EndDigest();

    //convert result to hex
    if (SUCCEEDED(hRes))
    {
      if (ConvertToHex(cStrHashKeyA[1], cDigest.GetResult(), cDigest.GetResultSize()) == FALSE)
        hRes = E_OUTOFMEMORY;
    }
  }

  //build response value
  if (SUCCEEDED(hRes))
  {
    hRes = cDigest.BeginDigest((CMessageDigest::eAlgorithm)nAlgorithm);
    if (SUCCEEDED(hRes))
    {
      //HA1
      hRes = cDigest.DigestStream((LPCSTR)cStrHashKeyA[0], cStrHashKeyA[0].GetLength());

      //nonce
      if (SUCCEEDED(hRes))
      {
        hRes = cDigest.DigestStream(":", 1);
        if (SUCCEEDED(hRes))
          hRes = cDigest.DigestStream((LPCSTR)cStrNonceA, cStrNonceA.GetLength());
      }

      //use AUTH qop?
      if (SUCCEEDED(hRes) && (nQop & 1) != 0)
      {
        //nonce count (this has the colon separators at the start and end of string)
        hRes = cDigest.DigestStream(szNonceCountA, StrLenA(szNonceCountA));

        //cnonce
        if (SUCCEEDED(hRes))
          hRes = cDigest.DigestStream(szCNonceA, StrLenA(szCNonceA));

        //qop
        if (SUCCEEDED(hRes))
          hRes = cDigest.DigestStream(":auth", 5);
      }

      //HA2
      if (SUCCEEDED(hRes))
      {
        hRes = cDigest.DigestStream(":", 1);
        if (SUCCEEDED(hRes))
          hRes = cDigest.DigestStream((LPCSTR)cStrHashKeyA[1], cStrHashKeyA[1].GetLength());
      }
      if (SUCCEEDED(hRes))
        hRes = cDigest.EndDigest();

      //convert result to hex
      if (SUCCEEDED(hRes))
      {
        if (ConvertToHex(cStrResponseA, cDigest.GetResult(), cDigest.GetResultSize()) == FALSE)
          hRes = E_OUTOFMEMORY;
      }
    }
  }

  //build response
  if (SUCCEEDED(hRes))
  {
    if (cStrDestA.ConcatN("Digest ", 7) == FALSE)
    {
      hRes = E_OUTOFMEMORY;
    }

    //username
    if (SUCCEEDED(hRes))
    {
      if (bExtendedUserName == FALSE)
      {
        if (cStrDestA.ConcatN("username=\"", 10) == FALSE ||
            cStrDestA.ConcatN((LPCSTR)cStrUserNameA, cStrUserNameA.GetLength()) == FALSE ||
            cStrDestA.ConcatN("\"", 1) == FALSE)
        {
          hRes = E_OUTOFMEMORY;
        }
      }
      else
      {
        if (cStrDestA.ConcatN("username*=", 10) == FALSE ||
            cStrDestA.ConcatN((LPCSTR)cStrUserNameA, cStrUserNameA.GetLength()) == FALSE)
        {
          hRes = E_OUTOFMEMORY;
        }
      }
    }

    //realm
    if (SUCCEEDED(hRes))
    {
      if (cStrDestA.ConcatN(", realm=\"", 9) != FALSE)
      {
        hRes = Encode(cStrDestA, (LPCWSTR)cStrRealmW, TRUE, bCharsetIsUtf8);
        if (SUCCEEDED(hRes) && cStrDestA.ConcatN("\"", 1) == FALSE)
          hRes = E_OUTOFMEMORY;
      }
      else
      {
        hRes = E_OUTOFMEMORY;
      }
    }

    //nonce
    if (SUCCEEDED(hRes) && cStrNonceA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(", nonce=\"", 9) == FALSE ||
          cStrDestA.ConcatN((LPCSTR)cStrNonceA, cStrNonceA.GetLength()) == FALSE ||
          cStrDestA.ConcatN("\"", 1) == FALSE)
      {
        hRes = E_OUTOFMEMORY;
      }
    }

    //qop and nonce-count (only AUTH supported)
    if (SUCCEEDED(hRes) && nQop != 0)
    {
      if (cStrDestA.ConcatN(", qop=\"auth\", nc=", 17) == FALSE ||
          cStrDestA.ConcatN(szNonceCountA + 1, StrLenA(szNonceCountA) - 2) == FALSE)
      {
        hRes = E_OUTOFMEMORY;
      }
    }

    //uri
    if (SUCCEEDED(hRes))
    {
      if (cStrDestA.ConcatN(", uri=\"", 7) == FALSE ||
          cStrDestA.ConcatN(szUriPathA, StrLenA(szUriPathA)) == FALSE ||
          cStrDestA.ConcatN("\"", 1) == FALSE)
      {
        hRes = E_OUTOFMEMORY;
      }
    }

    //cnonce
    if (SUCCEEDED(hRes) && *szCNonceA != 0)
    {
      if (cStrDestA.ConcatN(", cnonce=\"", 10) == FALSE ||
          cStrDestA.ConcatN(szCNonceA + 1, StrLenA(szCNonceA) - 2) == FALSE ||
          cStrDestA.ConcatN("\"", 1) == FALSE)
      {
        hRes = E_OUTOFMEMORY;
      }
    }

    //response
    if (SUCCEEDED(hRes))
    {
      if (cStrDestA.ConcatN(", response=\"", 12) == FALSE ||
          cStrDestA.ConcatN((LPCSTR)cStrResponseA, cStrResponseA.GetLength()) == FALSE ||
          cStrDestA.ConcatN("\"", 1) == FALSE)
      {
        hRes = E_OUTOFMEMORY;
      }
    }

    //opaque
    if (SUCCEEDED(hRes) && cStrOpaqueA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(", opaque=\"", 10) == FALSE ||
          cStrDestA.ConcatN((LPCSTR)cStrOpaqueA, cStrOpaqueA.GetLength()) == FALSE ||
          cStrDestA.ConcatN("\"", 1) == FALSE)
      {
        hRes = E_OUTOFMEMORY;
      }
    }

    //userhash
    if (SUCCEEDED(hRes) && bUserHash != FALSE)
    {
      if (cStrDestA.ConcatN(", userhash=true", 15) == FALSE)
        hRes = E_OUTOFMEMORY;
    }
    //charset
    if (SUCCEEDED(hRes) && bCharsetIsUtf8 != FALSE)
    {
      if (cStrDestA.ConcatN(", charset=UTF-8", 15) == FALSE)
        hRes = E_OUTOFMEMORY;
    }

    //end of string
    if (SUCCEEDED(hRes))
    {
      if (cStrDestA.ConcatN("\r\n", 2) == FALSE)
        hRes = E_OUTOFMEMORY;
    }
  }

  //done
  if (FAILED(hRes))
    cStrDestA.Empty();
  return hRes;
}

} //namespace MX

//-----------------------------------------------------------

static BOOL ConvertToHex(_Out_ MX::CStringA &cStrA, _In_ LPCVOID lpData, _In_ SIZE_T nDataLen)
{
  static const LPCSTR szHexaA = "0123456789abcdef"; //use lowercase
  LPBYTE s = (LPBYTE)lpData;
  LPBYTE sEnd = (LPBYTE)lpData + nDataLen;

  cStrA.Empty();
  while (s < sEnd)
  {
    if (cStrA.ConcatN(&szHexaA[(*s) >> 4], 1) == FALSE ||
        cStrA.ConcatN(&szHexaA[(*s) & 0x0F], 1) == FALSE)
    {
      return FALSE;
    }
    s++;
  }
  return TRUE;
}

static int ParseQOP(_In_opt_z_ LPCWSTR szValueW)
{
  int res = 0;

  while (*szValueW != 0)
  {
    while (*szValueW == L' ')
      szValueW++;

    if (MX::StrNCompareW(szValueW, L"auth-int", 8, TRUE) == 0 &&
        (szValueW[8] == 0 || szValueW[8] == L',' || szValueW[8] == L' '))
    {
      szValueW += 8;
      while (*szValueW == L' ')
        szValueW++;
      if (*szValueW != 0 && *szValueW != L',')
        return -1; //invalid data
      res |= 2;
    }
    else if (MX::StrNCompareW(szValueW, L"auth", 4, TRUE) == 0 &&
             (szValueW[4] == 0 || szValueW[4] == L',' || szValueW[4] == L' '))
    {
      szValueW += 4;
      while (*szValueW == L' ')
        szValueW++;
      if (*szValueW != 0 && *szValueW != L',')
        return -1; //invalid data
      res |= 1;
    }
    else
    {
      while (*szValueW != 0 && *szValueW != L',')
        szValueW++;
    }
    if (*szValueW == L',')
      szValueW++;
  }

  //done
  return res;
}

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
