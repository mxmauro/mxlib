/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2006-2015. All rights reserved.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 *
 * 2. This notice may not be removed or altered from any source distribution.
 *
 * 3. YOU MAY NOT:
 *
 *    a. Modify, translate, adapt, alter, or create derivative works from
 *       this software.
 *    b. Copy (other than one back-up copy), distribute, publicly display,
 *       transmit, sell, rent, lease or otherwise exploit this software.
 *    c. Distribute, sub-license, rent, lease, loan [or grant any third party
 *       access to or use of the software to any third party.
 **/
#ifndef _MX_JS_JSONWEBTOKEN_PLUGIN_H
#define _MX_JS_JSONWEBTOKEN_PLUGIN_H

#include "..\JavascriptVM.h"

//-----------------------------------------------------------

namespace MX {

class CJsonWebTokenPlugin : public CJsObjectBase
{
  MX_DISABLE_COPY_CONSTRUCTOR(CJsonWebTokenPlugin);
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
