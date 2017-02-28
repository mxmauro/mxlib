/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2006-2015. All rights reserved.
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
#include "..\..\Include\Http\HttpCommon.h"
#include "..\..\Include\Strings\Utf8.h"
#include "..\..\Include\Debug.h"
#include "..\..\Include\Http\HttpBodyParserDefault.h"
#include "..\..\Include\Http\HttpBodyParserUrlEncodedForm.h"
#include "..\..\Include\Http\HttpBodyParserMultipartFormData.h"

//-----------------------------------------------------------

#define MAX_REQUEST_STATUS_LINE_LENGTH                  4096

#define HEADER_FLAG_TransferEncodingChunked           0x0001
#define HEADER_FLAG_ContentEncodingGZip               0x0002
#define HEADER_FLAG_ContentEncodingDeflate            0x0004

#define HEADER_FLAG_MASK_ContentEncoding              0x0006

#define HEADER_FLAG_UpgradeWebSocket                  0x0008
#define HEADER_FLAG_ConnectionUpgrade                 0x0010
#define HEADER_FLAG_ConnectionKeepAlive               0x0020
#define HEADER_FLAG_ConnectionClose                   0x0040

//-----------------------------------------------------------

#define MAX_VERB_LENGTH                                   24
static struct {
  LPCSTR szNameA;
  SIZE_T nNameLen;
} sVerbs[] = {
  { "CHECKOUT", 8 }, { "CONNECT",      7 }, { "COPY",   4 }, { "DELETE",   6 },
  { "GET",      3 }, { "HEAD",         4 }, { "LOCK",   4 }, { "MKCOL",    5 },
  { "MOVE",     4 }, { "MKACTIVITY",  10 }, { "MERGE",  5 }, { "M-SEARCH", 7 },
  { "NOTIFY",   6 }, { "OPTIONS",      7 }, { "PATCH",  5 }, { "POST",     4 },
  { "PROPFIND", 8 }, { "PROPPATCH",    9 }, { "PUT",    3 }, { "PURGE",    5 },
  { "REPORT",   6 }, { "SUBSCRIBE",    9 }, { "SEARCH", 6 }, { "TRACE",    5 },
  { "UNLOCK",   6 }, { "UNSUBSCRIBE", 11 }
};

//-----------------------------------------------------------

static BOOL IsContentTypeHeader(__in_z LPCSTR szHeaderA);

//-----------------------------------------------------------

