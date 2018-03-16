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

#define MAX_SUBINDEXES_COUNT 16

//-----------------------------------------------------------

static HRESULT ParseNameAndIndexes(_In_z_ LPCWSTR szNameW, _Inout_ MX::CStringW &cStrNameW,
                                   _Inout_ MX::TArrayListWithFree<LPCWSTR> &aSubIndexesList);
static BOOL __inline IsNumeric(_In_z_ LPCWSTR szNumberW);
static int NumericCompare(_In_z_ LPCWSTR szNumber1, _In_z_ LPCWSTR szNumber2);

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

CHttpBodyParserFormBase::CField* CHttpBodyParserFormBase::GetField(_In_ SIZE_T nIndex) const
{
  return (nIndex < cFieldsList.GetCount()) ? cFieldsList.GetElementAt(nIndex) : NULL;
}

SIZE_T CHttpBodyParserFormBase::GetFileFieldsCount() const
{
  return cFileFieldsList.GetCount();
}

CHttpBodyParserFormBase::CFileField* CHttpBodyParserFormBase::GetFileField(_In_ SIZE_T nIndex) const
{
  return (nIndex < cFileFieldsList.GetCount()) ? cFileFieldsList.GetElementAt(nIndex) : NULL;
}

HRESULT CHttpBodyParserFormBase::AddField(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szValueW)
{
  CField *lpField, *lpChildField;
  CStringW cStrNameW, cStrIndexW;
  TArrayListWithFree<LPCWSTR> aSubIndexesList;
  SIZE_T i, nCurrIdx, nCount;
  LPCWSTR sW, szIdxW;
  HRESULT hRes;

  hRes = ParseNameAndIndexes(szNameW, cStrNameW, aSubIndexesList);
  if (FAILED(hRes))
    return hRes;
  if (szValueW == NULL)
    return E_POINTER;
  //locate field with name
  lpField = lpChildField = NULL;
  for (nCount = cFieldsList.GetCount(); nCount > 0; nCount--)
  {
    lpField = cFieldsList.GetElementAt(nCount-1);
    if (StrCompareW((LPWSTR)cStrNameW, lpField->GetName(), TRUE) == 0)
      break;
  }
  if (nCount == 0)
  {
    //not found -> create
    lpField = MX_DEBUG_NEW CField();
    if (!lpField)
      return E_OUTOFMEMORY;
    if (lpField->cStrNameW.Copy((LPCWSTR)cStrNameW) == FALSE)
      return E_OUTOFMEMORY;
    //add to list
    if (cFieldsList.AddElement(lpField) == FALSE)
    {
      delete lpField;
      return E_OUTOFMEMORY;
    }
  }
  //check indexes
  for (nCurrIdx=0; nCurrIdx<aSubIndexesList.GetCount(); nCurrIdx++)
  {
    lpField->ClearValue();
    //----
    sW = aSubIndexesList.GetElementAt(nCurrIdx);
    if (sW == NULL)
    {
      //if the item has an empty index, find next index
      cStrIndexW.Empty();
      for (i=0; i<lpField->GetSubIndexesCount(); i++)
      {
        lpChildField = lpField->GetSubIndexAt(i);

        szIdxW = lpChildField->GetName();
        //check if numeric index
        if (IsNumeric(szIdxW) != FALSE)
        {
          //yes, it is a numeric index, store in temp index var if greater
          if (cStrIndexW.IsEmpty() != FALSE || NumericCompare((LPCWSTR)cStrIndexW, szIdxW) < 0)
          {
            if (cStrIndexW.Copy(szIdxW) == FALSE)
              return E_OUTOFMEMORY;
          }
        }
      }
      //calculate new index (increment the biggest by 1)
      if (cStrIndexW.IsEmpty() == FALSE)
      {
        for (szIdxW=(LPCWSTR)cStrIndexW+(cStrIndexW.GetLength()-1); szIdxW>=(LPCWSTR)cStrIndexW; szIdxW--)
        {
          *((LPWSTR)szIdxW) += 1;
          if (*szIdxW <= L'9')
            break;
          *((LPWSTR)szIdxW) -= 10;
        }
        if (szIdxW < (LPCWSTR)cStrIndexW)
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
      //set index
      sW = (LPCWSTR)cStrIndexW;
    }
    //look for the subindex
    for (i=0; i<lpField->GetSubIndexesCount(); i++)
    {
      lpChildField = lpField->GetSubIndexAt(i);
      if (StrCompareW(sW, lpChildField->GetName(), TRUE) == 0)
        break;
    }
    if (i < lpField->GetSubIndexesCount())
    {
      //index exists, clear value
      lpChildField->ClearValue();
    }
    else
    {
      //create new subindex
      lpChildField = MX_DEBUG_NEW CField();
      if (!lpChildField)
        return E_OUTOFMEMORY;
      if (lpChildField->cStrNameW.Copy(sW) == FALSE)
        return E_OUTOFMEMORY;
      //add to list
      if (lpField->aSubIndexesItems.AddElement(lpChildField) == FALSE)
      {
        delete lpChildField;
        return E_OUTOFMEMORY;
      }
    }
    //go to next
    lpField = lpChildField;
  }
  //assign the value to the field
#pragma warning(suppress : 6011)
  if (lpField->cStrValueW.Copy(szValueW) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

HRESULT CHttpBodyParserFormBase::AddFileField(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szFileNameW, _In_z_ LPCSTR szTypeA,
                                              _In_ HANDLE hFile)
{
  CFileField *lpField, *lpChildField;
  CStringW cStrNameW, cStrIndexW;
  TArrayListWithFree<LPCWSTR> aSubIndexesList;
  SIZE_T i, nCurrIdx, nCount;
  LPCWSTR sW, szIdxW;
  HRESULT hRes;

  hRes = ParseNameAndIndexes(szNameW, cStrNameW, aSubIndexesList);
  if (FAILED(hRes))
    return hRes;
  if (szFileNameW == NULL || szTypeA == NULL)
    return E_POINTER;
  if (hFile == NULL || hFile == INVALID_HANDLE_VALUE)
    return E_INVALIDARG;
  //locate field with name
  lpField = lpChildField = NULL;
  for (nCount = cFileFieldsList.GetCount(); nCount > 0; nCount--)
  {
    lpField = cFileFieldsList.GetElementAt(nCount-1);
    if (StrCompareW((LPWSTR)cStrNameW, lpField->GetName(), TRUE) == 0)
      break;
  }
  if (nCount == 0)
  {
    //not found -> create
    lpField = MX_DEBUG_NEW CFileField();
    if (!lpField)
      return E_OUTOFMEMORY;
    if (lpField->cStrNameW.Copy((LPCWSTR)cStrNameW) == FALSE)
      return E_OUTOFMEMORY;
    //add to list
    if (cFileFieldsList.AddElement(lpField) == FALSE)
    {
      delete lpField;
      return E_OUTOFMEMORY;
    }
  }
  //check indexes
  for (nCurrIdx=0; nCurrIdx<aSubIndexesList.GetCount(); nCurrIdx++)
  {
    lpField->ClearValue();
    //----
    sW = aSubIndexesList.GetElementAt(nCurrIdx);
    if (sW == NULL)
    {
      //if the item has an empty index, find next index
      cStrIndexW.Empty();
      for (i=0; i<lpField->GetSubIndexesCount(); i++)
      {
        lpChildField = lpField->GetSubIndexAt(i);

        szIdxW = lpChildField->GetName();
        //check if numeric index
        if (IsNumeric(szIdxW) != FALSE)
        {
          //yes, it is a numeric index, store in temp index var if greater
          if (cStrIndexW.IsEmpty() != FALSE || NumericCompare((LPCWSTR)cStrIndexW, szIdxW) < 0)
          {
            if (cStrIndexW.Copy(szIdxW) == FALSE)
              return E_OUTOFMEMORY;
          }
        }
      }
      //calculate new index (increment the biggest by 1)
      if (cStrIndexW.IsEmpty() == FALSE)
      {
        for (szIdxW=(LPCWSTR)cStrIndexW+(cStrIndexW.GetLength()-1); szIdxW>=(LPCWSTR)cStrIndexW; szIdxW--)
        {
          *((LPWSTR)szIdxW) += 1;
          if (*szIdxW <= L'9')
            break;
          *((LPWSTR)szIdxW) -= 10;
        }
        if (szIdxW < (LPCWSTR)cStrIndexW)
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
      //set index
      sW = (LPCWSTR)cStrIndexW;
    }
    //look for the subindex
    for (i=0; i<lpField->GetSubIndexesCount(); i++)
    {
      lpChildField = lpField->GetSubIndexAt(i);
      if (StrCompareW(sW, lpChildField->GetName(), TRUE) == 0)
        break;
    }
    if (i < lpField->GetSubIndexesCount())
    {
      //index exists, clear value
      lpChildField->ClearValue();
    }
    else
    {
      //create new subindex
      lpChildField = MX_DEBUG_NEW CFileField();
      if (!lpChildField)
        return E_OUTOFMEMORY;
      if (lpChildField->cStrNameW.Copy(sW) == FALSE)
        return E_OUTOFMEMORY;
      //add to list
      if (lpField->aSubIndexesItems.AddElement(lpChildField) == FALSE)
      {
        delete lpChildField;
        return E_OUTOFMEMORY;
      }
    }
    //go to next
    lpField = lpChildField;
  }
  //assign the values to the field
#pragma warning(suppress : 6011)
  if (lpField->cStrFileNameW.Copy(szFileNameW) == FALSE)
    return E_OUTOFMEMORY;
#pragma warning(suppress : 6011)
  if (lpField->cStrTypeA.Copy(szTypeA) == FALSE)
    return E_OUTOFMEMORY;
  lpField->hFile = hFile;
  //done
  return S_OK;
}

//-----------------------------------------------------------

CHttpBodyParserFormBase::CField::CField() : CBaseMemObj()
{
  return;
}

VOID CHttpBodyParserFormBase::CField::ClearValue()
{
  cStrValueW.Empty();
  return;
}

//-----------------------------------------------------------

CHttpBodyParserFormBase::CFileField::CFileField() : CBaseMemObj()
{
  hFile = NULL;
  return;
}

CHttpBodyParserFormBase::CFileField::~CFileField()
{
  if (hFile != NULL && hFile != INVALID_HANDLE_VALUE)
    ::CloseHandle(hFile);
  cStrFileNameW.Empty();
  cStrTypeA.Empty();
  return;
}

VOID CHttpBodyParserFormBase::CFileField::ClearValue()
{
  if (hFile != NULL && hFile != INVALID_HANDLE_VALUE)
    ::CloseHandle(hFile);
  return;
}

} //namespace MX

//-----------------------------------------------------------

static HRESULT ParseNameAndIndexes(_In_z_ LPCWSTR szNameW, _Inout_ MX::CStringW &cStrNameW,
                                   _Inout_ MX::TArrayListWithFree<LPCWSTR> &aSubIndexesList)
{
  MX::CStringW cStrCurrSubIndexW;
  BOOL bValidChars, bPrevWasSpace;
  LPCWSTR sW;

  cStrNameW.Empty();
  aSubIndexesList.RemoveAllElements();
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
    szNameW++;
  }
  if (cStrNameW.IsEmpty() != FALSE || bValidChars == FALSE)
    return E_INVALIDARG;
parse_index:
  //skip blanks
  while (*szNameW == L' ' || *szNameW == L'\t')
    szNameW++;
  //parse indexes (if any)
  if (*szNameW == L'[')
  {
    //too much indexes?
    if (aSubIndexesList.GetCount() >= MAX_SUBINDEXES_COUNT)
      return MX_E_Unsupported;
    //parse
    cStrCurrSubIndexW.Empty();
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
          if (cStrCurrSubIndexW.ConcatN(" ", 1) == FALSE)
            return E_OUTOFMEMORY;
          bPrevWasSpace = FALSE;
        }
        if (cStrCurrSubIndexW.ConcatN(szNameW, 1) == FALSE)
          return E_OUTOFMEMORY;
        bValidChars = TRUE;
      }
      else if (*szNameW == '.')
      {
        if (bPrevWasSpace != FALSE)
        {
          if (cStrCurrSubIndexW.ConcatN(" ", 1) == FALSE)
            return E_OUTOFMEMORY;
          bPrevWasSpace = FALSE;
        }
        if (cStrCurrSubIndexW.ConcatN("_", 1) == FALSE)
          return E_OUTOFMEMORY;
      }
      else if (*szNameW == L' ' || *szNameW == L'\t')
      {
        bPrevWasSpace = TRUE;
      }
      szNameW++;
    }
    if (*szNameW != L']' || (cStrCurrSubIndexW.IsEmpty() == FALSE && bValidChars == FALSE))
      return E_INVALIDARG;
    szNameW++; //skip ']'
    //check if numeric index and remove leading zeroes
    if (cStrCurrSubIndexW.IsEmpty() == FALSE && IsNumeric((LPCWSTR)cStrCurrSubIndexW) != FALSE)
    {
      for (sW=(LPCWSTR)cStrCurrSubIndexW; *sW == L'0'; sW++);
      if (*sW == 0)
        sW--; //if all are zeroes, leave one
      cStrCurrSubIndexW.Delete(0, (SIZE_T)(sW-(LPCWSTR)cStrCurrSubIndexW));
    }
    //add to subindex list
    sW = cStrCurrSubIndexW.Detach();
    if (aSubIndexesList.AddElement(sW) == FALSE)
    {
      MX_FREE(sW);
      return E_OUTOFMEMORY;
    }
    //restart
    goto parse_index;
  }
  //skip blanks and check for end
  return (*szNameW == 0) ? S_OK : E_INVALIDARG;
}

static BOOL __inline IsNumeric(_In_z_ LPCWSTR szNumberW)
{
  while (*szNumberW >= L'0' && *szNumberW <= L'9')
    szNumberW++;
  return (*szNumberW == 0) ? TRUE : FALSE;
}

static int NumericCompare(_In_z_ LPCWSTR szNumber1, _In_z_ LPCWSTR szNumber2)
{
  SIZE_T nDigitsLeft[2];

  /* NOTE: No need of this because numbers won't have leading zeroes
  while (*szNumber1 == L'0')
    szNumber1++;
  while (*szNumber2 == L'0')
    szNumber2++;
  */
  while (*szNumber1 != 0 && *szNumber1 == *szNumber2)
  {
    szNumber1++;
    szNumber2++;
  }
  nDigitsLeft[0] = MX::StrLenW(szNumber1);
  nDigitsLeft[1] = MX::StrLenW(szNumber2);
  if (nDigitsLeft[0] == 0 && nDigitsLeft[1] == 0)
    return 0; //both numbers are equal
  if (nDigitsLeft[0] < nDigitsLeft[1])
    return -1; //less digits means smaller number
  if (nDigitsLeft[0] > nDigitsLeft[1])
    return 1; //more digits means bigger number
  //same digits count, compare digit
  return (*szNumber1 < *szNumber2) ? -1 : 1;
}
