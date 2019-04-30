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
#include "..\..\Include\Comm\Proxy.h"
#include "..\..\Include\WaitableObjects.h"
#include <Winhttp.h>

//-----------------------------------------------------------

typedef HINTERNET (WINAPI *lpfnWinHttpOpen)(_In_opt_z_ LPCWSTR pszAgentW, _In_ DWORD dwAccessType,
                                            _In_opt_z_ LPCWSTR pszProxyW, _In_opt_z_ LPCWSTR pszProxyBypassW,
                                            _In_ DWORD dwFlags);
typedef BOOL (WINAPI *lpfnWinHttpCloseHandle)(_In_ HINTERNET hInternet);
typedef BOOL (WINAPI *lpfnWinHttpGetIEProxyConfigForCurrentUser)(_Inout_
                                                                 WINHTTP_CURRENT_USER_IE_PROXY_CONFIG *pProxyConfig);
typedef BOOL (WINAPI *lpfnWinHttpGetProxyForUrl)(_In_ HINTERNET hSession, _In_z_ LPCWSTR lpcwszUrl,
                                                 _In_ WINHTTP_AUTOPROXY_OPTIONS *pAutoProxyOptions,
                                                 _Out_ WINHTTP_PROXY_INFO *pProxyInfo);
typedef BOOL (WINAPI *lpfnWinHttpGetDefaultProxyConfiguration)(_Inout_ WINHTTP_PROXY_INFO *pProxyInfo);
typedef HGLOBAL (WINAPI *lpfnGlobalFree)(_Frees_ptr_opt_ HGLOBAL hMem);

//-----------------------------------------------------------

static HINSTANCE volatile hWinHttpDll = NULL;
static lpfnWinHttpOpen fnWinHttpOpen = NULL;
static lpfnWinHttpCloseHandle fnWinHttpCloseHandle = NULL;
static lpfnWinHttpGetIEProxyConfigForCurrentUser fnWinHttpGetIEProxyConfigForCurrentUser = NULL;
static lpfnWinHttpGetProxyForUrl fnWinHttpGetProxyForUrl = NULL;
static lpfnWinHttpGetDefaultProxyConfiguration fnWinHttpGetDefaultProxyConfiguration = NULL;
static lpfnGlobalFree fnGlobalFree = NULL;

//-----------------------------------------------------------

static HRESULT InitApis();
static HRESULT GetProxyConfiguration(_In_opt_z_ LPCWSTR szTargetUrlW, _Out_ MX::CStringW &cStrProxyW,
                                     _Out_ int *lpnPort);
static HRESULT GetProxyForAutoSettings(_In_ HINTERNET hSession, _In_z_ LPCWSTR szUrlW,
                                       _In_opt_z_ LPCWSTR szAutoConfigUrlW, _Out_ LPWSTR *lpwszProxyW);
static BOOL IsValidProxyValue(_Inout_ MX::CStringW &cStrProxyW, _Out_ int *lpnPort, _In_opt_z_ LPCWSTR szTargetUrlW);

//-----------------------------------------------------------

