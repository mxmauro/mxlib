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
#ifndef _MX_HTTPHEADERENTITYCONTENTRANGE_H
#define _MX_HTTPHEADERENTITYCONTENTRANGE_H

#include "HttpHeaderBase.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderEntContentRange : public CHttpHeaderBase
{
public:
  CHttpHeaderEntContentRange();
  ~CHttpHeaderEntContentRange();

  MX_DECLARE_HTTPHEADER_NAME(Content-Range)

  HRESULT Parse(_In_z_ LPCSTR szValueA);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser);

  HRESULT SetRange(_In_ ULONGLONG nByteStart, _In_ ULONGLONG nByteEnd, _In_ ULONGLONG nTotalBytes);
  ULONGLONG GetRangeStart() const;
  ULONGLONG GetRangeEnd() const;
  ULONGLONG GetRangeTotal() const;

private:
  ULONGLONG nByteStart, nByteEnd;
  ULONGLONG nTotalBytes;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERENTITYCONTENTRANGE_H
