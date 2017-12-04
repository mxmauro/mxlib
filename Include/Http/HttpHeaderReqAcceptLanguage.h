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
#ifndef _MX_HTTPHEADERREQUESTACCEPTLANGUAGE_H
#define _MX_HTTPHEADERREQUESTACCEPTLANGUAGE_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderReqAcceptLanguage : public CHttpHeaderBase
{
public:
  class CLanguage : public virtual CBaseMemObj
  {
  public:
    CLanguage();
    ~CLanguage();

    HRESULT SetLanguage(__in_z LPCSTR szLanguageA);
    LPCSTR GetLanguage() const;

    HRESULT SetQ(__in double q);
    double GetQ() const;

  private:
    CStringA cStrLanguageA;
    double q;
  };

  //----

public:
  CHttpHeaderReqAcceptLanguage();
  ~CHttpHeaderReqAcceptLanguage();

  MX_DECLARE_HTTPHEADER_NAME(Accept-Language)

  HRESULT Parse(__in_z LPCSTR szValueA);

  HRESULT Build(__inout CStringA &cStrDestA);

  eDuplicateBehavior GetDuplicateBehavior() const
    {
    return DuplicateBehaviorAppend;
    };

  HRESULT AddLanguage(__in_z LPCSTR szLanguageA, __out_opt CLanguage **lplpLanguage=NULL);

  SIZE_T GetLanguagesCount() const;
  CLanguage* GetLanguage(__in SIZE_T nIndex) const;
  CLanguage* GetLanguage(__in_z LPCSTR szLanguageA) const;

private:
  TArrayListWithDelete<CLanguage*> cLanguagesList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERREQUESTACCEPTLANGUAGE_H