namespace MX {

CProxy::CProxy() : CBaseMemObj()
{
  nType = TypeNone;
  nPort = 0;
  return;
}

CProxy::CProxy(_In_ const CProxy& cSrc) throw(...) : CBaseMemObj()
{
  nType = TypeNone;
  nPort = 0;
  operator=(cSrc);
  return;
}

CProxy::~CProxy()
{
  return;
}

CProxy& CProxy::operator=(_In_ const CProxy& cSrc) throw(...)
{
  if (&cSrc != this)
  {
    CStringW cStrTempAddressW;
    CSecureStringW cStrTempUserNameW, cStrTempUserPasswordW;

    if (cStrTempAddressW.CopyN((LPCWSTR)(cSrc.cStrAddressW), cSrc.cStrAddressW.GetLength()) == FALSE ||
        cStrTempUserNameW.CopyN((LPCWSTR)(cSrc.cStrUserNameW), cSrc.cStrUserNameW.GetLength()) == FALSE ||
        cStrTempUserPasswordW.CopyN((LPCWSTR)(cSrc.cStrUserPasswordW), cSrc.cStrUserPasswordW.GetLength()) == FALSE)
    {
      throw (LONG)E_OUTOFMEMORY;
    }

    nType = cSrc.nType;
    cStrAddressW.Attach(cStrTempAddressW.Detach());
    cStrUserNameW.Attach(cStrTempUserNameW.Detach());
    cStrUserPasswordW.Attach(cStrTempUserPasswordW.Detach());
    nPort = cSrc.nPort;
  }
  return *this;
}

VOID CProxy::SetDirect()
{
  nType = TypeNone;
  cStrAddressW.Empty();
  nPort = 0;
  return;
}

HRESULT CProxy::SetManual(_In_z_ LPCWSTR szProxyServerW)
{
  MX::CStringW cStrTempAddressW;
  int nTempPort;

  if (szProxyServerW == NULL)
    return E_POINTER;
  if (*szProxyServerW == 0)
    return E_INVALIDARG;
  if (cStrTempAddressW.Copy(szProxyServerW) == FALSE)
    return E_OUTOFMEMORY;
  if (IsValidProxyValue(cStrTempAddressW, &nTempPort, NULL) == FALSE)
    return E_INVALIDARG;
  nType = TypeManual;
  cStrAddressW.Attach(cStrTempAddressW.Detach());
  nPort = nTempPort;
  return S_OK;
}

VOID CProxy::SetUseIE()
{
  nType = TypeUseIE;
  cStrAddressW.Empty();
  nPort = 0;
  return;
}

HRESULT CProxy::SetCredentials(_In_z_ LPCWSTR szUserNameW, _In_z_ LPCWSTR szPasswordW)
{
  if (szUserNameW != NULL && *szUserNameW != 0)
  {
    if (cStrUserNameW.Copy(szUserNameW) == FALSE)
      return E_OUTOFMEMORY;
    if (szPasswordW != NULL && *szPasswordW != 0)
    {
      if (cStrUserPasswordW.Copy(szPasswordW) == FALSE)
      {
        cStrUserNameW.Empty();
        return E_OUTOFMEMORY;
      }
    }
  }
  return S_OK;
}

HRESULT CProxy::Resolve(_In_opt_z_ LPCWSTR szTargetUrlW)
{
  if (nType == TypeUseIE)
  {
    MX::CStringW cStrTempAddressW;
    int nTempPort;
    HRESULT hRes;

    hRes = InitApis();
    if (SUCCEEDED(hRes))
      hRes = GetProxyConfiguration(szTargetUrlW, cStrTempAddressW, &nTempPort);
    if (FAILED(hRes))
    {
      cStrAddressW.Empty();
      nPort = 0;
      return hRes;
    }

    cStrAddressW.Attach(cStrTempAddressW.Detach());
    nPort = nTempPort;
  }
  return S_OK;
}

HRESULT CProxy::Resolve(_In_ CUrl &cUrl)
{
  MX::CStringW cStrTempW;
  HRESULT hRes;

  hRes = cUrl.ToString(cStrTempW, CUrl::ToStringAddScheme | CUrl::ToStringAddHostPort);
  if (SUCCEEDED(hRes))
    hRes = Resolve((LPCWSTR)cStrTempW);
  return hRes;
}

} //namespace MX

//-----------------------------------------------------------

