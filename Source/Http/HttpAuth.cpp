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
#include "..\..\Include\Http\HttpAuth.h"
#include "..\..\Include\Http\HttpCommon.h"
#include "..\..\Include\Crypto\Base64.h"
#include "..\..\Include\AutoPtr.h"
#include "..\..\Include\Strings\Utf8.h"
#include "..\..\Include\Crypto\DigestAlgorithmSHAx.h"
#include "..\..\Include\Crypto\DigestAlgorithmMDx.h"
#include "..\..\Include\FnvHash.h"

#define _ALGORITHM_MD5                                     0
#define _ALGORITHM_SHA1                                    1
#define _ALGORITHM_SHA256                                  2
#define _ALGORITHM_SHA512                                  3

//-----------------------------------------------------------

static LPCSTR SkipSpaces(_In_z_ LPCSTR sA);
static LPCSTR GetToken(_In_z_ LPCSTR sA, _In_opt_z_ LPCSTR szStopCharsA = NULL);
static LPCSTR Advance(_In_z_ LPCSTR sA, _In_opt_z_ LPCSTR szStopCharsA = NULL);
static HRESULT GetNextPair(_Inout_ LPCSTR &szValueA, _Out_ LPCSTR *lpszNameStartA, _Out_ SIZE_T *lpnNameLen,
                           _Out_ MX::CStringA &cStrValueA);

static HRESULT Decode(_Inout_ MX::CStringW &cStrW, _In_z_ LPCSTR szValueA, _In_ BOOL bIsUTF8);
static HRESULT Encode(_Inout_ MX::CStringA &cStrA, _In_z_ LPCWSTR szValueW, _In_ BOOL bAppend, _In_ BOOL bIsUTF8);

static HRESULT BeginHashing(_Out_ MX::CBaseDigestAlgorithm **lplpDigAlg, _In_ int nAlgorithm);
static BOOL ConvertToHex(_Out_ MX::CStringA &cStrA, _In_ LPCVOID lpData, _In_ SIZE_T nDataLen);
static int ParseQOP(_In_opt_z_ LPCSTR szValueA);

//-----------------------------------------------------------

