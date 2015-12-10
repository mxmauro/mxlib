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

  SIZE_T Find(__in BYTE nToScan, __in SIZE_T nStartPos=0); //returns -1 if not found

  VOID GetReadPtr(__out_opt LPBYTE *lplpPtr1, __out_opt SIZE_T *lpnSize1, __out_opt LPBYTE *lplpPtr2,
                  __out_opt SIZE_T *lpnSize2);
  HRESULT AdvanceReadPtr(__in SIZE_T nCount);

  VOID GetWritePtr(__out_opt LPBYTE *lplpPtr1, __out_opt SIZE_T *lpnSize1, __out_opt LPBYTE *lplpPtr2,
                   __out_opt SIZE_T *lpnSize2);
  HRESULT AdvanceWritePtr(__in SIZE_T nCount);

  SIZE_T GetAvailableForRead() const;
  SIZE_T Peek(__out LPVOID lpDest, __in SIZE_T nToRead);
  SIZE_T Read(__out LPVOID lpDest, __in SIZE_T nToRead);
  HRESULT Write(__in LPCVOID lpSrc, __in SIZE_T nSrcLength, __in BOOL bExpandIfNeeded=TRUE);

  HRESULT SetBufferSize(__in SIZE_T nSize);
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
