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
#ifndef _MX_HTTPHEADERREQUESTACCEPTCHARSET_H
#define _MX_HTTPHEADERREQUESTACCEPTCHARSET_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderReqAcceptCharset : public CHttpHeaderBase
{
public:
  class CType : public virtual CBaseMemObj
  {
  public:
    CType();
    ~CType();

    HRESULT SetType(_In_z_ LPCSTR szTypeA);
    LPCSTR GetType() const;

    HRESULT SetQ(_In_ double q);
    double GetQ() const;

  private:
    CStringA cStrTypeA;
    double q;
  };

  //----

public:
  CHttpHeaderReqAcceptCharset();
  ~CHttpHeaderReqAcceptCharset();

  MX_DECLARE_HTTPHEADER_NAME(Accept-Charset)

  HRESULT Parse(_In_z_ LPCSTR szValueA);

  HRESULT Build(_Inout_ CStringA &cStrDestA);

  eDuplicateBehavior GetDuplicateBehavior() const
    {
    return DuplicateBehaviorAppend;
    };

  HRESULT AddType(_In_z_ LPCSTR szTypeA, _Out_opt_ CType **lplpType=NULL);

  SIZE_T GetTypesCount() const;
  CType* GetType(_In_ SIZE_T nIndex) const;
  CType* GetType(_In_z_ LPCSTR szTypeA) const;

private:
  TArrayListWithDelete<CType*> cTypesList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERREQUESTACCEPTCHARSET_H
