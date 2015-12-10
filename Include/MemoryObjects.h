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
#ifndef _MX_MEMORY_OBJECTS_H
#define _MX_MEMORY_OBJECTS_H

#include "MallocMacros.h"

//-----------------------------------------------------------

namespace MX {

//-----------------------------------------------------------

LPVOID MemAlloc(__in SIZE_T nSize);
LPVOID MemRealloc(__in LPVOID lpPtr, __in SIZE_T nSize);
LPVOID MemAllocD(__in SIZE_T nSize, __in_z_opt const char *szFilenameA, __in int nLineNumber);
LPVOID MemReallocD(__in LPVOID lpPtr, __in SIZE_T nSize, __in_z_opt const char *szFilenameA, __in int nLineNumber);
VOID MemFree(__in LPVOID lpPtr);
SIZE_T MemSize(__in LPVOID lpPtr);

VOID MemSet(__out LPVOID lpDest, __in int nVal, __in SIZE_T nCount);
VOID MemCopy(__out LPVOID lpDest, __in LPCVOID lpSrc, __in SIZE_T nCount);
VOID MemMove(__out LPVOID lpDest, __in LPCVOID lpSrc, __in SIZE_T nCount);
int MemCompare(__in LPCVOID lpSrc1, __in LPCVOID lpSrc2, __in SIZE_T nCount);

SIZE_T TryMemCopy(__out LPVOID lpDest, __in LPCVOID lpSrc, __in SIZE_T nCount);

//-----------------------------------------------------------

#if defined(_DEBUG) || defined(MX_TRACEALLOC)

#define MX_DEBUG_NEW new(__FILE__, __LINE__)

void* __cdecl operator new(__in size_t nSize, __in_z_opt const char *szFilenameA, __in int nLineNumber);
void* __cdecl operator new[](__in size_t nSize, __in_z_opt const char *szFilenameA, __in int nLineNumber);
void __cdecl operator delete(__in void* p, __in_z_opt const char *szFilenameA, __in int nLineNumber);
void __cdecl operator delete[](__in void* p, __in_z_opt const char *szFilenameA, __in int nLineNumber);

#else //_DEBUG || MX_TRACEALLOC

#define MX_DEBUG_NEW new

void* __cdecl operator new(__in size_t nSize);
void* __cdecl operator new[](__in size_t nSize);
void __cdecl operator delete(__in void* p);
void __cdecl operator delete[](__in void* p);

#endif //_DEBUG || MX_TRACEALLOC

#define MX_DISABLE_COPY_CONSTRUCTOR(_class)                \
  private:                                                 \
    _class(__in const _class& cSrc);                       \
    _class& operator=(__in const _class& cSrc)

//-----------------------------------------------------------

class __declspec(novtable) CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CBaseMemObj);
public:

  void* __cdecl operator new(__in size_t nSize)
    {
    return MX_MALLOC(nSize);
    };
  void* __cdecl operator new[](__in size_t nSize)
    {
    return MX_MALLOC(nSize);
    };
  void* __cdecl operator new(__in size_t nSize, __inout void* lpInPlace)
    {
    return lpInPlace;
    };

#if defined(_DEBUG) || defined(MX_TRACEALLOC)
  void* __cdecl operator new(__in size_t nSize, __in const char *szFileNameA, int nLine)
    {
    return MX_MALLOC_D(nSize, szFileNameA, nLine);
    };
  void* __cdecl operator new[](__in size_t nSize, __in const char *szFileNameA, int nLine)
    {
    return MX_MALLOC_D(nSize, szFileNameA, nLine);
    };
#endif //_DEBUG || MX_TRACEALLOC

  void __cdecl operator delete(__inout void* p)
    {
    MX_FREE(p);
    return;
    };
  void __cdecl operator delete[](__inout void* p)
    {
    MX_FREE(p);
    return;
    };
  void __cdecl operator delete(__inout void* p, __inout void* lpPlace)
    {
    return;
    };

#if defined(_DEBUG) || defined(MX_TRACEALLOC)
  void __cdecl operator delete(__in void *p, __in const char *szFileNameA, int nLine)
    {
    MX_FREE(p);
    return;
    };
  void __cdecl operator delete[](__inout void* p, __in const char *szFileNameA, int nLine)
    {
    MX_FREE(p);
    return;
    };
#endif //_DEBUG || MX_TRACEALLOC

protected:
  CBaseMemObj()
    { };
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_MEMORY_OBJECTS_H
