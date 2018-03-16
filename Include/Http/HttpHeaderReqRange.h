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
#ifndef _MX_HTTPHEADERREQUESTRANGE_H
#define _MX_HTTPHEADERREQUESTRANGE_H

#include "HttpHeaderBase.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHttpHeaderReqRange : public CHttpHeaderBase
{
public:
  CHttpHeaderReqRange();
  ~CHttpHeaderReqRange();

  MX_DECLARE_HTTPHEADER_NAME(Range)

  HRESULT Parse(_In_z_ LPCSTR szValueA);

  HRESULT Build(_Inout_ CStringA &cStrDestA);

  HRESULT AddRangeSet(_In_ ULONGLONG nByteStart, _In_ ULONGLONG nByteEnd);

  SIZE_T GetRangeSetsCount() const;
  ULONGLONG GetRangeSetStart(_In_ SIZE_T nIndex) const;
  ULONGLONG GetRangeSetEnd(_In_ SIZE_T nIndex) const;

private:
  typedef struct {
    ULONGLONG nByteStart;
    ULONGLONG nByteEnd;
  } RANGESET;

  TArrayList4Structs<RANGESET> cRangeSetsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPHEADERREQUESTRANGE_H
