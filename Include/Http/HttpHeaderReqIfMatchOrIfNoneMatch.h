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
#ifndef _MX_HTTPHEADERREQUESTIFMATCH_or_IFNONEMATCH_H
#define _MX_HTTPHEADERREQUESTIFMATCH_or_IFNONEMATCH_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderReqIfXXXMatchBase : public CHttpHeaderBase
{
public:
  class CEntity : public virtual CBaseMemObj
  {
  public:
    CEntity();
    ~CEntity();

    HRESULT SetTag(__in_z LPCSTR szTagA);
    HRESULT SetTag(__in_z LPCWSTR szTagW);
    LPCWSTR GetTag() const;

    HRESULT SetWeak(__in BOOL bIsWeak);
    BOOL GetWeak() const;

  private:
    CStringW cStrTagW;
    BOOL bIsWeak;
  };

  //----

protected:
  CHttpHeaderReqIfXXXMatchBase(__in_opt BOOL bIsMatch=TRUE);
public:
  ~CHttpHeaderReqIfXXXMatchBase();

  HRESULT Parse(__in_z LPCSTR szValueA);

  HRESULT Build(__inout CStringA &cStrDestA);

  HRESULT AddEntity(__in_z LPCSTR szEntityA, __out_opt CEntity **lplpEntity=NULL);
  HRESULT AddEntity(__in_z LPCWSTR szEntityW, __out_opt CEntity **lplpEntity=NULL);

  SIZE_T GetEntitiesCount() const;
  CEntity* GetEntity(__in SIZE_T nIndex) const;
  CEntity* GetEntity(__in_z LPCWSTR szTagW) const;

private:
  TArrayListWithDelete<CEntity*> cEntitiesList;
  BOOL bIsMatch;
};

//-----------------------------------------------------------

class CHttpHeaderReqIfMatch : public CHttpHeaderReqIfXXXMatchBase
{
public:
  CHttpHeaderReqIfMatch() : CHttpHeaderReqIfXXXMatchBase(TRUE)
    { };

  MX_DECLARE_HTTPHEADER_NAME(If-Match)
};

//-----------------------------------------------------------

class CHttpHeaderReqIfNoneMatch : public CHttpHeaderReqIfXXXMatchBase
{
public:
  CHttpHeaderReqIfNoneMatch() : CHttpHeaderReqIfXXXMatchBase(FALSE)
    { };

  MX_DECLARE_HTTPHEADER_NAME(If-None-Match)
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERREQUESTIFMATCH_or_IFNONEMATCH_H