static HRESULT InitApis()
{
  static LONG volatile nMutex = 0;

  if (hWinHttpDll == NULL)
  {
    MX::CFastLock cLock(&nMutex);

    if (hWinHttpDll == NULL)
    {
      HINSTANCE hDll;
      LPVOID _fnWinHttpOpen = NULL;
      LPVOID _fnWinHttpCloseHandle = NULL;
      LPVOID _fnWinHttpGetIEProxyConfigForCurrentUser = NULL;
      LPVOID _fnWinHttpGetProxyForUrl = NULL;
      LPVOID _fnWinHttpGetDefaultProxyConfiguration = NULL;
      LPVOID _fnGlobalFree = NULL;
      WCHAR szDllNameW[4096];
      DWORD dwLen;

      hDll = ::GetModuleHandleW(L"kernel32.dll");
      if (hDll == NULL)
      {
err_procnotfound:
        hWinHttpDll = (HINSTANCE)1;
        return MX_E_ProcNotFound;
      }
      _fnGlobalFree = ::GetProcAddress(hDll, "GlobalFree");
      if (_fnGlobalFree == NULL)
        goto err_procnotfound;

      dwLen = ::GetWindowsDirectoryW(szDllNameW, MX_ARRAYLEN(szDllNameW) - 16);
      if (dwLen == 0)
        goto err_procnotfound;
      if (szDllNameW[dwLen] != L'\\')
        szDllNameW[dwLen++] = L'\\';
      MX::MemCopy(szDllNameW + dwLen, L"winhttp.dll", (11 + 1) * sizeof(WCHAR));
      hDll = ::LoadLibraryW(szDllNameW);
      if (hDll == NULL)
        goto err_procnotfound;
      _fnWinHttpOpen = ::GetProcAddress(hDll, "WinHttpOpen");
      _fnWinHttpCloseHandle = ::GetProcAddress(hDll, "WinHttpCloseHandle");
      _fnWinHttpGetIEProxyConfigForCurrentUser = ::GetProcAddress(hDll, "WinHttpGetIEProxyConfigForCurrentUser");
      _fnWinHttpGetProxyForUrl = ::GetProcAddress(hDll, "WinHttpGetProxyForUrl");
      _fnWinHttpGetDefaultProxyConfiguration = ::GetProcAddress(hDll, "WinHttpGetDefaultProxyConfiguration");
      if (_fnWinHttpOpen == NULL || _fnWinHttpCloseHandle == NULL ||
          _fnWinHttpGetIEProxyConfigForCurrentUser == NULL || _fnWinHttpGetProxyForUrl == NULL ||
          _fnWinHttpGetDefaultProxyConfiguration == NULL)
      {
        ::FreeLibrary(hDll);
        goto err_procnotfound;
      }

      fnWinHttpOpen = (lpfnWinHttpOpen)_fnWinHttpOpen;
      fnWinHttpCloseHandle = (lpfnWinHttpCloseHandle)_fnWinHttpCloseHandle;
      fnWinHttpGetIEProxyConfigForCurrentUser =
          (lpfnWinHttpGetIEProxyConfigForCurrentUser)_fnWinHttpGetIEProxyConfigForCurrentUser;
      fnWinHttpGetProxyForUrl = (lpfnWinHttpGetProxyForUrl)_fnWinHttpGetProxyForUrl;
      fnWinHttpGetDefaultProxyConfiguration =
          (lpfnWinHttpGetDefaultProxyConfiguration)_fnWinHttpGetDefaultProxyConfiguration;
      fnGlobalFree = (lpfnGlobalFree)_fnGlobalFree;

      hWinHttpDll = hDll;
    }
  }
  return (hWinHttpDll != (HINSTANCE)1) ? S_OK : MX_E_ProcNotFound;
}

