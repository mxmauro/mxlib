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
#ifndef _MX_MSVCRT_H
#define _MX_MSVCRT_H

#include "..\..\Include\Defines.h"

//-----------------------------------------------------------

#if defined(_M_IX86)
  #define MX_LINKER_SYMBOL_PREFIX "_"
#elif defined(_M_X64) || defined(_M_ARM) || defined(_M_ARM64)
  #define MX_LINKER_SYMBOL_PREFIX ""
#else
  #error Unsupported platform
#endif

#define MX_LINKER_FORCE_INCLUDE(name)                       \
    __pragma(comment(linker, "/include:"                    \
                             MX_LINKER_SYMBOL_PREFIX #name))

typedef int (*_PIFV)(void);

//-----------------------------------------------------------

#endif //_MX_MSVCRT_H
