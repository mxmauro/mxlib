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
#include "JsMySqlPluginCommon.h"
#include <locale.h>
#include "..\..\..\Include\Finalizer.h"

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

lpfn_mysql_init               fn_mysql_init = NULL;
lpfn_mysql_options            fn_mysql_options = NULL;
lpfn_mysql_real_connect       fn_mysql_real_connect = NULL;
lpfn_mysql_select_db          fn_mysql_select_db = NULL;
lpfn_mysql_close              fn_mysql_close = NULL;
lpfn_mysql_real_escape_string fn_mysql_real_escape_string = NULL;
lpfn_mysql_escape_string      fn_mysql_escape_string = NULL;
lpfn_mysql_real_query         fn_mysql_real_query = NULL;
lpfn_mysql_store_result       fn_mysql_store_result = NULL;
lpfn_mysql_use_result         fn_mysql_use_result = NULL;
lpfn_mysql_free_result        fn_mysql_free_result = NULL;
lpfn_mysql_field_count        fn_mysql_field_count = NULL;
lpfn_mysql_affected_rows      fn_mysql_affected_rows = NULL;
lpfn_mysql_num_rows           fn_mysql_num_rows = NULL;
lpfn_mysql_num_fields         fn_mysql_num_fields = NULL;
lpfn_mysql_fetch_row          fn_mysql_fetch_row = NULL;
lpfn_mysql_fetch_lengths      fn_mysql_fetch_lengths = NULL;
lpfn_mysql_fetch_fields       fn_mysql_fetch_fields = NULL;
lpfn_mysql_thread_id          fn_mysql_thread_id = NULL;
lpfn_mysql_insert_id          fn_mysql_insert_id = NULL;
lpfn_mysql_set_character_set  fn_mysql_set_character_set = NULL;
lpfn_mysql_errno              fn_mysql_errno = NULL;
lpfn_mysql_error              fn_mysql_error = NULL;

//-----------------------------------------------------------

HRESULT Initialize()
{
  if (hDll == NULL)
  {
    MX::CFastLock cLock(&nMutex);
    CStringW cStrTempW;
    HINSTANCE _hDll;
    LPWSTR sW;
    DWORD dw;
    HRESULT hRes;

    if (hDll == NULL)
    {
      lpfn_mysql_init               _fn_mysql_init = NULL;
      lpfn_mysql_options            _fn_mysql_options = NULL;
      lpfn_mysql_real_connect       _fn_mysql_real_connect = NULL;
      lpfn_mysql_select_db          _fn_mysql_select_db = NULL;
      lpfn_mysql_close              _fn_mysql_close = NULL;
      lpfn_mysql_real_escape_string _fn_mysql_real_escape_string = NULL;
      lpfn_mysql_escape_string      _fn_mysql_escape_string = NULL;
      lpfn_mysql_real_query         _fn_mysql_real_query = NULL;
      lpfn_mysql_store_result       _fn_mysql_store_result = NULL;
      lpfn_mysql_use_result         _fn_mysql_use_result = NULL;
      lpfn_mysql_free_result        _fn_mysql_free_result = NULL;
      lpfn_mysql_field_count        _fn_mysql_field_count = NULL;
      lpfn_mysql_affected_rows      _fn_mysql_affected_rows = NULL;
      lpfn_mysql_num_rows           _fn_mysql_num_rows = NULL;
      lpfn_mysql_num_fields         _fn_mysql_num_fields = NULL;
      lpfn_mysql_fetch_row          _fn_mysql_fetch_row = NULL;
      lpfn_mysql_fetch_lengths      _fn_mysql_fetch_lengths = NULL;
      lpfn_mysql_fetch_fields       _fn_mysql_fetch_fields = NULL;
      lpfn_mysql_thread_id          _fn_mysql_thread_id = NULL;
      lpfn_mysql_insert_id          _fn_mysql_insert_id = NULL;
      lpfn_mysql_set_character_set  _fn_mysql_set_character_set = NULL;
      lpfn_mysql_errno              _fn_mysql_errno = NULL;
      lpfn_mysql_error              _fn_mysql_error = NULL;
      MYSQL *lpMySql;

      //get application path
      if (cStrTempW.EnsureBuffer(2100) == FALSE)
        return E_OUTOFMEMORY;
      dw = ::GetModuleFileNameW(NULL, (LPWSTR)cStrTempW, 2048);
      ((LPWSTR)cStrTempW)[dw] = 0;
      sW = (LPWSTR)MX::StrChrW((LPWSTR)cStrTempW, L'\\', TRUE);
      if (sW != NULL)
        *(sW+1) = 0;
      cStrTempW.Refresh();
      if (cStrTempW.Concat(L"libmysql.dll") == FALSE)
        return E_OUTOFMEMORY;
      //load library
      _hDll = ::LoadLibraryW((LPCWSTR)cStrTempW);
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
#undef LOAD_API
      dw = 1;

afterLoad:
      if (dw == 0)
      {
        ::FreeLibrary(_hDll);
        return MX_E_ProcNotFound;
      }

      //initialize library
      lpMySql = _fn_mysql_init(NULL);
      if (lpMySql != NULL)
      {
        _fn_mysql_close(lpMySql);
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
      fn_mysql_init               = _fn_mysql_init;
      fn_mysql_options            = _fn_mysql_options;
      fn_mysql_real_connect       = _fn_mysql_real_connect;
      fn_mysql_select_db          = _fn_mysql_select_db;
      fn_mysql_close              = _fn_mysql_close;
      fn_mysql_real_escape_string = _fn_mysql_real_escape_string;
      fn_mysql_escape_string      = _fn_mysql_escape_string;
      fn_mysql_real_query         = _fn_mysql_real_query;
      fn_mysql_store_result       = _fn_mysql_store_result;
      fn_mysql_use_result         = _fn_mysql_use_result;
      fn_mysql_free_result        = _fn_mysql_free_result;
      fn_mysql_field_count        = _fn_mysql_field_count;
      fn_mysql_affected_rows      = _fn_mysql_affected_rows;
      fn_mysql_num_rows           = _fn_mysql_num_rows;
      fn_mysql_num_fields         = _fn_mysql_num_fields;
      fn_mysql_fetch_row          = _fn_mysql_fetch_row;
      fn_mysql_fetch_lengths      = _fn_mysql_fetch_lengths;
      fn_mysql_fetch_fields       = _fn_mysql_fetch_fields;
      fn_mysql_thread_id          = _fn_mysql_thread_id;
      fn_mysql_insert_id          = _fn_mysql_insert_id;
      fn_mysql_set_character_set  = _fn_mysql_set_character_set;
      fn_mysql_errno              = _fn_mysql_errno;
      fn_mysql_error              = _fn_mysql_error;
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
