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
#ifndef _MX_PROPERTYBAG_H
#define _MX_PROPERTYBAG_H

#include "Defines.h"
#include "ArrayList.h"
#include "Strings\Strings.h"

//-----------------------------------------------------------

namespace MX {

class CPropertyBag : public virtual CBaseMemObj, public CNonCopyableObj
{
public:
  enum class eType {
    Undefined,
    Null,
    DWord,
    QWord,
    AnsiString,
    WideString,
    Double
  };

  CPropertyBag();
  ~CPropertyBag();

  VOID Reset();

  HRESULT Clear(_In_z_ LPCSTR szNameA);

  HRESULT SetNull(_In_z_ LPCSTR szNameA);
  HRESULT SetDWord(_In_z_ LPCSTR szNameA, _In_ DWORD dwValue);
  HRESULT SetQWord(_In_z_ LPCSTR szNameA, _In_ ULONGLONG ullValue);
  HRESULT SetDouble(_In_z_ LPCSTR szNameA, _In_ double nValue);
  HRESULT SetString(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA);
  HRESULT SetString(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW);

  LPCSTR GetAt(_In_ SIZE_T nIndex) const;

  eType GetType(_In_ SIZE_T nIndex) const;
  eType GetType(_In_z_ LPCSTR szNameA) const;

  HRESULT GetDWord(_In_ SIZE_T nIndex, _Out_ DWORD &dwValue, _In_opt_ DWORD dwDefValue=0);
  HRESULT GetQWord(_In_ SIZE_T nIndex, _Out_ ULONGLONG &ullValue, _In_opt_ ULONGLONG ullDefValue=0);
  HRESULT GetDouble(_In_ SIZE_T nIndex, _Out_ double &nValue, _In_opt_ double nDefValue=0.0);
  HRESULT GetString(_In_ SIZE_T nIndex, _Out_ LPCSTR &szValueA, _In_opt_z_ LPCSTR szDefValueA=NULL);
  HRESULT GetString(_In_ SIZE_T nIndex, _Out_ LPCWSTR &szValueW, _In_opt_z_ LPCWSTR szDefValueW=NULL);

  HRESULT GetDWord(_In_z_ LPCSTR szNameA, _Out_ DWORD &dwValue, _In_opt_ DWORD dwDefValue=0);
  HRESULT GetQWord(_In_z_ LPCSTR szNameA, _Out_ ULONGLONG &ullValue, _In_opt_ ULONGLONG ullDefValue=0);
  HRESULT GetDouble(_In_z_ LPCSTR szNameA, _Out_ double &nValue, _In_opt_ double nDefValue=0.0);
  HRESULT GetString(_In_z_ LPCSTR szNameA, _Out_ LPCSTR &szValueA, _In_opt_z_ LPCSTR szDefValueA=NULL);
  HRESULT GetString(_In_z_ LPCSTR szNameA, _Out_ LPCWSTR &szValueW, _In_opt_z_ LPCWSTR szDefValueW=NULL);

private:
  typedef struct {
    eType nType;
    LPSTR szNameA;
    union {
      DWORD dwValue;
      ULONGLONG ullValue;
      double dblValue;
      LPSTR szValueA;
      LPWSTR szValueW;
    } u;
  } PROPERTY;

private:
  BOOL Insert(_In_ PROPERTY *lpNewProp);
  SIZE_T Find(_In_z_ LPCSTR szNameA);

  static int InsertCompareFunc(_In_ LPVOID lpContext, _In_ PROPERTY **lpItem1, _In_ PROPERTY **lpItem2)
    {
    return StrCompareA((*lpItem1)->szNameA, (*lpItem2)->szNameA, TRUE);
    };

  static int SearchCompareFunc(_In_ LPVOID lpContext, _In_ LPCVOID lpKey, _In_ PROPERTY **lpItem)
    {
    return StrCompareA((LPCSTR)lpKey, (*lpItem)->szNameA, TRUE);
    };

private:
  TArrayListWithFree<PROPERTY*> cPropertiesList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_PROPERTYBAG_H
