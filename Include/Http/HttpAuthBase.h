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
#ifndef _MX_HTTPAUTHBASE_H
#define _MX_HTTPAUTHBASE_H

#include "..\Defines.h"
#include "..\Strings\Strings.h"
#include "..\RefCounted.h"
#include "HttpHeaderRespWwwProxyAuthenticate.h"

//-----------------------------------------------------------

namespace MX {

class MX_NOVTABLE CHttpAuthBase : public TRefCounted<CBaseMemObj>
{
protected:
  CHttpAuthBase() : TRefCounted<CBaseMemObj>()
    {
    return;
    };

public:
  ~CHttpAuthBase()
    {
    return;
    };

  SIZE_T GetId()
    {
    return (SIZE_T)this;
    };

  virtual LPCSTR GetScheme() const = 0;

  virtual HRESULT Parse(_In_ CHttpHeaderRespWwwProxyAuthenticateCommon *lpHeader) = 0;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPAUTHBASE_H
