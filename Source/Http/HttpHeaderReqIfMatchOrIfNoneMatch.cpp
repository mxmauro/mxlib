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
  return;
}

HRESULT CHttpHeaderReqIfXXXMatchBase::Parse(_In_z_ LPCSTR szValueA)
{
  CEntity *lpEntity;
  LPCSTR szStartA;
  CStringA cStrTempA;
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
    if (*szValueA == ',')
      goto skip_null_listitem;

    bGotItem = TRUE;

    //is weak?
    bIsWeak = FALSE;
    if (szValueA[0] == 'W' && szValueA[1] == '/')
    {
      szValueA += 2;
      bIsWeak = TRUE;
    }

    //get entity
    if (*szValueA != '"')
    {
      szValueA = SkipUntil(szStartA = szValueA, " \t");
      if (szValueA == szStartA)
        return MX_E_InvalidData;
      if (cStrTempA.CopyN(szStartA, (SIZE_T)(szValueA - szStartA)) == FALSE)
        return E_OUTOFMEMORY;
    }
    else
    {
      hRes = GetQuotedString(cStrTempA, szValueA);
      if (FAILED(hRes))
        return hRes;
      if (cStrTempA.IsEmpty() != FALSE)
        return MX_E_InvalidData;
    }

    //get entity
    hRes = AddEntity((LPCSTR)cStrTempA, cStrTempA.GetLength(), &lpEntity);
    if (FAILED(hRes))
      return hRes;
    lpEntity->SetWeak(bIsWeak);

    //skip spaces
    szValueA = SkipSpaces(szValueA);

skip_null_listitem:
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

HRESULT CHttpHeaderReqIfXXXMatchBase::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  CStringA cStrTempA;
  SIZE_T i, nCount;
  CEntity *lpEntity;

  cStrDestA.Empty();

  nCount = cEntitiesList.GetCount();
  for (i = 0; i < nCount; i++)
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
    if (CHttpCommon::BuildQuotedString(cStrTempA, (LPCSTR)(lpEntity->GetTag()), lpEntity->cStrTagA.GetLength(),
                                       FALSE) == FALSE ||
        cStrDestA.ConcatN((LPCSTR)cStrTempA, cStrTempA.GetLength()) == FALSE)
    {
      return E_OUTOFMEMORY;
    }
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderReqIfXXXMatchBase::AddEntity(_In_z_ LPCSTR szTagA, _In_opt_ SIZE_T nTagLen,
                                                _Out_opt_ CEntity **lplpEntity)
{
  TAutoDeletePtr<CEntity> cNewEntity;
  SIZE_T i, nCount;
  HRESULT hRes;

  if (lplpEntity != NULL)
    *lplpEntity = NULL;
  if (nTagLen == (SIZE_T)-1)
    nTagLen = StrLenA(szTagA);
  if (nTagLen == 0)
    return MX_E_InvalidData;
  if (szTagA == NULL)
    return E_POINTER;
  //check for duplicates
  nCount = cEntitiesList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (cEntitiesList[i]->cStrTagA.GetLength() == nTagLen &&
        StrNCompareA(cEntitiesList[i]->GetTag(), szTagA, nTagLen, TRUE) == 0)
    {
      return MX_E_AlreadyExists;
    }
  }
  //create new type
  cNewEntity.Attach(MX_DEBUG_NEW CEntity());
  if (!cNewEntity)
    return E_OUTOFMEMORY;
  hRes = cNewEntity->SetTag(szTagA, nTagLen);
  if (FAILED(hRes))
    return hRes;
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

CHttpHeaderReqIfXXXMatchBase::CEntity* CHttpHeaderReqIfXXXMatchBase::GetEntity(_In_z_ LPCSTR szTagA) const
{
  SIZE_T i, nCount;

  if (szTagA != NULL && *szTagA != 0)
  {
    nCount = cEntitiesList.GetCount();
    for (i = 0; i < nCount; i++)
    {
      if (StrCompareA(cEntitiesList[i]->GetTag(), szTagA, TRUE) == 0)
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

HRESULT CHttpHeaderReqIfXXXMatchBase::CEntity::SetTag(_In_z_ LPCSTR szTagA, _In_opt_ SIZE_T nTagLen)
{
  SIZE_T i;

  if (nTagLen == (SIZE_T)-1)
    nTagLen = StrLenA(szTagA);
  if (nTagLen == 0)
    return MX_E_InvalidData;
  if (szTagA == NULL)
    return E_POINTER;
  //check for invalid characters
  for (i = 0; i < nTagLen; i++)
  {
    if (((UCHAR)szTagA[i]) < 0x20 || ((UCHAR)szTagA[i]) > 0x7E || szTagA[i] == '"')
      return MX_E_InvalidData;
  }
  //check completed
  if (cStrTagA.CopyN(szTagA, nTagLen) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

LPCSTR CHttpHeaderReqIfXXXMatchBase::CEntity::GetTag() const
{
  return (LPCSTR)cStrTagA;
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
