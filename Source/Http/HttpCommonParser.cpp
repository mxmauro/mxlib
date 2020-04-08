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
#include "..\..\Include\Http\HttpCommon.h"
#include "..\..\Include\Strings\Utf8.h"
#include "..\..\Include\Debug.h"
#include "..\..\Include\Http\HttpBodyParserDefault.h"
#include "..\..\Include\Http\HttpBodyParserUrlEncodedForm.h"
#include "..\..\Include\Http\HttpBodyParserMultipartFormData.h"

//-----------------------------------------------------------

#define MAX_REQUEST_STATUS_LINE_LENGTH                  4096
#define MAX_HEADER_LENGTH                               4096

#define HEADER_FLAG_TransferEncodingChunked           0x0001
#define HEADER_FLAG_ContentEncodingGZip               0x0002
#define HEADER_FLAG_ContentEncodingDeflate            0x0004

#define HEADER_FLAG_MASK_ContentEncoding              0x0006

#define HEADER_FLAG_UpgradeWebSocket                  0x0008
#define HEADER_FLAG_ConnectionUpgrade                 0x0010
#define HEADER_FLAG_ConnectionKeepAlive               0x0020
#define HEADER_FLAG_ConnectionClose                   0x0040

#define _HTTP_STATUS_NoContent                           204
#define _HTTP_STATUS_NotModified                         304

//-----------------------------------------------------------

static BOOL IsContentTypeHeader(_In_z_ LPCSTR szHeaderA);

//-----------------------------------------------------------

