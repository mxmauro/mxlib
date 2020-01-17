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
#ifndef _MX_HTTPCOMMON_H
#define _MX_HTTPCOMMON_H

#include "..\Defines.h"
#include "..\Strings\Strings.h"
#include "..\ArrayList.h"
#include "..\AutoPtr.h"
#include "..\Callbacks.h"
#include "..\RefCounted.h"
#include "..\Loggable.h"
#include "..\CircularBuffer.h"
#include "..\ZipLib\ZipLib.h"
#include "HttpCookie.h"
#include "Url.h"
#include <intsafe.h>

//-----------------------------------------------------------

namespace MX {

class CHttpBodyParserBase;
class CHttpHeaderBase;

class CHttpCommon : public virtual CBaseMemObj, public CLoggable, public CNonCopyableObj
{
public:
  class CContentDecoder;

  typedef enum {
    StateStart,

    StateRequestLine,
    StateStatusLine,
    StateNearRequestOrStatusEnd,

    StateHeaderStart,
    StateHeaderName,
    StateHeaderValueSpaceBefore,
    StateHeaderValue,
    StateHeaderValueSpaceAfter,
    StateNearHeaderValueEnd,

    StateHeadersEnd,

    StateIgnoringHeader,
    StateNearIgnoringHeaderEnd,

    StateBodyStart,

    StateReceivingBodyMarkStart,
    StateIdentityBodyStart = StateReceivingBodyMarkStart,

    StateChunkPreStart,
    StateChunkStart,
    StateChunkStartIgnoreExtension,
    StateNearEndOfChunkStart,

    StateChunkData,
    StateChunkAfterData,
    StateNearEndOfChunkAfterData,

    StateReceivingBodyMarkEnd = StateNearEndOfChunkAfterData,

    StateDone,
    StateError
  } eState;

  typedef enum {
    TransferEncodingMethodNone, TransferEncodingMethodChunked
  } eTransferEncodingMethod;

  typedef enum {
    ContentEncodingMethodIdentity, ContentEncodingMethodGZip, ContentEncodingMethodDeflate
  } eContentEncodingMethod;

public:
  CHttpCommon(_In_ BOOL bActAsServer, _In_ CLoggable *lpLogHandler);
  ~CHttpCommon();

  VOID SetOption_MaxHeaderSize(_In_ DWORD dwSize);

  VOID ResetParser();

  //NOTE: After a successful parsing, check for parser state...
  //... if 'stBodyStart', the headers has been parsed
  //... if 'stDone', the document has been completed
  HRESULT Parse(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize, _Out_ SIZE_T &nDataUsed);

  int GetErrorCode() const
    {
    return sParser.nErrorCode;
    };

  template<class T>
  HRESULT AddHeader(_Out_opt_ T **lplpHeader = NULL, _In_ BOOL bReplaceExisting = TRUE)
    {
    return AddHeader(T::GetHeaderNameStatic(), reinterpret_cast<CHttpHeaderBase**>(lplpHeader), bReplaceExisting);
    };
  HRESULT AddHeader(_In_z_ LPCSTR szNameA, _Out_opt_ CHttpHeaderBase **lplpHeader = NULL,
                    _In_ BOOL bReplaceExisting = TRUE);

  template<class T>
  HRESULT AddHeader(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1, _Out_opt_ T **lplpHeader = NULL,
                    _In_ BOOL bReplaceExisting = TRUE)
    {
    return AddHeader(T::GetHeaderNameStatic(), szValueA, nValueLen, reinterpret_cast<CHttpHeaderBase**>(lplpHeader),
                     bReplaceExisting);
    };
  HRESULT AddHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1,
                    _Out_opt_ CHttpHeaderBase **lplpHeader = NULL, _In_ BOOL bReplaceExisting = TRUE);

  //NOTE: Unicode values will be UTF-8 & URL encoded
  template<class T>
  HRESULT AddHeader(_In_z_ LPCWSTR szValueW, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1, _Out_opt_ T **lplpHeader = NULL,
                    _In_ BOOL bReplaceExisting = TRUE)
    {
    return AddHeader(T::GetHeaderNameStatic(), szValueW, nValueLen, reinterpret_cast<CHttpHeaderBase**>(lplpHeader),
                     bReplaceExisting);
    };
  HRESULT AddHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1,
                    _Out_opt_ CHttpHeaderBase **lplpHeader = NULL, _In_ BOOL bReplaceExisting = TRUE);

  HRESULT RemoveHeader(_In_z_ LPCSTR szNameA);
  HRESULT RemoveHeader(_In_ CHttpHeaderBase *lpHeader);
  VOID RemoveAllHeaders();

  SIZE_T GetHeadersCount() const;

  CHttpHeaderBase* GetHeader(_In_ SIZE_T nIndex) const;
  template<class T>
  T* GetHeader() const
    {
    return reinterpret_cast<T*>(GetHeader(T::GetHeaderNameStatic()));
    };
  CHttpHeaderBase* GetHeader(_In_z_ LPCSTR szNameA) const;

  HRESULT AddCookie(_In_ CHttpCookie &cSrc);
  HRESULT AddCookie(_In_ CHttpCookieArray &cSrc);
  HRESULT AddCookie(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA, _In_opt_z_ LPCSTR szDomainA = NULL,
                    _In_opt_z_ LPCSTR szPathA = NULL, _In_opt_ const CDateTime *lpDate = NULL,
                    _In_opt_ BOOL bIsSecure = FALSE, _In_opt_ BOOL bIsHttpOnly = FALSE);
  HRESULT AddCookie(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szValueW, _In_opt_z_ LPCWSTR szDomainW=NULL,
                    _In_opt_z_ LPCWSTR szPathW = NULL, _In_opt_ const CDateTime *lpDate = NULL,
                    _In_opt_ BOOL bIsSecure = FALSE, _In_opt_ BOOL bIsHttpOnly = FALSE);
  HRESULT RemoveCookie(_In_z_ LPCSTR szNameA);
  HRESULT RemoveCookie(_In_z_ LPCWSTR szNameW);
  VOID RemoveAllCookies();
  CHttpCookie* GetCookie(_In_ SIZE_T nIndex) const;
  CHttpCookie* GetCookie(_In_z_ LPCSTR szNameA) const;
  CHttpCookie* GetCookie(_In_z_ LPCWSTR szNameW) const;
  CHttpCookieArray* GetCookies() const;
  SIZE_T GetCookiesCount() const;

  eState GetParserState() const;

  HRESULT SetBodyParser(_In_ CHttpBodyParserBase *lpParser);
  CHttpBodyParserBase* GetBodyParser() const;

  BOOL IsActingAsServer() const
    {
    return bActAsServer;
    };

  int GetRequestVersionMajor() const;
  int GetRequestVersionMinor() const;
  LPCSTR GetRequestMethod() const;
  CUrl* GetRequestUri() const;

  BOOL IsKeepAliveRequest() const;

  LONG GetResponseStatus() const;
  LPCSTR GetResponseReasonA() const;

  static BOOL IsValidVerb(_In_ LPCSTR szStrA, _In_ SIZE_T nStrLen);

  static BOOL IsValidNameChar(_In_ CHAR chA);

  static BOOL FromISO_8859_1(_Out_ CStringW &cStrDestW, _In_ LPCSTR szStrA, _In_ SIZE_T nStrLen);
  static BOOL ToISO_8859_1(_Out_ CStringA &cStrDestA, _In_ LPCWSTR szStrW, _In_ SIZE_T nStrLen);

  static BOOL BuildQuotedString(_Out_ CStringA &cStrA, _In_ LPCSTR szStrA, _In_ SIZE_T nStrLen,
                                _In_ BOOL bDontQuoteIfToken);
  static BOOL BuildQuotedString(_Out_ CStringA &cStrA, _In_ LPCWSTR szStrW, _In_ SIZE_T nStrLen,
                                _In_ BOOL bDontQuoteIfToken);
  static BOOL BuildExtendedValueString(_Out_ CStringA &cStrA, _In_ LPCWSTR szStrW, _In_ SIZE_T nStrLen);

  static LPCSTR GetMimeType(_In_z_ LPCSTR szFileNameA);
  static LPCSTR GetMimeType(_In_z_ LPCWSTR szFileNameW);

  static HRESULT ParseDate(_Out_ CDateTime &cDt, _In_z_ LPCSTR szDateTimeA);
  static HRESULT ParseDate(_Out_ CDateTime &cDt, _In_z_ LPCWSTR szDateTimeW);

  static HRESULT _GetTempPath(_Out_ MX::CStringW &cStrPathW);

private:
  HRESULT ParseRequestLine(_In_z_ LPCSTR szLineA);
  HRESULT ParseStatusLine(_In_z_ LPCSTR szLineA);
  HRESULT ParseHeader(_Inout_ CStringA &cStrLineA);

  HRESULT ProcessContent(_In_ LPCVOID lpContent, _In_ SIZE_T nContentSize);
  HRESULT FlushContent();

private:
  BOOL bActAsServer;
  DWORD dwMaxHeaderSize;

  struct {
    eState nState;
    CStringA cStrCurrLineA;
    DWORD dwHeadersLen;
    struct {
      ULONGLONG nSize;
      ULONGLONG nReaded;
    } sChunk;
    int nErrorCode;
  } sParser;
  //----
  struct {
    ULONG nHttpProtocol;
    SIZE_T nMethodIndex;
    CUrl cUrl;
  } sRequest;
  struct {
    LONG nStatusCode;
    CStringA cStrReasonA;
  } sResponse;
  LONG nHeaderFlags;
  ULONGLONG nContentLength;
  ULONGLONG nIdentityBodyReadedContentLength;
  TArrayListWithDelete<CHttpHeaderBase*> cHeaders;
  CHttpCookieArray cCookies;
  TAutoDeletePtr<CZipLib> cBodyDecoder;
  TAutoRefCounted<CHttpBodyParserBase> cBodyParser;
};

} //namespace MX

//-----------------------------------------------------------

#include "HttpHeaderBase.h"
#include "HttpBodyParserBase.h"

//-----------------------------------------------------------

#endif //_MX_HTTPCOMMON_H
