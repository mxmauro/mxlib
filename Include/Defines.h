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
#ifndef _MX_DEFINITIONS_H
#define _MX_DEFINITIONS_H

#ifndef WINVER
  #define WINVER 0x0502
#endif //!WINVER
#ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0502
#endif //!_WIN32_WINNT
#ifndef _WIN32_WINDOWS
  #define _WIN32_WINDOWS 0x0502
#endif //!_WIN32_WINDOWS
#ifndef _WIN32_IE
  #define _WIN32_IE 0x0700
#endif //!_WIN32_IE
#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif //!WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "NtDefs.h"
#include "MemoryObjects.h"

//-----------------------------------------------------------

#ifdef _DEBUG
  #define MX_ASSERT(x) if (!(x)) __debugbreak();
#else
  #define MX_ASSERT(x)
#endif //_DEBUG
#define MX_ASSERT_ALWAYS(x) { if (!(x)) __debugbreak(); }

#define MX_DELETE(x)                                       \
                if ((x) != NULL) { delete (x); (x) = NULL; }
#define MX_RELEASE(x)                                      \
            if ((x) != NULL) { (x)->Release(); (x) = NULL; }

#if defined(_M_X64) || defined(_M_IA64) || defined(_M_AMD64)
  #define MX_UNALIGNED __unaligned
#else
  #define MX_UNALIGNED
#endif

#define MX_ARRAYLEN(arr)      (sizeof(arr) / sizeof(arr[0]))

//-----------------------------------------------------------

#define MX_E_AlreadyExists              MAKE_HRESULT(1, FACILITY_WIN32, ERROR_ALREADY_EXISTS)
#define MX_E_PartialCopy                MAKE_HRESULT(1, FACILITY_WIN32, ERROR_PARTIAL_COPY)
#define MX_E_Timeout                    MAKE_HRESULT(1, FACILITY_WIN32, WAIT_TIMEOUT)
#define MX_E_Unsupported                MAKE_HRESULT(1, FACILITY_WIN32, ERROR_NOT_SUPPORTED)
#define MX_E_NotFound                   MAKE_HRESULT(1, FACILITY_WIN32, ERROR_NOT_FOUND)
#define MX_E_ArithmeticOverflow         MAKE_HRESULT(1, FACILITY_WIN32, ERROR_ARITHMETIC_OVERFLOW)
#define MX_E_AlreadyInitialized         MAKE_HRESULT(1, FACILITY_WIN32, ERROR_ALREADY_INITIALIZED)
#define MX_E_Cancelled                  MAKE_HRESULT(1, FACILITY_WIN32, ERROR_CANCELLED)
#define MX_E_ExceptionRaised            DISP_E_EXCEPTION
#define MX_E_InvalidData                MAKE_HRESULT(1, FACILITY_WIN32, ERROR_INVALID_DATA)
#define MX_E_IoPending                  MAKE_HRESULT(1, FACILITY_WIN32, ERROR_IO_PENDING)
#define MX_E_ReadFault                  MAKE_HRESULT(1, FACILITY_WIN32, ERROR_READ_FAULT)
#define MX_E_WriteFault                 MAKE_HRESULT(1, FACILITY_WIN32, ERROR_WRITE_FAULT)
#define MX_E_UnhandledException         MAKE_HRESULT(1, FACILITY_WIN32, ERROR_UNHANDLED_EXCEPTION)
#define MX_E_PipeNotConnected           MAKE_HRESULT(1, FACILITY_WIN32, ERROR_PIPE_NOT_CONNECTED)
#define MX_E_BrokenPipe                 MAKE_HRESULT(1, FACILITY_WIN32, ERROR_BROKEN_PIPE)
#define MX_E_BufferOverflow             MAKE_HRESULT(1, FACILITY_WIN32, ERROR_BUFFER_OVERFLOW)
#define MX_E_FileNotFound               MAKE_HRESULT(1, FACILITY_WIN32, ERROR_FILE_NOT_FOUND)
#define MX_E_PathNotFound               MAKE_HRESULT(1, FACILITY_WIN32, ERROR_PATH_NOT_FOUND)
#define MX_E_PrivilegeNotHeld           MAKE_HRESULT(1, FACILITY_WIN32, ERROR_PRIVILEGE_NOT_HELD)
#define MX_E_MoreData                   MAKE_HRESULT(1, FACILITY_WIN32, ERROR_MORE_DATA)
#define MX_E_ProcNotFound               MAKE_HRESULT(1, FACILITY_WIN32, ERROR_PROC_NOT_FOUND)
#define MX_E_NotReady                   MAKE_HRESULT(1, FACILITY_WIN32, ERROR_NOT_READY)
#define MX_E_Busy                       MAKE_HRESULT(1, FACILITY_WIN32, ERROR_BUSY)
#define MX_E_BadLength                  MAKE_HRESULT(1, FACILITY_WIN32, ERROR_BAD_LENGTH)
#define MX_E_EndOfFileReached           MAKE_HRESULT(1, FACILITY_WIN32, ERROR_HANDLE_EOF)
#define MX_E_InvalidState               MAKE_HRESULT(1, FACILITY_WIN32, ERROR_INVALID_STATE)
#define MX_E_OperationInProgress        MAKE_HRESULT(1, FACILITY_WIN32, ERROR_OPERATION_IN_PROGRESS)
#define MX_E_OperationAborted           MAKE_HRESULT(1, FACILITY_WIN32, ERROR_OPERATION_ABORTED)
#define MX_E_NoData                     MAKE_HRESULT(1, FACILITY_WIN32, ERROR_NO_DATA)

#define MX_SCODE_FACILITY               0xF18
#define MX_E_DuplicateKey               MX_E_AlreadyExists
#define MX_E_ConstraintsCheckFailed     MAKE_HRESULT(1, MX_SCODE_FACILITY, 1) //0x8F180001
#define MX_E_ArithmeticUnderflow        MAKE_HRESULT(1, MX_SCODE_FACILITY, 2) //0x8F180002

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

__inline HRESULT MX_HRESULT_FROM_WIN32(_In_ DWORD dwOsErr)
{
  if (dwOsErr == ERROR_NOT_ENOUGH_MEMORY)
    dwOsErr = ERROR_OUTOFMEMORY;
  return HRESULT_FROM_WIN32(dwOsErr);
}

static HRESULT MX_HRESULT_FROM_NT(_In_ NTSTATUS nNtStatus)
{
  switch (nNtStatus)
  {
    case STATUS_NO_MEMORY:
    case STATUS_INSUFFICIENT_RESOURCES:
      return E_OUTOFMEMORY;
    case STATUS_CANCELLED:
      return MX_E_Cancelled;
    case STATUS_NOT_FOUND:
      return MX_E_NotFound;
    case STATUS_NOT_IMPLEMENTED:
      return E_NOTIMPL;
  }
  return NT_SUCCESS(nNtStatus) ? (HRESULT)nNtStatus : HRESULT_FROM_NT(nNtStatus);
}

__inline HRESULT MX_HRESULT_FROM_LASTERROR()
{
  return MX_HRESULT_FROM_WIN32(MxGetLastWin32Error());
}

#ifdef __cplusplus
}; //extern "C"
#endif //__cplusplus

//-----------------------------------------------------------

#endif //_MX_DEFINITIONS_H
