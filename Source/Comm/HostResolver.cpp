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
#include "HostResolverCommon.h"

#pragma comment(lib, "ws2_32.lib")

//-----------------------------------------------------------

static SIZE_T Helper_IPv6_Fill(__out LPWORD lpnAddr, __in_z LPCSTR szStrA, __in SIZE_T nLen);
static SIZE_T Helper_IPv6_Fill(__out LPWORD lpnAddr, __in_z LPCWSTR szStrW, __in SIZE_T nLen);

//-----------------------------------------------------------

#define __resolver (reinterpret_cast<MX::Internals::CIPAddressResolver*>(lpInternal))

//-----------------------------------------------------------

namespace MX {

CHostResolver::CHostResolver(__in_opt OnNotifyCallback cCallback, __in_opt LPVOID lpUserData) :
               TEventNotifyBase<CHostResolver>(cCallback, lpUserData)
{
  lpInternal = MX::Internals::CIPAddressResolver::Get();
  MemSet(&sAddr, 0, sizeof(sAddr));
  hErrorCode = MX_E_Timeout;
  return;
}

CHostResolver::~CHostResolver()
{
  Cancel();
  if (lpInternal != NULL)
    __resolver->Release();
  return;
}

HRESULT CHostResolver::Resolve(__in_z LPCSTR szHostNameA, __in int nDesiredFamily)
{
  if (szHostNameA == NULL)
    return E_POINTER;
  if (*szHostNameA == 0)
    return E_INVALIDARG;
  if (nDesiredFamily != AF_UNSPEC && nDesiredFamily != AF_INET && nDesiredFamily != AF_INET6)
    return E_INVALIDARG;
  if (lpInternal == NULL)
    return E_OUTOFMEMORY;
  return __resolver->ResolveAddr(&sAddr, szHostNameA, nDesiredFamily);
}

HRESULT CHostResolver::Resolve(__in_z LPCWSTR szHostNameW, __in int nDesiredFamily)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (szHostNameW == NULL)
    return E_POINTER;
  if (*szHostNameW == 0)
    return E_INVALIDARG;
  if (nDesiredFamily != AF_UNSPEC && nDesiredFamily != AF_INET && nDesiredFamily != AF_INET6)
    return E_INVALIDARG;
  if (lpInternal == NULL)
    return E_OUTOFMEMORY;
  hRes = Punycode_Encode(cStrTempA, szHostNameW);
  if (SUCCEEDED(hRes))
    hRes = __resolver->ResolveAddr(&sAddr, (LPCSTR)cStrTempA, nDesiredFamily);
  return hRes;
}

HRESULT CHostResolver::ResolveAsync(__in_z LPCSTR szHostNameA, __in int nDesiredFamily, __in DWORD dwTimeoutMs)
{
  if (lpInternal == NULL)
    return E_OUTOFMEMORY;
  return __resolver->Resolve(this, &sAddr, &hErrorCode, szHostNameA, nDesiredFamily, dwTimeoutMs);
}

HRESULT CHostResolver::ResolveAsync(__in_z LPCWSTR szHostNameW, __in int nDesiredFamily, __in DWORD dwTimeoutMs)
{
  CStringA cStrTempA;
  HRESULT hRes;

  if (szHostNameW == NULL)
    return E_POINTER;
  hRes = Punycode_Encode(cStrTempA, szHostNameW);
  if (SUCCEEDED(hRes))
    hRes = ResolveAsync((LPSTR)cStrTempA, nDesiredFamily, dwTimeoutMs);
  return hRes;
}

VOID CHostResolver::Cancel()
{
  if (lpInternal != NULL)
    __resolver->Cancel(this);
  return;
}

