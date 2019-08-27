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
#ifndef _MX_BASE64_H
#define _MX_BASE64_H

#include "..\Defines.h"
#include "..\Strings\Strings.h"

//-----------------------------------------------------------

namespace MX {

class CBase64Encoder : public MX::CBaseMemObj
{
public:
  CBase64Encoder();
  ~CBase64Encoder();

  HRESULT Begin(_In_opt_ SIZE_T nPreallocateOutputLen=0);
  HRESULT Process(_In_ LPVOID lpData, _In_ SIZE_T nDataLen);
  HRESULT End();

  LPCSTR GetBuffer() const;
  SIZE_T GetOutputLength() const;
  VOID ConsumeOutput(_In_ SIZE_T nChars);

  static SIZE_T GetRequiredSpace(_In_ SIZE_T nDataLen);

private:
  __inline BOOL AddToBuffer(_In_ CHAR szDataA[4]);

private:
  LPSTR szBufferA;
  SIZE_T nSize, nLength;
  BYTE aInput[3];
  SIZE_T nInputLength;
};

//-----------------------------------------------------------

class CBase64Decoder : public MX::CBaseMemObj
{
public:
  CBase64Decoder();
  ~CBase64Decoder();

  HRESULT Begin(_In_opt_ SIZE_T nPreallocateOutputLen=0);
  HRESULT Process(_In_ LPCSTR szDataA, _In_opt_ SIZE_T nDataLen=-1);
  HRESULT End();

  LPBYTE GetBuffer() const;
  SIZE_T GetOutputLength() const;
  VOID ConsumeOutput(_In_ SIZE_T nBytes);

  static SIZE_T GetRequiredSpace(_In_ SIZE_T nDataLen);

private:
  __inline BOOL AddToBuffer(_In_ LPBYTE aData, _In_ SIZE_T nLen);

private:
  LPBYTE lpBuffer;
  SIZE_T nSize, nLength;
  BYTE aInput[4];
  SIZE_T nInputLength, nEqualCounter;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_BASE64_H
