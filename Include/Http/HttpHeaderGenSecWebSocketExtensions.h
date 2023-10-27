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
#ifndef _MX_HTTPHEADERGENSECWEBSOCKETEXTENSIONS_H
#define _MX_HTTPHEADERGENSECWEBSOCKETEXTENSIONS_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderGenSecWebSocketExtensions : public CHttpHeaderBase
{
public:
  class CExtension : public virtual CBaseMemObj
  {
  public:
    CExtension();
    ~CExtension();

    CExtension &operator=(_In_ const CExtension &cSrc) throw(...);

    HRESULT SetExtension(_In_z_ LPCSTR szExtensionA, _In_ SIZE_T nExtensionLen = (SIZE_T)-1);
    LPCSTR GetExtension() const;

    HRESULT AddParam(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW);

    SIZE_T GetParamsCount() const;
    LPCSTR GetParamName(_In_ SIZE_T nIndex) const;
    LPCWSTR GetParamValue(_In_ SIZE_T nIndex) const;
    LPCWSTR GetParamValue(_In_z_ LPCSTR szNameA) const;

  private:
    friend class CHttpHeaderGenSecWebSocketExtensions;

    typedef struct {
      LPWSTR szValueW;
      CHAR szNameA[1];
    } PARAMETER, *LPPARAMETER;

  private:
    CStringA cStrExtensionA;
    TArrayListWithFree<LPPARAMETER> aParamsList;
  };

  //----

public:
  CHttpHeaderGenSecWebSocketExtensions();
  ~CHttpHeaderGenSecWebSocketExtensions();

  MX_DECLARE_HTTPHEADER_NAME(Sec-WebSocket-Extensions)

  HRESULT Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser);

  eDuplicateBehavior GetDuplicateBehavior() const
    {
    return eDuplicateBehavior::Merge;
    };

  HRESULT AddExtension(_In_z_ LPCSTR szExtensionA, _In_opt_ SIZE_T nExtension = (SIZE_T)-1,
                       _Out_opt_ CExtension **lplpExtension = NULL);

  SIZE_T GetExtensionsCount() const;
  CExtension* GetExtension(_In_ SIZE_T nIndex) const;
  CExtension* GetExtension(_In_z_ LPCSTR szTypeA) const;

  HRESULT Merge(_In_ CHttpHeaderBase *lpHeader);

private:
  friend class CExtension;

  TArrayListWithDelete<CExtension*> cExtensionsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERGENSECWEBSOCKETEXTENSIONS_H
