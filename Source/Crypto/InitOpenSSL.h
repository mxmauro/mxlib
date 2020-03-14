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
#ifndef _MX_INITOPENSSL_H
#define _MX_INITOPENSSL_H

#include "..\..\Include\Defines.h"
#define NO_SYS_TYPES_H
#include <OpenSSL\ssl.h>
#include <OpenSSL\err.h>

//-----------------------------------------------------------

namespace MX {

namespace Internals {

namespace OpenSSL {

HRESULT Init();

HRESULT GetLastErrorCode(_In_ HRESULT hResDefault);

SSL_CTX* GetSslContext(_In_ BOOL bServerSide);

} //namespace OpenSSL

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_INITOPENSSL_H