namespace MX {

CHttpCommon::CHttpCommon(__in BOOL _bActAsServer, __in CPropertyBag &_cPropBag) : CBaseMemObj(), cPropBag(_cPropBag)
{
  bActAsServer = _bActAsServer;
  nMaxHeaderSize = 2048;
  //----
  ResetParser();
  return;
}

CHttpCommon::~CHttpCommon()
{
  RemoveAllHeaders();
  RemoveAllCookies();
  return;
}

VOID CHttpCommon::ResetParser()
{
  DWORD dw;

  sParser.nState = StateStart;
  sParser.cStrCurrLineA.Empty();
  sParser.nHeadersLen = 0;
  sParser.sChunk.nSize = 0;
  sParser.sChunk.nReaded = 0;
  sParser.nErrorCode = 0;
  //----
  sRequest.nHttpProtocol = 0;
  sRequest.nMethodIndex = (SIZE_T)-1;
  sRequest.cUrl.Reset();
  //----
  sResponse.nStatusCode = 0;
  sResponse.cStrReasonA.Empty();
  //----
  nHeaderFlags = 0;
  nContentLength = ULONGLONG_MAX;
  nIdentityBodyReadedContentLength = 0;
  RemoveAllHeaders();
  RemoveAllCookies();
  cBodyDecoder.Reset();
  cBodyParser.Release();

  //read properties from property bag
  cPropBag.GetDWord(MX_HTTP_MaxHeaderSize, dw, MX_HTTP_MaxHeaderSize_DEFVAL);
  if (dw < 2048)
    nMaxHeaderSize = 2048;
  else if (dw > 327680)
    nMaxHeaderSize = 327680;
  else
    nMaxHeaderSize = (int)((dw+16383) & (~16383));

  return;
}

#define BACKWARD_CHAR()     szDataA--
HRESULT CHttpCommon::Parse(__in LPCVOID lpData, __in SIZE_T nDataSize, __out SIZE_T &nDataUsed)
{
  LPCSTR szDataA, sA;
  HRESULT hRes;

  nDataUsed = 0;
  if (lpData == NULL && nDataSize > 0)
    return E_POINTER;
  if (sParser.nState == StateDone)
    return S_OK;
  if (sParser.nState == StateError)
    return MX_E_InvalidData;
  hRes = S_OK;
  for (szDataA = (LPCSTR)lpData; szDataA!=(LPCSTR)lpData+nDataSize; szDataA++)
  {
    switch (sParser.nState)
    {
      case StateStart:
        if (*szDataA == '\r' || *szDataA == '\n')
          break;
        sParser.nState = (bActAsServer != FALSE) ? StateRequestLine : StateStatusLine;
        BACKWARD_CHAR();
        break;

      case StateRequestLine:
      case StateStatusLine:
        if (*szDataA == '\r' || *szDataA == '\n')
        {
          if (sParser.nErrorCode == 0)
          {
            if (sParser.nState == StateRequestLine)
              hRes = ParseRequestLine((LPCSTR)(sParser.cStrCurrLineA));
            else
              hRes = ParseStatusLine((LPCSTR)(sParser.cStrCurrLineA));
          }
          //end of request/status line
          sParser.nState = (*szDataA == '\r') ? StateNearRequestOrStatusEnd : StateHeaderStart;
          sParser.cStrCurrLineA.Empty();
          break;
        }
        //is status/request line too long?
        if (sParser.cStrCurrLineA.GetLength() < MAX_REQUEST_STATUS_LINE_LENGTH)
        {
          //add character to line
          if (sParser.cStrCurrLineA.ConcatN(szDataA, 1) == FALSE)
          {
err_nomem:  hRes = E_OUTOFMEMORY;
            goto done;
          }
        }
        else
        {
          if (sParser.nErrorCode == 0)
            sParser.nErrorCode = 400; //header line too long
        }
        break;

      case StateNearRequestOrStatusEnd:
        if (*szDataA != '\n')
        {
err_invalid_data:
          hRes = MX_E_InvalidData;
          goto done;
        }
        sParser.nState = StateHeaderStart;
        break;

      case StateHeaderStart:
        if (*szDataA == '\r' || *szDataA == '\n')
        {
          //no more headers
          if (sParser.cStrCurrLineA.IsEmpty() == FALSE)
          {
            if (sParser.nErrorCode == 0 || IsContentTypeHeader((LPCSTR)(sParser.cStrCurrLineA)) != FALSE)
            {
              hRes = ParseHeader(sParser.cStrCurrLineA);
              if (FAILED(hRes))
                goto done;
            }
            sParser.cStrCurrLineA.Empty();
          }
          sParser.nState = StateHeadersEnd;
          if (*szDataA == '\n')
            goto headers_end_reached;
          break;
        }
        //are we continuing the last header?
        if (*szDataA == ' ' || *szDataA == '\t')
        {
          if (sParser.cStrCurrLineA.IsEmpty() != FALSE)
            goto err_invalid_data;
          sParser.nState = StateHeaderValueSpaceAfter;
          break;
        }
        //new header arrives, first check if we have a previous defined
        if (sParser.cStrCurrLineA.IsEmpty() == FALSE)
        {
          if (sParser.nErrorCode == 0 || IsContentTypeHeader((LPCSTR)(sParser.cStrCurrLineA)) != FALSE)
          {
            hRes = ParseHeader(sParser.cStrCurrLineA);
            if (FAILED(hRes))
              goto done;
          }
          sParser.cStrCurrLineA.Empty();
        }
        sParser.nState = StateHeaderName;
        BACKWARD_CHAR();
        break;

      case StateHeaderName:
        //check headers length
        if (sParser.nHeadersLen < nMaxHeaderSize)
          (sParser.nHeadersLen)++;
        else if (sParser.nErrorCode == 0)
          sParser.nErrorCode = 400; //headers too long
        //end of header name?
        if (*szDataA == ':')
        {
          if (sParser.cStrCurrLineA.IsEmpty() != FALSE)
            goto err_invalid_data;
          if (sParser.cStrCurrLineA.ConcatN(":", 1) == FALSE)
            goto err_nomem;
          sParser.nState = StateHeaderValueSpaceBefore;
          break;
        }
        //check for valid token char
        if (IsValidNameChar(*szDataA) == FALSE)
          goto err_invalid_data;
        if (sParser.cStrCurrLineA.GetLength() < MAX_REQUEST_STATUS_LINE_LENGTH)
        {
          //add character to line
          if (sParser.cStrCurrLineA.ConcatN(szDataA, 1) == FALSE)
            goto err_nomem;
        }
        else
        {
          if (sParser.nErrorCode == 0)
            sParser.nErrorCode = 400; //header line too long
        }
        //ignore IIS bug sending HTTP/ more than once
        sA = (LPCSTR)(sParser.cStrCurrLineA);
        if (sA[0] == 'H' && sA[1] == 'T' && sA[2] == 'T' && sA[3] == 'P' && sA[4] == '/')
        {
          sParser.nHeadersLen -= 5;
          sParser.nState = StateIgnoringHeader;
          sParser.cStrCurrLineA.Empty();
          break;
        }
        break;

      case StateHeaderValueSpaceBefore:
        if (*szDataA == ' ' || *szDataA == '\t')
          break; //ignore spaces
        //real header begin
        sParser.nState = StateHeaderValue;
        //fall into 'stHeaderValue'

      case StateHeaderValue:
on_header_value:
        if (*szDataA == '\r' || *szDataA == '\n')
        {
          if (sParser.nErrorCode == 0 || IsContentTypeHeader((LPCSTR)(sParser.cStrCurrLineA)) != FALSE)
          {
            hRes = ParseHeader(sParser.cStrCurrLineA);
            if (FAILED(hRes))
              goto done;
            sParser.cStrCurrLineA.Empty();
          }
          sParser.nState = (*szDataA == '\r') ? StateNearHeaderValueEnd : StateHeaderStart;
          break;
        }
        if (*szDataA == ' ' || *szDataA == '\t')
        {
          sParser.nState = StateHeaderValueSpaceAfter;
          break;
        }
        //check headers length and valid value char
        if (sParser.nHeadersLen < nMaxHeaderSize)
          (sParser.nHeadersLen)++;
        else if (sParser.nErrorCode == 0)
          sParser.nErrorCode = 400; //headers too long
        if (*szDataA == 0)
          goto err_invalid_data;
        if (sParser.cStrCurrLineA.GetLength() < MAX_REQUEST_STATUS_LINE_LENGTH)
        {
          //add character to line
          if (sParser.cStrCurrLineA.ConcatN(szDataA, 1) == FALSE)
            goto err_nomem;
        }
        else
        {
          if (sParser.nErrorCode == 0)
            sParser.nErrorCode = 400; //header line too long
        }
        break;

      case StateHeaderValueSpaceAfter:
        if (*szDataA == '\r' || *szDataA == '\n')
          goto on_header_value;
        if (*szDataA == ' ' || *szDataA == '\t')
          break;
        if (sParser.nHeadersLen < nMaxHeaderSize)
          (sParser.nHeadersLen)++;
        else if (sParser.nErrorCode == 0)
          sParser.nErrorCode = 400; //headers too long
        if (sParser.cStrCurrLineA.GetLength() < MAX_REQUEST_STATUS_LINE_LENGTH)
        {
          if (sParser.cStrCurrLineA.ConcatN(" ", 1) == FALSE)
            goto err_nomem;
        }
        else
        {
          if (sParser.nErrorCode == 0)
            sParser.nErrorCode = 400; //header line too long
        }
        sParser.nState = StateHeaderValue;
        goto on_header_value;

      case StateNearHeaderValueEnd:
        if (*szDataA != '\n')
          goto err_invalid_data;
        sParser.nState = StateHeaderStart;
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
            if (nContentLength != ULONGLONG_MAX || (nHeaderFlags & HEADER_FLAG_TransferEncodingChunked) != 0)
            {
              sParser.nState = StateBodyStart;
              sParser.nErrorCode = 400;
            }
            else
            {
              //no content... we are done
              sParser.nState = StateDone;
            }
            goto done;
          }
          if (StrCompareA(GetRequestMethod(), "POST") == 0)
          {
            //content-length must be specified
            if (nContentLength == ULONGLONG_MAX || (nHeaderFlags & HEADER_FLAG_TransferEncodingChunked) != 0)
            {
              sParser.nErrorCode = 400;
            }
          }
        }
        else
        {
          //1xx, 204 and 304 responses must not contain a body
          if (sResponse.nStatusCode < 200 || sResponse.nStatusCode == 204 || sResponse.nStatusCode == 304)
          {
            //content or chunked transfer is not allowed
            if (nContentLength != ULONGLONG_MAX || (nHeaderFlags & HEADER_FLAG_TransferEncodingChunked) != 0)
              goto err_invalid_data;
            //no content... we are done
            sParser.nState = StateDone;
            goto done;
          }
        }
        //if transfer encoding is set, then ignore content-length
        if ((nHeaderFlags & HEADER_FLAG_TransferEncodingChunked) != 0)
          nContentLength = ULONGLONG_MAX;
        //if no content we are done
        if (nContentLength == 0)
        {
          sParser.nState = StateDone;
          goto done;
        }
        sParser.nState = StateBodyStart;
#ifdef HTTP_DEBUG_OUTPUT
        MX::DebugPrint("HttpCommon(HeadersComplete/0x%p): BodyStart\n", this);
#endif //HTTP_DEBUG_OUTPUT
        goto done;

      case StateBodyStart:
        sParser.nState = ((nHeaderFlags & HEADER_FLAG_TransferEncodingChunked) != 0) ? StateChunkPreStart :
                                                                                       StateIdentityBodyStart;
        BACKWARD_CHAR();
        break;

      case StateIgnoringHeader:
        if (*szDataA == '\r' || *szDataA == '\n')
          sParser.nState = (*szDataA == '\r') ? StateNearIgnoringHeaderEnd : StateHeaderStart;
        break;

      case StateNearIgnoringHeaderEnd:
        if (*szDataA != '\n')
          goto err_invalid_data;
        sParser.nState = StateHeaderStart;
        break;

      case StateIdentityBodyStart:
        {
        SIZE_T nToRead;

        nToRead = nDataSize - (szDataA - (LPCSTR)lpData);
        if ((ULONGLONG)nToRead > nContentLength - nIdentityBodyReadedContentLength)
          nToRead = (SIZE_T)(nContentLength - nIdentityBodyReadedContentLength);
        hRes = ProcessContent(szDataA, nToRead);
        if (FAILED(hRes))
          goto done;
        szDataA += (SIZE_T)nToRead;
        szDataA--;
        nIdentityBodyReadedContentLength += (ULONGLONG)nToRead;
        if (nIdentityBodyReadedContentLength == nContentLength)
        {
          sParser.nState = StateDone;
          hRes = FlushContent();
          goto done;
        }
        }
        break;

      case StateChunkPreStart:
        if ((  *szDataA          < '0' ||   *szDataA          > '9') &&
            (((*szDataA) & 0xDF) < 'A' || ((*szDataA) & 0xDF) > 'F'))
          goto err_invalid_data;
        sParser.nState = StateChunkStart;
        sParser.sChunk.nSize = sParser.sChunk.nReaded = 0;
        //fall into 'stChunkStart'

      case StateChunkStart:
        if ((  *szDataA          >= '0' &&   *szDataA          <= '9') ||
            (((*szDataA) & 0xDF) >= 'A' && ((*szDataA) & 0xDF) <= 'F'))
        {
          if ((sParser.sChunk.nSize & 0xF000000000000000ui64) != 0)
          {
            hRes = MX_E_ArithmeticOverflow;
            goto done;
          }
          sParser.sChunk.nSize <<= 4;
          if (*szDataA >= '0' && *szDataA <= '9')
            sParser.sChunk.nSize |= (ULONGLONG)(*szDataA - '0');
          else
            sParser.sChunk.nSize |= (ULONGLONG)(((*szDataA) & 0xDF) - 'A' + 10);
          break;
        }
        //end of chunk size
        if (*szDataA == ' ' || *szDataA == '\t' || *szDataA == ';')
        {
          sParser.nState = StateChunkStartIgnoreExtension;
          break;
        }
        if (*szDataA == '\r')
        {
          sParser.nState = StateNearEndOfChunkStart;
          break;
        }
        if (*szDataA != '\n')
          goto err_invalid_data;
chunk_data_begin:
#ifdef HTTP_DEBUG_OUTPUT
        MX::DebugPrint("HttpCommon(Chunk/0x%p): %I64u\n", this, sParser.sChunk.nSize);
#endif //HTTP_DEBUG_OUTPUT
        sParser.nState = StateChunkData;
        if (sParser.sChunk.nSize == 0)
        {
          //the trailing CR/LF will be consumed by a new parse
          sParser.nState = StateDone;
          hRes = FlushContent();
          goto done;
        }
        break;

      case StateChunkStartIgnoreExtension:
        if (*szDataA == '\r')
          sParser.nState = StateNearEndOfChunkStart;
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
        if ((ULONGLONG)nToRead > sParser.sChunk.nSize - sParser.sChunk.nReaded)
          nToRead = (SIZE_T)(sParser.sChunk.nSize - sParser.sChunk.nReaded);
        hRes = ProcessContent(szDataA, nToRead);
        if (FAILED(hRes))
          goto done;
        szDataA += (SIZE_T)nToRead;
        szDataA--;
        sParser.sChunk.nReaded += (ULONGLONG)nToRead;
        nIdentityBodyReadedContentLength += (ULONGLONG)nToRead;
        if (sParser.sChunk.nReaded == sParser.sChunk.nSize)
          sParser.nState = StateChunkAfterData;
        }
        break;

      case StateChunkAfterData:
        if (*szDataA == '\r')
          sParser.nState = StateNearEndOfChunkAfterData;
        else if (*szDataA == '\n')
          sParser.nState = StateChunkPreStart;
        else
          goto err_invalid_data;
        break;

      case StateNearEndOfChunkAfterData:
        if (*szDataA != '\n')
          goto err_invalid_data;
        sParser.nState = StateChunkPreStart;
        break;

      default:
        MX_ASSERT(FALSE);
        break;
    }
  }

done:
  if (FAILED(hRes))
    sParser.nState = StateError;
  nDataUsed = (SIZE_T)szDataA - (SIZE_T)lpData;
  return hRes;
}
#undef BACKWARD_CHAR

HRESULT CHttpCommon::AddHeader(__in_z LPCSTR szNameA, __out_opt CHttpHeaderBase **lplpHeader)
{
  TAutoDeletePtr<CHttpHeaderBase> cHeader;
  HRESULT hRes;

  RemoveHeader(szNameA); //remove any existing one
  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  hRes = CHttpHeaderBase::Create(szNameA, bActAsServer, &cHeader);
  if (SUCCEEDED(hRes))
  {
    if (cHeaders.AddElement(cHeader.Get()) != FALSE)
    {
      if (lplpHeader != NULL)
        *lplpHeader = cHeader.Get();
      cHeader.Detach();
    }
    else
    {
      hRes = E_OUTOFMEMORY;
    }
  }
  //done
  return hRes;
}

