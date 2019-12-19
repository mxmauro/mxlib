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
#include "InitOpenSSL.h"
#include "..\..\Include\WaitableObjects.h"
#include "..\..\Include\Finalizer.h"
#include "..\..\Include\Strings\Strings.h"
#include <OpenSSL\ssl.h>
#include <OpenSSL\err.h>
#include <OpenSSL\conf.h>
#include <OpenSSL\pkcs12err.h>

#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")

//-----------------------------------------------------------

#define OPENSSL_FINALIZER_PRIORITY 10000

#define AVAILABLE_CIPHER_SUITES "ALL:!EXPORT:!LOW:!aNULL:!eNULL:!SSLv2:!ADH:!EDH:!DH:!IDEA:!FZA:!RC4"

//-----------------------------------------------------------

static LONG volatile nInitialized = 0;
static SSL_CTX* volatile lpSslContexts[2][1] ={ 0 };

//-----------------------------------------------------------

extern "C" {
  void OPENSSL_thread_stop(void);
};

//-----------------------------------------------------------

static HRESULT _OpenSSL_Init();
static VOID OpenSSL_Shutdown();
static void NTAPI OnTlsCallback(_In_ PVOID DllHandle, _In_ DWORD dwReason, _In_ PVOID);
static void* __cdecl my_malloc_withinfo(size_t _Size, const char *_filename, int _linenum);
static void* __cdecl my_realloc_withinfo(void *_Memory, size_t _NewSize, const char *_filename, int _linenum);
static void __cdecl my_free(void * _Memory, const char *_filename, int _linenum);

//-----------------------------------------------------------

#if defined(_M_IX86)
  #pragma comment (linker, "/INCLUDE:__tls_used")
  #pragma comment (linker, "/INCLUDE:_OpenSslTlsCallback")
  #pragma data_seg(".CRT$XLF")
  EXTERN_C const PIMAGE_TLS_CALLBACK OpenSslTlsCallback = &OnTlsCallback;
  #pragma data_seg()
#elif defined(_M_X64)
  #pragma comment (linker, "/INCLUDE:_tls_used")
  #pragma comment (linker, "/INCLUDE:OpenSslTlsCallback")
  #pragma const_seg(".CRT$XLF")
  EXTERN_C const PIMAGE_TLS_CALLBACK OpenSslTlsCallback = &OnTlsCallback;
  #pragma const_seg()
#endif

//-----------------------------------------------------------