BOOL CHostResolver::IsValidIPV4(__in_z LPCSTR szAddressA, __in_opt SIZE_T nAddressLen, __out_opt sockaddr *lpAddress)
{
  SIZE_T i, j, nLen, nBlocksCount;
  DWORD dw, dwBase, dwValue, dwOldValue;
  LPCSTR sA;
  struct {
    LPCSTR szStartA;
    SIZE_T nLen;
  } sBlocks[4];
  sockaddr sTempAddr;

  if (lpAddress == NULL)
    lpAddress = &sTempAddr;
  MX::MemSet(lpAddress, 0, sizeof(sockaddr));
  lpAddress->sa_family = AF_INET;
  if (szAddressA == NULL)
    return FALSE;
  if (nAddressLen == (SIZE_T)-1)
    nAddressLen = strlen(szAddressA);
  for (nBlocksCount=0; nBlocksCount<4; nBlocksCount++)
  {
    sBlocks[nBlocksCount].szStartA = szAddressA;
    for (nLen=0; nLen<nAddressLen && szAddressA[nLen]!='.' && szAddressA[nLen]!='/' && szAddressA[nLen]!='%';
         nLen++);
      sBlocks[nBlocksCount].nLen = nLen;
    if (nLen >= nAddressLen || szAddressA[nLen] != '.')
    {
      nBlocksCount++;
      break;
    }
    if (nBlocksCount == 3)
      return FALSE;
    szAddressA += nLen+1;
    nAddressLen -= nLen+1;
  }
  if (nBlocksCount < 2)
    return FALSE;
  for (i=0; i<nBlocksCount; i++)
  {
    dwValue = 0;
    dwBase = 10;
    sA = sBlocks[i].szStartA;
    if (sBlocks[i].nLen == 0)
      return FALSE;
    if (sA[0] == '0' && sBlocks[i].nLen >= 2)
    {
      if (sA[1] == 'X' || sA[1] == 'x')
      {
        dwBase = 16;
        sA += 2;
        if ((sBlocks[i].nLen -= 2) == 0)
          return FALSE;
      }
      else
      {
        dwBase = 8;
        sA++;
        sBlocks[i].nLen--;
      }
    }
    for (j=0; j<sBlocks[i].nLen; j++,sA++)
    {
      if (*sA >= '0' && *sA <= '9')
        dw = (DWORD)(*sA - '0');
      else if (*sA >= 'A' && *sA <= 'F')
        dw = (DWORD)(*sA - 'A') + 10;
      else if (*sA >= 'a' && *sA <= 'f')
        dw = (DWORD)(*sA - 'a') + 10;
      else
        return FALSE;
      if (dw >= dwBase)
        return FALSE;
      dwOldValue = dwValue;
      dwValue = (dwValue * dwBase) + dw;
      if (dwOldValue < dwValue)
        return FALSE; //if overflow, not a valid ipv4 address
    }
    if (i < nBlocksCount-1)
    {
      if (dwValue > 255)
        return FALSE;
    }
    else
    {
      i = 3;
    }
    if (lpAddress != NULL)
    {
      LPBYTE d = (LPBYTE)&(((sockaddr_in*)lpAddress)->sin_addr.S_un.S_un_b.s_b1);
      for (j=0; dwValue>0; j++,dwValue/=0x100)
        d[i-j] = (BYTE)(dwValue & 0xFF);
    }
  }
  return TRUE;
}

BOOL CHostResolver::IsValidIPV4(__in_z LPCWSTR szAddressW, __in_opt SIZE_T nAddressLen, __out_opt sockaddr *lpAddress)
{
  SIZE_T i, j, nLen, nBlocksCount;
  DWORD dw, dwBase, dwValue, dwOldValue;
  LPCWSTR sW;
  struct {
    LPCWSTR szStartW;
    SIZE_T nLen;
  } sBlocks[4];
  sockaddr sTempAddr;

  if (lpAddress == NULL)
    lpAddress = &sTempAddr;
  MX::MemSet(lpAddress, 0, sizeof(sockaddr));
  lpAddress->sa_family = AF_INET;
  if (szAddressW == NULL)
    return FALSE;
  if (nAddressLen == (SIZE_T)-1)
    nAddressLen = wcslen(szAddressW);
  for (nBlocksCount=0; nBlocksCount<4; nBlocksCount++)
  {
    sBlocks[nBlocksCount].szStartW = szAddressW;
    for (nLen=0; nLen<nAddressLen && szAddressW[nLen]!=L'.' && szAddressW[nLen]!=L'/' && szAddressW[nLen]!=L'%';
         nLen++);
      sBlocks[nBlocksCount].nLen = nLen;
    if (nLen >= nAddressLen || szAddressW[nLen] != L'.')
    {
      nBlocksCount++;
      break;
    }
    if (nBlocksCount == 3)
      return FALSE;
    szAddressW += nLen+1;
    nAddressLen -= nLen+1;
  }
  if (nBlocksCount < 2)
    return FALSE;
  for (i=0; i<nBlocksCount; i++)
  {
    dwValue = 0;
    dwBase = 10;
    sW = sBlocks[i].szStartW;
    if (sBlocks[i].nLen == 0)
      return FALSE;
    if (sW[0] == L'0' && sBlocks[i].nLen >= 2)
    {
      if (sW[1] == L'X' || sW[1] == L'x')
      {
        dwBase = 16;
        sW += 2;
        if ((sBlocks[i].nLen -= 2) == 0)
          return FALSE;
      }
      else
      {
        dwBase = 8;
        sW++;
        sBlocks[i].nLen--;
      }
    }
    for (j=0; j<sBlocks[i].nLen; j++,sW++)
    {
      if (*sW >= L'0' && *sW <= L'9')
        dw = (DWORD)(*sW - L'0');
      else if (*sW >= L'A' && *sW <= L'F')
        dw = (DWORD)(*sW - L'A') + 10;
      else if (*sW >= L'a' && *sW <= L'f')
        dw = (DWORD)(*sW - L'a') + 10;
      else
        return FALSE;
      if (dw >= dwBase)
        return FALSE;
      dwOldValue = dwValue;
      dwValue = (dwValue * dwBase) + dw;
      if (dwOldValue < dwValue)
        return FALSE; //if overflow, not a valid ipv4 address
    }
    if (i < nBlocksCount-1)
    {
      if (dwValue > 255)
        return FALSE;
    }
    else
    {
      i = 3;
    }
    if (lpAddress != NULL)
    {
      LPBYTE d = (LPBYTE)&(((sockaddr_in*)lpAddress)->sin_addr.S_un.S_un_b.s_b1);
      for (j=0; dwValue>0; j++,dwValue/=0x100)
        d[i-j] = (BYTE)(dwValue & 0xFF);
    }
  }
  return TRUE;
}

