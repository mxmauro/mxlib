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
#ifndef _MX_LOGGABLE_H
#define _MX_LOGGABLE_H

#include "Callbacks.h"

//-----------------------------------------------------------

namespace MX {

class CLoggable : public virtual CBaseMemObj
{
public:
  typedef Callback <HRESULT (_In_z_ LPCWSTR szInfoW)> OnLogCallback;
public:
  CLoggable();
  CLoggable(_In_ OnLogCallback cCallback);

  CLoggable& operator=(_In_ const CLoggable &cSrc);

  VOID SetLogParent(_In_opt_ CLoggable *lpParentLog);
  VOID SetLogLevel(_In_ DWORD dwLevel);
  VOID SetLogCallback(_In_ OnLogCallback cCallback);

  __inline BOOL ShouldLog(_In_ DWORD dwRequiredLevel) const
    {
    return (GetRoot()->dwLevel >= dwRequiredLevel) ? TRUE : FALSE;
    };

  HRESULT Log(_Printf_format_string_ LPCWSTR szFormatW, ...);
  HRESULT LogIfError(_In_ HRESULT hResError, _Printf_format_string_ LPCWSTR szFormatW, ...);
  HRESULT LogAlways(_In_ HRESULT hResError, _Printf_format_string_ LPCWSTR szFormatW, ...);

private:
  CLoggable* GetRoot() const;

  HRESULT WriteLogCommon(_In_ BOOL bAddError, _In_ HRESULT hResError, _In_z_ LPCWSTR szFormatW, _In_ va_list argptr);

private:
  CLoggable *lpParentLog;
  DWORD dwLevel;
  OnLogCallback cCallback;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_LOGGABLE_H
