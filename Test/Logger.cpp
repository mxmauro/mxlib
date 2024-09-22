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
