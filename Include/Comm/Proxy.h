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
#ifndef _MX_PROXY_H
#define _MX_PROXY_H

#include "..\Defines.h"
#include "..\Strings\Strings.h"
#include "..\Http\Url.h"

//-----------------------------------------------------------

namespace MX {

class CProxy : public CBaseMemObj
{
public:
  typedef enum {
    TypeNone = 0,
    TypeUseIE,
    TypeManual
  } eType;

public:
  CProxy();
  CProxy(_In_ const CProxy& cSrc) throw(...);
  ~CProxy();

  CProxy& operator=(_In_ const CProxy& cSrc) throw(...);

  VOID SetDirect();
  HRESULT SetManual(_In_z_ LPCWSTR szProxyServerW);
  VOID SetUseIE();

  HRESULT Resolve(_In_opt_z_ LPCWSTR szTargetUrlW);
  HRESULT Resolve(_In_ CUrl &cUrl);

  eType GetType() const
    {
    return nType;
    };

  LPCWSTR GetAddress() const
    {
    return (LPCWSTR)cStrAddressW;
    };
  int GetPort() const
    {
    return nPort;
    };

private:
  eType nType;
  MX::CStringW cStrAddressW;
  int nPort;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_PROXY_H