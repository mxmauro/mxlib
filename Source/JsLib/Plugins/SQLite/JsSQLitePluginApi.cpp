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
#include "JsSQLitePluginCommon.h"
#include <locale.h>
#include "..\..\..\..\Include\Finalizer.h"
#include "..\..\..\..\Include\FnvHash.h"

//-----------------------------------------------------------

#define SQLITE_FINALIZER_PRIORITY 10001

//-----------------------------------------------------------

static LONG volatile nMutex = 0;
static LONG nInitialized = 0;
static MX::TArrayListWithRelease<MX::Internals::CJsSQLiteDatabase*> *lpOpenedDatabasesList = NULL;

//-----------------------------------------------------------

static VOID SQLITE_Shutdown();
static int DatabaseInsertCompare(_In_ LPVOID lpContext, _In_ MX::Internals::CJsSQLiteDatabase **lplpDB1,
                                 _In_ MX::Internals::CJsSQLiteDatabase **lplpDB2);
static int DatabaseSearchCompare(_In_ LPVOID lpContext, _In_ ULONGLONG nHash,
                                 _In_ MX::Internals::CJsSQLiteDatabase **lplpDB);

//-----------------------------------------------------------

namespace MX {

namespace Internals {

namespace API {

HRESULT SQLiteInitialize()
{
  if (__InterlockedRead(&nInitialized) == 0)
  {
    MX::CFastLock cLock(&nMutex);
    HRESULT hRes;

    if (__InterlockedRead(&nInitialized) == 0)
    {
      MX::Internals::API::SQLiteInitializeMemory();

      //register shutdown callback
      hRes = MX::RegisterFinalizer(&SQLITE_Shutdown, SQLITE_FINALIZER_PRIORITY);
      if (FAILED(hRes))
        return hRes;

      _InterlockedExchange(&nInitialized, 1);
    }
  }
  //done
  return S_OK;
};

} //namespace API

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

namespace MX {

namespace Internals {

CJsSQLiteDatabase::CJsSQLiteDatabase() : TRefCounted<CBaseMemObj>()
{
  lpDB = NULL;
  nHash = 0ui64;
  bReadOnly = FALSE;
  return;
}

CJsSQLiteDatabase::~CJsSQLiteDatabase()
{
  if (lpDB != NULL)
  {
    sqlite3_close(lpDB);
    lpDB = NULL;
  }
  return;
}

HRESULT CJsSQLiteDatabase::GetOrCreate(_In_z_ LPCSTR szFileNameA, _In_ BOOL bReadOnly, _In_ BOOL bDontCreate,
                                       _Deref_out_ CJsSQLiteDatabase **lplpDatabase)
{
  ULONGLONG nHash;

  if (lplpDatabase != NULL)
    *lplpDatabase = NULL;

  if (szFileNameA == NULL || lplpDatabase == NULL)
    return E_POINTER;
  if (*szFileNameA == 0)
    return E_INVALIDARG;
  nHash = fnv_64a_buf(szFileNameA, MX::StrLenA(szFileNameA), FNV1A_64_INIT);

  {
    MX::CFastLock cLock(&nMutex);
    CJsSQLiteDatabase **lpFoundDb;

    if (lpOpenedDatabasesList == NULL)
    {
      lpOpenedDatabasesList = MX_DEBUG_NEW MX::TArrayListWithRelease<CJsSQLiteDatabase*>();
      if (lpOpenedDatabasesList == NULL)
        return E_OUTOFMEMORY;
    }

    lpFoundDb = lpOpenedDatabasesList->BinarySearchPtr(nHash, &DatabaseSearchCompare);
    if (lpFoundDb != NULL)
    {
      if (bReadOnly != (*lpFoundDb)->bReadOnly)
        return E_ACCESSDENIED;

      (*lpFoundDb)->AddRef();
      *lplpDatabase = *lpFoundDb;
    }
    else
    {
      MX::TAutoRefCounted<CJsSQLiteDatabase> cNewDB;
      int err, flags;

      cNewDB.Attach(MX_DEBUG_NEW CJsSQLiteDatabase());
      if (!cNewDB)
        return E_OUTOFMEMORY;
      cNewDB->nHash = nHash;
      cNewDB->bReadOnly = bReadOnly;

      //open database
      flags = SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_SHAREDCACHE;
      if (bDontCreate == FALSE)
        flags |= SQLITE_OPEN_CREATE;
      flags |= (bReadOnly == FALSE) ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY;

      err = sqlite3_open_v2(szFileNameA, &(cNewDB->lpDB), flags, NULL);
      if (err != SQLITE_OK)
      {
        cNewDB->lpDB = NULL;
        if (err == SQLITE_NOTFOUND)
          return MX_E_NotFound;
        if (err == SQLITE_NOMEM)
          return E_OUTOFMEMORY;
        return E_FAIL;
      }
      sqlite3_extended_result_codes(cNewDB->lpDB, 1);
      //insert into opened list
      if (lpOpenedDatabasesList->SortedInsert(cNewDB.Get(), &DatabaseInsertCompare) == FALSE)
        return E_OUTOFMEMORY;
      *lplpDatabase = cNewDB.Detach();
      (*lplpDatabase)->AddRef();
    }
  }
  return S_OK;
}

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

static VOID SQLITE_Shutdown()
{
  MX::CFastLock cLock(&nMutex);

  if (__InterlockedRead(&nInitialized) != 0)
  {
    MX_DELETE(lpOpenedDatabasesList);

    sqlite3_shutdown();

    _InterlockedExchange(&nInitialized, 0);
  }
  return;
}

static int DatabaseInsertCompare(_In_ LPVOID lpContext, _In_ MX::Internals::CJsSQLiteDatabase **lplpDB1,
                                 _In_ MX::Internals::CJsSQLiteDatabase **lplpDB2)
{
  if ((*lplpDB1)->nHash < (*lplpDB2)->nHash)
    return -1;
  if ((*lplpDB1)->nHash > (*lplpDB2)->nHash)
    return 1;
  return 0;
}

static int DatabaseSearchCompare(_In_ LPVOID lpContext, _In_ ULONGLONG nHash,
                                 _In_ MX::Internals::CJsSQLiteDatabase **lplpDB)
{
  if (nHash < (*lplpDB)->nHash)
    return -1;
  if (nHash > (*lplpDB)->nHash)
    return 1;
  return 0;
}
