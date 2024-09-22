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
#ifndef _MX_HTTPHEADERENTITYCONTENTDISPOSITION_H
#define _MX_HTTPHEADERENTITYCONTENTDISPOSITION_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"
#include "..\DateTime\DateTime.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderEntContentDisposition : public CHttpHeaderBase
{
public:
  CHttpHeaderEntContentDisposition();
  ~CHttpHeaderEntContentDisposition();

  MX_DECLARE_HTTPHEADER_NAME(Content-Disposition)

  HRESULT Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser);

  HRESULT SetType(_In_z_ LPCSTR szTypeA, _In_ SIZE_T nTypeLen = (SIZE_T)-1);
  LPCSTR GetType() const;

  HRESULT SetName(_In_opt_z_ LPCWSTR szNameW, _In_ SIZE_T nNameLen = (SIZE_T)-1);
  LPCWSTR GetName() const;

  HRESULT SetFileName(_In_opt_z_ LPCWSTR szFileNameW, _In_ SIZE_T nFileNameLen = (SIZE_T)-1);
  LPCWSTR GetFileName() const;
  BOOL HasFileName() const;

  HRESULT SetCreationDate(_In_ CDateTime &cDt);
  CDateTime GetCreationDate() const; //ticks=0 if no creation date parameter

  HRESULT SetModificationDate(_In_ CDateTime &cDt);
  CDateTime GetModificationDate() const; //ticks=0 if no modification date parameter

  HRESULT SetReadDate(_In_ CDateTime &cDt);
  CDateTime GetReadDate() const; //ticks=0 if no read date parameter

  HRESULT SetSize(_In_ ULONGLONG nSize);
  ULONGLONG GetSize() const; //ULONGLONG_MAX if size parameter

  HRESULT AddParam(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW);

  SIZE_T GetParamsCount() const;
  LPCSTR GetParamName(_In_ SIZE_T nIndex) const;
  LPCWSTR GetParamValue(_In_ SIZE_T nIndex) const;
  LPCWSTR GetParamValue(_In_z_ LPCSTR szNameA) const;

private:
  typedef struct {
    LPWSTR szValueW;
    CHAR szNameA[1];
  } PARAMETER, *LPPARAMETER;

  CStringA cStrTypeA;
  CStringW cStrNameW;
  CStringW cStrFileNameW;
  BOOL bHasFileName;
  CDateTime cCreationDt;
  CDateTime cModificationDt;
  CDateTime cReadDt;
  ULONGLONG nSize;
  TArrayListWithFree<LPPARAMETER> aParamsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERENTITYCONTENTDISPOSITION_H
