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

#include "HttpUtils.h"
#include "..\ArrayList.h"
#include "..\AutoPtr.h"
#include "..\Callbacks.h"
#include "..\RefCounted.h"
#include "..\Loggable.h"
#include "..\CircularBuffer.h"
#include "..\ZipLib\ZipLib.h"
#include "HttpCookie.h"
#include "HttpHeaderBase.h"
#include "Url.h"
#include <intsafe.h>

//-----------------------------------------------------------

namespace MX {

class CHttpBodyParserBase;
class CHttpHeaderBase;

namespace Internals {

class CHttpParser : public virtual CBaseMemObj, public CLoggable, public CNonCopyableObj
{
public:
  class CContentDecoder;

  typedef enum {
    StateStart,

    StateRequestOrStatusLine,
    StateRequestOrStatusLineEnding,

    StateHeaderStart,
    StateHeaderName,
    StateHeaderValue,
    StateHeaderValueEnding,
    StateHeadersEnding,

    StateBodyStart,

    StateReceivingBodyMarkStart,
    StateIdentityBodyStart = StateReceivingBodyMarkStart,

    StateChunkPreStart,
    StateChunkStart,
    StateChunkStartEnding,
    StateChunkStartIgnoreExtension,

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
  CHttpParser(_In_ BOOL bActAsServer, _In_opt_ CLoggable *lpLogHandler);

  VOID SetOption_MaxHeaderSize(_In_ DWORD dwSize);

  VOID Reset();

  //NOTE: After a successful parsing, check for parser state...
  //... if 'stBodyStart', the headers has been parsed
  //... if 'stDone', the document has been completed
  HRESULT Parse(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize, _Out_ SIZE_T &nDataUsed);

  eState GetState() const
    {
    return nState;
    };

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
  MX::Http::eBrowser GetRequestBrowser() const;

  BOOL IsKeepAliveRequest() const;

  LONG GetResponseStatus() const;
  LPCSTR GetResponseReasonA() const;

  CHttpCookieArray& Cookies() const
    {
    return const_cast<CHttpParser*>(this)->cCookies;
    };
  CHttpHeaderArray& Headers() const
    {
    return const_cast<CHttpParser*>(this)->cHeaders;
    };

private:
  HRESULT ParseRequestLine(_In_z_ LPCSTR szLineA);
  HRESULT ParseStatusLine(_In_z_ LPCSTR szLineA);
  HRESULT ParseHeader(_In_ CStringA &cStrLineA);

  HRESULT ProcessContent(_In_ LPCVOID lpContent, _In_ SIZE_T nContentSize);
  HRESULT FlushContent();

private:
  BOOL bActAsServer;
  DWORD dwMaxHeaderSize;

  eState nState;
  CStringA cStrCurrLineA;
  DWORD dwHeadersLen;

  struct {
    ULONG nHttpProtocol;
    LPCSTR szMethodA;
    CUrl cUrl;
    MX::Http::eBrowser nBrowser;
  } sRequest;
  struct {
    LONG nStatusCode;
    CStringA cStrReasonA;
  } sResponse;
  LONG nHeaderFlags;
  CHttpCookieArray cCookies;
  CHttpHeaderArray cHeaders;
  struct {
    ULONGLONG nContentLength;
    ULONGLONG nIdentityReadedContentLength;
    struct {
      ULONGLONG nSize;
      ULONGLONG nReaded;
    } sChunk;
    TAutoDeletePtr<CZipLib> cDecoder;
    TAutoRefCounted<CHttpBodyParserBase> cParser;
  } sBody;
};

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

#include "HttpHeaderBase.h"
#include "HttpBodyParserBase.h"

//-----------------------------------------------------------

#endif //_MX_HTTPCOMMON_H