HRESULT CHttpCommon::AddHeader(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA, __in_opt SIZE_T nValueLen,
                               __out_opt CHttpHeaderBase **lplpHeader)
{
  CHttpHeaderBase *lpHeader;
  HRESULT hRes;

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  hRes = AddHeader(szNameA, &lpHeader);
  if (SUCCEEDED(hRes))
  {
    if (nValueLen == (SIZE_T)-1)
    {
      if (szValueA[0] != 0)
        hRes = lpHeader->Parse(szValueA);
    }
    else if (nValueLen > 0)
    {
      CStringA cStrTempA;

      if (cStrTempA.CopyN(szValueA, nValueLen) != FALSE)
        hRes = lpHeader->Parse((LPCSTR)cStrTempA);
      else
        hRes = E_OUTOFMEMORY;
    }
  }
  //done
  if (SUCCEEDED(hRes))
  {
    if (lplpHeader != NULL)
      *lplpHeader = lpHeader;
  }
  else
  {
    RemoveHeader(szNameA);
  }
  return hRes;
}

HRESULT CHttpCommon::AddHeader(__in_z LPCSTR szNameA, __in_z LPCWSTR szValueW, __in_opt SIZE_T nValueLen,
                               __out_opt CHttpHeaderBase **lplpHeader)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (lplpHeader != NULL)
    *lplpHeader = NULL;
  if (szNameA == NULL)
    return E_POINTER;
  if (nValueLen == (SIZE_T)-1)
    nValueLen = (szValueW != NULL) ? StrLenW(szValueW) : 0;
  if (nValueLen > 0)
  {
    hRes = Utf8_Encode(cStrTempA, szValueW, nValueLen);
    if (FAILED(hRes))
      return hRes;
  }
  return AddHeader(szNameA, (LPSTR)cStrTempA, (SIZE_T)-1, lplpHeader);
}

HRESULT CHttpCommon::RemoveHeader(__in_z LPCSTR szNameA)
{
  SIZE_T i, nCount;

  if (szNameA == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  nCount = cHeaders.GetCount();
  for (i=0; i<nCount; i++)
  {
    if (StrCompareA(szNameA, cHeaders[i]->GetName(), TRUE) == 0)
    {
      cHeaders.RemoveElementAt(i);
      return S_OK;
    }
  }
  return MX_E_NotFound;
}

VOID CHttpCommon::RemoveAllHeaders()
{
  cHeaders.RemoveAllElements();
  return;
}

SIZE_T CHttpCommon::GetHeadersCount() const
{
  return cHeaders.GetCount();
}

CHttpHeaderBase* CHttpCommon::GetHeader(__in SIZE_T nIndex) const
{
  return (nIndex < cHeaders.GetCount()) ? cHeaders.GetElementAt(nIndex) : NULL;
}

CHttpHeaderBase* CHttpCommon::GetHeader(__in_z LPCSTR szNameA) const
{
  SIZE_T i, nCount;

  if (szNameA != NULL && szNameA[0] != 0)
  {
    nCount = cHeaders.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (StrCompareA(szNameA, cHeaders[i]->GetName(), TRUE) == 0)
        return cHeaders.GetElementAt(i);
    }
  }
  return NULL;
}

HRESULT CHttpCommon::AddCookie(__in CHttpCookie &cSrc)
{
  TAutoDeletePtr<CHttpCookie> cNewCookie;
  HRESULT hRes;

  if (cSrc.GetName()[0] == 0)
    return E_INVALIDARG;
  //clone cookie
  cNewCookie.Attach(MX_DEBUG_NEW CHttpCookie());
  if (!cNewCookie)
    return E_OUTOFMEMORY;
  hRes = (*(cNewCookie.Get()) = cSrc);
  if (FAILED(hRes))
    return hRes;
  //add to array
  if (cCookies.AddElement(cNewCookie.Get()) == FALSE)
    return E_OUTOFMEMORY;
  cNewCookie.Detach();
  return S_OK;
}

HRESULT CHttpCommon::AddCookie(__in CHttpCookieArray &cSrc)
{
  CHttpCookie *lpCookie;
  SIZE_T i, nCount;
  HRESULT hRes;

  nCount = cSrc.GetCount();
  for (i=0; i<nCount; i++)
  {
    lpCookie = cSrc.GetElementAt(i);
    if (lpCookie != NULL)
    {
      hRes = AddCookie(*lpCookie);
      if (FAILED(hRes))
        return hRes;
    }
  }
  return S_OK;
}

HRESULT CHttpCommon::AddCookie(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA, __in_z_opt LPCSTR szDomainA,
                               __in_z_opt LPCSTR szPathA, __in_opt const CDateTime *lpDate, __in_opt BOOL bIsSecure,
                               __in_opt BOOL bIsHttpOnly)
{
  CHttpCookie cCookie;
  HRESULT hRes;

  hRes = cCookie.SetName(szNameA);
  if (SUCCEEDED(hRes))
    hRes = cCookie.SetValue(szValueA);
  if (SUCCEEDED(hRes) && szDomainA != NULL)
    hRes = cCookie.SetDomain(szDomainA);
  if (SUCCEEDED(hRes) && szPathA != NULL)
    hRes = cCookie.SetPath(szPathA);
  if (SUCCEEDED(hRes))
  {
    if (lpDate != NULL)
      cCookie.SetExpireDate(lpDate);
    cCookie.SetSecureFlag(bIsSecure);
    cCookie.SetHttpOnlyFlag(bIsHttpOnly);
  }
  if (SUCCEEDED(hRes))
    hRes = AddCookie(cCookie);
  return hRes;
}

HRESULT CHttpCommon::AddCookie(__in_z LPCWSTR szNameW, __in_z LPCWSTR szValueW, __in_z_opt LPCWSTR szDomainW,
                               __in_z_opt LPCWSTR szPathW, __in_opt const CDateTime *lpDate, __in_opt BOOL bIsSecure,
                               __in_opt BOOL bIsHttpOnly)
{
  CHttpCookie cCookie;
  HRESULT hRes;

  hRes = cCookie.SetName(szNameW);
  if (SUCCEEDED(hRes))
    hRes = cCookie.SetValue(szValueW);
  if (SUCCEEDED(hRes) && szDomainW != NULL)
    hRes = cCookie.SetDomain(szDomainW);
  if (SUCCEEDED(hRes) && szPathW != NULL)
    hRes = cCookie.SetPath(szPathW);
  if (SUCCEEDED(hRes))
  {
    if (lpDate != NULL)
      cCookie.SetExpireDate(lpDate);
    cCookie.SetSecureFlag(bIsSecure);
    cCookie.SetHttpOnlyFlag(bIsHttpOnly);
  }
  if (SUCCEEDED(hRes))
    hRes = AddCookie(cCookie);
  return hRes;
}

HRESULT CHttpCommon::RemoveCookie(__in_z LPCSTR szNameA)
{
  SIZE_T i, nCount;

  if (szNameA == NULL)
    return E_POINTER;
  if (*szNameA == 0)
    return E_INVALIDARG;
  nCount = cCookies.GetCount();
  for (i=0; i<nCount; i++)
  {
    if (StrCompareA(szNameA, cCookies[i]->GetName(), TRUE) == 0)
    {
      cCookies.RemoveElementAt(i);
      return S_OK;
    }
  }
  return MX_E_NotFound;
}

