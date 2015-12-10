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
#ifndef _MX_HTTPHEADERREQUESTACCEPTENCODING_H
#define _MX_HTTPHEADERREQUESTACCEPTENCODING_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderReqAcceptEncoding : public CHttpHeaderBase
{
public:
  class CType : public virtual CBaseMemObj
  {
  public:
    CType();
    ~CType();

    HRESULT SetType(__in_z LPCSTR szTypeA);
    LPCSTR GetType() const;

    HRESULT SetQ(__in double q);
    double GetQ() const;

  private:
    CStringA cStrTypeA;
    double q;
  };

  //----

public:
  CHttpHeaderReqAcceptEncoding();
  ~CHttpHeaderReqAcceptEncoding();

  MX_DECLARE_HTTPHEADER_NAME(Accept-Encoding)

  HRESULT Parse(__in_z LPCSTR szValueA);

  HRESULT Build(__inout CStringA &cStrDestA);

  HRESULT AddType(__in_z LPCSTR szTypeA, __out_opt CType **lplpType=NULL);

  SIZE_T GetTypesCount() const;
  CType* GetType(__in SIZE_T nIndex) const;
  CType* GetType(__in_z LPCSTR szTypeA) const;

private:
  TArrayListWithDelete<CType*> cTypesList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERREQUESTACCEPTENCODING_H
