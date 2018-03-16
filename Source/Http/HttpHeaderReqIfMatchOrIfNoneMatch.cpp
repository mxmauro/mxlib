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
#include "..\..\Include\Http\HttpHeaderReqIfMatchOrIfNoneMatch.h"

//-----------------------------------------------------------

namespace MX {

CHttpHeaderReqIfXXXMatchBase::CHttpHeaderReqIfXXXMatchBase(_In_opt_ BOOL _bIsMatch) : CHttpHeaderBase()
{
  bIsMatch = _bIsMatch;
  return;
}

CHttpHeaderReqIfXXXMatchBase::~CHttpHeaderReqIfXXXMatchBase()
{
  cEntitiesList.RemoveAllElements();
  return;
}

HRESULT CHttpHeaderReqIfXXXMatchBase::Parse(_In_z_ LPCSTR szValueA)
{
  CEntity *lpEntity;
  LPCSTR szStartA;
  CStringA cStrTempA, cStrTempA_2;
  CStringW cStrTempW;
  BOOL bIsWeak, bGotItem;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;
  //parse
  bGotItem = FALSE;
  do
  {
    //skip spaces
    szValueA = SkipSpaces(szValueA);
    if (*szValueA == 0)
      break;
    bGotItem = TRUE;
    //is weak?
    bIsWeak = FALSE;
    if (szValueA[0] == 'W' && szValueA[1] == '/')
    {
      szValueA += 2;
      bIsWeak = TRUE;
    }
    //get entity
    szValueA = Advance(szStartA = szValueA, ",");
    if (cStrTempA.CopyN(szStartA, (SIZE_T)(szValueA-szStartA)) == FALSE)
      return E_OUTOFMEMORY;
    hRes = AddEntity((LPSTR)cStrTempA, &lpEntity);
    if (FAILED(hRes))
      return hRes;
    lpEntity->SetWeak(bIsWeak);
    //skip spaces
    szValueA = SkipSpaces(szValueA);
    //check for separator or end
    if (*szValueA == ',')
      szValueA++;
    else if (*szValueA != 0)
      return MX_E_InvalidData;
  }
  while (*szValueA != 0);
  //done
  return (bGotItem != FALSE) ? S_OK : MX_E_InvalidData;
}

HRESULT CHttpHeaderReqIfXXXMatchBase::Build(_Inout_ CStringA &cStrDestA)
{
  SIZE_T i, nCount;
  CStringA cStrTempA;
  CEntity *lpEntity;
  HRESULT hRes;

  cStrDestA.Empty();
  nCount = cEntitiesList.GetCount();
  for (i=0; i<nCount; i++)
  {
    lpEntity = cEntitiesList.GetElementAt(i);
    if (cStrDestA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(",", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    //weak
    if (lpEntity->GetWeak() != FALSE)
    {
      if (cStrDestA.ConcatN("W/", 2) == FALSE)
        return E_OUTOFMEMORY;
    }
    //entity
    hRes = Utf8_Encode(cStrTempA, lpEntity->GetTag());
    if (FAILED(hRes))
      return hRes;
    if (CHttpCommon::EncodeQuotedString(cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
    if (cStrDestA.AppendFormat("\"%s\"", (LPCSTR)cStrTempA) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderReqIfXXXMatchBase::AddEntity(_In_z_ LPCSTR szTagA, _Out_opt_ CEntity **lplpEntity)
{
  TAutoDeletePtr<CEntity> cNewEntity;
  SIZE_T i, nCount;
  HRESULT hRes;

  if (lplpEntity != NULL)
    *lplpEntity = NULL;
  if (szTagA == NULL)
    return E_POINTER;
  //create new type
  cNewEntity.Attach(MX_DEBUG_NEW CEntity());
  if (!cNewEntity)
    return E_OUTOFMEMORY;
  hRes = cNewEntity->SetTag(szTagA);
  if (FAILED(hRes))
    return hRes;
  //check if already exists in list
  nCount = cEntitiesList.GetCount();
  for (i=0; i<nCount; i++)
  {
    if (StrCompareW(cEntitiesList[i]->GetTag(), cNewEntity->GetTag()) == 0)
      return MX_E_AlreadyExists;
  }
  //add to list
  if (cEntitiesList.AddElement(cNewEntity.Get()) == FALSE)
    return E_OUTOFMEMORY;
  //done
  if (lplpEntity != NULL)
    *lplpEntity = cNewEntity.Detach();
  else
    cNewEntity.Detach();
  return S_OK;
}

HRESULT CHttpHeaderReqIfXXXMatchBase::AddEntity(_In_z_ LPCWSTR szTagW, _Out_opt_ CEntity **lplpEntity)
{
  TAutoDeletePtr<CEntity> cNewEntity;
  SIZE_T i, nCount;
  HRESULT hRes;

  if (lplpEntity != NULL)
    *lplpEntity = NULL;
  if (szTagW == NULL)
    return E_POINTER;
  //create new type
  cNewEntity.Attach(MX_DEBUG_NEW CEntity());
  if (!cNewEntity)
    return E_OUTOFMEMORY;
  hRes = cNewEntity->SetTag(szTagW);
  if (FAILED(hRes))
    return hRes;
  //check if already exists in list
  nCount = cEntitiesList.GetCount();
  for (i=0; i<nCount; i++)
  {
    if (StrCompareW(cEntitiesList[i]->GetTag(), cNewEntity->GetTag()) == 0)
      return MX_E_AlreadyExists;
  }
  //add to list
  if (cEntitiesList.AddElement(cNewEntity.Get()) == FALSE)
    return E_OUTOFMEMORY;
  //done
  if (lplpEntity != NULL)
    *lplpEntity = cNewEntity.Detach();
  else
    cNewEntity.Detach();
  return S_OK;
}

SIZE_T CHttpHeaderReqIfXXXMatchBase::GetEntitiesCount() const
{
  return cEntitiesList.GetCount();
}

CHttpHeaderReqIfXXXMatchBase::CEntity* CHttpHeaderReqIfXXXMatchBase::GetEntity(_In_ SIZE_T nIndex) const
{
  return (nIndex < cEntitiesList.GetCount()) ? cEntitiesList.GetElementAt(nIndex) : NULL;
}

CHttpHeaderReqIfXXXMatchBase::CEntity* CHttpHeaderReqIfXXXMatchBase::GetEntity(_In_z_ LPCWSTR szTagW) const
{
  SIZE_T i, nCount;

  if (szTagW != NULL && szTagW[0] != 0)
  {
    nCount = cEntitiesList.GetCount();
    for (i=0; i<nCount; i++)
    {
      if (StrCompareW(cEntitiesList[i]->GetTag(), szTagW, TRUE) == 0)
        return cEntitiesList.GetElementAt(i);
    }
  }
  return NULL;
}

//-----------------------------------------------------------
//-----------------------------------------------------------

CHttpHeaderReqIfXXXMatchBase::CEntity::CEntity() : CBaseMemObj()
{
  bIsWeak = FALSE;
  return;
}

CHttpHeaderReqIfXXXMatchBase::CEntity::~CEntity()
{
  return;
}

HRESULT CHttpHeaderReqIfXXXMatchBase::CEntity::SetTag(_In_z_ LPCSTR szTagA)
{
  LPCSTR szStartA, szEndA;
  CStringA cStrTempA;
  HRESULT hRes;

  if (szTagA == NULL)
    return E_POINTER;
  //skip spaces
  szTagA = CHttpHeaderBase::SkipSpaces(szTagA);
  //get entity
  if (szTagA[0] == '"')
  {
    szTagA = CHttpHeaderBase::Advance(szStartA = ++szTagA, "\"");
    if (*szTagA != '"')
      return MX_E_InvalidData;
    szEndA = szTagA;
    szTagA++;
  }
  else if (szTagA[0] != '*')
  {
    szTagA = CHttpHeaderBase::Advance(szStartA = szTagA, " \t;,");
    if (szTagA == szStartA)
      return MX_E_InvalidData;
    szEndA = szTagA;
  }
  else
  {
    szStartA = szTagA++;
    szEndA = szTagA;
  }
  if (szEndA == szStartA)
    return MX_E_InvalidData;
  //skip spaces and check for end
  if (*CHttpHeaderBase::SkipSpaces(szTagA) != 0)
    return MX_E_InvalidData;
  //set new value
  if (cStrTempA.CopyN(szStartA, (SIZE_T)(szEndA-szStartA)) == FALSE)
    return E_OUTOFMEMORY;
  hRes = Utf8_Decode(cStrTagW, (LPSTR)cStrTempA);
  //done
  return hRes;
}

HRESULT CHttpHeaderReqIfXXXMatchBase::CEntity::SetTag(_In_z_ LPCWSTR szTagW)
{
  LPCWSTR szStartW, szEndW;

  if (szTagW == NULL)
    return E_POINTER;
  //skip spaces
  szTagW = CHttpHeaderBase::SkipSpaces(szTagW);
  //get entity
  if (szTagW[0] == L'"')
  {
    szTagW = CHttpHeaderBase::Advance(szStartW = ++szTagW, L"\"");
    if (*szTagW != L'"')
      return MX_E_InvalidData;
    szEndW = szTagW;
    szTagW++;
  }
  else if (szTagW[0] != L'*')
  {
    szTagW = CHttpHeaderBase::Advance(szStartW = szTagW, L" \t;,");
    if (szTagW == szStartW)
      return MX_E_InvalidData;
    szEndW = szTagW;
  }
  else
  {
    szStartW = szTagW++;
    szEndW = szTagW;
  }
  if (szEndW == szStartW)
    return MX_E_InvalidData;
  //skip spaces and check for end
  if (*CHttpHeaderBase::SkipSpaces(szTagW) != 0)
    return MX_E_InvalidData;
  //set new value
  if (cStrTagW.CopyN(szStartW, (SIZE_T)(szEndW-szStartW)) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCWSTR CHttpHeaderReqIfXXXMatchBase::CEntity::GetTag() const
{
  return (LPCWSTR)cStrTagW;
}

HRESULT CHttpHeaderReqIfXXXMatchBase::CEntity::SetWeak(_In_ BOOL _bIsWeak)
{
  bIsWeak = _bIsWeak;
  return S_OK;
}

BOOL CHttpHeaderReqIfXXXMatchBase::CEntity::GetWeak() const
{
  return bIsWeak;
}

} //namespace MX
