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
#ifndef __FNV_H__
#define __FNV_H__

//NOTE: This is a stripped down version. Refer to original
//      webpage for full version

//-----------------------------------------------------------

#include <windows.h>
#include <sys/types.h>

#define FNV_VERSION "5.0.2"

typedef ULONG     Fnv32_t;
typedef ULONGLONG Fnv64_t;

#define FNV1A_32_INIT ((Fnv32_t)0x811C9DC5)
#define FNV1A_64_INIT ((Fnv64_t)0xCBF29CE484222325ULL)

//-----------------------------------------------------------

Fnv32_t fnv_32a_buf(const void *buf, size_t len, Fnv32_t hashval);
Fnv64_t fnv_64a_buf(const void *buf, size_t len, Fnv64_t hashval);

//-----------------------------------------------------------

#endif //__FNV_H__
