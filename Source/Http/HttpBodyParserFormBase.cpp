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
#include "..\..\Include\Http\HttpBodyParserFormBase.h"
#include "..\..\Include\AutoPtr.h"

//-----------------------------------------------------------

static HRESULT ParseName(__in_z LPCWSTR szNameW, __inout MX::CStringW &cStrNameW, __inout MX::CStringW &cStrIndexW);

//-----------------------------------------------------------

namespace MX {

CHttpBodyParserFormBase::CHttpBodyParserFormBase() : CHttpBodyParserBase()
{
  return;
}

CHttpBodyParserFormBase::~CHttpBodyParserFormBase()
{
  cFieldsList.RemoveAllElements();
  cFileFieldsList.RemoveAllElements();
  return;
}

SIZE_T CHttpBodyParserFormBase::GetFieldsCount() const
{
  return cFieldsList.GetCount();
}

CHttpBodyParserFormBase::CField* CHttpBodyParserFormBase::GetField(__in SIZE_T nIndex) const
{
  return (nIndex < cFieldsList.GetCount()) ? cFieldsList.GetElementAt(nIndex) : NULL;
}

SIZE_T CHttpBodyParserFormBase::GetFileFieldsCount() const
{
  return cFileFieldsList.GetCount();
}

CHttpBodyParserFormBase::CFileField* CHttpBodyParserFormBase::GetFileField(__in SIZE_T nIndex) const
{
  return (nIndex < cFileFieldsList.GetCount()) ? cFileFieldsList.GetElementAt(nIndex) : NULL;
}

HRESULT CHttpBodyParserFormBase::AddField(__in_z LPCWSTR szNameW, __in_z LPCWSTR szValueW)
{
  TAutoDeletePtr<CField> cNewField;
  CField *lpField;
  CStringW cStrNameW, cStrIndexW;
  SIZE_T i, k, nCount, nValueLen;
  LPWSTR szIdxW;
  HRESULT hRes;

  hRes = ParseName(szNameW, cStrNameW, cStrIndexW);
  if (FAILED(hRes))
    return hRes;
  if (szValueW == NULL)
    return E_POINTER;
  //delete previous non-indexed items
  for (nCount = cFieldsList.GetCount(); nCount > 0; nCount--)
  {
    lpField = cFieldsList.GetElementAt(nCount-1);
    if (StrCompareW((LPWSTR)cStrNameW, lpField->GetName(), TRUE) == 0 && lpField->GetIndex() == NULL)
      cFieldsList.RemoveElementAt(nCount-1);
  }
  //if the item has an index...
  if (cStrIndexW.IsEmpty() == FALSE)
  {
    //if the item has an empty index, find next index
    if (((LPCWSTR)cStrIndexW)[0] == L'\x05')
    {
      cStrIndexW.Empty();
      nCount = cFieldsList.GetCount();
      for (i=0; i<nCount; i++)
      {
        lpField = cFieldsList.GetElementAt(i);
        if (StrCompareW((LPWSTR)cStrNameW, lpField->GetName(), TRUE) == 0)
        {
          szIdxW = (LPWSTR)(lpField->GetIndex());
          //check if numeric index
          for (k=0; szIdxW[k] != 0 && szIdxW[k] >= L'0' && szIdxW[k] <= L'9'; k++);
          if (szIdxW[k] == 0)
          {
            //yes, it is a numeric index, store in temp index var if greater
            if (cStrIndexW.IsEmpty() != FALSE || StrCompareW((LPCWSTR)cStrIndexW, szIdxW) > 0)
            {
              if (cStrIndexW.Copy(szIdxW) == FALSE)
                return E_OUTOFMEMORY;
            }
          }
        }
      }
      //calculate new index (increment the biggest by 1)
      if (cStrIndexW.IsEmpty() == FALSE)
      {
        for (szIdxW=(LPWSTR)cStrIndexW+(cStrIndexW.GetLength()-1); szIdxW>=(LPWSTR)cStrIndexW; szIdxW--)
        {
          (*szIdxW)++;
          if (*szIdxW <= L'9')
            break;
          *szIdxW -= 10;
        }
        if (szIdxW < (LPWSTR)cStrIndexW)
        {
          if (cStrIndexW.Insert(L"1", 0) == FALSE)
            return E_OUTOFMEMORY;
        }
      }
      else
      {
        if (cStrIndexW.Copy(L"0") == FALSE)
          return E_OUTOFMEMORY;
      }
    }
    else
    {
      //else delete previous indexed items with the same name and index
      for (nCount = cFieldsList.GetCount(); nCount > 0; nCount--)
      {
        lpField = cFieldsList.GetElementAt(nCount-1);
        if (StrCompareW((LPWSTR)cStrNameW, lpField->GetName(), TRUE) == 0 &&
            lpField->GetIndex() != NULL &&
            StrCompareW((LPWSTR)cStrIndexW, lpField->GetIndex(), TRUE) == 0)
        {
          cFieldsList.RemoveElementAt(nCount-1);
        }
      }
    }
  }
  //create new item
  cNewField.Attach(MX_DEBUG_NEW CField());
  if (!cNewField)
    return E_OUTOFMEMORY;
  //allocate memory for data
  k = (cStrNameW.GetLength() + 1) * sizeof(WCHAR);
  if (cStrIndexW.IsEmpty() == FALSE)
    k += (cStrIndexW.GetLength() + 1) * sizeof(WCHAR);
  nValueLen = StrLenW(szValueW);
  k += (nValueLen + 1) * sizeof(WCHAR);
  cNewField->szNameW = (LPWSTR)MX_MALLOC(k);
  if (cNewField->szNameW == NULL)
    return E_OUTOFMEMORY;
  //name
  MemCopy(cNewField->szNameW, (LPWSTR)cStrNameW, cStrNameW.GetLength() * sizeof(WCHAR));
  cNewField->szNameW[cStrNameW.GetLength()] = 0;
  //index (if any)
  if (cStrIndexW.IsEmpty() == FALSE)
  {
    cNewField->szIndexW = cNewField->szNameW + (cStrNameW.GetLength() + 1);
    MemCopy(cNewField->szIndexW, (LPWSTR)cStrIndexW, cStrIndexW.GetLength() * sizeof(WCHAR));
    cNewField->szIndexW[cStrIndexW.GetLength()] = 0;
    //value
    cNewField->szValueW = cNewField->szIndexW + (cStrIndexW.GetLength() + 1);
  }
  else
  {
    //value
    cNewField->szValueW = cNewField->szNameW + (cStrNameW.GetLength() + 1);
  }
  MemCopy(cNewField->szValueW, szValueW, nValueLen * sizeof(WCHAR));
  cNewField->szValueW[nValueLen] = 0;
  //add to list
  if (cFieldsList.AddElement(cNewField.Get()) == FALSE)
    return E_OUTOFMEMORY;
  //done
  cNewField.Detach();
  return S_OK;
}

HRESULT CHttpBodyParserFormBase::AddFileField(__in_z LPCWSTR szNameW, __in_z LPCWSTR szFileNameW, __in_z LPCSTR szTypeA,
                                              __in HANDLE hFile)
{
  TAutoDeletePtr<CFileField> cNewFileField;
  CFileField *lpFileField;
  CStringW cStrNameW, cStrIndexW;
  SIZE_T i, k, nCount, nFileNameLen, nTypeLen;
  LPWSTR szIdxW;
  HRESULT hRes;

  hRes = ParseName(szNameW, cStrNameW, cStrIndexW);
  if (FAILED(hRes))
    return hRes;
  if (szFileNameW == NULL || szTypeA == NULL)
    return E_POINTER;
  if (hFile == NULL || hFile == INVALID_HANDLE_VALUE)
    return E_INVALIDARG;
  //delete previous non-indexed items
  for (nCount=cFileFieldsList.GetCount(); nCount>0; nCount--)
  {
    lpFileField = cFileFieldsList.GetElementAt(nCount-1);
    if (StrCompareW((LPWSTR)cStrNameW, lpFileField->GetName(), TRUE) == 0 && lpFileField->GetIndex() == NULL)
      cFileFieldsList.RemoveElementAt(nCount-1);
  }
  //if the item has an index...
  if (cStrIndexW.IsEmpty() == FALSE)
  {
    //if the item has an empty index, find next index
    if (((LPCWSTR)cStrIndexW)[0] == L'\x05')
    {
      cStrIndexW.Empty();
      nCount = cFileFieldsList.GetCount();
      for (i=0; i<nCount; i++)
      {
        lpFileField = cFileFieldsList.GetElementAt(i);
        if (StrCompareW((LPWSTR)cStrNameW, lpFileField->GetName(), TRUE) == 0)
        {
          szIdxW = (LPWSTR)(lpFileField->GetIndex());
          //check if numeric index
          for (k=0; szIdxW[k] != 0 && szIdxW[k] >= L'0' && szIdxW[k] <= L'9'; k++);
          if (szIdxW[k] == 0)
          {
            //yes, it is a numeric index, store in temp index var if greater
            if (cStrIndexW.IsEmpty() != FALSE || StrCompareW((LPCWSTR)cStrIndexW, szIdxW) > 0)
            {
              if (cStrIndexW.Copy(szIdxW) == FALSE)
                return E_OUTOFMEMORY;
            }
          }
        }
      }
      //calculate new index (increment the biggest by 1)
      if (cStrIndexW.IsEmpty() == FALSE)
      {
        for (szIdxW=(LPWSTR)cStrIndexW+(cStrIndexW.GetLength()-1); szIdxW>=(LPWSTR)cStrIndexW; szIdxW--)
        {
          (*szIdxW)++;
          if (*szIdxW <= L'9')
            break;
          *szIdxW -= 10;
        }
        if (szIdxW < (LPWSTR)cStrIndexW)
        {
          if (cStrIndexW.Insert(L"1", 0) == FALSE)
            return E_OUTOFMEMORY;
        }
      }
      else
      {
        if (cStrIndexW.Copy(L"0") == FALSE)
          return E_OUTOFMEMORY;
      }
    }
    else
    {
      //else delete previous indexed items with the same name and index
      for (nCount=cFileFieldsList.GetCount(); nCount>0; nCount--)
      {
        lpFileField = cFileFieldsList.GetElementAt(nCount-1);
        if (StrCompareW((LPWSTR)cStrNameW, lpFileField->GetName(), TRUE) == 0 &&
            lpFileField->GetIndex() != NULL &&
            StrCompareW((LPWSTR)cStrIndexW, lpFileField->GetIndex(), TRUE) == 0)
        {
          cFileFieldsList.RemoveElementAt(nCount-1);
        }
      }
    }
  }
  //create new item
  cNewFileField.Attach(MX_DEBUG_NEW CFileField());
  if (!cNewFileField)
    return E_OUTOFMEMORY;
  //allocate memory for data
  k = (cStrNameW.GetLength() + 1) * sizeof(WCHAR);
  if (cStrIndexW.IsEmpty() == FALSE)
    k += (cStrIndexW.GetLength() + 1) * sizeof(WCHAR);
  nFileNameLen = StrLenW(szFileNameW);
  k += (nFileNameLen + 1) * sizeof(WCHAR);
  nTypeLen = StrLenA(szTypeA);
  k += (nTypeLen + 1) * sizeof(CHAR);
  cNewFileField->szNameW = (LPWSTR)MX_MALLOC(k);
  if (cNewFileField->szNameW == NULL)
    return E_OUTOFMEMORY;
  //name
  MemCopy(cNewFileField->szNameW, (LPWSTR)cStrNameW, cStrNameW.GetLength() * sizeof(WCHAR));
  cNewFileField->szNameW[cStrNameW.GetLength()] = 0;
  //index (if any)
  if (cStrIndexW.IsEmpty() == FALSE)
  {
    cNewFileField->szIndexW = cNewFileField->szNameW + (cStrNameW.GetLength() + 1);
    MemCopy(cNewFileField->szIndexW, (LPWSTR)cStrIndexW, cStrIndexW.GetLength() * sizeof(WCHAR));
    cNewFileField->szIndexW[cStrIndexW.GetLength()] = 0;
    //filename
    cNewFileField->szFileNameW = cNewFileField->szIndexW + (cStrIndexW.GetLength() + 1);
  }
  else
  {
    //filename
    cNewFileField->szFileNameW = cNewFileField->szNameW + (cStrNameW.GetLength() + 1);
  }
  MemCopy(cNewFileField->szFileNameW, szFileNameW, nFileNameLen * sizeof(WCHAR));
  cNewFileField->szFileNameW[nFileNameLen] = 0;
  //type
  cNewFileField->szTypeA = (LPSTR)(cNewFileField->szFileNameW + (nFileNameLen + 1));
  MemCopy(cNewFileField->szTypeA, szTypeA, nTypeLen * sizeof(CHAR));
  cNewFileField->szTypeA[nTypeLen] = 0;
  //handle to file
  cNewFileField->hFile = hFile;
  //add to list
  if (cFileFieldsList.AddElement(cNewFileField.Get()) == FALSE)
  {
    cNewFileField->hFile = NULL; //to avoid closing file on error
    return E_OUTOFMEMORY;
  }
  //done
  cNewFileField.Detach();
  return S_OK;
}

//-----------------------------------------------------------

CHttpBodyParserFormBase::CField::CField() : CBaseMemObj()
{
  szNameW = szIndexW = szValueW = NULL;
  return;
}

CHttpBodyParserFormBase::CField::~CField()
{
  MX_FREE(szNameW);
  return;
}

//-----------------------------------------------------------

CHttpBodyParserFormBase::CFileField::CFileField() : CBaseMemObj()
{
  szNameW = szIndexW = szFileNameW = NULL;
  hFile = NULL;
  szTypeA = NULL;
  return;
}

CHttpBodyParserFormBase::CFileField::~CFileField()
{
  MX_FREE(szNameW);
  if (hFile != NULL && hFile != INVALID_HANDLE_VALUE)
    ::CloseHandle(hFile);
  return;
}

} //namespace MX

