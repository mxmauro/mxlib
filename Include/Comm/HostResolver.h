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
#ifndef _MX_HOSTRESOLVER_H
#define _MX_HOSTRESOLVER_H

#include "..\Defines.h"
#include "..\EventNotifyBase.h"
#if (!defined(_WS2DEF_)) && (!defined(_WINSOCKAPI_))
  #include <WS2tcpip.h>
#endif

//-----------------------------------------------------------

namespace MX {

class CHostResolver : public TEventNotifyBase<CHostResolver>
{
  MX_DISABLE_COPY_CONSTRUCTOR(CHostResolver);
public:
  CHostResolver(__in OnNotifyCallback cCallback, __in_opt LPVOID lpUserData=NULL);
  ~CHostResolver();

  HRESULT Resolve(__in_z LPCSTR szHostNameA, __in int nDesiredFamily);
  HRESULT Resolve(__in_z LPCWSTR szHostNameW, __in int nDesiredFamily);

  HRESULT ResolveAsync(__in_z LPCSTR szHostNameA, __in int nDesiredFamily, __in DWORD dwTimeoutMs);
  HRESULT ResolveAsync(__in_z LPCWSTR szHostNameW, __in int nDesiredFamily, __in DWORD dwTimeoutMs);
  VOID Cancel();

  sockaddr GetResolvedAddr() const
    {
    return sAddr;
    };

  HRESULT GetErrorCode() const
    {
    return hErrorCode;
    };

  static BOOL IsValidIPV4(__in_z LPCSTR szAddressA, __in_opt SIZE_T nAddressLen=(SIZE_T)-1,
                          __out_opt sockaddr *lpAddress=NULL);
  static BOOL IsValidIPV4(__in_z LPCWSTR szAddressW, __in_opt SIZE_T nAddressLen=(SIZE_T)-1,
                          __out_opt sockaddr *lpAddress=NULL);
  static BOOL IsValidIPV6(__in_z LPCSTR szAddressA, __in_opt SIZE_T nAddressLen=(SIZE_T)-1,
                          __out_opt sockaddr *lpAddress=NULL);
  static BOOL IsValidIPV6(__in_z LPCWSTR szAddressW, __in_opt SIZE_T nAddressLen=(SIZE_T)-1,
                          __out_opt sockaddr *lpAddress=NULL);

private:
  LPVOID lpInternal;
  sockaddr sAddr;
  OnNotifyCallback cCallback;
  LPVOID lpUserData;
  HRESULT hErrorCode;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HOSTRESOLVER_H
