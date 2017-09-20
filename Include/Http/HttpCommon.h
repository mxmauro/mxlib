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
#ifndef _MX_HTTPCOMMON_H
#define _MX_HTTPCOMMON_H

#include "..\Defines.h"
#include "..\Strings\Strings.h"
#include "..\ArrayList.h"
#include "..\AutoPtr.h"
#include "..\Callbacks.h"
#include "..\RefCounted.h"
#include "..\CircularBuffer.h"
#include "..\ZipLib\ZipLib.h"
#include "HttpCookie.h"
#include "Url.h"
#include "HttpHeaderBase.h"
#include "HttpBodyParserBase.h"
#include <intsafe.h>

//-----------------------------------------------------------

#ifdef _DEBUG
  #define HTTP_DEBUG_OUTPUT
#else //_DEBUG
  //#define HTTP_DEBUG_OUTPUT
#endif //_DEBUG

#define MX_HTTP_MaxHeaderSize                 "HTTP_MaxHeaderSize"
#define MX_HTTP_MaxHeaderSize_DEFVAL          16384

//-----------------------------------------------------------

namespace MX {

class CHttpBodyParserBase;
class CHttpHeaderBase;

class CHttpCommon : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CHttpCommon);
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
  CHttpCommon(__in BOOL bActAsServer, __in CPropertyBag &cPropBag);
  ~CHttpCommon();

  VOID ResetParser();

  //NOTE: After a successful parsing, check for parser state...
  //... if 'stBodyStart', the headers has been parsed
  //... if 'stDone', the document has been completed
  HRESULT Parse(__in LPCVOID lpData, __in SIZE_T nDataSize, __out SIZE_T &nDataUsed);

  int GetErrorCode() const
    {
    return sParser.nErrorCode;
    };

  template<class T>
  HRESULT AddHeader(__out_opt T **lplpHeader=NULL)
    {
    return AddHeader(T::GetNameStatic(), reinterpret_cast<CHttpHeaderBase**>(lplpHeader));
    };
  HRESULT AddHeader(__in_z LPCSTR szNameA, __out_opt CHttpHeaderBase **lplpHeader=NULL);

  template<class T>
  HRESULT AddHeader(__in_z LPCSTR szValueA, __in_opt SIZE_T nValueLen=(SIZE_T)-1, __out_opt T **lplpHeader=NULL)
    {
    return AddHeader(T::GetNameStatic(), szValueA, nValueLen, reinterpret_cast<CHttpHeaderBase**>(lplpHeader));
    };
  HRESULT AddHeader(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA, __in_opt SIZE_T nValueLen=(SIZE_T)-1,
                    __out_opt CHttpHeaderBase **lplpHeader=NULL);

  //NOTE: Unicode values will be UTF-8 & URL encoded
  template<class T>
  HRESULT AddHeader(__in_z LPCWSTR szValueW, __in_opt SIZE_T nValueLen=(SIZE_T)-1, __out_opt T **lplpHeader=NULL)
    {
    return AddHeader(T::GetNameStatic(), szValueW, nValueLen, reinterpret_cast<CHttpHeaderBase**>(lplpHeader));
    };
  HRESULT AddHeader(__in_z LPCSTR szNameA, __in_z LPCWSTR szValueW, __in_opt SIZE_T nValueLen=(SIZE_T)-1,
                    __out_opt CHttpHeaderBase **lplpHeader=NULL);

  HRESULT RemoveHeader(__in_z LPCSTR szNameA);
  VOID RemoveAllHeaders();

  SIZE_T GetHeadersCount() const;

  CHttpHeaderBase* GetHeader(__in SIZE_T nIndex) const;
  template<class T>
  T* GetHeader() const
    {
    return reinterpret_cast<T*>(GetHeader(T::GetNameStatic()));
    };
  CHttpHeaderBase* GetHeader(__in_z LPCSTR szNameA) const;

  HRESULT AddCookie(__in CHttpCookie &cSrc);
  HRESULT AddCookie(__in CHttpCookieArray &cSrc);
  HRESULT AddCookie(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA, __in_z_opt LPCSTR szDomainA=NULL,
                    __in_z_opt LPCSTR szPathA=NULL, __in_opt const CDateTime *lpDate=NULL,
                    __in_opt BOOL bIsSecure=FALSE, __in_opt BOOL bIsHttpOnly=FALSE);
  HRESULT AddCookie(__in_z LPCWSTR szNameW, __in_z LPCWSTR szValueW, __in_z_opt LPCWSTR szDomainW=NULL,
                    __in_z_opt LPCWSTR szPathW=NULL, __in_opt const CDateTime *lpDate=NULL,
                    __in_opt BOOL bIsSecure=FALSE, __in_opt BOOL bIsHttpOnly=FALSE);
  HRESULT RemoveCookie(__in_z LPCSTR szNameA);
  HRESULT RemoveCookie(__in_z LPCWSTR szNameW);
  VOID RemoveAllCookies();
  CHttpCookie* GetCookie(__in SIZE_T nIndex) const;
  CHttpCookie* GetCookie(__in_z LPCSTR szNameA) const;
  CHttpCookie* GetCookie(__in_z LPCWSTR szNameW) const;
  CHttpCookieArray* GetCookies() const;
  SIZE_T GetCookiesCount() const;

  eState GetParserState() const;

  HRESULT SetBodyParser(__in CHttpBodyParserBase *lpParser, __in CPropertyBag &cPropBag);
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

  CHttpBodyParserBase* GetDefaultBodyParser();

  static BOOL IsValidVerb(__in_z LPCSTR szVerbA);
  static BOOL IsValidNameChar(__in CHAR chA);

  static BOOL EncodeQuotedString(__inout CStringA &cStrA);

  static LPCSTR GetMimeType(__in_z LPCSTR szFileNameA);
  static LPCSTR GetMimeType(__in_z LPCWSTR szFileNameW);

  static HRESULT ParseDate(__out CDateTime &cDt, __in_z LPCSTR szDateTimeA);

private:
  HRESULT ParseRequestLine(__in_z LPCSTR szLineA);
  HRESULT ParseStatusLine(__in_z LPCSTR szLineA);
  HRESULT ParseHeader(__inout CStringA &cStrLineA);

  HRESULT ProcessContent(__in LPCVOID lpContent, __in SIZE_T nContentSize);
  HRESULT FlushContent();

private:
  CPropertyBag &cPropBag;
  BOOL bActAsServer;
  int nMaxHeaderSize;

  struct {
    eState nState;
    CStringA cStrCurrLineA;
    int nHeadersLen;
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

#endif //_MX_HTTPCOMMON_H
