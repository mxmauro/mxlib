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
#include "..\..\include\Database\BaseConnector.h"
#include "..\..\Include\Strings\Utf8.h"

//-----------------------------------------------------------

namespace MX {

namespace Database {

CBaseConnector::CBaseConnector() : TRefCounted<CBaseMemObj>(), CNonCopyableObj()
{
  ullAffectedRows = ullLastInsertId = 0ui64;
  return;
}

CBaseConnector::~CBaseConnector()
{
  return;
}

HRESULT CBaseConnector::QueryExecute(_In_ LPCWSTR szQueryW, _In_ SIZE_T nQueryLen, _In_ CFieldList *lpInputFieldsList)
{
  CStringA cStrTempA;
  HRESULT hRes;

  hRes = Utf8_Encode(cStrTempA, szQueryW, nQueryLen);
  if (SUCCEEDED(hRes))
    hRes = QueryExecute((LPCSTR)cStrTempA, cStrTempA.GetLength(), lpInputFieldsList);
  return hRes;
}

ULONGLONG CBaseConnector::GetAffectedRows() const
{
  return ullAffectedRows;
}

ULONGLONG CBaseConnector::GetLastInsertId() const
{
  return ullLastInsertId;
}

SIZE_T CBaseConnector::GetFieldsCount() const
{
  return aColumnsList.GetCount();
}

CField* CBaseConnector::GetField(_In_ SIZE_T nColumnIndex)
{
  CField *lpField = NULL;

  if (nColumnIndex < aColumnsList.GetCount())
  {
    lpField = aColumnsList.GetElementAt(nColumnIndex)->cField.Get();
    lpField->AddRef();
  }
  return lpField;
}

CField* CBaseConnector::GetFieldByName(_In_z_ LPCSTR szNameA)
{
  return GetField(GetColumnIndex(szNameA));
}

LPCSTR CBaseConnector::GetFieldName(_In_ SIZE_T nColumnIndex)
{
  CColumn *lpColumn;

  if (nColumnIndex >= aColumnsList.GetCount())
    return NULL;
  lpColumn = aColumnsList.GetElementAt(nColumnIndex);
  return (LPCSTR)(lpColumn->cStrNameA);
}

LPCSTR CBaseConnector::GetFieldTableName(_In_ SIZE_T nColumnIndex)
{
  CColumn *lpColumn;

  if (nColumnIndex >= aColumnsList.GetCount())
    return NULL;
  lpColumn = aColumnsList.GetElementAt(nColumnIndex);
  return (LPCSTR)(lpColumn->cStrTableA);
}

SIZE_T CBaseConnector::GetColumnIndex(_In_z_ LPCSTR szNameA)
{
  CColumn **lplpColumns;
  SIZE_T i, nCount;

  if (szNameA == NULL || *szNameA == 0)
    return (SIZE_T)-1;

  nCount = aColumnsList.GetCount();
  lplpColumns = aColumnsList.GetBuffer();
  for (i = 0; i < nCount; i++)
  {
    if (StrCompareA(szNameA, (LPCSTR)(lplpColumns[i]->cStrNameA), TRUE) == 0)
      return i;
  }

  //done
  return (SIZE_T)-1;
}

VOID CBaseConnector::QueryClose()
{
  aColumnsList.RemoveAllElements();
  ullAffectedRows = ullLastInsertId = 0ui64;
  return;
}

} //namespace Database

} //namespace MX
