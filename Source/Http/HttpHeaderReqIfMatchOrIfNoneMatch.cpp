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
#include "..\..\Include\Http\HttpHeaderReqIfMatchOrIfNoneMatch.h"
#include "..\..\Include\AutoPtr.h"

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

HRESULT CHttpHeaderReqIfXXXMatchBase::Parse(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen)
{
  CEntity *lpEntity;
  LPCSTR szValueEndA, szStartA;
  CStringA cStrTempA;
  BOOL bIsWeak, bGotItem;
  HRESULT hRes;

  if (szValueA == NULL)
    return E_POINTER;

  if (nValueLen == (SIZE_T)-1)
    nValueLen = StrLenA(szValueA);
  szValueEndA = szValueA + nValueLen;

  //parse
  bGotItem = FALSE;
  do
  {
    //skip spaces
    szValueA = SkipSpaces(szValueA, szValueEndA);
    if (szValueA >= szValueEndA)
      break;
    if (*szValueA == ',')
      goto skip_null_listitem;

    bGotItem = TRUE;

    //is weak?
    bIsWeak = FALSE;
    if ((SIZE_T)(szValueEndA - szValueA) >= 2 && szValueA[0] == 'W' && szValueA[1] == '/')
    {
      szValueA += 2;
      bIsWeak = TRUE;
    }

    //get entity
    if (szValueA < szValueEndA && *szValueA != '"')
    {
      szValueA = SkipUntil(szStartA = szValueA, szValueEndA, " \t");
      if (szValueA == szStartA)
        return MX_E_InvalidData;
      if (cStrTempA.CopyN(szStartA, (SIZE_T)(szValueA - szStartA)) == FALSE)
        return E_OUTOFMEMORY;
    }
    else
    {
      hRes = GetQuotedString(cStrTempA, szValueA, szValueEndA);
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
    szValueA = SkipSpaces(szValueA, szValueEndA);

skip_null_listitem:
    //check for separator or end
    if (szValueA < szValueEndA)
    {
      if (*szValueA == ',')
        szValueA++;
      else
        return MX_E_InvalidData;
    }
  }
  while (szValueA < szValueEndA);

  //do we got one?
  if (bGotItem == FALSE)
    return MX_E_InvalidData;

  //done
  return S_OK;
}

HRESULT CHttpHeaderReqIfXXXMatchBase::Build(_Inout_ CStringA &cStrDestA, _In_ Http::eBrowser nBrowser)
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
    if (Http::BuildQuotedString(cStrTempA, (LPCSTR)(lpEntity->GetTag()), lpEntity->cStrTagA.GetLength(),
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
