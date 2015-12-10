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
#ifndef _MX_DUKTAPECONFIG_H
#define _MX_DUKTAPECONFIG_H

#define DUK_F_C99
#define DUK_OPT_NO_FILE_IO
#ifdef _DEBUG
  //#define DUK_OPT_DEBUG
  //#define DUK_OPT_DPRINT
  //#define DUK_OPT_DDPRINT
  //#define DUK_OPT_DDDPRINT
#else //_DEBUG
  //#define DUK_OPT_DEBUG
  //#define DUK_OPT_DPRINT
  //#define DUK_OPT_DDPRINT
  //#define DUK_OPT_DDDPRINT
#endif
#define DUK_OPT_NO_BROWSER_LIKE
#define DUK_OPT_PANIC_HANDLER DukTape::atPanic

#define DUK_OPT_NO_MARK_AND_SWEEP
#define DUK_OPT_HAVE_CUSTOM_H

//-----------------------------------------------------------

#endif //_MX_DUKTAPECONFIG_H
