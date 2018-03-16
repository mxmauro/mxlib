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
#ifndef _MX_HTTPHEADERRESPONSEETAG_H
#define _MX_HTTPHEADERRESPONSEETAG_H

#include "HttpHeaderBase.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderRespETag : public CHttpHeaderBase
{
public:
  CHttpHeaderRespETag();
  ~CHttpHeaderRespETag();

  MX_DECLARE_HTTPHEADER_NAME(ETag)

  HRESULT Parse(_In_z_ LPCSTR szValueA);

  HRESULT Build(_Inout_ CStringA &cStrDestA);

  HRESULT SetEntity(_In_z_ LPCSTR szEntityA);
  HRESULT SetEntity(_In_z_ LPCWSTR szEntityW);
  LPCWSTR GetEntity() const;

  HRESULT SetWeak(_In_ BOOL bIsWeak);
  BOOL GetWeak() const;

private:
  CStringW cStrEntityW;
  BOOL bIsWeak;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERRESPONSEETAG_H
