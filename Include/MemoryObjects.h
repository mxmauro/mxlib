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
#ifndef _MX_MEMORY_OBJECTS_H
#define _MX_MEMORY_OBJECTS_H

#include "MallocMacros.h"

//-----------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

void* MxMemAlloc(size_t nSize);
void* MxMemRealloc(void *lpPtr, size_t nSize);
void* MxMemAllocD(size_t nSize, const char *szFilenameA, int nLineNumber);
void* MxMemReallocD(void *lpPtr, size_t nSize, const char *szFilenameA, int nLineNumber);
void MxMemFree(void *lpPtr);
size_t MxMemSize(void *lpPtr);

#ifdef __cplusplus
} //extern "C"
#endif //__cplusplus

//-----------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

void MxMemSet(void *lpDest, int nVal, size_t nCount);
void MxMemCopy(void *lpDest, const void *lpSrc, size_t nCount);
void MxMemMove(void *lpDest, const void *lpSrc, size_t nCount);
int MxMemCompare(const void *lpSrc1, const void *lpSrc2, size_t nCount);
size_t __stdcall MxTryMemCopy(void *lpDest, const void *lpSrc, size_t nCount);

#ifdef __cplusplus
} //extern "C"
#endif //__cplusplus

//-----------------------------------------------------------

#ifdef __cplusplus

namespace MX {

#if defined(_DEBUG) || defined(MX_TRACEALLOC)

#define MX_DEBUG_NEW new(__FILE__, __LINE__)

#else //_DEBUG || MX_TRACEALLOC

#define MX_DEBUG_NEW new

#endif //_DEBUG || MX_TRACEALLOC

#define MX_DISABLE_COPY_CONSTRUCTOR(_class)                \
  private:                                                 \
    _class(_In_ const _class& cSrc) = delete;              \
    _class& operator=(_In_ const _class& cSrc) = delete

#define MX_NOVTABLE __declspec(novtable)

//-----------------------------------------------------------

class MX_NOVTABLE CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CBaseMemObj);
public:
  void* __cdecl operator new(_In_ size_t nSize)
    {
    return MX_MALLOC(nSize);
    };
  void* __cdecl operator new[](_In_ size_t nSize)
    {
    return MX_MALLOC(nSize);
    };
  void* __cdecl operator new(_In_ size_t nSize, _Inout_ void* lpInPlace)
    {
    return lpInPlace;
    };

#if defined(_DEBUG) || defined(MX_TRACEALLOC)
  void* __cdecl operator new(_In_ size_t nSize, _In_ const char *szFileNameA, int nLine)
    {
    return MX_MALLOC_D(nSize, szFileNameA, nLine);
    };
  void* __cdecl operator new[](_In_ size_t nSize, _In_ const char *szFileNameA, int nLine)
    {
    return MX_MALLOC_D(nSize, szFileNameA, nLine);
    };
#endif //_DEBUG || MX_TRACEALLOC

  void __cdecl operator delete(_Inout_ void* p)
    {
    MX_FREE(p);
    return;
    };
  void __cdecl operator delete[](_Inout_ void* p)
    {
    MX_FREE(p);
    return;
    };
  void __cdecl operator delete(_Inout_ void* p, _Inout_ void* lpPlace)
    {
    return;
    };

#if defined(_DEBUG) || defined(MX_TRACEALLOC)
  void __cdecl operator delete(_In_ void *p, _In_ const char *szFileNameA, int nLine)
    {
    MX_FREE(p);
    return;
    };
  void __cdecl operator delete[](_Inout_ void* p, _In_ const char *szFileNameA, int nLine)
    {
    MX_FREE(p);
    return;
    };
#endif //_DEBUG || MX_TRACEALLOC

protected:
  CBaseMemObj()
    { };
  virtual ~CBaseMemObj()
    { };
};

} //namespace MX

#endif //__cplusplus

//-----------------------------------------------------------

#endif //_MX_MEMORY_OBJECTS_H
