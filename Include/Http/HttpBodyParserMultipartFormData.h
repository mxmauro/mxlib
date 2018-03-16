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
#ifndef _MX_HTTPBODYPARSERMULTIPARTFORMDATA_H
#define _MX_HTTPBODYPARSERMULTIPARTFORMDATA_H

#include "HttpBodyParserFormBase.h"

//-----------------------------------------------------------

namespace MX {

class CHttpBodyParserMultipartFormData : public CHttpBodyParserFormBase
{
public:
  CHttpBodyParserMultipartFormData();
  ~CHttpBodyParserMultipartFormData();

protected:
  HRESULT Initialize(_In_ CPropertyBag &cPropBag, _In_ CHttpCommon &cHttpCmn);;
  HRESULT Parse(_In_opt_ LPCVOID lpData, _In_opt_ SIZE_T nDataSize);

  HRESULT ParseHeader(_Inout_ CStringA &cStrLineA);
  HRESULT AccumulateData(_In_ CHAR chA);

private:
  typedef enum {
    StateBoundary,
    StateBoundaryAfter,
    StateBoundaryAfterEnd,
    StateBoundaryEndCheck,
    StateBoundaryEndCheck2,
    StateBoundaryEndCheck3,
    StateBoundaryEndCheck3End,

    StateHeaderStart,
    StateHeaderName,
    StateHeaderValueSpaceBefore,
    StateHeaderValue,
    StateHeaderValueSpaceAfter,
    StateNearHeaderValueEnd,
    StateHeadersEnd,

    StateData,
    StateDataEnd,

    StateMayBeDataEnd,
    StateMayBeDataEndAlt,

    StateDone,
    StateError
  } eState;

  ULONGLONG nMaxFileUploadSize;
  SIZE_T nMaxFileUploadCount;
  CStringW cStrTempFolderW;
  SIZE_T nMaxNonFileFormFieldsDataSize;
  CStringA cStrBoundaryA;

  struct {
    eState nState;
    SIZE_T nBoundaryPos;
    CStringA cStrCurrLineA;
    struct {
      struct {
        CStringW cStrNameW;
        BOOL bHasFileName;
        CStringW cStrFileNameW;
      } sContentDisposition;
      CStringA cStrContentTypeA;
      BOOL bContentTransferEncoding;
    } sCurrentBlock;
    CStringW cStrCurrFieldNameW;
    BYTE aTempBuf[16384];
    SIZE_T nUsedTempBuf, nFileUploadCounter;
    ULONGLONG nFileUploadSize;
    CWindowsHandle cFileH;
  } sParser;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPBODYPARSERMULTIPARTFORMDATA_H
