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
#ifndef _MX_JS_SQLITE_PLUGIN_COMMON_H
#define _MX_JS_SQLITE_PLUGIN_COMMON_H

#include "..\..\..\..\Include\JsLib\Plugins\JsSQLitePlugin.h"
#include "..\..\..\..\Include\ArrayList.h"
#include "internals\config.h"
#include "internals\sqlite3.h"

//-----------------------------------------------------------

namespace MX {

namespace Internals {

class CJsSQLiteFieldInfo : public CJsObjectBase, public CNonCopyableObj
{
public:
  CJsSQLiteFieldInfo();
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
  DukTape::duk_ret_t getName(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t getOriginalName(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t getTable(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t getOriginalTable(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t getDatabase(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t getIsNumeric(_In_ DukTape::duk_context *lpCtx);
  DukTape::duk_ret_t getType(_In_ DukTape::duk_context *lpCtx);

private:
  friend class CJsSQLitePlugin;

  CStringA cStrNameA, cStrOriginalNameA;
  CStringA cStrTableA;
  CStringA cStrDatabaseA;
  int nType, nRealType, nCurrType;
};

//-----------------------------------------------------------

namespace API {

HRESULT SQLiteInitialize();
VOID SQLiteInitializeMemory();

} //namespace API

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JS_SQLITE_PLUGIN_COMMON_H