HRESULT CHttpCommon::RemoveCookie(__in_z LPCWSTR szNameW)
{
  SIZE_T i, nCount;

  if (szNameW == NULL)
    return E_POINTER;
  if (*szNameW == 0)
    return E_INVALIDARG;
  nCount = cCookies.GetCount();
  for (i=0; i<nCount; i++)
  {
    if (StrCompareAW(cCookies[i]->GetName(), szNameW, TRUE) == 0)
    {
      cCookies.RemoveElementAt(i);
      return S_OK;
    }
  }
  return MX_E_NotFound;
}

VOID CHttpCommon::RemoveAllCookies()
{
  cCookies.RemoveAllElements();
  return;
}

CHttpCookie* CHttpCommon::GetCookie(__in SIZE_T nIndex) const
{
  return (nIndex < cCookies.GetCount()) ? cCookies.GetElementAt(nIndex) : NULL;
}

CHttpCookie* CHttpCommon::GetCookie(__in_z LPCSTR szNameA) const
{
  SIZE_T i, nCount;

  if (szNameA == NULL || *szNameA == 0)
    return NULL;
  nCount = cCookies.GetCount();
  for (i=0; i<nCount; i++)
  {
    if (StrCompareA(szNameA, cCookies[i]->GetName(), TRUE) == 0)
      return cCookies.GetElementAt(i);
  }
  return NULL;
}

CHttpCookie* CHttpCommon::GetCookie(__in_z LPCWSTR szNameW) const
{
  SIZE_T i, nCount;

  if (szNameW == NULL || *szNameW == 0)
    return NULL;
  nCount = cCookies.GetCount();
  for (i=0; i<nCount; i++)
  {
    if (StrCompareAW(cCookies[i]->GetName(), szNameW, TRUE) == 0)
      return cCookies.GetElementAt(i);
  }
  return NULL;
}

CHttpCookieArray* CHttpCommon::GetCookies() const
{
  return const_cast<CHttpCookieArray*>(&cCookies);
}

SIZE_T CHttpCommon::GetCookiesCount() const
{
  return cCookies.GetCount();
}

CHttpCommon::eState CHttpCommon::GetParserState() const
{
  return sParser.nState;
}

HRESULT CHttpCommon::SetBodyParser(__in CHttpBodyParserBase *lpParser, __in CPropertyBag &cPropBag)
{
  if (sParser.nState != StateBodyStart)
    return E_FAIL;
  MX_ASSERT(lpParser != NULL);
  cBodyParser = lpParser;
  return lpParser->Initialize(cPropBag, *this);
}

CHttpBodyParserBase* CHttpCommon::GetBodyParser() const
{
  CHttpBodyParserBase *lpParser;

  lpParser = cBodyParser.Get();
  if (lpParser != NULL)
    lpParser->AddRef();
  return lpParser;
}

int CHttpCommon::GetRequestVersionMajor() const
{
  return (int)((sRequest.nHttpProtocol >> 8) & 0xFF);
}

int CHttpCommon::GetRequestVersionMinor() const
{
  return (int)(sRequest.nHttpProtocol & 0xFF);
}

LPCSTR CHttpCommon::GetRequestMethod() const
{
  return (sRequest.nMethodIndex != (SIZE_T)-1) ? sVerbs[sRequest.nMethodIndex].szNameA : "";
}

CUrl* CHttpCommon::GetRequestUri() const
{
  return const_cast<CUrl*>(&(sRequest.cUrl));
}

BOOL CHttpCommon::IsKeepAliveRequest() const
{
  return ((nHeaderFlags & HEADER_FLAG_ConnectionKeepAlive) != 0) ? TRUE : FALSE;
}

LONG CHttpCommon::GetResponseStatus() const
{
  return sResponse.nStatusCode;
}

LPCSTR CHttpCommon::GetResponseReasonA() const
{
  return (LPSTR)(sResponse.cStrReasonA);
}

CHttpBodyParserBase* CHttpCommon::GetDefaultBodyParser()
{
  CHttpHeaderEntContentType *lpHeader;

  lpHeader = GetHeader<CHttpHeaderEntContentType>();
  if (lpHeader != NULL)
  {
    if (StrCompareA(lpHeader->GetType(), "application/x-www-form-urlencoded") == 0)
      return MX_DEBUG_NEW CHttpBodyParserUrlEncodedForm();
    if (StrCompareA(lpHeader->GetType(), "multipart/form-data") == 0)
      return MX_DEBUG_NEW CHttpBodyParserMultipartFormData();
  }
  return MX_DEBUG_NEW CHttpBodyParserDefault();
}