namespace MX {

namespace Internals {

CHttpParser::CHttpParser(_In_ BOOL _bActAsServer, _In_opt_ CLoggable *lpLogHandler) : CBaseMemObj(), CLoggable(),
                                                                                      CNonCopyableObj()
{
  bActAsServer = _bActAsServer;
  dwMaxHeaderSize = 16384;
  //----
  SetLogParent(lpLogHandler);
  //----
  Reset();
  return;
}

VOID CHttpParser::SetOption_MaxHeaderSize(_In_ DWORD dwSize)
{
  if (dwSize < 2048)
    dwMaxHeaderSize = 2048;
  else if (dwSize > 327680)
    dwMaxHeaderSize = 327680;
  else
    dwMaxHeaderSize = (dwSize + 16383) & (~16383);
  return;
}

VOID CHttpParser::Reset()
{
  nState = StateStart;
  cStrCurrLineA.Empty();
  dwHeadersLen = 0;
  nHeaderFlags = 0;
  cHeaders.RemoveAllElements();
  cCookies.RemoveAllElements();
  //----
  sRequest.nHttpProtocol = 0;
  sRequest.szMethodA = NULL;
  sRequest.cUrl.Reset();
  sRequest.nBrowser = Http::BrowserOther;
  //----
  sResponse.nStatusCode = 0;
  sResponse.cStrReasonA.Empty();
  //----
  sBody.nContentLength = ULONGLONG_MAX;
  sBody.nIdentityReadedContentLength = 0;
  sBody.cDecoder.Reset();
  sBody.cParser.Release();
  sBody.sChunk.nSize = 0;
  sBody.sChunk.nReaded = 0;
  return;
}

#define BACKWARD_CHAR()     szDataA--
HRESULT CHttpParser::Parse(_In_ LPCVOID lpData, _In_ SIZE_T nDataSize, _Out_ SIZE_T &nDataUsed)
{
  LPCSTR szDataA, sA;
  HRESULT hRes;

  nDataUsed = 0;
  if (lpData == NULL && nDataSize > 0)
    return E_POINTER;
  if (nState == StateDone)
    return S_OK;
  if (nState == StateError)
    return MX_E_InvalidData;
  hRes = S_OK;
  for (szDataA = (LPCSTR)lpData; szDataA != (LPCSTR)lpData + nDataSize; szDataA++)
  {
    switch (nState)
    {
      case StateStart:
        if (*szDataA == '\r' || *szDataA == '\n')
          break;
        nState = (bActAsServer != FALSE) ? StateRequestLine : StateStatusLine;
        BACKWARD_CHAR();
        break;

      case StateRequestLine:
      case StateStatusLine:
        if (*szDataA == '\r' || *szDataA == '\n')
        {
          hRes = (nState == StateRequestLine) ? ParseRequestLine((LPCSTR)cStrCurrLineA)
                                              : ParseStatusLine((LPCSTR)cStrCurrLineA);
          if (FAILED(hRes))
            goto done; //if the request/status line fails to parse, it it a not recoverable error
          //end of request/status line
          nState = (*szDataA == '\r') ? StateNearRequestOrStatusEnd : StateHeaderStart;
          cStrCurrLineA.Empty();
          break;
        }
        //is status/request line too long?
        if (cStrCurrLineA.GetLength() > MAX_REQUEST_STATUS_LINE_LENGTH)
        {
err_line_too_long:
          hRes = MX_E_BadLength; //header line too long
          goto done;
        }
        //add character to line
        if (cStrCurrLineA.ConcatN(szDataA, 1) == FALSE)
        {
err_nomem:  hRes = E_OUTOFMEMORY;
          goto done;
        }
        break;

      case StateNearRequestOrStatusEnd:
        if (*szDataA != '\n')
        {
err_invalid_data:
          hRes = MX_E_InvalidData;
          goto done;
        }
        nState = StateHeaderStart;
        break;

      case StateHeaderStart:
        if (*szDataA == '\r' || *szDataA == '\n')
        {
          //no more headers
          if (cStrCurrLineA.IsEmpty() == FALSE)
          {
            hRes = ParseHeader(cStrCurrLineA);
            if (FAILED(hRes))
              goto done;
            cStrCurrLineA.Empty();
          }
          nState = StateHeadersEnd;
          if (*szDataA == '\n')
            goto headers_end_reached;
          break;
        }
        //are we continuing the last header?
        if (*szDataA == ' ' || *szDataA == '\t')
        {
          if (cStrCurrLineA.IsEmpty() != FALSE)
            goto err_invalid_data;
          nState = StateHeaderValueSpaceAfter;
          break;
        }
        //new header arrives, first check if we have a previous defined
        if (cStrCurrLineA.IsEmpty() == FALSE)
        {
          hRes = ParseHeader(cStrCurrLineA);
          if (FAILED(hRes))
            goto done;
          cStrCurrLineA.Empty();
        }
        nState = StateHeaderName;
        BACKWARD_CHAR();
        break;

      case StateHeaderName:
        //check headers length
        if (dwHeadersLen >= dwMaxHeaderSize)
          goto err_line_too_long;
        dwHeadersLen++;
        //end of header name?
        if (*szDataA == ':')
        {
          if (cStrCurrLineA.IsEmpty() != FALSE)
            goto err_invalid_data;
          if (cStrCurrLineA.ConcatN(":", 1) == FALSE)
            goto err_nomem;
          nState = StateHeaderValueSpaceBefore;
          break;
        }
        //check for valid token char
        if (Http::IsValidNameChar(*szDataA) == FALSE)
          goto err_invalid_data;
        if (cStrCurrLineA.GetLength() > MAX_HEADER_LENGTH)
          goto err_line_too_long;
        //add character to line
        if (cStrCurrLineA.ConcatN(szDataA, 1) == FALSE)
          goto err_nomem;
        //ignore IIS bug sending HTTP/ more than once
        sA = (LPCSTR)cStrCurrLineA;
        if (sA[0] == 'H' && sA[1] == 'T' && sA[2] == 'T' && sA[3] == 'P' && sA[4] == '/')
        {
          dwHeadersLen -= 5;
          nState = StateIgnoringHeader;
          cStrCurrLineA.Empty();
          break;
        }
        break;

      case StateHeaderValueSpaceBefore:
        if (*szDataA == ' ' || *szDataA == '\t')
          break; //ignore spaces
        //real header begin
        nState = StateHeaderValue;
        //fall into 'stHeaderValue'

      case StateHeaderValue:
on_header_value:
        if (*szDataA == '\r' || *szDataA == '\n')
        {
          hRes = ParseHeader(cStrCurrLineA);
          if (FAILED(hRes))
            goto done;
          cStrCurrLineA.Empty();
          nState = (*szDataA == '\r') ? StateNearHeaderValueEnd : StateHeaderStart;
          break;
        }
        if (*szDataA == ' ' || *szDataA == '\t')
        {
          nState = StateHeaderValueSpaceAfter;
          break;
        }
        //check headers length and valid value char
        if (dwHeadersLen >= dwMaxHeaderSize)
          goto err_line_too_long;
        dwHeadersLen++;
        if (*szDataA == 0)
          goto err_invalid_data;
        if (cStrCurrLineA.GetLength() > MAX_HEADER_LENGTH)
          goto err_line_too_long;
        //add character to line
        if (cStrCurrLineA.ConcatN(szDataA, 1) == FALSE)
          goto err_nomem;
        break;

      case StateHeaderValueSpaceAfter:
        if (*szDataA == '\r' || *szDataA == '\n')
          goto on_header_value;
        if (*szDataA == ' ' || *szDataA == '\t')
          break;
        if (dwHeadersLen >= dwMaxHeaderSize)
          goto err_line_too_long;
        dwHeadersLen++;
        //add character to the line
        if (cStrCurrLineA.GetLength() > MAX_HEADER_LENGTH)
          goto err_line_too_long;
        if (cStrCurrLineA.ConcatN(" ", 1) == FALSE)
          goto err_nomem;
        nState = StateHeaderValue;
        goto on_header_value;

      case StateNearHeaderValueEnd:
        if (*szDataA != '\n')
          goto err_invalid_data;
        nState = StateHeaderStart;
        break;

      case StateHeadersEnd:
        if (*szDataA != '\n')
          goto err_invalid_data;
headers_end_reached:
        szDataA++;
        //do some checks
        if (bActAsServer != FALSE)
        {
          if (StrCompareA(GetRequestMethod(), "HEAD") == 0 || StrCompareA(GetRequestMethod(), "GET") == 0)
          {
            //content or chunked transfer is not allowed
            if ((sBody.nContentLength != 0 && sBody.nContentLength != ULONGLONG_MAX) ||
                (nHeaderFlags & HEADER_FLAG_TransferEncodingChunked) != 0)
            {
              nState = StateBodyStart;
              hRes = MX_HRESULT_FROM_WIN32(ERROR_DATA_NOT_ACCEPTED);
            }
            else
            {
              //no content... we are done
              nState = StateDone;
            }
            goto done;
          }
          if (StrCompareA(GetRequestMethod(), "POST") == 0)
          {
            //content-length must be specified
            if (sBody.nContentLength == ULONGLONG_MAX || (nHeaderFlags & HEADER_FLAG_TransferEncodingChunked) != 0)
              goto err_invalid_data;
          }
        }
        else
        {
          //1xx, 204 and 304 responses must not contain a body
          if (sResponse.nStatusCode < 200 || sResponse.nStatusCode == _HTTP_STATUS_NoContent ||
              sResponse.nStatusCode == _HTTP_STATUS_NotModified)
          {
            //content or chunked transfer is not allowed
            if (sBody.nContentLength != ULONGLONG_MAX || (nHeaderFlags & HEADER_FLAG_TransferEncodingChunked) != 0)
            {
              hRes = MX_HRESULT_FROM_WIN32(ERROR_DATA_NOT_ACCEPTED);
            }
            else
            {
              //no content... we are done
              nState = StateDone;
              goto done;
            }
          }
        }
        //if transfer encoding is set, then ignore content-length
        if ((nHeaderFlags & HEADER_FLAG_TransferEncodingChunked) != 0)
          sBody.nContentLength = ULONGLONG_MAX;
        //if no content we are done
        if (sBody.nContentLength == 0)
        {
          nState = StateDone;
          goto done;
        }
        nState = StateBodyStart;
        if (ShouldLog(1) != FALSE)
        {
          Log(L"HttpCommon(HeadersComplete/0x%p): BodyStart", this);
        }
        goto done;

      case StateBodyStart:
        nState = ((nHeaderFlags & HEADER_FLAG_TransferEncodingChunked) != 0) ? StateChunkPreStart
                                                                             : StateIdentityBodyStart;
        BACKWARD_CHAR();
        break;

      case StateIgnoringHeader:
        if (*szDataA == '\r' || *szDataA == '\n')
          nState = (*szDataA == '\r') ? StateNearIgnoringHeaderEnd : StateHeaderStart;
        break;

      case StateNearIgnoringHeaderEnd:
        if (*szDataA != '\n')
          goto err_invalid_data;
        nState = StateHeaderStart;
        break;

      case StateIdentityBodyStart:
        {
        SIZE_T nToRead;

        nToRead = nDataSize - (szDataA - (LPCSTR)lpData);
        if ((ULONGLONG)nToRead > sBody.nContentLength - sBody.nIdentityReadedContentLength)
          nToRead = (SIZE_T)(sBody.nContentLength - sBody.nIdentityReadedContentLength);
        if (ShouldLog(1) != FALSE && sBody.nContentLength > 0ui64)
        {
          double dbl;
          int _pre_pct, _post_pct;

          dbl = (double)(sBody.nIdentityReadedContentLength) / (double)(sBody.nContentLength);
          _pre_pct = (int)(dbl * 10.0);
          dbl += (double)nToRead / (double)(sBody.nContentLength);
          _post_pct = (int)(dbl * 10.0);
          if (_pre_pct != _post_pct && _post_pct < 10)
          {
            Log(L"HttpCommon(Body/0x%p): %ld0%%", this, _post_pct);
          }
        }
        hRes = ProcessContent(szDataA, nToRead);
        if (FAILED(hRes))
        {
          sBody.cParser.Release();
          sBody.cDecoder.Reset();
          goto done;
        }
        szDataA += (SIZE_T)nToRead;
        sBody.nIdentityReadedContentLength += (ULONGLONG)nToRead;
        if (sBody.nIdentityReadedContentLength == sBody.nContentLength)
        {
          nState = StateDone;
          hRes = FlushContent();
          goto done;
        }
        BACKWARD_CHAR();
        }
        break;

      case StateChunkPreStart:
        if ((*szDataA < '0' || *szDataA          > '9') &&
            (((*szDataA) & 0xDF) < 'A' || ((*szDataA) & 0xDF) > 'F'))
        {
          goto err_invalid_data;
        }
        nState = StateChunkStart;
        sBody.sChunk.nSize = sBody.sChunk.nReaded = 0;
        //fall into 'stChunkStart'

      case StateChunkStart:
        if ((*szDataA >= '0' && *szDataA <= '9') ||
            (((*szDataA) & 0xDF) >= 'A' && ((*szDataA) & 0xDF) <= 'F'))
        {
          if ((sBody.sChunk.nSize & 0xF000000000000000ui64) != 0)
          {
            hRes = MX_E_ArithmeticOverflow;
            goto done;
          }
          sBody.sChunk.nSize <<= 4;
          if (*szDataA >= '0' && *szDataA <= '9')
            sBody.sChunk.nSize |= (ULONGLONG)(*szDataA) - (ULONGLONG)'0';
          else
            sBody.sChunk.nSize |= (ULONGLONG)((*szDataA) & 0xDF) - (ULONGLONG)'A' + 10ui64;
          break;
        }
        //end of chunk size
        if (*szDataA == ' ' || *szDataA == '\t' || *szDataA == ';')
        {
          nState = StateChunkStartIgnoreExtension;
          break;
        }
        if (*szDataA == '\r')
        {
          nState = StateNearEndOfChunkStart;
          break;
        }
        if (*szDataA != '\n')
          goto err_invalid_data;
chunk_data_begin:
        if (ShouldLog(1) != FALSE)
        {
          Log(L"HttpCommon(Chunk/0x%p): %I64u", this, sBody.sChunk.nSize);
        }
        nState = StateChunkData;
        if (sBody.sChunk.nSize == 0)
        {
          //the trailing CR/LF will be consumed by a new parse
          nState = StateDone;
          hRes = FlushContent();
          goto done;
        }
        break;

      case StateChunkStartIgnoreExtension:
        if (*szDataA == '\r')
          nState = StateNearEndOfChunkStart;
        else if (*szDataA == '\n')
          goto chunk_data_begin;
        break;

      case StateNearEndOfChunkStart:
        if (*szDataA != '\n')
          goto err_invalid_data;
        goto chunk_data_begin;

      case StateChunkData:
        {
        SIZE_T nToRead;

        nToRead = nDataSize - (szDataA - (LPCSTR)lpData);
        if ((ULONGLONG)nToRead > sBody.sChunk.nSize - sBody.sChunk.nReaded)
          nToRead = (SIZE_T)(sBody.sChunk.nSize - sBody.sChunk.nReaded);
        hRes = ProcessContent(szDataA, nToRead);
        if (FAILED(hRes))
          goto done;
        szDataA += (SIZE_T)nToRead;
        BACKWARD_CHAR();
        sBody.sChunk.nReaded += (ULONGLONG)nToRead;
        sBody.nIdentityReadedContentLength += (ULONGLONG)nToRead;
        if (sBody.sChunk.nReaded == sBody.sChunk.nSize)
          nState = StateChunkAfterData;
        }
        break;

      case StateChunkAfterData:
        if (*szDataA == '\r')
          nState = StateNearEndOfChunkAfterData;
        else if (*szDataA == '\n')
          nState = StateChunkPreStart;
        else
          goto err_invalid_data;
        break;

      case StateNearEndOfChunkAfterData:
        if (*szDataA != '\n')
          goto err_invalid_data;
        nState = StateChunkPreStart;
        break;

      default:
        MX_ASSERT(FALSE);
        break;
    }
  }

done:
  if (FAILED(hRes))
  {
    nState = StateError;
  }
  nDataUsed = (SIZE_T)szDataA - (SIZE_T)lpData;
  return hRes;
}
#undef BACKWARD_CHAR

HRESULT CHttpParser::SetBodyParser(_In_ CHttpBodyParserBase *lpParser)
{
  MX_ASSERT(lpParser != NULL);
  sBody.cParser = lpParser;
  return lpParser->Initialize(*this);
}

CHttpBodyParserBase *CHttpParser::GetBodyParser() const
{
  CHttpBodyParserBase *lpParser;

  lpParser = sBody.cParser.Get();
  if (lpParser != NULL)
    lpParser->AddRef();
  return lpParser;
}

int CHttpParser::GetRequestVersionMajor() const
{
  return (int)((sRequest.nHttpProtocol >> 8) & 0xFF);
}

int CHttpParser::GetRequestVersionMinor() const
{
  return (int)(sRequest.nHttpProtocol & 0xFF);
}

LPCSTR CHttpParser::GetRequestMethod() const
{
  return (sRequest.szMethodA != NULL) ? sRequest.szMethodA : "";
}

CUrl* CHttpParser::GetRequestUri() const
{
  return const_cast<CUrl *>(&(sRequest.cUrl));
}

MX::Http::eBrowser CHttpParser::GetRequestBrowser() const
{
  return sRequest.nBrowser;
}

BOOL CHttpParser::IsKeepAliveRequest() const
{
  return ((nHeaderFlags & HEADER_FLAG_ConnectionKeepAlive) != 0) ? TRUE : FALSE;
}

LONG CHttpParser::GetResponseStatus() const
{
  return sResponse.nStatusCode;
}

LPCSTR CHttpParser::GetResponseReasonA() const
{
  return (LPSTR)(sResponse.cStrReasonA);
}

HRESULT CHttpParser::ParseRequestLine(_In_z_ LPCSTR szLineA)
{
  SIZE_T nMethodLen, nUriLength;
  LPCSTR szUriStartA;
  HRESULT hRes;

  //get method
  if (ShouldLog(1) != FALSE)
  {
    Log(L"HttpCommon(RequestLine/0x%p): %S", this, szLineA);
  }

  for (nMethodLen = 0; szLineA[nMethodLen] != 0 && szLineA[nMethodLen] != ' '; nMethodLen++);
  if (szLineA[nMethodLen] != ' ')
    return MX_E_InvalidData;
  sRequest.szMethodA = Http::IsValidVerb(szLineA, nMethodLen);
  if (sRequest.szMethodA == NULL)
    return MX_E_InvalidData;

  //parse uri
  szUriStartA = szLineA + (nMethodLen + 1);
  for (nUriLength = 0; szUriStartA[nUriLength] == 9 || szUriStartA[nUriLength] == 12 ||
       (szUriStartA[nUriLength] >= 33 && szUriStartA[nUriLength] <= 126); nUriLength++);
  if (nUriLength == 0)
    return MX_E_InvalidData;
  if (nUriLength == 1 && szUriStartA[0] == '*')
    return E_NOTIMPL;
  if ((nUriLength >= 7 && StrNCompareA(szUriStartA, "http://", 7) == 0) ||
      (nUriLength >= 8 && StrNCompareA(szUriStartA, "https://", 8) == 0) ||
      szUriStartA[0] == '/')
  {
    //full uri specified or absolute path 
    hRes = sRequest.cUrl.ParseFromString(szUriStartA, nUriLength);
    if (FAILED(hRes))
      return hRes;
    sRequest.cUrl.SetScheme((LPCSTR)NULL);
    //leave host if any
  }
  else
  {
    //relative paths and the rest are not supported
    return MX_E_InvalidData;
  }

  //skip blanks
  szLineA = szUriStartA + nUriLength;
  if (*szLineA++ != ' ')
    return MX_E_InvalidData;
  while (*szLineA == ' ' || *szLineA == '\t')
    szLineA++;

  //check http version
  if (szLineA[0] != 'H' || szLineA[1] != 'T' || szLineA[2] != 'T' || szLineA[3] != 'P' ||
      szLineA[4] != '/' || szLineA[5] != '1' || szLineA[6] != '.' ||
      (szLineA[7] != '0' && szLineA[7] != '1'))
    return MX_E_InvalidData;
  sRequest.nHttpProtocol = ((ULONG)(szLineA[5] - '0') << 8) | (ULONG)(szLineA[7] - '0');

  //ignore blanks at the end
  for (szLineA += 8; *szLineA == ' ' || *szLineA == '\t'; szLineA++);
  if (*szLineA != 0)
    return MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpParser::ParseStatusLine(_In_z_ LPCSTR szLineA)
{
  //check http version
  if (szLineA[0] != 'H' || szLineA[1] != 'T' || szLineA[2] != 'T' || szLineA[3] != 'P' ||
      szLineA[4] != '/' || szLineA[5] != '1' || szLineA[6] != '.' ||
      (szLineA[7] != '0' && szLineA[7] != '1') ||
      szLineA[8] != ' ' ||
      szLineA[9] < '1' || szLineA[9] > '5' ||
      szLineA[10] < '0' || szLineA[10] > '9' ||
      szLineA[11] < '0' || szLineA[11] > '9' ||
      szLineA[12] != ' ')
  {
    return MX_E_InvalidData;
  }

  //get status code
  if (ShouldLog(1) != FALSE)
  {
    Log(L"HttpCommon(StatusLine/0x%p): %S", this, szLineA);
  }
  sResponse.nStatusCode = (LONG)(szLineA[9] - '0') * 100 +
                          (LONG)(szLineA[10] - '0') * 10 +
                          (LONG)(szLineA[11] - '0');

  //skip blanks
  for (szLineA += 12; *szLineA == ' ' || *szLineA == '\t'; szLineA++);

  //get reason
  if (sResponse.cStrReasonA.Copy(szLineA) == FALSE)
    return E_OUTOFMEMORY;

  //done
  return S_OK;
}

HRESULT CHttpParser::ParseHeader(_In_ CStringA &cStrLineA)
{
  TAutoDeletePtr<CHttpHeaderBase> cHeader;
  LPSTR szLineA, szNameStartA, szNameEndA, szValueStartA, szValueEndA;
  HRESULT hRes;

  szLineA = (LPSTR)cStrLineA;
  //skip blanks
  while (*szLineA == ' ' || *szLineA == '\t')
    szLineA++;
  if (ShouldLog(1) != FALSE)
  {
    Log(L"HttpCommon(Header/0x%p): %S", this, szLineA);
  }

  //get header name
  szNameStartA = szNameEndA = szLineA;
  while (*szNameEndA != 0 && *szNameEndA != ':')
    szNameEndA++;
  if (*szNameEndA != ':')
    return MX_E_InvalidData;

  //skip blanks
  szLineA = szNameEndA + 1;
  while (*szLineA == ' ' || *szLineA == '\t')
    szLineA++;

  //get header value
  szValueStartA = szValueEndA = szLineA;
  while (*szValueEndA != 0)
    szValueEndA++;
  while (szValueEndA > szValueStartA && (*(szValueEndA - 1) == ' ' || *(szValueEndA - 1) == '\t'))
    szValueEndA--;

  //is a cookie?
  if (bActAsServer != FALSE &&
      (((SIZE_T)(szNameEndA - szNameStartA) == 6 && StrNCompareA(szNameStartA, "Cookie", 6, TRUE) == 0) ||
       ((SIZE_T)(szNameEndA - szNameStartA) == 7 && StrNCompareA(szNameStartA, "Cookie2", 7, TRUE) == 0)))
  {
    CHttpCookieArray cCookieArray;

    hRes = cCookieArray.ParseFromRequestHeader(szValueStartA, (SIZE_T)(szValueEndA - szValueStartA));
    if (SUCCEEDED(hRes))
      hRes = cCookies.Merge(cCookieArray, FALSE);
    if (FAILED(hRes))
      return hRes;
  }
  else if (bActAsServer == FALSE &&
           (((SIZE_T)(szNameEndA - szNameStartA) == 10 && StrNCompareA(szNameStartA, "Set-Cookie", 10, TRUE) == 0) ||
            ((SIZE_T)(szNameEndA - szNameStartA) == 11 && StrNCompareA(szNameStartA, "Set-Cookie2", 11, TRUE) == 0)))
  {
    TAutoRefCounted<CHttpCookie> cCookie;
    HRESULT hRes;

    cCookie.Attach(MX_DEBUG_NEW CHttpCookie());
    if (!cCookie)
      return E_OUTOFMEMORY;
    hRes = cCookie->ParseFromResponseHeader(szValueStartA, (SIZE_T)(szValueEndA - szValueStartA));
    if (SUCCEEDED(hRes))
      hRes = cCookies.Merge(cCookie, TRUE);
    if (FAILED(hRes))
      return hRes;
  }
  else
  {
    TAutoRefCounted<CHttpHeaderBase> cNewHeader;

    //add new header
    hRes = CHttpHeaderBase::Create(szNameStartA, bActAsServer, &cNewHeader, (SIZE_T)(szNameEndA - szNameStartA));
    if (SUCCEEDED(hRes))
    {
      hRes = cNewHeader->Parse(szValueStartA, (SIZE_T)(szValueEndA - szValueStartA));
      if (SUCCEEDED(hRes))
        hRes = cHeaders.Merge(cNewHeader.Get(), FALSE);
    }
    if (FAILED(hRes))
      return hRes;

    //cache some header values
    if (StrCompareA(cNewHeader->GetHeaderName(), "Transfer-Encoding", TRUE) == 0)
    {
      CHttpHeaderGenTransferEncoding *lpHdr = (CHttpHeaderGenTransferEncoding*)(cNewHeader.Get());

      switch (lpHdr->GetEncoding())
      {
        case CHttpHeaderGenTransferEncoding::EncodingIdentity:
          break;

        case CHttpHeaderGenTransferEncoding::EncodingChunked:
          nHeaderFlags |= HEADER_FLAG_TransferEncodingChunked;
          break;

        default:
          return MX_E_Unsupported;
      }
    }
    else if (StrCompareA(cNewHeader->GetHeaderName(), "Content-Length", TRUE) == 0)
    {
      CHttpHeaderEntContentLength *lpHdr = (CHttpHeaderEntContentLength*)(cNewHeader.Get());

      sBody.nContentLength = lpHdr->GetLength();
    }
    else if (StrCompareA(cNewHeader->GetHeaderName(), "Content-Encoding", TRUE) == 0)
    {
      CHttpHeaderEntContentEncoding *lpHdr = (CHttpHeaderEntContentEncoding*)(cNewHeader.Get());

      switch (lpHdr->GetEncoding())
      {
        case CHttpHeaderEntContentEncoding::EncodingIdentity:
          break;

        case CHttpHeaderEntContentEncoding::EncodingGZip:
          nHeaderFlags |= HEADER_FLAG_ContentEncodingGZip;
          break;
        case CHttpHeaderEntContentEncoding::EncodingDeflate:
          nHeaderFlags |= HEADER_FLAG_ContentEncodingDeflate;
          break;

        default:
          return MX_E_Unsupported;
      }
    }
    else if (StrCompareA(cNewHeader->GetHeaderName(), "Connection", TRUE) == 0)
    {
      CHttpHeaderGenConnection *lpHdr = (CHttpHeaderGenConnection*)(cNewHeader.Get());

      if (lpHdr->HasConnection("upgrade") != FALSE)
      {
        nHeaderFlags |= HEADER_FLAG_ConnectionUpgrade;
      }
      else if (lpHdr->HasConnection("keep-alive") != FALSE)
      {
        nHeaderFlags |= HEADER_FLAG_ConnectionKeepAlive;
      }
      else if (lpHdr->HasConnection("close") != FALSE)
      {
        nHeaderFlags |= HEADER_FLAG_ConnectionClose;
      }
    }
    else if (StrCompareA(cNewHeader->GetHeaderName(), "Upgrade", TRUE) == 0)
    {
      CHttpHeaderGenUpgrade *lpHdr = (CHttpHeaderGenUpgrade*)(cNewHeader.Get());

      if (lpHdr->HasProduct("WebSocket") == 0)
      {
        nHeaderFlags |= HEADER_FLAG_UpgradeWebSocket;
      }
    }
    else if (bActAsServer != FALSE && StrCompareA(cNewHeader->GetHeaderName(), "Host", TRUE) == 0)
    {
      CHttpHeaderReqHost *lpHdr = (CHttpHeaderReqHost*)(cNewHeader.Get());

      hRes = sRequest.cUrl.SetHost(lpHdr->GetHost());
      if (SUCCEEDED(hRes))
        hRes = sRequest.cUrl.SetPort(lpHdr->GetPort());
      if (FAILED(hRes))
        return hRes;
    }
    else if (bActAsServer != FALSE && StrCompareA(cNewHeader->GetHeaderName(), "User-Agent", TRUE) == 0)
    {
      CHttpHeaderGeneric *lpHdr = (CHttpHeaderGeneric*)(cNewHeader.Get());

      sRequest.nBrowser = Http::GetBrowserFromUserAgent(lpHdr->GetValue());
    }
  }

  //done
  return S_OK;
}

HRESULT CHttpParser::ProcessContent(_In_ LPCVOID lpContent, _In_ SIZE_T nContentSize)
{
  BYTE aTempBuf[4096];
  SIZE_T nSize;
  HRESULT hRes;

  hRes = S_OK;
  switch (nHeaderFlags & HEADER_FLAG_MASK_ContentEncoding)
  {
    case HEADER_FLAG_ContentEncodingGZip:
    case HEADER_FLAG_ContentEncodingDeflate:
      //create decompressor if not done yet
      if (!(sBody.cDecoder))
      {
        sBody.cDecoder.Attach(MX_DEBUG_NEW CZipLib(FALSE));
        if (!(sBody.cDecoder))
          return E_OUTOFMEMORY;
        hRes = sBody.cDecoder->BeginDecompress();
        if (FAILED(hRes))
          return hRes;
      }
      //if data can be decompressed...
      if (sBody.cDecoder->HasDecompressEndOfStreamBeenReached() == FALSE)
      {
        hRes = sBody.cDecoder->DecompressStream(lpContent, nContentSize, NULL);
        //send available decompressed data to callback
        while (SUCCEEDED(hRes) && sBody.cDecoder->GetAvailableData() > 0)
        {
          nSize = sBody.cDecoder->GetData(aTempBuf, sizeof(aTempBuf));
          //parse body
          hRes = sBody.cParser->Parse(aTempBuf, nSize);
        }
      }
      break;

    default:
      //parse body
      hRes = sBody.cParser->Parse(lpContent, nContentSize);
      break;
  }
  //done
  return hRes;
}

HRESULT CHttpParser::FlushContent()
{
  BYTE aTempBuf[4096];
  SIZE_T nSize;
  HRESULT hRes;

  hRes = S_OK;
  if (sBody.cDecoder)
  {
    if (sBody.cDecoder->HasDecompressEndOfStreamBeenReached() == FALSE)
    {
      hRes = sBody.cDecoder->End();
      while (SUCCEEDED(hRes) && sBody.cDecoder->GetAvailableData() > 0)
      {
        nSize = sBody.cDecoder->GetData(aTempBuf, sizeof(aTempBuf));
        if (ShouldLog(1) != FALSE)
        {
          Log(L"HttpCommon(Body/0x%p): %.*S", this, (unsigned int)nSize, (LPCSTR)aTempBuf);
        }
        //parse body
        hRes = sBody.cParser->Parse(aTempBuf, nSize);
      }
    }
    else
    {
      hRes = sBody.cDecoder->End();
    }
  }
  //end body parsing
  if (SUCCEEDED(hRes))
    hRes = sBody.cParser->Parse(NULL, 0);
  //done
  return hRes;
}

} //namespace Internals

} //namespace MX

//-----------------------------------------------------------

static BOOL IsContentTypeHeader(_In_z_ LPCSTR szHeaderA)
{
  while (*szHeaderA == ' ' || *szHeaderA == '\t')
    szHeaderA++;
  return (MX::StrNCompareA(szHeaderA, "Content-Type:", 13) == 0) ? TRUE : FALSE;
}