//-----------------------------------------------------------

static HRESULT ParseName(__in_z LPCWSTR szNameW, __inout MX::CStringW &cStrNameW, __inout MX::CStringW &cStrIndexW)
{
  BOOL bValidChars, bPrevWasSpace;

  cStrNameW.Empty();
  cStrIndexW.Empty();
  if (szNameW == NULL)
    return E_POINTER;
  //skip blanks
  while (*szNameW == L' ' || *szNameW == L'\t')
    szNameW++;
  //parse name
  bValidChars = bPrevWasSpace = FALSE;
  while (*szNameW != 0 && *szNameW != L'[' && *szNameW != L'\t')
  {
    if (*szNameW > L' ' && *szNameW != L'.')
    {
      if (bPrevWasSpace != FALSE)
      {
        if (cStrNameW.ConcatN(" ", 1) == FALSE)
          return E_OUTOFMEMORY;
        bPrevWasSpace = FALSE;
      }
      if (cStrNameW.ConcatN(szNameW, 1) == FALSE)
        return E_OUTOFMEMORY;
      bValidChars = TRUE;
    }
    else if (*szNameW == '.')
    {
      if (bPrevWasSpace != FALSE)
      {
        if (cStrNameW.ConcatN(" ", 1) == FALSE)
          return E_OUTOFMEMORY;
        bPrevWasSpace = FALSE;
      }
      if (cStrNameW.ConcatN("_", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    else if (*szNameW == L' ' || *szNameW == L'\t')
    {
      bPrevWasSpace = TRUE;
    }
    *szNameW++;
  }
  if (cStrNameW.IsEmpty() != FALSE || bValidChars == FALSE)
    return E_INVALIDARG;
  //skip blanks
  while (*szNameW == L' ' || *szNameW == L'\t')
    szNameW++;
  //parse index (if any)
  if (*szNameW == L'[')
  {
    szNameW++;
    //skip blanks
    while (*szNameW == L' ' || *szNameW == L'\t')
      szNameW++;
    //parse index
    bValidChars = bPrevWasSpace = FALSE;
    while (*szNameW != 0 && *szNameW != L']')
    {
      if (*szNameW > L' ' && *szNameW != L'.')
      {
        if (bPrevWasSpace != FALSE)
        {
          if (cStrIndexW.ConcatN(" ", 1) == FALSE)
            return E_OUTOFMEMORY;
          bPrevWasSpace = FALSE;
        }
        if (cStrIndexW.ConcatN(szNameW, 1) == FALSE)
          return E_OUTOFMEMORY;
        bValidChars = TRUE;
      }
      else if (*szNameW == '.')
      {
        if (bPrevWasSpace != FALSE)
        {
          if (cStrIndexW.ConcatN(" ", 1) == FALSE)
            return E_OUTOFMEMORY;
          bPrevWasSpace = FALSE;
        }
        if (cStrIndexW.ConcatN("_", 1) == FALSE)
          return E_OUTOFMEMORY;
      }
      else if (*szNameW == L' ' || *szNameW == L'\t')
      {
        bPrevWasSpace = TRUE;
      }
      *szNameW++;
    }
    if (*szNameW != L']' || (cStrIndexW.IsEmpty() == FALSE && bValidChars == FALSE))
      return E_INVALIDARG;
    szNameW++; //skip ']'
    if (cStrIndexW.IsEmpty() != FALSE)
    {
      if (cStrIndexW.CopyN(L"\x05", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  //skip blanks and check for end
  while (*szNameW == L' ' || *szNameW == L'\t')
    szNameW++;
  return (*szNameW == 0) ? S_OK : E_INVALIDARG;
}
