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
#include "..\Include\NtDefs.h"
#include "..\Include\MemoryObjects.h"
#include "..\Include\WaitableObjects.h"
#include "..\Include\Debug.h"
#include <intrin.h>

#pragma intrinsic (_InterlockedIncrement)
#pragma intrinsic(_ReturnAddress)

//#define USE_CRT_ALLOC
#ifdef _DEBUG
  #define DO_HEAP_CHECK
#else //_DEBUG
  //#define DO_HEAP_CHECK
#endif //_DEBUG

//-----------------------------------------------------------

#ifdef DO_HEAP_CHECK
  #if defined(_M_X64) || defined(_M_IA64) || defined(_M_AMD64)
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
    MINIDEBUG_POSTBLOCK MX_UNALIGNED *lpPost;
  } MINIDEBUG_PREBLOCK;
  #pragma pack()

  #define EXTRA_ALLOC (sizeof(MINIDEBUG_PREBLOCK)+sizeof(MINIDEBUG_POSTBLOCK))

  #define CHECK_COUNTER_START 128
#endif //DO_HEAP_CHECK

#define XISALIGNED(x)  ((((SIZE_T)(x)) & (sizeof(SIZE_T)-1)) == 0)

//-----------------------------------------------------------

extern "C" {

size_t __stdcall MxTryMemCopy(__out const void *lpDest, __in const void *lpSrc, __in size_t nCount);

}; //extern "C"

#ifdef DO_HEAP_CHECK
static VOID InitializeBlock(__in MINIDEBUG_PREBLOCK *pPreBlk, __in SIZE_T nSize, __in_z_opt const char *szFilenameA,
                            __in int nLineNumber, __in LPVOID lpRetAddress);
static VOID SetBlockTags(__in MINIDEBUG_PREBLOCK *pPreBlk, __in BOOL bOnFree);
static VOID CheckBlocks();
static VOID CheckBlock(__in MINIDEBUG_PREBLOCK *pPreBlk);
static VOID LinkBlock(__in MINIDEBUG_PREBLOCK *pPreBlk);
static VOID UnlinkBlock(__in MINIDEBUG_PREBLOCK *pPreBlk);
static VOID AssertNotTag(__in MINIDEBUG_PREBLOCK *pPreBlk);
static VOID DumpLeaks();
#endif //DO_HEAP_CHECK

//-----------------------------------------------------------

#ifdef DO_HEAP_CHECK
typedef void(*_PVFV)(void);

#pragma section(".CRT$XTY", long, read)  // NOLINT
static __declspec(allocate(".CRT$XTY")) _PVFV my_exit_funcs[] = { &DumpLeaks };
#endif //DO_HEAP_CHECK

//-----------------------------------------------------------

#ifdef DO_HEAP_CHECK
static struct {
  LONG volatile nMutex;
  LONG volatile nCheckCounter;
  MINIDEBUG_PREBLOCK *lpHead;
  MINIDEBUG_PREBLOCK *lpTail;
} sAllocatedBlocks = { 0, CHECK_COUNTER_START, NULL, NULL };
#endif //DO_HEAP_CHECK

//-----------------------------------------------------------

namespace MX {

LPVOID MemAlloc(__in SIZE_T nSize)
{
#if (!defined(USE_CRT_ALLOC)) && defined(DO_HEAP_CHECK)
  return MemAllocD(nSize, (const char*)_ReturnAddress(), -1);
#else //!USE_CRT_ALLOC && DO_HEAP_CHECK
  return MemAllocD(nSize, NULL, 0);
#endif //!USE_CRT_ALLOC && DO_HEAP_CHECK
}

LPVOID MemAllocD(__in SIZE_T nSize, __in_z_opt const char *szFilenameA, __in int nLineNumber)
{
#ifdef USE_CRT_ALLOC

#ifdef _CRTDBG_MAP_ALLOC
  return _malloc_dbg(nSize, _NORMAL_BLOCK, szFilenameA, nLineNumber);
#else //_CRTDBG_MAP_ALLOC
  return malloc(nSize);
#endif //_CRTDBG_MAP_ALLOC

#else //USE_CRT_ALLOC

#ifdef DO_HEAP_CHECK
  LPBYTE p;
  MINIDEBUG_PREBLOCK *pPreBlk;
  LPVOID lpRetAddress;

  if (nLineNumber < 0)
  {
    lpRetAddress = (LPVOID)szFilenameA;
    szFilenameA = NULL;
    nLineNumber = 0;
  }
  else
  {
    lpRetAddress = _ReturnAddress();
  }
#endif //DO_HEAP_CHECK

  if (nSize == 0)
    nSize = 1;
#ifdef DO_HEAP_CHECK
  CheckBlocks();
  p = (LPBYTE)::MxRtlAllocateHeap(::MxGetProcessHeap(), 0, nSize+EXTRA_ALLOC);
  if (p != NULL)
  {
    pPreBlk = (MINIDEBUG_PREBLOCK*)p;
    InitializeBlock(pPreBlk, nSize, szFilenameA, nLineNumber, lpRetAddress);
    LinkBlock(pPreBlk);
    p += sizeof(MINIDEBUG_PREBLOCK);
  }
  return p;
#else //DO_HEAP_CHECK
  return ::MxRtlAllocateHeap(::MxGetProcessHeap(), 0, nSize);
#endif //DO_HEAP_CHECK

#endif //USE_CRT_ALLOC
}

LPVOID MemRealloc(__in LPVOID lpPtr, __in SIZE_T nSize)
{
#if (!defined(USE_CRT_ALLOC)) && defined(DO_HEAP_CHECK)
  return MemReallocD(lpPtr, nSize, (const char*)_ReturnAddress(), -1);
#else //!USE_CRT_ALLOC && DO_HEAP_CHECK
  return MemReallocD(lpPtr, nSize, NULL, 0);
#endif //!USE_CRT_ALLOC && DO_HEAP_CHECK
}

LPVOID MemReallocD(__in LPVOID lpPtr, __in SIZE_T nSize, __in_z_opt const char *szFilenameA, __in int nLineNumber)
{
#ifdef USE_CRT_ALLOC

#ifdef _CRTDBG_MAP_ALLOC
  return _realloc_dbg(lpPtr, nSize, _NORMAL_BLOCK, szFilenameA, nLineNumber);
#else //_CRTDBG_MAP_ALLOC
  return realloc(lpPtr, nSize);
#endif //_CRTDBG_MAP_ALLOC

#else //USE_CRT_ALLOC

#ifdef DO_HEAP_CHECK
  MINIDEBUG_PREBLOCK *pPreBlk;
  LPBYTE p;
  LPVOID lpRetAddress;
#endif //DO_HEAP_CHECK

  if (nSize == 0)
  {
    MemFree(lpPtr);
    return NULL;
  }
  if (lpPtr == NULL)
    return MemAllocD(nSize, szFilenameA, nLineNumber);

#ifdef DO_HEAP_CHECK
  if (nLineNumber < 0)
  {
    lpRetAddress = (LPVOID)szFilenameA;
    szFilenameA = NULL;
    nLineNumber = 0;
  }
  else
  {
    lpRetAddress = _ReturnAddress();
  }

  CheckBlocks();
  pPreBlk = (MINIDEBUG_PREBLOCK*)((LPBYTE)lpPtr - sizeof(MINIDEBUG_PREBLOCK));
  CheckBlock(pPreBlk);
  UnlinkBlock(pPreBlk);
  SetBlockTags(pPreBlk, TRUE);
  p = (LPBYTE)::MxRtlReAllocateHeap(::MxGetProcessHeap(), 0, pPreBlk, nSize+EXTRA_ALLOC);
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
#else //DO_HEAP_CHECK
  return ::MxRtlReAllocateHeap(::MxGetProcessHeap(), 0, lpPtr, nSize);
#endif //DO_HEAP_CHECK

#endif //USE_CRT_ALLOC
}

VOID MemFree(__in LPVOID lpPtr)
{
#ifdef USE_CRT_ALLOC

#ifdef _CRTDBG_MAP_ALLOC
  _free_dbg(lpPtr, _NORMAL_BLOCK);
#else //_CRTDBG_MAP_ALLOC
  free(lpPtr);
#endif //_CRTDBG_MAP_ALLOC

#else //USE_CRT_ALLOC

  if (lpPtr != NULL)
  {
#ifdef DO_HEAP_CHECK
    MINIDEBUG_PREBLOCK *pPreBlk;

    CheckBlocks();
    pPreBlk = (MINIDEBUG_PREBLOCK*)((LPBYTE)lpPtr - sizeof(MINIDEBUG_PREBLOCK));
    CheckBlock(pPreBlk);
    UnlinkBlock(pPreBlk);
    SetBlockTags(pPreBlk, TRUE);
    lpPtr = pPreBlk;
#endif //DO_HEAP_CHECK
    ::MxRtlFreeHeap(::MxGetProcessHeap(), 0, lpPtr);
  }

#endif //USE_CRT_ALLOC
  return;
}

SIZE_T MemSize(__in LPVOID lpPtr)
{
#ifdef USE_CRT_ALLOC

#ifdef _CRTDBG_MAP_ALLOC
  return _msize_dbg(lpPtr, _NORMAL_BLOCK);
#else //_CRTDBG_MAP_ALLOC
  return _msize(lpPtr);
#endif //_CRTDBG_MAP_ALLOC

#else //USE_CRT_ALLOC

  if (lpPtr != NULL)
  {
#ifdef DO_HEAP_CHECK
    MINIDEBUG_PREBLOCK *pPreBlk;
    SIZE_T nSize;

    CheckBlocks();
    pPreBlk = (MINIDEBUG_PREBLOCK*)((LPBYTE)lpPtr - sizeof(MINIDEBUG_PREBLOCK));
    CheckBlock(pPreBlk);
    nSize = ::MxRtlSizeHeap(::MxGetProcessHeap(), 0, pPreBlk);
    return (nSize > EXTRA_ALLOC) ? (nSize - EXTRA_ALLOC) : 0;
#else //DO_HEAP_CHECK
    return ::MxRtlSizeHeap(::MxGetProcessHeap(), 0, lpPtr);
#endif //DO_HEAP_CHECK
  }
  return 0;

#endif //USE_CRT_ALLOC
}

VOID MemSet(__out LPVOID lpDest, __in int nVal, __in SIZE_T nCount)
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

VOID MemCopy(__out LPVOID lpDest, __in LPCVOID lpSrc, __in SIZE_T nCount)
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

VOID MemMove(__out LPVOID lpDest, __in LPCVOID lpSrc, __in SIZE_T nCount)
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

int MemCompare(__in LPCVOID lpSrc1, __in LPCVOID lpSrc2, __in SIZE_T nCount)
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

SIZE_T TryMemCopy(__out LPVOID lpDest, __in LPCVOID lpSrc, __in SIZE_T nCount)
{
  return MxTryMemCopy(lpDest, lpSrc, nCount);
}

} //namespace MX

#ifdef DO_HEAP_CHECK
static VOID InitializeBlock(__in MINIDEBUG_PREBLOCK *pPreBlk, __in SIZE_T nSize, __in_z_opt const char *szFilenameA,
                            __in int nLineNumber, __in LPVOID lpRetAddress)
{
  pPreBlk->lpNext = pPreBlk->lpPrev = NULL;
  pPreBlk->szFilenameA = (LPCSTR)szFilenameA;
  pPreBlk->nLineNumber = (SIZE_T)nLineNumber;
  pPreBlk->lpRetAddress = lpRetAddress;
  pPreBlk->lpPost = (MINIDEBUG_POSTBLOCK*)((LPBYTE)pPreBlk + sizeof(MINIDEBUG_PREBLOCK) + nSize);
  SetBlockTags(pPreBlk, FALSE);
  pPreBlk->lpPost->szFilenameA = (LPCSTR)szFilenameA;
  pPreBlk->lpPost->nLineNumber = (SIZE_T)nLineNumber;
  return;
}

static VOID SetBlockTags(__in MINIDEBUG_PREBLOCK *pPreBlk, __in BOOL bOnFree)
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

static VOID CheckBlocks()
{
  if (_InterlockedDecrement(&(sAllocatedBlocks.nCheckCounter)) == 0)
  {
    MX::CFastLock cLock(&(sAllocatedBlocks.nMutex));
    MINIDEBUG_PREBLOCK *lpBlock;

    _InterlockedExchange(&(sAllocatedBlocks.nCheckCounter), CHECK_COUNTER_START);
    for (lpBlock=sAllocatedBlocks.lpHead; lpBlock!=NULL; lpBlock=lpBlock->lpNext)
      CheckBlock(lpBlock);
  }
  return;
}

static VOID CheckBlock(__in MINIDEBUG_PREBLOCK *pPreBlk)
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

static VOID LinkBlock(__in MINIDEBUG_PREBLOCK *pPreBlk)
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
  return;
}

static VOID UnlinkBlock(__in MINIDEBUG_PREBLOCK *pPreBlk)
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
  return;
}

static VOID AssertNotTag(__in MINIDEBUG_PREBLOCK *pPreBlk)
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

static VOID DumpLeaks()
{
  MX::CFastLock cLock(&(sAllocatedBlocks.nMutex));
  MINIDEBUG_PREBLOCK *lpBlock;

  for (lpBlock=sAllocatedBlocks.lpHead; lpBlock!=NULL; lpBlock=lpBlock->lpNext)
  {
    LPBYTE p = (LPBYTE)(lpBlock+1);

    if (lpBlock->szFilenameA != NULL && lpBlock->szFilenameA[0] != 0)
    {
      MX::DebugPrint("MXLIB: Leak 0x%p (%Iu bytes): [%02X %02X %02X %02X %02X %02X %02X %02X] / RA:0x%p / %s[%lu]\n",
                     p, (SIZE_T)((LPBYTE)(lpBlock->lpPost) - p),
                     p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], lpBlock->lpRetAddress,
                     lpBlock->szFilenameA, (ULONG)(lpBlock->nLineNumber));
    }
    else
    {
      MX::DebugPrint("MXLIB: Leak 0x%p (%Iu bytes): [%02X %02X %02X %02X %02X %02X %02X %02X] / RA:0x%p\n",
                     p, (SIZE_T)((LPBYTE)(lpBlock->lpPost) - p),
                     p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], lpBlock->lpRetAddress);
    }
  }
  return;
}
#endif //DO_HEAP_CHECK
