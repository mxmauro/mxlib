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
#include "MemoryManagerPool.h"
#include "WaitableObjects.h"
#include <intrin.h>

#pragma intrinsic(_BitScanReverse)

//-----------------------------------------------------------

#define BINS_COUNT                                        12

#if defined(_M_IX86)
  #define TAG_MARK                              0xDE324F1AUL
#elif defined(_M_X64)
  #define TAG_MARK                    0xDE324F1A8492B311ui64
#else
  #error Unsupported platform
#endif

#ifdef _DEBUG
  #define DO_SANITY_CHECK
#else //_DEBUG
  //#define DO_SANITY_CHECK
#endif //_DEBUG

//-----------------------------------------------------------

#pragma pack(8)
typedef struct tagHEADER {
  union {
    SIZE_T nTag;
    ULONGLONG _pad0;
  };
  DWORD dwFreeCount;
  CMemoryManager::DLLIST_ITEM sLink;
  LPVOID lpBin;
  SIZE_T* lpFirstFreeBlock;
} HEADER, *LPHEADER;
#pragma pack()

#pragma pack(1)
typedef struct tagCHUNK {
  HEADER sHeader;
  BYTE aBuffer[0x10000-sizeof(HEADER)];
} CHUNK, *LPCHUNK;
#pragma pack()

#define CHUNK_FROM_DLLIST(ptr) (LPCHUNK)((LPBYTE)ptr - FIELD_OFFSET(CHUNK, sHeader.sLink))

//-----------------------------------------------------------

#ifdef _DEBUG
static VOID SanityCheck(__in LPCHUNK lpChunk, __in DWORD dwBlocksPerChunk);
#endif //_DEBUG

//-----------------------------------------------------------

CMemoryManager::CMemoryManager(__in BOOL _bExecutable)
{
  SIZE_T i;

  bExecutable = _bExecutable;
  for (i=0; i<NKT_ARRAYLEN(cBins); i++)
  {
    cBins[i].dwBlockSize = 8UL << i;
    cBins[i].dwBlocksPerChunk = (DWORD)(0x10000-sizeof(HEADER)) / cBins[i].dwBlockSize;
    //----
    cBins[i].sFreeList.lpNext =  cBins[i].sFreeList.lpPrev =  &(cBins[i].sFreeList);
    cBins[i].sInUseList.lpNext = cBins[i].sInUseList.lpPrev = &(cBins[i].sInUseList);
    cBins[i].sFullList.lpNext =  cBins[i].sFullList.lpPrev =  &(cBins[i].sFullList);
  }
  return;
}

CMemoryManager::~CMemoryManager()
{
  return;
}

LPVOID CMemoryManager::Malloc(__in SIZE_T nSize)
{
  return InternalAlloc(nSize);
}

LPVOID CMemoryManager::Realloc(__in LPVOID lpOld, __in SIZE_T nNewSize)
{
  LPVOID lpNewPtr;
  SIZE_T nOrigSize;

  if (lpOld == NULL)
    return Malloc(nNewSize);
  if (nNewSize == 0)
  {
    Free(lpOld);
    return NULL;
  }
  //get info from original memory
  nOrigSize = BlockSize(lpOld);
  //reallocate
  lpNewPtr = Malloc(nNewSize);
  if (lpNewPtr != NULL)
  {
    NktHookLibHelpers::MemCopy(lpNewPtr, lpOld, (nOrigSize < nNewSize) ? nOrigSize : nNewSize);
    Free(lpOld);
  }
  return lpNewPtr;
}

VOID CMemoryManager::Free(__in LPVOID lpPtr)
{
  if (lpPtr != NULL)
  {
    LPHEADER lpHdr;
    SIZE_T k;

    lpHdr = (LPHEADER)((SIZE_T)lpPtr & (~0xFFFF));
    NKT_ASSERT(lpHdr->nTag == (TAG_MARK ^ (SIZE_T)lpHdr));
    if (lpHdr->lpBin != NULL)
    {
      ((CBin*)(lpHdr->lpBin))->Free(lpPtr);
    }
    else
    {
      k = 0;
      NktNtFreeVirtualMemory((HANDLE)-1, (PVOID*)&lpHdr, &k, MEM_RELEASE);
    }
  }
  return;
}

SIZE_T CMemoryManager::BlockSize(__in LPVOID lpPtr)
{
  LPHEADER lpHdr;
  MEMORY_BASIC_INFORMATION sMbi;
  SIZE_T k, nSize;
  NTSTATUS nNtStatus;

  nSize = 0;
  if (lpPtr != NULL)
  {
    lpHdr = (LPHEADER)((SIZE_T)lpPtr & (~0xFFFF));
    NKT_ASSERT(lpHdr->nTag == (TAG_MARK ^ (SIZE_T)lpHdr));
    if (lpHdr->lpBin != NULL)
    {
      nSize = ((CBin*)(lpHdr->lpBin))->dwBlockSize;
    }
    else
    {
      NktHookLibHelpers::MemSet(&sMbi, 0, sizeof(sMbi));
      k = 0;
      nNtStatus = NktNtQueryVirtualMemory((HANDLE)-1, lpHdr, 0, &sMbi, sizeof(sMbi), &k);
      if (NT_SUCCESS(nNtStatus) && k >= FIELD_OFFSET(MEMORY_BASIC_INFORMATION, State))
      {
        NKT_ASSERT(sMbi.RegionSize >= sizeof(HEADER));
        nSize = (sMbi.RegionSize >= sizeof(HEADER)) ? (sMbi.RegionSize-sizeof(HEADER)) : 0;
      }
    }
  }
  return nSize;
}

LPVOID CMemoryManager::InternalAlloc(__in SIZE_T nSize)
{
  LPHEADER lpPtr;
  SIZE_T k, nBinIdx;
  NTSTATUS nNtStatus;

  nBinIdx = IndexFromSize(nSize);
  if (nBinIdx < BINS_COUNT)
    return cBins[nBinIdx].Alloc((bExecutable != FALSE) ? 1 : 0);
  //allocate a superblock
  lpPtr = NULL;
  k = nSize+sizeof(HEADER);
  nNtStatus = NktNtAllocateVirtualMemory(NKTHOOKLIB_CurrentProcess, (PVOID*)&lpPtr, 0, &k, MEM_RESERVE|MEM_COMMIT,
                                         (bExecutable == FALSE) ? PAGE_READWRITE : PAGE_EXECUTE_READWRITE);
  if (!NT_SUCCESS(nNtStatus))
    return NULL;
  NKT_ASSERT(((SIZE_T)lpPtr & 0xFFFF) == 0); //ensure it is 64k aligned
  NktHookLibHelpers::MemSet(lpPtr, 0, sizeof(HEADER));
  lpPtr->nTag = TAG_MARK ^ (SIZE_T)lpPtr;
  return (lpPtr+1);
}

VOID CMemoryManager::DlListInsertHead(__inout LPDLLIST_ITEM lpList, __inout LPDLLIST_ITEM lpItem)
{
  NKT_ASSERT(lpItem->lpNext == lpItem && lpItem->lpPrev == lpItem);
  lpItem->lpNext = lpList->lpNext;
  lpItem->lpPrev = lpList;
  lpList->lpNext->lpPrev = lpItem;
  lpList->lpNext = lpItem;
  return;
}

VOID CMemoryManager::DlListInsertTail(__inout LPDLLIST_ITEM lpList, __inout LPDLLIST_ITEM lpItem)
{
  NKT_ASSERT(lpItem->lpNext == lpItem && lpItem->lpPrev == lpItem);
  lpItem->lpNext = lpList;
  lpItem->lpPrev = lpList->lpPrev;
  lpList->lpPrev->lpNext = lpItem;
  lpList->lpPrev = lpItem;
  return;
}

VOID CMemoryManager::DlListRemove(__inout LPDLLIST_ITEM lpItem)
{
  lpItem->lpPrev->lpNext = lpItem->lpNext;
  lpItem->lpNext->lpPrev = lpItem->lpPrev;
  lpItem->lpNext = lpItem->lpPrev = lpItem;
  return;
}

SIZE_T CMemoryManager::IndexFromSize(__in SIZE_T nSize)
{
  unsigned long _ndx;

  if (nSize >= 32768)
    return BINS_COUNT;
  if (nSize < 8)
    nSize = 8;
  _BitScanReverse(&_ndx, (unsigned long)nSize);
  if ((nSize & (nSize-1)) != 0)
    _ndx++;
  return _ndx-3;
}

//-----------------------------------------------------------

CMemoryManager::CBin::CBin()
{
  return;
}

CMemoryManager::CBin::~CBin()
{
  Cleanup();
  return;
}

LPVOID CMemoryManager::CBin::Alloc(__in DWORD dwFlags)
{
  CNktSimpleLockNonReentrant cLock(&nMutex);
  LPCHUNK lpChunk;
  SIZE_T k, *lpPtr;
  DWORD dw;
  NTSTATUS nNtStatus;

  if (sInUseList.lpNext == &sInUseList)
  {
    //check if we can get a chunk from the free list
    if (sFreeList.lpNext != &sFreeList)
    {
      lpChunk = CHUNK_FROM_DLLIST(sFreeList.lpNext);
      DlListRemove(&(lpChunk->sHeader.sLink));
      DlListInsertHead(&sInUseList, &(lpChunk->sHeader.sLink));
    }
    else
    {
      //allocate a chunk
      lpChunk = NULL;
      k = sizeof(CHUNK);
      nNtStatus = NktNtAllocateVirtualMemory((HANDLE)-1, (PVOID*)&lpChunk, 0, &k, MEM_RESERVE|MEM_COMMIT,
                                             ((dwFlags & 1) == 0) ? PAGE_READWRITE : PAGE_EXECUTE_READWRITE);
      if (!NT_SUCCESS(nNtStatus))
        return NULL;
      NKT_ASSERT(((SIZE_T)lpChunk & 0xFFFF) == 0); //ensure it is 64k aligned
      NktHookLibHelpers::MemSet(&(lpChunk->sHeader), 0, sizeof(lpChunk->sHeader));
      lpChunk->sHeader.dwFreeCount = dwBlocksPerChunk;
      lpChunk->sHeader.nTag = TAG_MARK ^ (SIZE_T)&(lpChunk->sHeader);
      lpChunk->sHeader.sLink.lpNext = lpChunk->sHeader.sLink.lpPrev = &(lpChunk->sHeader.sLink);
      lpChunk->sHeader.lpBin = this;
      lpPtr = lpChunk->sHeader.lpFirstFreeBlock = (SIZE_T*)(lpChunk->aBuffer);
      //init buffers internal links
      for (dw=lpChunk->sHeader.dwFreeCount; dw>1; dw--)
      {
        *lpPtr = (SIZE_T)lpPtr + (SIZE_T)dwBlockSize;
        lpPtr = (SIZE_T*)(*lpPtr);
      }
      *lpPtr = 0; //last is null
      //insert block into available list
      DlListInsertHead(&sInUseList, &(lpChunk->sHeader.sLink));
    }
  }
  //get a chunk from the available list
  NKT_ASSERT(sInUseList.lpNext != &sInUseList);
  lpChunk = CHUNK_FROM_DLLIST(sInUseList.lpNext);
  //get the block to return
  lpPtr = lpChunk->sHeader.lpFirstFreeBlock;
  //decrement free count
  (lpChunk->sHeader.dwFreeCount)--;
  //set the pointer to the new block
  lpChunk->sHeader.lpFirstFreeBlock = (SIZE_T*)(*lpPtr);
  if (lpChunk->sHeader.lpFirstFreeBlock == NULL)
  {
    NKT_ASSERT(lpChunk->sHeader.dwFreeCount == 0);
    //if chunk is full, move it to the full list
    DlListRemove(&(lpChunk->sHeader.sLink));
    DlListInsertHead(&sFullList, &(lpChunk->sHeader.sLink));
  }
#ifdef _DEBUG
  SanityCheck(lpChunk, dwBlocksPerChunk);
#endif //_DEBUG
  //done
  return lpPtr;
}

