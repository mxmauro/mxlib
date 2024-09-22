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
#ifndef _MX_TIMEDEVENT_H
#define _MX_TIMEDEVENT_H

#include "Defines.h"
#include "Callbacks.h"

//-----------------------------------------------------------

namespace MX {

namespace TimedEvent {

typedef Callback <VOID (_In_ LONG nTimerId, _In_ LPVOID lpUserData, _In_opt_ LPBOOL lpbCancel)> OnTimeoutCallback;

//-----------------------------------------------------------

HRESULT SetTimeout(_Inout_ _Interlocked_operand_ LONG volatile *lpnTimerId, _In_ DWORD dwTimeoutMs,
                   _In_ OnTimeoutCallback cCallback, _In_opt_ LPVOID lpUserData);
HRESULT SetInterval(_Inout_ _Interlocked_operand_ LONG volatile *lpnTimerId, _In_ DWORD dwTimeoutMs,
                    _In_ OnTimeoutCallback cCallback, _In_opt_ LPVOID lpUserData);
VOID Clear(_Inout_ _Interlocked_operand_ LONG volatile *lpnTimerId);

} //namespace TimedEvent

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_TIMEDEVENT_H
