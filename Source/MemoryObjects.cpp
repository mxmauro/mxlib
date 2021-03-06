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
#include "..\Include\NtDefs.h"
#include "..\Include\MemoryObjects.h"
#include "..\Include\WaitableObjects.h"
#include "..\Include\Debug.h"
#include "Internals\MsVcrt.h"
#include <intrin.h>
#include "Internals\SystemDll.h"

#pragma intrinsic (_InterlockedIncrement)
#pragma intrinsic(_ReturnAddress)

#ifdef _DEBUG
  #define HEAP_CHECK_CAPTURE_STACK
#else //_DEBUG
  //#define HEAP_CHECK_CAPTURE_STACK
#endif //_DEBUG

#if defined(MX_MEMORY_OBJECTS_HEAP_CHECK) && defined(HEAP_CHECK_CAPTURE_STACK)
  #define HEAP_CHECK_STACK_ENTRIES  32
#endif //MX_MEMORY_OBJECTS_HEAP_CHECK && HEAP_CHECK_CAPTURE_STACK

//-----------------------------------------------------------

#if defined(MX_MEMORY_OBJECTS_HEAP_CHECK) && defined(HEAP_CHECK_CAPTURE_STACK)
#include <ImageHlp.h>
#include <VersionHelpers.h>

typedef USHORT (WINAPI *lpfnRtlCaptureStackBackTrace)(ULONG, ULONG, PVOID *, PULONG);
typedef BOOL (WINAPI *lpfnSymInitializeW)(_In_ HANDLE hProcess, _In_opt_z_ PCWSTR UserSearchPath,
                                          _In_ BOOL fInvadeProcess);
typedef BOOL (WINAPI *lpfnSymRefreshModuleList)(_In_ HANDLE hProcess);
typedef BOOL (WINAPI *lpfnSymFromAddrW)(_In_ HANDLE hProcess, _In_ DWORD64 Address, _Out_opt_ PDWORD64 Displacement,
                                        _Inout_ PSYMBOL_INFOW Symbol);
typedef LPAPI_VERSION (WINAPI *lpfnImagehlpApiVersion)(VOID);
#endif //MX_MEMORY_OBJECTS_HEAP_CHECK && HEAP_CHECK_CAPTURE_STACK

#ifdef MX_MEMORY_OBJECTS_HEAP_CHECK
  #include "..\Include\Strings\Strings.h"

  #if defined(_M_X64)
    #define MX_UNALIGNED __unaligned
  #else
    #define MX_UNALIGNED
  #endif

  #define CHECKTAG1a 0xDEAD0012
  #define CHECKTAG1b 0xDEAD0013
  #define CHECKTAG2a 0xDEADBEEF
  #define CHECKTAG2b 0xDEADBEF0

  #define CHECKTAG1a_FREE 0xCDCDCDCD
  #define CHECKTAG1b_FREE 0xDCDCDCDC
  #define CHECKTAG2a_FREE 0xCECECECE
  #define CHECKTAG2b_FREE 0xECECECEC

  #define CHECKTAG_COUNT 16

  #pragma pack(8)
  typedef struct {
    DWORD dwTag1[CHECKTAG_COUNT];
    LPCSTR szFilenameA;
    SIZE_T nLineNumber;
    DWORD dwTag2[CHECKTAG_COUNT];
  } MINIDEBUG_POSTBLOCK;

  typedef struct tagMINIDEBUG_PREBLOCK {
    DWORD dwTag1[CHECKTAG_COUNT];
    struct tagMINIDEBUG_PREBLOCK *lpNext, *lpPrev;
    LPCSTR szFilenameA;
    SIZE_T nLineNumber;
    LPVOID lpRetAddress;
    DWORD dwTag2[CHECKTAG_COUNT];
#ifdef HEAP_CHECK_CAPTURE_STACK
    SIZE_T nStackValues[HEAP_CHECK_STACK_ENTRIES];
#endif //HEAP_CHECK_CAPTURE_STACK
    MINIDEBUG_POSTBLOCK MX_UNALIGNED *lpPost;
  } MINIDEBUG_PREBLOCK;
  #pragma pack()

  #define EXTRA_ALLOC (sizeof(MINIDEBUG_PREBLOCK)+sizeof(MINIDEBUG_POSTBLOCK))

  #define CHECK_COUNTER_START 128
#endif //MX_MEMORY_OBJECTS_HEAP_CHECK

#define XISALIGNED(x)  ((((SIZE_T)(x)) & (sizeof(SIZE_T)-1)) == 0)

//-----------------------------------------------------------

