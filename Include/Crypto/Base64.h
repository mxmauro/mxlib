/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2006-2015. All rights reserved.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 *
 * 2. This notice may not be removed or altered from any source distribution.
 *
 * 3. YOU MAY NOT:
 *
 *    a. Modify, translate, adapt, alter, or create derivative works from
 *       this software.
 *    b. Copy (other than one back-up copy), distribute, publicly display,
 *       transmit, sell, rent, lease or otherwise exploit this software.
 *    c. Distribute, sub-license, rent, lease, loan [or grant any third party
 *       access to or use of the software to any third party.
 **/
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
