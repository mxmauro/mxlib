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
#ifndef _MX_HTTPAUTHENTICATERESPONSEPARSER_H
#define _MX_HTTPAUTHENTICATERESPONSEPARSER_H

#include "..\Defines.h"
#include "..\Callbacks.h"
#include "..\ArrayList.h"
#include "..\Strings\Strings.h"
//#include "..\Comm\Sockets.h"
//#include "HttpCommon.h"

//-----------------------------------------------------------

namespace MX {

class CHttpAuthenticateResponseParser : public virtual CBaseMemObj
{
public:
  typedef enum {
    ClassUnknown=0, ClassWWW, ClassProxy
  } eClass;

  typedef enum {
    TypeUnknown=0, TypeBasic, TypeDigest
  } eType;

  CHttpAuthenticateResponseParser();
  ~CHttpAuthenticateResponseParser();

  VOID Reset();
  HRESULT Parse(_In_z_ LPCSTR szResponseA);

  eClass GetClass() const
    {
    return nClass;
    };
  eType GetType() const
    {
    return nType;
    };
  LPCSTR GetValue(_In_z_ LPCSTR szKeyA) const;

  HRESULT MakeAutoAuthorization(_Out_ CStringA &cStrDestA, _In_z_ LPCSTR szUserNameA, _In_z_ LPCSTR szPasswordA);

  HRESULT MakeBasicAuthorization(_Out_ CStringA &cStrDestA, _In_z_ LPCSTR szUserNameA, _In_z_ LPCSTR szPasswordA,
                                 _In_ BOOL bIsProxy);
  HRESULT MakeDigestAuthorization(_Out_ CStringA &cStrDestA, _In_z_ LPCSTR szUserNameA, _In_z_ LPCSTR szRealmA,
                                  _In_z_ LPCSTR szPasswordA, _In_z_ LPCSTR szAlgorithmA, _In_z_ LPCSTR szMethodA,
                                  _In_z_ LPCSTR szNonceA, _In_ SIZE_T nNonceCount, _In_z_ LPCSTR szQopA,
                                  _In_z_ LPCSTR szDigestUriA, _In_ LPVOID lpBodyMD5Checksum, _In_z_ LPCSTR szOpaqueA,
                                  _In_ BOOL bIsProxy);

private:
  typedef struct {
    LPSTR szValueA;
    CHAR szNameA[1];
  } TOKEN_VALUE, *LPTOKEN_VALUE;

  eClass nClass;
  eType nType;
  TArrayListWithFree<LPTOKEN_VALUE> cTokenValuesList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPAUTHENTICATERESPONSEPARSER_H
