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
#ifndef _MX_BASE_DB_CONNECTOR_H
#define _MX_BASE_DB_CONNECTOR_H

#include "..\Defines.h"
#include "..\DateTime\DateTime.h"
#include "..\ArrayList.h"
#include "..\RefCounted.h"

//-----------------------------------------------------------

namespace MX {

namespace Database {

enum class eFieldType
{
  Null = 0, String, Boolean,
  UInt32, Int32, UInt64, Int64,
  Double, DateTime, Blob
};

//-----------------------------------------------------------

class CField : public virtual TRefCounted<CBaseMemObj>, public CNonCopyableObj
{
public:
  CField();
  ~CField();

  VOID SetNull();
  HRESULT SetString(_In_ LPCSTR szStrA, _In_ SIZE_T nLength = (SIZE_T)-1);
  HRESULT SetString(_In_ LPCWSTR szStrW, _In_ SIZE_T nLength = (SIZE_T)-1);
  HRESULT SetFormattedString(_In_ LPCSTR szFormatA, ...);
  HRESULT SetFormattedString(_In_ LPCWSTR szFormatW, ...);
  HRESULT SetFormattedStringV(_In_ LPCSTR szFormatA, _In_ va_list args);
  HRESULT SetFormattedStringV(_In_ LPCWSTR szFormatW, _In_ va_list args);
  VOID SetBoolean(_In_ BOOL bValue);
  VOID SetUInt32(_In_ ULONG nValue);
  VOID SetInt32(_In_ LONG nValue);
  VOID SetUInt64(_In_ ULONGLONG nValue);
  VOID SetInt64(_In_ LONGLONG nValue);
  VOID SetDouble(_In_ double nValue);
  HRESULT SetDateTime(_In_ CDateTime &cDt);
  HRESULT SetBlob(_In_ LPVOID lpData, _In_ SIZE_T nLength);

  eFieldType GetType() const;

  SIZE_T GetLength() const;

  BOOL GetBoolean() const;
  LPCSTR GetString() const;
  HRESULT GetString(_Out_ CStringW &cStrW);
  ULONG GetUInt32() const;
  LONG GetInt32() const;
  ULONGLONG GetUInt64() const;
  LONGLONG GetInt64() const;
  double GetDouble() const;
  CDateTime* GetDateTime() const;
  LPBYTE GetBlob() const;

  BOOL GetAsBoolean(_Out_ PBOOL lpbValue);
  BOOL GetAsUInt32(_Out_ PULONG lpnValue);
  BOOL GetAsInt32(_Out_ PLONG lpnValue) const;
  BOOL GetAsUInt64(_Out_ PULONGLONG lpnValue) const;
  BOOL GetAsInt64(_Out_ PLONGLONG lpnValue) const;
  BOOL GetAsDouble(_Out_ double *lpnValue) const;

private:
  BOOL EnsureSize(_In_ SIZE_T nNewSize);

private:
  eFieldType nFieldType{ eFieldType::Null };
  union {
    LPCSTR szStrA{ NULL };
    BOOL b;
    ULONG ul;
    LONG l;
    ULONGLONG ull;
    LONGLONG ll;
    double dbl;
    LPVOID lpBlob;
  };
  CDateTime *lpDt{ NULL };
  SIZE_T nLength{ 0 };
  LPBYTE lpBuffer{ NULL };
  SIZE_T nBufferSize{ 0 };
};

//-----------------------------------------------------------

class CFieldList : public TArrayListWithRelease<CField*>
{
public:
  CFieldList();

  HRESULT AddNull();
  HRESULT AddString(_In_ LPCSTR szStrA, _In_ SIZE_T nLength = (SIZE_T)-1);
  HRESULT AddString(_In_ LPCWSTR szStrW, _In_ SIZE_T nLength = (SIZE_T)-1);
  HRESULT AddFormattedString(_In_ LPCSTR szFormatA, ...);
  HRESULT AddFormattedString(_In_ LPCWSTR szFormatW, ...);
  HRESULT AddFormattedStringV(_In_ LPCSTR szFormatA, _In_ va_list args);
  HRESULT AddFormattedStringV(_In_ LPCWSTR szFormatW, _In_ va_list args);
  HRESULT AddBoolean(_In_ BOOL bValue);
  HRESULT AddUInt32(_In_ ULONG nValue);
  HRESULT AddInt32(_In_ LONG nValue);
  HRESULT AddUInt64(_In_ ULONGLONG nValue);
  HRESULT AddInt64(_In_ LONGLONG nValue);
  HRESULT AddDouble(_In_ double nValue);
  HRESULT AddDateTime(_In_ CDateTime &cDt);
  HRESULT AddBlob(_In_ LPVOID lpData, _In_ SIZE_T nLength);
};

//-----------------------------------------------------------

class CBaseConnector : public virtual TRefCounted<CBaseMemObj>, public CNonCopyableObj
{
protected:
  CBaseConnector();

public:
  ~CBaseConnector();

  virtual BOOL IsConnected() const = 0;

  virtual HRESULT QueryExecute(_In_ LPCSTR szQueryA, _In_ SIZE_T nQueryLen = (SIZE_T)-1,
                               _In_ CFieldList *lpInputFieldsList = NULL) = 0;
  HRESULT QueryExecute(_In_ LPCWSTR szQueryW, _In_ SIZE_T nQueryLen = (SIZE_T)-1,
                       _In_ CFieldList *lpInputFieldsList = NULL);

  ULONGLONG GetAffectedRows() const;
  ULONGLONG GetLastInsertId() const;

  SIZE_T GetFieldsCount() const;

  CField* GetField(_In_ SIZE_T nColumnIndex);
  CField* GetFieldByName(_In_z_ LPCSTR szNameA);
  LPCSTR GetFieldName(_In_ SIZE_T nColumnIndex);
  LPCSTR GetFieldTableName(_In_ SIZE_T nColumnIndex);

  SIZE_T GetColumnIndex(_In_z_ LPCSTR szNameA);

  virtual HRESULT FetchRow() = 0;

  virtual VOID QueryClose();

  virtual HRESULT TransactionStart() = 0;
  virtual HRESULT TransactionCommit() = 0;
  virtual HRESULT TransactionRollback() = 0;

  virtual HRESULT EscapeString(_Out_ CStringA &cStrA, _In_ LPCSTR szStrA, _In_opt_ SIZE_T nStrLen = (SIZE_T)-1,
                               _In_opt_ BOOL bIsLike = FALSE) = 0;
  virtual HRESULT EscapeString(_Out_ CStringW &cStrW, _In_ LPCWSTR szStrW, _In_opt_ SIZE_T nStrLen = (SIZE_T)-1,
                               _In_opt_ BOOL bIsLike = FALSE) = 0;

protected:
  class CColumn : public virtual CBaseMemObj, public CNonCopyableObj
  {
  public:
    CColumn() : CBaseMemObj(), CNonCopyableObj()
      {
      cField.Attach(MX_DEBUG_NEW CField());
      return;
      };

  protected:
    friend class CBaseConnector;

    CStringA cStrNameA;
    CStringA cStrTableA;
    TAutoRefCounted<CField> cField;
  };

protected:
  TArrayListWithDelete<CColumn*> aColumnsList;
  ULONGLONG ullAffectedRows;
  ULONGLONG ullLastInsertId;
};

} //namespace Database

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_BASE_DB_CONNECTOR_H