BOOL CHostResolver::IsValidIPV6(__in_z LPCSTR szAddressA, __in_opt SIZE_T nAddressLen, __out_opt sockaddr *lpAddress)
{
  SIZE_T i, k, nSlots, nLastColonPos;
  BOOL bIPv4, bIPv6;
  LPCSTR sA;
  sockaddr sTempAddr;
  WORD *w;

  if (lpAddress == NULL)
    lpAddress = &sTempAddr;
  MX::MemSet(lpAddress, 0, sizeof(sockaddr));
  lpAddress->sa_family = AF_INET6;
  if (szAddressA == NULL)
    return FALSE;
  if (nAddressLen == (SIZE_T)-1)
    nAddressLen = StrLenA(szAddressA);
  w = ((SOCKADDR_IN6_W2KSP1*)lpAddress)->sin6_addr.u.Word;
  if (nAddressLen >= 2 && szAddressA[0] == '[' && szAddressA[nAddressLen-1] == ']')
  {
    szAddressA++;
    nAddressLen -= 2;
  }
  for (i=0; i<nAddressLen; i++)
  {
    if (szAddressA[i] == '/' || szAddressA[i] == '%')
    {
      nAddressLen = i;
      break;
    }
  }
  if (nAddressLen < 2)
    return FALSE;
  //IPv4 inside?
  for (nLastColonPos=nAddressLen; nLastColonPos>0; nLastColonPos--)
  {
    if (szAddressA[nLastColonPos-1] == ':')
      break;
  }
  if (nLastColonPos == 0)
    return FALSE;
  nLastColonPos--;
  bIPv4 = FALSE;
  nSlots = 0;
  if (nLastColonPos < nAddressLen-1)
  {
    sockaddr_in sTempAddr;

    if (IsValidIPV4(szAddressA+nLastColonPos+1, nAddressLen-nLastColonPos-1, (sockaddr*)&sTempAddr) != FALSE)
    {
      w[6] = sTempAddr.sin_addr.S_un.S_un_w.s_w1;
      w[7] = sTempAddr.sin_addr.S_un.S_un_w.s_w2;
      nAddressLen = nLastColonPos;
      if (nLastColonPos > 0 && szAddressA[nLastColonPos-1] == ':')
        nAddressLen++;
      bIPv4 = TRUE;
      nSlots = 2;
    }
  }
  // Only an bIPv6 block remains, either:
  // "hexnumbers::hexnumbers", "hexnumbers::" or "hexnumbers"
  sA = szAddressA;
  k = nAddressLen;
  while (k > 1)
  {
    if (sA[0] == ':' && sA[1] == ':')
      break;
    sA++;
    k--;
  }
  if (k > 1)
  {
    SIZE_T nPos, nLeftSlots, nRightSlots;

    nPos = (SIZE_T)(sA-szAddressA);
    nRightSlots = Helper_IPv6_Fill(w, szAddressA+nPos+2, nAddressLen-nPos-2);
    if (nRightSlots == (SIZE_T)-1 || nRightSlots+nSlots > 8)
      return FALSE;
    k = 8 - nSlots - nRightSlots;
    for (i=nRightSlots; i>0; i--)
    {
      w[i+k-1] = w[i-1];
      w[i-1] = 0;
    }
    nLeftSlots = Helper_IPv6_Fill(w, szAddressA, nPos);
    if (nLeftSlots == (SIZE_T)-1 || nLeftSlots+nRightSlots+nSlots > 7)
      return FALSE;
  }
  else
  {
    if (Helper_IPv6_Fill(w, szAddressA, nAddressLen) != 8-nSlots)
      return FALSE;
  }
  //Now check the results in the bIPv6-address range only
  bIPv6 = FALSE;
  for (i=0; i<nSlots; i++)
  {
    if (w[i] != 0 || (i == 5 && w[i] != 0xFFFF))
      bIPv6 = TRUE;
  }
  //check IPv4 validity
  if (bIPv4 != FALSE && bIPv6 == FALSE)
  {
    for (i=0; i<5; i++)
    {
      if (w[i] != 0)
        return FALSE;
    }
    if (w[5] != 0 && w[5] != 0xFFFF)
      return FALSE;
  }
  return TRUE;
}

