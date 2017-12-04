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
#ifndef _MX_HTTPHEADERBASE_H
#define _MX_HTTPHEADERBASE_H

#include "..\Defines.h"
#include "HttpCommon.h"
#include "..\LinkedList.h"
#include "..\Strings\Utf8.h"

//-----------------------------------------------------------

#define MX_DECLARE_HTTPHEADER_NAME(__name)    \
            static LPCSTR GetNameStatic()     \
              { return #__name; };              \
            virtual LPCSTR GetName() const    \
              { return GetNameStatic(); };

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderBase : public virtual CBaseMemObj
{
public:
  typedef enum {
    DuplicateBehaviorError, DuplicateBehaviorReplace, DuplicateBehaviorAppend, DuplicateBehaviorAdd
  } eDuplicateBehavior;

protected:
  CHttpHeaderBase();
public:
  ~CHttpHeaderBase();

  static HRESULT Create(__in LPCSTR szHeaderNameA, __in BOOL bIsRequest, __out CHttpHeaderBase **lplpHeader);
  template<class T>
  static HRESULT Create(__in BOOL bIsRequest, __out T **lplpHeader)
    {
    return Create(T::GetNameStatic(), bIsRequest, reinterpret_cast<CHttpHeaderBase**>(lplpHeader));
    };

  virtual LPCSTR GetName() const = 0;

  virtual HRESULT Parse(__in_z LPCSTR szValueA) = 0;
  virtual HRESULT Build(__inout CStringA &cStrDestA) = 0;

  virtual eDuplicateBehavior GetDuplicateBehavior() const
    {
    return DuplicateBehaviorError;
    };

  //helpers
  static LPCSTR SkipSpaces(__in_z LPCSTR sA);
  static LPCWSTR SkipSpaces(__in_z LPCWSTR sW);

  static LPCSTR GetToken(__in_z LPCSTR sA, __in_z_opt LPCSTR szStopCharsA=NULL);
  static LPCWSTR GetToken(__in_z LPCWSTR sW, __in_z_opt LPCWSTR szStopCharsW=NULL);

  static LPCSTR Advance(__in_z LPCSTR sA, __in_z_opt LPCSTR szStopCharsA=NULL);
  static LPCWSTR Advance(__in_z LPCWSTR sW, __in_z_opt LPCWSTR szStopCharsW=NULL);
};

} //namespace MX

//-----------------------------------------------------------

#include "HttpHeaderEntAllow.h"
#include "HttpHeaderEntContentDisposition.h"
#include "HttpHeaderEntContentEncoding.h"
#include "HttpHeaderEntContentLanguage.h"
#include "HttpHeaderEntContentLength.h"
#include "HttpHeaderEntContentRange.h"
#include "HttpHeaderEntContentType.h"
#include "HttpHeaderEntExpires.h"
#include "HttpHeaderEntLastModified.h"
#include "HttpHeaderGenConnection.h"
#include "HttpHeaderGenDate.h"
#include "HttpHeaderGenTransferEncoding.h"
#include "HttpHeaderGenUpgrade.h"
#include "HttpHeaderReqAccept.h"
#include "HttpHeaderReqAcceptCharset.h"
#include "HttpHeaderReqAcceptEncoding.h"
#include "HttpHeaderReqAcceptLanguage.h"
#include "HttpHeaderReqCacheControl.h"
#include "HttpHeaderReqExpect.h"
#include "HttpHeaderReqHost.h"
#include "HttpHeaderReqIfMatchOrIfNoneMatch.h"
#include "HttpHeaderReqIfModifiedSinceOrIfUnmodifiedSince.h"
#include "HttpHeaderReqRange.h"
#include "HttpHeaderReqReferer.h"
#include "HttpHeaderRespAcceptRanges.h"
#include "HttpHeaderRespAge.h"
#include "HttpHeaderRespCacheControl.h"
#include "HttpHeaderRespETag.h"
#include "HttpHeaderRespLocation.h"
#include "HttpHeaderRespRetryAfter.h"
#include "HttpHeaderGeneric.h"

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERBASE_H