HRESULT CHttpCommon::ParseRequestLine(__in_z LPCSTR szLineA)
{
  SIZE_T nMethod, nUriLength;
  LPCSTR szUriStartA;
  HRESULT hRes;

  //get method
  for (nMethod=0; nMethod<MX_ARRAYLEN(sVerbs); nMethod++)
  {
    if (StrNCompareA(szLineA, sVerbs[nMethod].szNameA, sVerbs[nMethod].nNameLen, FALSE) == 0)
      break;
  }
  if (nMethod >= MX_ARRAYLEN(sVerbs))
    return MX_E_InvalidData;
  sRequest.nMethodIndex = nMethod;
  szLineA += sVerbs[nMethod].nNameLen;
  if (*szLineA != ' ')
    return MX_E_InvalidData;
  //parse uri
  szUriStartA = szLineA+1;
  for (nUriLength=0; szUriStartA[nUriLength]==9 || szUriStartA[nUriLength]==12 ||
                     (szUriStartA[nUriLength]>=33 && szUriStartA[nUriLength]<=126); nUriLength++);
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
  while (*szLineA==' ' || *szLineA=='\t')
    szLineA++;
  //check http version
  if (szLineA[0] != 'H' || szLineA[1] != 'T' || szLineA[2] != 'T' || szLineA[3] != 'P' ||
      szLineA[4] != '/' || szLineA[5] != '1' || szLineA[6] != '.' ||
      (szLineA[7] != '0' && szLineA[7] != '1'))
    return MX_E_InvalidData;
  sRequest.nHttpProtocol = ((ULONG)(szLineA[5] - '0') << 8) | (ULONG)(szLineA[7] - '0');
  //ignore blanks at the end
  for (szLineA+=8; *szLineA==' ' || *szLineA=='\t'; szLineA++);
  return (*szLineA == 0) ? S_OK : MX_E_InvalidData;
}

HRESULT CHttpCommon::ParseStatusLine(__in_z LPCSTR szLineA)
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
    return MX_E_InvalidData;
  //get status code
  sResponse.nStatusCode = (LONG)(szLineA[ 9] - '0') * 100 +
                          (LONG)(szLineA[10] - '0') * 10 +
                          (LONG)(szLineA[11] - '0');
  //skip blanks
  for (szLineA+=12; *szLineA==' ' || *szLineA=='\t'; szLineA++);
  //get reason
  if (sResponse.cStrReasonA.Copy(szLineA) == FALSE)
    return E_OUTOFMEMORY;
  return S_OK;
}

