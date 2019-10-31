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
#include "..\JsSQLitePluginCommon.h"
extern "C" {
  #include "sqlite3.h"
};

//-----------------------------------------------------------

static void* x_alloc(int nByte)
{
  void *p = MX_MALLOC((SIZE_T)nByte);
  if (p == NULL) {
    sqlite3_log(SQLITE_NOMEM, "failed to allocate %u bytes of memory", nByte);
  }
  return p;
}

static void x_free(void *pPrior)
{
  MX_FREE(pPrior);
  return;
}

static int x_memsize(void *pPrior)
{
  return (int)::MxMemSize(pPrior);
}

static void* x_realloc(void *pPrior, int nByte)
{
  void *p = MX_REALLOC(pPrior, (SIZE_T)nByte);
  if (p == NULL) {
    sqlite3_log(SQLITE_NOMEM, "failed memory resize %u to %u bytes", x_memsize(pPrior), nByte);
  }
  return p;
}

static int x_roundup(int n)
{
  return (n + 7) & (~7);
}

static int x_init(void *NotUsed)
{
  return SQLITE_OK;
}

static void x_shutdown(void *NotUsed)
{
  return;
}

//-----------------------------------------------------------

namespace MX {

namespace Internals {

namespace API {

VOID SQLiteInitializeMemory()
{
  static const sqlite3_mem_methods defaultMethods = {
    x_alloc, x_free, x_realloc, x_memsize, x_roundup, x_init, x_shutdown, 0
  };
  sqlite3_config(SQLITE_CONFIG_MALLOC, &defaultMethods);
  return;
}

} //namespace API

} //namespace Internals

} //namespace MX
