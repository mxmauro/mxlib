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

//-----------------------------------------------------------

#define SESSION_ID_COOKIE_NAME                 "mxsessionid"

//-----------------------------------------------------------

namespace MX {

CJsHttpServerSessionPlugin::CJsHttpServerSessionPlugin() : CJsObjectBase(), CNonCopyableObj()
{
  *szCurrentIdA = 0;
  nExpireTimeInSeconds = 0;
  bIsSecure = bIsHttpOnly = FALSE;
  szSessionVarNameA = SESSION_ID_COOKIE_NAME;
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
  HRESULT hRes = S_OK;

  if (*szCurrentIdA != 0 && bDirty != FALSE)
  {
    hRes = cLoadSaveCallback(this, FALSE);
    if (SUCCEEDED(hRes))
      bDirty = FALSE;
  }
  return hRes;
}

HRESULT CJsHttpServerSessionPlugin::Setup(_In_ CJsHttpServer::CClientRequest *_lpRequest,
                                          _In_ OnLoadSaveCallback _cLoadSaveCallback,
                                          _In_opt_z_ LPCWSTR szSessionVarNameW, _In_opt_z_ LPCWSTR szDomainW,
                                          _In_opt_z_ LPCWSTR szPathW, _In_opt_ int _nExpireTimeInSeconds,
                                          _In_opt_ BOOL _bIsSecure, _In_opt_ BOOL _bIsHttpOnly)
{
  TAutoRefCounted<CHttpCookie> cSessionIdCookie;
  MX::CDateTime cExpireDt;
  SIZE_T i;
  HRESULT hRes;

  if (_lpRequest == NULL || (!_cLoadSaveCallback))
    return E_POINTER;
  if (szSessionVarNameW != NULL && *szSessionVarNameW != 0)
  {
    for (i = 0; szSessionVarNameW[i] != 0; i++)
    {
      if (i > 64)
        return E_INVALIDARG;
      if ((szSessionVarNameW[i] < L'A' || szSessionVarNameW[i] > L'Z') &&
          (szSessionVarNameW[i] < L'a' || szSessionVarNameW[i] > L'z') &&
          (szSessionVarNameW[i] < L'0' || szSessionVarNameW[i] > L'9') &&
          szSessionVarNameW[i] != L'-' && szSessionVarNameW[i] != L'_')
      {
        return E_INVALIDARG;
      }
    }
    if (cStrSessionVarNameA.Copy(szSessionVarNameW) == FALSE)
      return E_OUTOFMEMORY;
    szSessionVarNameA = (LPCSTR)cStrSessionVarNameA;
  }
  else
  {
    szSessionVarNameA = SESSION_ID_COOKIE_NAME;
  }
  lpHttpServer = (CJsHttpServer*)(_lpRequest->GetHttpServer());
  lpRequest = _lpRequest;
  cLoadSaveCallback = _cLoadSaveCallback;
  if (szDomainW != NULL && *szDomainW != 0)
  {
    hRes = Punycode_Encode(cStrDomainA, szDomainW);
    if (FAILED(hRes))
      return hRes;
  }
  if (szPathW != NULL && *szPathW != 0)
  {
    hRes = Utf8_Encode(cStrPathA, szPathW);
    if (FAILED(hRes))
      return hRes;
  }
  nExpireTimeInSeconds = _nExpireTimeInSeconds;
  if (nExpireTimeInSeconds < -1)
    nExpireTimeInSeconds = -1;
  bIsSecure = _bIsSecure;
  bIsHttpOnly = _bIsHttpOnly;

  //initialize session data
  for (i = 0; ; i++)
  {
    cSessionIdCookie.Attach(lpRequest->GetRequestCookie(i));
    if (!cSessionIdCookie)
      break;
    if (StrCompareA(cSessionIdCookie->GetName(), szSessionVarNameA) == 0)
      break;
    cSessionIdCookie.Release();
  }
  if (cSessionIdCookie)
  {
    LPCSTR szValueA = cSessionIdCookie->GetValue();

    if (IsValidSessionId(szValueA) == FALSE)
      goto regenerate_session_id;

    ::MxMemCopy(szCurrentIdA, szValueA, StrLenA(szValueA) + 1);
    hRes = cLoadSaveCallback(this, TRUE);
    if (FAILED(hRes) && hRes != E_OUTOFMEMORY)
      goto regenerate_session_id;
  }
  else
  {
regenerate_session_id:
    cBag.Reset();
    hRes = RegenerateSessionId();
    if (SUCCEEDED(hRes) && nExpireTimeInSeconds >= 0)
    {
      hRes = cExpireDt.SetFromNow(FALSE);
      if (SUCCEEDED(hRes))
        hRes = cExpireDt.Add(nExpireTimeInSeconds, MX::CDateTime::UnitsSeconds);
    }
    if (SUCCEEDED(hRes))
    {
      lpRequest->RemoveResponseCookie(szSessionVarNameA);
      hRes = lpRequest->AddResponseCookie(szSessionVarNameA, szCurrentIdA, (LPCSTR)cStrDomainA,
                                          (LPCSTR)cStrPathA, (nExpireTimeInSeconds >= 0) ? &cExpireDt : NULL,
                                          bIsSecure, bIsHttpOnly);
    }
  }
  //done
  return hRes;
}

CPropertyBag* CJsHttpServerSessionPlugin::GetBag() const
{
  return const_cast<CPropertyBag*>(&cBag);
}

BOOL CJsHttpServerSessionPlugin::IsValidSessionId(_In_z_ LPCSTR szSessionIdA)
{
  SIZE_T i;

  for (i=0; i<40; i++)
  {
    if ((szSessionIdA[i] < '0' || szSessionIdA[i] > '9') && (szSessionIdA[i] < 'A' || szSessionIdA[i] > 'F'))
      return FALSE;
  }
  return (szSessionIdA[40] == 0) ? TRUE : FALSE;
}

HRESULT CJsHttpServerSessionPlugin::RegenerateSessionId()
{
  CMessageDigest cDigest;
  ULARGE_INTEGER liSysTime;
  CSockets *lpSckMgr;
  HANDLE hConn;
  SOCKADDR_INET sAddr;
  DWORD dw;
  HRESULT hRes;

  *szCurrentIdA = 0;
  //----
  hConn = lpRequest->GetUnderlyingSocketHandle();
  lpSckMgr = lpRequest->GetUnderlyingSocketManager();
  if (hConn == NULL || lpSckMgr == NULL)
    return E_UNEXPECTED;
  //calculate hash
  hRes = lpSckMgr->GetPeerAddress(hConn, &sAddr);
  if (SUCCEEDED(hRes))
    hRes = cDigest.BeginDigest(CMessageDigest::AlgorithmSHA1);
  if (SUCCEEDED(hRes))
  {
    switch (sAddr.si_family)
    {
      case AF_INET:
        hRes = cDigest.DigestStream(&(sAddr.Ipv4.sin_addr), sizeof(sAddr.Ipv4.sin_addr));
        break;
      case AF_INET6:
        hRes = cDigest.DigestStream(&(sAddr.Ipv6.sin6_addr), sizeof(sAddr.Ipv6.sin6_addr));
        break;
      default:
        hRes = E_NOTIMPL;
        break;
    }
  }
  if (SUCCEEDED(hRes))
  {
    MxNtQuerySystemTime(&liSysTime);
    hRes = cDigest.DigestStream(&liSysTime, sizeof(liSysTime));
  }
  if (SUCCEEDED(hRes))
  {
    dw = MxGetCurrentProcessId();
    hRes = cDigest.DigestStream(&dw, sizeof(dw));
  }
  if (SUCCEEDED(hRes))
  {
    dw = MxGetCurrentThreadId();
    hRes = cDigest.DigestStream(&dw, sizeof(dw));
  }
  if (SUCCEEDED(hRes))
    hRes = cDigest.DigestStream(&lpRequest, sizeof(lpRequest));
  if (SUCCEEDED(hRes))
    hRes = cDigest.EndDigest();
  if (SUCCEEDED(hRes))
  {
    static LPCSTR szHexaA = "0123456789ABCDEF";
    LPBYTE lpResult = cDigest.GetResult();
    for (dw=0; dw<20; dw++)
    {
      szCurrentIdA[ dw<<1   ] = szHexaA[(lpResult[dw] >> 4) & 0x0F];
      szCurrentIdA[(dw<<1)+1] = szHexaA[ lpResult[dw]       & 0x0F];
    }
    szCurrentIdA[40] = 0;
  }
  //done
  return S_OK;
}

DukTape::duk_ret_t CJsHttpServerSessionPlugin::_Save(_In_ DukTape::duk_context *lpCtx)
{
  HRESULT hRes;

  hRes = Save();
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  return 0;
}

DukTape::duk_ret_t CJsHttpServerSessionPlugin::RegenerateId(_In_ DukTape::duk_context *lpCtx)
{
  MX::CDateTime cExpireDt;
  HRESULT hRes;

  hRes = RegenerateSessionId();
  if (SUCCEEDED(hRes) && nExpireTimeInSeconds >= 0)
  {
    hRes = cExpireDt.SetFromNow(FALSE);
    if (SUCCEEDED(hRes))
      hRes = cExpireDt.Add(nExpireTimeInSeconds, MX::CDateTime::UnitsSeconds);
  }
  if (SUCCEEDED(hRes))
  {
    lpRequest->RemoveResponseCookie(szSessionVarNameA);
    hRes = lpRequest->AddResponseCookie(szSessionVarNameA, szCurrentIdA, (LPCSTR)cStrDomainA,
                                        (LPCSTR)cStrPathA, (nExpireTimeInSeconds >= 0) ? &cExpireDt : NULL,
                                        bIsSecure, bIsHttpOnly);
  }
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);
  //on success
  cBag.Reset();
  bDirty = TRUE;
  return 0;
}

int CJsHttpServerSessionPlugin::OnProxyHasNamedProperty(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szPropNameA)
{
  if (szPropNameA[0] == 'i' && szPropNameA[1] == 'd' && szPropNameA[2] == 0)
    return 1;
  switch (cBag.GetType(szPropNameA))
  {
    case CPropertyBag::PropertyTypeNull:
    case CPropertyBag::PropertyTypeDouble:
    case CPropertyBag::PropertyTypeAnsiString:
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
    case CPropertyBag::PropertyTypeNull:
      DukTape::duk_push_null(lpCtx);
      return 1;

    case CPropertyBag::PropertyTypeDouble:
      if (FAILED(cBag.GetDouble(szPropNameA, nDblValue)))
        break;
      DukTape::duk_push_number(lpCtx, nDblValue);
      return 1;

    case CPropertyBag::PropertyTypeAnsiString:
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
