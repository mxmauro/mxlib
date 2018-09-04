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

#define MX_E_AlreadyExists              HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)
#define MX_E_PartialCopy                HRESULT_FROM_WIN32(ERROR_PARTIAL_COPY)
#define MX_E_Timeout                    HRESULT_FROM_WIN32(WAIT_TIMEOUT)
#define MX_E_Unsupported                HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)
#define MX_E_NotFound                   HRESULT_FROM_WIN32(ERROR_NOT_FOUND)
#define MX_E_ArithmeticOverflow         HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW)
#define MX_E_AlreadyInitialized         HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED)
#define MX_E_Cancelled                  HRESULT_FROM_WIN32(ERROR_CANCELLED)
#define MX_E_ExceptionRaised            DISP_E_EXCEPTION
#define MX_E_InvalidData                HRESULT_FROM_WIN32(ERROR_INVALID_DATA)
#define MX_E_IoPending                  HRESULT_FROM_WIN32(ERROR_IO_PENDING)
#define MX_E_ReadFault                  HRESULT_FROM_WIN32(ERROR_READ_FAULT)
#define MX_E_WriteFault                 HRESULT_FROM_WIN32(ERROR_WRITE_FAULT)
#define MX_E_UnhandledException         HRESULT_FROM_WIN32(ERROR_UNHANDLED_EXCEPTION)
#define MX_E_PipeNotConnected           HRESULT_FROM_WIN32(ERROR_PIPE_NOT_CONNECTED)
#define MX_E_BrokenPipe                 HRESULT_FROM_WIN32(ERROR_BROKEN_PIPE)
#define MX_E_BufferOverflow             HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW)
#define MX_E_FileNotFound               HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)
#define MX_E_PathNotFound               HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)
#define MX_E_PrivilegeNotHeld           HRESULT_FROM_WIN32(ERROR_PRIVILEGE_NOT_HELD)
#define MX_E_MoreData                   HRESULT_FROM_WIN32(ERROR_MORE_DATA)
#define MX_E_ProcNotFound               HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND)
#define MX_E_NotReady                   HRESULT_FROM_WIN32(ERROR_NOT_READY)
#define MX_E_Busy                       HRESULT_FROM_WIN32(ERROR_BUSY)
#define MX_E_BadLength                  HRESULT_FROM_WIN32(ERROR_BAD_LENGTH)
#define MX_E_EndOfFileReached           HRESULT_FROM_WIN32(ERROR_HANDLE_EOF)
#define MX_E_InvalidState               HRESULT_FROM_WIN32(ERROR_INVALID_STATE)

#define MX_SCODE_FACILITY               0xF18
#define MX_E_DuplicateKey               MX_E_AlreadyExists
#define MX_E_ConstraintsCheckFailed     MAKE_HRESULT(1, MX_SCODE_FACILITY, 1) //0x8F180001

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
  return MX_HRESULT_FROM_WIN32(::MxGetLastWin32Error());
}

#ifdef __cplusplus
}; //extern "C"
#endif //__cplusplus

//-----------------------------------------------------------

#endif //_MX_DEFINITIONS_H
