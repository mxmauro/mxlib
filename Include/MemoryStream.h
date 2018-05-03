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
#ifndef _MX_MEMORYSTREAM_H
#define _MX_MEMORYSTREAM_H

#include "Streams.h"

//-----------------------------------------------------------

namespace MX {

class CMemoryStream : public CStream
{
  MX_DISABLE_COPY_CONSTRUCTOR(CMemoryStream);
public:
  CMemoryStream(_In_opt_ SIZE_T nAllocationGranularity=65536);
  ~CMemoryStream();

  HRESULT Create(_In_opt_ SIZE_T nInitialSize=0, _In_opt_ BOOL bGrowable=TRUE);
  VOID Close();

  HRESULT Read(_Out_ LPVOID lpDest, _In_ SIZE_T nBytes, _Out_ SIZE_T &nBytesRead,
               _In_opt_ ULONGLONG nStartOffset=ULONGLONG_MAX);
  HRESULT Write(_In_ LPCVOID lpSrc, _In_ SIZE_T nBytes, _Out_ SIZE_T &nBytesWritten,
                _In_opt_ ULONGLONG nStartOffset=ULONGLONG_MAX);

  HRESULT Seek(_In_ ULONGLONG nPosition, _In_opt_ eSeekMethod nMethod=SeekStart);

  ULONGLONG GetLength() const;

  BOOL CanGrow() const;

  CMemoryStream* Clone();

private:
  BOOL EnsureSize(_In_ SIZE_T nRequiredSize);

private:
  LPBYTE lpData;
  SIZE_T nCurrPos, nSize, nAllocated, nGranularity;
  BOOL bCanGrow;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_MEMORYSTREAM_H
