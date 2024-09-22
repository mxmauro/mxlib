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

#ifdef _DEBUG
  #define MX_MEMORY_OBJECTS_HEAP_CHECK
#else //_DEBUG
 //#define MX_MEMORY_OBJECTS_HEAP_CHECK
#endif //_DEBUG

//-----------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

void* MxMemAlloc(_In_ size_t nSize);
void* MxMemRealloc(_In_opt_ void *lpPtr, _In_ size_t nSize);
void* MxMemAllocD(_In_ size_t nSize, _In_opt_z_ const char *szFilenameA, _In_ int nLineNumber);
void* MxMemReallocD(_In_opt_ void *lpPtr, _In_ size_t nSize, _In_opt_z_ const char *szFilenameA, _In_ int nLineNumber);
void MxMemFree(_In_opt_ void *lpPtr);
size_t MxMemSize(_In_opt_ void *lpPtr);

#ifdef __cplusplus
} //extern "C"
#endif //__cplusplus

//-----------------------------------------------------------

typedef struct tagMX_MALLOC_OVERRIDE {
  void* (*alloc)(size_t nSize);
  void* (*realloc)(void *lpPtr, size_t nSize);
  void (*free)(void *lpPtr);
  size_t (*memsize)(void *lpPtr);
} MX_MALLOC_OVERRIDE, *LPMX_MALLOC_OVERRIDE;

#define MX_DEFINE_MALLOC_OVERRIDE(alloc, realloc, free, memsize)                        \
    static MX_MALLOC_OVERRIDE sMxAllocatorOverride = { alloc, realloc, free, memsize }; \
    extern "C" LPMX_MALLOC_OVERRIDE lpMxAllocatorOverride = &sMxAllocatorOverride;

//-----------------------------------------------------------

#if defined(_DEBUG)
#define MX_MALLOC(nSize)                       MxMemAllocD((size_t)(nSize), __FILE__, __LINE__)
#define MX_REALLOC(lpPtr, nNewSize)            MxMemReallocD((void*)(lpPtr), (size_t)(nNewSize), __FILE__, __LINE__)
#define MX_MALLOC_D(nSize, _f, _l)             MxMemAllocD((size_t)(nSize), _f, _l)
#define MX_REALLOC_D(lpPtr, nNewSize, _f, _l)  MxMemReallocD((void*)(lpPtr), (size_t)(nNewSize), _f, _l)
#else //_DEBUG
#define MX_MALLOC(nSize)              MxMemAlloc((size_t)(nSize))
#define MX_REALLOC(lpPtr, nNewSize)   MxMemRealloc((void*)(lpPtr), (size_t)(nNewSize))
#endif //_DEBUG

#define MX_FREE(lpPtr)                if (lpPtr != NULL) { MxMemFree((void*)(lpPtr));  lpPtr = NULL; }
#define MX__MSIZE(lpPtr)              MxMemSize((void*)(lpPtr))

//-----------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

void MxMemSet(_Out_writes_bytes_all_(nCount) void *lpDest, _In_ int nVal, _In_ size_t nCount);
void MxMemCopy(_Out_writes_bytes_all_(nCount) void *lpDest, _In_reads_bytes_(nCount) const void *lpSrc, _In_ size_t nCount);
void MxMemMove(_Out_writes_bytes_all_(nCount) void *lpDest, _In_ const void *lpSrc, _In_ size_t nCount);
int MxMemCompare(_In_ const void *lpSrc1, _In_ const void *lpSrc2, _In_ size_t nCount);
size_t __stdcall MxTryMemCopy(_Out_writes_bytes_all_(nCount) void *lpDest, const void *lpSrc, _In_ size_t nCount);

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

#define MX_NOVTABLE __declspec(novtable)

//-----------------------------------------------------------

class CNonCopyableObj
{
public:
  CNonCopyableObj()
    {
    return;
    };

  CNonCopyableObj(CNonCopyableObj const&) = delete;
  CNonCopyableObj& operator=(CNonCopyableObj const&) = delete;
};

//-----------------------------------------------------------

class MX_NOVTABLE CBaseMemObj
{
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
  void* __cdecl operator new(_In_ size_t nSize, _In_opt_z_ const char *szFileNameA, _In_ int nLine)
    {
    return MX_MALLOC_D(nSize, szFileNameA, nLine);
    };
  void* __cdecl operator new[](_In_ size_t nSize, _In_opt_z_ const char *szFileNameA, _In_ int nLine)
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
  void __cdecl operator delete(_In_ void *p, _In_z_ const char *szFileNameA, int nLine)
    {
    MX_FREE(p);
    return;
    };
  void __cdecl operator delete[](_Inout_ void* p, _In_z_ const char *szFileNameA, int nLine)
    {
    MX_FREE(p);
    return;
    };
#endif //_DEBUG || MX_TRACEALLOC

protected:
  CBaseMemObj()
    {
    return;
    };
  virtual ~CBaseMemObj()
    {
    return;
    };
};

} //namespace MX

#endif //__cplusplus

//-----------------------------------------------------------

#ifdef MX_MEMORY_OBJECTS_HEAP_CHECK

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

VOID MxDumpLeaks(_In_opt_z_ LPCSTR szFilenameA);

#ifdef __cplusplus
} //extern "C"
#endif //__cplusplus

#endif //MX_MEMORY_OBJECTS_HEAP_CHECK

//-----------------------------------------------------------

#endif //_MX_MEMORY_OBJECTS_H
