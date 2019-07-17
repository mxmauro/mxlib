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
#include "Logger.h"
#include "Test.h"
#include <Finalizer.h>

//-----------------------------------------------------------

static LONG volatile nLogLock = 0;
static BOOL bShutdownRegistered = FALSE;
static FILE *fp = NULL;

//-----------------------------------------------------------

static VOID ShutdownLogger();

//-----------------------------------------------------------

namespace Logger {

VOID Log(_In_z_ LPCWSTR szFormatW, ...)
{
  MX::CFastLock cLock(&nLogLock);
  va_list args;

  if (fp == NULL)
  {
    MX::CStringW cStrFileNameW;

    if (bShutdownRegistered == FALSE)
    {
      if (FAILED(MX::RegisterFinalizer(&ShutdownLogger, 10)))
        return;
      bShutdownRegistered = TRUE;
    }

    if (FAILED(GetAppPath(cStrFileNameW)))
      return;
    if (cStrFileNameW.ConcatN(L"Logs", 4) == FALSE)
      return;
    ::CreateDirectoryW((LPCWSTR)cStrFileNameW, NULL);
    if (cStrFileNameW.ConcatN(L"\\output.log", 11) == FALSE)
      return;
    fp = _wfsopen((LPCWSTR)cStrFileNameW, L"a+", _SH_DENYWR);
    if (fp == NULL)
      return;
  }
  va_start(args, szFormatW);
  vfwprintf_s(fp, szFormatW, args);
  va_end(args);
}

}; //namespace Logger

//-----------------------------------------------------------

static VOID ShutdownLogger()
{
  if (fp != NULL)
    fclose(fp);
  return;
}
