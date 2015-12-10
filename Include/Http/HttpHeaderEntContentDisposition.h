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

  HRESULT Parse(__in_z LPCSTR szValueA);

  HRESULT Build(__inout CStringA &cStrDestA);

  HRESULT SetType(__in_z LPCSTR szTypeA);
  LPCSTR GetType() const;

  HRESULT SetFileName(__in_z_opt LPCWSTR szFileNameW);
  LPCWSTR GetFileName() const; //NULL if no filename parameter

  HRESULT SetCreationDate(__in CDateTime &cDt);
  CDateTime GetCreationDate() const; //ticks=0 if no creation date parameter

  HRESULT SetModificationDate(__in CDateTime &cDt);
  CDateTime GetModificationDate() const; //ticks=0 if no modification date parameter

  HRESULT SetReadDate(__in CDateTime &cDt);
  CDateTime GetReadDate() const; //ticks=0 if no read date parameter

  HRESULT SetSize(__in ULONGLONG nSize);
  ULONGLONG GetSize() const; //ULONGLONG_MAX if size parameter

  HRESULT AddParam(__in_z LPCSTR szNameA, __in_z LPCWSTR szValueW);

  SIZE_T GetParamsCount() const;
  LPCSTR GetParamName(__in SIZE_T nIndex) const;
  LPCWSTR GetParamValue(__in SIZE_T nIndex) const;
  LPCWSTR GetParamValue(__in_z LPCSTR szNameA) const;

private:
  typedef struct {
    LPWSTR szValueW;
    CHAR szNameA[1];
  } PARAMETER, *LPPARAMETER;

  CStringA cStrTypeA;
  BOOL bHasFileName;
  CStringW cStrFileNameW;
  CDateTime cCreationDt;
  CDateTime cModificationDt;
  CDateTime cReadDt;
  ULONGLONG nSize;
  TArrayListWithFree<LPPARAMETER> cParamsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERENTITYCONTENTDISPOSITION_H
