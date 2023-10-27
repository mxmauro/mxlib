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
#include "..\..\..\Include\JsHttpServer\Plugins\JsHttpServerSessionPlugin.h"
#include "..\..\..\Include\Crypto\MessageDigest.h"
#include "..\..\..\Include\Http\punycode.h"
#include "..\..\..\Include\Strings\Utf8.h"
#include "..\..\..\Include\Crypto\SecureRandom.h"

//-----------------------------------------------------------

#define SESSION_ID_COOKIE_NAME                 "mxsessionid"

//-----------------------------------------------------------

namespace MX {

CJsHttpServerSessionPlugin::CJsHttpServerSessionPlugin() : CJsObjectBase(), CNonCopyableObj()
{
  ::MxMemSet(szCurrentIdA, 0, sizeof(szCurrentIdA));
  nExpireTimeInSeconds = 0;
  bIsSecure = bIsHttpOnly = FALSE;
  nSameSite = CHttpCookie::eSameSite::None;
  szSessionVarNameA = SESSION_ID_COOKIE_NAME;
  cPersistanceCallback = NullCallback();
  bDirty = FALSE;
  return;
}

CJsHttpServerSessionPlugin::~CJsHttpServerSessionPlugin()
{
  Save();
  return;
}

HRESULT CJsHttpServerSessionPlugin::Save()
{
  HRESULT hRes;

  if ((!cPersistanceCallback) || bDirty == FALSE)
    return S_OK;

  hRes = cPersistanceCallback(this, ePersistanceOption::Save);
  if (SUCCEEDED(hRes))
    bDirty = FALSE;
  return hRes;
}

VOID CJsHttpServerSessionPlugin::Destroy()
{
  HRESULT hRes = S_OK;

  if (cPersistanceCallback)
  {
    cPersistanceCallback(this, ePersistanceOption::Delete);
  }
  cBag.Reset();
  bDirty = FALSE;
  return;
}

HRESULT CJsHttpServerSessionPlugin::Setup(_In_ CJsHttpServer::CClientRequest *_lpRequest,
                                          _In_ OnPersistanceCallback _cPersistanceCallback,
                                          _In_opt_z_ LPCWSTR szSessionVarNameW, _In_opt_z_ LPCWSTR szDomainW,
                                          _In_opt_z_ LPCWSTR szPathW, _In_opt_ int _nExpireTimeInSeconds,
                                          _In_opt_ BOOL _bIsSecure, _In_opt_ BOOL _bIsHttpOnly,
                                          _In_opt_ CHttpCookie::eSameSite _nSameSite)
{
  SIZE_T i;
  HRESULT hRes;

  if (_lpRequest == NULL || (!_cPersistanceCallback))
  {
    hRes = E_POINTER;

on_error:
    ::MxMemSet(szCurrentIdA, 0, sizeof(szCurrentIdA));
    nExpireTimeInSeconds = 0;
    bIsSecure = bIsHttpOnly = FALSE;
    nSameSite = CHttpCookie::eSameSite::None;
    szSessionVarNameA = SESSION_ID_COOKIE_NAME;
    cPersistanceCallback = NullCallback();
    bDirty = FALSE;

    return hRes;
  }

  if (szSessionVarNameW != NULL && *szSessionVarNameW != 0)
  {
    for (i = 0; szSessionVarNameW[i] != 0; i++)
    {
      if (i > 64)
      {
        hRes = E_INVALIDARG;
        goto on_error;
      }
      if ((szSessionVarNameW[i] < L'A' || szSessionVarNameW[i] > L'Z') &&
          (szSessionVarNameW[i] < L'a' || szSessionVarNameW[i] > L'z') &&
          (szSessionVarNameW[i] < L'0' || szSessionVarNameW[i] > L'9') &&
          szSessionVarNameW[i] != L'-' && szSessionVarNameW[i] != L'_')
      {
        hRes = E_INVALIDARG;
        goto on_error;
      }
    }
    if (cStrSessionVarNameA.Copy(szSessionVarNameW) == FALSE)
    {
      hRes = E_OUTOFMEMORY;
      goto on_error;
    }
    szSessionVarNameA = (LPCSTR)cStrSessionVarNameA;
  }
  else
  {
    szSessionVarNameA = SESSION_ID_COOKIE_NAME;
  }

  lpHttpServer = (CJsHttpServer*)(_lpRequest->GetHttpServer());
  lpRequest = _lpRequest;

  cPersistanceCallback = _cPersistanceCallback;

  if (szDomainW != NULL && *szDomainW != 0)
  {
    hRes = Punycode_Encode(cStrDomainA, szDomainW);
    if (FAILED(hRes))
      goto on_error;
  }
  if (szPathW != NULL && *szPathW != 0)
  {
    hRes = Utf8_Encode(cStrPathA, szPathW);
    if (FAILED(hRes))
      goto on_error;
  }

  nExpireTimeInSeconds = _nExpireTimeInSeconds;
  if (nExpireTimeInSeconds < -1)
    nExpireTimeInSeconds = -1;

  bIsSecure = _bIsSecure;
  bIsHttpOnly = _bIsHttpOnly;
  nSameSite = _nSameSite;

  //initialize session data
  for (i = 0; ; i++)
  {
    MX::TAutoRefCounted<CHttpCookie> cCookie;

    cCookie.Attach(lpRequest->GetRequestCookie(i));
    if (!cCookie)
      break;

    if (StrCompareA(cCookie->GetName(), szSessionVarNameA) == 0)
    {
      LPCSTR szValueA = cCookie->GetValue();

      if (IsValidSessionId(szValueA) != FALSE)
      {
        ::MxMemCopy(szCurrentIdA, szValueA, StrLenA(szValueA) + 1);
        hRes = cPersistanceCallback(this, ePersistanceOption::Load);
        if (SUCCEEDED(hRes))
          return S_OK;
        if (hRes == E_OUTOFMEMORY)
          goto on_error;
        //else fall into session regeneration
      }

      break;
    }
  }

  //if we reach here, create a new session id
  cBag.Reset();
  GenerateSessionId();
  hRes = CreateRequestCookie();
  if (FAILED(hRes))
    goto on_error;

  //done
  return S_OK;
}

CPropertyBag* CJsHttpServerSessionPlugin::GetBag() const
{
  return const_cast<CPropertyBag*>(&cBag);
}

BOOL CJsHttpServerSessionPlugin::IsValidSessionId(_In_z_ LPCSTR szSessionIdA)
{
  for (SIZE_T i = 0; i < 64; i++)
  {
    if ((szSessionIdA[i] < '0' || szSessionIdA[i] > '9') && (szSessionIdA[i] < 'A' || szSessionIdA[i] > 'F'))
      return FALSE;
  }
  return (szSessionIdA[64] == 0) ? TRUE : FALSE;
}

VOID CJsHttpServerSessionPlugin::GenerateSessionId()
{
  static LPCSTR szHexaA = "0123456789ABCDEF";
  CMessageDigest cDigest;
  BYTE aBuf[64], *lpResult;
  DWORD dw;
  HRESULT hRes;

  //calculate hash
  hRes = cDigest.BeginDigest(CMessageDigest::eAlgorithm::SHA256);
  if (SUCCEEDED(hRes))
  {
    CSockets *lpSckMgr;
    HANDLE hConn;
    SOCKADDR_INET sAddr;

    hConn = lpRequest->GetUnderlyingSocketHandle();
    lpSckMgr = lpRequest->GetUnderlyingSocketManager();
    if (hConn != NULL && lpSckMgr != NULL && SUCCEEDED(lpSckMgr->GetPeerAddress(hConn, &sAddr)))
    {
      switch (sAddr.si_family)
      {
        case AF_INET:
          hRes = cDigest.DigestStream(&(sAddr.Ipv4.sin_addr), sizeof(sAddr.Ipv4.sin_addr));
          break;

        case AF_INET6:
          hRes = cDigest.DigestStream(&(sAddr.Ipv6.sin6_addr), sizeof(sAddr.Ipv6.sin6_addr));
          break;
      }
    }
    if (SUCCEEDED(hRes))
    {
      ULARGE_INTEGER liSysTime;

      ::MxNtQuerySystemTime(&liSysTime);
      hRes = cDigest.DigestStream(&liSysTime, sizeof(liSysTime));
    }
    if (SUCCEEDED(hRes))
    {
      dw = ::MxGetCurrentProcessId();
      hRes = cDigest.DigestStream(&dw, sizeof(dw));
      if (SUCCEEDED(hRes))
      {
        dw = ::MxGetCurrentThreadId();
        hRes = cDigest.DigestStream(&dw, sizeof(dw));
        if (SUCCEEDED(hRes))
        {
          hRes = cDigest.DigestStream(&lpRequest, sizeof(lpRequest));

          if (SUCCEEDED(hRes))
          {
            BYTE aBuf[64];

            MX::SecureRandom::Generate(aBuf, sizeof(aBuf));
            hRes = cDigest.DigestStream(aBuf, sizeof(aBuf));

            if (SUCCEEDED(hRes))
              hRes = cDigest.EndDigest();
          }
        }
      }
    }
  }

  //get result
  if (SUCCEEDED(hRes))
  {
    lpResult = cDigest.GetResult();
  }
  else
  {
    MX::SecureRandom::Generate(aBuf, 32);
    lpResult = aBuf;
  }

  for (dw = 0; dw < 32; dw++)
  {
    szCurrentIdA[ dw << 1     ] = szHexaA[(lpResult[dw] >> 4) & 0x0F];
    szCurrentIdA[(dw << 1) + 1] = szHexaA[ lpResult[dw]       & 0x0F];
  }
  szCurrentIdA[64] = 0;

  //done
  return;
}

HRESULT CJsHttpServerSessionPlugin::CreateRequestCookie()
{
  MX::CDateTime cExpireDt;
  HRESULT hRes;

  if (nExpireTimeInSeconds >= 0)
  {
    hRes = cExpireDt.SetFromNow(FALSE);
    if (FAILED(hRes))
      return hRes;

    hRes = cExpireDt.Add(nExpireTimeInSeconds, MX::CDateTime::eUnits::Seconds);
    if (FAILED(hRes))
      return hRes;
  }

  lpRequest->RemoveResponseCookie(szSessionVarNameA);
  hRes = lpRequest->AddResponseCookie(szSessionVarNameA, szCurrentIdA, (LPCSTR)cStrDomainA,
                                      (LPCSTR)cStrPathA, ((nExpireTimeInSeconds >= 0) ? &cExpireDt : NULL),
                                      bIsSecure, bIsHttpOnly, nSameSite);

  //done
  return hRes;
}

DukTape::duk_ret_t CJsHttpServerSessionPlugin::_Save(_In_ DukTape::duk_context *lpCtx)
{
  HRESULT hRes;

  hRes = Save();
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  return 0;
}

DukTape::duk_ret_t CJsHttpServerSessionPlugin::_Destroy(_In_ DukTape::duk_context *lpCtx)
{
  Destroy();
  return 0;
}

DukTape::duk_ret_t CJsHttpServerSessionPlugin::RegenerateId(_In_ DukTape::duk_context *lpCtx)
{
  HRESULT hRes;

  if (!cPersistanceCallback)
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, MX_E_NotReady);