static HRESULT GetProxyConfiguration(_In_opt_z_ LPCWSTR szTargetUrlW, _Out_ MX::CStringW &cStrProxyW,
                                     _Out_ int *lpnPort)
{
  HINTERNET hSession = NULL;
  MX::CStringW cStrUrlW;
  LPWSTR szProxyW = NULL;
  WINHTTP_CURRENT_USER_IE_PROXY_CONFIG sIeProxy = { 0 };
  HRESULT hRes;

  cStrProxyW.Empty();
  *lpnPort = 0;

  if (szTargetUrlW != NULL && *szTargetUrlW != 0)
  {
    LPCWSTR sW;

    if (cStrUrlW.Copy(szTargetUrlW) == FALSE)
      return E_OUTOFMEMORY;
    sW = MX::StrFindW((LPCWSTR)cStrUrlW, L"://");
    if (sW == NULL)
    {
      if (cStrUrlW.InsertN(L"http://", 0, 7) == FALSE)
        return E_OUTOFMEMORY;
      sW += 7;
    }
    else
    {
      sW += 3;
    }
    sW = MX::StrChrW(sW, L'/');
    if (sW != NULL)
    {
      *((LPWSTR)sW) = 0;
      cStrUrlW.Refresh();
    }
  }

  hSession = fnWinHttpOpen(L"Mozilla/5.0 (compatible; MX-Lib 1.0)", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession)
  {
    hRes = MX_HRESULT_FROM_LASTERROR();
    goto done;
  }
  if (fnWinHttpGetIEProxyConfigForCurrentUser(&sIeProxy) == FALSE)
  {
    hRes = MX_HRESULT_FROM_LASTERROR();
    if (hRes == E_OUTOFMEMORY)
      goto done;
    MX::MemSet(&sIeProxy, 0, sizeof(sIeProxy));
  }
  if (sIeProxy.fAutoDetect == FALSE && sIeProxy.lpszProxy != NULL)
  {
    if (cStrProxyW.Copy(sIeProxy.lpszProxy) == FALSE)
    {
      hRes = E_OUTOFMEMORY;
      goto done;
    }
    if (IsValidProxyValue(cStrProxyW, lpnPort, (LPCWSTR)cStrUrlW) != FALSE)
    {
      hRes = S_OK;
      goto done;
    }
    cStrProxyW.Empty();
  }

  hRes = GetProxyForAutoSettings(hSession, (LPCWSTR)cStrUrlW,
                                 ((sIeProxy.lpszAutoConfigUrl != NULL &&
                                   sIeProxy.lpszAutoConfigUrl[0] != 0) ? sIeProxy.lpszAutoConfigUrl : NULL), &szProxyW);
  if (FAILED(hRes))
    goto done;
  if (szProxyW == NULL)
  {
    hRes = MX_E_NotFound;
    goto done;
  }
  if (cStrProxyW.Copy(szProxyW) == FALSE)
  {
    hRes = E_OUTOFMEMORY;
    goto done;
  }
  if (IsValidProxyValue(cStrProxyW, lpnPort, (LPCWSTR)cStrUrlW) != FALSE)
  {
    hRes = S_OK;
    goto done;
  }

  hRes = MX_E_NotFound;

done:
  if (szProxyW != NULL)
    fnGlobalFree(szProxyW);
  if (sIeProxy.lpszAutoConfigUrl != NULL)
    fnGlobalFree(sIeProxy.lpszAutoConfigUrl);
  if (sIeProxy.lpszProxy != NULL)
    fnGlobalFree(sIeProxy.lpszProxy);
  if (sIeProxy.lpszProxyBypass != NULL)
    fnGlobalFree(sIeProxy.lpszProxyBypass);
  if (hSession != NULL)
    fnWinHttpCloseHandle(hSession);
  if (FAILED(hRes))
  {
    cStrProxyW.Empty();
    *lpnPort = 0;
  }
  return hRes;
}

