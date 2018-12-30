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
#ifndef _MX_JS_SQLITE_PLUGIN_COMMON_H
#define _MX_JS_SQLITE_PLUGIN_COMMON_H

#include "..\..\..\..\Include\JsLib\Plugins\JsSQLitePlugin.h"
#include "..\..\..\..\Include\ArrayList.h"
#include "internals\config.h"
#include "internals\sqlite3.h"

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CJsSQLiteFieldInfo : public CJsObjectBase
{
public:
  CJsSQLiteFieldInfo(_In_ DukTape::duk_context *lpCtx);
  ~CJsSQLiteFieldInfo();

  MX_JS_DECLARE(CJsSQLiteFieldInfo, "SQLiteLFieldInfo")

  MX_JS_BEGIN_MAP(CJsSQLiteFieldInfo)
    MX_JS_MAP_PROPERTY("name", &CJsSQLiteFieldInfo::getName, NULL, TRUE)
    MX_JS_MAP_PROPERTY("originalName", &CJsSQLiteFieldInfo::getOriginalName, NULL, TRUE)
    MX_JS_MAP_PROPERTY("table", &CJsSQLiteFieldInfo::getTable, NULL, TRUE)
    MX_JS_MAP_PROPERTY("originalTable", &CJsSQLiteFieldInfo::getOriginalTable, NULL, TRUE)
    MX_JS_MAP_PROPERTY("database", &CJsSQLiteFieldInfo::getDatabase, NULL, TRUE)
    MX_JS_MAP_PROPERTY("isNumeric", &CJsSQLiteFieldInfo::getIsNumeric, NULL, TRUE)
    MX_JS_MAP_PROPERTY("type", &CJsSQLiteFieldInfo::getType, NULL, TRUE)
  MX_JS_END_MAP()

private:
  DukTape::duk_ret_t getName();
  DukTape::duk_ret_t getOriginalName();
  DukTape::duk_ret_t getTable();
  DukTape::duk_ret_t getOriginalTable();
  DukTape::duk_ret_t getDatabase();
  DukTape::duk_ret_t getIsNumeric();
  DukTape::duk_ret_t getType();

private:
  friend class CJsSQLitePlugin;

  CStringA cStrNameA, cStrOriginalNameA;
  CStringA cStrTableA;
  CStringA cStrDatabaseA;
  int nType, nRealType, nCurrType;
};

//-----------------------------------------------------------

/*
namespace JsSQLiteDbManager {

HRESULT Open(_In_z_ LPCSTR szFileNameA, _In_ BOOL bReadOnly, _In_ BOOL bDontCreate, _In_ DWORD dwCloseTimeoutMs,
             _Out_ sqlite3 **lplpDB, _Out_ int *lpnSQLiteErr);
VOID Close(_In_ sqlite3 *lpDB);

VOID Shutdown();

} //namespace DbManager
*/

//-----------------------------------------------------------

namespace API {

HRESULT SQLiteInitialize();
VOID SQLiteInitializeMemory();

} //namespace API

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JS_SQLITE_PLUGIN_COMMON_H
