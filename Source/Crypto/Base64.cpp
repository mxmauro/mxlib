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
#include "..\..\Include\Crypto\Base64.h"

#define __SIZE_T_MAX      ((SIZE_T)-1)

//-----------------------------------------------------------

static const LPCSTR szBase64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "abcdefghijklmnopqrstuvwxyz"
                                    "0123456789+/";

static const BYTE aDecodeTable[] = "|$$$}rstuvwxyz{$$$=$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

//-----------------------------------------------------------

namespace MX {

CBase64Encoder::CBase64Encoder() : MX::CBaseMemObj()
{
  szBufferA = NULL;
  nLength = nSize = 0;
  aInput[0] = aInput[1] = aInput[2] = 0;
  nInputLength = 0;
  return;
}

CBase64Encoder::~CBase64Encoder()
{
  MX_FREE(szBufferA);
  return;
}

HRESULT CBase64Encoder::Begin(_In_opt_ SIZE_T nPreallocateOutputLen)
{
  aInput[0] = aInput[1] = aInput[2] = 0;
  nLength = nInputLength = 0;
  MX_FREE(szBufferA);
  if (nPreallocateOutputLen == 0)
    nPreallocateOutputLen = 1024;
  else if (nPreallocateOutputLen < __SIZE_T_MAX - 1024)
    nPreallocateOutputLen = (nPreallocateOutputLen + 1023) & (~1023);
  else
    nPreallocateOutputLen = __SIZE_T_MAX;
  szBufferA = (LPSTR)MX_MALLOC(nSize = nPreallocateOutputLen);
  if (szBufferA == NULL)
  {
    nSize = 0;
    return E_OUTOFMEMORY;
  }
  szBufferA[0] = 0;
  return S_OK;
}

HRESULT CBase64Encoder::Process(_In_ LPVOID lpData, _In_ SIZE_T nDataLen)
{
  CHAR szDestA[4];

  if (lpData == NULL && nDataLen > 0)
    return E_POINTER;
  while (nDataLen > 0)
  {
    aInput[nInputLength++] = *((LPBYTE)lpData);
    if (nInputLength == 3)
    {
      szDestA[0] = szBase64Chars[(aInput[0] & 0xFC) >> 2];
      szDestA[1] = szBase64Chars[((aInput[0] & 0x03) << 4) + ((aInput[1] & 0xF0) >> 4)];
      szDestA[2] = szBase64Chars[((aInput[1] & 0x0F) << 2) + ((aInput[2] & 0xC0) >> 6)];
      szDestA[3] = szBase64Chars[aInput[2] & 0x3F];
      if (AddToBuffer(szDestA) == FALSE)
        return E_OUTOFMEMORY;
      nInputLength = 0;
    }
    lpData = (LPBYTE)lpData + 1;
    nDataLen--;
  }
  return S_OK;
}

HRESULT CBase64Encoder::End()
{
  CHAR szDestA[4];

  if (nInputLength > 0)
  {
    for (SIZE_T i=nInputLength; i < 3; i++)
      aInput[i] = 0;

    szDestA[0] = szBase64Chars[(aInput[0] & 0xFC) >> 2];
    szDestA[1] = szBase64Chars[((aInput[0] & 0x03) << 4) + ((aInput[1] & 0xF0) >> 4)];
    szDestA[2] = szBase64Chars[((aInput[1] & 0x0F) << 2) + ((aInput[2] & 0xC0) >> 6)];
    szDestA[3] = szBase64Chars[aInput[2] & 0x3F];
    while (nInputLength < 3)
      szDestA[++nInputLength] = '=';
    if (AddToBuffer(szDestA) == FALSE)
      return E_OUTOFMEMORY;
  }
  return S_OK;
}

LPCSTR CBase64Encoder::GetBuffer() const
{
  return (szBufferA != NULL) ? szBufferA : "";
}

SIZE_T CBase64Encoder::GetOutputLength() const
{
  return nLength;
}

VOID CBase64Encoder::ConsumeOutput(_In_ SIZE_T nChars)
{
  if (szBufferA != NULL)
  {
    if (nChars > nLength)
      nChars = nLength;
    nLength -= nChars;
    MX::MemMove(szBufferA, szBufferA + nChars, nLength);
    szBufferA[nLength] = 0;
  }
  return;
}

SIZE_T CBase64Encoder::GetRequiredSpace(_In_ SIZE_T nDataLen)
{
  return ((nDataLen + 2) / 3) << 2;
}

BOOL CBase64Encoder::AddToBuffer(_In_ CHAR szDataA[4])
{
  if (nLength > nSize - 5)
  {
    LPSTR szNewBufferA;
    SIZE_T nNewSize;

    if (nSize == __SIZE_T_MAX)
      return FALSE;
    nNewSize = (nSize > __SIZE_T_MAX - 1024) ? __SIZE_T_MAX : (nSize + 1024);
    szNewBufferA = (LPSTR)MX_MALLOC(nNewSize);
    if (szNewBufferA == NULL)
      return FALSE;
    MX::MemCopy(szNewBufferA, szBufferA, nLength);
    MX_FREE(szBufferA);
    szBufferA = szNewBufferA;
    nSize = nNewSize;
  }
  szBufferA[nLength    ] = szDataA[0];
  szBufferA[nLength + 1] = szDataA[1];
  szBufferA[nLength + 2] = szDataA[2];
  szBufferA[nLength + 3] = szDataA[3];
  szBufferA[nLength + 4] = 0;
  nLength += 4;
  return TRUE;
}

//-----------------------------------------------------------

CBase64Decoder::CBase64Decoder() : MX::CBaseMemObj()
{
  lpBuffer = NULL;
  nLength = nSize = 0;
  aInput[0] = aInput[1] = aInput[2] = aInput[3] = 0;
  nInputLength = nEqualCounter = 0;
  return;
}

CBase64Decoder::~CBase64Decoder()
{
  MX_FREE(lpBuffer);
  return;
}

HRESULT CBase64Decoder::Begin(_In_opt_ SIZE_T nPreallocateOutputLen)
{
  aInput[0] = aInput[1] = aInput[2] = aInput[3] = 0;
  nLength = nInputLength = nEqualCounter = 0;
  MX_FREE(lpBuffer);
  if (nPreallocateOutputLen == 0)
    nPreallocateOutputLen = 1024;
  else if (nPreallocateOutputLen < __SIZE_T_MAX - 1024)
    nPreallocateOutputLen = (nPreallocateOutputLen + 1023) & (~1023);
  else
    nPreallocateOutputLen = __SIZE_T_MAX;
  lpBuffer = (LPBYTE)MX_MALLOC(nSize = nPreallocateOutputLen);
  if (lpBuffer == NULL)
  {
    nSize = 0;
    return E_OUTOFMEMORY;
  }
  return S_OK;
}

HRESULT CBase64Decoder::Process(_In_ LPCSTR szDataA, _In_opt_ SIZE_T nDataLen)
{
  BYTE aDest[3];
  BYTE val;

  if (nDataLen == (SIZE_T)-1)
    nDataLen = StrLenA(szDataA);
  if (szDataA == NULL && nDataLen > 0)
    return E_POINTER;
  while (nDataLen > 0)
  {
    val = (*((LPBYTE)szDataA) < 43 || *((LPBYTE)szDataA) > 122) ? '$' : aDecodeTable[*((LPBYTE)szDataA) - 43];
    if (val == (BYTE)'=')
    {
      if ((++nEqualCounter) > 2)
        return MX_E_InvalidData;
    }
    else if (val != (BYTE)'$')
    {
      //reset 'equal signs' that where in the middle
      nEqualCounter = 0;
      aInput[nInputLength++] = val - 62;
      if (nInputLength >= 4)
      {
        aDest[0] =  ((aInput[0] << 2)         | (aInput[1] >> 4));
        aDest[1] =  ((aInput[1] << 4)         | (aInput[2] >> 2));
        aDest[2] = (((aInput[2] << 6) & 0xC0) |  aInput[3]);
        if (AddToBuffer(aDest, 3) == FALSE)
          return E_OUTOFMEMORY;
        nInputLength = 0;
      }
    }
    szDataA++;
    nDataLen--;
  }
  return S_OK;
}

HRESULT CBase64Decoder::End()
{
  BYTE aDest[3];

  if (nInputLength > 1) //if only one remaining char, just ignore
  {
    for (SIZE_T i=nInputLength; i < 4; i++)
      aInput[i] = 0;
    aDest[0] =  ((aInput[0] << 2)         | (aInput[1] >> 4));
    aDest[1] =  ((aInput[1] << 4)         | (aInput[2] >> 2));
    aDest[2] = (((aInput[2] << 6) & 0xC0) |  aInput[3]);
    if (AddToBuffer(aDest, nInputLength-1) == FALSE)
      return E_OUTOFMEMORY;
  }
  return S_OK;
}

LPBYTE CBase64Decoder::GetBuffer() const
{
  return lpBuffer;
}

SIZE_T CBase64Decoder::GetOutputLength() const
{
  return nLength;
}

VOID CBase64Decoder::ConsumeOutput(_In_ SIZE_T nBytes)
{
  if (lpBuffer != NULL)
  {
    if (nBytes > nLength)
      nBytes = nLength;
    nLength -= nBytes;
    MX::MemMove(lpBuffer, lpBuffer + nBytes, nLength);
  }
  return;
}

SIZE_T CBase64Decoder::GetRequiredSpace(_In_ SIZE_T nDataLen)
{
  return ((nDataLen + 3) >> 2) * 3;
}

BOOL CBase64Decoder::AddToBuffer(_In_ LPBYTE aData, _In_ SIZE_T nLen)
{
  SIZE_T i;

  if (nLength + nLen < nLength)
    return FALSE;
  if (nLength > nSize - nLen)
  {
    LPBYTE lpNewBuffer;
    SIZE_T nNewSize;

    if (nSize == __SIZE_T_MAX)
      return FALSE;
    nNewSize = (nSize > __SIZE_T_MAX - 1024) ? __SIZE_T_MAX : (nSize + 1024);
    lpNewBuffer = (LPBYTE)MX_MALLOC(nNewSize);
    if (lpNewBuffer == NULL)
      return FALSE;
    MX::MemCopy(lpNewBuffer, lpBuffer, nLength);
    MX_FREE(lpBuffer);
    lpBuffer = lpNewBuffer;
    nSize = nNewSize;
  }
  for (i=0; i<nLen; i++)
    lpBuffer[nLength++] = aData[i];
  return TRUE;
}

} //namespace MX
