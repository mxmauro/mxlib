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
#ifndef _MX_BASECRYPTO_H
#define _MX_BASECRYPTO_H

#include "..\Defines.h"
#include "..\CircularBuffer.h"

//-----------------------------------------------------------

namespace MX {

class CBaseCrypto : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CBaseCrypto);
protected:
  CBaseCrypto();

public:
  virtual HRESULT BeginEncrypt()=0;
  virtual HRESULT EncryptStream(__in LPCVOID lpData, __in SIZE_T nDataLength)=0;
  virtual HRESULT EndEncrypt()=0;
  SIZE_T GetAvailableEncryptedData();
  SIZE_T GetEncryptedData(__out LPVOID lpDest, __in SIZE_T nDestSize);

  virtual HRESULT BeginDecrypt()=0;
  virtual HRESULT DecryptStream(__in LPCVOID lpData, __in SIZE_T nDataLength)=0;
  virtual HRESULT EndDecrypt()=0;
  SIZE_T GetAvailableDecryptedData();
  SIZE_T GetDecryptedData(__out LPVOID lpDest, __in SIZE_T nDestSize);

protected:
  CCircularBuffer cEncryptedData;
  CCircularBuffer cDecryptedData;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_BASECRYPTO_H
