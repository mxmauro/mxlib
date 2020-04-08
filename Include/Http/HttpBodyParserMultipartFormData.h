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
#ifndef _MX_HTTPBODYPARSERMULTIPARTFORMDATA_H
#define _MX_HTTPBODYPARSERMULTIPARTFORMDATA_H

#include "HttpBodyParserFormBase.h"

//-----------------------------------------------------------

namespace MX {

class CHttpBodyParserMultipartFormData : public CHttpBodyParserFormBase, public CNonCopyableObj
{
public:
  typedef Callback<HRESULT(_Out_ LPHANDLE lphFile, _In_z_ LPCWSTR szFileNameW,
                           _In_opt_ LPVOID lpUserParam)> OnDownloadStartedCallback;

public:
  CHttpBodyParserMultipartFormData(_In_ OnDownloadStartedCallback cDownloadStartedCallback, _In_opt_ LPVOID lpUserData,
                                   _In_ DWORD dwMaxFieldSize = 256000, _In_ ULONGLONG ullMaxFileSize = 2097152ui64,
                                   _In_ DWORD dwMaxFilesCount = 4);
  ~CHttpBodyParserMultipartFormData();

  LPCSTR GetType() const
    {
    return "multipart/form-data";
    };

protected:
  HRESULT Initialize(_In_ Internals::CHttpParser &cHttpParser);
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

  OnDownloadStartedCallback cDownloadStartedCallback;
  LPVOID lpUserData;
  DWORD dwMaxFieldSize;
  ULONGLONG ullMaxFileSize;
  DWORD dwMaxFilesCount;

  CStringA cStrBoundaryA;

  struct {
    eState nState;
    SIZE_T nBoundaryPos;
    CStringA cStrCurrLineA;
    struct {
      struct {
        CStringW cStrNameW;
        CStringW cStrFileNameW;
        BOOL bHasFileName;
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
