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
#ifndef _MX_ZIPLIB_H
#define _MX_ZIPLIB_H

#include "..\Defines.h"
#include "..\CircularBuffer.h"

//-----------------------------------------------------------

namespace MX {

class CZipLib : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CZipLib);
public:
  CZipLib(__in BOOL bUseZipLibHeader=TRUE);
  virtual ~CZipLib();

  HRESULT BeginCompress(__in int nCompressionLevel);
  HRESULT BeginDecompress();
  HRESULT CompressStream(__in LPCVOID lpSrc, __in SIZE_T nSrcLen);
  HRESULT DecompressStream(__in LPCVOID lpSrc, __in SIZE_T nSrcLen, __out_opt SIZE_T *lpnUnusedBytes=NULL);
  HRESULT End();

  SIZE_T GetAvailableData() const;
  SIZE_T GetData(__out LPVOID lpDest, __in SIZE_T nDestSize);

  BOOL HasDecompressEndOfStreamBeenReached();

protected:
  VOID Cleanup();
  BOOL CheckAndSkipGZipHeader(__inout LPBYTE &s, __inout SIZE_T &nSrcLen, __out_opt SIZE_T *lpnUnusedBytes);

protected:
  BOOL bUseZipLibHeader;
  int nInUse, nLevel, nGZipHdrState;
  LPVOID lpStream;
  BYTE aTempBuf[4096];
  BYTE aWindow[65536];
  BOOL bEndReached;
  WORD wTemp16;
  CCircularBuffer cProcessed;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_ZIPLIB_H