#ifdef MX_MEMORY_OBJECTS_HEAP_CHECK
#ifdef HEAP_CHECK_CAPTURE_STACK
static int __cdecl InitializeMemory();
#endif //HEAP_CHECK_CAPTURE_STACK
static VOID InitializeBlock(_In_ MINIDEBUG_PREBLOCK *pPreBlk, _In_ SIZE_T nSize, _In_opt_z_ const char *szFilenameA,
                            _In_ int nLineNumber, _In_ LPVOID lpRetAddress);
static VOID SetBlockTags(_In_ MINIDEBUG_PREBLOCK *pPreBlk, _In_ BOOL bOnFree);
//static VOID CheckBlocks();
static VOID CheckBlock(_In_ MINIDEBUG_PREBLOCK *pPreBlk);
static VOID LinkBlock(_In_ MINIDEBUG_PREBLOCK *pPreBlk);
static VOID UnlinkBlock(_In_ MINIDEBUG_PREBLOCK *pPreBlk);
static VOID AssertNotTag(_In_ MINIDEBUG_PREBLOCK *pPreBlk);
static VOID DumpAllLeaks();

#ifdef HEAP_CHECK_CAPTURE_STACK
static HINSTANCE hDbgHelpDLL = NULL;
static lpfnRtlCaptureStackBackTrace fnRtlCaptureStackBackTrace = NULL;
static lpfnSymInitializeW fnSymInitializeW = NULL;
static lpfnSymRefreshModuleList fnSymRefreshModuleList = NULL;
static lpfnSymFromAddrW fnSymFromAddrW = NULL;
static lpfnImagehlpApiVersion fnImagehlpApiVersion = NULL;
#endif //HEAP_CHECK_CAPTURE_STACK

#endif //MX_MEMORY_OBJECTS_HEAP_CHECK

static void *DefaultMalloc(_In_ size_t nSize);
static void *DefaultRealloc(_In_opt_ void *lpPtr, _In_ size_t nSize);
static void DefaultFree(_In_opt_ void *lpPtr);
static size_t DefaultMemSize(_In_opt_ void *lpPtr);

//-----------------------------------------------------------

static MX_MALLOC_OVERRIDE MxDefaultAllocatorOverride = { DefaultMalloc, DefaultRealloc, DefaultFree, DefaultMemSize };
extern "C" LPMX_MALLOC_OVERRIDE lpMxDefaultAllocatorOverride = &MxDefaultAllocatorOverride;
extern "C" LPMX_MALLOC_OVERRIDE lpMxAllocatorOverride;

#if defined(_M_IX86)
  #pragma comment(linker, "/alternatename:_lpMxAllocatorOverride=_lpMxDefaultAllocatorOverride")
#elif defined (_M_X64)
  #pragma comment(linker, "/alternatename:lpMxAllocatorOverride=lpMxDefaultAllocatorOverride")
#else
  #error Unsupported platform
#endif

//-----------------------------------------------------------

#ifdef MX_MEMORY_OBJECTS_HEAP_CHECK
#ifdef HEAP_CHECK_CAPTURE_STACK
typedef int (*_PIFV)(void);
#endif //HEAP_CHECK_CAPTURE_STACK
typedef void (*_PVFV)(void);

#ifdef HEAP_CHECK_CAPTURE_STACK
MX_LINKER_FORCE_INCLUDE(___mx_memory_init);
#pragma section(".CRT$XIAZ", long, read)
extern "C" __declspec(allocate(".CRT$XIAZ")) const _PIFV ___mx_memory_init = &InitializeMemory;
#endif //HEAP_CHECK_CAPTURE_STACK

MX_LINKER_FORCE_INCLUDE(___mx_memory_finalize);
#pragma section(".CRT$XTY", long, read)  // NOLINT
extern "C" __declspec(allocate(".CRT$XTY")) const _PVFV ___mx_memory_finalize = &DumpAllLeaks;
#endif //MX_MEMORY_OBJECTS_HEAP_CHECK

//-----------------------------------------------------------

#ifdef MX_MEMORY_OBJECTS_HEAP_CHECK
static struct {
  LONG volatile nMutex;
  LONG volatile nCount;
  //LONG volatile nCheckCounter;
  MINIDEBUG_PREBLOCK *lpHead;
  MINIDEBUG_PREBLOCK *lpTail;
} sAllocatedBlocks = { 0, 0, /*CHECK_COUNTER_START, */NULL, NULL };
#endif //MX_MEMORY_OBJECTS_HEAP_CHECK

//-----------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

void* MxMemAlloc(_In_ size_t nSize)
{
#ifdef MX_MEMORY_OBJECTS_HEAP_CHECK
  LPBYTE p;
  MINIDEBUG_PREBLOCK *pPreBlk;
  LPVOID lpRetAddress;

  lpRetAddress = _ReturnAddress();
#endif //MX_MEMORY_OBJECTS_HEAP_CHECK

  if (nSize == 0)
    nSize = 1;
#ifdef MX_MEMORY_OBJECTS_HEAP_CHECK
  //CheckBlocks();
  p = (LPBYTE)(lpMxAllocatorOverride->alloc(nSize + EXTRA_ALLOC));
  if (p != NULL)
  {
    pPreBlk = (MINIDEBUG_PREBLOCK*)p;
    InitializeBlock(pPreBlk, nSize, NULL, 0, lpRetAddress);
    LinkBlock(pPreBlk);
    p += sizeof(MINIDEBUG_PREBLOCK);
  }
  return p;
#else //MX_MEMORY_OBJECTS_HEAP_CHECK
  return lpMxAllocatorOverride->alloc(nSize);
#endif //MX_MEMORY_OBJECTS_HEAP_CHECK
}

void* MxMemAllocD(_In_ size_t nSize, _In_opt_z_ const char *szFilenameA, _In_ int nLineNumber)
{
#ifdef MX_MEMORY_OBJECTS_HEAP_CHECK
  LPBYTE p;
  MINIDEBUG_PREBLOCK *pPreBlk;
  LPVOID lpRetAddress;

  lpRetAddress = _ReturnAddress();
#endif //MX_MEMORY_OBJECTS_HEAP_CHECK

  if (nSize == 0)
    nSize = 1;
#ifdef MX_MEMORY_OBJECTS_HEAP_CHECK
  //CheckBlocks();
  p = (LPBYTE)(lpMxAllocatorOverride->alloc(nSize + EXTRA_ALLOC));
  if (p != NULL)
  {
    pPreBlk = (MINIDEBUG_PREBLOCK*)p;
    InitializeBlock(pPreBlk, nSize, szFilenameA, nLineNumber, lpRetAddress);
    LinkBlock(pPreBlk);
    p += sizeof(MINIDEBUG_PREBLOCK);
  }
  return p;
#else //MX_MEMORY_OBJECTS_HEAP_CHECK
  return lpMxAllocatorOverride->alloc(nSize);
#endif //MX_MEMORY_OBJECTS_HEAP_CHECK
}

void* MxMemRealloc(_In_opt_ void *lpPtr, _In_ size_t nSize)
{
#ifdef MX_MEMORY_OBJECTS_HEAP_CHECK
  MINIDEBUG_PREBLOCK *pPreBlk;
  LPBYTE p;
  LPVOID lpRetAddress;
#endif //MX_MEMORY_OBJECTS_HEAP_CHECK

  if (nSize == 0)
  {
    ::MxMemFree(lpPtr);
    return NULL;
  }
  if (lpPtr == NULL)
  {
    return ::MxMemAlloc(nSize);
  }

#ifdef MX_MEMORY_OBJECTS_HEAP_CHECK
  lpRetAddress = _ReturnAddress();

  //CheckBlocks();
  pPreBlk = (MINIDEBUG_PREBLOCK*)((LPBYTE)lpPtr - sizeof(MINIDEBUG_PREBLOCK));
  CheckBlock(pPreBlk);
  UnlinkBlock(pPreBlk);
  SetBlockTags(pPreBlk, TRUE);
  p = (LPBYTE)(lpMxAllocatorOverride->realloc(pPreBlk, nSize + EXTRA_ALLOC));
  if (p != NULL)
  {
    pPreBlk = (MINIDEBUG_PREBLOCK*)p;
    InitializeBlock(pPreBlk, nSize, NULL, 0, lpRetAddress);
    p += sizeof(MINIDEBUG_PREBLOCK);
  }
  else
  {
    SetBlockTags(pPreBlk, FALSE);
  }
  LinkBlock(pPreBlk);
  return p;
#else //MX_MEMORY_OBJECTS_HEAP_CHECK
  return lpMxAllocatorOverride->realloc(lpPtr, nSize);
#endif //MX_MEMORY_OBJECTS_HEAP_CHECK
}

void* MxMemReallocD(_In_opt_ void *lpPtr, _In_ size_t nSize, _In_opt_z_ const char *szFilenameA, _In_ int nLineNumber)
{
#ifdef MX_MEMORY_OBJECTS_HEAP_CHECK
  MINIDEBUG_PREBLOCK *pPreBlk;
  LPBYTE p;
  LPVOID lpRetAddress;
#endif //MX_MEMORY_OBJECTS_HEAP_CHECK

  if (nSize == 0)
  {
    ::MxMemFree(lpPtr);
    return NULL;
  }
  if (lpPtr == NULL)
    return ::MxMemAllocD(nSize, szFilenameA, nLineNumber);

#ifdef MX_MEMORY_OBJECTS_HEAP_CHECK
  lpRetAddress = _ReturnAddress();

  //CheckBlocks();
  pPreBlk = (MINIDEBUG_PREBLOCK*)((LPBYTE)lpPtr - sizeof(MINIDEBUG_PREBLOCK));
  CheckBlock(pPreBlk);
  UnlinkBlock(pPreBlk);
  SetBlockTags(pPreBlk, TRUE);
  p = (LPBYTE)(lpMxAllocatorOverride->realloc(pPreBlk, nSize + EXTRA_ALLOC));
  if (p != NULL)
  {
    pPreBlk = (MINIDEBUG_PREBLOCK*)p;
    InitializeBlock(pPreBlk, nSize, szFilenameA, nLineNumber, lpRetAddress);
    p += sizeof(MINIDEBUG_PREBLOCK);
  }
  else
  {
    SetBlockTags(pPreBlk, FALSE);
  }
  LinkBlock(pPreBlk);
  return p;
#else //MX_MEMORY_OBJECTS_HEAP_CHECK
  return lpMxAllocatorOverride->realloc(lpPtr, nSize);
#endif //MX_MEMORY_OBJECTS_HEAP_CHECK
}

