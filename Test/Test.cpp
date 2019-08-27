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

#ifdef _DEBUG
  #define USE_PATH_TO_SOURCE
#endif //_DEBUG

//-----------------------------------------------------------

int wmain(int argc, WCHAR* argv[])
{
  BOOL bUseSSL = FALSE;
  DWORD dwLogLevel = 0;
  int nTest = 0;

  if (Console::Initialize() == FALSE)
  {
    wprintf_s(L"Error: Unable to initialize console.\n\n");
    return -1;
  }

  if (argc < 2)
  {
    wprintf_s(L"Use: Test.exe test-module [options]\n\n");
    wprintf_s(L"Where 'test-module' can be:\n");
    wprintf_s(L"    HttpServer, HttpClient, Javascript or JsHttpServer\n\n");
    wprintf_s(L"And 'options' can be:\n");
    wprintf_s(L"    /ssl: Enable SSL on HttpServer and JsHttpServer\n");
    wprintf_s(L"    /v #: Set the verbosity level to #\n");
    return 1;
  }
  for (int arg_idx = 1; arg_idx < argc; arg_idx++)
  {
    if (argv[arg_idx][0] == L'/' || argv[arg_idx][0] == L'-')
    {
      if (_wcsicmp(argv[arg_idx] + 1, L"ssl") == 0)
      {
        bUseSSL = TRUE;
      }
      else if (_wcsicmp(argv[arg_idx] + 1, L"v") == 0)
      {
        LPWSTR sW;

        if ((++arg_idx) >= argc)
        {
          wprintf_s(L"Error: Missing parameter for verbosity argument.\n");
          return 1;
        }
        dwLogLevel = 0;
        for (sW = argv[arg_idx]; *sW != 0; sW++)
        {
          if (*sW >= L'0' && *sW <= L'9')
          {
            dwLogLevel = dwLogLevel * 10 + (DWORD)(*sW - L'0');
            if (dwLogLevel > 1000)
              goto err_cmdline_invalidverbosity;
          }
          else
          {
err_cmdline_invalidverbosity:
            wprintf_s(L"Error: Invalid log level for verbosity argument.\n");
            return 1;
          }
        }
      }
      else
      {
        wprintf_s(L"Error: Invalid command-line argument (%s).\n", argv[arg_idx]);
        return 1;
      }
    }
    else
    {
      if (_wcsicmp(argv[arg_idx], L"HttpServer") == 0)
      {
        if (nTest != 0)
        {
err_cmdline_alreadyspecified:
          wprintf_s(L"Error: Test to execute already specified.\n");
          return 1;
        }
        nTest = 1;
      }
      else if (_wcsicmp(argv[1], L"HttpClient") == 0)
      {
        if (nTest != 0)
          goto err_cmdline_alreadyspecified;
        nTest = 2;
      }
      else if (_wcsicmp(argv[1], L"Javascript") == 0)
      {
        if (nTest != 0)
          goto err_cmdline_alreadyspecified;
        nTest = 3;
      }
      else if (_wcsicmp(argv[1], L"JsHttpServer") == 0)
      {
        if (nTest != 0)
          goto err_cmdline_alreadyspecified;
        nTest = 4;
      }
      else
      {
        wprintf_s(L"Error: An unknown test name has been specified (%s).\n", argv[arg_idx]);
        return 1;
      }
    }
  }
  if (nTest == 0)
  {
    wprintf_s(L"Error: No test name has been specified.\n");
    return 1;
  }

  switch (nTest)
  {
    case 1:
      return TestHttpServer(bUseSSL, dwLogLevel);

    case 2:
      return TestHttpClient(dwLogLevel);

    case 3:
      return TestJavascript(dwLogLevel);

    case 4:
      return TestJsHttpServer(bUseSSL, dwLogLevel);
  }
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
  if (Console::MustQuit() != FALSE)
  {
    _InterlockedExchange(&nDoAbort, 1);
    return TRUE;
  }
  return FALSE;
}

HRESULT GetAppPath(_Out_ MX::CStringW &cStrPathW)
{
#ifdef USE_PATH_TO_SOURCE
#define __WIDEN_(x) L##x
#define __WIDEN(x) __WIDEN_(x)
  LPCWSTR sW;

  if (cStrPathW.Copy(__WIDEN(__FILE__)) == FALSE)
    return E_OUTOFMEMORY;
  sW = (LPCWSTR)MX::StrChrW((LPCWSTR)cStrPathW, L'\\', TRUE) + 1;
  cStrPathW.Delete((SIZE_T)(sW - (LPCWSTR)cStrPathW), (SIZE_T)-1);
  if (cStrPathW.ConcatN(L"Data\\", 5) == FALSE)
    return E_OUTOFMEMORY;
#undef __WIDEN_
#undef __WIDEN
#else //USE_PATH_TO_SOURCE
  DWORD dw;
  LPWSTR sW;

  if (cStrPathW.EnsureBuffer(2100) == FALSE)
    return E_OUTOFMEMORY;
  dw = ::GetModuleFileNameW(NULL, (LPWSTR)cStrPathW, 2048);
  ((LPWSTR)cStrPathW)[dw] = 0;
  sW = (LPWSTR)MX::StrChrW((LPWSTR)cStrPathW, L'\\', TRUE);
  if (sW != NULL)
    sW[1] = 0;
  cStrPathW.Refresh();
#endif //USE_PATH_TO_SOURCE
  return S_OK;
}
