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
#include "Console.h"

//-----------------------------------------------------------

static LONG volatile nPrintLock = 0;

//-----------------------------------------------------------

namespace Console {

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
  while (_InterlockedCompareExchange(&nPrintLock, 1, 0) != 0)
    MX::_YieldProcessor();
  return;
}

CPrintLock::~CPrintLock()
{
  _InterlockedExchange(&nPrintLock, 0);
  return;
}

}; //namespace Console