namespace MX {

CHttpAuthBasic::CHttpAuthBasic() : CHttpBaseAuth()
{
  bCharsetIsUtf8 = FALSE;
  return;
}

CHttpAuthBasic::~CHttpAuthBasic()
{
  return;
}

HRESULT CHttpAuthBasic::Parse(_In_z_ LPCSTR szValueA)
{
  MX::CStringA cStrValueA;
  CStringA _cStrRealmA;
  DWORD dwFields = 0;
  LPCSTR szNameStartA;
  SIZE_T nNameLen;
  HRESULT hRes;

  //parse tokens
  while ((hRes = GetNextPair(szValueA, &szNameStartA, &nNameLen, cStrValueA)) != S_FALSE)
  {
    if (FAILED(hRes))
      return hRes;

    //process pair
    switch (nNameLen)
    {
      case 5:
        if (StrNCompareA(szNameStartA, "realm", 5) == 0)
        {
          if ((dwFields & 1) != 0)
            return MX_E_InvalidData;
          dwFields |= 1;
          //copy value
          _cStrRealmA.Attach(cStrValueA.Detach());
        }
        break;

      case 7:
        if (StrNCompareA(szNameStartA, "charset", 7) == 0)
        {
          if ((dwFields & 2) != 0)
            return MX_E_InvalidData;
          dwFields |= 2;
          //parse value
          if (StrCompareA((LPCSTR)cStrValueA, "utf-8", TRUE) == 0 || StrCompareA((LPCSTR)cStrValueA, "utf8", TRUE) == 0)
          {
            bCharsetIsUtf8 = TRUE;
          }
          else if (StrCompareA((LPCSTR)cStrValueA, "iso-8859-1", TRUE) != 0)
          {
            return E_NOTIMPL;
          }
        }
        break;
    }
  }

  //some checks
  if (_cStrRealmA.IsEmpty() != FALSE)
    return MX_E_InvalidData;

  //decode
  hRes = Decode(cStrRealmW, (LPCSTR)_cStrRealmA, bCharsetIsUtf8);
  if (FAILED(hRes))
    return hRes;

  //done
  return S_OK;
}

HRESULT CHttpAuthBasic::MakeAuthenticateResponse(_Out_ CStringA &cStrDestA, _In_z_ LPCWSTR szUserNameW,
                                                 _In_z_ LPCWSTR szPasswordW, _In_ BOOL bIsProxy)
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
      if ((bIsProxy != FALSE && cStrDestA.CopyN("Proxy-", 6) == FALSE) ||
          cStrDestA.ConcatN("Authorization: Basic ", 21) == FALSE ||
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

BOOL CHttpAuthBasic::IsSameThan(_In_ CHttpBaseAuth *_lpCompareTo)
{
  CHttpAuthBasic *lpCompareTo;

  if (_lpCompareTo == NULL || _lpCompareTo->GetType() != GetType())
    return FALSE;

  lpCompareTo = (CHttpAuthBasic*)_lpCompareTo;

  if (lpCompareTo->bCharsetIsUtf8 != bCharsetIsUtf8)
    return FALSE;
  if (StrCompareW((LPCWSTR)(lpCompareTo->cStrRealmW), (LPCWSTR)cStrRealmW) != 0)
    return FALSE;

  return TRUE;
}

//--------

CHttpAuthDigest::CHttpAuthDigest() : CHttpBaseAuth()
{
  bStale = bUserHash = FALSE;
  nAlgorithm = _ALGORITHM_MD5;
  nQop = 0;
  bCharsetIsUtf8 = FALSE;
  _InterlockedExchange(&nNonceCount, 0);
  return;
}

CHttpAuthDigest::~CHttpAuthDigest()
{
  return;
}

HRESULT CHttpAuthDigest::Parse(_In_z_ LPCSTR szValueA)
{
  MX::CStringA cStrValueA;
  CStringA _cStrRealmA, _cStrNonceA, _cStrDomainA, _cStrOpaqueA;
  DWORD dwFields = 0;
  BOOL _bStale = FALSE, _bUserHash = FALSE;
  int _nAlgorithm = _ALGORITHM_MD5;
  BOOL _bAlgorithmSession = FALSE;
  int _nQop = 0;
  LPCSTR szNameStartA;
  SIZE_T nNameLen;
  HRESULT hRes;

  //parse tokens
  while ((hRes = GetNextPair(szValueA, &szNameStartA, &nNameLen, cStrValueA)) != S_FALSE)
  {
    if (FAILED(hRes))
      return hRes;

    //process pair
    switch (nNameLen)
    {
      case 3:
        if (StrNCompareA(szNameStartA, "qop", 3) == 0)
        {
          if ((dwFields & 1) != 0)
            return MX_E_InvalidData;
          dwFields |= 1;
          //parse value
          _nQop = ParseQOP((LPCSTR)cStrValueA);
          if (_nQop < 0)
            return MX_E_InvalidData;
          if (_nQop == 2)
            return MX_E_Unsupported;
        }
        break;

      case 5:
        if (StrNCompareA(szNameStartA, "realm", 5) == 0)
        {
          if ((dwFields & 2) != 0)
            return MX_E_InvalidData;
          dwFields |= 2;
          //copy value
          _cStrRealmA.Attach(cStrValueA.Detach());
        }
        else if (StrNCompareA(szNameStartA, "nonce", 5) == 0)
        {
          if ((dwFields & 4) != 0)
            return MX_E_InvalidData;
          dwFields |= 4;
          //copy value
          _cStrNonceA.Attach(cStrValueA.Detach());
        }
        else if (StrNCompareA(szNameStartA, "stale", 5) == 0)
        {
          if ((dwFields & 8) != 0)
            return MX_E_InvalidData;
          dwFields |= 8;
          //parse value
          if (StrCompareA((LPCSTR)cStrValueA, "true", TRUE) == 0 || StrCompareA((LPCSTR)cStrValueA, "1") == 0)
          {
            _bStale = TRUE;
          }
          else if (StrCompareA((LPCSTR)cStrValueA, "false", TRUE) != 0 && StrCompareA((LPCSTR)cStrValueA, "0") != 0)
          {
            return MX_E_InvalidData;
          }
        }
        break;

      case 6:
        if (StrNCompareA(szNameStartA, "domain", 6) == 0)
        {
          if ((dwFields & 16) != 0)
            return MX_E_InvalidData;
          dwFields |= 16;
          //copy value
          _cStrDomainA.Attach(cStrValueA.Detach());
        }
        else if (StrNCompareA(szNameStartA, "opaque", 6) == 0)
        {
          if ((dwFields & 32) != 0)
            return MX_E_InvalidData;
          dwFields |= 32;
          //copy value
          _cStrOpaqueA.Attach(cStrValueA.Detach());
        }
        break;

      case 7:
        if (StrNCompareA(szNameStartA, "charset", 7) == 0)
        {
          if ((dwFields & 64) != 0)
            return MX_E_InvalidData;
          dwFields |= 64;
          //parse value
          if (StrCompareA((LPCSTR)cStrValueA, "utf-8", TRUE) == 0 || StrCompareA((LPCSTR)cStrValueA, "utf8", TRUE) == 0)
          {
            bCharsetIsUtf8 = TRUE;
          }
          else if (StrCompareA((LPCSTR)cStrValueA, "iso-8859-1", TRUE) != 0)
          {
            return E_NOTIMPL;
          }
        }
        break;

      case 8:
        if (StrNCompareA(szNameStartA, "userhash", 8) == 0)
        {
          if ((dwFields & 128) != 0)
            return MX_E_InvalidData;
          dwFields |= 128;
          //parse value
          if (StrCompareA((LPCSTR)cStrValueA, "true", TRUE) == 0 || StrCompareA((LPCSTR)cStrValueA, "1") == 0)
          {
            _bUserHash = TRUE;
          }
          else if (StrCompareA((LPCSTR)cStrValueA, "false", TRUE) != 0 && StrCompareA((LPCSTR)cStrValueA, "0") != 0)
          {
            return MX_E_InvalidData;
          }
        }
        break;

      case 9:
        if (StrNCompareA(szNameStartA, "algorithm", 9) == 0)
        {
          SIZE_T nAlgLen;

          if ((dwFields & 256) != 0)
            return MX_E_InvalidData;
          dwFields |= 256;
          //parse value
          nAlgLen = cStrValueA.GetLength();
          if (nAlgLen >= 5 && StrCompareA((LPCSTR)cStrValueA + nAlgLen - 5, "-sess", TRUE) == 0)
          {
            _bAlgorithmSession = TRUE;
            cStrValueA.Delete(nAlgLen - 5, 5);
          }
          if (StrCompareA((LPCSTR)cStrValueA, "sha-1", TRUE) == 0 || StrCompareA((LPCSTR)cStrValueA, "sha1", TRUE) == 0)
          {
            _nAlgorithm = _ALGORITHM_SHA1;
          }
          else if (StrCompareA((LPCSTR)cStrValueA, "sha-256", TRUE) == 0 ||
                   StrCompareA((LPCSTR)cStrValueA, "sha256", TRUE) == 0)
          {
            _nAlgorithm = _ALGORITHM_SHA256;
          }
          else if (StrCompareA((LPCSTR)cStrValueA, "sha-512", TRUE) == 0 ||
                   StrCompareA((LPCSTR)cStrValueA, "sha512", TRUE) == 0)
          {
            _nAlgorithm = _ALGORITHM_SHA512;
          }
          else if (StrCompareA((LPCSTR)cStrValueA, "md5", TRUE) != 0)
          {
            return E_NOTIMPL;
          }
        }
        break;
    }
  }

  //some checks
  if (_cStrRealmA.IsEmpty() != FALSE)
    return MX_E_InvalidData;
  if (_cStrNonceA.IsEmpty() != FALSE)
    return MX_E_InvalidData;

  //decode
  hRes = Decode(cStrRealmW, (LPCSTR)_cStrRealmA, bCharsetIsUtf8);
  if (FAILED(hRes))
    return hRes;

  cStrNonceA.Attach(_cStrNonceA.Detach());

  hRes = Decode(cStrDomainW, (LPCSTR)_cStrDomainA, bCharsetIsUtf8);
  if (FAILED(hRes))
    return hRes;

  bStale = _bStale;
  bUserHash = _bUserHash;
  nAlgorithm = _nAlgorithm;
  bAlgorithmSession = _bAlgorithmSession;
  nQop = _nQop;

  //done
  return S_OK;
}

HRESULT CHttpAuthDigest::MakeAuthenticateResponse(_Out_ CStringA &cStrDestA, _In_z_ LPCWSTR szUserNameW,
                                                  _In_z_ LPCWSTR szPasswordW, _In_z_ LPCSTR szMethodA,
                                                  _In_z_ LPCSTR szUriPathA, _In_ BOOL bIsProxy)
{
  TAutoDeletePtr<CBaseDigestAlgorithm> cDigAlg;
  CSecureStringA cStrTempA, cStrHashKeyA[2], cStrResponseA, cStrUserNameA;
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

    nCNonce = fnv_64a_buf((LPCSTR)cStrNonceA, cStrNonceA.GetLength(), FNV1A_64_INIT);
    nCNonce = fnv_64a_buf((LPCWSTR)cStrRealmW, cStrRealmW.GetLength() * 2, nCNonce);
    nCNonce = fnv_64a_buf((LPCWSTR)cStrDomainW, cStrDomainW.GetLength() * 2, nCNonce);
    nCNonce = fnv_64a_buf((LPCSTR)cStrOpaqueA, cStrOpaqueA.GetLength(), nCNonce);

    nCNonce = fnv_64a_buf(&bStale, sizeof(bStale), nCNonce);
    nCNonce = fnv_64a_buf(&bAlgorithmSession, sizeof(bAlgorithmSession), nCNonce);
    nCNonce = fnv_64a_buf(&nAlgorithm, sizeof(nAlgorithm), nCNonce);
    nCNonce = fnv_64a_buf(&nQop, sizeof(nQop), nCNonce);
    lp = this;
    nCNonce = fnv_64a_buf(&lp, sizeof(lp), nCNonce);
    dw = ::GetTickCount();
    nCNonce = fnv_64a_buf(&dw, sizeof(dw), nCNonce);

    mx_sprintf_s(szCNonceA, MX_ARRAYLEN(szCNonceA), "%16x", nCNonce);
  }

  //nonce count
  _nNonceCount = _InterlockedIncrement(&nNonceCount);
  mx_sprintf_s(szNonceCountA, MX_ARRAYLEN(szNonceCountA), ":%08lx:", (ULONG)_nNonceCount);

  //HA1 = DIGEST(encoded(username):realm:encoded(password))
  hRes = BeginHashing(&cDigAlg, nAlgorithm);
  if (SUCCEEDED(hRes))
  {
    //username
    if (bUserHash != FALSE)
    {
      TAutoDeletePtr<CBaseDigestAlgorithm> cDigAlgUserHash;

      hRes = BeginHashing(&cDigAlgUserHash, nAlgorithm);
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
          hRes = cDigAlgUserHash->DigestStream((LPCSTR)cStrTempA, cStrTempA.GetLength());

          //add result hash
          if (SUCCEEDED(hRes))
          {
            hRes = cDigAlgUserHash->EndDigest();
            if (SUCCEEDED(hRes))
              hRes = ConvertToHex(cStrUserNameA, cDigAlgUserHash->GetResult(), cDigAlgUserHash->GetResultSize());
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
      hRes = cDigAlg->DigestStream((LPCSTR)cStrUserNameA, cStrUserNameA.GetLength());
    //realm
    if (SUCCEEDED(hRes))
    {
      hRes = cDigAlg->DigestStream(":", 1);
      if (SUCCEEDED(hRes))
      {
        hRes = Encode(cStrTempA, (LPCWSTR)cStrRealmW, FALSE, bCharsetIsUtf8);
        if (SUCCEEDED(hRes))
          hRes = cDigAlg->DigestStream((LPCSTR)cStrTempA, cStrTempA.GetLength());
      }
    }
    //password
    if (SUCCEEDED(hRes))
    {
      hRes = cDigAlg->DigestStream(":", 1);
      if (SUCCEEDED(hRes))
      {
        hRes = Encode(cStrTempA, szPasswordW, FALSE, bCharsetIsUtf8);
        if (SUCCEEDED(hRes))
          hRes = cDigAlg->DigestStream((LPCSTR)cStrTempA, cStrTempA.GetLength());
      }
    }

    //session-ize (HA1 = DIGEST(HA1:nonce:cnonce)
    if (SUCCEEDED(hRes) && bAlgorithmSession != FALSE)
    {
      TAutoDeletePtr<CBaseDigestAlgorithm> cDigAlgSess;

      hRes = BeginHashing(&cDigAlgSess, nAlgorithm);
      if (SUCCEEDED(hRes))
      {
        //HA1
        if (ConvertToHex(cStrTempA, cDigAlg->GetResult(), cDigAlg->GetResultSize()) != FALSE)
          hRes = cDigAlg->DigestStream((LPCSTR)cStrTempA, cStrTempA.GetLength());
        else
          hRes = E_OUTOFMEMORY;
        //nonce
        if (SUCCEEDED(hRes))
        {
          hRes = cDigAlg->DigestStream(":", 1);
          if (SUCCEEDED(hRes))
            hRes = cDigAlg->DigestStream((LPCSTR)cStrNonceA, cStrNonceA.GetLength());
        }
        //cnonce
        if (SUCCEEDED(hRes))
        {
          hRes = cDigAlg->DigestStream(":", 1);
          if (SUCCEEDED(hRes))
            hRes = cDigAlg->DigestStream(szCNonceA, StrLenA(szCNonceA));
        }
        //replace digest
        if (SUCCEEDED(hRes))
          cDigAlg.Attach(cDigAlgSess.Detach());
      }
    }
    if (SUCCEEDED(hRes))
      hRes = cDigAlg->EndDigest();
    //convert result to hex
    if (SUCCEEDED(hRes))
    {
      if (ConvertToHex(cStrHashKeyA[0], cDigAlg->GetResult(), cDigAlg->GetResultSize()) == FALSE)
        hRes = E_OUTOFMEMORY;
    }
  }

  //IMPORTANT: "Auth-int" quality of protection is not supported
  //HA2 = DIGEST(method:path)
  if (SUCCEEDED(hRes))
  {
    cDigAlg.Reset();
    hRes = BeginHashing(&cDigAlg, nAlgorithm);
    if (SUCCEEDED(hRes))
    {
      //method
      hRes = cDigAlg->DigestStream(szMethodA, StrLenA(szMethodA));
      //uri
      if (SUCCEEDED(hRes))
      {
        hRes = cDigAlg->DigestStream(":", 1);
        if (SUCCEEDED(hRes))
          hRes = cDigAlg->DigestStream(szUriPathA, StrLenA(szUriPathA));
      }
    }
    if (SUCCEEDED(hRes))
      hRes = cDigAlg->EndDigest();
    //convert result to hex
    if (SUCCEEDED(hRes))
    {
      if (ConvertToHex(cStrHashKeyA[1], cDigAlg->GetResult(), cDigAlg->GetResultSize()) == FALSE)
        hRes = E_OUTOFMEMORY;
    }
  }

  //build response value
  if (SUCCEEDED(hRes))
  {
    cDigAlg.Reset();
    hRes = BeginHashing(&cDigAlg, nAlgorithm);
    if (SUCCEEDED(hRes))
    {
      //HA1
      hRes = cDigAlg->DigestStream((LPCSTR)cStrHashKeyA[0], cStrHashKeyA[0].GetLength());
      //nonce
      if (SUCCEEDED(hRes))
      {
        hRes = cDigAlg->DigestStream(":", 1);
        if (SUCCEEDED(hRes))
          hRes = cDigAlg->DigestStream((LPCSTR)cStrNonceA, cStrNonceA.GetLength());
      }
      //use AUTH qop?
      if (SUCCEEDED(hRes) && (nQop & 1) != 0)
      {
        //nonce count (this has the colon separators at the start and end of string)
        hRes = cDigAlg->DigestStream(szNonceCountA, StrLenA(szNonceCountA));
        //cnonce
        if (SUCCEEDED(hRes))
          hRes = cDigAlg->DigestStream(szCNonceA, StrLenA(szCNonceA));
        //qop
        if (SUCCEEDED(hRes))
          hRes = cDigAlg->DigestStream(":auth", 5);
      }
      //HA2
      if (SUCCEEDED(hRes))
      {
        hRes = cDigAlg->DigestStream(":", 1);
        if (SUCCEEDED(hRes))
          hRes = cDigAlg->DigestStream((LPCSTR)cStrHashKeyA[1], cStrHashKeyA[1].GetLength());
      }
      if (SUCCEEDED(hRes))
        hRes = cDigAlg->EndDigest();
      //convert result to hex
      if (SUCCEEDED(hRes))
      {
        if (ConvertToHex(cStrResponseA, cDigAlg->GetResult(), cDigAlg->GetResultSize()) == FALSE)
          hRes = E_OUTOFMEMORY;
      }
    }
  }

  //build response
  if (SUCCEEDED(hRes))
  {
    if ((bIsProxy != FALSE && cStrDestA.CopyN("Proxy-", 6) == FALSE) ||
        cStrDestA.ConcatN("Authorization: Digest ", 22) == FALSE)
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

BOOL CHttpAuthDigest::IsSameThan(_In_ CHttpBaseAuth *_lpCompareTo)
{
  CHttpAuthDigest *lpCompareTo;

  if (_lpCompareTo == NULL || _lpCompareTo->GetType() != GetType())
    return FALSE;

  lpCompareTo = (CHttpAuthDigest*)_lpCompareTo;

  if (bStale != FALSE)
    return FALSE;

  if (lpCompareTo->bCharsetIsUtf8 != bCharsetIsUtf8)
    return FALSE;
  if (lpCompareTo->bAlgorithmSession != bAlgorithmSession)
    return FALSE;
  if (lpCompareTo->bUserHash != bUserHash)
    return FALSE;

  if (lpCompareTo->nAlgorithm != nAlgorithm)
    return FALSE;
  if (lpCompareTo->nQop != nQop)
    return FALSE;

  if (StrCompareW((LPCWSTR)(lpCompareTo->cStrRealmW), (LPCWSTR)cStrRealmW) != 0)
    return FALSE;
  if (StrCompareW((LPCWSTR)(lpCompareTo->cStrDomainW), (LPCWSTR)cStrDomainW) != 0)
    return FALSE;
  if (StrCompareA((LPCSTR)(lpCompareTo->cStrNonceA), (LPCSTR)cStrNonceA) != 0)
    return FALSE;
  if (StrCompareA((LPCSTR)(lpCompareTo->cStrOpaqueA), (LPCSTR)cStrOpaqueA) != 0)
    return FALSE;

  return TRUE;
}

} //namespace MX

//-----------------------------------------------------------

static HRESULT BeginHashing(_Out_ MX::CBaseDigestAlgorithm **lplpDigAlg, _In_ int nAlgorithm)
{
  HRESULT hRes;

  if (nAlgorithm == _ALGORITHM_SHA1 || nAlgorithm == _ALGORITHM_SHA256 || nAlgorithm == _ALGORITHM_SHA512)
  {
    MX::CDigestAlgorithmSecureHash *lpDigAlg;

    lpDigAlg = MX_DEBUG_NEW MX::CDigestAlgorithmSecureHash();
    if (lpDigAlg == NULL)
      return E_OUTOFMEMORY;
    hRes = lpDigAlg->BeginDigest((nAlgorithm == _ALGORITHM_SHA1) ? MX::CDigestAlgorithmSecureHash::AlgorithmSHA1
                                 :
                                 ((nAlgorithm == _ALGORITHM_SHA256) ? MX::CDigestAlgorithmSecureHash::AlgorithmSHA256
                                 : MX::CDigestAlgorithmSecureHash::AlgorithmSHA512)
    );
    if (SUCCEEDED(hRes))
      *lplpDigAlg = lpDigAlg;
    else
      delete lpDigAlg;
  }
  else
  {
    MX::CDigestAlgorithmMessageDigest *lpDigAlg;

    lpDigAlg = MX_DEBUG_NEW MX::CDigestAlgorithmMessageDigest();
    if (lpDigAlg == NULL)
      return E_OUTOFMEMORY;
    hRes = lpDigAlg->BeginDigest(MX::CDigestAlgorithmMessageDigest::AlgorithmMD5);
    if (SUCCEEDED(hRes))
      *lplpDigAlg = lpDigAlg;
    else
      delete lpDigAlg;
  }
  return hRes;
}

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

static int ParseQOP(_In_opt_z_ LPCSTR szValueA)
{
  int res = 0;

  while (*szValueA != 0)
  {
    while (*szValueA == ' ')
      szValueA++;
    if (MX::StrNCompareA(szValueA, "auth-int", 8, TRUE) == 0 &&
        (szValueA[8] == 0 || szValueA[8] == ',' || szValueA[8] == ' '))
    {
      szValueA += 8;
      while (*szValueA == ' ')
        szValueA++;
      if (*szValueA != 0 && *szValueA != ',')
        return -1; //invalid data
      res |= 2;
    }
    else if (MX::StrNCompareA(szValueA, "auth", 4, TRUE) == 0 &&
             (szValueA[4] == 0 || szValueA[4] == ',' || szValueA[4] == ' '))
    {
      szValueA += 4;
      while (*szValueA == ' ')
        szValueA++;
      if (*szValueA != 0 && *szValueA != ',')
        return -1; //invalid data
      res |= 1;
    }
    else
    {
      while (*szValueA != 0 && *szValueA != ',')
        szValueA++;
    }
    if (*szValueA == ',')
      szValueA++;
  }
  //done
  return res;
}

static LPCSTR SkipSpaces(_In_z_ LPCSTR sA)
{
  while (*sA == ' ' || *sA == '\t')
    sA++;
  return sA;
}

static LPCSTR GetToken(_In_z_ LPCSTR sA, _In_opt_z_ LPCSTR szStopCharsA)
{
  while (*sA != 0)
  {
    if (MX::CHttpCommon::IsValidNameChar(*sA) == FALSE)
      break;
    if (szStopCharsA != NULL && MX::StrChrA(szStopCharsA, *sA) != NULL)
      break;
    sA++;
  }
  return sA;
}

static LPCSTR Advance(_In_z_ LPCSTR sA, _In_opt_z_ LPCSTR szStopCharsA)
{
  while (*sA != 0)
  {
    if (szStopCharsA != NULL && MX::StrChrA(szStopCharsA, *sA) != NULL)
      break;
    sA++;
  }
  return sA;
}

static HRESULT GetNextPair(_Inout_ LPCSTR &szValueA, _Out_ LPCSTR *lpszNameStartA, _Out_ SIZE_T *lpnNameLen,
                           _Out_ MX::CStringA &cStrValueA)
{
  LPCSTR szValueStartA;

loop:
  if (*szValueA == 0)
    return S_FALSE;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //get key
  if (*szValueA == ',')
  {
    szValueA++;
    goto loop;
  }
  //parse name
  szValueA = GetToken(*lpszNameStartA = szValueA);
  if (*lpszNameStartA == szValueA)
    return MX_E_InvalidData;
  *lpnNameLen = (SIZE_T)(szValueA - (*lpszNameStartA));
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //is equal sign?
  if (*szValueA++ != '=')
    return MX_E_InvalidData;
  //skip spaces
  szValueA = SkipSpaces(szValueA);
  //parse value
  if (*szValueA == '"')
  {
    szValueA = Advance(szValueStartA = ++szValueA, "\"");
    if (*szValueA != '"')
      return MX_E_InvalidData;
    if (cStrValueA.CopyN(szValueStartA, (SIZE_T)(szValueA - szValueStartA)) == FALSE)
      return E_OUTOFMEMORY;
    szValueA++;
  }
  else
  {
    szValueA = Advance(szValueStartA = szValueA, " \t;,");
    if (cStrValueA.CopyN(szValueStartA, (SIZE_T)(szValueA - szValueStartA)) == FALSE)
      return E_OUTOFMEMORY;
  }
  return S_OK;
}

static HRESULT Decode(_Inout_ MX::CStringW &cStrW, _In_z_ LPCSTR szValueA, _In_ BOOL bIsUTF8)
{
  SIZE_T nValueLen = MX::StrLenA(szValueA);
  int nDestChars;

  if (bIsUTF8 != FALSE)
    return MX::Utf8_Decode(cStrW, szValueA, nValueLen, FALSE);

  cStrW.Empty();
  nDestChars = ::MultiByteToWideChar(28591, MB_PRECOMPOSED, szValueA, (int)nValueLen, NULL, 0);
  if (nDestChars > 0)
  {
    int nBufSize = nDestChars;

    if (cStrW.EnsureBuffer(nBufSize) == FALSE)
      return E_OUTOFMEMORY;
    nDestChars = ::MultiByteToWideChar(28591, MB_PRECOMPOSED, szValueA, (int)nValueLen, (LPWSTR)cStrW, nBufSize);
    if (nDestChars < 0)
      nDestChars = 0;
    ((LPWSTR)cStrW)[(SIZE_T)nDestChars] = 0;
    cStrW.Refresh();
  }
  //done
  return S_OK;
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
