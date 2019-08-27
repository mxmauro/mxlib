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
#include "JsMySqlPluginCommon.h"
#include <locale.h>
#include "..\..\..\..\Include\Finalizer.h"

//-----------------------------------------------------------

#define LIBMYSQL_FINALIZER_PRIORITY 10001

//-----------------------------------------------------------

static LONG volatile nMutex = 0;

//-----------------------------------------------------------

static VOID LIBMYSQL_Shutdown();

//-----------------------------------------------------------

namespace MX {

namespace Internals {

namespace API {

static HINSTANCE hDll = NULL;

lpfn_mysql_init                  fn_mysql_init = NULL;
lpfn_mysql_options               fn_mysql_options = NULL;
lpfn_mysql_real_connect          fn_mysql_real_connect = NULL;
lpfn_mysql_select_db             fn_mysql_select_db = NULL;
lpfn_mysql_close                 fn_mysql_close = NULL;
lpfn_mysql_real_escape_string    fn_mysql_real_escape_string = NULL;
lpfn_mysql_escape_string         fn_mysql_escape_string = NULL;
lpfn_mysql_real_query            fn_mysql_real_query = NULL;
lpfn_mysql_store_result          fn_mysql_store_result = NULL;
lpfn_mysql_use_result            fn_mysql_use_result = NULL;
lpfn_mysql_free_result           fn_mysql_free_result = NULL;
lpfn_mysql_field_count           fn_mysql_field_count = NULL;
lpfn_mysql_affected_rows         fn_mysql_affected_rows = NULL;
lpfn_mysql_num_rows              fn_mysql_num_rows = NULL;
lpfn_mysql_num_fields            fn_mysql_num_fields = NULL;
lpfn_mysql_fetch_row             fn_mysql_fetch_row = NULL;
lpfn_mysql_fetch_lengths         fn_mysql_fetch_lengths = NULL;
lpfn_mysql_fetch_fields          fn_mysql_fetch_fields = NULL;
lpfn_mysql_thread_id             fn_mysql_thread_id = NULL;
lpfn_mysql_insert_id             fn_mysql_insert_id = NULL;
lpfn_mysql_set_character_set     fn_mysql_set_character_set = NULL;
lpfn_mysql_errno                 fn_mysql_errno = NULL;
lpfn_mysql_error                 fn_mysql_error = NULL;
lpfn_mysql_sqlstate              fn_mysql_sqlstate = NULL;
lpfn_mysql_stmt_init             fn_mysql_stmt_init = NULL;
lpfn_mysql_stmt_error            fn_mysql_stmt_error = NULL;
lpfn_mysql_stmt_errno            fn_mysql_stmt_errno = NULL;
lpfn_mysql_stmt_sqlstate         fn_mysql_stmt_sqlstate = NULL;
lpfn_mysql_stmt_close            fn_mysql_stmt_close = NULL;
lpfn_mysql_stmt_prepare          fn_mysql_stmt_prepare = NULL;
lpfn_mysql_stmt_bind_param       fn_mysql_stmt_bind_param = NULL;
lpfn_mysql_stmt_execute          fn_mysql_stmt_execute = NULL;
lpfn_mysql_stmt_affected_rows    fn_mysql_stmt_affected_rows = NULL;
lpfn_mysql_stmt_insert_id        fn_mysql_stmt_insert_id = NULL;
lpfn_mysql_stmt_fetch            fn_mysql_stmt_fetch = NULL;
lpfn_mysql_stmt_fetch_column     fn_mysql_stmt_fetch_column = NULL;
lpfn_mysql_stmt_store_result     fn_mysql_stmt_store_result = NULL;
lpfn_mysql_stmt_bind_result      fn_mysql_stmt_bind_result = NULL;
lpfn_mysql_stmt_param_count      fn_mysql_stmt_param_count = NULL;
lpfn_mysql_stmt_result_metadata  fn_mysql_stmt_result_metadata = NULL;
lpfn_mysql_stmt_attr_set         fn_mysql_stmt_attr_set = NULL;

//-----------------------------------------------------------

HRESULT MySqlInitialize()
{
  if (hDll == NULL)
  {
    MX::CFastLock cLock(&nMutex);
    HINSTANCE _hDll;
    DWORD dw;

    if (hDll == NULL)
    {
      lpfn_mysql_init                  _fn_mysql_init = NULL;
      lpfn_mysql_options               _fn_mysql_options = NULL;
      lpfn_mysql_real_connect          _fn_mysql_real_connect = NULL;
      lpfn_mysql_select_db             _fn_mysql_select_db = NULL;
      lpfn_mysql_close                 _fn_mysql_close = NULL;
      lpfn_mysql_real_escape_string    _fn_mysql_real_escape_string = NULL;
      lpfn_mysql_escape_string         _fn_mysql_escape_string = NULL;
      lpfn_mysql_real_query            _fn_mysql_real_query = NULL;
      lpfn_mysql_store_result          _fn_mysql_store_result = NULL;
      lpfn_mysql_use_result            _fn_mysql_use_result = NULL;
      lpfn_mysql_free_result           _fn_mysql_free_result = NULL;
      lpfn_mysql_field_count           _fn_mysql_field_count = NULL;
      lpfn_mysql_affected_rows         _fn_mysql_affected_rows = NULL;
      lpfn_mysql_num_rows              _fn_mysql_num_rows = NULL;
      lpfn_mysql_num_fields            _fn_mysql_num_fields = NULL;
      lpfn_mysql_fetch_row             _fn_mysql_fetch_row = NULL;
      lpfn_mysql_fetch_lengths         _fn_mysql_fetch_lengths = NULL;
      lpfn_mysql_fetch_fields          _fn_mysql_fetch_fields = NULL;
      lpfn_mysql_thread_id             _fn_mysql_thread_id = NULL;
      lpfn_mysql_insert_id             _fn_mysql_insert_id = NULL;
      lpfn_mysql_set_character_set     _fn_mysql_set_character_set = NULL;
      lpfn_mysql_errno                 _fn_mysql_errno = NULL;
      lpfn_mysql_error                 _fn_mysql_error = NULL;
      lpfn_mysql_sqlstate              _fn_mysql_sqlstate = NULL;
      lpfn_mysql_stmt_init             _fn_mysql_stmt_init = NULL;
      lpfn_mysql_stmt_error            _fn_mysql_stmt_error = NULL;
      lpfn_mysql_stmt_errno            _fn_mysql_stmt_errno = NULL;
      lpfn_mysql_stmt_sqlstate         _fn_mysql_stmt_sqlstate = NULL;
      lpfn_mysql_stmt_close            _fn_mysql_stmt_close = NULL;
      lpfn_mysql_stmt_prepare          _fn_mysql_stmt_prepare = NULL;
      lpfn_mysql_stmt_bind_param       _fn_mysql_stmt_bind_param = NULL;
      lpfn_mysql_stmt_execute          _fn_mysql_stmt_execute = NULL;
      lpfn_mysql_stmt_affected_rows    _fn_mysql_stmt_affected_rows = NULL;
      lpfn_mysql_stmt_insert_id        _fn_mysql_stmt_insert_id = NULL;
      lpfn_mysql_stmt_fetch            _fn_mysql_stmt_fetch = NULL;
      lpfn_mysql_stmt_fetch_column     _fn_mysql_stmt_fetch_column = NULL;
      lpfn_mysql_stmt_store_result     _fn_mysql_stmt_store_result = NULL;
      lpfn_mysql_stmt_bind_result      _fn_mysql_stmt_bind_result = NULL;
      lpfn_mysql_stmt_param_count      _fn_mysql_stmt_param_count = NULL;
      lpfn_mysql_stmt_result_metadata  _fn_mysql_stmt_result_metadata = NULL;
      lpfn_mysql_stmt_attr_set         _fn_mysql_stmt_attr_set = NULL;
      lpfn_mysql_get_client_version    _fn_mysql_get_client_version = NULL;
      MYSQL *lpSQLite;
      WCHAR szDllNameW[4096], *sW;
      DWORD dwLen;
      HRESULT hRes;

      //get application path
      dwLen = ::GetModuleFileNameW(NULL, szDllNameW, MX_ARRAYLEN(szDllNameW) - 20);
      if (dwLen == 0)
        return MX_E_ProcNotFound;
      szDllNameW[dwLen] = 0;
      sW = (LPWSTR)MX::StrChrW(szDllNameW, L'\\', TRUE);
      sW = (sW != NULL) ? (sW + 1) : szDllNameW;
      MX::MemCopy(sW, L"libmariadb.dll", (14 + 1) * sizeof(WCHAR));
      //load library
      _hDll = ::LoadLibraryW(szDllNameW);
      if (_hDll == NULL)
        return MX_HRESULT_FROM_LASTERROR();

      //load apis
#define LOAD_API(x) _fn_##x = (lpfn_##x)::GetProcAddress(_hDll, #x); \
      if (_fn_##x == NULL)                                           \
        goto afterLoad;
      dw = 0;
      LOAD_API(mysql_init);
      LOAD_API(mysql_options);
      LOAD_API(mysql_real_connect);
      LOAD_API(mysql_select_db);
      LOAD_API(mysql_close);
      LOAD_API(mysql_real_escape_string);
      LOAD_API(mysql_escape_string);
      LOAD_API(mysql_real_query);
      LOAD_API(mysql_store_result);
      LOAD_API(mysql_use_result);
      LOAD_API(mysql_free_result);
      LOAD_API(mysql_field_count);
      LOAD_API(mysql_affected_rows);
      LOAD_API(mysql_num_rows);
      LOAD_API(mysql_num_fields);
      LOAD_API(mysql_fetch_row);
      LOAD_API(mysql_fetch_lengths);
      LOAD_API(mysql_fetch_fields);
      LOAD_API(mysql_thread_id);
      LOAD_API(mysql_insert_id);
      LOAD_API(mysql_set_character_set);
      LOAD_API(mysql_errno);
      LOAD_API(mysql_error);
      LOAD_API(mysql_sqlstate);
      LOAD_API(mysql_stmt_init);
      LOAD_API(mysql_stmt_error);
      LOAD_API(mysql_stmt_errno);
      LOAD_API(mysql_stmt_sqlstate);
      LOAD_API(mysql_stmt_close);
      LOAD_API(mysql_stmt_prepare);
      LOAD_API(mysql_stmt_bind_param);
      LOAD_API(mysql_stmt_execute);
      LOAD_API(mysql_stmt_affected_rows);
      LOAD_API(mysql_stmt_insert_id);
      LOAD_API(mysql_stmt_fetch);
      LOAD_API(mysql_stmt_fetch_column);
      LOAD_API(mysql_stmt_store_result);
      LOAD_API(mysql_stmt_bind_result);
      LOAD_API(mysql_stmt_param_count);
      LOAD_API(mysql_stmt_result_metadata);
      LOAD_API(mysql_stmt_attr_set);
      LOAD_API(mysql_get_client_version);
#undef LOAD_API
      dw = 1;

afterLoad:
      if (dw == 0)
      {
        ::FreeLibrary(_hDll);
        return MX_E_ProcNotFound;
      }
      if (_fn_mysql_get_client_version() != MYSQL_VERSION_ID)
      {
        ::FreeLibrary(_hDll);
        return MX_E_Unsupported;
      }

      //initialize library
      lpSQLite = _fn_mysql_init(NULL);
      if (lpSQLite != NULL)
      {
        _fn_mysql_close(lpSQLite);
      }
      else
      {
        ::FreeLibrary(_hDll);
        return E_OUTOFMEMORY;
      }

      //register shutdown callback
      hRes = MX::RegisterFinalizer(&LIBMYSQL_Shutdown, LIBMYSQL_FINALIZER_PRIORITY);
      if (FAILED(hRes))
      {
        ::FreeLibrary(_hDll);
        return hRes;
      }

      //copy data
      fn_mysql_init                 = _fn_mysql_init;
      fn_mysql_options              = _fn_mysql_options;
      fn_mysql_real_connect         = _fn_mysql_real_connect;
      fn_mysql_select_db            = _fn_mysql_select_db;
      fn_mysql_close                = _fn_mysql_close;
      fn_mysql_real_escape_string   = _fn_mysql_real_escape_string;
      fn_mysql_escape_string        = _fn_mysql_escape_string;
      fn_mysql_real_query           = _fn_mysql_real_query;
      fn_mysql_store_result         = _fn_mysql_store_result;
      fn_mysql_use_result           = _fn_mysql_use_result;
      fn_mysql_free_result          = _fn_mysql_free_result;
      fn_mysql_field_count          = _fn_mysql_field_count;
      fn_mysql_affected_rows        = _fn_mysql_affected_rows;
      fn_mysql_num_rows             = _fn_mysql_num_rows;
      fn_mysql_num_fields           = _fn_mysql_num_fields;
      fn_mysql_fetch_row            = _fn_mysql_fetch_row;
      fn_mysql_fetch_lengths        = _fn_mysql_fetch_lengths;
      fn_mysql_fetch_fields         = _fn_mysql_fetch_fields;
      fn_mysql_thread_id            = _fn_mysql_thread_id;
      fn_mysql_insert_id            = _fn_mysql_insert_id;
      fn_mysql_set_character_set    = _fn_mysql_set_character_set;
      fn_mysql_errno                = _fn_mysql_errno;
      fn_mysql_error                = _fn_mysql_error;
      fn_mysql_sqlstate             = _fn_mysql_sqlstate;
      fn_mysql_stmt_init            = _fn_mysql_stmt_init;
      fn_mysql_stmt_error           = _fn_mysql_stmt_error;
      fn_mysql_stmt_errno           = _fn_mysql_stmt_errno;
      fn_mysql_stmt_sqlstate        = _fn_mysql_stmt_sqlstate;
      fn_mysql_stmt_close           = _fn_mysql_stmt_close;
      fn_mysql_stmt_prepare         = _fn_mysql_stmt_prepare;
      fn_mysql_stmt_bind_param      = _fn_mysql_stmt_bind_param;
      fn_mysql_stmt_execute         = _fn_mysql_stmt_execute;
      fn_mysql_stmt_affected_rows   = _fn_mysql_stmt_affected_rows;
      fn_mysql_stmt_insert_id       = _fn_mysql_stmt_insert_id;
      fn_mysql_stmt_fetch           = _fn_mysql_stmt_fetch;
      fn_mysql_stmt_fetch_column    = _fn_mysql_stmt_fetch_column;
      fn_mysql_stmt_store_result    = _fn_mysql_stmt_store_result;
      fn_mysql_stmt_bind_result     = _fn_mysql_stmt_bind_result;
      fn_mysql_stmt_param_count     = _fn_mysql_stmt_param_count;
      fn_mysql_stmt_result_metadata = _fn_mysql_stmt_result_metadata;
      fn_mysql_stmt_attr_set        = _fn_mysql_stmt_attr_set;

      hDll = _hDll;
    }
  }
  //done
  return S_OK;
};

} //namespace API

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

static VOID LIBMYSQL_Shutdown()
{
  if (MX::Internals::API::hDll != NULL)
  {
    ::FreeLibrary(MX::Internals::API::hDll);
    MX::Internals::API::hDll = NULL;
  }
  return;
}
