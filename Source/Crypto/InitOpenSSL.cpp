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
#include "..\..\Include\FnvHash.h"
#include "..\..\Include\Strings\Strings.h"
#include <OpenSSL\ssl.h>
#include <OpenSSL\err.h>
#include <OpenSSL\conf.h>
#include <OpenSSL\pkcs12err.h>

#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")

//-----------------------------------------------------------

#define OPENSSL_FINALIZER_PRIORITY 10000

//#define AVAILABLE_CIPHER_SUITES "ALL:!EXPORT:!LOW:!aNULL:!eNULL:!SSLv2:!ADH:!EDH:!DH:!IDEA:!FZA:!RC4"
#define AVAILABLE_CIPHER_SUITES "HIGH:!aNULL:!MD5"

#define __HEAPS_COUNT 64

//-----------------------------------------------------------

static LONG volatile nInitialized = 0;
static SSL_CTX* volatile lpSslContexts[2] = { NULL, NULL };
static HANDLE hHeaps[__HEAPS_COUNT] = { 0 };
static LONG volatile nNextHeapIndex = 0;

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

HRESULT GetLastErrorCode(_In_ HRESULT hResDefault)
{
  unsigned long err;
  BOOL bHasError = FALSE;
  HRESULT hRes = hResDefault;

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
      hRes = hResDefault;
  }
  return hRes;
}

SSL_CTX* GetSslContext(_In_ BOOL bServerSide)
{
  static LONG volatile nSslContextMutex = 0;

  if (lpSslContexts[(bServerSide != FALSE) ? 1 : 0] == NULL)
  {
    MX::CFastLock cSslLock(&nSslContextMutex);
    SSL_CTX *lpSslCtx;

    if (lpSslContexts[(bServerSide != FALSE) ? 1 : 0] == NULL)
    {
      lpSslCtx = SSL_CTX_new((bServerSide != FALSE) ? TLS_server_method() : TLS_client_method());
      if (lpSslCtx == NULL)
        return NULL;
      if (SSL_CTX_set_cipher_list(lpSslCtx, AVAILABLE_CIPHER_SUITES) <= 0)
      {
        SSL_CTX_free(lpSslCtx);
        return NULL;
      }
      //----
      if (bServerSide != FALSE)
      {
        Fnv64_t nId;
        LPVOID lp;

        SSL_CTX_set_min_proto_version(lpSslCtx, TLS1_2_VERSION);
        SSL_CTX_set_max_proto_version(lpSslCtx, TLS_MAX_VERSION);

        lp = &lpSslCtx;
        nId = fnv_64a_buf(&lp, sizeof(lp), FNV1A_64_INIT);
        lp = (LPVOID)&nSslContextMutex;
        nId = fnv_64a_buf(&lp, sizeof(lp), nId);
        SSL_CTX_set_session_id_context(lpSslCtx, (unsigned char*)&nId, (unsigned int)sizeof(nId));
        SSL_CTX_set_session_cache_mode(lpSslCtx, SSL_SESS_CACHE_SERVER);
        SSL_CTX_sess_set_cache_size(lpSslCtx, 131072);
      }
      else
      {
        SSL_CTX_set_min_proto_version(lpSslCtx, TLS1_VERSION);
        SSL_CTX_set_max_proto_version(lpSslCtx, TLS_MAX_VERSION);

        SSL_CTX_set_session_cache_mode(lpSslCtx, SSL_SESS_CACHE_CLIENT);
        SSL_CTX_sess_set_cache_size(lpSslCtx, 10240);
      }
      //SSL_CTX_sess_set_new_cb(lpSslCtx, NewSessionCallbackStatic);
      //SSL_CTX_sess_set_remove_cb(lpSslCtx, RemoveSessionCallbackStatic);
      SSL_CTX_set_timeout(lpSslCtx, 300);
      SSL_CTX_set_read_ahead(lpSslCtx, 1);
      //----
      __InterlockedExchangePointer((volatile LPVOID *)&(lpSslContexts[(bServerSide != FALSE) ? 1 : 0]), lpSslCtx);
    }
  }
  //done
  return lpSslContexts[(bServerSide != FALSE) ? 1 : 0];
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
      SIZE_T i;

      //setup memory allocator
      for (i = 0; i < __HEAPS_COUNT; i++)
      {
        hHeaps[i] = ::HeapCreate(0, 1048576, 0);
        if (hHeaps[i] == NULL)
        {
          while (i > 0)
          {
            i--;
            ::HeapDestroy(hHeaps[i]);
            hHeaps[i] = NULL;
          }
          return E_OUTOFMEMORY;
        }
      }

#ifdef _DEBUG
      CRYPTO_set_mem_debug(1);
      CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
#endif //_DEBUG
      CRYPTO_set_mem_functions(&my_malloc_withinfo, &my_realloc_withinfo, &my_free);
      //init lib
      if (OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS |
                           OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS, NULL) == 0)
      {
        for (i = 0; i < __HEAPS_COUNT; i++)
        {
          ::HeapDestroy(hHeaps[i]);
          hHeaps[i] = NULL;
        }
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

  for (i = 0; i < MX_ARRAYLEN(lpSslContexts); i++)
  {
    if (lpSslContexts[i] != NULL)
    {
      SSL_CTX_free(lpSslContexts[i]);
      lpSslContexts[i] = NULL;
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

  for (i = 0; i < __HEAPS_COUNT; i++)
  {
    ::HeapDestroy(hHeaps[i]);
    hHeaps[i] = NULL;
  }
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
  SIZE_T nIdx = (::GetCurrentThreadId() >> 2) & (__HEAPS_COUNT - 1);
  //SIZE_T nIdx = (SIZE_T)_InterlockedIncrement(&nNextHeapIndex) & (__HEAPS_COUNT - 1);
  LPBYTE lpPtr;

  lpPtr = (LPBYTE)::HeapAlloc(hHeaps[nIdx], 0, (DWORD)(sizeof(SIZE_T) + _Size));
  if (lpPtr != NULL)
  {
    *((PSIZE_T)lpPtr) = nIdx;
    lpPtr += sizeof(SIZE_T);
  }
  return lpPtr;
}

static void* __cdecl my_realloc_withinfo(void *_Memory, size_t _NewSize, const char *_filename, int _linenum)
{
  LPBYTE lpPtr, lpNewPtr;

  if (_Memory == NULL)
    return my_malloc_withinfo(_NewSize, _filename, _linenum);
  if (_NewSize == 0)
  {
    my_free(_Memory, _filename, _linenum);
    return NULL;
  }

  lpPtr = (LPBYTE)_Memory - sizeof(SIZE_T);

  lpNewPtr = (LPBYTE)::HeapReAlloc(hHeaps[*((PSIZE_T)lpPtr)], 0, lpPtr, (DWORD)(_NewSize + sizeof(SIZE_T)));
  if (lpNewPtr != NULL)
    lpNewPtr += sizeof(SIZE_T);
  return lpNewPtr;
}

static void __cdecl my_free(void *_Memory, const char *_filename, int _linenum)
{
  if (_Memory != NULL)
  {
    LPBYTE lpPtr = (LPBYTE)_Memory - sizeof(SIZE_T);

    ::HeapFree(hHeaps[*((PSIZE_T)lpPtr)], 0, lpPtr);
  }
  return;
}
