/*
 * Copyright (C) 2010-2014 Nektra S.A., Buenos Aires, Argentina.
 * All rights reserved.
 *
 **/

#ifndef _MEMORYMANAGER_POOL_H
#define _MEMORYMANAGER_POOL_H

#include "Main.h"

//-----------------------------------------------------------

#define NKT_MEMMGRPOOL_MAXAGE                          10000
#define NKT_MEMMGRPOOL_CHECK_FREQUENCY                   128

//-----------------------------------------------------------

class CMemoryManager
{
public:
  CMemoryManager(__in BOOL bExecutable);
  ~CMemoryManager();

  LPVOID Malloc(__in SIZE_T nSize);
  LPVOID Realloc(__in LPVOID lpOld, __in SIZE_T nNewSize);
  VOID Free(__in LPVOID lpPtr);

  SIZE_T BlockSize(__in LPVOID lpPtr);

public:
  typedef struct tagDLLIST_ITEM {
    struct tagDLLIST_ITEM *lpNext, *lpPrev;
  } DLLIST_ITEM, *LPDLLIST_ITEM;

private:
  class CBin
  {
  public:
    CBin();
    ~CBin();

    LPVOID Alloc(__in DWORD dwFlags);
    VOID Free(__in LPVOID lpPtr);

    VOID Cleanup();

  private:
    friend class CMemoryManager;

    LONG volatile nMutex;
    DLLIST_ITEM sFreeList, sInUseList, sFullList;
    DWORD dwBlockSize, dwBlocksPerChunk;
  };

private:
  LPVOID InternalAlloc(__in SIZE_T nSize);
  static VOID DlListInsertHead(__inout LPDLLIST_ITEM lpList, __inout LPDLLIST_ITEM lpItem);
  static VOID DlListInsertTail(__inout LPDLLIST_ITEM lpList, __inout LPDLLIST_ITEM lpItem);
  static VOID DlListRemove(__inout LPDLLIST_ITEM lpItem);
  static SIZE_T IndexFromSize(__in SIZE_T nSize);

private:
  CBin cBins[12];
  BOOL bExecutable;
};

//-----------------------------------------------------------

#endif _MEMORYMANAGER_POOL_H
