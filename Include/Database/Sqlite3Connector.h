#ifndef _MX_SQLITE3_CONNECTOR_H
#define _MX_SQLITE3_CONNECTOR_H

#include "BaseConnector.h"

//-----------------------------------------------------------

namespace MX {

namespace Database {

class CSQLite3Connector : public CBaseConnector
{
public:
  class CConnectOptions : public virtual CBaseMemObj
  {
  public:
    CConnectOptions();

  public:
    BOOL bDontCreateIfNotExists;
    BOOL bReadOnly;
    DWORD dwBusyTimeoutMs;
  };

public:
  enum class eTxType
  {
    Standard, Immediate, Exclusive
  };

public:
  CSQLite3Connector();
  ~CSQLite3Connector();

  HRESULT Connect(_In_z_ LPCSTR szFileNameA, _In_opt_ CConnectOptions *lpOptions = NULL);
  HRESULT Connect(_In_z_ LPCWSTR szFileNameW, _In_opt_ CConnectOptions *lpOptions = NULL);
  VOID Disconnect();

  BOOL IsConnected() const;

  int GetErrorCode() const;
  LPCSTR GetErrorDescription() const;

  HRESULT QueryExecute(_In_ LPCSTR szQueryA, _In_opt_ SIZE_T nQueryLen = (SIZE_T)-1,
                       _In_opt_ CFieldList *lpInputFieldsList = NULL);
  using CBaseConnector::QueryExecute;

  HRESULT FetchRow();

  VOID QueryClose();

  HRESULT TransactionStart();
  HRESULT TransactionStart(_In_ eTxType nType);
  HRESULT TransactionCommit();
  HRESULT TransactionRollback();

  HRESULT EscapeString(_Out_ CStringA &cStrA, _In_ LPCSTR szStrA, _In_opt_ SIZE_T nStrLen = (SIZE_T)-1,
                       _In_opt_ BOOL bIsLike = FALSE);
  HRESULT EscapeString(_Out_ CStringW &cStrW, _In_ LPCWSTR szStrW, _In_opt_ SIZE_T nStrLen = (SIZE_T)-1,
                       _In_opt_ BOOL bIsLike = FALSE);

private:
  class CSQLite3Column : public CColumn
  {
  public:
    CSQLite3Column() : CColumn()
      {
      nType = nRealType = nCurrType = nFlags = 0;
      return;
      };

  private:
    friend class CSQLite3Connector;

    ULONG nType, nRealType, nCurrType, nFlags;
  };

private:
  LPVOID lpInternalData;
};

} //namespace Database

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_SQLITE3_CONNECTOR_H
