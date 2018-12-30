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
#include "..\..\..\..\Include\ArrayList.h"
#include "..\..\..\..\Include\FnvHash.h"
#include "..\..\..\..\Include\TimedEventQueue.h"

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CJsSQLiteDatabase;

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

static LONG volatile nMutex = 0;
static MX::TArrayList<MX::Internals::CJsSQLiteDatabase*> cListSortedByHash;
static MX::TArrayList<MX::Internals::CJsSQLiteDatabase*> cListSortedByDatabase;
static MX::CSystemTimedEventQueue *lpTimedEventQueue = NULL;
static MX::TPendingListHelperGeneric<MX::CTimedEventQueue::CEvent*> cPendingEvents;

//-----------------------------------------------------------

static int InsertByHashCompare(_In_ LPVOID lpContext, _In_ MX::Internals::CJsSQLiteDatabase **lplpDB1,
                               _In_ MX::Internals::CJsSQLiteDatabase **lplpDB2);
static int SearchByHashCompare(_In_ LPVOID lpContext, _In_ ULONGLONG nHash,
                               _In_ MX::Internals::CJsSQLiteDatabase **lplpDB);
static int InsertByDatabaseCompare(_In_ LPVOID lpContext, _In_ MX::Internals::CJsSQLiteDatabase **lplpDB1,
                                   _In_ MX::Internals::CJsSQLiteDatabase **lplpDB2);
static int SearchByDatabaseCompare(_In_ LPVOID lpContext, _In_ sqlite3 *lpDb,
                                   _In_ MX::Internals::CJsSQLiteDatabase **lplpDB);
static VOID OnCloseTimeout(_In_ MX::CTimedEventQueue::CEvent *lpEvent);

static void myFuncUtcNow(sqlite3_context *ctx, int argc, sqlite3_value **argv);
static void myFuncNow(sqlite3_context *ctx, int argc, sqlite3_value **argv);
static void myFuncRegExp(sqlite3_context *ctx, int argc, sqlite3_value **argv);
static void myFuncConcatStep(sqlite3_context *ctx, int argc, sqlite3_value **argv);
static void myFuncConcatFinal(sqlite3_context *ctx, int argc, sqlite3_value **argv);

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CJsSQLiteDatabase : public TRefCounted<CBaseMemObj>
{
public:
  CJsSQLiteDatabase(_In_ ULONGLONG _nHash, _In_ DWORD _dwCloseTimeoutMs) : TRefCounted<CBaseMemObj>()
    {
    nHash = _nHash;
    lpDB = NULL;
    dwCloseTimeoutMs = _dwCloseTimeoutMs;
    return;
    };

  ~CJsSQLiteDatabase()
    {
    SIZE_T nIndex;

    nIndex = cListSortedByHash.BinarySearch(nHash, &SearchByHashCompare);
    if (nIndex != (SIZE_T)-1)
      cListSortedByHash.RemoveElementAt(nIndex);
    if (lpDB != NULL)
    {
      nIndex = cListSortedByDatabase.BinarySearch(lpDB, &SearchByDatabaseCompare);
      if (nIndex != (SIZE_T)-1)
        cListSortedByDatabase.RemoveElementAt(nIndex);

      sqlite3_close(lpDB);
    }
    return;
    };

public:
  ULONGLONG nHash;
  sqlite3 *lpDB;
  DWORD dwCloseTimeoutMs;
};

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

