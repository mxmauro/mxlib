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

//-----------------------------------------------------------

static VOID SQLITE_Shutdown();

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

static VOID SQLITE_Shutdown()
{
  MX::CFastLock cLock(&nMutex);

  if (__InterlockedRead(&nInitialized) != 0)
  {
    MX::Internals::JsSQLiteDbManager::Shutdown();

    sqlite3_shutdown();

    _InterlockedExchange(&nInitialized, 0);
  }
  return;
}
