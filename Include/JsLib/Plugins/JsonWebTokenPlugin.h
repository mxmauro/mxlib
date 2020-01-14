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
#ifndef _MX_JS_JSONWEBTOKEN_PLUGIN_H
#define _MX_JS_JSONWEBTOKEN_PLUGIN_H

#include "..\JavascriptVM.h"

//-----------------------------------------------------------

namespace MX {

class CJsonWebTokenPlugin : public CJsObjectBase, public CNonCopyableObj
{
public:
  CJsonWebTokenPlugin(_In_ DukTape::duk_context *lpCtx);
  ~CJsonWebTokenPlugin();

  MX_JS_DECLARE_CREATABLE(CJsonWebTokenPlugin, "JWT")

  MX_JS_BEGIN_MAP(CJsonWebTokenPlugin)
    MX_JS_MAP_STATIC_METHOD("create", &CJsonWebTokenPlugin::Create, 3) //object,secret,options
    MX_JS_MAP_STATIC_METHOD("verify", &CJsonWebTokenPlugin::Verify, 3) //string,secret,options
  MX_JS_END_MAP()

private:
  static DukTape::duk_ret_t Create(_In_ DukTape::duk_context *lpCtx);
  static DukTape::duk_ret_t Verify(_In_ DukTape::duk_context *lpCtx);
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JS_JSONWEBTOKEN_PLUGIN_H
