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
#ifndef _MX_HTTPBODYPARSERBASE_H
#define _MX_HTTPBODYPARSERBASE_H

#include "..\Defines.h"
#include "..\Strings\Strings.h"
#include "..\RefCounted.h"
#include "..\PropertyBag.h"
#include "HttpCommon.h"

//-----------------------------------------------------------

#define MX_HTTP_BODYPARSER_PROPERTY_MaxBodySizeInMemory                 "HTTP_BODYPARSER_MaxBodySizeInMemory"
#define MX_HTTP_BODYPARSER_PROPERTY_MaxBodySizeInMemory_DEFVAL          32768
#define MX_HTTP_BODYPARSER_PROPERTY_TempFolder                          "HTTP_BODYPARSER_TempFolder"
#define MX_HTTP_BODYPARSER_PROPERTY_TempFolder_DEFVAL                   L""
#define MX_HTTP_BODYPARSER_PROPERTY_MaxFileUploadSize                   "HTTP_BODYPARSER_MaxFileUploadSize"
#define MX_HTTP_BODYPARSER_PROPERTY_MaxFileUploadSize_DEFVAL            (2ui64*1048576ui64)
#define MX_HTTP_BODYPARSER_PROPERTY_MaxFileUploadCount                  "HTTP_BODYPARSER_MaxFileUploadCount"
#define MX_HTTP_BODYPARSER_PROPERTY_MaxFileUploadCount_DEFVAL           4
#define MX_HTTP_BODYPARSER_PROPERTY_MaxNonFileFormFieldsDataSize        "HTTP_BODYPARSER_MaxNonFileFormFieldsDataSize"
#define MX_HTTP_BODYPARSER_PROPERTY_MaxNonFileFormFieldsDataSize_DEFVAL 256000
#define MX_HTTP_BODYPARSER_PROPERTY_MaxNonFormBodySize                  "HTTP_BODYPARSER_MaxNonFormBodySize"
#define MX_HTTP_BODYPARSER_PROPERTY_MaxNonFormBodySize_DEFVAL           (10ui64*1048576ui64)

//-----------------------------------------------------------

namespace MX {

class CHttpBodyParserBase : public virtual TRefCounted<CBaseMemObj>
{
protected:
  CHttpBodyParserBase();
public:
  ~CHttpBodyParserBase();

  virtual LPCSTR GetType() const = 0;

  BOOL IsEntityTooLarge() const
    {
    return bEntityTooLarge;
    };

  VOID MarkEntityAsTooLarge()
    {
    bEntityTooLarge = TRUE;
    return;
    };

protected:
  friend class CHttpCommon;

  virtual HRESULT Initialize(__in CPropertyBag &cPropBag, __in CHttpCommon &cHttpCmn) = 0;
  virtual HRESULT Parse(__in LPCVOID lpData, __in SIZE_T nDataSize) = 0;

protected:
  BOOL bEntityTooLarge;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPBODYPARSERBASE_H
