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
#ifndef _MALLOCMACROS_H
#define _MALLOCMACROS_H

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

  #if defined(_DEBUG)
    #define MX_MALLOC(nSize)                       MX::MemAllocD((SIZE_T)(nSize), __FILE__, __LINE__)
    #define MX_REALLOC(lpPtr, nNewSize)            MX::MemReallocD((LPVOID)(lpPtr), (SIZE_T)(nNewSize), __FILE__, \
                                                                    __LINE__)
    #define MX_MALLOC_D(nSize, _f, _l)             MX::MemAllocD((SIZE_T)(nSize), _f, _l)
    #define MX_REALLOC_D(lpPtr, nNewSize, _f, _l)  MX::MemReallocD((LPVOID)(lpPtr), (SIZE_T)(nNewSize), _f, _l)
  #else //_DEBUG
    #define MX_MALLOC(nSize)              MX::MemAlloc((SIZE_T)(nSize))
    #define MX_REALLOC(lpPtr, nNewSize)   MX::MemRealloc((LPVOID)(lpPtr), (SIZE_T)(nNewSize))
  #endif //_DEBUG

  #define MX_FREE(lpPtr)                if (lpPtr != NULL) { MX::MemFree((LPVOID)(lpPtr));  lpPtr = NULL; }
  #define MX__MSIZE(lpPtr)              MX::MemSize((LPVOID)(lpPtr))
#endif //MX_MALLOC || MX_FREE || MX_REALLOC || MX__MSIZE

//-----------------------------------------------------------

#endif //_MALLOCMACROS_H
