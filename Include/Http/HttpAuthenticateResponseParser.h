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
  HRESULT Parse(__in_z LPCSTR szResponseA);

  eClass GetClass() const
    {
    return nClass;
    };
  eType GetType() const
    {
    return nType;
    };
  LPCSTR GetValue(__in_z LPCSTR szKeyA) const;

  HRESULT MakeAutoAuthorization(__out CStringA &cStrDestA, __in_z LPCSTR szUserNameA, __in_z LPCSTR szPasswordA);

  HRESULT MakeBasicAuthorization(__out CStringA &cStrDestA, __in_z LPCSTR szUserNameA, __in_z LPCSTR szPasswordA,
                                 __in BOOL bIsProxy);
  HRESULT MakeDigestAuthorization(__out CStringA &cStrDestA, __in_z LPCSTR szUserNameA, __in_z LPCSTR szRealmA,
                                  __in_z LPCSTR szPasswordA, __in_z LPCSTR szAlgorithmA, __in_z LPCSTR szMethodA,
                                  __in_z LPCSTR szNonceA, __in SIZE_T nNonceCount, __in_z LPCSTR szQopA,
                                  __in_z LPCSTR szDigestUriA, __in LPVOID lpBodyMD5Checksum, __in_z LPCSTR szOpaqueA,
                                  __in BOOL bIsProxy);

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
