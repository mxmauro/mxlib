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
#ifndef _MX_HTTPSERVER_COMMON_H
#define _MX_HTTPSERVER_COMMON_H

#include "..\..\Include\Http\HttpServer.h"
#include "..\..\Include\Strings\Utf8.h"
#include "..\..\Include\FnvHash.h"
#include "..\..\Include\DateTime\DateTime.h"
#include "..\..\Include\Comm\IpcSslLayer.h"
#include "..\..\Include\Http\punycode.h"
#include "..\..\Include\Http\Url.h"
#include "..\..\Include\MemoryStream.h"
#include "..\..\Include\FileStream.h"

//-----------------------------------------------------------

#define REQUEST_FLAG_DontKeepAlive                    0x0001
#define REQUEST_FLAG_CloseInDestructorMark            0x0002
#define REQUEST_FLAG_ErrorPageSent                    0x0004
#define REQUEST_FLAG_LinkClosed                       0x0008

//-----------------------------------------------------------

typedef struct {
  int nErrorCode;
  LPCSTR szMsgA;
} HTTPSERVER_ERROR_MSG;

//-----------------------------------------------------------

#endif //_MX_HTTPSERVER_COMMON_H