void MxMemFree(_In_opt_ void *lpPtr)
{
  if (lpPtr != NULL)
  {
#ifdef MX_MEMORY_OBJECTS_HEAP_CHECK
    MINIDEBUG_PREBLOCK *pPreBlk;

    //CheckBlocks();
    pPreBlk = (MINIDEBUG_PREBLOCK*)((LPBYTE)lpPtr - sizeof(MINIDEBUG_PREBLOCK));
    CheckBlock(pPreBlk);
    UnlinkBlock(pPreBlk);
    SetBlockTags(pPreBlk, TRUE);
    lpPtr = pPreBlk;
#endif //MX_MEMORY_OBJECTS_HEAP_CHECK

    lpMxAllocatorOverride->free(lpPtr);
  }
  return;
}

size_t MxMemSize(_In_opt_ void *lpPtr)
{
  if (lpPtr != NULL)
  {
#ifdef MX_MEMORY_OBJECTS_HEAP_CHECK
    MINIDEBUG_PREBLOCK *pPreBlk;
    SIZE_T nSize;

    //CheckBlocks();
    pPreBlk = (MINIDEBUG_PREBLOCK*)((LPBYTE)lpPtr - sizeof(MINIDEBUG_PREBLOCK));
    CheckBlock(pPreBlk);

    nSize = lpMxAllocatorOverride->memsize(pPreBlk);
    return (nSize > EXTRA_ALLOC) ? (nSize - EXTRA_ALLOC) : 0;
#else //MX_MEMORY_OBJECTS_HEAP_CHECK
    return lpMxAllocatorOverride->memsize(lpPtr);
#endif //MX_MEMORY_OBJECTS_HEAP_CHECK
  }
  return 0;
}

#ifdef __cplusplus
} //extern "C"
#endif //__cplusplus

//-----------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

void MxMemSet(_Out_writes_bytes_all_(nCount) void *lpDest, _In_ int nVal, _In_ size_t nCount)
{
  SIZE_T n;

  nVal &= 0xFF;
  if (XISALIGNED(lpDest))
  {
    n = ((SIZE_T)nVal) | (((SIZE_T)nVal) << 8);
    n = n | (n << 16);
#if defined(_M_X64) || defined(_M_IA64) || defined(_M_AMD64)
    n = n | (n << 32);
#endif //_M_X64 || _M_IA64 || _M_AMD64
    while (nCount >= sizeof(SIZE_T))
    {
      *((SIZE_T*)lpDest) = n;
      lpDest = (LPBYTE)lpDest + sizeof(SIZE_T);
      nCount -= sizeof(SIZE_T);
    }
  }
  //the following code is not fully optimized but avoid VC compiler to insert undesired "_memset" calls
  if (nCount > 0)
  {
    do
    {
      *((LPBYTE)lpDest) = (BYTE)nVal;
      lpDest = (LPBYTE)lpDest + 1;
    }
    while (--nCount > 0);
  }
  return;
}

void MxMemCopy(_Out_writes_bytes_all_(nCount) void *lpDest, _In_ const void *lpSrc, _In_ size_t nCount)
{
  if (XISALIGNED(lpSrc) && XISALIGNED(lpDest))
  {
    while (nCount >= sizeof(SIZE_T))
    {
      *((SIZE_T*)lpDest) = *((SIZE_T*)lpSrc);
      lpSrc = (LPBYTE)lpSrc + sizeof(SIZE_T);
      lpDest = (LPBYTE)lpDest + sizeof(SIZE_T);
      nCount -= sizeof(SIZE_T);
    }
  }
  while (nCount > 0)
  {
    *((LPBYTE)lpDest) = *((LPBYTE)lpSrc);
    lpSrc = (LPBYTE)lpSrc + 1;
    lpDest = (LPBYTE)lpDest + 1;
    nCount--;
  }
  return;
}