BOOL CHostResolver::IsValidIPV6(__in_z LPCWSTR szAddressW, __in_opt SIZE_T nAddressLen, __out_opt sockaddr *lpAddress)
{
  SIZE_T i, k, nSlots, nLastColonPos;
  BOOL bIPv4, bIPv6;
  LPCWSTR sW;
  sockaddr sTempAddr;
  WORD *w;

  if (lpAddress == NULL)
    lpAddress = &sTempAddr;
  MX::MemSet(lpAddress, 0, sizeof(sockaddr));
  lpAddress->sa_family = AF_INET6;
  if (szAddressW == NULL)
    return FALSE;
  if (nAddressLen == (SIZE_T)-1)
    nAddressLen = StrLenW(szAddressW);
  w = ((SOCKADDR_IN6_W2KSP1*)lpAddress)->sin6_addr.u.Word;
  if (nAddressLen >= 2 && szAddressW[0] == L'[' && szAddressW[nAddressLen-1] == L']')
  {
    szAddressW++;
    nAddressLen -= 2;
  }
  for (i=0; i<nAddressLen; i++)
  {
    if (szAddressW[i] == L'/' || szAddressW[i] == L'%')
    {
      nAddressLen = i;
      break;
    }
  }
  if (nAddressLen < 2)
    return FALSE;
  //IPv4 inside?
  for (nLastColonPos=nAddressLen; nLastColonPos>0; nLastColonPos--)
  {
    if (szAddressW[nLastColonPos-1] == L':')
      break;
  }
  if (nLastColonPos == 0)
    return FALSE;
  nLastColonPos--;
  bIPv4 = FALSE;
  nSlots = 0;
  if (nLastColonPos < nAddressLen-1)
  {
    sockaddr_in sTempAddr;

    if (IsValidIPV4(szAddressW+nLastColonPos+1, nAddressLen-nLastColonPos-1, (sockaddr*)&sTempAddr) != FALSE)
    {
      w[6] = sTempAddr.sin_addr.S_un.S_un_w.s_w1;
      w[7] = sTempAddr.sin_addr.S_un.S_un_w.s_w2;
      nAddressLen = nLastColonPos;
      if (nLastColonPos > 0 && szAddressW[nLastColonPos-1] == L':')
        nAddressLen++;
      bIPv4 = TRUE;
      nSlots = 2;
    }
  }
  // Only an bIPv6 block remains, either:
  // "hexnumbers::hexnumbers", "hexnumbers::" or "hexnumbers"
  sW = szAddressW;
  k = nAddressLen;
  while (k > 1)
  {
    if (sW[0] == L':' && sW[1] == L':')
      break;
    sW++;
    k--;
  }
  if (k > 1)
  {
    SIZE_T nPos, nLeftSlots, nRightSlots;

    nPos = (SIZE_T)(sW-szAddressW);
    nRightSlots = Helper_IPv6_Fill(w, szAddressW+nPos+2, nAddressLen-nPos-2);
    if (nRightSlots == (SIZE_T)-1 || nRightSlots+nSlots > 8)
      return FALSE;
    k = 8 - nSlots - nRightSlots;
    for (i=nRightSlots; i>0; i--)
    {
      w[i+k-1] = w[i-1];
      w[i-1] = 0;
    }
    nLeftSlots = Helper_IPv6_Fill(w, szAddressW, nPos);
    if (nLeftSlots == (SIZE_T)-1 || nLeftSlots+nRightSlots+nSlots > 7)
      return FALSE;
  }
  else
  {
    if (Helper_IPv6_Fill(w, szAddressW, nAddressLen) != 8-nSlots)
      return FALSE;
  }
  //Now check the results in the bIPv6-address range only
  bIPv6 = FALSE;
  for (i=0; i<nSlots; i++)
  {
    if (w[i] != 0 || (i == 5 && w[i] != 0xFFFF))
      bIPv6 = TRUE;
  }
  //check IPv4 validity
  if (bIPv4 != FALSE && bIPv6 == FALSE)
  {
    for (i=0; i<5; i++)
    {
      if (w[i] != 0)
        return FALSE;
    }
    if (w[5] != 0 && w[5] != 0xFFFF)
      return FALSE;
  }
  return TRUE;
}

} //namespace MX

