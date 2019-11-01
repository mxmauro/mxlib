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
#ifndef _MX_ZIP_FILE_H
#define _MX_ZIP_FILE_H

#include "ZipLib.h"
#include "..\Streams.h"

//-----------------------------------------------------------

namespace MX {

class CZipFile : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CZipFile);
public:
  CZipFile();
  ~CZipFile();

  HRESULT CreateArchive(_In_z_ LPCWSTR szFileNameW);
  HRESULT OpenArchive(_In_z_ LPCWSTR szFileNameW);
  VOID CloseArchive();

  HRESULT AddFile(_In_z_ LPCWSTR szFileNameInZipW, _In_z_ LPCWSTR szSrcFileNameW,
                  _In_opt_z_ LPCWSTR szPasswordW = NULL);
  HRESULT AddStream(_In_z_ LPCWSTR szFileNameInZipW, _In_ CStream *lpStream, _In_opt_z_ LPCWSTR szPasswordW = NULL);

  HRESULT OpenFile(_In_z_ LPCWSTR szFileNameInZipW, _In_opt_z_ LPCWSTR szPasswordW = NULL);
  VOID CloseFile();

  HRESULT GetFileInfo(_Out_opt_ PULONGLONG lpnFileSize, _Out_opt_ LPDWORD lpdwFileAttributes,
                      _Out_opt_ LPSYSTEMTIME lpFileTime);

  HRESULT Read(_Out_ LPVOID lpDest, _In_ SIZE_T nToRead, _Out_opt_ SIZE_T *lpnRead = NULL);
  HRESULT Read(_Out_ CStream *lpStream, _In_ SIZE_T nToRead, _Out_opt_ SIZE_T *lpnRead = NULL);

private:
  HRESULT CreateOrOpenArchive(_In_z_ LPCWSTR szFileNameW, _In_ int mode);

private:
  LPVOID lpData;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_ZIP_FILE_H
