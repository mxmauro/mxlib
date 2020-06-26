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
#include <float.h>

#define __DBL_EPSILON  0.00001

//-----------------------------------------------------------

template<typename T>
static bool isNan(_In_ T value)
{
  return false;
}

template<>
inline bool isNan(_In_ double value)
{
  return _isnan(value);
}

template<typename T>
static T getSign(_In_ T value)
{
  if (value == T(0) || isNan<T>(value))
    return value;
  return value > T(0) ? 1 : -1;
}

template <typename fromType, typename toType>
static bool convertNumber(_In_ fromType n, _Out_ toType *lpRes)
{
  toType convertedN = static_cast<toType>(n);

  if (convertedN == n && getSign<fromType>(n) == getSign<toType>(convertedN))
  {
    *lpRes = convertedN;
    return TRUE;
  }

  *lpRes = toType(0);
  return FALSE;
}

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

HRESULT CField::SetFormattedString(_In_ LPCSTR szFormatA, ...)
{
  va_list argptr;
  HRESULT hRes;

  va_start(argptr, szFormatA);
  hRes = SetFormattedStringV(szFormatA, argptr);
  va_end(argptr);
  return hRes;
}

HRESULT CField::SetFormattedString(_In_ LPCWSTR szFormatW, ...)
{
  va_list argptr;
  HRESULT hRes;

  va_start(argptr, szFormatW);
  hRes = SetFormattedStringV(szFormatW, argptr);
  va_end(argptr);
  return hRes;
}

HRESULT CField::SetFormattedStringV(_In_ LPCSTR szFormatA, _In_ va_list argptr)
{
  CStringA cStrTempA;

  if (cStrTempA.FormatV(szFormatA, argptr) == FALSE)
    return E_OUTOFMEMORY;
  return SetString((LPCSTR)cStrTempA, cStrTempA.GetLength());
}

