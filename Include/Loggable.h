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
#ifndef _MX_LOGGABLE_H
#define _MX_LOGGABLE_H

#include "Callbacks.h"

//-----------------------------------------------------------

namespace MX {

class CLoggable : public virtual CBaseMemObj
{
public:
  typedef Callback <HRESULT (_In_z_ LPCWSTR szInfoW)> OnLogCallback;
public:
  CLoggable();
  CLoggable(_In_ OnLogCallback cCallback);

  CLoggable& operator=(_In_ const CLoggable &cSrc);

  VOID SetLogParent(_In_opt_ CLoggable *lpParentLog);
  VOID SetLogLevel(_In_ DWORD dwLevel);
  VOID SetLogCallback(_In_ OnLogCallback cCallback);

  __inline BOOL ShouldLog(_In_ DWORD dwRequiredLevel) const
    {
    return (GetRoot()->dwLevel >= dwRequiredLevel) ? TRUE : FALSE;
    };

  HRESULT Log(_Printf_format_string_ LPCWSTR szFormatW, ...);
  HRESULT LogIfError(_In_ HRESULT hResError, _Printf_format_string_ LPCWSTR szFormatW, ...);
  HRESULT LogAlways(_In_ HRESULT hResError, _Printf_format_string_ LPCWSTR szFormatW, ...);

private:
  CLoggable* GetRoot() const;

  HRESULT WriteLogCommon(_In_ BOOL bAddError, _In_ HRESULT hResError, _In_z_ LPCWSTR szFormatW, _In_ va_list argptr);

private:
  CLoggable *lpParentLog;
  DWORD dwLevel;
  OnLogCallback cCallback;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_LOGGABLE_H
