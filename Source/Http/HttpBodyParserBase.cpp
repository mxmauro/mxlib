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
#include "..\..\Include\Http\HttpBodyParserBase.h"

//-----------------------------------------------------------

#ifdef _DEBUG
  #define _SHARING_MODE FILE_SHARE_READ
#else //_DEBUG
  #define _SHARING_MODE 0
#endif //_DEBUG

//-----------------------------------------------------------

namespace MX {

CHttpBodyParserBase::CHttpBodyParserBase() : CBaseMemObj(), TRefCounted<CHttpBodyParserBase>()
{
  return;
}

CHttpBodyParserBase::~CHttpBodyParserBase()
{
  return;
}

} //namespace MX
