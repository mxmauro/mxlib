/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2019. All rights reserved.
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
  class CEncoding : public virtual CBaseMemObj
  {
  public:
    CEncoding();
    ~CEncoding();

    HRESULT SetEncoding(_In_z_ LPCSTR szEncodingA, _In_opt_ SIZE_T nEncodingLen = (SIZE_T)-1);
    LPCSTR GetEncoding() const;

    HRESULT SetQ(_In_ double q);
    double GetQ() const;

  private:
    CStringA cStrEncodingA;
    double q;
  };

  //----

public:
  CHttpHeaderReqAcceptEncoding();
  ~CHttpHeaderReqAcceptEncoding();

  MX_DECLARE_HTTPHEADER_NAME(Accept-Encoding)

  HRESULT Parse(_In_z_ LPCSTR szValueA);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser);

  eDuplicateBehavior GetDuplicateBehavior() const
    {
    return DuplicateBehaviorAppend;
    };

  HRESULT AddEncoding(_In_z_ LPCSTR szEncodingA, _In_opt_ SIZE_T nEncodingLen = (SIZE_T)-1,
                      _Out_opt_ CEncoding **lplpEncoding = NULL);

  SIZE_T GetEncodingsCount() const;
  CEncoding* GetEncoding(_In_ SIZE_T nIndex) const;
  CEncoding* GetEncoding(_In_z_ LPCSTR szEncodingA) const;

private:
  TArrayListWithDelete<CEncoding*> cEncodingsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERREQUESTACCEPTENCODING_H
