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
#define _CRT_SECURE_NO_WARNINGS
#include "..\..\Include\Defines.h"

#define MZ_CUSTOM_ALLOC
#define MZ_ALLOC(SIZE)  MX_MALLOC(SIZE)

#define MZ_CUSTOM_FREE
#define MZ_FREE(PTR)    MX_FREE(PTR)

#ifndef SYMBOLIC_LINK_FLAG_DIRECTORY
  #define SYMBOLIC_LINK_FLAG_DIRECTORY            (0x1)
#endif //!SYMBOLIC_LINK_FLAG_DIRECTORY

#ifndef FSCTL_GET_REPARSE_POINT
  #define FSCTL_GET_REPARSE_POINT 0x900A8
#endif //!FSCTL_GET_REPARSE_POINT

#define HAVE_ZLIB
#define ZLIB_COMPAT

#include "MiniZipSource\mz_crypt.c"
#include "MiniZipSource\mz_crypt_win32.c"
#include "MiniZipSource\mz_os.c"
#include "MiniZipSource\mz_os_win32.c"
#include "MiniZipSource\mz_strm.c"
#include "MiniZipSource\mz_strm_buf.c"
#include "MiniZipSource\mz_strm_mem.c"
#include "MiniZipSource\mz_strm_os_win32.c"
#include "MiniZipSource\mz_strm_split.c"
#include "MiniZipSource\mz_strm_zlib.c"
#include "MiniZipSource\mz_zip.c"
#include "MiniZipSource\mz_zip_rw.c"