static HRESULT GetProxyForAutoSettings(_In_ HINTERNET hSession, _In_z_ LPCWSTR szUrlW,
                                       _In_opt_z_ LPCWSTR szAutoConfigUrlW, _Out_ LPWSTR *lpwszProxyW)
{
  WINHTTP_AUTOPROXY_OPTIONS sProxyOpts = { 0 };
  WINHTTP_PROXY_INFO sProxyInfo = { 0 };
  HRESULT hRes;

  *lpwszProxyW = NULL;

  if (*szUrlW != 0)
  {
    if (szAutoConfigUrlW != NULL)
    {
      sProxyOpts.dwFlags = WINHTTP_AUTOPROXY_CONFIG_URL;
      sProxyOpts.lpszAutoConfigUrl = szAutoConfigUrlW;
    }
    else
    {
      sProxyOpts.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
      sProxyOpts.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;
    }
    sProxyOpts.dwFlags |= WINHTTP_AUTOPROXY_NO_CACHE_SVC;

    //First call with no autologon.  Autologon prevents the session (in proc) or autoproxy service (out of proc) from
    //caching the proxy script.  This causes repetitive network traffic, so it is best not to do autologon unless it
    //is required according to the result of WinHttpGetProxyForUrl.
    if (fnWinHttpGetProxyForUrl(hSession, szUrlW, &sProxyOpts, &sProxyInfo) != FALSE)
      goto have_proxy;
    hRes = MX_HRESULT_FROM_LASTERROR();
    if (hRes == E_OUTOFMEMORY)
      goto done;
    if (hRes == HRESULT_FROM_WIN32(ERROR_WINHTTP_LOGIN_FAILURE))
    {
      sProxyOpts.fAutoLogonIfChallenged = TRUE;
      if (fnWinHttpGetProxyForUrl(hSession, szUrlW, &sProxyOpts, &sProxyInfo) != FALSE)
        goto have_proxy;

      hRes = MX_HRESULT_FROM_LASTERROR();
      if (hRes == E_OUTOFMEMORY)
        goto done;
    }
  }

  if (fnWinHttpGetDefaultProxyConfiguration(&sProxyInfo) != FALSE)
    goto have_proxy;

  hRes = MX_HRESULT_FROM_LASTERROR();
  goto done;

have_proxy:
  if (sProxyInfo.lpszProxy != NULL && sProxyInfo.lpszProxy[0] != 0)
  {
    *lpwszProxyW = sProxyInfo.lpszProxy;
    sProxyInfo.lpszProxy = NULL;
  }

  hRes = S_OK;

done:
  if (sProxyInfo.lpszProxy != NULL)
    fnGlobalFree(sProxyInfo.lpszProxy);
  if (sProxyInfo.lpszProxyBypass != NULL)
    fnGlobalFree(sProxyInfo.lpszProxyBypass);
  return hRes;
}

/*
static BOOL IsRecoverableAutoProxyError(_In_ HRESULT hr)
{
  return (SUCCEEDED(hr) ||
          hr == E_INVALIDARG ||
          hr == HRESULT_FROM_WIN32(ERROR_WINHTTP_AUTO_PROXY_SERVICE_ERROR) ||
          hr == HRESULT_FROM_WIN32(ERROR_WINHTTP_AUTODETECTION_FAILED) ||
          hr == HRESULT_FROM_WIN32(ERROR_WINHTTP_BAD_AUTO_PROXY_SCRIPT) ||
          hr == HRESULT_FROM_WIN32(ERROR_WINHTTP_LOGIN_FAILURE) ||
          hr == HRESULT_FROM_WIN32(ERROR_WINHTTP_OPERATION_CANCELLED) ||
          hr == HRESULT_FROM_WIN32(ERROR_WINHTTP_TIMEOUT) ||
          hr == HRESULT_FROM_WIN32(ERROR_WINHTTP_UNABLE_TO_DOWNLOAD_SCRIPT) ||
          hr == HRESULT_FROM_WIN32(ERROR_WINHTTP_UNRECOGNIZED_SCHEME)) ? TRUE : FALSE;
}
*/