HRESULT CField::SetFormattedStringV(_In_ LPCWSTR szFormatW, _In_ va_list argptr)
{
  CStringW cStrTempW;

  if (cStrTempW.FormatV(szFormatW, argptr) == FALSE)
    return E_OUTOFMEMORY;
  return SetString((LPCWSTR)cStrTempW, cStrTempW.GetLength());
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

BOOL CField::GetAsBoolean(_Out_ PBOOL lpbValue)
{
  switch (nFieldType)
  {
    case FieldTypeBoolean:
      *lpbValue = b;
      return TRUE;

    case FieldTypeUInt32:
      *lpbValue = (ul != 0) ? TRUE : FALSE;
      return TRUE;

    case FieldTypeInt32:
      *lpbValue = (l != 0) ? TRUE : FALSE;
      return TRUE;

    case FieldTypeUInt64:
      *lpbValue = (ull != 0) ? TRUE : FALSE;
      return TRUE;

    case FieldTypeInt64:
      *lpbValue = (ll != 0) ? TRUE : FALSE;
      return TRUE;

    case FieldTypeDouble:
      *lpbValue = (dbl < -__DBL_EPSILON || dbl > __DBL_EPSILON) ? TRUE : FALSE;
      return TRUE;
  }
  *lpbValue = FALSE;
  return FALSE;
}

BOOL CField::GetAsUInt32(_Out_ PULONG lpnValue)
{
  switch (nFieldType)
  {
    case FieldTypeBoolean:
      *lpnValue = (b != FALSE) ? 1 : 0;
      return TRUE;

    case FieldTypeUInt32:
      *lpnValue = ul;
      return TRUE;

    case FieldTypeInt32:
      if (l >= 0)
      {
        *lpnValue = (ULONG)l;
        return TRUE;
      }
      break;

    case FieldTypeUInt64:
      if (ull <= 0xFFFFFFFFui64)
      {
        *lpnValue = (ULONG)ull;
        return TRUE;
      }
      break;

    case FieldTypeInt64:
      if (ll >= 0i64 && ll <= 0xFFFFFFFFi64)
      {
        *lpnValue = (ULONG)(ULONGLONG)ll;
        return TRUE;
      }
      break;

    case FieldTypeDouble:
      if (convertNumber<double, ULONG>(dbl, lpnValue))
        return TRUE;
      break;
  }
  *lpnValue = 0;
  return FALSE;
}

BOOL CField::GetAsInt32(_Out_ PLONG lpnValue) const
{
  switch (nFieldType)
  {
    case FieldTypeBoolean:
      *lpnValue = (b != FALSE) ? 1 : 0;
      return TRUE;

    case FieldTypeUInt32:
      if (ul <= 0x7FFFFFFF)
      {
        *lpnValue = (LONG)ul;
        return TRUE;
      }
      break;

    case FieldTypeInt32:
      *lpnValue = l;
      return TRUE;

    case FieldTypeUInt64:
      if (ull <= 0x7FFFFFFFui64)
      {
        *lpnValue = (LONG)(ULONG)ull;
        return TRUE;
      }
      break;

    case FieldTypeInt64:
      if (ll >= -2147483648i64 && ll <= 2147483647i64)
      {
        *lpnValue = (LONG)ll;
        return TRUE;
      }
      break;

    case FieldTypeDouble:
      if (convertNumber<double, LONG>(dbl, lpnValue))
        return TRUE;
      break;
  }
  *lpnValue = 0;
  return FALSE;
}

BOOL CField::GetAsUInt64(_Out_ PULONGLONG lpnValue) const
{
  switch (nFieldType)
  {
    case FieldTypeBoolean:
      *lpnValue = (b != FALSE) ? 1ui64 : 0ui64;
      return TRUE;

    case FieldTypeUInt32:
      *lpnValue = (ULONGLONG)ul;
      return TRUE;

    case FieldTypeInt32:
      if (l >= 0)
      {
        *lpnValue = (ULONGLONG)(ULONG)l;
        return TRUE;
      }
      break;

    case FieldTypeUInt64:
      *lpnValue = (ULONG)ull;
      return TRUE;

    case FieldTypeInt64:
      if (ll >= 0i64)
      {
        *lpnValue = (ULONGLONG)ll;
        return TRUE;
      }
      break;

    case FieldTypeDouble:
      if (convertNumber<double, ULONGLONG>(dbl, lpnValue))
        return TRUE;
      break;
  }
  *lpnValue = 0;
  return FALSE;
}

BOOL CField::GetAsInt64(_Out_ PLONGLONG lpnValue) const
{
  switch (nFieldType)
  {
    case FieldTypeBoolean:
      *lpnValue = (b != FALSE) ? 1 : 0;
      return TRUE;

    case FieldTypeUInt32:
      *lpnValue = (LONGLONG)(ULONGLONG)ul;
      return TRUE;

    case FieldTypeInt32:
      *lpnValue = (LONGLONG)l;
      return TRUE;

    case FieldTypeUInt64:
      if (ull <= 0x7FFFFFFFFFFFFFFFui64)
      {
        *lpnValue = (LONGLONG)ull;
        return TRUE;
      }
      break;

    case FieldTypeInt64:
      *lpnValue = (LONGLONG)ll;
      return TRUE;

    case FieldTypeDouble:
      if (convertNumber<double, LONGLONG>(dbl, lpnValue))
        return TRUE;
      break;
  }
  *lpnValue = 0;
  return FALSE;
}

BOOL CField::GetAsDouble(_Out_ double *lpnValue) const
{
  switch (nFieldType)
  {
    case FieldTypeBoolean:
      *lpnValue = (b != FALSE) ? 1 : 0;
      return TRUE;

    case FieldTypeUInt32:
      if (convertNumber<ULONG, double>(ul, lpnValue))
        return TRUE;
      break;

    case FieldTypeInt32:
      if (convertNumber<LONG, double>(l, lpnValue))
        return TRUE;
      break;

    case FieldTypeUInt64:
      if (convertNumber<ULONGLONG, double>(ull, lpnValue))
        return TRUE;
      break;

    case FieldTypeInt64:
      if (convertNumber<LONGLONG, double>(ll, lpnValue))
        return TRUE;
      break;

    case FieldTypeDouble:
      *lpnValue = dbl;
      return TRUE;
  }
  *lpnValue = 0;
  return FALSE;
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

//-----------------------------------------------------------

CFieldList::CFieldList() : TArrayListWithRelease<CField *>()
{
  return;
}

HRESULT CFieldList::AddNull()
{
  CField *lpNewField;

  //create new field
  lpNewField = MX_DEBUG_NEW CField();
  if (lpNewField == NULL)
    return E_OUTOFMEMORY;

  //set value
  lpNewField->SetNull();

  //add to list
  if (AddElement(lpNewField) == FALSE)
  {
    lpNewField->Release();
    return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

HRESULT CFieldList::AddString(_In_ LPCSTR szStrA, _In_ SIZE_T nLength)
{
  CField *lpNewField;
  HRESULT hRes;

  //create new field
  lpNewField = MX_DEBUG_NEW CField();
  if (lpNewField == NULL)
    return E_OUTOFMEMORY;

  //set value
  hRes = lpNewField->SetString(szStrA, nLength);
  if (FAILED(hRes))
  {
    lpNewField->Release();
    return hRes;
  }

  //add to list
  if (AddElement(lpNewField) == FALSE)
  {
    lpNewField->Release();
    return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

HRESULT CFieldList::AddString(_In_ LPCWSTR szStrW, _In_ SIZE_T nLength)
{
  CField *lpNewField;
  HRESULT hRes;

  //create new field
  lpNewField = MX_DEBUG_NEW CField();
  if (lpNewField == NULL)
    return E_OUTOFMEMORY;

  //set value
  hRes = lpNewField->SetString(szStrW, nLength);
  if (FAILED(hRes))
  {
    lpNewField->Release();
    return hRes;
  }

  //add to list
  if (AddElement(lpNewField) == FALSE)
  {
    lpNewField->Release();
    return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

HRESULT CFieldList::AddFormattedString(_In_ LPCSTR szFormatA, ...)
{
  va_list argptr;
  HRESULT hRes;

  va_start(argptr, szFormatA);
  hRes = AddFormattedStringV(szFormatA, argptr);
  va_end(argptr);
  return hRes;
}

HRESULT CFieldList::AddFormattedString(_In_ LPCWSTR szFormatW, ...)
{
  va_list argptr;
  HRESULT hRes;

  va_start(argptr, szFormatW);
  hRes = AddFormattedStringV(szFormatW, argptr);
  va_end(argptr);
  return hRes;
}

HRESULT CFieldList::AddFormattedStringV(_In_ LPCSTR szFormatA, _In_ va_list argptr)
{
  CStringA cStrTempA;
  CField *lpNewField;
  HRESULT hRes;

  if (cStrTempA.FormatV(szFormatA, argptr) == FALSE)
    return E_OUTOFMEMORY;

  //create new field
  lpNewField = MX_DEBUG_NEW CField();
  if (lpNewField == NULL)
    return E_OUTOFMEMORY;

  //set value
  hRes = lpNewField->SetString((LPCSTR)cStrTempA, cStrTempA.GetLength());
  if (FAILED(hRes))
  {
    lpNewField->Release();
    return hRes;
  }

  //add to list
  if (AddElement(lpNewField) == FALSE)
  {
    lpNewField->Release();
    return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

HRESULT CFieldList::AddFormattedStringV(_In_ LPCWSTR szFormatW, _In_ va_list argptr)
{
  CStringW cStrTempW;
  CField *lpNewField;
  HRESULT hRes;

  if (cStrTempW.FormatV(szFormatW, argptr) == FALSE)
    return E_OUTOFMEMORY;

  //create new field
  lpNewField = MX_DEBUG_NEW CField();
  if (lpNewField == NULL)
    return E_OUTOFMEMORY;

  //set value
  hRes = lpNewField->SetString((LPCWSTR)cStrTempW, cStrTempW.GetLength());
  if (FAILED(hRes))
  {
    lpNewField->Release();
    return hRes;
  }

  //add to list
  if (AddElement(lpNewField) == FALSE)
  {
    lpNewField->Release();
    return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

HRESULT CFieldList::AddBoolean(_In_ BOOL bValue)
{
  CField *lpNewField;

  //create new field
  lpNewField = MX_DEBUG_NEW CField();
  if (lpNewField == NULL)
    return E_OUTOFMEMORY;

  //set value
  lpNewField->SetBoolean(bValue);

  //add to list
  if (AddElement(lpNewField) == FALSE)
  {
    lpNewField->Release();
    return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

HRESULT CFieldList::AddUInt32(_In_ ULONG nValue)
{
  CField *lpNewField;

  //create new field
  lpNewField = MX_DEBUG_NEW CField();
  if (lpNewField == NULL)
    return E_OUTOFMEMORY;

  //set value
  lpNewField->SetUInt32(nValue);

  //add to list
  if (AddElement(lpNewField) == FALSE)
  {
    lpNewField->Release();
    return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

HRESULT CFieldList::AddInt32(_In_ LONG nValue)
{
  CField *lpNewField;

  //create new field
  lpNewField = MX_DEBUG_NEW CField();
  if (lpNewField == NULL)
    return E_OUTOFMEMORY;

  //set value
  lpNewField->SetInt32(nValue);

  //add to list
  if (AddElement(lpNewField) == FALSE)
  {
    lpNewField->Release();
    return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

HRESULT CFieldList::AddUInt64(_In_ ULONGLONG nValue)
{
  CField *lpNewField;

  //create new field
  lpNewField = MX_DEBUG_NEW CField();
  if (lpNewField == NULL)
    return E_OUTOFMEMORY;

  //set value
  lpNewField->SetUInt64(nValue);

  //add to list
  if (AddElement(lpNewField) == FALSE)
  {
    lpNewField->Release();
    return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

HRESULT CFieldList::AddInt64(_In_ LONGLONG nValue)
{
  CField *lpNewField;

  //create new field
  lpNewField = MX_DEBUG_NEW CField();
  if (lpNewField == NULL)
    return E_OUTOFMEMORY;

  //set value
  lpNewField->SetInt64(nValue);

  //add to list
  if (AddElement(lpNewField) == FALSE)
  {
    lpNewField->Release();
    return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

HRESULT CFieldList::AddDouble(_In_ double nValue)
{
  CField *lpNewField;

  //create new field
  lpNewField = MX_DEBUG_NEW CField();
  if (lpNewField == NULL)
    return E_OUTOFMEMORY;

  //set value
  lpNewField->SetDouble(nValue);

  //add to list
  if (AddElement(lpNewField) == FALSE)
  {
    lpNewField->Release();
    return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

HRESULT CFieldList::AddDateTime(_In_ CDateTime &cDt)
{
  CField *lpNewField;
  HRESULT hRes;

  //create new field
  lpNewField = MX_DEBUG_NEW CField();
  if (lpNewField == NULL)
    return E_OUTOFMEMORY;

  //set value
  hRes = lpNewField->SetDateTime(cDt);
  if (FAILED(hRes))
  {
    lpNewField->Release();
    return hRes;
  }

  //add to list
  if (AddElement(lpNewField) == FALSE)
  {
    lpNewField->Release();
    return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

HRESULT CFieldList::AddBlob(_In_ LPVOID lpData, _In_ SIZE_T nLength)
{
  CField *lpNewField;
  HRESULT hRes;

  //create new field
  lpNewField = MX_DEBUG_NEW CField();
  if (lpNewField == NULL)
    return E_OUTOFMEMORY;

  //set value
  hRes = lpNewField->SetBlob(lpData, nLength);
  if (FAILED(hRes))
  {
    lpNewField->Release();
    return hRes;
  }

  //add to list
  if (AddElement(lpNewField) == FALSE)
  {
    lpNewField->Release();
    return E_OUTOFMEMORY;
  }

  //done
  return S_OK;
}

} //namespace Database

} //namespace MX
