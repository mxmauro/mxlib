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
#ifndef _MX_HTTPBODYPARSERBASE_H
#define _MX_HTTPBODYPARSERBASE_H

#include "..\Defines.h"
#include "..\Strings\Strings.h"
#include "..\RefCounted.h"
#include "..\PropertyBag.h"
#include "HttpCommon.h"

//-----------------------------------------------------------

namespace MX {

class MX_NOVTABLE CHttpBodyParserBase : public virtual TRefCounted<CBaseMemObj>
{
protected:
  CHttpBodyParserBase();
public:
  ~CHttpBodyParserBase();

  virtual LPCSTR GetType() const = 0;

  BOOL IsEntityTooLarge() const
    {
    return bEntityTooLarge;
    };

  VOID MarkEntityAsTooLarge()
    {
    bEntityTooLarge = TRUE;
    return;
    };

protected:
  friend class CHttpCommon;

  virtual HRESULT Initialize(_In_ CHttpCommon &cHttpCmn) = 0;
  virtual HRESULT Parse(_In_opt_ LPCVOID lpData, _In_opt_ SIZE_T nDataSize) = 0;

protected:
  BOOL bEntityTooLarge;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPBODYPARSERBASE_H
