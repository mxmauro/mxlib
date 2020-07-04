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
#ifndef _MX_FILESTREAM_H
#define _MX_FILESTREAM_H

#include "Streams.h"

//-----------------------------------------------------------

namespace MX {

class CFileStream : public CStream, public CNonCopyableObj
{
public:
  CFileStream();
  ~CFileStream();

  HRESULT Create(_In_ LPCWSTR szFileNameW, _In_opt_ DWORD dwDesiredAccess=GENERIC_READ,
                 _In_opt_ DWORD dwShareMode=FILE_SHARE_READ, _In_opt_ DWORD dwCreationDisposition=OPEN_EXISTING,
                 _In_opt_ DWORD dwFlagsAndAttributes=FILE_ATTRIBUTE_NORMAL,
                 _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes=NULL);
  VOID Close();

  HRESULT Read(_Out_ LPVOID lpDest, _In_ SIZE_T nBytes, _Out_ SIZE_T &nBytesRead,
               _In_opt_ ULONGLONG nStartOffset=ULONGLONG_MAX);
  HRESULT Write(_In_ LPCVOID lpSrc, _In_ SIZE_T nBytes, _Out_ SIZE_T &nBytesWritten,
                _In_opt_ ULONGLONG nStartOffset=ULONGLONG_MAX);

  HRESULT Seek(_In_ ULONGLONG nPosition, _In_opt_ eSeekMethod nMethod=SeekStart);

  ULONGLONG GetLength() const;

  CFileStream* Clone();

  virtual HANDLE GetHandle() const;

private:
  CWindowsHandle cFileH;
#ifdef _DEBUG
  ULONGLONG nCurrentOffset;
#endif //_DEBUG
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_FILESTREAM_H
