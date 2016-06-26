/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2015. All rights reserved.
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
#include <exception>
/*
#include <xstddef>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <limits.h>
#include <math.h>
#include <xstddef>
*/
#ifdef _MX_JAVASCRIPT_VM_CPP

#define DUK_OPT_HAVE_CUSTOM_H
#define DUK_COMPILING_DUKTAPE
#include "..\..\Include\JsLib\JavascriptVM.h"

namespace DukTape {

//#include "..\..\Include\JsLib\DukTape\duktape.h"
#pragma warning(disable : 4101)
#include "DukTape\Source\dist\src\duktape.c"
#define snprintf mx_sprintf_s
#include "DukTape\Source\extras\module-duktape\duk_module_duktape.c"
#undef snprintf
#pragma warning(default : 4101)

} //namespace DukTape

#endif //_MX_JAVASCRIPT_VM_CPP

#include "..\..\Include\JsLib\JavascriptVM.h"

//-----------------------------------------------------------

namespace MX {

namespace Internals {

namespace JsLib {

HRESULT AddNativeFunctionCommon(__in DukTape::duk_context *lpCtx, __in_z_opt LPCSTR szObjectNameA,
                                __in DukTape::duk_idx_t nObjectIndex, __in_z LPCSTR szFunctionNameA,
                                __in MX::CJavascriptVM::OnNativeFunctionCallback cNativeFunctionCallback,
                                __in int nArgsCount);

HRESULT AddPropertyCommon(__in DukTape::duk_context *lpCtx, __in_z_opt LPCSTR szObjectNameA,
                          __in DukTape::duk_idx_t nObjectIndex, __in_z LPCSTR szPropertyNameA,
                          __in BOOL bInitialValueOnStack, __in int nFlags,
                          __in MX::CJavascriptVM::OnGetPropertyCallback cGetValueCallback,
                          __in MX::CJavascriptVM::OnSetPropertyCallback cSetValueCallback);

HRESULT RemovePropertyCommon(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                             __in_z LPCSTR szPropertyNameA);

HRESULT HasPropertyCommon(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA, __in_z LPCSTR szPropertyNameA);

HRESULT PushPropertyCommon(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA,
                           __in_z LPCSTR szPropertyNameA);

HRESULT FindObject(__in DukTape::duk_context *lpCtx, __in_z LPCSTR szObjectNameA, __in BOOL bCreateIfNotExists,
                   __in BOOL bResolveProxyOnLast);

#ifdef _DEBUG
VOID DebugDump(__in DukTape::duk_context *lpCtx, __in DukTape::duk_idx_t nIndex);
#endif //_DEBUG

} //namespace JsLib

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_JAVASCRIPTVM_COMMON_H
