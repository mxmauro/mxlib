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
#ifndef _MX_PROPERTYBAG_H
#define _MX_PROPERTYBAG_H

#include "Defines.h"
#include "ArrayList.h"
#include "Strings\Strings.h"

//-----------------------------------------------------------

namespace MX {

class CPropertyBag : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CPropertyBag);
public:
  typedef enum {
    PropertyTypeUndefined,
    PropertyTypeNull,
    PropertyTypeDWord,
    PropertyTypeQWord,
    PropertyTypeAnsiString,
    PropertyTypeWideString,
    PropertyTypeDouble
  } eType;

  CPropertyBag();
  ~CPropertyBag();

  VOID Reset();

  HRESULT Clear(__in_z LPCSTR szNameA);

  HRESULT SetNull(__in_z LPCSTR szNameA);
  HRESULT SetDWord(__in_z LPCSTR szNameA, __in DWORD dwValue);
  HRESULT SetQWord(__in_z LPCSTR szNameA, __in ULONGLONG ullValue);
  HRESULT SetDouble(__in_z LPCSTR szNameA, __in double nValue);
  HRESULT SetString(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA);
  HRESULT SetString(__in_z LPCSTR szNameA, __in_z LPCWSTR szValueW);

  LPCSTR GetAt(__in SIZE_T nIndex) const;

  eType GetType(__in SIZE_T nIndex) const;
  eType GetType(__in_z LPCSTR szNameA) const;

  HRESULT GetDWord(__in SIZE_T nIndex, __out DWORD &dwValue, __in_opt DWORD dwDefValue=0);
  HRESULT GetQWord(__in SIZE_T nIndex, __out ULONGLONG &ullValue, __in_opt ULONGLONG ullDefValue=0);
  HRESULT GetDouble(__in SIZE_T nIndex, __out double &nValue, __in_opt double nDefValue=0.0);
  HRESULT GetString(__in SIZE_T nIndex, __out LPCSTR &szValueA, __in_z_opt LPCSTR szDefValueA=NULL);
  HRESULT GetString(__in SIZE_T nIndex, __out LPCWSTR &szValueW, __in_z_opt LPCWSTR szDefValueW=NULL);

  HRESULT GetDWord(__in_z LPCSTR szNameA, __out DWORD &dwValue, __in_opt DWORD dwDefValue=0);
  HRESULT GetQWord(__in_z LPCSTR szNameA, __out ULONGLONG &ullValue, __in_opt ULONGLONG ullDefValue=0);
  HRESULT GetDouble(__in_z LPCSTR szNameA, __out double &nValue, __in_opt double nDefValue=0.0);
  HRESULT GetString(__in_z LPCSTR szNameA, __out LPCSTR &szValueA, __in_z_opt LPCSTR szDefValueA=NULL);
  HRESULT GetString(__in_z LPCSTR szNameA, __out LPCWSTR &szValueW, __in_z_opt LPCWSTR szDefValueW=NULL);

private:
  typedef struct {
    eType nType;
    LPSTR szNameA;
    union {
      DWORD dwValue;
      ULONGLONG ullValue;
      double dblValue;
      LPSTR szValueA;
      LPWSTR szValueW;
    } u;
  } PROPERTY;

private:
  BOOL Insert(__in PROPERTY *lpNewProp);
  SIZE_T Find(__in_z LPCSTR szNameA);

  static int SearchCompareFunc(__in LPVOID lpContext, __in PROPERTY **lpItem1, __in PROPERTY **lpItem2);

private:
  TArrayListWithFree<PROPERTY*> cPropertiesList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_PROPERTYBAG_H