//-----------------------------------------------------------

static SIZE_T Helper_IPv6_Fill(__out LPWORD lpnAddr, __in_z LPCSTR szStrA, __in SIZE_T nLen)
{
  SIZE_T i, nSlot;
  DWORD dw, dwValue;

  if (nLen == 0)
    return 0;
  //catch double uses of ::
  if (MX::StrFindA(szStrA, "::") != NULL)
    return (SIZE_T)-1;
  nSlot = 0;
  dwValue = 0;
  for (i=0; i<nLen; i++)
  {
    if (szStrA[i] == ':')
    {
      //trailing : is not allowed
      if (i == nLen-1 || nSlot == 8)
        return (SIZE_T)-1;
      lpnAddr[nSlot++] = (WORD)dwValue;
      dwValue = 0;
    }
    else
    {
      if (szStrA[i] >= '0' && szStrA[i] <= '9')
        dw = (DWORD)(szStrA[i] - '0');
      else if (szStrA[i] >= 'a' && szStrA[i] <= 'f')
        dw = (DWORD)(szStrA[i] - 'a' + 10);
      else if (szStrA[i] >= 'A' && szStrA[i] <= 'F')
        dw = (DWORD)(szStrA[i] - 'A' + 10);
      else
        return (SIZE_T)-1;
      dwValue = (dwValue << 4) + dw;
      if (dwValue > 0xFFFF)
        return (SIZE_T)-1;
    }
  }
  if (nSlot == 8)
    return (SIZE_T)-1;
  lpnAddr[nSlot++] = (WORD)dwValue;
  return nSlot;
}

static SIZE_T Helper_IPv6_Fill(__out LPWORD lpnAddr, __in_z LPCWSTR szStrW, __in SIZE_T nLen)
{
  SIZE_T i, nSlot;
  DWORD dw, dwValue;

  if (nLen == 0)
    return 0;
  //catch double uses of ::
  if (MX::StrFindW(szStrW, L"::") != NULL)
    return (SIZE_T)-1;
  nSlot = 0;
  dwValue = 0;
  for (i=0; i<nLen; i++)
  {
    if (szStrW[i] == L':')
    {
      //trailing : is not allowed
      if (i == nLen-1 || nSlot == 8)
        return (SIZE_T)-1;
      lpnAddr[nSlot++] = (WORD)dwValue;
      dwValue = 0;
    }
    else
    {
      if (szStrW[i] >= L'0' && szStrW[i] <= L'9')
        dw = (DWORD)(szStrW[i] - L'0');
      else if (szStrW[i] >= L'a' && szStrW[i] <= L'f')
        dw = (DWORD)(szStrW[i] - L'a' + 10);
      else if (szStrW[i] >= L'A' && szStrW[i] <= L'F')
        dw = (DWORD)(szStrW[i] - L'A' + 10);
      else
        return (SIZE_T)-1;
      dwValue = (dwValue << 4) + dw;
      if (dwValue > 0xFFFF)
        return (SIZE_T)-1;
    }
  }
  if (nSlot == 8)
    return (SIZE_T)-1;
  lpnAddr[nSlot++] = (WORD)dwValue;
  return nSlot;
}
