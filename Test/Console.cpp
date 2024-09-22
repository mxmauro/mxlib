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
#include "Console.h"
#include <WaitableObjects.h>

//-----------------------------------------------------------

static LONG volatile nPrintLock = 0;
static LONG volatile nMustQuit = 0;

//-----------------------------------------------------------

static BOOL WINAPI _ConsoleHandlerRoutine(_In_ DWORD dwCtrlType);

//-----------------------------------------------------------

namespace Console {

BOOL Initialize()
{
  if (::SetConsoleCtrlHandler(_ConsoleHandlerRoutine, TRUE) == FALSE)
    return FALSE;
  return TRUE;
}

BOOL MustQuit()
{
  return (__InterlockedRead(&nMustQuit) != 0) ? TRUE : FALSE;
}

VOID SetCursorPosition(_In_ int X, _In_ int Y)
{
  COORD sCursorPos;

  sCursorPos.X = (short)X;
  sCursorPos.Y = (short)Y;
  ::SetConsoleCursorPosition(::GetStdHandle(STD_OUTPUT_HANDLE), sCursorPos);
  return;
}

VOID GetCursorPosition(_Out_ int *lpnX, _Out_ int *lpnY)
{
  CONSOLE_SCREEN_BUFFER_INFO sCsbi;

  ::GetConsoleScreenBufferInfo(::GetStdHandle(STD_OUTPUT_HANDLE), &sCsbi);
  *lpnX = (int)(sCsbi.dwCursorPosition.X);
  *lpnY = (int)(sCsbi.dwCursorPosition.Y);
  return;
}

VOID PrintTimestamp()
{
  SYSTEMTIME sSi;

  ::GetLocalTime(&sSi);
  wprintf_s(L"%02u:%02u:%02u.%03u", (unsigned int)sSi.wHour, (unsigned int)sSi.wMinute, (unsigned int)sSi.wSecond,
            (unsigned int)sSi.wMilliseconds);
  return;
}

//-----------------------------------------------------------

CPrintLock::CPrintLock()
{
  MX::FastLock_Enter(&nPrintLock);
  return;
}

CPrintLock::~CPrintLock()
{
  MX::FastLock_Exit(&nPrintLock);
  return;
}

}; //namespace Console

//-----------------------------------------------------------

static BOOL WINAPI _ConsoleHandlerRoutine(_In_ DWORD dwCtrlType)
{
  switch (dwCtrlType)
  {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
      _InterlockedExchange(&nMustQuit, 1);
      return TRUE;
  }
  return FALSE;
}
