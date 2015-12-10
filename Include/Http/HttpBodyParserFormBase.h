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
#ifndef _MX_HTTPBODYPARSERFORMBASE_H
#define _MX_HTTPBODYPARSERFORMBASE_H

#include "HttpBodyParserBase.h"
#include "..\WaitableObjects.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class CHttpBodyParserFormBase : public CHttpBodyParserBase
{
protected:
  CHttpBodyParserFormBase();
public:
  ~CHttpBodyParserFormBase();

  class CField;
  class CFileField;

  LPCSTR GetType() const
    {
    return "form";
    };

  SIZE_T GetFieldsCount() const;
  CField* GetField(__in SIZE_T nIndex) const;

  SIZE_T GetFileFieldsCount() const;
  CFileField* GetFileField(__in SIZE_T nIndex) const;

public:
  class CField : public virtual CBaseMemObj
  {
  public:
    CField();
    ~CField();

    __inline LPCWSTR GetName() const
      {
      return szNameW;
      };

    __inline LPCWSTR GetIndex() const
      {
      return szIndexW;
      };

    __inline LPCWSTR GetValue() const
      {
      return szValueW;
      };

  private:
    friend class CHttpBodyParserFormBase;

    LPWSTR szNameW, szIndexW, szValueW;
  };

  //----

  class CFileField : public virtual CBaseMemObj
  {
  public:
    CFileField();
    ~CFileField();

    __inline LPCWSTR GetName() const
      {
      return szNameW;
      };

    __inline LPCWSTR GetIndex() const
      {
      return szIndexW;
      };

    __inline LPCSTR GetType() const
      {
      return (szTypeA != NULL) ? szTypeA : "text/plain";
      };

    __inline LPCWSTR GetFileName() const
      {
      return szFileNameW;
      };

    __inline HANDLE GetFileHandle() const
      {
      return hFile;
      };

  private:
    friend class CHttpBodyParserFormBase;

    LPWSTR szNameW, szIndexW, szFileNameW;
    LPSTR szTypeA;
    HANDLE hFile;
  };

protected:
  HRESULT AddField(__in_z LPCWSTR szNameW, __in_z LPCWSTR szValueW);
  HRESULT AddFileField(__in_z LPCWSTR szNameW, __in_z LPCWSTR szFileNameW, __in_z LPCSTR szTypeA, __in HANDLE hFile);

protected:
  TArrayListWithDelete<CField*> cFieldsList;
  TArrayListWithDelete<CFileField*> cFileFieldsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPBODYPARSERFORMBASE_H
