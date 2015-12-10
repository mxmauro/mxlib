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
#include "..\..\..\Include\JsHttpServer\Plugins\JsHttpServerSessionPlugin.h"
#include "..\..\..\Include\Crypto\DigestAlgorithmSHAx.h"

//-----------------------------------------------------------

#define SESSION_ID_COOKIE_NAME                 "mxsessionid"

//-----------------------------------------------------------

namespace MX {

CJsHttpServerSessionPlugin::CJsHttpServerSessionPlugin(__in DukTape::duk_context *lpCtx) : CJsObjectBase(lpCtx)
{
  *szCurrentIdA = 0;
  return;
}

CJsHttpServerSessionPlugin::~CJsHttpServerSessionPlugin()
{
  _Save();
  return;
}

HRESULT CJsHttpServerSessionPlugin::Setup(__in CJavascriptVM &cJvm, __in CJsHttpServer *_lpHttpServer,
                                          __in CHttpServer::CRequest *_lpRequest,
                                          __in OnLoadSaveCallback _cLoadSaveCallback)
{
  CHttpCookie *lpSessionIdCookie;
  SIZE_T i, nCount;
  HRESULT hRes;

  if (_lpHttpServer == NULL || _lpRequest == NULL || (!_cLoadSaveCallback))
    return E_POINTER;

  lpHttpServer = _lpHttpServer;
  lpRequest = _lpRequest;
  lpJvm = &cJvm;
  cLoadSaveCallback = _cLoadSaveCallback;

  //initialize session data
  lpSessionIdCookie = NULL;
  nCount = lpRequest->GetRequestCookiesCount();
  for (i=0; i<nCount; i++)
  {
    lpSessionIdCookie = lpRequest->GetRequestCookie(i);
    if (StrCompareA(lpSessionIdCookie->GetName(), SESSION_ID_COOKIE_NAME) == 0)
      break;
  }
  if (i < nCount && IsValidSessionId(lpSessionIdCookie->GetValue()) != FALSE)
  {
    MemCopy(szCurrentIdA, lpSessionIdCookie->GetValue(), StrLenA(lpSessionIdCookie->GetValue())+1);
    
    hRes = cLoadSaveCallback(this, TRUE);
    if (FAILED(hRes) && hRes != E_OUTOFMEMORY)
    {
      cBag.Reset();
      hRes = S_OK;
    }
  }
  else
  {
    hRes = RegenerateSessionId();
    if (SUCCEEDED(hRes))
      hRes = lpRequest->AddResponseCookie(SESSION_ID_COOKIE_NAME, szCurrentIdA);
  }
  if (FAILED(hRes))
    return hRes;

  //done
  return S_OK;
}

LPCSTR CJsHttpServerSessionPlugin::GetSessionId() const
{
  return szCurrentIdA;
}

CJsHttpServer* CJsHttpServerSessionPlugin::GetServer() const
{
  return lpHttpServer;
}

CHttpServer::CRequest* CJsHttpServerSessionPlugin::GetRequest() const
{
  return lpRequest;
}

CPropertyBag* CJsHttpServerSessionPlugin::GetBag() const
{
  return const_cast<CPropertyBag*>(&cBag);
}

BOOL CJsHttpServerSessionPlugin::IsValidSessionId(__in_z LPCSTR szSessionIdA)
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
  CDigestAlgorithmSecureHash cDigest;
  ULARGE_INTEGER liSysTime;
  CSockets *lpSckMgr;
  HANDLE hConn;
  sockaddr sAddr;
  DWORD dw;
  HRESULT hRes;

  *szCurrentIdA = 0;
  //----
  hConn = lpRequest->GetUnderlyingSocket();
  lpSckMgr = lpRequest->GetUnderlyingSocketManager();
  if (hConn == NULL || lpSckMgr == NULL)
    return E_UNEXPECTED;
  //calculate hash
  hRes = lpSckMgr->GetPeerAddress(hConn, &sAddr);
  if (SUCCEEDED(hRes))
    hRes = cDigest.BeginDigest(CDigestAlgorithmSecureHash::AlgorithmSHA1);
  if (SUCCEEDED(hRes))
  {
    switch (sAddr.sa_family)
    {
      case AF_INET:
        hRes = cDigest.DigestStream(&(((sockaddr_in*)&sAddr)->sin_addr), sizeof(((sockaddr_in*)&sAddr)->sin_addr));
        break;
      case AF_INET6:
        hRes = cDigest.DigestStream(&(((sockaddr_in6_w2ksp1*)&sAddr)->sin6_addr),
                                    sizeof(((sockaddr_in6_w2ksp1*)&sAddr)->sin6_addr));
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

HRESULT CJsHttpServerSessionPlugin::_Save()
{
  return cLoadSaveCallback(this, FALSE);
}

DukTape::duk_ret_t CJsHttpServerSessionPlugin::Save(__in DukTape::duk_context *lpCtx)
{
  DukTape::duk_push_boolean(lpCtx, (SUCCEEDED(_Save())) ? 1 : 0);
  return 1;
}

DukTape::duk_ret_t CJsHttpServerSessionPlugin::RegenerateId(__in DukTape::duk_context *lpCtx)
{
  HRESULT hRes;

  hRes = RegenerateSessionId();
  if (SUCCEEDED(hRes))
    hRes = lpRequest->AddResponseCookie(SESSION_ID_COOKIE_NAME, szCurrentIdA);
  if (FAILED(hRes))
    return CJavascriptVM::DukTapeRetFromHResult(hRes);
  //on success
  cBag.Reset();
  return 0;
}

int CJsHttpServerSessionPlugin::OnProxyHasNamedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szPropNameA)
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

int CJsHttpServerSessionPlugin::OnProxyHasIndexedProperty(__in DukTape::duk_context *lpCtx, __in int nIndex)
{
  return 0; //throw error
}

int CJsHttpServerSessionPlugin::OnProxyGetNamedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szPropNameA)
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

int CJsHttpServerSessionPlugin::OnProxyGetIndexedProperty(__in DukTape::duk_context *lpCtx, __in int nIndex)
{
  return -1; //throw error
}

int CJsHttpServerSessionPlugin::OnProxySetNamedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szPropNameA,
                                                        __in DukTape::duk_idx_t nValueIndex)
{
  HRESULT hRes;

  if (szPropNameA[0] == 'i' && szPropNameA[1] == 'd' && szPropNameA[2] == 0)
    return -1; //cannot set ID property
  if (DukTape::duk_is_undefined(lpCtx, nValueIndex) != 0)
  {
    hRes = cBag.Clear(szPropNameA);
    return 0;
  }
  if (DukTape::duk_is_null(lpCtx, nValueIndex) != 0)
  {
    hRes = cBag.SetNull(szPropNameA);
    return (SUCCEEDED(hRes)) ? 0 : -1;
  }
  if (DukTape::duk_is_number(lpCtx, nValueIndex) != 0)
  {
    hRes = cBag.SetDouble(szPropNameA, DukTape::duk_get_number(lpCtx, nValueIndex));
    return (SUCCEEDED(hRes)) ? 0 : -1;
  }
  if (DukTape::duk_is_string(lpCtx, nValueIndex) != 0)
  {
    hRes = cBag.SetString(szPropNameA, DukTape::duk_get_string(lpCtx, nValueIndex));
    return (SUCCEEDED(hRes)) ? 0 : -1;
  }
  return -1; //throw error
}

int CJsHttpServerSessionPlugin::OnProxySetIndexedProperty(__in DukTape::duk_context *lpCtx, __in int nIndex,
                                                          __in DukTape::duk_idx_t nValueIndex)
{
  return -1; //throw error
}

int CJsHttpServerSessionPlugin::OnProxyDeleteNamedProperty(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szPropNameA)
{
  if (szPropNameA[0] == 'i' && szPropNameA[1] == 'd' && szPropNameA[2] == 0)
    return -1; //cannot delete property
  cBag.Clear(szPropNameA);
  return 1;
}

int CJsHttpServerSessionPlugin::OnProxyDeleteIndexedProperty(__in DukTape::duk_context *lpCtx, __in int nIndex)
{
  return -1; //throw error
}

} //namespace MX
