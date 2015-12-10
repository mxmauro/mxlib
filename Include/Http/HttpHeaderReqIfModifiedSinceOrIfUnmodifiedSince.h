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
#ifndef _MX_HTTPHEADERREQUESTIFMODIFIEDSINCE_or_IFUNMODIFIEDSINCE_H
#define _MX_HTTPHEADERREQUESTIFMODIFIEDSINCE_or_IFUNMODIFIEDSINCE_H

#include "HttpHeaderBase.h"
#include "..\DateTime\DateTime.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderReqIfXXXSinceBase : public CHttpHeaderBase
{
protected:
  CHttpHeaderReqIfXXXSinceBase(__in BOOL bIfModified);
public:
  ~CHttpHeaderReqIfXXXSinceBase();

  HRESULT Parse(__in_z LPCSTR szValueA);

  HRESULT Build(__inout CStringA &cStrDestA);

  HRESULT SetDate(__in CDateTime &cDt);
  CDateTime GetDate() const;

private:
  BOOL bIfModified;
  CDateTime cDt;
};

//-----------------------------------------------------------

class CHttpHeaderReqIfModifiedSince : public CHttpHeaderReqIfXXXSinceBase
{
public:
  CHttpHeaderReqIfModifiedSince() : CHttpHeaderReqIfXXXSinceBase(TRUE)
    { };

  MX_DECLARE_HTTPHEADER_NAME(If-Modified-Since)
};

//-----------------------------------------------------------

class CHttpHeaderReqIfUnmodifiedSince : public CHttpHeaderReqIfXXXSinceBase
{
public:
  CHttpHeaderReqIfUnmodifiedSince() : CHttpHeaderReqIfXXXSinceBase(FALSE)
    { };

  MX_DECLARE_HTTPHEADER_NAME(If-Unmodified-Since)
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERREQUESTIFMODIFIEDSINCE_or_IFUNMODIFIEDSINCE_H
