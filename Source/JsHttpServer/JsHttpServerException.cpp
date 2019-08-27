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
#include "JsHttpServerCommon.h"

//-----------------------------------------------------------

namespace MX {

CJsHttpServerSystemExit::CJsHttpServerSystemExit(_In_ DukTape::duk_context *lpCtx,
                                                 _In_ DukTape::duk_idx_t nStackIndex) : CJsError(lpCtx, nStackIndex)
{
  return;
}

CJsHttpServerSystemExit::CJsHttpServerSystemExit(_In_ const CJsHttpServerSystemExit &obj) : CJsError()
{
  *this = obj;
  return;
}

CJsHttpServerSystemExit& CJsHttpServerSystemExit::operator=(_In_ const CJsHttpServerSystemExit &obj)
{
  CJsError::operator=(obj);
  return *this;
}

} //namespace MX