namespace MX {

namespace Internals {

namespace OpenSSL {

HRESULT Init()
{
  return _OpenSSL_Init();
}

HRESULT GetLastErrorCode(_In_ BOOL bDefaultIsInvalidData)
{
  unsigned long err;
  BOOL bHasError = FALSE;
  HRESULT hRes = (bDefaultIsInvalidData != FALSE) ? MX_E_InvalidData : S_OK;

  while ((err = ERR_get_error()) != 0)
  {
    if (ERR_GET_REASON(err) == ERR_R_MALLOC_FAILURE)
    {
      hRes = E_OUTOFMEMORY;
      break;
    }
    if (ERR_GET_LIB(err) == ERR_LIB_PKCS12 && ERR_GET_REASON(err) == PKCS12_R_MAC_VERIFY_FAILURE)
    {
      hRes = HRESULT_FROM_WIN32(ERROR_INVALID_PASSWORD);
      break;
    }
    bHasError = TRUE;
  }
  if (bHasError != FALSE)
  {
    ERR_clear_error();
    if (hRes == S_OK)
      hRes = MX_E_InvalidData;
  }
  return hRes;
}

SSL_CTX* GetSslContext(_In_ BOOL bServerSide, _In_z_ LPCSTR szVersionA)
{
  static LONG volatile nSslContextMutex = 0;
  int idx;

  if (szVersionA == NULL)
    return NULL;
  if (MX::StrCompareA(szVersionA, "tls") == 0 || MX::StrCompareA(szVersionA, "tls1.0") == 0 ||
      MX::StrCompareA(szVersionA, "tls1.1") == 0 || MX::StrCompareA(szVersionA, "tls1.2") == 0)
  {
    idx = 0;
  }
  else
  {
    return NULL;
  }
  //----
  if (lpSslContexts[(bServerSide != FALSE) ? 1 : 0][idx] == NULL)
  {
    MX::CFastLock cSslLock(&nSslContextMutex);
    SSL_CTX *lpSslCtx;

    if (lpSslContexts[(bServerSide != FALSE) ? 1 : 0][idx] == NULL)
    {
      lpSslCtx = SSL_CTX_new((bServerSide != FALSE) ? TLS_server_method() : TLS_client_method());
      if (lpSslCtx == NULL)
        return NULL;
      if (SSL_CTX_set_cipher_list(lpSslCtx, AVAILABLE_CIPHER_SUITES) <= 0)
      {
        SSL_CTX_free(lpSslCtx);
        return NULL;
      }
      SSL_CTX_set_session_cache_mode(lpSslCtx, SSL_SESS_CACHE_SERVER);
      //SSL_CTX_sess_set_new_cb(lpSslCtx, NewSessionCallbackStatic);
      //SSL_CTX_sess_set_remove_cb(lpSslCtx, RemoveSessionCallbackStatic);
      SSL_CTX_set_timeout(lpSslCtx, 3600);
      SSL_CTX_sess_set_cache_size(lpSslCtx, 1024);
      SSL_CTX_set_read_ahead(lpSslCtx, 1);
      //----
      __InterlockedExchangePointer((volatile LPVOID *)&(lpSslContexts[(bServerSide != FALSE) ? 1 : 0][idx]), lpSslCtx);
    }
  }
  //done
  return lpSslContexts[(bServerSide != FALSE) ? 1 : 0][idx];
}

} //namespace OpenSSL

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

static HRESULT _OpenSSL_Init()
{
  static LONG volatile nMutex = 0;

  if (__InterlockedRead(&nInitialized) == 0)
  {
    MX::CFastLock cLock(&nMutex);
    HRESULT hRes;

    if (__InterlockedRead(&nInitialized) == 0)
    {
      //setup memory allocator
#ifdef _DEBUG
      CRYPTO_set_mem_debug(1);
      CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
#endif //_DEBUG
      CRYPTO_set_mem_functions(&my_malloc_withinfo, &my_realloc_withinfo, &my_free);
      //init lib
      if (OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS |
                           OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS, NULL) == 0)
      {
        return E_OUTOFMEMORY;
      }
      ERR_clear_error();
      //register shutdown callback
      hRes = MX::RegisterFinalizer(&OpenSSL_Shutdown, OPENSSL_FINALIZER_PRIORITY);
      if (FAILED(hRes))
      {
        OpenSSL_Shutdown();
        return hRes;
      }
      //done
      _InterlockedExchange(&nInitialized, 1);
    }
  }
  return S_OK;
}

static VOID OpenSSL_Shutdown()
{
  SIZE_T i;

  for (i=0; i<MX_ARRAYLEN(lpSslContexts[0]); i++)
  {
    if (lpSslContexts[1][i] != NULL)
    {
      SSL_CTX_free(lpSslContexts[1][i]);
      lpSslContexts[1][i] = NULL;
    }
    if (lpSslContexts[0][i] != NULL)
    {
      SSL_CTX_free(lpSslContexts[0][i]);
      lpSslContexts[0][i] = NULL;
    }
  }
  //----
  FIPS_mode_set(0);
  CONF_modules_unload(1);
  EVP_cleanup();
  CRYPTO_cleanup_all_ex_data();
  ERR_free_strings();

  OPENSSL_thread_stop();
  OPENSSL_cleanup();

  _InterlockedExchange(&nInitialized, 0);
  return;
}

static void NTAPI OnTlsCallback(_In_ PVOID DllHandle, _In_ DWORD dwReason, _In_ PVOID)
{
  if (dwReason == DLL_THREAD_DETACH)
  {
    if (__InterlockedRead(&nInitialized) != 0)
      OPENSSL_thread_stop();
  }
  return;
}

static void* __cdecl my_malloc_withinfo(size_t _Size, const char *_filename, int _linenum)
{
  return ::MxMemAllocD(_Size, _filename, _linenum);
}

static void* __cdecl my_realloc_withinfo(void *_Memory, size_t _NewSize, const char *_filename, int _linenum)
{
  return ::MxMemReallocD(_Memory, _NewSize, _filename, _linenum);
}

static void __cdecl my_free(void * _Memory, const char *_filename, int _linenum)
{
  ::MxMemFree(_Memory);
  return;
}
