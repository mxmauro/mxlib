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
#ifndef _MX_HTTPBODYPARSERDEFAULT_H
#define _MX_HTTPBODYPARSERDEFAULT_H

#include "HttpBodyParserBase.h"
#include "..\AutoHandle.h"
#include "..\AutoPtr.h"
#include "..\WaitableObjects.h"

//-----------------------------------------------------------

namespace MX {

class CHttpBodyParserDefault : public CHttpBodyParserBase
{
public:
  typedef Callback<HRESULT (_Out_ LPHANDLE lphFile, _In_z_ LPCWSTR szFileNameW,
                            _In_opt_ LPVOID lpUserParam)> OnDownloadStartedCallback;

public:
  CHttpBodyParserDefault(_In_ OnDownloadStartedCallback cDownloadStartedCallback, _In_opt_ LPVOID lpUserParam,
                         _In_ DWORD dwMaxBodySizeInMemory=32768, _In_ ULONGLONG ullMaxBodySize=10ui64*1048576ui64);
  ~CHttpBodyParserDefault();

  virtual LPCSTR GetType() const
    {
    return "default";
    };

  ULONGLONG GetSize() const
    {
    return nSize;
    };

  HRESULT Read(_Out_writes_to_(nToRead, *lpnReaded) LPVOID lpDest, _In_ ULONGLONG nOffset, _In_ SIZE_T nToRead,
               _Out_opt_ SIZE_T *lpnReaded=NULL);

  HRESULT ToString(_Inout_ CStringA &cStrDestA);

  operator HANDLE() const
    {
    return cFileH.Get();
    };

  VOID KeepFile();

protected:
  HRESULT Initialize(_In_ CHttpCommon &cHttpCmn);
  HRESULT Parse(_In_opt_ LPCVOID lpData, _In_opt_ SIZE_T nDataSize);

private:
  typedef enum {
    StateReading,
    StateDone,
    StateError
  } eState;

  OnDownloadStartedCallback cDownloadStartedCallback;
  LPVOID lpUserParam;
  DWORD dwMaxBodySizeInMemory;
  ULONGLONG ullMaxBodySize;

  struct {
    eState nState;
  } sParser;
  LONG volatile nMutex;
  TAutoFreePtr<BYTE> cMemBuffer;
  SIZE_T nMemBufferSize;
  ULONGLONG nSize;
  CWindowsHandle cFileH;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPBODYPARSERDEFAULT_H
