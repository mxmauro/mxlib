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
#pragma once

#include <SDKDDKVer.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <WaitableObjects.h>

#include <Strings\Strings.h>

//-----------------------------------------------------------

extern DWORD dwLogLevel;

//-----------------------------------------------------------

BOOL ShouldAbort();

HRESULT GetAppPath(_Out_ MX::CStringW &cStrPathW);

BOOL DoesCmdLineParamExist(_In_z_ LPCWSTR szParamNameW);
HRESULT GetCmdLineParamString(_In_z_ LPCWSTR szParamNameW, _Out_ MX::CStringW &cStrParamValueW);
HRESULT GetCmdLineParamUInt(_In_z_ LPCWSTR szParamNameW, _Out_ LPDWORD lpdwValue);
