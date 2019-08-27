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
