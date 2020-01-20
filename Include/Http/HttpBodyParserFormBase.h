/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the LICENSE file distributed with
 * this work for additional information regarding copyright ownership.
 *
 * Also, if exists, check the Licenses directory for information about
 * third-party modules.
 *
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _MX_HTTPBODYPARSERFORMBASE_H
#define _MX_HTTPBODYPARSERFORMBASE_H

#include "HttpBodyParserBase.h"
#include "..\WaitableObjects.h"
#include "..\ArrayList.h"

//-----------------------------------------------------------

namespace MX {

class MX_NOVTABLE CHttpBodyParserFormBase : public CHttpBodyParserBase
{
protected:
  CHttpBodyParserFormBase();
public:
  ~CHttpBodyParserFormBase();

  class CField;
  class CFileField;

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
