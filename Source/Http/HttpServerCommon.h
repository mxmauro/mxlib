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
#ifndef _MX_HTTPSERVER_COMMON_H
#define _MX_HTTPSERVER_COMMON_H

#include "..\..\Include\Http\HttpServer.h"
#include "..\..\Include\Strings\Utf8.h"
#include "..\..\Include\FnvHash.h"
#include "..\..\Include\DateTime\DateTime.h"
#include "..\..\Include\Http\punycode.h"
#include "..\..\Include\Http\Url.h"
#include "..\..\Include\MemoryStream.h"
#include "..\..\Include\FileStream.h"

//-----------------------------------------------------------

#define REQUEST_FLAG_DontKeepAlive                    0x0001
#define REQUEST_FLAG_ClosingOnShutdown                0x0002
#define REQUEST_FLAG_ErrorPageSent                    0x0004
#define REQUEST_FLAG_LinkClosed                       0x0008
#define REQUEST_FLAG_HeadersSent                      0x0010
#define REQUEST_FLAG_RequestTimeoutProcessed          0x0020

//-----------------------------------------------------------

typedef struct {
  int nErrorCode;
  LPCSTR szMsgA;
} HTTPSERVER_ERROR_MSG;

//-----------------------------------------------------------

#endif //_MX_HTTPSERVER_COMMON_H
