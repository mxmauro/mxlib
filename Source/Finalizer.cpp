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
#include "..\Include\Finalizer.h"
#include "..\Include\WaitableObjects.h"

//-----------------------------------------------------------

typedef struct tagFINALIZER_ITEM {
  MX::lpfnFinalizer fnFinalizer;
  SIZE_T nPriority;
} FINALIZER_ITEM;

//-----------------------------------------------------------

static void __cdecl RunFinalizers(void);

//-----------------------------------------------------------

#if _MSC_FULL_VER < 140050214
  #error Unsupported compiler version
#endif //_MSC_FULL_VER < 140050214
#pragma section(".CRT$XTS",long,read)
#pragma data_seg(".CRT$XTS")
static LPVOID lpfnRunFinalizers = &RunFinalizers;
#pragma data_seg()

static LONG volatile nMutex = 0;
static FINALIZER_ITEM *lpList = NULL;
static SIZE_T nListSize = 0;
static SIZE_T nListCount = 0;

//-----------------------------------------------------------

namespace MX {

HRESULT RegisterFinalizer(_In_ lpfnFinalizer fnFinalizer, _In_ SIZE_T nPriority)
{
  CFastLock cLock(&nMutex);
  SIZE_T i;

  if (fnFinalizer == NULL)
    return E_POINTER;
  //ensure enough space is available
  if (nListCount >= nListSize)
  {
    FINALIZER_ITEM *lpNewList = NULL;
    SIZE_T nSize = (nListSize+32) * sizeof(FINALIZER_ITEM);

    if (!NT_SUCCESS(::MxNtAllocateVirtualMemory(MX_CURRENTPROCESS, (PVOID*)&lpNewList, 0, &nSize, MEM_COMMIT,
                                                PAGE_READWRITE)))
      return E_OUTOFMEMORY;
    if (lpList != NULL)
    {
      MxMemCopy(lpNewList, lpList, nListCount*sizeof(FINALIZER_ITEM));
      nSize = 0;
      MxNtFreeVirtualMemory(MX_CURRENTPROCESS, (PVOID*)&lpList, &nSize, MEM_RELEASE);
    }
    lpList = lpNewList;
    nListSize += 32;
  }
  //find insertion point
  for (i=0; i<nListCount; i++)
  {
    if (lpList[i].nPriority > nPriority)
      break;
  }
  //insert new item at position 'i'
  MxMemMove(lpList+(i+1), lpList+i, (nListCount-i) * sizeof(FINALIZER_ITEM));
  lpList[i].fnFinalizer = fnFinalizer;
  lpList[i].nPriority = nPriority;
  nListCount++;
  //done
  return S_OK;
}

} //namespace MX

//-----------------------------------------------------------

static void __cdecl RunFinalizers(void)
{
  MX::CFastLock cLock(&nMutex);
  SIZE_T i;

  for (i=0; i<nListCount; i++)
  {
    lpList[i].fnFinalizer();
  }
  //free list
  if (lpList != NULL)
  {
    i = 0;
    MxNtFreeVirtualMemory(MX_CURRENTPROCESS, (PVOID*)&lpList, &i, MEM_RELEASE);
    lpList = NULL;
    nListCount = nListSize = 0;
  }
  return;
}