void MxMemMove(_Out_writes_bytes_all_(nCount) void *lpDest, _In_ const void *lpSrc, _In_ size_t nCount)
{
  LPBYTE s, d;

  s = (LPBYTE)lpSrc;
  d = (LPBYTE)lpDest;
  if (d <= s || d >= (s+nCount))
  {
    //dest is before source or non-overlapping buffers
    //copy from lower to higher addresses
    if (d+sizeof(SIZE_T) <= s && XISALIGNED(s) && XISALIGNED(d))
    {
      while (nCount >= sizeof(SIZE_T))
      {
        *((SIZE_T*)d) = *((SIZE_T*)s);
        s += sizeof(SIZE_T);
        d += sizeof(SIZE_T);
        nCount -= sizeof(SIZE_T);
      }
    }
    while ((nCount--) > 0)
      *d++ = *s++;
  }
  else
  {
    //dest is past source or overlapping buffers
    //copy from higher to lower addresses
    if (nCount >= sizeof(SIZE_T) && XISALIGNED(s) && XISALIGNED(d))
    {
      s += nCount;
      d += nCount;
      while (nCount>0 && (!XISALIGNED(nCount))) {
        --s;
        --d;
        *d = *s;
        nCount--;
      }
      while (nCount > 0)
      {
        s -= sizeof(SIZE_T);
        d -= sizeof(SIZE_T);
        *((SIZE_T*)d) = *((SIZE_T*)s);
        nCount -= sizeof(SIZE_T);
      }
    }
    else
    {
      s += nCount;
      d += nCount;
      while (nCount > 0)
      {
        --s;
        --d;
        *d = *s;
        nCount--;
      }
    }
  }
  return;
}

int MxMemCompare(_In_ const void *lpSrc1, _In_ const void *lpSrc2, _In_ size_t nCount)
{
  if (XISALIGNED(lpSrc1) && XISALIGNED(lpSrc2))
  {
    while (nCount >= sizeof(SIZE_T))
    {
      if (*((SIZE_T*)lpSrc1) != *((SIZE_T*)lpSrc2))
        break;
      lpSrc1 = (LPBYTE)lpSrc1 + sizeof(SIZE_T);
      lpSrc2 = (LPBYTE)lpSrc2 + sizeof(SIZE_T);
      nCount -= sizeof(SIZE_T);
    }
  }
  while (nCount > 0)
  {
    if (*((LPBYTE)lpSrc1) != *((LPBYTE)lpSrc2))
      return (*((LPBYTE)lpSrc1) - *((LPBYTE)lpSrc2));
    lpSrc1 = (LPBYTE)lpSrc1 + 1;
    lpSrc2 = (LPBYTE)lpSrc2 + 1;
    nCount--;
  }
  return 0;
}

#ifdef __cplusplus
} //extern "C"
#endif //__cplusplus

//-----------------------------------------------------------

#ifdef MX_MEMORY_OBJECTS_HEAP_CHECK

#ifdef HEAP_CHECK_CAPTURE_STACK
static int __cdecl InitializeMemory()
{
  HINSTANCE hNtDll;

  hNtDll = ::GetModuleHandleW(L"ntdll.dll");
  if (hNtDll != NULL)
  {
    if (::IsWindowsXPOrGreater() != FALSE)
    {
      //use RtlCaptureStackBackTrace only on xp or later
      fnRtlCaptureStackBackTrace = (lpfnRtlCaptureStackBackTrace)::GetProcAddress(hNtDll,
                                                                                  "RtlCaptureStackBackTrace");
    }
  }
  //IMPORTANT: Always check these functions does not use memory
  if (FAILED(MX::Internals::LoadAppDll(L"dbghelp.dll", &hDbgHelpDLL)))
  {
    MX::Internals::LoadSystemDll(L"dbghelp.dll", &hDbgHelpDLL);
  }
  if (hDbgHelpDLL != NULL)
  {
    BOOL b = FALSE;

    fnSymInitializeW = (lpfnSymInitializeW)::GetProcAddress(hDbgHelpDLL, "SymInitializeW");
    fnSymRefreshModuleList = (lpfnSymRefreshModuleList)::GetProcAddress(hDbgHelpDLL, "SymRefreshModuleList");
    fnSymFromAddrW = (lpfnSymFromAddrW)::GetProcAddress(hDbgHelpDLL, "SymFromAddrW");
    fnImagehlpApiVersion = (lpfnImagehlpApiVersion)::GetProcAddress(hDbgHelpDLL, "ImagehlpApiVersion");
    if (fnSymInitializeW != NULL && fnSymRefreshModuleList != NULL && fnSymFromAddrW != NULL &&
        fnImagehlpApiVersion != NULL)
    {
      try
      {
        LPAPI_VERSION lpApiVer = fnImagehlpApiVersion();

        if (lpApiVer != NULL &&
            (lpApiVer->MajorVersion > 4 ||
             (lpApiVer->MajorVersion == 4 && lpApiVer->MinorVersion > 0) ||
             (lpApiVer->MajorVersion == 4 && lpApiVer->MinorVersion == 0 && lpApiVer->Revision >= 5)))
        {
          b = fnSymInitializeW(::GetCurrentProcess(), NULL, TRUE);
        }
      }
      catch (...)
      {
      }
    }
    if (b == FALSE)
    {
      fnSymInitializeW = NULL;
      fnSymRefreshModuleList = NULL;
      fnSymFromAddrW = NULL;
      fnImagehlpApiVersion = NULL;

      ::FreeLibrary(hDbgHelpDLL);
      hDbgHelpDLL = NULL;
    }
  }

  //done
  return 0;
}
#endif //HEAP_CHECK_CAPTURE_STACK

static VOID InitializeBlock(_In_ MINIDEBUG_PREBLOCK *pPreBlk, _In_ SIZE_T nSize, _In_opt_z_ const char *szFilenameA,
                            _In_ int nLineNumber, _In_ LPVOID lpRetAddress)
{
  pPreBlk->lpNext = pPreBlk->lpPrev = NULL;
  pPreBlk->szFilenameA = (LPCSTR)szFilenameA;
  pPreBlk->nLineNumber = (SIZE_T)nLineNumber;
  pPreBlk->lpRetAddress = lpRetAddress;
#ifdef HEAP_CHECK_CAPTURE_STACK
  ::MxMemSet(pPreBlk->nStackValues, 0, sizeof(pPreBlk->nStackValues));
  if (fnRtlCaptureStackBackTrace != NULL)
  {
    ULONG nHash;

    fnRtlCaptureStackBackTrace(2, HEAP_CHECK_STACK_ENTRIES, (LPVOID*)(pPreBlk->nStackValues), &nHash);
  }
#endif //HEAP_CHECK_CAPTURE_STACK
  pPreBlk->lpPost = (MINIDEBUG_POSTBLOCK*)((LPBYTE)pPreBlk + sizeof(MINIDEBUG_PREBLOCK) + nSize);
  SetBlockTags(pPreBlk, FALSE);
  pPreBlk->lpPost->szFilenameA = (LPCSTR)szFilenameA;
  pPreBlk->lpPost->nLineNumber = (SIZE_T)nLineNumber;
  return;
}

static VOID SetBlockTags(_In_ MINIDEBUG_PREBLOCK *pPreBlk, _In_ BOOL bOnFree)
{
  SIZE_T i;

  if (bOnFree == FALSE)
  {
    for (i=0; i<CHECKTAG_COUNT; i++)
    {
      pPreBlk->dwTag1[i] = CHECKTAG1a;
      pPreBlk->dwTag2[i] = CHECKTAG2a;
      pPreBlk->lpPost->dwTag1[i] = CHECKTAG1b;
      pPreBlk->lpPost->dwTag2[i] = CHECKTAG2b;
    }
  }
  else
  {
    for (i=0; i<CHECKTAG_COUNT; i++)
    {
      pPreBlk->dwTag1[i] = CHECKTAG1a_FREE;
      pPreBlk->dwTag2[i] = CHECKTAG2a_FREE;
      pPreBlk->lpPost->dwTag1[i] = CHECKTAG1b_FREE;
      pPreBlk->lpPost->dwTag2[i] = CHECKTAG2b_FREE;
    }
  }
  return;
}

//static VOID CheckBlocks()
//{
//  if (_InterlockedDecrement(&(sAllocatedBlocks.nCheckCounter)) == 0)
//  {
//    MX::CFastLock cLock(&(sAllocatedBlocks.nMutex));
//    MINIDEBUG_PREBLOCK *lpBlock;
//
//    _InterlockedExchange(&(sAllocatedBlocks.nCheckCounter), CHECK_COUNTER_START);
//    for (lpBlock=sAllocatedBlocks.lpHead; lpBlock!=NULL; lpBlock=lpBlock->lpNext)
//      CheckBlock(lpBlock);
//  }
//  return;
//}

static VOID CheckBlock(_In_ MINIDEBUG_PREBLOCK *pPreBlk)
{
  SIZE_T i;

  for (i=0; i<CHECKTAG_COUNT; i++)
  {
    MX_ASSERT_ALWAYS(pPreBlk->dwTag1[i] == CHECKTAG1a);
    MX_ASSERT_ALWAYS(pPreBlk->dwTag2[i] == CHECKTAG2a);
    MX_ASSERT_ALWAYS(pPreBlk->lpPost->dwTag1[i] == CHECKTAG1b);
    MX_ASSERT_ALWAYS(pPreBlk->lpPost->dwTag2[i] == CHECKTAG2b);
  }
  return;
}

static VOID LinkBlock(_In_ MINIDEBUG_PREBLOCK *pPreBlk)
{
  MX::CFastLock cLock(&(sAllocatedBlocks.nMutex));

  MX_ASSERT_ALWAYS(pPreBlk->lpNext == NULL);
  MX_ASSERT_ALWAYS(pPreBlk->lpPrev == NULL);
  if (sAllocatedBlocks.lpTail != NULL)
    sAllocatedBlocks.lpTail->lpNext = pPreBlk;
  pPreBlk->lpPrev = sAllocatedBlocks.lpTail;
  sAllocatedBlocks.lpTail = pPreBlk;
  if (sAllocatedBlocks.lpHead == NULL)
    sAllocatedBlocks.lpHead = pPreBlk;
  //----
  (sAllocatedBlocks.nCount)++;
  return;
}

static VOID UnlinkBlock(_In_ MINIDEBUG_PREBLOCK *pPreBlk)
{
  MX::CFastLock cLock(&(sAllocatedBlocks.nMutex));

  AssertNotTag(pPreBlk->lpNext);
  AssertNotTag(pPreBlk->lpPrev);
  if (sAllocatedBlocks.lpHead == pPreBlk)
    sAllocatedBlocks.lpHead = pPreBlk->lpNext;
  if (sAllocatedBlocks.lpTail == pPreBlk)
    sAllocatedBlocks.lpTail = pPreBlk->lpPrev;
  if (pPreBlk->lpNext != NULL)
  {
    AssertNotTag(pPreBlk->lpNext->lpPrev);
    pPreBlk->lpNext->lpPrev = pPreBlk->lpPrev;
  }
  if (pPreBlk->lpPrev != NULL)
  {
    AssertNotTag(pPreBlk->lpPrev->lpNext);
    pPreBlk->lpPrev->lpNext = pPreBlk->lpNext;
  }
  pPreBlk->lpNext = pPreBlk->lpPrev = NULL;
  //----
  (sAllocatedBlocks.nCount)--;
  return;
}

static VOID AssertNotTag(_In_ MINIDEBUG_PREBLOCK *pPreBlk)
{
  union {
    SIZE_T val;
    DWORD dw[2];
  };

  val = (SIZE_T)pPreBlk;
#if defined(_M_IX86)
  MX_ASSERT_ALWAYS(val != CHECKTAG1a && val != CHECKTAG1b && val != CHECKTAG2a && val != CHECKTAG2b);
  MX_ASSERT_ALWAYS(val != CHECKTAG1a_FREE && val != CHECKTAG1b_FREE &&
                   val != CHECKTAG2a_FREE && val != CHECKTAG2b_FREE);
#elif defined(_M_X64)
  MX_ASSERT_ALWAYS(dw[0] != CHECKTAG1a && dw[0] != CHECKTAG1b && dw[0] != CHECKTAG2a && dw[0] != CHECKTAG2b);
  MX_ASSERT_ALWAYS(dw[0] != CHECKTAG1a_FREE && dw[0] != CHECKTAG1b_FREE &&
                   dw[0] != CHECKTAG2a_FREE && dw[0] != CHECKTAG2b_FREE);
  MX_ASSERT_ALWAYS(dw[1] != CHECKTAG1a && dw[1] != CHECKTAG1b && dw[1] != CHECKTAG2a && dw[1] != CHECKTAG2b);
  MX_ASSERT_ALWAYS(dw[1] != CHECKTAG1a_FREE && dw[1] != CHECKTAG1b_FREE &&
                   dw[1] != CHECKTAG2a_FREE && dw[1] != CHECKTAG2b_FREE);
#endif
  return;
}

static VOID DumpAllLeaks()
{
  MxDumpLeaks(NULL);
  return;
}

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

