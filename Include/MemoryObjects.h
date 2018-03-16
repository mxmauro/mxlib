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

LPVOID MemAlloc(_In_ SIZE_T nSize);
LPVOID MemRealloc(_In_opt_ LPVOID lpPtr, _In_ SIZE_T nSize);
LPVOID MemAllocD(_In_ SIZE_T nSize, _In_opt_z_ const char *szFilenameA, _In_ int nLineNumber);
LPVOID MemReallocD(_In_opt_ LPVOID lpPtr, _In_ SIZE_T nSize, _In_opt_z_ const char *szFilenameA, _In_ int nLineNumber);
VOID MemFree(_In_opt_ LPVOID lpPtr);
SIZE_T MemSize(_In_opt_ LPVOID lpPtr);

VOID MemSet(_Out_writes_bytes_all_(nCount) LPVOID lpDest, _In_ int nVal, _In_ SIZE_T nCount);
VOID MemCopy(_Out_writes_bytes_all_(nCount) LPVOID lpDest, _In_ LPCVOID lpSrc, _In_ SIZE_T nCount);
VOID MemMove(_Out_writes_bytes_all_(nCount) LPVOID lpDest, _In_ LPCVOID lpSrc, _In_ SIZE_T nCount);
int MemCompare(_In_ LPCVOID lpSrc1, _In_ LPCVOID lpSrc2, _In_ SIZE_T nCount);

SIZE_T TryMemCopy(_Out_ LPVOID lpDest, _In_ LPCVOID lpSrc, _In_ SIZE_T nCount);

//-----------------------------------------------------------

#if defined(_DEBUG) || defined(MX_TRACEALLOC)

#define MX_DEBUG_NEW new(__FILE__, __LINE__)

#else //_DEBUG || MX_TRACEALLOC

#define MX_DEBUG_NEW new

#endif //_DEBUG || MX_TRACEALLOC

#define MX_DISABLE_COPY_CONSTRUCTOR(_class)                \
  private:                                                 \
    _class(_In_ const _class& cSrc) = delete;              \
    _class& operator=(_In_ const _class& cSrc) = delete

//-----------------------------------------------------------

class __declspec(novtable) CBaseMemObj
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

//-----------------------------------------------------------

#endif //_MX_MEMORY_OBJECTS_H
