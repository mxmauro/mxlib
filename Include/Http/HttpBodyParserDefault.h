/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2015. All rights reserved.
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
#ifndef _MX_HTTPBODYPARSERDEFAULT_H
#define _MX_HTTPBODYPARSERDEFAULT_H

#include "HttpBodyParserBase.h"
#include "..\AutoHandle.h"
#include "..\AutoPtr.h"
#include "..\WaitableObjects.h"

//-----------------------------------------------------------

namespace MX {

class CHttpBodyParserDefault : public CHttpBodyParserBase
{
public:
  CHttpBodyParserDefault();
  ~CHttpBodyParserDefault();

  virtual LPCSTR GetType() const
    {
    return "default";
    };

  ULONGLONG GetSize() const
    {
    return nSize;
    };

  HRESULT Read(_Out_writes_to_(nToRead, *lpnReaded) LPVOID lpDest, _In_ ULONGLONG nOffset, _In_ SIZE_T nToRead,
               _Out_opt_ SIZE_T *lpnReaded=NULL);

  HRESULT ToString(_Inout_ CStringA &cStrDestA);

protected:
  HRESULT Initialize(_In_ CPropertyBag &cPropBag, _In_ CHttpCommon &cHttpCmn);
  HRESULT Parse(_In_opt_ LPCVOID lpData, _In_opt_ SIZE_T nDataSize);

private:
  typedef enum {
    StateReading,

    StateDone,
    StateError
  } eState;

  int nMaxBodySizeInMemory;
  CStringW cStrTempFolderW;
  ULONGLONG nMaxBodySize;

  struct {
    eState nState;
  } sParser;
  LONG volatile nMutex;
  TAutoFreePtr<BYTE> cMemBuffer;
  SIZE_T nMemBufferSize;
  ULONGLONG nSize;
  CWindowsHandle cFileH;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPBODYPARSERDEFAULT_H
