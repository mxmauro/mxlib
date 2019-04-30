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
#ifndef _MX_HTTPHEADERENTITYCONTENTDISPOSITION_H
#define _MX_HTTPHEADERENTITYCONTENTDISPOSITION_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"
#include "..\DateTime\DateTime.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderEntContentDisposition : public CHttpHeaderBase
{
public:
  CHttpHeaderEntContentDisposition();
  ~CHttpHeaderEntContentDisposition();

  MX_DECLARE_HTTPHEADER_NAME(Content-Disposition)

  HRESULT Parse(_In_z_ LPCSTR szValueA);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser);

  HRESULT SetType(_In_z_ LPCSTR szTypeA, _In_ SIZE_T nTypeLen = (SIZE_T)-1);
  LPCSTR GetType() const;

  HRESULT SetName(_In_opt_z_ LPCWSTR szNameW, _In_ SIZE_T nNameLen = (SIZE_T)-1);
  LPCWSTR GetName() const;

  HRESULT SetFileName(_In_opt_z_ LPCWSTR szFileNameW, _In_ SIZE_T nFileNameLen = (SIZE_T)-1);
  LPCWSTR GetFileName() const;
  BOOL HasFileName() const;

  HRESULT SetCreationDate(_In_ CDateTime &cDt);
  CDateTime GetCreationDate() const; //ticks=0 if no creation date parameter

  HRESULT SetModificationDate(_In_ CDateTime &cDt);
  CDateTime GetModificationDate() const; //ticks=0 if no modification date parameter

  HRESULT SetReadDate(_In_ CDateTime &cDt);
  CDateTime GetReadDate() const; //ticks=0 if no read date parameter

  HRESULT SetSize(_In_ ULONGLONG nSize);
  ULONGLONG GetSize() const; //ULONGLONG_MAX if size parameter

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
  CStringW cStrNameW;
  CStringW cStrFileNameW;
  BOOL bHasFileName;
  CDateTime cCreationDt;
  CDateTime cModificationDt;
  CDateTime cReadDt;
  ULONGLONG nSize;
  TArrayListWithFree<LPPARAMETER> cParamsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERENTITYCONTENTDISPOSITION_H
