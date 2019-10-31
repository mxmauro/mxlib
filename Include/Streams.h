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
#ifndef _MX_STREAMS_H
#define _MX_STREAMS_H

#include "Defines.h"
#include "RefCounted.h"
#include <intsafe.h>

//-----------------------------------------------------------

namespace MX {

class MX_NOVTABLE CStream : public virtual TRefCounted<CBaseMemObj>
{
  MX_DISABLE_COPY_CONSTRUCTOR(CStream);
public:
  typedef enum {
    SeekStart=0,
    SeekCurrent,
    SeekEnd,
  } eSeekMethod;

protected:
  CStream() : TRefCounted<CBaseMemObj>()
    {
    lpNextStream = NULL;
    return;
    };

public:
  virtual ~CStream()
    {
    if (lpNextStream != NULL)
      lpNextStream->Release();
    return;
    };

  virtual HRESULT Read(_Out_ LPVOID lpDest, _In_ SIZE_T nBytes, _Out_ SIZE_T &nBytesRead,
                       _In_opt_ ULONGLONG nStartOffset = ULONGLONG_MAX) = 0;
  virtual HRESULT Write(_In_ LPCVOID lpSrc, _In_ SIZE_T nBytes, _Out_ SIZE_T &nBytesWritten,
                        _In_opt_ ULONGLONG nStartOffset = ULONGLONG_MAX) = 0;
  HRESULT WriteString(_In_ LPCSTR szFormatA, ...);
  HRESULT WriteStringV(_In_ LPCSTR szFormatA, _In_ va_list argptr);
  HRESULT WriteString(_In_ LPCWSTR szFormatA, ...);
  HRESULT WriteStringV(_In_ LPCWSTR szFormatA, _In_ va_list argptr);

  virtual ULONGLONG GetLength() const = 0;

  virtual HRESULT Seek(_In_ ULONGLONG nPosition, _In_opt_ eSeekMethod nMethod=SeekStart);

  HRESULT CopyTo(_In_ CStream *lpTarget, _In_ SIZE_T nBytes, _Out_ SIZE_T &nBytesWritten,
                 _In_opt_ ULONGLONG nStartOffset = ULONGLONG_MAX);

  VOID SetChainedStream(_In_opt_ CStream *lpStream);
  CStream* GetChainedStream() const;

  operator HANDLE() const
    {
    return GetHandle();
    };
  virtual HANDLE GetHandle() const
    {
    return NULL;
    };

private:
  CStream *lpNextStream;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_STREAMS_H
