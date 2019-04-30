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
#include "..\..\Include\Http\HttpBodyParserMultipartFormData.h"
#include "..\..\Include\FnvHash.h"
#include "..\..\Include\Http\Url.h"
#include "..\..\Include\Strings\Utf8.h"

//-----------------------------------------------------------

#define MAX_HEADER_LINE                                 4096

//-----------------------------------------------------------

namespace MX {

CHttpBodyParserMultipartFormData::CHttpBodyParserMultipartFormData(
                                                 _In_ OnDownloadStartedCallback _cDownloadStartedCallback,
                                                 _In_opt_ LPVOID _lpUserData, _In_ DWORD _dwMaxFieldSize,
                                                 _In_ ULONGLONG _ullMaxFileSize, _In_ DWORD _dwMaxFilesCount) :
                                  CHttpBodyParserFormBase()
{
  cDownloadStartedCallback = _cDownloadStartedCallback;
  lpUserData = _lpUserData;
  dwMaxFieldSize = _dwMaxFieldSize;
  if (dwMaxFieldSize < 32768)
    dwMaxFieldSize = 32768;
  ullMaxFileSize = _ullMaxFileSize;
  dwMaxFilesCount = _dwMaxFilesCount;
  sParser.nState = StateBoundary;
  sParser.nBoundaryPos = 0;
  sParser.sCurrentBlock.sContentDisposition.cStrNameW.Empty();
  sParser.sCurrentBlock.sContentDisposition.bHasFileName = FALSE;
  sParser.sCurrentBlock.sContentDisposition.cStrFileNameW.Empty();
  sParser.sCurrentBlock.cStrContentTypeA.Empty();
  sParser.sCurrentBlock.bContentTransferEncoding = FALSE;
  sParser.nUsedTempBuf = sParser.nFileUploadCounter = 0;
  sParser.nFileUploadSize = 0ui64;
  return;
}

CHttpBodyParserMultipartFormData::~CHttpBodyParserMultipartFormData()
{
  return;
}

HRESULT CHttpBodyParserMultipartFormData::Initialize(_In_ CHttpCommon &cHttpCmn)
{
  CHttpHeaderEntContentType *lpHeader;
  LPCWSTR szBoundaryW;

  lpHeader = cHttpCmn.GetHeader<CHttpHeaderEntContentType>();
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
  LPCSTR szDataA, sA;
  DWORD dw;
  SIZE_T k;
  HRESULT hRes;

  if (lpData == NULL && nDataSize > 0)
    return E_POINTER;
  if (sParser.nState == StateDone)
    return S_OK;
  if (sParser.nState == StateError)
    return MX_E_InvalidData;

  //end of parsing?
  if (lpData == NULL)
  {
    if (sParser.nState != StateDone)
    {
      sParser.nState = StateError;
      return MX_E_InvalidData;
    }
    sParser.nState = StateDone;
    return S_OK;
  }

  //begin
  hRes = S_OK;
  //process data
  for (szDataA=(LPCSTR)lpData; szDataA!=(LPCSTR)lpData+nDataSize; szDataA++)
  {
    switch (sParser.nState)
    {
      case StateBoundary:
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
          sA = (LPSTR)cStrBoundaryA;
          if (*szDataA != sA[sParser.nBoundaryPos-2])
            goto err_invalid_data;
          if (sA[sParser.nBoundaryPos-1] == 0)
          {
            sParser.nState = StateBoundaryEndCheck;
            break;
          }
        }
        sParser.nBoundaryPos++;
        break;

      case StateBoundaryEndCheck:
        if (*szDataA == '-')
        {
          sParser.nState = StateBoundaryEndCheck2;
          break;
        }
        sParser.nState = StateBoundaryAfter;
        //fall into 'StateBoundaryAfter'

      case StateBoundaryAfter:
        if (*szDataA == ' ' || *szDataA == '\t')
          break;
        if (*szDataA == '\r')
        {
          sParser.nState = StateBoundaryAfterEnd;
          break;
        }
        //fall into 'StateBoundaryAfterEnd'

      case StateBoundaryAfterEnd:
        if (*szDataA != '\n')
          goto err_invalid_data;
        sParser.nState = StateHeaderStart;
        sParser.sCurrentBlock.sContentDisposition.cStrNameW.Empty();
        sParser.sCurrentBlock.sContentDisposition.bHasFileName = FALSE;
        sParser.sCurrentBlock.sContentDisposition.cStrFileNameW.Empty();
        sParser.sCurrentBlock.cStrContentTypeA.Empty();
        sParser.sCurrentBlock.bContentTransferEncoding = FALSE;
        sParser.cFileH.Close();
        sParser.nUsedTempBuf = 0;
        sParser.nFileUploadSize = 0ui64;
        break;

      case StateBoundaryEndCheck2:
        if (*szDataA != '-')
          goto err_invalid_data;
        sParser.nState = StateBoundaryEndCheck3;
        break;

      case StateBoundaryEndCheck3:
        if (*szDataA == ' ' || *szDataA == '\t')
          break;
        if (*szDataA == '\r')
        {
          sParser.nState = StateBoundaryEndCheck3End;
          break;
        }
        //fall into 'StateBoundaryEndCheck3End'

      case StateBoundaryEndCheck3End:
        if (*szDataA != '\n')
          goto err_invalid_data;
        sParser.nState = StateDone;
        break;

      case StateHeaderStart:
        if (*szDataA == '\r' || *szDataA == '\n')
        {
          //no more headers
          if (sParser.cStrCurrLineA.IsEmpty() == FALSE)
          {
            if (IsEntityTooLarge() == FALSE) //ignore if 413 error is postponed
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
          if (IsEntityTooLarge() == FALSE) //ignore if 413 error is postponed
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
        //end of header name?
        if (*szDataA == ':')
        {
          if (sParser.cStrCurrLineA.IsEmpty() != FALSE)
            goto err_invalid_data;
          if (sParser.cStrCurrLineA.ConcatN(":", 1) == FALSE)
          {
err_nomem:  hRes = E_OUTOFMEMORY;
            goto done;
          }
          sParser.nState = StateHeaderValueSpaceBefore;
          break;
        }
        //check for valid token char
        if (CHttpCommon::IsValidNameChar(*szDataA) == FALSE)
          goto err_invalid_data;
        if (sParser.cStrCurrLineA.GetLength() < MAX_HEADER_LINE)
        {
          if (sParser.cStrCurrLineA.ConcatN(szDataA, 1) == FALSE)
            goto err_nomem;
        }
        else
        {
          MarkEntityAsTooLarge();
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
          if (IsEntityTooLarge() == FALSE) //ignore if 413 error is postponed
          {
            hRes = ParseHeader(sParser.cStrCurrLineA);
            if (FAILED(hRes))
              goto done;
          }
          sParser.cStrCurrLineA.Empty();
          sParser.nState = (*szDataA == '\r') ? StateNearHeaderValueEnd : StateHeaderStart;
          break;
        }
        if (*szDataA == ' ' || *szDataA == '\t')
        {
          sParser.nState = StateHeaderValueSpaceAfter;
          break;
        }
        //check valid value char
        if (*szDataA == 0)
          goto err_invalid_data;
        if (sParser.cStrCurrLineA.GetLength() < MAX_HEADER_LINE)
        {
          if (sParser.cStrCurrLineA.ConcatN(szDataA, 1) == FALSE)
            goto err_nomem;
        }
        else
        {
          MarkEntityAsTooLarge();
        }
        break;

      case StateHeaderValueSpaceAfter:
        if (*szDataA == '\r' || *szDataA == '\n')
          goto on_header_value;
        if (*szDataA == ' ' || *szDataA == '\t')
          break;
        if (sParser.cStrCurrLineA.ConcatN(" ", 1) == FALSE)
          goto err_nomem;
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
        //verify if required headers are set
        if (IsEntityTooLarge() == FALSE)
        {
          if (sParser.sCurrentBlock.sContentDisposition.cStrNameW.IsEmpty() != FALSE)
            goto err_invalid_data; //requires header not set
        }
        //if dealing with a file...
        if (sParser.sCurrentBlock.sContentDisposition.bHasFileName != FALSE)
        {
          if ((sParser.nFileUploadCounter++) >= (SIZE_T)dwMaxFilesCount)
          {
            MarkEntityAsTooLarge(); //too many file uploads
          }
          //switch to file container
          if (IsEntityTooLarge() == FALSE)
          {
            if (cDownloadStartedCallback)
            {
              LPCWSTR sW = (LPCWSTR)(sParser.sCurrentBlock.sContentDisposition.cStrFileNameW);
              hRes = cDownloadStartedCallback(&(sParser.cFileH), sW, lpUserData);
              if (SUCCEEDED(hRes) && (!(sParser.cFileH)))
                hRes = MX_HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
            }
            else
            {
              hRes = MX_HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
            }
            if (FAILED(hRes))
              goto done;
          }
        }
        else
        {
          sParser.cStrCurrLineA.Empty();
        }
        //process data
        sParser.nState = StateData;
        break;

      case StateData:
        if (*szDataA == '\r')
        {
          sParser.nState = StateMayBeDataEnd;
          sParser.nBoundaryPos = 0;
          break;
        }
        if (*szDataA == '\n')
        {
          sParser.nState = StateMayBeDataEndAlt;
          sParser.nBoundaryPos = 1;
          break;
        }
        //normal data
        hRes = AccumulateData(*szDataA);
        if (FAILED(hRes))
          goto done;
        break;

      case StateMayBeDataEnd:
      case StateMayBeDataEndAlt:
        if (sParser.nBoundaryPos == 0)
        {
          if (*szDataA != '\n')
          {
not_boundary_end:
            if (sParser.nState == StateMayBeDataEnd)
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
              sA = (LPSTR)cStrBoundaryA;
              for (k=3; k<sParser.nBoundaryPos; k++)
              {
                hRes = AccumulateData(sA[k-3]);
                if (FAILED(hRes))
                  goto done;
              }
            }
            //'*szData' points to the current char but it can be the beginning of a boundary end
            sParser.nState = StateData;
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
          sA = (LPSTR)cStrBoundaryA;
          if (*szDataA != sA[sParser.nBoundaryPos - 3])
            goto not_boundary_end;
          if (sA[sParser.nBoundaryPos - 2] == 0)
          {
            //boundary detected, add accumulated value
            if (IsEntityTooLarge() == FALSE)
            {
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
            }
            else
            {
              sParser.cFileH.Close();
            }
            sParser.cStrCurrLineA.Empty();
            sParser.nState = StateDataEnd;
            break;
          }
        }
        (sParser.nBoundaryPos)++;
        break;

      case StateDataEnd:
        if (*szDataA == '-')
        {
          sParser.nState = StateBoundaryEndCheck2;
        }
        else if (*szDataA == '\r')
        {
          sParser.nState = StateBoundaryAfterEnd;
        }
        else
        {
          BACKWARD_CHAR();
          sParser.nState = StateBoundaryAfter;
        }
        break;

      case StateDone:
        break; //ignore

      default:
        MX_ASSERT(FALSE);
        break;
    }
  }

done:
  if (FAILED(hRes))
    sParser.nState = StateError;
  return hRes;
}
#undef BACKWARD_CHAR

HRESULT CHttpBodyParserMultipartFormData::ParseHeader(_Inout_ CStringA &cStrLineA)
{
  LPSTR szLineA, szNameStartA, szNameEndA, szValueStartA, szValueEndA;
  CHAR chA[2];
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
        TAutoDeletePtr<CHttpHeaderEntContentDisposition> cHeader;
        LPCWSTR sW;

        if (sParser.sCurrentBlock.sContentDisposition.cStrNameW.IsEmpty() == FALSE)
          return MX_E_InvalidData; //header already specified
        //parse value
        chA[0] = *szNameEndA;
        chA[1] = *szValueEndA;
        *szNameEndA = *szValueEndA = 0;
        hRes = CHttpHeaderBase::Create<CHttpHeaderEntContentDisposition>(TRUE, &cHeader);
        if (SUCCEEDED(hRes))
          hRes = cHeader->Parse(szValueStartA);
        *szNameEndA = chA[0];
        *szValueEndA = chA[1];
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
        TAutoDeletePtr<CHttpHeaderEntContentType> cHeader;

        if (sParser.sCurrentBlock.cStrContentTypeA.IsEmpty() == FALSE)
          return MX_E_InvalidData; //header already specified
        //parse value
        chA[0] = *szNameEndA;
        chA[1] = *szValueEndA;
        *szNameEndA = *szValueEndA = 0;
        hRes = CHttpHeaderBase::Create<CHttpHeaderEntContentType>(TRUE, &cHeader);
        if (SUCCEEDED(hRes))
          hRes = cHeader->Parse(szValueStartA);
        *szNameEndA = chA[0];
        *szValueEndA = chA[1];
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
        TAutoDeletePtr<CHttpHeaderEntContentTransferEncoding> cHeader;

        if (sParser.sCurrentBlock.cStrContentTypeA.IsEmpty() == FALSE)
          return MX_E_InvalidData; //header already specified
        //parse value
        chA[0] = *szNameEndA;
        chA[1] = *szValueEndA;
        *szNameEndA = *szValueEndA = 0;
        hRes = CHttpHeaderBase::Create<CHttpHeaderEntContentTransferEncoding>(TRUE, &cHeader);
        if (SUCCEEDED(hRes))
          hRes = cHeader->Parse(szValueStartA);
        *szNameEndA = chA[0];
        *szValueEndA = chA[1];
        if (FAILED(hRes))
          return hRes;
        if (cHeader->GetEncoding() != CHttpHeaderEntContentTransferEncoding::EncodingIdentity)
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

  if (IsEntityTooLarge() != FALSE)
    return S_OK; //ignore if 413 error is postponed
  sParser.aTempBuf[sParser.nUsedTempBuf++] = (BYTE)chA;
  if (!(sParser.cFileH))
  {
    if (sParser.cStrCurrLineA.GetLength() < (SIZE_T)dwMaxFieldSize)
    {
      if (sParser.cStrCurrLineA.ConcatN(&chA, 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    else
    {
      MarkEntityAsTooLarge();
    }
  }
  else
  {
    if ((++sParser.nFileUploadSize) < ullMaxFileSize)
    {
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
    else
    {
      MarkEntityAsTooLarge();
    }
  }
  return S_OK;
}

} //namespace MX
