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
#include "SystemDll.h"
#include "..\..\Include\Strings\Strings.h"

//-----------------------------------------------------------

namespace MX {

namespace Internals {

HRESULT LoadSystemDll(_In_z_ LPCWSTR szDllNameW, _Out_ HINSTANCE *lphInstance)
{
  WCHAR szFullDllNameW[4096];
  SIZE_T nDllNameLen;
  DWORD dwLen;
  HRESULT hRes;

  if (lphInstance != NULL)
    *lphInstance = NULL;
  if (szDllNameW == NULL)
    return E_POINTER;

  nDllNameLen = StrLenW(szDllNameW);
  dwLen = ::GetSystemDirectoryW(szFullDllNameW, MX_ARRAYLEN(szFullDllNameW) - (DWORD)nDllNameLen - 2);
  if (dwLen == 0)
  {
    hRes = MX_HRESULT_FROM_LASTERROR();
    return (FAILED(hRes)) ? hRes : E_FAIL;
  }
  if (szFullDllNameW[dwLen - 1] != L'\\')
    szFullDllNameW[dwLen++] = L'\\';
  ::MxMemCopy(szFullDllNameW + dwLen, szDllNameW, (nDllNameLen + 1) * sizeof(WCHAR));

  *lphInstance = ::LoadLibraryW(szFullDllNameW);
  if ((*lphInstance) == NULL)
  {
    hRes = MX_HRESULT_FROM_LASTERROR();
    return (FAILED(hRes)) ? hRes : E_FAIL;
  }

  //done
  return S_OK;
}

HRESULT LoadAppDll(_In_z_ LPCWSTR szDllNameW, _Out_ HINSTANCE *lphInstance)
{
  WCHAR szFullDllNameW[4096];
  SIZE_T nDllNameLen;
  DWORD dwLen;
  HRESULT hRes;

  if (lphInstance != NULL)
    *lphInstance = NULL;
  if (szDllNameW == NULL)
    return E_POINTER;

  nDllNameLen = StrLenW(szDllNameW);
  dwLen = ::GetModuleFileNameW(NULL, szFullDllNameW, MX_ARRAYLEN(szFullDllNameW) - (DWORD)nDllNameLen - 2);
  if (dwLen == 0)
  {
    hRes = MX_HRESULT_FROM_LASTERROR();
    return (FAILED(hRes)) ? hRes : E_FAIL;
  }
  if (szFullDllNameW[dwLen - 1] != L'\\')
    szFullDllNameW[dwLen++] = L'\\';
  ::MxMemCopy(szFullDllNameW + dwLen, szDllNameW, (nDllNameLen + 1) * sizeof(WCHAR));

  *lphInstance = ::LoadLibraryW(szFullDllNameW);
  if ((*lphInstance) == NULL)
  {
    hRes = MX_HRESULT_FROM_LASTERROR();
    return (FAILED(hRes)) ? hRes : E_FAIL;
  }

  //done
  return S_OK;
}

} //namespace Internals

} //namespace MX
