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

BOOL Init()
{
  return (SUCCEEDED(_OpenSSL_Init())) ? TRUE : FALSE;
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
  static const WORD aDisallowedCiphers[] ={
    0x000B, 0x000C, 0x000D, 0x0011, 0x0012, 0x0013, 0x002F, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034,
    0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A
  };
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
    SSL *lpSsl;
    CStringA cStrCipherListA;
    STACK_OF(SSL_CIPHER) *lpCiphers;
    const SSL_CIPHER *lpCipher;
    unsigned long _id;
    int cipherIdx;
    SIZE_T i;
    BOOL bEnable;

    if (lpSslContexts[(bServerSide != FALSE) ? 1 : 0][idx] == NULL)
    {
      lpSslCtx = SSL_CTX_new((bServerSide != FALSE) ? TLS_server_method() : TLS_client_method());
      if (lpSslCtx == NULL)
        return NULL;
      if (cStrCipherListA.Copy("DEFAULT:!NULL:!aNULL:!IDEA:!FZA") == FALSE)
      {
        SSL_CTX_free(lpSslCtx);
        return NULL;
      }
      lpSsl = SSL_new(lpSslCtx);
      if (lpSsl == NULL)
        return NULL;
      lpCiphers = SSL_get_ciphers(lpSsl);
      for (cipherIdx=0; cipherIdx<sk_SSL_CIPHER_num(lpCiphers); cipherIdx++)
      {
        bEnable = TRUE;
        lpCipher = sk_SSL_CIPHER_value(lpCiphers, cipherIdx);
        if (SSL_CIPHER_get_bits(lpCipher, NULL) < 80)
        {
          bEnable = FALSE;
        }
        else
        {
          _id = SSL_CIPHER_get_id(lpCipher);
          for (i=0; i<MX_ARRAYLEN(aDisallowedCiphers) && _id != (unsigned long)aDisallowedCiphers[i]; i++);
          if (i < MX_ARRAYLEN(aDisallowedCiphers))
            bEnable = FALSE;
        }
        if (bEnable == FALSE)
        {
          if (cStrCipherListA.ConcatN(":!", 2) == FALSE ||
              cStrCipherListA.Concat(SSL_CIPHER_get_name(lpCipher)) == FALSE)
          {
            SSL_free(lpSsl);
            SSL_CTX_free(lpSslCtx);
            return NULL;
          }
        }
      }
      SSL_free(lpSsl);
      if (SSL_CTX_set_cipher_list(lpSslCtx, (LPSTR)cStrCipherListA) <= 0)
      {
        SSL_CTX_free(lpSslCtx);
        return NULL;
      }
      SSL_CTX_set_session_cache_mode(lpSslCtx, SSL_SESS_CACHE_SERVER);
      //SSL_CTX_sess_set_new_cb(lpSslCtx, NewSessionCallbackStatic);
      //SSL_CTX_sess_set_remove_cb(lpSslCtx, RemoveSessionCallbackStatic);
      SSL_CTX_set_timeout(lpSslCtx, 3600);
      SSL_CTX_sess_set_cache_size(lpSslCtx, 1024);
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
      OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS |
                       OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS, NULL);
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
  return MX::MemAllocD(_Size, _filename, _linenum);
}

static void* __cdecl my_realloc_withinfo(void *_Memory, size_t _NewSize, const char *_filename, int _linenum)
{
  return MX::MemReallocD(_Memory, _NewSize, _filename, _linenum);
}

static void __cdecl my_free(void * _Memory, const char *_filename, int _linenum)
{
  MX::MemFree(_Memory);
  return;
}
