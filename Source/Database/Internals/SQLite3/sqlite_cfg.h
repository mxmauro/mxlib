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
#ifndef _CUSTOM_SQLITE3_CONFIG
#define _CUSTOM_SQLITE3_CONFIG

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>

//-----------------------------------------------------------

#define SQLITE_THREADSAFE                 1
#define SQLITE_SYSTEM_MALLOC              1

//#define SQLITE_CORE                       0
//#define SQLITE_AMALGAMATION 0
//#define SQLITE_PRIVATE
#define SQLITE_API
#define SQLITE_ENABLE_COLUMN_METADATA     1
#define SQLITE_ENABLE_FTS3                1
#define SQLITE_ENABLE_FTS3_PARENTHESIS    1
#define SQLITE_ENABLE_FTS4                1
#define SQLITE_ENABLE_MEMORY_MANAGEMENT   1
#define SQLITE_ENABLE_RTREE               1
//#define SQLITE_ENABLE_TREE_EXPLAIN        1
#define SQLITE_ENABLE_UPDATE_DELETE_LIMIT 1
#define SQLITE_MAX_MMAP_SIZE              0

#ifdef _DEBUG
  #define SQLITE_DEBUG                    1
#endif //_DEBUG

#if defined(_M_IX86)
  #define SQLITE_4_BYTE_ALIGNED_MALLOC    1
#endif //_M_IX86

//-----------------------------------------------------------

#endif //!_CUSTOM_SQLITE3_CONFIG
