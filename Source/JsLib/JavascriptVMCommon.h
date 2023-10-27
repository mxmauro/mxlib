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
#ifndef _MX_JAVASCRIPTVM_COMMON_H
#define _MX_JAVASCRIPTVM_COMMON_H

#define _CRT_SECURE_NO_WARNINGS
#include "..\..\Include\Defines.h"
#include "..\..\Include\Strings\Strings.h"
#include "..\..\Include\Strings\Utf8.h"
#include "..\..\Include\Debug.h"
#include "..\..\Include\FnvHash.h"

//include the following files OUTSIDE the namespace
//#include <exception>

#ifdef _MX_JAVASCRIPT_VM_CPP

#define DUK_COMPILING_DUKTAPE
#include "..\..\Include\JsLib\JavascriptVM.h"

namespace DukTape {

#pragma check_stack(off)
#pragma warning(disable : 4101 4703 4244 4267 6262 6385 6011 6387 26450 28182)
#include "DukTape\Source\dist\duktape.c"
#define snprintf mx_sprintf_s
//#include "DukTape\Source\extras\module-duktape\duk_module_duktape.c"
#include "DukTape\Source\extras\module-node\duk_module_node.c"
#undef snprintf
#pragma warning(default : 4101 4703 4244 4267 6262 6385 6011 6387 26450 28182)
#pragma check_stack()

} //namespace DukTape

#endif //_MX_JAVASCRIPT_VM_CPP

#include "..\..\Include\JsLib\JavascriptVM.h"

//-----------------------------------------------------------

namespace MX {

namespace Internals {

namespace JsLib {

HRESULT AddNativeFunctionCommon(_In_ DukTape::duk_context *lpCtx, _In_opt_z_ LPCSTR szObjectNameA,
                                _In_ DukTape::duk_idx_t nObjectIndex, _In_z_ LPCSTR szFunctionNameA,
                                _In_ MX::CJavascriptVM::OnNativeFunctionCallback cNativeFunctionCallback,
                                _In_ int nArgsCount);

HRESULT AddPropertyCommon(_In_ DukTape::duk_context *lpCtx, _In_opt_z_ LPCSTR szObjectNameA,
                          _In_ DukTape::duk_idx_t nObjectIndex, _In_z_ LPCSTR szPropertyNameA,
                          _In_ BOOL bInitialValueOnStack, _In_ MX::CJavascriptVM::ePropertyFlags nFlags,
                          _In_ MX::CJavascriptVM::OnGetPropertyCallback cGetValueCallback,
                          _In_ MX::CJavascriptVM::OnSetPropertyCallback cSetValueCallback);

HRESULT RemovePropertyCommon(_In_ DukTape::duk_context *lpCtx, _In_opt_z_ LPCSTR szObjectNameA,
                             _In_z_ LPCSTR szPropertyNameA);

HRESULT HasPropertyCommon(_In_ DukTape::duk_context *lpCtx, _In_opt_z_ LPCSTR szObjectNameA,
                          _In_z_ LPCSTR szPropertyNameA);

VOID PushPropertyCommon(_In_ DukTape::duk_context *lpCtx, _In_opt_z_ LPCSTR szObjectNameA,
                        _In_z_ LPCSTR szPropertyNameA);

HRESULT FindObject(_In_ DukTape::duk_context *lpCtx, _In_opt_z_ LPCSTR szObjectNameA, _In_ BOOL bCreateIfNotExists,
                   _In_ BOOL bResolveProxyOnLast);

} //namespace JsLib

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JAVASCRIPTVM_COMMON_H
