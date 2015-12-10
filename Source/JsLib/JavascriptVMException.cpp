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
#include "JavascriptVMCommon.h"

//-----------------------------------------------------------

namespace MX {

CJavascriptVM::CException::CException(__in int _nCode, __in LPCSTR _szMsgA)
{
  SIZE_T nLen;

  nCode = _nCode;
  nLen = MX::StrLenA(_szMsgA);
  if (nLen >= MX_ARRAYLEN(szMsgA))
    nLen = MX_ARRAYLEN(szMsgA) - 1;
  MemCopy(szMsgA, _szMsgA, nLen);
  szMsgA[nLen] = 0;
  return;
}

VOID CJavascriptVM::CException::DebugPrintMessage()
{
#ifdef _DEBUG
  if (szMsgA[0] != 0)
    MX::DebugPrint("%s\r\n", szMsgA);
#endif //_DEBUG
  return;
}

} //namespace MX
