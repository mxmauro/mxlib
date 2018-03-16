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
#include <intsafe.h>

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

  virtual HRESULT Read(_Out_ LPVOID lpDest, _In_ SIZE_T nBytes, _Out_ SIZE_T &nReaded,
                       _In_opt_ ULONGLONG nStartOffset=ULONGLONG_MAX) = 0;
  virtual HRESULT Write(_In_ LPCVOID lpSrc, _In_ SIZE_T nBytes, _Out_ SIZE_T &nWritten,
                        _In_opt_ ULONGLONG nStartOffset=ULONGLONG_MAX) = 0;
  HRESULT WriteString(_In_ LPCSTR szFormatA, ...);
  HRESULT WriteStringV(_In_ LPCSTR szFormatA, _In_ va_list argptr);
  HRESULT WriteString(_In_ LPCWSTR szFormatA, ...);
  HRESULT WriteStringV(_In_ LPCWSTR szFormatA, _In_ va_list argptr);

  virtual ULONGLONG GetLength() const = 0;

  virtual HRESULT Seek(_In_ ULONGLONG nPosition, _In_opt_ eSeekMethod nMethod=SeekStart);
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_STREAMS_H
