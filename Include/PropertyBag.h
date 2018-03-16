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

  HRESULT Clear(_In_z_ LPCSTR szNameA);

  HRESULT SetNull(_In_z_ LPCSTR szNameA);
  HRESULT SetDWord(_In_z_ LPCSTR szNameA, _In_ DWORD dwValue);
  HRESULT SetQWord(_In_z_ LPCSTR szNameA, _In_ ULONGLONG ullValue);
  HRESULT SetDouble(_In_z_ LPCSTR szNameA, _In_ double nValue);
  HRESULT SetString(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA);
  HRESULT SetString(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW);

  LPCSTR GetAt(_In_ SIZE_T nIndex) const;

  eType GetType(_In_ SIZE_T nIndex) const;
  eType GetType(_In_z_ LPCSTR szNameA) const;

  HRESULT GetDWord(_In_ SIZE_T nIndex, _Out_ DWORD &dwValue, _In_opt_ DWORD dwDefValue=0);
  HRESULT GetQWord(_In_ SIZE_T nIndex, _Out_ ULONGLONG &ullValue, _In_opt_ ULONGLONG ullDefValue=0);
  HRESULT GetDouble(_In_ SIZE_T nIndex, _Out_ double &nValue, _In_opt_ double nDefValue=0.0);
  HRESULT GetString(_In_ SIZE_T nIndex, _Out_ LPCSTR &szValueA, _In_opt_z_ LPCSTR szDefValueA=NULL);
  HRESULT GetString(_In_ SIZE_T nIndex, _Out_ LPCWSTR &szValueW, _In_opt_z_ LPCWSTR szDefValueW=NULL);

  HRESULT GetDWord(_In_z_ LPCSTR szNameA, _Out_ DWORD &dwValue, _In_opt_ DWORD dwDefValue=0);
  HRESULT GetQWord(_In_z_ LPCSTR szNameA, _Out_ ULONGLONG &ullValue, _In_opt_ ULONGLONG ullDefValue=0);
  HRESULT GetDouble(_In_z_ LPCSTR szNameA, _Out_ double &nValue, _In_opt_ double nDefValue=0.0);
  HRESULT GetString(_In_z_ LPCSTR szNameA, _Out_ LPCSTR &szValueA, _In_opt_z_ LPCSTR szDefValueA=NULL);
  HRESULT GetString(_In_z_ LPCSTR szNameA, _Out_ LPCWSTR &szValueW, _In_opt_z_ LPCWSTR szDefValueW=NULL);

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
  BOOL Insert(_In_ PROPERTY *lpNewProp);
  SIZE_T Find(_In_z_ LPCSTR szNameA);

  static int SearchCompareFunc(_In_ LPVOID lpContext, _In_ PROPERTY **lpItem1, _In_ PROPERTY **lpItem2);

private:
  TArrayListWithFree<PROPERTY*> cPropertiesList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_PROPERTYBAG_H