VOID CMemoryManager::CBin::Free(__in LPVOID lpPtr)
{
  if (lpPtr != NULL)
  {
    CNktSimpleLockNonReentrant cLock(&nMutex);
    LPCHUNK lpChunk;

    lpChunk = (LPCHUNK)((SIZE_T)lpPtr & (~0xFFFF));
    NKT_ASSERT((LPBYTE)lpPtr >= (LPBYTE)&(lpChunk->aBuffer));
    NKT_ASSERT((LPBYTE)lpPtr < (LPBYTE)&(lpChunk->aBuffer)+sizeof(lpChunk->aBuffer));
    NKT_ASSERT((((SIZE_T)lpPtr - (SIZE_T)&(lpChunk->aBuffer)) % (SIZE_T)dwBlockSize) == 0);
    //add to the free block list
    *((SIZE_T*)lpPtr) = (SIZE_T)(lpChunk->sHeader.lpFirstFreeBlock);
    lpChunk->sHeader.lpFirstFreeBlock = (SIZE_T*)lpPtr;
    //increment free count
    (lpChunk->sHeader.dwFreeCount)++;
    if (lpChunk->sHeader.dwFreeCount >= dwBlocksPerChunk)
    {
      //chunk is completely free so move to the free list
      DlListRemove(&(lpChunk->sHeader.sLink));
      DlListInsertHead(&sFreeList, &(lpChunk->sHeader.sLink));
    }
    else if (lpChunk->sHeader.dwFreeCount > dwBlocksPerChunk/4)
    {
      //the chunk now has 1/4 of its blocks free so move it to the in-use list
      DlListRemove(&(lpChunk->sHeader.sLink));
      DlListInsertHead(&sInUseList, &(lpChunk->sHeader.sLink));
    }
#ifdef _DEBUG
    SanityCheck(lpChunk, dwBlocksPerChunk);
#endif //_DEBUG
  }
  return;
}

VOID CMemoryManager::CBin::Cleanup()
{
  LPCHUNK lpChunk;
  SIZE_T k;

  while (sFullList.lpNext != &sFullList)
  {
    lpChunk = CHUNK_FROM_DLLIST(sFullList.lpNext);
    DlListRemove(&(lpChunk->sHeader.sLink));
    //if some memory is occuped then leak
  }
  while (sInUseList.lpNext != &sInUseList)
  {
    lpChunk = CHUNK_FROM_DLLIST(sInUseList.lpNext);
    DlListRemove(&(lpChunk->sHeader.sLink));
    //if some memory is occuped then leak
  }
  while (sFreeList.lpNext != &sFreeList)
  {
    lpChunk = CHUNK_FROM_DLLIST(sFreeList.lpNext);
    DlListRemove(&(lpChunk->sHeader.sLink));
    k = 0;
    NktNtFreeVirtualMemory((HANDLE)-1, (PVOID*)&lpChunk, &k, MEM_RELEASE);
  }
  return;
}

//-----------------------------------------------------------

#ifdef _DEBUG
static VOID SanityCheck(__in LPCHUNK lpChunk, __in DWORD dwBlocksPerChunk)
{
  DWORD dwFreeCount;
  SIZE_T *lpPtr;

  NKT_ASSERT(lpChunk->sHeader.nTag == (TAG_MARK ^ (SIZE_T)(&(lpChunk->sHeader))));
  dwFreeCount = 0;
  lpPtr = lpChunk->sHeader.lpFirstFreeBlock;
  while (lpPtr != NULL)
  {
    dwFreeCount++;
    NKT_ASSERT((LPBYTE)lpPtr >= lpChunk->aBuffer);
    NKT_ASSERT((LPBYTE)lpPtr < lpChunk->aBuffer + sizeof(lpChunk->aBuffer));
    lpPtr = (SIZE_T*)(*lpPtr);
  }
  NKT_ASSERT(dwFreeCount == lpChunk->sHeader.dwFreeCount);
  NKT_ASSERT(dwFreeCount <= dwBlocksPerChunk);
  return;
}
#endif //DO_SANITY_CHECK