  cPersistanceCallback(this, ePersistanceOption::Delete);
  GenerateSessionId();
  hRes = CreateRequestCookie();
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);

  //on success
  bDirty = TRUE;
  return 0;
}

int CJsHttpServerSessionPlugin::OnProxyHasNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szPropNameA)
{
  if (szPropNameA[0] == 'i' && szPropNameA[1] == 'd' && szPropNameA[2] == 0)
    return 1;
  switch (cBag.GetType(szPropNameA))
  {
    case CPropertyBag::eType::Null:
    case CPropertyBag::eType::Double:
    case CPropertyBag::eType::AnsiString:
      return 1;
  }
  return 0;
}

int CJsHttpServerSessionPlugin::OnProxyHasIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_ int nIndex)
{
  return 0; //throw error
}

int CJsHttpServerSessionPlugin::OnProxyGetNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szPropNameA)
{
  LPCSTR szValueA;
  double nDblValue;

  if (szPropNameA[0] == 'i' && szPropNameA[1] == 'd' && szPropNameA[2] == 0)
  {
    DukTape::duk_push_string(lpCtx, szCurrentIdA);
    return 1;
  }
  switch (cBag.GetType(szPropNameA))
  {
    case CPropertyBag::eType::Null:
      DukTape::duk_push_null(lpCtx);
      return 1;

    case CPropertyBag::eType::Double:
      if (FAILED(cBag.GetDouble(szPropNameA, nDblValue)))
        break;
      DukTape::duk_push_number(lpCtx, nDblValue);
      return 1;

    case CPropertyBag::eType::AnsiString:
      if (FAILED(cBag.GetString(szPropNameA, szValueA)))
        break;
      DukTape::duk_push_string(lpCtx, szValueA);
      return 1;
  }
  return 0; //pass original
}

int CJsHttpServerSessionPlugin::OnProxyGetIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_ int nIndex)
{
  return -1; //throw error
}

int CJsHttpServerSessionPlugin::OnProxySetNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szPropNameA,
                                                        _In_ DukTape::duk_idx_t nValueIndex)
{
  HRESULT hRes;

  if (szPropNameA[0] == 'i' && szPropNameA[1] == 'd' && szPropNameA[2] == 0)
    return -1; //cannot set ID property
  if (DukTape::duk_is_undefined(lpCtx, nValueIndex) != 0)
  {
    hRes = cBag.Clear(szPropNameA);
    bDirty = TRUE;
    return 0;
  }
  if (DukTape::duk_is_null(lpCtx, nValueIndex) != 0)
  {
    hRes = cBag.SetNull(szPropNameA);
    if (SUCCEEDED(hRes))
    {
      bDirty = TRUE;
      return 0;
    }
  }
  else if (DukTape::duk_is_boolean(lpCtx, nValueIndex) != 0)
  {
    hRes = cBag.SetDouble(szPropNameA, DukTape::duk_get_boolean(lpCtx, nValueIndex) ? 1.0 : 0.0);
    return (SUCCEEDED(hRes)) ? 0 : -1; if (SUCCEEDED(hRes))
    {
      bDirty = TRUE;
      return 0;
    }
  }
  else if (DukTape::duk_is_number(lpCtx, nValueIndex) != 0)
  {
    hRes = cBag.SetDouble(szPropNameA, DukTape::duk_get_number(lpCtx, nValueIndex));
    if (SUCCEEDED(hRes))
    {
      bDirty = TRUE;
      return 0;
    }
  }
  else if (DukTape::duk_is_string(lpCtx, nValueIndex) != 0)
  {
    hRes = cBag.SetString(szPropNameA, DukTape::duk_get_string(lpCtx, nValueIndex));
    if (SUCCEEDED(hRes))
    {
      bDirty = TRUE;
      return 0;
    }
  }
  return -1; //throw error
}

int CJsHttpServerSessionPlugin::OnProxySetIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_ int nIndex,
                                                          _In_ DukTape::duk_idx_t nValueIndex)
{
  return -1; //throw error
}

int CJsHttpServerSessionPlugin::OnProxyDeleteNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szPropNameA)
{
  if (szPropNameA[0] == 'i' && szPropNameA[1] == 'd' && szPropNameA[2] == 0)
    return -1; //cannot delete property
  if (SUCCEEDED(cBag.Clear(szPropNameA)))
  {
    bDirty = TRUE;
  }
  return 1;
}

int CJsHttpServerSessionPlugin::OnProxyDeleteIndexedProperty(_In_ DukTape::duk_context *lpCtx, _In_ int nIndex)
{
  return -1; //throw error
}

} //namespace MX