VOID MxDumpLeaks(_In_opt_z_ LPCSTR szFilenameA)
{
  MX::CFastLock cLock(&(sAllocatedBlocks.nMutex));
  CHAR szBufferA[512];
  MINIDEBUG_PREBLOCK *lpBlock;
#ifdef HEAP_CHECK_CAPTURE_STACK
  SIZE_T nIdx;
  struct {
    SYMBOL_INFOW sSymbolInfoW;
    WCHAR szNameContinuationW[256];
  } sFullSymbolInfoW;
  DWORD64 qwDisp;
#endif //HEAP_CHECK_CAPTURE_STACK

#ifdef HEAP_CHECK_CAPTURE_STACK
  if (sAllocatedBlocks.lpHead != NULL && fnSymRefreshModuleList != NULL)
  {
    fnSymRefreshModuleList(::GetCurrentProcess());
  }
#endif //HEAP_CHECK_CAPTURE_STACK

  for (lpBlock = sAllocatedBlocks.lpHead; lpBlock != NULL; lpBlock = lpBlock->lpNext)
  {
    LPBYTE p = (LPBYTE)(lpBlock+1);

    if (lpBlock->szFilenameA != NULL && lpBlock->szFilenameA[0] != 0)
    {
      if (szFilenameA != NULL)
      {
        if (MX::StrCompareA(szFilenameA, lpBlock->szFilenameA, TRUE) != 0)
          continue;
      }

      MX::DebugPrint("MXLIB: Leak 0x%p (%Iu bytes): [%02X %02X %02X %02X %02X %02X %02X %02X] / RA:0x%p / %s[%lu]\n",
                     p, (SIZE_T)((LPBYTE)(lpBlock->lpPost) - p),
                     p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], lpBlock->lpRetAddress,
                     lpBlock->szFilenameA, (ULONG)(lpBlock->nLineNumber));
    }
    else
    {
      if (szFilenameA != NULL)
        continue;
      MX::DebugPrint("MXLIB: Leak 0x%p (%Iu bytes): [%02X %02X %02X %02X %02X %02X %02X %02X] / RA:0x%p\n",
                     p, (SIZE_T)((LPBYTE)(lpBlock->lpPost) - p),
                     p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], lpBlock->lpRetAddress);
    }

#ifdef HEAP_CHECK_CAPTURE_STACK
    if (fnSymFromAddrW != NULL)
    {
      for (nIdx = 0; nIdx < HEAP_CHECK_STACK_ENTRIES; nIdx++)
      {
        if (lpBlock->nStackValues[nIdx] == 0)
          break;
        ::MxMemSet(&(sFullSymbolInfoW.sSymbolInfoW), 0, sizeof(sFullSymbolInfoW.sSymbolInfoW));
        sFullSymbolInfoW.sSymbolInfoW.SizeOfStruct = (ULONG)sizeof(sFullSymbolInfoW.sSymbolInfoW);
        sFullSymbolInfoW.sSymbolInfoW.MaxNameLen = MX_ARRAYLEN(sFullSymbolInfoW.szNameContinuationW);
        qwDisp = 0;
        if (fnSymFromAddrW(::GetCurrentProcess(), (DWORD64)(lpBlock->nStackValues[nIdx]), &qwDisp,
                           &(sFullSymbolInfoW.sSymbolInfoW)) != FALSE)
        {
          sFullSymbolInfoW.szNameContinuationW[MX_ARRAYLEN(sFullSymbolInfoW.szNameContinuationW) - 1] = 0;
        }
        else
        {
          sFullSymbolInfoW.sSymbolInfoW.Name[0] = 0;
        }
        MX::DebugPrint("  %s 0x%IX => %S + 0x%IX\n", ((nIdx == 0) ? "Stack:" : "      "), lpBlock->nStackValues[nIdx],
                       ((sFullSymbolInfoW.sSymbolInfoW.Name[0] != 0) ? sFullSymbolInfoW.sSymbolInfoW.Name
                                                                     : L"Unknown"),
                       qwDisp);
      }
    }
    else
    {
      int len, ofs = 0;
      for (nIdx = 0; nIdx < HEAP_CHECK_STACK_ENTRIES; nIdx++)
      {
        if (lpBlock->nStackValues[nIdx] == 0)
          break;
        len = mx_sprintf_s(szBufferA + (SIZE_T)ofs, MX_ARRAYLEN(szBufferA) - (SIZE_T)ofs, "%s0x%IX",
                           ((nIdx == 0) ? "" : ", "), lpBlock->nStackValues[nIdx]);
        if (len > 0)
          ofs += len;
      }
      if (ofs > 0)
      {
        MX::DebugPrint("  Stack: [%.*s]\n", ofs, szBufferA);
      }
    }
#endif //HEAP_CHECK_CAPTURE_STACK
  }
  return;
}

#ifdef __cplusplus
} //extern "C"
#endif //__cplusplus

#endif //MX_MEMORY_OBJECTS_HEAP_CHECK

static void* DefaultMalloc(_In_ size_t nSize)
{
  return ::MxRtlAllocateHeap(::MxGetProcessHeap(), 0, nSize);
}

static void* DefaultRealloc(_In_opt_ void *lpPtr, _In_ size_t nSize)
{
  return ::MxRtlReAllocateHeap(::MxGetProcessHeap(), 0, lpPtr, nSize);
}

static void DefaultFree(_In_opt_ void *lpPtr)
{
  ::MxRtlFreeHeap(::MxGetProcessHeap(), 0, lpPtr);
  return;
}

static size_t DefaultMemSize(_In_opt_ void *lpPtr)
{
  return ::MxRtlSizeHeap(::MxGetProcessHeap(), 0, lpPtr);
}
