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
#ifndef _MX_HTTPHEADERREQUESTIFMODIFIEDSINCE_or_IFUNMODIFIEDSINCE_H
#define _MX_HTTPHEADERREQUESTIFMODIFIEDSINCE_or_IFUNMODIFIEDSINCE_H

#include "HttpHeaderBase.h"
#include "..\DateTime\DateTime.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderReqIfXXXSinceBase : public CHttpHeaderBase
{
protected:
  CHttpHeaderReqIfXXXSinceBase(_In_ BOOL bIfModified);
public:
  ~CHttpHeaderReqIfXXXSinceBase();

  HRESULT Parse(_In_z_ LPCSTR szValueA);

  HRESULT Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser);

  HRESULT SetDate(_In_ CDateTime &cDt);
  CDateTime GetDate() const;

private:
  BOOL bIfModified;
  CDateTime cDt;
};

//-----------------------------------------------------------

class CHttpHeaderReqIfModifiedSince : public CHttpHeaderReqIfXXXSinceBase
{
public:
  CHttpHeaderReqIfModifiedSince() : CHttpHeaderReqIfXXXSinceBase(TRUE)
    { };

  MX_DECLARE_HTTPHEADER_NAME(If-Modified-Since)
};

//-----------------------------------------------------------

class CHttpHeaderReqIfUnmodifiedSince : public CHttpHeaderReqIfXXXSinceBase
{
public:
  CHttpHeaderReqIfUnmodifiedSince() : CHttpHeaderReqIfXXXSinceBase(FALSE)
    { };

  MX_DECLARE_HTTPHEADER_NAME(If-Unmodified-Since)
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERREQUESTIFMODIFIEDSINCE_or_IFUNMODIFIEDSINCE_H
