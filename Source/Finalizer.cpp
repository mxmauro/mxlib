/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2015. All rights reserved.
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

HRESULT RegisterFinalizer(__in lpfnFinalizer fnFinalizer, __in SIZE_T nPriority)
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
      MemCopy(lpNewList, lpList, nListCount*sizeof(FINALIZER_ITEM));
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
  MemMove(lpList+(i+1), lpList+i, (nListCount-i) * sizeof(FINALIZER_ITEM));
  lpList[i].fnFinalizer = fnFinalizer;
  lpList[i].nPriority = nPriority;
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
