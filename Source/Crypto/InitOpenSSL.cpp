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

#pragma comment(lib, "libeay32.lib")
#pragma comment(lib, "ssleay32.lib")

//-----------------------------------------------------------

#define OPENSSL_FINALIZER_PRIORITY 10000

//-----------------------------------------------------------

struct CRYPTO_dynlock_value {
  CRITICAL_SECTION cs;
};

static MX::CCriticalSection *lpCritSect = NULL;
static int nCritSectCount = 0;
static SSL_CTX * volatile lpSslContexts[2][4] ={ 0 };

//-----------------------------------------------------------

static HRESULT _OpenSSL_Init();
static VOID OpenSSL_Shutdown();
static void* __cdecl my_malloc_withinfo(size_t _Size, const char *_filename, int _linenum);
static void* __cdecl my_realloc_withinfo(void *_Memory, size_t _NewSize, const char *_filename, int _linenum);
static void __cdecl my_free(void * _Memory);

static void my_locking_function(int mode, int n, const char * file, int line);
static CRYPTO_dynlock_value* my_dynlock_create_callback(const char *file, int line);
static void my_dynlock_lock_callback(int mode, CRYPTO_dynlock_value* l, const char *file, int line);
static void my_dynlock_destroy_callback(CRYPTO_dynlock_value* l, const char *file, int line);

//-----------------------------------------------------------

namespace MX {

namespace Internals {

namespace OpenSSL {

BOOL Init()
{
  return (SUCCEEDED(_OpenSSL_Init())) ? TRUE : FALSE;
}

BOOL IsMemoryError()
{
  unsigned long err;

  while ((err = ERR_get_error()) != 0)
  {
    if (ERR_GET_REASON(err) == ERR_R_MALLOC_FAILURE)
    {
      ERR_clear_error();
      return TRUE;
    }
  }
  return FALSE;
}

SSL_CTX* GetSslContext(__in BOOL bServerSide, __in_z LPCSTR szVersionA)
{
  static const WORD aDisallowedCiphers[] ={
    0x000B, 0x000C, 0x000D, 0x0011, 0x0012, 0x0013, 0x002F, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034,
    0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A
  };
  int idx;

  if (szVersionA == NULL)
    return NULL;
  if (MX::StrCompareA(szVersionA, "ssl3") == 0)
    idx = 0;
  else if (MX::StrCompareA(szVersionA, "tls1.0") == 0)
    idx = 1;
  else if (MX::StrCompareA(szVersionA, "tls1.1") == 0)
    idx = 2;
  else if (MX::StrCompareA(szVersionA, "tls1.2") == 0)
    idx = 3;
  else
    return NULL;
  //----
  if (lpSslContexts[(bServerSide != FALSE) ? 1 : 0][idx] == NULL)
  {
    MX::CCriticalSection::CAutoLock cSslLock(lpCritSect[nCritSectCount]);
    SSL_CTX *lpSslCtx;
    SSL *lpSsl;
    CStringA cStrCipherListA;
    STACK_OF(SSL_CIPHER) *lpCiphers;
    const SSL_CIPHER *lpCipher;
    unsigned long _id;
    int cipherIdx;
    SIZE_T i;
    BOOL bEnable;

    lpSslCtx = NULL;
    if (lpSslContexts[(bServerSide != FALSE) ? 1 : 0][idx] == NULL)
    {
      switch (idx)
      {
        case 0:
          lpSslCtx = SSL_CTX_new((bServerSide != FALSE) ? SSLv3_server_method(): SSLv3_client_method());
          break;
        case 1:
          lpSslCtx = SSL_CTX_new((bServerSide != FALSE) ? TLSv1_server_method() : TLSv1_client_method());
          break;
        case 2:
          lpSslCtx = SSL_CTX_new((bServerSide != FALSE) ? TLSv1_1_server_method() : TLSv1_1_client_method());
          break;
        case 3:
          lpSslCtx = SSL_CTX_new((bServerSide != FALSE) ? TLSv1_2_server_method() : TLSv1_2_client_method());
          break;
      }
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
  static LONG volatile nInitialized = 0;

  if (MX::__InterlockedRead(&nInitialized) == 0)
  {
    MX::CFastLock cLock(&nMutex);
    HRESULT hRes;

    if (MX::__InterlockedRead(&nInitialized) == 0)
    {
      //setup locks
      nCritSectCount = CRYPTO_num_locks();
      lpCritSect = MX_DEBUG_NEW MX::CCriticalSection[nCritSectCount+1];
      if (lpCritSect == NULL)
        return E_OUTOFMEMORY;
      //setup memory allocator
#ifdef _DEBUG
      CRYPTO_set_mem_debug(1);
      CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
#endif //_DEBUG
      CRYPTO_set_mem_functions(my_malloc_withinfo, my_realloc_withinfo, my_free);
      //setup dynamic locks
      CRYPTO_set_locking_callback(my_locking_function);
      CRYPTO_set_dynlock_create_callback(&my_dynlock_create_callback);
      CRYPTO_set_dynlock_lock_callback(&my_dynlock_lock_callback);
      CRYPTO_set_dynlock_destroy_callback(&my_dynlock_destroy_callback);
      //init lib
      SSL_load_error_strings();
      SSL_library_init();
      OpenSSL_add_all_algorithms();
      ERR_load_crypto_strings();
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
  int i;

  for (i=0; i<4; i++)
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
  CRYPTO_set_locking_callback(NULL);
  CRYPTO_set_dynlock_create_callback(NULL);
  CRYPTO_set_dynlock_lock_callback(NULL);
  CRYPTO_set_dynlock_destroy_callback(NULL);
  //----
  EVP_cleanup();
  CRYPTO_cleanup_all_ex_data();
  ERR_remove_thread_state(NULL);
  ERR_free_strings();
  //----
  if (lpCritSect != NULL)
  {
    delete [] lpCritSect;
    lpCritSect = NULL;
  }
  nCritSectCount = 0;
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

static void __cdecl my_free(void * _Memory)
{
  MX::MemFree(_Memory);
  return;
}

static void my_locking_function(int mode, int n, const char * file, int line)
{
  if (mode & CRYPTO_LOCK)
    lpCritSect[n].Lock();
  else
    lpCritSect[n].Unlock();
  return;
}

static CRYPTO_dynlock_value* my_dynlock_create_callback(const char *file, int line)
{
  CRYPTO_dynlock_value *l = (CRYPTO_dynlock_value*)MX_MALLOC(sizeof(CRYPTO_dynlock_value));
  if (l != NULL)
    ::InitializeCriticalSection(&(l->cs));
  return l;
}

static void my_dynlock_lock_callback(int mode, CRYPTO_dynlock_value* l, const char *file, int line)
{
  if (mode & CRYPTO_LOCK)
    ::EnterCriticalSection(&(l->cs));
  else
    ::LeaveCriticalSection(&(l->cs));
  return;
}

static void my_dynlock_destroy_callback(CRYPTO_dynlock_value* l, const char *file, int line)
{
  if (l != NULL)
    ::DeleteCriticalSection(&(l->cs));
  MX_FREE(l);
  return;
}
