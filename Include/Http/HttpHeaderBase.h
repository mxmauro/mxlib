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

#include "HttpUtils.h"
#include "..\RefCounted.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

#define MX_DECLARE_HTTPHEADER_NAME(__name)          \
            static LPCSTR GetHeaderNameStatic()     \
              { return #__name; };                  \
            virtual LPCSTR GetHeaderName() const    \
              { return GetHeaderNameStatic(); };

//-----------------------------------------------------------

namespace MX {

class MX_NOVTABLE CHttpHeaderBase : public virtual TRefCounted<CBaseMemObj>
{
public:
  typedef enum {
    DuplicateBehaviorError, DuplicateBehaviorReplace, DuplicateBehaviorAdd, DuplicateBehaviorMerge
  } eDuplicateBehavior;

protected:
  CHttpHeaderBase();
public:
  ~CHttpHeaderBase();

  static HRESULT Create(_In_ LPCSTR szHeaderNameA, _In_ BOOL bIsRequest, _Out_ CHttpHeaderBase **lplpHeader,
                        _In_opt_ SIZE_T nHeaderNameLen = (SIZE_T)-1);
  template<class T>
  static HRESULT Create(_In_ BOOL bIsRequest, _Out_ T **lplpHeader)
    {
    return Create(T::GetHeaderNameStatic(), bIsRequest, reinterpret_cast<CHttpHeaderBase**>(lplpHeader));
    };

  virtual LPCSTR GetHeaderName() const = 0;

  virtual HRESULT Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1) = 0;
  //NOTE: Unicode values will be UTF-8 & URL encoded
  HRESULT Parse(_In_z_ LPCWSTR szValueW, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1);

  virtual HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser) = 0;

  virtual eDuplicateBehavior GetDuplicateBehavior() const
    {
    return DuplicateBehaviorError;
    };

  virtual HRESULT Merge(_In_ CHttpHeaderBase *lpHeader);

protected:
  //helpers
  static LPCSTR SkipSpaces(_In_ LPCSTR sA, _In_ LPCSTR szEndA);
  static LPCSTR SkipUntil(_In_ LPCSTR sA, _In_ LPCSTR szEndA, _In_opt_z_ LPCSTR szStopCharsA = NULL);

  static LPCSTR GetToken(_In_ LPCSTR sA, _In_ LPCSTR szEndA);
  static HRESULT GetQuotedString(_Out_ CStringA &cStrA, _Inout_ LPCSTR &sA, _In_ LPCSTR szEndA);

  static HRESULT GetParamNameAndValue(_In_ BOOL bUseUtf8AsDefaultCharset, _Out_ CStringA &cStrTokenA,
                                      _Out_ CStringW &cStrValueW, _Inout_ LPCSTR &sA, _In_ LPCSTR szEndA,
                                      _Out_opt_ LPBOOL lpbExtendedParam = NULL);

  static BOOL RawISO_8859_1_to_UTF8(_Out_ CStringW &cStrDestW, _In_ LPCWSTR szSrcW, _In_ SIZE_T nSrcLen);
};

//-----------------------------------------------------------

class CHttpHeaderArray : public TArrayListWithRelease<CHttpHeaderBase*>
{
private:
public:
  CHttpHeaderArray() : TArrayListWithRelease<CHttpHeaderBase*>()
    { };
  CHttpHeaderArray(_In_ const CHttpHeaderArray& cSrc) throw(...);

  CHttpHeaderArray& operator=(_In_ const CHttpHeaderArray& cSrc) throw(...);

  //NOTE: Returns -1 if not found
  SIZE_T Find(_In_z_ LPCSTR szNameA) const;
  //NOTE: Returns -1 if not found
  SIZE_T Find(_In_z_ LPCWSTR szNameW) const;

  template<class T>
  T* Find() const
    {
    SIZE_T nIndex = Find(T::GetHeaderNameStatic());
    if (nIndex == (SIZE_T)-1)
      return NULL;
    return reinterpret_cast<T*>(GetElementAt(nIndex));
    };

  HRESULT Merge(_In_ const CHttpHeaderArray& cSrc, _In_ BOOL bForceReplaceExisting);
  HRESULT Merge(_In_ CHttpHeaderBase *lpSrc, _In_ BOOL bForceReplaceExisting);
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
