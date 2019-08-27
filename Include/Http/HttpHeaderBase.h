/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the LICENSE file distributed with
 * this work for additional information regarding copyright ownership.
 *
 * Also, if exists, check the Licenses directory for information about
 * third-party modules.
 *
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _MX_HTTPHEADERBASE_H
#define _MX_HTTPHEADERBASE_H

#include "..\Defines.h"
#include "HttpCommon.h"
#include "..\LinkedList.h"
#include "..\Strings\Utf8.h"

//-----------------------------------------------------------

#define MX_DECLARE_HTTPHEADER_NAME(__name)          \
            static LPCSTR GetHeaderNameStatic()     \
              { return #__name; };                  \
            virtual LPCSTR GetHeaderName() const    \
              { return GetHeaderNameStatic(); };

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderBase : public virtual CBaseMemObj
{
public:
  typedef enum {
    DuplicateBehaviorError, DuplicateBehaviorReplace, DuplicateBehaviorAppend, DuplicateBehaviorAdd
  } eDuplicateBehavior;

  typedef enum {
    BrowserOther, BrowserIE, BrowserIE6, BrowserOpera, BrowserGecko, BrowserChrome, BrowserSafari, BrowserKonqueror
  } eBrowser;

protected:
  CHttpHeaderBase();
public:
  ~CHttpHeaderBase();

  static HRESULT Create(_In_ LPCSTR szHeaderNameA, _In_ BOOL bIsRequest, _Out_ CHttpHeaderBase **lplpHeader);
  template<class T>
  static HRESULT Create(_In_ BOOL bIsRequest, _Out_ T **lplpHeader)
    {
    return Create(T::GetHeaderNameStatic(), bIsRequest, reinterpret_cast<CHttpHeaderBase**>(lplpHeader));
    };

  virtual LPCSTR GetHeaderName() const = 0;

  virtual HRESULT Parse(_In_z_ LPCSTR szValueA) = 0;
  virtual HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser) = 0;

  virtual eDuplicateBehavior GetDuplicateBehavior() const
    {
    return DuplicateBehaviorError;
    };

  //helpers
  static LPCSTR SkipSpaces(_In_z_ LPCSTR sA, _In_opt_ SIZE_T nMaxLen = (SIZE_T)-1);
  static LPCSTR SkipUntil(_In_z_ LPCSTR sA, _In_opt_z_ LPCSTR szStopCharsA = NULL,
                          _In_opt_ SIZE_T nMaxLen = (SIZE_T)-1);

  static LPCSTR GetToken(_In_z_ LPCSTR sA, _In_opt_ SIZE_T nMaxLen = (SIZE_T)-1);
  static HRESULT GetQuotedString(_Out_ CStringA &cStrA, _Inout_ LPCSTR &sA);

  static HRESULT GetParamNameAndValue(_Out_ CStringA &cStrTokenA, _Out_ CStringW &cStrValueW, _Inout_ LPCSTR &sA,
                                      _Out_opt_ LPBOOL lpbExtendedParam = NULL);

  static BOOL RawISO_8859_1_to_UTF8(_Out_ CStringW &cStrDestW, _In_ LPCWSTR szSrcW, _In_ SIZE_T nSrcLen);

  static eBrowser GetBrowserFromUserAgent(_In_ LPCSTR szUserAgentA, _In_opt_ SIZE_T nUserAgentLen = (SIZE_T)-1);
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
#include "HttpHeaderGenSecWebSocketExtensions.h"
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
#include "HttpHeaderReqSecWebSocketKey.h"
#include "HttpHeaderReqSecWebSocketProtocol.h"
#include "HttpHeaderReqSecWebSocketVersion.h"
#include "HttpHeaderRespAcceptRanges.h"
#include "HttpHeaderRespAge.h"
#include "HttpHeaderRespCacheControl.h"
#include "HttpHeaderRespETag.h"
#include "HttpHeaderRespLocation.h"
#include "HttpHeaderRespRetryAfter.h"
#include "HttpHeaderRespSecWebSocketAccept.h"
#include "HttpHeaderRespSecWebSocketProtocol.h"
#include "HttpHeaderRespSecWebSocketVersion.h"
#include "HttpHeaderRespWwwProxyAuthenticate.h"
#include "HttpHeaderGeneric.h"

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERBASE_H
