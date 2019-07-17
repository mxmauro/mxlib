/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2019. All rights reserved.
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
#pragma warning(disable : 4101 4703 4244 4267 6262 6385 6011 6387)
#include "DukTape\Source\dist\duktape.c"
#define snprintf mx_sprintf_s
//#include "DukTape\Source\extras\module-duktape\duk_module_duktape.c"
#include "DukTape\Source\extras\module-node\duk_module_node.c"
#undef snprintf
#pragma warning(default : 4101 4703 4244 4267 6262 6385 6011 6387)
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
                          _In_ BOOL bInitialValueOnStack, _In_ int nFlags,
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
