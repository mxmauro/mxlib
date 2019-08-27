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
#ifndef _MX_CIRCULARBUFFER_H
#define _MX_CIRCULARBUFFER_H

#include "Defines.h"

//-----------------------------------------------------------

namespace MX {

class CCircularBuffer : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CCircularBuffer);
public:
  CCircularBuffer();
  ~CCircularBuffer();

  SIZE_T Find(_In_ BYTE nToScan, _In_ SIZE_T nStartPos=0); //returns -1 if not found

  VOID GetReadPtr(_Out_opt_ LPBYTE *lplpPtr1, _Out_opt_ SIZE_T *lpnSize1, _Out_opt_ LPBYTE *lplpPtr2,
                  _Out_opt_ SIZE_T *lpnSize2);
  HRESULT AdvanceReadPtr(_In_ SIZE_T nCount);

  VOID GetWritePtr(_Out_opt_ LPBYTE *lplpPtr1, _Out_opt_ SIZE_T *lpnSize1, _Out_opt_ LPBYTE *lplpPtr2,
                   _Out_opt_ SIZE_T *lpnSize2);
  HRESULT AdvanceWritePtr(_In_ SIZE_T nCount);

  SIZE_T GetAvailableForRead() const;
  SIZE_T Peek(_Out_writes_(nToRead) LPVOID lpDest, _In_ SIZE_T nToRead);
  SIZE_T Read(_Out_writes_(nToRead) LPVOID lpDest, _In_ SIZE_T nToRead);
  HRESULT Write(_In_ LPCVOID lpSrc, _In_ SIZE_T nSrcLength, _In_ BOOL bExpandIfNeeded=TRUE);

  HRESULT SetBufferSize(_In_ SIZE_T nSize);
  SIZE_T GetBufferSize() const
    {
    return nSize;
    };
  SIZE_T GetWrittenBufferLength() const
    {
    return nLen;
    };

  VOID ReArrangeBuffer();

private:
  LPBYTE lpData;
  SIZE_T nSize, nStart, nLen;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_CIRCULARBUFFER_H
