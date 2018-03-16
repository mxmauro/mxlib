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
#include "Test.h"
#include <conio.h>
#include "TestHttpServer.h"
#include "TestHttpClient.h"
#include "TestJavascript.h"
#include "TestJsHttpServer.h"
#include "Comm\SslCertificates.h"

#pragma comment(lib, "MxLib.lib")
#pragma comment(lib, "CommLib.lib")
#pragma comment(lib, "ZLib.lib")
#pragma comment(lib, "CryptoLib.lib")
#pragma comment(lib, "JsLib.lib")
#pragma comment(lib, "JsHttpServer.lib")

//-----------------------------------------------------------

int wmain(int argc, WCHAR* argv[])
{
  if (argc >= 2)
  {
    if (_wcsicmp(argv[1], L"HttpServer") == 0)
    {
      BOOL bUseSSL;

      bUseSSL = (argc >= 3 && (argv[2][0] == L'/' || argv[2][0] == L'-') &&
                 _wcsicmp(argv[2]+1, L"ssl") == 0) ? TRUE : FALSE;
      return TestHttpServer(bUseSSL);
    }
    if (_wcsicmp(argv[1], L"HttpClient") == 0)
    {
      return TestHttpClient();
    }
    if (_wcsicmp(argv[1], L"Javascript") == 0)
    {
      return TestJavascript();
    }
    if (_wcsicmp(argv[1], L"JsHttpServer") == 0)
    {
      BOOL bUseSSL;

      bUseSSL = (argc >= 3 && (argv[2][0] == L'/' || argv[2][0] == L'-') &&
                 _wcsicmp(argv[2]+1, L"ssl") == 0) ? TRUE : FALSE;
      return TestJsHttpServer(bUseSSL);
    }
  }
  wprintf_s(L"Use: Test.exe test-module\n\n");
  wprintf_s(L"Where 'test-module' can be:\n");
  wprintf_s(L"    HttpServer, HttpClient, Javascript or JsHttpServer\n");
  return 0;
}

BOOL ShouldAbort()
{
  static LONG volatile nDoAbort = 0;

  if (__InterlockedRead(&nDoAbort) != 0)
    return TRUE;
  if (_kbhit() != 0 && _getwch() == 27)
  {
    _InterlockedExchange(&nDoAbort, 1);
    return TRUE;
  }
  return FALSE;
}

HRESULT GetAppPath(_Out_ MX::CStringW &cStrPathW)
{
  DWORD dw;
  LPWSTR sW;

  if (cStrPathW.EnsureBuffer(2100) == FALSE)
    return E_OUTOFMEMORY;
  dw = ::GetModuleFileNameW(NULL, (LPWSTR)cStrPathW, 2048);
  ((LPWSTR)cStrPathW)[dw] = 0;
  sW = (LPWSTR)MX::StrChrW((LPWSTR)cStrPathW, L'\\', TRUE);
  if (sW != NULL)
    *(sW+1) = 0;
  cStrPathW.Refresh();
  return S_OK;
}