HRESULT CHttpCommon::ParseHeader(__inout CStringA &cStrLineA)
{
  TAutoDeletePtr<CHttpHeaderBase> cHeader;
  LPSTR szLineA, szNameStartA, szNameEndA, szValueStartA, szValueEndA;
  CHAR chA[2];
  HRESULT hRes;

  szLineA = (LPSTR)cStrLineA;
  //skip blanks
  while (*szLineA==' ' || *szLineA=='\t')
    szLineA++;
#ifdef HTTP_DEBUG_OUTPUT
  MX::DebugPrint("HttpCommon(Header/0x%p): %s\n", this, szLineA);
#endif //HTTP_DEBUG_OUTPUT
  //get header name
  szNameStartA = szNameEndA = szLineA;
  while (*szNameEndA != 0 && *szNameEndA != ':')
    szNameEndA++;
  if (*szNameEndA != ':')
    return MX_E_InvalidData;
  //skip blanks
  szLineA = szNameEndA+1;
  while (*szLineA==' ' || *szLineA=='\t')
    szLineA++;
  //get header value
  szValueStartA = szValueEndA = szLineA;
  while (*szValueEndA != 0)
    szValueEndA++;
  while (szValueEndA > szValueStartA && (*(szValueEndA-1)==' ' || *(szValueEndA-1)=='\t'))
    szValueEndA--;

  //is a cookie?
  if (bActAsServer != FALSE &&
      (((SIZE_T)(szNameEndA-szNameStartA) == 6 && StrNCompareA(szNameStartA, "Cookie", 6, TRUE) == 0) ||
       ((SIZE_T)(szNameEndA-szNameStartA) == 7 && StrNCompareA(szNameStartA, "Cookie2", 7, TRUE) == 0)))
  {
    CHttpCookieArray cCookieArray;

    hRes = cCookieArray.ParseFromRequestHeader(szValueStartA, (SIZE_T)(szValueEndA-szValueStartA));
    if (SUCCEEDED(hRes))
      hRes = cCookies.Update(cCookieArray, FALSE);
    if (FAILED(hRes))
      return hRes;
  }
  else if (bActAsServer == FALSE &&
           (((SIZE_T)(szNameEndA-szNameStartA) == 10 && StrNCompareA(szNameStartA, "Set-Cookie", 10, TRUE) == 0) ||
            ((SIZE_T)(szNameEndA-szNameStartA) == 11 && StrNCompareA(szNameStartA, "Set-Cookie2", 11, TRUE) == 0)))
  {
    CHttpCookie cCookie;
    HRESULT hRes;

    hRes = cCookie.ParseFromResponseHeader(szValueStartA, (SIZE_T)(szValueEndA-szValueStartA));
    if (SUCCEEDED(hRes))
      hRes = cCookies.Update(cCookie, TRUE);
    if (FAILED(hRes))
      return hRes;
  }
  else
  {
    CHttpHeaderBase *lpHeader = NULL;

    //create new header
    chA[0] = *szNameEndA;
    chA[1] = *szValueEndA;
    *szNameEndA = *szValueEndA = 0;
    hRes = CHttpHeaderBase::Create(szNameStartA, bActAsServer, &cHeader);
    if (SUCCEEDED(hRes))
      hRes = cHeader->Parse(szValueStartA);
    *szNameEndA = chA[0];
    *szValueEndA = chA[1];
    if (SUCCEEDED(hRes))
    {
      lpHeader = cHeader.Get();
      if (cHeaders.AddElement(lpHeader) != FALSE)
        cHeader.Detach();
      else
        hRes = E_OUTOFMEMORY;
    }
    if (FAILED(hRes))
      return hRes;
    //cache some header values
    if (StrCompareA(lpHeader->GetName(), "Transfer-Encoding", TRUE) == 0)
    {
      CHttpHeaderGenTransferEncoding *lpHdr = (CHttpHeaderGenTransferEncoding*)lpHeader;

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
    else if (StrCompareA(lpHeader->GetName(), "Content-Length", TRUE) == 0)
    {
      CHttpHeaderEntContentLength *lpHdr = (CHttpHeaderEntContentLength*)lpHeader;

      nContentLength = lpHdr->GetLength();
    }
    else if (StrCompareA(lpHeader->GetName(), "Content-Encoding", TRUE) == 0)
    {
      CHttpHeaderEntContentEncoding *lpHdr = (CHttpHeaderEntContentEncoding*)lpHeader;

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
    else if (StrCompareA(lpHeader->GetName(), "Connection", TRUE) == 0)
    {
      CHttpHeaderGenConnection *lpHdr = (CHttpHeaderGenConnection*)lpHeader;

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
    else if (StrCompareA(lpHeader->GetName(), "Upgrade", TRUE) == 0)
    {
      CHttpHeaderGenUpgrade *lpHdr = (CHttpHeaderGenUpgrade*)lpHeader;

      if (lpHdr->HasProduct("WebSocket") == 0)
      {
        nHeaderFlags |= HEADER_FLAG_UpgradeWebSocket;
      }
    }
    else if (bActAsServer != FALSE && StrCompareA(lpHeader->GetName(), "Host", TRUE) == 0)
    {
      CHttpHeaderReqHost *lpHdr = (CHttpHeaderReqHost*)lpHeader;

      hRes = sRequest.cUrl.SetHost(lpHdr->GetHost());
      if (SUCCEEDED(hRes))
        hRes = sRequest.cUrl.SetPort(lpHdr->GetPort());
      if (FAILED(hRes))
        return hRes;
    }
  }
  //done
  return S_OK;
}

HRESULT CHttpCommon::ProcessContent(__in LPCVOID lpContent, __in SIZE_T nContentSize)
{
  BYTE aTempBuf[4096];
  SIZE_T nSize;
  HRESULT hRes;

  if (!cBodyParser)
    return MX_E_NotReady;
  hRes = S_OK;
  switch (nHeaderFlags & HEADER_FLAG_MASK_ContentEncoding)
  {
    case HEADER_FLAG_ContentEncodingGZip:
    case HEADER_FLAG_ContentEncodingDeflate:
      //create decompressor if not done yet
      if (!cBodyDecoder)
      {
        cBodyDecoder.Attach(MX_DEBUG_NEW CZipLib(FALSE));
        if (!cBodyDecoder)
          return E_OUTOFMEMORY;
        hRes = cBodyDecoder->BeginDecompress();
        if (FAILED(hRes))
          return hRes;
      }
      //if data can be decompressed...
      if (cBodyDecoder->HasDecompressEndOfStreamBeenReached() == FALSE)
      {
        hRes = cBodyDecoder->DecompressStream(lpContent, nContentSize, NULL);
        //send available decompressed data to callback
        while (SUCCEEDED(hRes) && cBodyDecoder->GetAvailableData() > 0)
        {
          nSize = cBodyDecoder->GetData(aTempBuf, sizeof(aTempBuf));
          //parse body
          hRes = cBodyParser->Parse(aTempBuf, nSize);
        }
      }
      break;

    default:
      //parse body
      hRes = cBodyParser->Parse(lpContent, nContentSize);
      break;
  }
  return hRes;
}

HRESULT CHttpCommon::FlushContent()
{
  BYTE aTempBuf[4096];
  SIZE_T nSize;
  HRESULT hRes;

  if (cBodyDecoder)
  {
    if (cBodyDecoder->HasDecompressEndOfStreamBeenReached() == FALSE)
    {
      hRes = cBodyDecoder->End();
      while (SUCCEEDED(hRes) && cBodyDecoder->GetAvailableData() > 0)
      {
        nSize = cBodyDecoder->GetData(aTempBuf, sizeof(aTempBuf));
#ifdef HTTP_DEBUG_OUTPUT
        MX::DebugPrint("HttpCommon(Body/0x%p): %.*s\n", this, nSize, (LPSTR)aTempBuf);
#endif //HTTP_DEBUG_OUTPUT
        //parse body
        hRes = cBodyParser->Parse(aTempBuf, nSize);
      }
    }
    else
    {
      hRes = cBodyDecoder->End();
    }
  }
  else
  {
    hRes = S_OK;
  }
  //end body parsing
  if (SUCCEEDED(hRes))
    hRes = cBodyParser->Parse(NULL, 0);
  return hRes;
}

BOOL CHttpCommon::IsValidVerb(__in_z LPCSTR szVerbA)
{
  if (szVerbA != NULL && szVerbA[0] != 0)
  {
    SIZE_T nMethod;

    for (nMethod=0; nMethod<MX_ARRAYLEN(sVerbs); nMethod++)
    {
      if (StrNCompareA(szVerbA, sVerbs[nMethod].szNameA, sVerbs[nMethod].nNameLen, FALSE) == 0)
        return TRUE;
    }
  }
  return FALSE;
}

} //namespace MX

//-----------------------------------------------------------

static BOOL IsContentTypeHeader(__in_z LPCSTR szHeaderA)
{
  while (*szHeaderA==' ' || *szHeaderA=='\t')
    szHeaderA++;
  return (MX::StrNCompareA(szHeaderA, "Content-Type:", 13) == 0) ? TRUE : FALSE;
}
