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
  CHttpCommon(_In_ BOOL bActAsServer, _In_ CPropertyBag &cPropBag);
  ~CHttpCommon();

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
  HRESULT AddHeader(_Out_opt_ T **lplpHeader=NULL, _In_ BOOL bReplaceExisting=TRUE)
    {
    return AddHeader(T::GetNameStatic(), reinterpret_cast<CHttpHeaderBase**>(lplpHeader), bReplaceExisting);
    };
  HRESULT AddHeader(_In_z_ LPCSTR szNameA, _Out_opt_ CHttpHeaderBase **lplpHeader=NULL,
                    _In_ BOOL bReplaceExisting=TRUE);

  template<class T>
  HRESULT AddHeader(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen=(SIZE_T)-1, _Out_opt_ T **lplpHeader=NULL)
    {
    return AddHeader(T::GetNameStatic(), szValueA, nValueLen, reinterpret_cast<CHttpHeaderBase**>(lplpHeader),
                     bReplaceExisting);
    };
  HRESULT AddHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen=(SIZE_T)-1,
                    _Out_opt_ CHttpHeaderBase **lplpHeader=NULL, _In_ BOOL bReplaceExisting=TRUE);

  //NOTE: Unicode values will be UTF-8 & URL encoded
  template<class T>
  HRESULT AddHeader(_In_z_ LPCWSTR szValueW, _In_opt_ SIZE_T nValueLen=(SIZE_T)-1, _Out_opt_ T **lplpHeader=NULL,
                    _In_ BOOL bReplaceExisting=TRUE)
    {
    return AddHeader(T::GetNameStatic(), szValueW, nValueLen, reinterpret_cast<CHttpHeaderBase**>(lplpHeader),
                     bReplaceExisting);
    };
  HRESULT AddHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW, _In_opt_ SIZE_T nValueLen=(SIZE_T)-1,
                    _Out_opt_ CHttpHeaderBase **lplpHeader=NULL, _In_ BOOL bReplaceExisting=TRUE);

  HRESULT RemoveHeader(_In_z_ LPCSTR szNameA);
  HRESULT RemoveHeader(_In_ CHttpHeaderBase *lpHeader);
  VOID RemoveAllHeaders();

  SIZE_T GetHeadersCount() const;

  CHttpHeaderBase* GetHeader(_In_ SIZE_T nIndex) const;
  template<class T>
  T* GetHeader() const
    {
    return reinterpret_cast<T*>(GetHeader(T::GetNameStatic()));
    };
  CHttpHeaderBase* GetHeader(_In_z_ LPCSTR szNameA) const;

  HRESULT AddCookie(_In_ CHttpCookie &cSrc);
  HRESULT AddCookie(_In_ CHttpCookieArray &cSrc);
  HRESULT AddCookie(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA, _In_opt_z_ LPCSTR szDomainA=NULL,
                    _In_opt_z_ LPCSTR szPathA=NULL, _In_opt_ const CDateTime *lpDate=NULL,
                    _In_opt_ BOOL bIsSecure=FALSE, _In_opt_ BOOL bIsHttpOnly=FALSE);
  HRESULT AddCookie(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szValueW, _In_opt_z_ LPCWSTR szDomainW=NULL,
                    _In_opt_z_ LPCWSTR szPathW=NULL, _In_opt_ const CDateTime *lpDate=NULL,
                    _In_opt_ BOOL bIsSecure=FALSE, _In_opt_ BOOL bIsHttpOnly=FALSE);
  HRESULT RemoveCookie(_In_z_ LPCSTR szNameA);
  HRESULT RemoveCookie(_In_z_ LPCWSTR szNameW);
  VOID RemoveAllCookies();
  CHttpCookie* GetCookie(_In_ SIZE_T nIndex) const;
  CHttpCookie* GetCookie(_In_z_ LPCSTR szNameA) const;
  CHttpCookie* GetCookie(_In_z_ LPCWSTR szNameW) const;
  CHttpCookieArray* GetCookies() const;
  SIZE_T GetCookiesCount() const;

  eState GetParserState() const;

  HRESULT SetBodyParser(_In_ CHttpBodyParserBase *lpParser, _In_ CPropertyBag &cPropBag);
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

  static BOOL IsValidVerb(_In_z_ LPCSTR szVerbA);
  static BOOL IsValidNameChar(_In_ CHAR chA);

  static BOOL EncodeQuotedString(_Inout_ CStringA &cStrA);

  static LPCSTR GetMimeType(_In_z_ LPCSTR szFileNameA);
  static LPCSTR GetMimeType(_In_z_ LPCWSTR szFileNameW);

  static HRESULT ParseDate(_Out_ CDateTime &cDt, _In_z_ LPCSTR szDateTimeA);

private:
  HRESULT ParseRequestLine(_In_z_ LPCSTR szLineA);
  HRESULT ParseStatusLine(_In_z_ LPCSTR szLineA);
  HRESULT ParseHeader(_Inout_ CStringA &cStrLineA);

  HRESULT ProcessContent(_In_ LPCVOID lpContent, _In_ SIZE_T nContentSize);
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
