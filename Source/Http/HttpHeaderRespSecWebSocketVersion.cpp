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
#include "..\..\Include\Http\HttpHeaderRespSecWebSocketVersion.h"

//-----------------------------------------------------------

static int VersionCompareFunc(_In_ LPVOID lpContext, _In_ int *lpElem1, _In_ int *lpElem2);

//-----------------------------------------------------------

namespace MX {

CHttpHeaderRespSecWebSocketVersion::CHttpHeaderRespSecWebSocketVersion() : CHttpHeaderBase()
{
  return;
}

CHttpHeaderRespSecWebSocketVersion::~CHttpHeaderRespSecWebSocketVersion()
{
  return;
}

HRESULT CHttpHeaderRespSecWebSocketVersion::Parse(_In_z_ LPCSTR szValueA)
{
  BOOL bGotItem;
  int nVersion;
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

    //get value
    nVersion = 0;
    while (*szValueA >= '0' && *szValueA <= '9')
    {
      nVersion = nVersion * 10 + (int)((*szValueA) - '0');
      if (nVersion > 255)
        return MX_E_InvalidData;
      szValueA++;
    }
    hRes = AddVersion(nVersion);
    if (FAILED(hRes))
      return hRes;

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

HRESULT CHttpHeaderRespSecWebSocketVersion::Build(_Inout_ CStringA &cStrDestA, _In_ eBrowser nBrowser)
{
  SIZE_T i, nCount;

  cStrDestA.Empty();

  nCount = cVersionsList.GetCount();
  for (i = 0; i < nCount; i++)
  {
    if (cStrDestA.IsEmpty() == FALSE)
    {
      if (cStrDestA.ConcatN(",", 1) == FALSE)
        return E_OUTOFMEMORY;
    }
    if (cStrDestA.AppendFormat("%ld", cVersionsList.GetElementAt(i)) == FALSE)
      return E_OUTOFMEMORY;
  }
  //done
  return S_OK;
}

HRESULT CHttpHeaderRespSecWebSocketVersion::AddVersion(_In_ int nVersion)
{
  if (nVersion < 0 || nVersion > 255)
    return E_INVALIDARG;

  //add version to list
  if (cVersionsList.SortedInsert(nVersion, &VersionCompareFunc, NULL, TRUE) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

SIZE_T CHttpHeaderRespSecWebSocketVersion::GetVersionsCount() const
{
  return cVersionsList.GetCount();
}

int CHttpHeaderRespSecWebSocketVersion::GetVersion(_In_ SIZE_T nIndex) const
{
  return (nIndex < cVersionsList.GetCount()) ? cVersionsList.GetElementAt(nIndex) : -1;
}

} //namespace MX

//-----------------------------------------------------------

static int VersionCompareFunc(_In_ LPVOID lpContext, _In_ int *lpElem1, _In_ int *lpElem2)
{
  //DESC order
  if ((*lpElem1) > (*lpElem2))
    return -1;
  if ((*lpElem1) < (*lpElem2))
    return 1;
  return 0;
}
