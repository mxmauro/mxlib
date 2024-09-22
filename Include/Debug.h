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
#ifndef _MX_DEBUG_H
#define _MX_DEBUG_H

#include "Defines.h"

//-----------------------------------------------------------

namespace MX {

VOID DebugPrint(_In_z_ LPCSTR szFormatA, ...);
VOID DebugPrintV(_In_z_ LPCSTR szFormatA, va_list ap);

} //namespace MX

//-----------------------------------------------------------

#if (defined(_DEBUG) || defined(MX_RELEASE_DEBUG_MACROS))
  #define MX_DEBUGPRINT(expr)     MX::DebugPrint expr
  #define MX_DEBUGBREAK()         __debugbreak()
#else //_DEBUG || MX_RELEASE_DEBUG_MACROS
  #define MX_DEBUGPRINT(expr)     ((void)0)
  #define MX_DEBUGBREAK()         ((void)0)
#endif //_DEBUG || MX_RELEASE_DEBUG_MACROS

#endif //_MX_DEBUG_H