namespace MX {

namespace Internals {

namespace JsSQLiteDbManager {

HRESULT Open(_In_z_ LPCSTR szFileNameA, _In_ BOOL bReadOnly, _In_ BOOL bDontCreate, _In_ DWORD dwCloseTimeoutMs,
             _Out_ sqlite3 **lplpDB, _Out_ int *lpnSQLiteErr)
{
  MX::CFastLock cLock(&nMutex);
  MX::TAutoRefCounted<MX::Internals::CJsSQLiteDatabase> cDatabase;
  ULONGLONG nHash;
  BYTE bVal;
  SIZE_T nIndex;
  HRESULT hRes;

  if (lplpDB != NULL)
    *lplpDB = NULL;
  if (lpnSQLiteErr != NULL)
    *lpnSQLiteErr = SQLITE_ERROR;

  if (szFileNameA == NULL || lplpDB == NULL || lpnSQLiteErr == NULL)
    return E_POINTER;
  if (*szFileNameA == 0)
    return E_INVALIDARG;
  nHash = fnv_64a_buf(szFileNameA, MX::StrLenA(szFileNameA), FNV1A_64_INIT);
  bVal = (bDontCreate != FALSE) ? 1 : 0;
  nHash = fnv_64a_buf(&bVal, sizeof(bVal), nHash);

  if (lpTimedEventQueue == NULL)
  {
    lpTimedEventQueue = MX::CSystemTimedEventQueue::Get();
    if (lpTimedEventQueue == NULL)
    {
      *lpnSQLiteErr = SQLITE_NOMEM;
      return E_OUTOFMEMORY;
    }
  }

  //lookup already opened database
  nIndex = cListSortedByHash.BinarySearch(nHash, &SearchByHashCompare);
  if (nIndex == (SIZE_T)-1)
  {
    int err, flags;

    cDatabase.Attach(MX_DEBUG_NEW MX::Internals::CJsSQLiteDatabase(nHash, dwCloseTimeoutMs));
    if (!cDatabase)
      return E_OUTOFMEMORY;

    //open database
    flags = SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_SHAREDCACHE;
    if (bDontCreate == FALSE)
      flags |= SQLITE_OPEN_CREATE;
    flags |= (bReadOnly == FALSE) ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY;

    //NOTE: Docs says the name must be UTF-8
    err = sqlite3_open_v2(szFileNameA, &(cDatabase->lpDB), flags, NULL);
    if (err != SQLITE_OK)
    {
      hRes = S_OK;
      if (cDatabase->lpDB)
      {
        hRes = MX_HRESULT_FROM_WIN32(sqlite3_system_errno(cDatabase->lpDB));
      }
      if (SUCCEEDED(hRes))
      {
        if (err == SQLITE_NOTFOUND || err == SQLITE_CANTOPEN)
          hRes = MX_E_NotFound;
        else if (err == SQLITE_NOMEM)
          hRes = E_OUTOFMEMORY;
        else
          hRes = E_FAIL;
      }
      *lpnSQLiteErr = err;
      return hRes;
    }
    sqlite3_extended_result_codes(cDatabase->lpDB, 1);

    err = sqlite3_create_function(cDatabase->lpDB, "UTC_TIMESTAMP", 0, SQLITE_UTF8, 0, myFuncUtcNow, NULL, 0);
    if (err == SQLITE_OK)
    {
      err = sqlite3_create_function(cDatabase->lpDB, "NOW", 0, SQLITE_UTF8, 0, myFuncNow, 0, 0);
    }
    if (err == SQLITE_OK)
    {
      err = sqlite3_create_function(cDatabase->lpDB, "CURRENT_TIMESTAMP", 0, SQLITE_UTF8, 0, myFuncNow, 0, 0);
    }
    if (err == SQLITE_OK)
    {
      err = sqlite3_create_function(cDatabase->lpDB, "REGEXP", 2, SQLITE_UTF8, 0, myFuncRegExp, 0, 0);
    }
    if (err == SQLITE_OK)
    {
      err = sqlite3_create_function(cDatabase->lpDB, "CONCAT", -1, SQLITE_UTF8, 0, NULL, myFuncConcatStep,
                                    myFuncConcatFinal);
    }
    if (err != SQLITE_OK)
    {
      hRes = MX_HRESULT_FROM_WIN32(sqlite3_system_errno(cDatabase->lpDB));
      *lpnSQLiteErr = err;
      return hRes;
    }

    //insert into lists
    if (cListSortedByHash.SortedInsert(cDatabase.Get(), &InsertByHashCompare) == FALSE ||
        cListSortedByDatabase.SortedInsert(cDatabase.Get(), &InsertByDatabaseCompare) == FALSE)
    {
      *lpnSQLiteErr = SQLITE_NOMEM;
      return E_OUTOFMEMORY;
    }
  }
  else
  {
    cDatabase = cListSortedByHash.GetElementAt(nIndex);
  }

  //done
  *lplpDB = cDatabase.Detach()->lpDB;
  *lpnSQLiteErr = SQLITE_OK;
  return S_OK;
}

VOID Close(_In_ sqlite3 *lpDB)
{
  if (lpDB != NULL)
  {
    MX::CFastLock cLock(&nMutex);
    MX::TAutoRefCounted<MX::Internals::CJsSQLiteDatabase> cDatabase;
    MX::CTimedEventQueue::CEvent *lpCloseEvent;
    SIZE_T nIndex;

    nIndex = cListSortedByDatabase.BinarySearch(lpDB, &SearchByDatabaseCompare);
    if (nIndex == (SIZE_T)-1)
      return;

    cDatabase.Attach(cListSortedByDatabase.GetElementAt(nIndex));

    //initiate a new close on timeout
    lpCloseEvent = MX_DEBUG_NEW MX::CTimedEventQueue::CEvent(MX_BIND_CALLBACK(&OnCloseTimeout), cDatabase.Get());
    if (lpCloseEvent == NULL)
      return; //cDatabase will de-ref now and close the DB (keep cLock before cDatabase to ensure lock)
    if (FAILED(cPendingEvents.Add(lpCloseEvent)))
    {
      lpCloseEvent->Release();
      return; //cDatabase will de-ref now and close the DB (keep cLock before cDatabase to ensure lock)
    }

    cDatabase->AddRef();
    if (FAILED(lpTimedEventQueue->Add(lpCloseEvent, cDatabase->dwCloseTimeoutMs)))
    {
      //if cannot initiate, remove from the list and close now
      cPendingEvents.Remove(lpCloseEvent);
      lpCloseEvent->Release();
      cDatabase->Release(); //decrement extra ref
      return; //cDatabase will de-ref now and close the DB (keep cLock before cDatabase to ensure lock)
    }
  }
  return;
}

VOID Shutdown()
{
  cPendingEvents.WaitAll();
  MX_ASSERT(cListSortedByHash.GetCount() == 0);
  MX_ASSERT(cListSortedByDatabase.GetCount() == 0);
  MX_RELEASE(lpTimedEventQueue);
  return;
}

} //namespace DbManager

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

static int InsertByHashCompare(_In_ LPVOID lpContext, _In_ MX::Internals::CJsSQLiteDatabase **lplpDB1,
                               _In_ MX::Internals::CJsSQLiteDatabase **lplpDB2)
{
  if ((*lplpDB1)->nHash < (*lplpDB2)->nHash)
    return -1;
  if ((*lplpDB1)->nHash > (*lplpDB2)->nHash)
    return 1;
  return 0;
}

static int SearchByHashCompare(_In_ LPVOID lpContext, _In_ ULONGLONG nHash,
                               _In_ MX::Internals::CJsSQLiteDatabase **lplpDB)
{
  if (nHash < (*lplpDB)->nHash)
    return -1;
  if (nHash > (*lplpDB)->nHash)
    return 1;
  return 0;
}

static int InsertByDatabaseCompare(_In_ LPVOID lpContext, _In_ MX::Internals::CJsSQLiteDatabase **lplpDB1,
                                   _In_ MX::Internals::CJsSQLiteDatabase **lplpDB2)
{
  if ((SIZE_T)((*lplpDB1)->lpDB) < (SIZE_T)((*lplpDB2)->lpDB))
    return -1;
  if ((SIZE_T)((*lplpDB1)->lpDB) > (SIZE_T)((*lplpDB2)->lpDB))
    return 1;
  return 0;
}

static int SearchByDatabaseCompare(_In_ LPVOID lpContext, _In_ sqlite3 *lpDb,
                                   _In_ MX::Internals::CJsSQLiteDatabase **lplpDB)
{
  if ((SIZE_T)lpDb < (SIZE_T)((*lplpDB)->lpDB))
    return -1;
  if ((SIZE_T)lpDb > (SIZE_T)((*lplpDB)->lpDB))
    return 1;
  return 0;
}

static VOID OnCloseTimeout(_In_ MX::CTimedEventQueue::CEvent *lpEvent)
{
  MX::CFastLock cLock(&nMutex);
  MX::Internals::CJsSQLiteDatabase *lpDatabase;

  lpDatabase = (MX::Internals::CJsSQLiteDatabase*)(lpEvent->GetUserData());

  //if refcount reaches zero, close database, else another open happened while timeout was about to occur
  //no need to check if the event was canceled because the refcounting will do all the job
  lpDatabase->Release();

  cPendingEvents.Remove(lpEvent);
  lpEvent->Release();
  return;
}

static void myFuncUtcNow(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  return;
}

static void myFuncNow(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  return;
}

static void myFuncRegExp(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  return;
}

static void myFuncConcatStep(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  return;
}

static void myFuncConcatFinal(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  return;
}
