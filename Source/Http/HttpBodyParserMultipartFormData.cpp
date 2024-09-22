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
#include "..\..\Include\Http\HttpBodyParserMultipartFormData.h"
#include "..\..\Include\FnvHash.h"
#include "..\..\Include\Http\Url.h"
#include "..\..\Include\Strings\Utf8.h"

//-----------------------------------------------------------

#define MAX_HEADER_LINE                                 4096
#define MAX_HEADER_LENGTH                              65536

//-----------------------------------------------------------

namespace MX {

CHttpBodyParserMultipartFormData::CHttpBodyParserMultipartFormData(
                                                 _In_ OnDownloadStartedCallback _cDownloadStartedCallback,
                                                 _In_opt_ LPVOID _lpUserData, _In_ DWORD _dwMaxFieldSize,
                                                 _In_ ULONGLONG _ullMaxFileSize, _In_ DWORD _dwMaxFilesCount) :
                                  CHttpBodyParserFormBase(), CNonCopyableObj()
{
  cDownloadStartedCallback = _cDownloadStartedCallback;
  lpUserData = _lpUserData;
  dwMaxFieldSize = _dwMaxFieldSize;
  if (dwMaxFieldSize < 32768)
    dwMaxFieldSize = 32768;
  ullMaxFileSize = _ullMaxFileSize;
  dwMaxFilesCount = _dwMaxFilesCount;
  return;
}

CHttpBodyParserMultipartFormData::~CHttpBodyParserMultipartFormData()
{
  return;
}

HRESULT CHttpBodyParserMultipartFormData::Initialize(_In_ Internals::CHttpParser &cHttpParser)
{
  CHttpHeaderEntContentType *lpHeader;
  LPCWSTR szBoundaryW;

  lpHeader = cHttpParser.Headers().Find<CHttpHeaderEntContentType>();
  if (lpHeader == NULL)
    return MX_E_InvalidData;
  szBoundaryW = lpHeader->GetParamValue("boundary");
  if (szBoundaryW == NULL || *szBoundaryW == 0)
    return MX_E_InvalidData;
  if (cStrBoundaryA.Copy(szBoundaryW) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

#define BACKWARD_CHAR()     szDataA--
HRESULT CHttpBodyParserMultipartFormData::Parse(_In_opt_ LPCVOID lpData, _In_opt_ SIZE_T nDataSize)
{
  CStringW cStrTempW;
  LPCSTR szDataA;
  DWORD dw;
  SIZE_T k;
  HRESULT hRes;

  if (lpData == NULL && nDataSize > 0)
    return E_POINTER;
  if (sParser.nState == eState::Done)
    return S_OK;
  if (sParser.nState == eState::Error)
    return MX_E_InvalidData;

  //end of parsing?
  if (lpData == NULL)
  {
    if (sParser.nState != eState::Done)
    {
      sParser.nState = eState::Error;
      return MX_E_InvalidData;
    }
    sParser.nState = eState::Done;
    return S_OK;
  }

  //begin
  hRes = S_OK;
  //process data
  for (szDataA = (LPCSTR)lpData; szDataA != (LPCSTR)lpData+nDataSize; szDataA++)
  {
    switch (sParser.nState)
    {
      case eState::Boundary:
        if (*szDataA == '\r' || *szDataA == '\n')
          break;

        if (sParser.nBoundaryPos < 2)
        {
          if (*szDataA != '-')
          {
err_invalid_data:
            hRes = MX_E_InvalidData;
            goto done;
          }
        }
        else
        {
          LPCSTR sA = (LPCSTR)cStrBoundaryA;
          if (*szDataA != sA[sParser.nBoundaryPos - 2])
            goto err_invalid_data;
          if (sA[sParser.nBoundaryPos - 1] == 0)
          {
            sParser.nState = eState::BoundaryAfter;
            break;
          }
        }
        sParser.nBoundaryPos++;
        break;

      case eState::BoundaryAfter:
        if (*szDataA == '-')
        {
          sParser.nState = eState::BoundaryEndCheck;
          break;
        }
        sParser.nState = eState::BoundaryAfter2;
        //fall into 'StateBoundaryAfter2'

      case eState::BoundaryAfter2:
        if (*szDataA == ' ' || *szDataA == '\t')
          break;
        if (*szDataA != '\r')
          goto err_invalid_data;
        sParser.nState = eState::BoundaryAfterEnd;
        break;

      case eState::BoundaryAfterEnd:
        if (*szDataA != '\n')
          goto err_invalid_data;

        //start of a header block
        sParser.nState = eState::HeaderStart;
        sParser.dwHeadersLen = 0;
        sParser.sCurrentBlock.sContentDisposition.cStrNameW.Empty();
        sParser.sCurrentBlock.sContentDisposition.bHasFileName = FALSE;
        sParser.sCurrentBlock.sContentDisposition.cStrFileNameW.Empty();
        sParser.sCurrentBlock.cStrContentTypeA.Empty();
        sParser.sCurrentBlock.bContentTransferEncoding = FALSE;
        sParser.cFileH.Close();
        sParser.nUsedTempBuf = 0;
        sParser.nFileUploadSize = 0ui64;
        break;

      case eState::BoundaryEndCheck:
        //check second dash
        if (*szDataA != '-')
          goto err_invalid_data;
        sParser.nState = eState::BoundaryEndCheckAfterDashes;
        break;

      case eState::BoundaryEndCheckAfterDashes:
        if (*szDataA == ' ' || *szDataA == '\t')
          break;
        if (*szDataA != '\r')
          goto err_invalid_data;
        sParser.nState = eState::BoundaryEndCheckAfterDashes2;
        break;

      case eState::BoundaryEndCheckAfterDashes2:
        if (*szDataA != '\n')
          goto err_invalid_data;
        sParser.nState = eState::Done;
        break;

      case eState::HeaderStart:
        if (*szDataA == '\r')
        {
          //no more headers
          if (sParser.cStrCurrLineA.IsEmpty() == FALSE)
          {
            hRes = ParseHeader(sParser.cStrCurrLineA);
            if (FAILED(hRes))
              goto done;
            sParser.cStrCurrLineA.Empty();
          }
          sParser.nState = eState::HeadersEnding;
          break;
        }

        //are we continuing the last header?
        if (*szDataA == ' ' || *szDataA == '\t')
        {
          if (sParser.cStrCurrLineA.IsEmpty() != FALSE)
            goto err_invalid_data;
          sParser.nState = eState::HeaderValue;
          BACKWARD_CHAR();
          break;
        }

        //new header arrives, first check if we have a previous defined
        if (sParser.cStrCurrLineA.IsEmpty() == FALSE)
        {
          hRes = ParseHeader(sParser.cStrCurrLineA);
          if (FAILED(hRes))
            goto done;
          sParser.cStrCurrLineA.Empty();
        }
        sParser.nState = eState::HeaderName;
        //fall into 'eState::HeaderName'

      case eState::HeaderName:
        //check headers length
        if (sParser.dwHeadersLen >= MAX_HEADER_LENGTH)
        {
err_line_too_long:
          hRes = MX_E_BadLength; //header line too long
          goto done;
        }
        (sParser.dwHeadersLen)++;

        //end of header name?
        if (*szDataA == ':')
        {
          //no need to insert pending space
          if (sParser.cStrCurrLineA.IsEmpty() != FALSE)
            goto err_invalid_data;

          if (sParser.cStrCurrLineA.ConcatN(":", 1) == FALSE)
          {
err_nomem:  hRes = E_OUTOFMEMORY;
            goto done;
          }
          sParser.nState = eState::HeaderValue;
          break;
        }

        //check for valid token char
        if (Http::IsValidNameChar(*szDataA) == FALSE)
          goto err_invalid_data;
        if (sParser.cStrCurrLineA.GetLength() > MAX_HEADER_LINE)
          goto err_line_too_long;

        //add character to line
        if (sParser.cStrCurrLineA.ConcatN(szDataA, 1) == FALSE)
          goto err_nomem;
        break;

      case eState::HeaderValue:
        //check headers length
        if (sParser.dwHeadersLen >= MAX_HEADER_LENGTH)
          goto err_line_too_long;
        (sParser.dwHeadersLen)++;

        if (*szDataA == '\r')
        {
          sParser.nState = eState::HeaderValueEnding;
          break;
        }

        //check valid value char
        if (*szDataA == 0)
          goto err_invalid_data;

        //and append
        if (sParser.cStrCurrLineA.GetLength() > MAX_HEADER_LINE)
          goto err_line_too_long;
        if (sParser.cStrCurrLineA.ConcatN(szDataA, 1) == FALSE)
          goto err_nomem;
        break;

      case eState::HeaderValueEnding:
        if (*szDataA != '\n')
          goto err_invalid_data;

        sParser.nState = eState::HeaderStart;
        break;

      case eState::HeadersEnding:
        if (*szDataA != '\n')
          goto err_invalid_data;

        //verify if required headers are set
        if (sParser.sCurrentBlock.sContentDisposition.cStrNameW.IsEmpty() != FALSE)
          goto err_invalid_data; //requires header not set

        //if dealing with a file...
        if (sParser.sCurrentBlock.sContentDisposition.bHasFileName != FALSE)
        {
          if ((sParser.nFileUploadCounter++) >= (SIZE_T)dwMaxFilesCount)
          {
            hRes = MX_E_BadLength;
            goto done;
          }

          //switch to file container
          if (cDownloadStartedCallback)
          {
            LPCWSTR sW = (LPCWSTR)(sParser.sCurrentBlock.sContentDisposition.cStrFileNameW);
            hRes = cDownloadStartedCallback(&(sParser.cFileH), sW, lpUserData);
            if (SUCCEEDED(hRes) && (!(sParser.cFileH)))
              hRes = MX_HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
            if (FAILED(hRes))
              goto done;
          }
        }
        else
        {
          sParser.cStrCurrLineA.Empty();
        }

        //process data
        sParser.nState = eState::Data;
        break;

      case eState::Data:
        if (*szDataA == '\r')
        {
          sParser.nState = eState::MayBeDataEnd;
          sParser.nBoundaryPos = 0;
          break;
        }
        if (*szDataA == '\n')
        {
          sParser.nState = eState::MayBeDataEndAlt;
          sParser.nBoundaryPos = 1;
          break;
        }

        //normal data
        hRes = AccumulateData(*szDataA);
        if (FAILED(hRes))
          goto done;
        break;

      case eState::MayBeDataEnd:
      case eState::MayBeDataEndAlt:
        if (sParser.nBoundaryPos == 0)
        {
          if (*szDataA != '\n')
          {
not_boundary_end:
            if (sParser.nState == eState::MayBeDataEnd)
            {
              hRes = AccumulateData('\r');
              if (FAILED(hRes))
                goto done;
            }
            if (sParser.nBoundaryPos > 0)
            {
              hRes = AccumulateData('\n');
              if (SUCCEEDED(hRes) && sParser.nBoundaryPos > 1)
                hRes = AccumulateData('-');
              if (SUCCEEDED(hRes) && sParser.nBoundaryPos > 2)
                hRes = AccumulateData('-');
              if (FAILED(hRes))
                goto done;
            }
            if (sParser.nBoundaryPos > 3)
            {
              LPCSTR sA = (LPCSTR)cStrBoundaryA;

              for (k = 3; k < sParser.nBoundaryPos; k++)
              {
                hRes = AccumulateData(sA[k - 3]);
                if (FAILED(hRes))
                  goto done;
              }
            }
            //'*szData' points to the current char but it can be the beginning of a boundary end
            sParser.nState = eState::Data;
            BACKWARD_CHAR();
            break;
          }
        }
        else if (sParser.nBoundaryPos <= 2)
        {
          if (*szDataA != '-')
            goto not_boundary_end;
        }
        else
        {
          LPCSTR sA = (LPCSTR)cStrBoundaryA;

          if (*szDataA != sA[sParser.nBoundaryPos - 3])
            goto not_boundary_end;
          if (sA[sParser.nBoundaryPos - 2] == 0)
          {
            //boundary detected, add accumulated value
            if (!(sParser.cFileH))
            {
              hRes = Utf8_Decode(cStrTempW, (LPCSTR)(sParser.cStrCurrLineA));
              if (SUCCEEDED(hRes))
                hRes = AddField((LPCWSTR)(sParser.sCurrentBlock.sContentDisposition.cStrNameW), (LPCWSTR)cStrTempW);
              if (FAILED(hRes))
                goto done;
            }
            else
            {
              //flush buffers
              hRes = S_OK;
              if (sParser.nUsedTempBuf >= 0)
              {
                if (::WriteFile(sParser.cFileH, sParser.aTempBuf, (DWORD)(sParser.nUsedTempBuf), &dw, NULL) == FALSE)
                  hRes = MX_HRESULT_FROM_LASTERROR();
                else if ((SIZE_T)dw != sParser.nUsedTempBuf)
                  hRes = MX_E_WriteFault;
              }
              if (SUCCEEDED(hRes))
              {
                hRes = AddFileField((LPCWSTR)(sParser.sCurrentBlock.sContentDisposition.cStrNameW),
                                    (LPCWSTR)(sParser.sCurrentBlock.sContentDisposition.cStrFileNameW),
                                    (LPCSTR)(sParser.sCurrentBlock.cStrContentTypeA), sParser.cFileH.Get());
              }
              if (FAILED(hRes))
                goto done;
              sParser.cFileH.Detach();
            }
            sParser.cStrCurrLineA.Empty();
            sParser.nState = eState::DataEnd;
            break;
          }
        }
        (sParser.nBoundaryPos)++;
        break;

      case eState::DataEnd:
        if (*szDataA == '-')
        {
          sParser.nState = eState::BoundaryEndCheck;
        }
        else if (*szDataA == '\r')
        {
          sParser.nState = eState::BoundaryAfterEnd;
        }
        else
        {
          BACKWARD_CHAR();
          sParser.nState = eState::BoundaryAfter;
        }
        break;

      case eState::Done:
        break; //ignore

      default:
        MX_ASSERT(FALSE);
        break;
    }
  }

done:
  if (FAILED(hRes))
    sParser.nState = eState::Error;
  return hRes;
}
#undef BACKWARD_CHAR

HRESULT CHttpBodyParserMultipartFormData::ParseHeader(_Inout_ CStringA &cStrLineA)
{
  LPSTR szLineA, szNameStartA, szNameEndA, szValueStartA, szValueEndA;
  HRESULT hRes;

  szLineA = (LPSTR)cStrLineA;
  //skip blanks
  while (*szLineA==' ' || *szLineA=='\t')
    szLineA++;
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
  while (szValueEndA > szValueStartA && (*(szValueEndA-1) == ' ' || *(szValueEndA-1) == '\t'))
    szValueEndA--;
  //parse header
  switch ((SIZE_T)(szNameEndA - szNameStartA))
  {
    case 19:
      if (StrNCompareA(szNameStartA, "Content-Disposition", 19, TRUE) == 0)
      {
        TAutoRefCounted<CHttpHeaderEntContentDisposition> cHeader;
        LPCWSTR sW;

        if (sParser.sCurrentBlock.sContentDisposition.cStrNameW.IsEmpty() == FALSE)
          return MX_E_InvalidData; //header already specified

        //parse value
        hRes = CHttpHeaderBase::Create<CHttpHeaderEntContentDisposition>(TRUE, &cHeader);
        if (SUCCEEDED(hRes))
          hRes = cHeader->Parse(szValueStartA, (SIZE_T)(szValueEndA - szValueStartA));
        if (FAILED(hRes))
          return hRes;
        if (StrCompareA(cHeader->GetType(), "form-data", TRUE) != 0)
          return MX_E_InvalidData;

        //name parameter
        sW = cHeader->GetName();
        if (*sW == 0)
          return MX_E_InvalidData;
        if (sParser.sCurrentBlock.sContentDisposition.cStrNameW.Copy(sW) == FALSE)
          return E_OUTOFMEMORY;

        //filename parameter
        sParser.sCurrentBlock.sContentDisposition.bHasFileName = cHeader->HasFileName();
        if (sParser.sCurrentBlock.sContentDisposition.bHasFileName != FALSE)
        {
          if (sParser.sCurrentBlock.sContentDisposition.cStrFileNameW.Copy(cHeader->GetFileName()) == FALSE)
            return E_OUTOFMEMORY;
        }

        //done
        return S_OK;
      }
      break;

    case 12:
      if (StrNCompareA(szNameStartA, "Content-Type", 12, TRUE) == 0)
      {
        TAutoRefCounted<CHttpHeaderEntContentType> cHeader;

        if (sParser.sCurrentBlock.cStrContentTypeA.IsEmpty() == FALSE)
          return MX_E_InvalidData; //header already specified

        //parse value
        hRes = CHttpHeaderBase::Create<CHttpHeaderEntContentType>(TRUE, &cHeader);
        if (SUCCEEDED(hRes))
          hRes = cHeader->Parse(szValueStartA, (SIZE_T)(szValueEndA - szValueStartA));
        if (FAILED(hRes))
          return hRes;

        if (sParser.sCurrentBlock.cStrContentTypeA.Copy(cHeader->GetType()) == FALSE)
          return E_OUTOFMEMORY;

        //done
        return S_OK;
      }
      break;

    case 25:
      if (StrNCompareA(szNameStartA, "Content-Transfer-Encoding", 25, TRUE) == 0)
      {
        TAutoRefCounted<CHttpHeaderEntContentTransferEncoding> cHeader;

        if (sParser.sCurrentBlock.cStrContentTypeA.IsEmpty() == FALSE)
          return MX_E_InvalidData; //header already specified

                                   //parse value
        hRes = CHttpHeaderBase::Create<CHttpHeaderEntContentTransferEncoding>(TRUE, &cHeader);
        if (SUCCEEDED(hRes))
          hRes = cHeader->Parse(szValueStartA, (SIZE_T)(szValueEndA - szValueStartA));
        if (FAILED(hRes))
          return hRes;

        if (cHeader->GetEncoding() != CHttpHeaderEntContentTransferEncoding::eEncoding::Identity)
          return MX_E_InvalidData; //header already specified
        sParser.sCurrentBlock.bContentTransferEncoding = TRUE;

        //done
        return S_OK;
      }
      break;
  }
  //ignore header
  return S_OK;
}

HRESULT CHttpBodyParserMultipartFormData::AccumulateData(_In_ CHAR chA)
{
  DWORD dw;

  sParser.aTempBuf[sParser.nUsedTempBuf++] = (BYTE)chA;
  if (!(sParser.cFileH))
  {
    if (sParser.cStrCurrLineA.GetLength() > (SIZE_T)dwMaxFieldSize)
      return MX_E_BadLength;
    if (sParser.cStrCurrLineA.ConcatN(&chA, 1) == FALSE)
      return E_OUTOFMEMORY;
  }
  else
  {
    if ((++sParser.nFileUploadSize) >= ullMaxFileSize)
      return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
    //flush current data?
    if (sParser.nUsedTempBuf >= sizeof(sParser.aTempBuf))
    {
      if (::WriteFile(sParser.cFileH, sParser.aTempBuf, (DWORD)(sParser.nUsedTempBuf), &dw, NULL) == FALSE)
        return MX_HRESULT_FROM_LASTERROR();
      if ((SIZE_T)dw != sParser.nUsedTempBuf)
        return MX_E_WriteFault;
      sParser.nUsedTempBuf = 0;
    }
  }
  return S_OK;
}

} //namespace MX
