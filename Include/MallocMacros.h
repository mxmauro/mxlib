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
#ifndef _MALLOCMACROS_H
#define _MALLOCMACROS_H

#define MX_TRACEALLOC

//MallocMacros usage:
//~~~~~~~~~~~~~~~~~~
//  Add the following macro to the C/C++/Project macro list: MX_MALLOCMACFROS_FILE=mymalloc.h
//  Where mymalloc.h is the name of the header file that will define MX_MALLOC macros.
//  Do NOT enclose the header file inside quotes because Visual Studio will complain about that.
//  The stringizing operator below will do the job.

//-----------------------------------------------------------

#ifdef MX_MALLOCMACFROS_FILE
  #define MX_MALLOCMACFROS_STRINGIFY(x) #x
  #define MX_MALLOCMACFROS_TOSTRING(x) MX_MALLOCMACFROS_STRINGIFY(x)
  #include MX_MALLOCMACFROS_TOSTRING( MX_MALLOCMACFROS_FILE )
#endif //MX_MALLOCMACFROS_FILE

#if (defined(MX_MALLOC) || defined(MX_FREE) || defined(MX_REALLOC) || defined(MX__MSIZE))
  #if ((!defined(MX_MALLOC)) || (!defined(MX_FREE)) || (!defined(MX_REALLOC)) || (!defined(MX__MSIZE)))
    #error You MUST define MX_MALLOC, MX_FREE, MX_REALLOC and MX__MSIZE
  #endif
#else //MX_MALLOC || MX_FREE || MX_REALLOC || MX__MSIZE

  #if defined(_DEBUG) || defined(MX_TRACEALLOC)
    #define MX_MALLOC(nSize)              MX::MemAllocD((SIZE_T)(nSize), __FILE__, __LINE__)
    #define MX_REALLOC(lpPtr, nNewSize)   MX::MemReallocD((LPVOID)(lpPtr), (SIZE_T)(nNewSize), __FILE__, __LINE__)
    #define MX_MALLOC_D(nSize, _f, _l)             MX::MemAllocD((SIZE_T)(nSize), _f, _l)
    #define MX_REALLOC_D(lpPtr, nNewSize, _f, _l)  MX::MemReallocD((LPVOID)(lpPtr), (SIZE_T)(nNewSize), _f, _l)
  #else //_DEBUG || MX_TRACEALLOC
    #define MX_MALLOC(nSize)              MX::MemAlloc((SIZE_T)(nSize))
    #define MX_REALLOC(lpPtr, nNewSize)   MX::MemRealloc((LPVOID)(lpPtr), (SIZE_T)(nNewSize))
  #endif //_DEBUG || MX_TRACEALLOC

  #define MX_FREE(lpPtr)                if (lpPtr != NULL) { MX::MemFree((LPVOID)(lpPtr));  lpPtr = NULL; }
  #define MX__MSIZE(lpPtr)              MX::MemSize((LPVOID)(lpPtr))
#endif //MX_MALLOC || MX_FREE || MX_REALLOC || MX__MSIZE

//-----------------------------------------------------------

#endif //_MALLOCMACROS_H
