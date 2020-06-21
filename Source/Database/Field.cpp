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
#include "..\..\include\Database\BaseConnector.h"
#include "..\..\Include\Strings\Utf8.h"

//-----------------------------------------------------------

namespace MX {

namespace Database {

CField::CField() : TRefCounted<CBaseMemObj>(), CNonCopyableObj()
{
  nFieldType = FieldTypeNull;
  szStrA = NULL;
  nLength = 0;
  lpDt = NULL;
  lpBuffer = NULL;
  nBufferSize = 0;
  return;
}

CField::~CField()
{
  MX_DELETE(lpDt);
  MX_FREE(lpBuffer);
  return;
}

VOID CField::SetNull()
{
  nFieldType = FieldTypeNull;
  szStrA = NULL;
  nLength = 0;
  return;
}

HRESULT CField::SetString(_In_ LPCSTR _szStrA, _In_ SIZE_T _nLength)
{
  if (_nLength == (SIZE_T)-1)
    _nLength = StrLenA(_szStrA);

  if (EnsureSize(_nLength + 1) == FALSE)
    return E_OUTOFMEMORY;
  ::MxMemCopy(lpBuffer, _szStrA, _nLength);
  lpBuffer[_nLength] = 0;

  nFieldType = FieldTypeString;
  szStrA = (LPCSTR)lpBuffer;
  nLength = _nLength;

  //done
  return S_OK;
}

HRESULT CField::SetString(_In_ LPCWSTR szStrW, _In_ SIZE_T nLength)
{
  CStringA cStrTempA;
  HRESULT hRes;

  hRes = Utf8_Encode(cStrTempA, szStrW, nLength);
  if (FAILED(hRes))
    return hRes;

  if (EnsureSize(cStrTempA.GetLength() + 1) == FALSE)
    return E_OUTOFMEMORY;
  ::MxMemCopy(lpBuffer, (LPCSTR)cStrTempA, cStrTempA.GetLength() + 1);

  nFieldType = FieldTypeString;
  szStrA = (LPCSTR)lpBuffer;
  nLength = cStrTempA.GetLength();

  //done
  return S_OK;
}

VOID CField::SetBoolean(_In_ BOOL bValue)
{
  nFieldType = FieldTypeBoolean;
  b = bValue;

  //done
  return;
}

VOID CField::SetUInt32(_In_ ULONG nValue)
{
  nFieldType = FieldTypeUInt32;
  ul = nValue;

  //done
  return;
}

VOID CField::SetInt32(_In_ LONG nValue)
{
  nFieldType = FieldTypeInt32;
  l = nValue;

  //done
  return;
}

VOID CField::SetUInt64(_In_ ULONGLONG nValue)
{
  nFieldType = FieldTypeUInt64;
  ull = nValue;

  //done
  return;
}

VOID CField::SetInt64(_In_ LONGLONG nValue)
{
  nFieldType = FieldTypeInt64;
  ll = nValue;

  //done
  return;
}

VOID CField::SetDouble(_In_ double nValue)
{
  nFieldType = FieldTypeDouble;
  dbl = nValue;

  //done
  return;
}

HRESULT CField::SetDateTime(_In_ CDateTime &cDt)
{
  if (lpDt == NULL)
  {
    lpDt = MX_DEBUG_NEW CDateTime();
    if (lpDt == NULL)
      return E_OUTOFMEMORY;
  }

  nFieldType = FieldTypeDateTime;
  *lpDt = cDt;

  //done
  return S_OK;
}

HRESULT CField::SetBlob(_In_ LPVOID lpData, _In_ SIZE_T _nLength)
{
  if (_nLength > 0)
  {
    if (EnsureSize(_nLength + 1) == FALSE)
      return E_OUTOFMEMORY;
    ::MxMemCopy(lpBuffer, lpData, _nLength);
  }

  nFieldType = FieldTypeBlob;
  lpBlob = (_nLength > 0) ? lpBuffer : NULL;
  nLength = _nLength;

  //done
  return S_OK;
}

eFieldType CField::GetType() const
{
  return nFieldType;
}

SIZE_T CField::GetLength() const
{
  return nLength;
}

BOOL CField::GetBoolean() const
{
  return (nFieldType == FieldTypeBoolean) ? b : NULL;
}

LPCSTR CField::GetString() const
{
  return (nFieldType == FieldTypeString) ? szStrA : NULL;
}

HRESULT CField::GetString(_Out_ CStringW &cStrW)
{
  if (nFieldType != FieldTypeString)
    return E_FAIL;
  return Utf8_Decode(cStrW, szStrA, nLength);
}

ULONG CField::GetUInt32() const
{
  return (nFieldType == FieldTypeUInt32) ? ul : 0;
}

LONG CField::GetInt32() const
{
  return (nFieldType == FieldTypeInt32) ? l : 0;
}

ULONGLONG CField::GetUInt64() const
{
  return (nFieldType == FieldTypeUInt64) ? ull : 0ui64;
}

LONGLONG CField::GetInt64() const
{
  return (nFieldType == FieldTypeInt64) ? ll : 0i64;
}

double CField::GetDouble() const
{
  return (nFieldType == FieldTypeDouble) ? dbl : 0.0;
}

CDateTime* CField::GetDateTime() const
{
  return (nFieldType == FieldTypeDateTime) ? lpDt : NULL;
}

LPBYTE CField::GetBlob() const
{
  return (nFieldType == FieldTypeBlob) ? (LPBYTE)lpBlob : NULL;
}

BOOL CField::EnsureSize(_In_ SIZE_T nNewSize)
{
  if (nNewSize > nBufferSize)
  {
    LPBYTE lpNewBuffer;

    lpNewBuffer = (LPBYTE)MX_MALLOC(nNewSize);
    if (lpNewBuffer == NULL)
      return FALSE;
    MX_FREE(lpBuffer);
    lpBuffer = lpNewBuffer;
    nBufferSize = nNewSize;
  }
  return TRUE;
}

} //namespace Database

} //namespace MX
