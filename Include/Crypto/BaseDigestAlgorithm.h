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
#ifndef _MX_BASEDIGESTALGORITHM_H
#define _MX_BASEDIGESTALGORITHM_H

#include "..\Defines.h"

//-----------------------------------------------------------

namespace MX {

class CBaseDigestAlgorithm : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CBaseDigestAlgorithm);
protected:
  CBaseDigestAlgorithm();

public:
  virtual HRESULT BeginDigest()=0;
  virtual HRESULT DigestStream(_In_ LPCVOID lpData, _In_ SIZE_T nDataLength)=0;
  HRESULT DigestWordLE(_In_ LPWORD lpnValues, _In_ SIZE_T nCount);
  HRESULT DigestWordBE(_In_ LPWORD lpnValues, _In_ SIZE_T nCount);
  HRESULT DigestDWordLE(_In_ LPDWORD lpnValues, _In_ SIZE_T nCount);
  HRESULT DigestDWordBE(_In_ LPDWORD lpnValues, _In_ SIZE_T nCount);
  HRESULT DigestQWordLE(_In_ ULONGLONG *lpnValues, _In_ SIZE_T nCount);
  HRESULT DigestQWordBE(_In_ ULONGLONG *lpnValues, _In_ SIZE_T nCount);
  virtual HRESULT EndDigest()=0;

  virtual LPBYTE GetResult() const=0;
  virtual SIZE_T GetResultSize() const=0;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_BASEDIGESTALGORITHM_H