static BOOL IsValidProxyValue(_Inout_ MX::CStringW &cStrProxyW, _Out_ int *lpnPort, _In_opt_z_ LPCWSTR szTargetUrlW)
{
  LPCWSTR sW, szProxyStartW = NULL, szProxyEndW = NULL;
  SIZE_T nLen, nSchemeLen;

  nSchemeLen = 0;
  if (szTargetUrlW != NULL && *szTargetUrlW != 0)
  {
    sW = MX::StrFindW(szTargetUrlW, L"://");
    if (sW != NULL)
      nSchemeLen = (SIZE_T)(sW - szTargetUrlW);
  }
  if (nSchemeLen == 0)
  {
    szTargetUrlW = L"http";
    nSchemeLen = 4;
  }

  *lpnPort = 0;

  //PROXY FORMAT: ([<scheme>=][<scheme>"://"]<server>[":"<port>])

  //lookup for the proxy matching the scheme first
  sW = (LPCWSTR)cStrProxyW;
  while (sW != NULL && *sW != 0)
  {
    //skip blanks
    while (*sW == L' ' || *sW == L'\t')
      sW++;
    //matches scheme?
    if (MX::StrNCompareW(sW, szTargetUrlW, nSchemeLen) == 0)
    {
      nLen = nSchemeLen;
      while (sW[nLen] == L' ' || sW[nLen] == L'\t')
        nLen++;
      if (sW[nLen] == L'=')
      {
        szProxyStartW = sW + nLen + 1;
        szProxyEndW = MX::StrChrW(szProxyStartW, L';');
        if (szProxyEndW == NULL)
          szProxyEndW = szProxyStartW + MX::StrLenW(szProxyStartW);
        break;
      }
    }
    //advance to next
    sW = MX::StrChrW(sW, L';');
    if (sW != NULL)
      sW++;
  }
  //if scheme not found, take first
  if (szProxyStartW == NULL)
  {
    szProxyStartW = (LPCWSTR)cStrProxyW;
    szProxyEndW = MX::StrChrW(szProxyStartW, L';');
    if (szProxyEndW == NULL)
      szProxyEndW = szProxyStartW + MX::StrLenW(szProxyStartW);
  }

  //strip scheme
  sW = MX::StrNFindW(szProxyStartW, L"://", (SIZE_T)(szProxyEndW - szProxyStartW));
  if (sW != NULL)
    szProxyStartW = sW + 3;

  //strip path, querystring, etc
  sW = MX::StrNChrW(szProxyStartW, L'/', (SIZE_T)(szProxyEndW - szProxyStartW));
  if (sW != NULL)
    szProxyEndW = sW;

  //strip scheme=
  sW = MX::StrNChrW(szProxyStartW, L'=', (SIZE_T)(szProxyEndW - szProxyStartW));
  if (sW != NULL)
    szProxyStartW = sW + 1;

  //skip blanks at the end
  while (szProxyEndW > szProxyStartW && (*(szProxyEndW-1) == L' ' || *(szProxyEndW-1) == L'\t'))
    szProxyEndW--;
  //skip blanks at the start
  while (szProxyStartW < szProxyEndW && (*szProxyStartW == L' ' || *szProxyStartW == L'\t'))
    szProxyStartW--;

  //here we have: "address[:port]", split them
  sW = MX::StrNChrW(szProxyStartW, L':', (SIZE_T)(szProxyEndW - szProxyStartW), TRUE);
  if (sW != NULL)
  {
    LPCWSTR szAddressEndW = sW;

    sW++;
    while (sW < szProxyEndW)
    {
      if (*sW < L'0' || *sW > L'9')
        return FALSE;
      *lpnPort = (*lpnPort) * 10 + (int)(*sW - L'0');
      if ((*lpnPort) > 65535)
        return FALSE;
      sW++;
    }

    szProxyEndW = szAddressEndW;
  }
  else
  {
    *lpnPort = 8080;
  }

  if (szProxyStartW >= szProxyEndW)
  {
    *lpnPort = 0;
    return FALSE;
  }
  nLen = (SIZE_T)(szProxyEndW - szProxyStartW);
  cStrProxyW.Delete(0, (SIZE_T)(szProxyStartW - (LPCWSTR)cStrProxyW));
  cStrProxyW.Delete(nLen, (SIZE_T)-1);
  return TRUE;
}

static VOID CleanupSensibleData(_Inout_ MX::CStringW &cStrW)
{
  MX::MemSet((LPWSTR)cStrW, '*', cStrW.GetLength() * 2);
  cStrW.Empty();
  return;
}
