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
#ifndef _MX_DEBUG_H
#define _MX_DEBUG_H

#include "Defines.h"

//-----------------------------------------------------------

namespace MX {

VOID DebugPrint(_In_z_ LPCSTR szFormatA, ...);
VOID DebugPrintV(_In_z_ LPCSTR szFormatA, va_list ap);

} //namespace MX

//-----------------------------------------------------------

#if (defined(_DEBUG) || defined(MX_RELEASE_DEBUG_MACROS))
  #define MX_DEBUGPRINT(expr)     MX::DebugPrint expr
  #define MX_DEBUGBREAK()         __debugbreak()
#else //_DEBUG || MX_RELEASE_DEBUG_MACROS
  #define MX_DEBUGPRINT(expr)     ((void)0)
  #define MX_DEBUGBREAK()         ((void)0)
#endif //_DEBUG || MX_RELEASE_DEBUG_MACROS

#endif //_MX_DEBUG_H
