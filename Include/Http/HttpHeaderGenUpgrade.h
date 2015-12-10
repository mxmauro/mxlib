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
#ifndef _MX_HTTPHEADERGENERALUPGRADE_H
#define _MX_HTTPHEADERGENERALUPGRADE_H

#include "HttpHeaderBase.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderGenUpgrade : public CHttpHeaderBase
{
public:
  CHttpHeaderGenUpgrade();
  ~CHttpHeaderGenUpgrade();

  MX_DECLARE_HTTPHEADER_NAME(Upgrade)

  HRESULT Parse(__in_z LPCSTR szValueA);

  HRESULT Build(__inout CStringA &cStrDestA);

  HRESULT AddProduct(__in_z LPCSTR szProductA);

  SIZE_T GetProductsCount() const;
  LPCSTR GetProduct(__in SIZE_T nIndex) const;
  BOOL HasProduct(__in_z LPCSTR szProductA) const;

private:
  TArrayListWithFree<LPSTR> cProductsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERGENERALUPGRADE_H
