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
#ifndef _MX_STREAMS_H
#define _MX_STREAMS_H

#include "Defines.h"
#include "RefCounted.h"

//-----------------------------------------------------------

namespace MX {

class CStream : public virtual TRefCounted<CBaseMemObj>
{
  MX_DISABLE_COPY_CONSTRUCTOR(CStream);
public:
  typedef enum {
    SeekStart=0,
    SeekCurrent,
    SeekEnd,
  } eSeekMethod;

public:
  CStream() : TRefCounted<CBaseMemObj>()
    { };
  virtual ~CStream()
    { };

  virtual HRESULT Read(__out LPVOID lpDest, __in SIZE_T nBytes, __out SIZE_T &nReaded) = 0;
  virtual HRESULT Write(__in LPCVOID lpSrc, __in SIZE_T nBytes, __out SIZE_T &nWritten) = 0;
  HRESULT WriteString(__in LPCSTR szFormatA, ...);
  HRESULT WriteStringV(__in LPCSTR szFormatA, __in va_list argptr);
  HRESULT WriteString(__in LPCWSTR szFormatA, ...);
  HRESULT WriteStringV(__in LPCWSTR szFormatA, __in va_list argptr);

  virtual ULONGLONG GetLength() const = 0;

  virtual HRESULT Seek(__in ULONGLONG nPosition, __in_opt eSeekMethod nMethod=SeekStart);
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_STREAMS_H
