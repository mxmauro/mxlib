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
#ifndef _MX_ZIPLIB_H
#define _MX_ZIPLIB_H

#include "..\Defines.h"
#include "..\CircularBuffer.h"

//-----------------------------------------------------------

namespace MX {

class CZipLib : public virtual CBaseMemObj, public CNonCopyableObj
{
public:
  CZipLib(_In_ BOOL bUseZipLibHeader = TRUE);
  virtual ~CZipLib();

  HRESULT BeginCompress(_In_ int nCompressionLevel);
  HRESULT BeginDecompress();
  HRESULT CompressStream(_In_ LPCVOID lpSrc, _In_ SIZE_T nSrcLen);
  HRESULT DecompressStream(_In_ LPCVOID lpSrc, _In_ SIZE_T nSrcLen, _Out_opt_ SIZE_T *lpnUnusedBytes=NULL);
  HRESULT End();

  SIZE_T GetAvailableData() const;
  SIZE_T GetData(_Out_ LPVOID lpDest, _In_ SIZE_T nDestSize);

  BOOL HasDecompressEndOfStreamBeenReached();

protected:
  VOID Cleanup();
  BOOL CheckAndSkipGZipHeader(_Inout_ LPBYTE &s, _Inout_ SIZE_T &nSrcLen, _Inout_opt_ SIZE_T *lpnUnusedBytes);

protected:
  BOOL bUseZipLibHeader;
  int nInUse, nLevel, nGZipHdrState;
  LPVOID lpStream;
  BYTE aTempBuf[4096];
  BYTE aWindow[65536];
  BOOL bEndReached;
  WORD wTemp16;
  CCircularBuffer cProcessed;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_ZIPLIB_H
