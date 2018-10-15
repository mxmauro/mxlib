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
  CField* GetField(_In_ SIZE_T nIndex) const;

  SIZE_T GetFileFieldsCount() const;
  CFileField* GetFileField(_In_ SIZE_T nIndex) const;

public:
  class CField : public virtual CBaseMemObj
  {
  public:
    CField();

    __inline LPCWSTR GetName() const
      {
      return (LPCWSTR)cStrNameW;
      };

    __inline LPCWSTR GetValue() const
      {
      return (LPCWSTR)cStrValueW;
      };

    __inline SIZE_T GetSubIndexesCount() const
      {
      return aSubIndexesItems.GetCount();
      };

    __inline CField* GetSubIndexAt(_In_ SIZE_T nPos) const
      {
      return (nPos < aSubIndexesItems.GetCount()) ? aSubIndexesItems[nPos] : NULL;
      };

  private:
    friend class CHttpBodyParserFormBase;

    VOID ClearValue();

  private:
    MX::CStringW cStrNameW, cStrValueW;
    TArrayListWithDelete<CField*> aSubIndexesItems;
  };

  //----

  class CFileField : public virtual CBaseMemObj
  {
  public:
    CFileField();
    ~CFileField();

    __inline LPCWSTR GetName() const
      {
      return (LPCWSTR)cStrNameW;
      };

    __inline LPCSTR GetType() const
      {
      if (cStrTypeA.IsEmpty() == FALSE)
        return (LPCSTR)cStrTypeA;
      return (cStrFileNameW.IsEmpty() == FALSE) ? "text/plain" : "";
      };

    __inline LPCWSTR GetFileName() const
      {
      return (LPCWSTR)cStrFileNameW;
      };

    __inline SIZE_T GetSubIndexesCount() const
      {
      return aSubIndexesItems.GetCount();
      };

    __inline CFileField* GetSubIndexAt(_In_ SIZE_T nPos) const
      {
      return (nPos < aSubIndexesItems.GetCount()) ? aSubIndexesItems[nPos] : NULL;
      };

    ULONGLONG GetSize() const
      {
      return nSize;
      };

    HRESULT Read(_Out_writes_to_(nToRead, *lpnReaded) LPVOID lpDest, _In_ ULONGLONG nOffset, _In_ SIZE_T nToRead,
                 _Out_opt_ SIZE_T *lpnReaded=NULL);

    operator HANDLE() const
      {
      return hFile;
      };

  private:
    friend class CHttpBodyParserFormBase;

    VOID ClearValue();

  private:
    MX::CStringW cStrNameW, cStrFileNameW;
    MX::CStringA cStrTypeA;
    HANDLE hFile;
    ULONGLONG nSize;
    TArrayListWithDelete<CFileField*> aSubIndexesItems;
  };

protected:
  HRESULT AddField(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szValueW);
  HRESULT AddFileField(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szFileNameW, _In_z_ LPCSTR szTypeA, _In_ HANDLE hFile);

protected:
  TArrayListWithDelete<CField*> cFieldsList;
  TArrayListWithDelete<CFileField*> cFileFieldsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPBODYPARSERFORMBASE_H
