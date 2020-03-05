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
#include "..\Include\StackTrace.h"

//-----------------------------------------------------------

#if defined(_M_X64)
#define ___UNW_FLAG_NHANDLER                               0

#define ___UNWIND_HISTORY_TABLE_SIZE                      12
#define ___UNWIND_HISTORY_TABLE_NONE                       0
#define ___UNWIND_HISTORY_TABLE_GLOBAL                     1
#define ___UNWIND_HISTORY_TABLE_LOCAL                      2
#endif //_M_X64

//-----------------------------------------------------------

#if defined(_M_X64)
typedef struct tagMX_UNWIND_HISTORY_TABLE_ENTRY {
  ULONG64 ImageBase;
  PRUNTIME_FUNCTION FunctionEntry;
} MX_UNWIND_HISTORY_TABLE_ENTRY, *PMX_UNWIND_HISTORY_TABLE_ENTRY;

typedef struct tagMX_UNWIND_HISTORY_TABLE {
  ULONG Count;
  UCHAR Search;
  DWORD64 LowAddress;
  DWORD64 HighAddress;
  MX_UNWIND_HISTORY_TABLE_ENTRY Entry[___UNWIND_HISTORY_TABLE_SIZE];
} MX_UNWIND_HISTORY_TABLE, *PMX_UNWIND_HISTORY_TABLE;
#endif //_M_X64

//-----------------------------------------------------------

static VOID GetStackLimits(_In_ HANDLE hThread, _Out_ SIZE_T &nLow, _Out_ SIZE_T &nHigh);

//-----------------------------------------------------------

namespace MX {

namespace StackTrace {

HRESULT Get(_Out_ SIZE_T *lpnOutput, _In_ SIZE_T nCount, _In_opt_ DWORD dwThreadId)
{
  CONTEXT sCtx;
  SIZE_T nStackLimitLow, nStackLimitHigh;
#if defined(_M_IX86)
  LPDWORD lpdwFrame;
#elif defined(_M_X64)
  CONTEXT sCapturedCtx;
  MX_UNWIND_HISTORY_TABLE sUnwindHistTbl;
  PRUNTIME_FUNCTION lpFunc;
  PVOID lpHandlerData;
  ULONG64 nEstablisherFrame, nImageBase;
#endif
  BYTE temp8;
  SIZE_T nRetrieved, nSkipCount;

  if (lpnOutput == NULL)
    return E_POINTER;
  if (nCount == 0)
    return E_INVALIDARG;
  *lpnOutput = 0;

  ::MxMemSet(&sCtx, 0, sizeof(sCtx));

  if (dwThreadId == 0 || dwThreadId == ::GetCurrentThreadId())
  {
    if (::GetThreadContext(::GetCurrentThread(), &sCtx) == FALSE)
      return MX_HRESULT_FROM_LASTERROR();
    GetStackLimits(NULL, nStackLimitLow, nStackLimitHigh);
  }
  else
  {
    HANDLE hThread;

    hThread = ::OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, dwThreadId);
    if (hThread == NULL)
      return MX_HRESULT_FROM_LASTERROR();
    if (::GetThreadContext(::GetCurrentThread(), &sCtx) == FALSE)
    {
      HRESULT hRes = MX_HRESULT_FROM_LASTERROR();
      ::CloseHandle(hThread);
      return hRes;
    }
    GetStackLimits(NULL, nStackLimitLow, nStackLimitHigh);
    ::CloseHandle(hThread);
  }

  nRetrieved = 0;
  nSkipCount = 1;

  __try
  {
#if defined(_M_IX86)
    lpdwFrame = (LPDWORD)(sCtx.Ebp);
    //first item is pointer by esp register
    if (nCount > 0)
    {
      if (nSkipCount == 0)
      {
        lpnOutput[nRetrieved] = (SIZE_T)*((LPDWORD)(sCtx.Esp));
        //try to read byte at lpnOutput[k], if that "eip" is invalid, an exception will raise and loop will end
        temp8 = *((LPBYTE)lpnOutput[nRetrieved]);
        nRetrieved++;
        nCount--;
      }
      else
      {
        nSkipCount--;
      }
    }
    //get the rest
    while (nCount > 0)
    {
      if (((SIZE_T)lpdwFrame & 0x03) != 0)
        break; //unaligned
      if ((SIZE_T)lpdwFrame < nStackLimitLow || (SIZE_T)lpdwFrame >= nStackLimitHigh)
        break;
      if (nSkipCount > 0)
      {
        nSkipCount--;
      }
      else
      {
        lpnOutput[nRetrieved] = (SIZE_T)lpdwFrame[1];
        //try to read byte at lpnOutput[k], if that "eip" is invalid, an exception will raise and loop will end
        if (lpnOutput[nRetrieved] == 0)
          break;
        temp8 = *((LPBYTE)lpnOutput[nRetrieved]);
        nRetrieved++;
        nCount--;
      }
      lpdwFrame = (LPDWORD)(lpdwFrame[0]);
    }

#elif defined(_M_X64)

    ::MxRtlCaptureContext(&sCapturedCtx);
    sUnwindHistTbl.Search = ___UNWIND_HISTORY_TABLE_GLOBAL;
    sUnwindHistTbl.LowAddress = (DWORD64)-1;
    sCapturedCtx.Rip = sCtx.Rip;
    sCapturedCtx.Rsp = sCtx.Rsp;
    while (nCount > 0)
    {
      if ((sCapturedCtx.Rsp & 0x07) != 0)
        break; //unaligned
      if ((SIZE_T)(sCapturedCtx.Rsp) < nStackLimitLow || (SIZE_T)(sCapturedCtx.Rsp) >= nStackLimitHigh)
        break;
      lpFunc = (PRUNTIME_FUNCTION)::MxRtlLookupFunctionEntry(sCapturedCtx.Rip, &nImageBase, NULL);
      if (lpFunc == NULL)
      {
        //break;
        sCapturedCtx.Rip = *((ULONGLONG*)(sCapturedCtx.Rsp));
        sCapturedCtx.Rsp += 8;
      }
      else
      {
        ::MxRtlVirtualUnwind(___UNW_FLAG_NHANDLER, nImageBase, sCapturedCtx.Rip, lpFunc, &sCapturedCtx, &lpHandlerData,
                             &nEstablisherFrame, NULL);
        if ((SIZE_T)nEstablisherFrame < nStackLimitLow || (SIZE_T)nEstablisherFrame >= nStackLimitHigh)
          break;
      }
      if (sCapturedCtx.Rip == 0)
        break;
      if (nSkipCount > 0)
      {
        nSkipCount--;
      }
      else
      {
        sCapturedCtx.Rip = lpnOutput[nRetrieved] = (SIZE_T)(sCapturedCtx.Rip);
        temp8 = *((LPBYTE)lpnOutput[nRetrieved]);
        nRetrieved++;
      }
    }

#else
  #error Unsupported platform
#endif
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  { }

  //done
  if (nCount > 0)
    lpnOutput[nRetrieved++] = 0;
  return S_OK;
}

}; //namespace StackTrace

}; //namespace MX

//-----------------------------------------------------------

static VOID GetStackLimits(_In_ HANDLE hThread, _Out_ SIZE_T &nLow, _Out_ SIZE_T &nHigh)
{
  MX_THREAD_BASIC_INFORMATION sBi;
  NT_TIB *lpTIB;
  SIZE_T k;

  nLow = 0;
  nHigh = (SIZE_T)-1;
  if (hThread == NULL)
  {
#if defined _M_IX86
    lpTIB = (NT_TIB*)__readfsdword(0x18);
#elif defined _M_X64
    lpTIB = (NT_TIB*)__readgsqword(0x30);
#endif
  }
  else
  {
    //using documented(?) slow way
    lpTIB = NULL;
    //get TEB address
    if (::MxNtQueryInformationThread(hThread, MxThreadBasicInformation, &sBi, sizeof(sBi), NULL) >= 0)
      lpTIB = (NT_TIB*)(sBi.TebBaseAddress);
  }
  if (lpTIB != NULL)
  {
    __try
    {
      nHigh = (SIZE_T)(lpTIB->StackBase);
      nLow = (SIZE_T)(lpTIB->StackLimit);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      nLow = 0;
      nHigh = (SIZE_T)-1;
    }
  }
  if (nLow > nHigh)
  {
    k = nLow;  nLow = nHigh;  nHigh = k;
  }
  return;
}
