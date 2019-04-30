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
#ifndef _MX_HTTPHEADERREQUESTACCEPT_H
#define _MX_HTTPHEADERREQUESTACCEPT_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderReqAccept : public CHttpHeaderBase
{
public:
  class CType : public virtual CBaseMemObj
  {
  public:
    CType();
    ~CType();

    HRESULT SetType(_In_z_ LPCSTR szTypeA, _In_ SIZE_T nTypeLen = (SIZE_T)-1);
    LPCSTR GetType() const;

    HRESULT SetQ(_In_ double q);
    double GetQ() const;

    HRESULT AddParam(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW);

    SIZE_T GetParamsCount() const;
    LPCSTR GetParamName(_In_ SIZE_T nIndex) const;
    LPCWSTR GetParamValue(_In_ SIZE_T nIndex) const;
    LPCWSTR GetParamValue(_In_z_ LPCSTR szNameA) const;

  private:
    typedef struct {
      LPWSTR szValueW;
      CHAR szNameA[1];
    } PARAMETER, *LPPARAMETER;

    CStringA cStrTypeA;
    double q;
    TArrayListWithFree<LPPARAMETER> cParamsList;
  };

  //----

public:
  CHttpHeaderReqAccept();
  ~CHttpHeaderReqAccept();

  MX_DECLARE_HTTPHEADER_NAME(Accept)

  HRESULT Parse(_In_z_ LPCSTR szValueA);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser);

  eDuplicateBehavior GetDuplicateBehavior() const
    {
    return DuplicateBehaviorAppend;
    };

  HRESULT AddType(_In_z_ LPCSTR szTypeA, _In_opt_ SIZE_T nTypeLen = (SIZE_T)-1, _Out_opt_ CType **lplpType = NULL);

  SIZE_T GetTypesCount() const;
  CType* GetType(_In_ SIZE_T nIndex) const;
  CType* GetType(_In_z_ LPCSTR szTypeA) const;

private:
  TArrayListWithDelete<CType*> cTypesList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERREQUESTACCEPT_H
