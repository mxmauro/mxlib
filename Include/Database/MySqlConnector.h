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
#ifndef _MX_MYSQL_CONNECTOR_H
#define _MX_MYSQL_CONNECTOR_H

#include "BaseConnector.h"

//-----------------------------------------------------------

namespace MX {

namespace Database {

class CMySqlConnector : public CBaseConnector
{
public:
  class CConnectOptions : public virtual CBaseMemObj
  {
  public:
    CConnectOptions();
  
  public:
    DWORD dwConnectTimeoutMs;
    DWORD dwReadTimeoutMs;
    DWORD dwWriteTimeoutMs;
  };

public:
  CMySqlConnector();
  ~CMySqlConnector();

  HRESULT Connect(_In_z_ LPCSTR szServerHostA, _In_z_ LPCSTR szUserNameA, _In_opt_z_ LPCSTR szUserPasswordA,
                  _In_opt_z_ LPCSTR szDatabaseNameA, _In_opt_ USHORT wServerPort = 3306,
                  _In_opt_ CConnectOptions *lpOptions = NULL);
  HRESULT Connect(_In_z_ LPCWSTR szServerHostW, _In_z_ LPCWSTR szUserNameW, _In_opt_z_ LPCWSTR szUserPasswordW,
                  _In_opt_z_ LPCWSTR szDatabaseNameW, _In_opt_ USHORT wServerPort = 3306,
                  _In_opt_ CConnectOptions *lpOptions = NULL);
  VOID Disconnect();

  BOOL IsConnected() const;

  int GetErrorCode() const;
  LPCSTR GetErrorDescription() const;
  LPCSTR GetSqlState() const;

  HRESULT SelectDatabase(_In_ LPCSTR szDatabaseNameA);
  HRESULT SelectDatabase(_In_ LPCWSTR szDatabaseNameW);

  HRESULT QueryExecute(_In_ LPCSTR szQueryA, _In_opt_ SIZE_T nQueryLen = (SIZE_T)-1,
                       _In_opt_ CFieldList *lpInputFieldsList = NULL);
  using CBaseConnector::QueryExecute;

  HRESULT FetchRow();

  VOID QueryClose();

  HRESULT TransactionStart();
  HRESULT TransactionStart(_In_ BOOL bReadOnly);
  HRESULT TransactionCommit();
  HRESULT TransactionRollback();

  HRESULT EscapeString(_Out_ CStringA &cStrA, _In_ LPCSTR szStrA, _In_opt_ SIZE_T nStrLen = (SIZE_T)-1,
                       _In_opt_ BOOL bIsLike = FALSE);
  HRESULT EscapeString(_Out_ CStringW &cStrW, _In_ LPCWSTR szStrW, _In_opt_ SIZE_T nStrLen = (SIZE_T)-1,
                       _In_opt_ BOOL bIsLike = FALSE);

private:
  class CMySqlColumn : public CColumn
  {
  public:
    CMySqlColumn() : CColumn()
      {
      nType = nFlags = 0;
      return;
      };

  private:
    friend class CMySqlConnector;

    ULONG nType, nFlags;
  };

private:
  LPVOID lpInternalData;
};

} //namespace Database

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_MYSQL_CONNECTOR_H
