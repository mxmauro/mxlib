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
#include "..\Include\Loggable.h"
#include <stdio.h>

#ifdef _DEBUG
  //#define DEBUGOUTPUT_LOG
#else //_DEBUG
  //#define DEBUGOUTPUT_LOG
#endif //_DEBUG

#ifdef DEBUGOUTPUT_LOG
  #include "..\Include\Debug.h"
#endif //DEBUGOUTPUT_LOG

//-----------------------------------------------------------

namespace MX {

CLoggable::CLoggable() : CBaseMemObj()
{
  lpParentLog = NULL;
  dwLevel = 0;
  cCallback = NullCallback();
  return;
}

CLoggable::CLoggable(_In_ OnLogCallback _cCallback) : CBaseMemObj()
{
  lpParentLog = NULL;
  dwLevel = 0;
  cCallback = _cCallback;
  return;
}

CLoggable& CLoggable::operator=(_In_ const CLoggable &cSrc)
{
  lpParentLog = cSrc.lpParentLog;
  dwLevel = cSrc.dwLevel;
  cCallback = cSrc.cCallback;
  return *this;
}

VOID CLoggable::SetLogParent(_In_opt_ CLoggable *_lpParentLog)
{
  lpParentLog = _lpParentLog;
  return;
}

VOID CLoggable::SetLogLevel(_In_ DWORD _dwLevel)
{
  dwLevel = _dwLevel;
  return;
}

VOID CLoggable::SetLogCallback(_In_ OnLogCallback _cCallback)
{
  cCallback = _cCallback;
  return;
}

HRESULT CLoggable::Log(_Printf_format_string_ LPCWSTR szFormatW, ...)
{
  va_list argptr;
  HRESULT hRes;

  if (szFormatW == NULL)
    return E_POINTER;
  //write log
  va_start(argptr, szFormatW);
  hRes = GetRoot()->WriteLogCommon(FALSE, S_OK, szFormatW, argptr);
  va_end(argptr);
  //done
  return hRes;
}

HRESULT CLoggable::LogIfError(_In_ HRESULT hResError, _Printf_format_string_ LPCWSTR szFormatW, ...)
{
  va_list argptr;
  HRESULT hRes;

  if (SUCCEEDED(hResError))
    return S_OK;
  if (szFormatW == NULL)
    return E_POINTER;
  //write log
  va_start(argptr, szFormatW);
  hRes = GetRoot()->WriteLogCommon(TRUE, hResError, szFormatW, argptr);
  va_end(argptr);
  //done
  return hRes;
}

HRESULT CLoggable::LogAlways(_In_ HRESULT hResError, _Printf_format_string_ LPCWSTR szFormatW, ...)
{
  va_list argptr;
  HRESULT hRes;

  if (szFormatW == NULL)
    return E_POINTER;
  //write log
  va_start(argptr, szFormatW);
  hRes = GetRoot()->WriteLogCommon(TRUE, hResError, szFormatW, argptr);
  va_end(argptr);
  //done
  return hRes;
}

CLoggable* CLoggable::GetRoot() const
{
  CLoggable *lpThis;

  if (lpParentLog == NULL)
    return const_cast<CLoggable*>(this);
  lpThis = lpParentLog;
  while (lpThis->lpParentLog != NULL)
    lpThis = lpThis->lpParentLog;
  return lpThis;
}

HRESULT CLoggable::WriteLogCommon(_In_ BOOL bAddError, _In_ HRESULT hResError, _In_z_ LPCWSTR szFormatW,
                                  _In_ va_list argptr)
{
  WCHAR szTempBufW[1024], *lpszBufW;
  int count[2];
  SIZE_T nTotal, nBufLen = MX_ARRAYLEN(szTempBufW);
  HRESULT hRes;

#ifndef DEBUGOUTPUT_LOG
  if (!cCallback)
    return E_FAIL;
#endif //!DEBUGOUTPUT_LOG

  lpszBufW = szTempBufW;
  count[0] = 0;
  if (bAddError != FALSE)
  {
    count[0] = _snwprintf_s(szTempBufW + (SIZE_T)count[0], MX_ARRAYLEN(szTempBufW) - (SIZE_T)count[0], _TRUNCATE,
                            L"Error 0x%08X: ", hResError);
    if (count[0] < 0)
      count[0] = 0;
  }

  count[1] = _vscwprintf(szFormatW, argptr);
  if (count[0] + count[1] + 3 < MX_ARRAYLEN(szTempBufW))
  {
    count[1] = _vsnwprintf_s(szTempBufW + count[0], MX_ARRAYLEN(szTempBufW) - (SIZE_T)count[0], _TRUNCATE, szFormatW,
                             argptr);
  }
  else
  {
    lpszBufW = (LPWSTR)MX_MALLOC(4096 * sizeof(WCHAR));
    if (lpszBufW == NULL)
      return E_OUTOFMEMORY;
    nBufLen = 4096;
    ::MxMemCopy(lpszBufW, szTempBufW, (SIZE_T)count[0] * sizeof(WCHAR));
    count[1] = _vsnwprintf_s(lpszBufW + (SIZE_T)count[0], nBufLen - (SIZE_T)count[0], _TRUNCATE, szFormatW, argptr);
    if (count[1] < 0)
      count[1] = (int)(unsigned int)wcslen(lpszBufW + (SIZE_T)count[0]);
  }
  nTotal = (SIZE_T)count[0] + (SIZE_T)count[1];
  if (nTotal > nBufLen - 1)
    nTotal = nBufLen - 1;
  lpszBufW[nTotal] = 0;

  hRes = (cCallback) ? cCallback(lpszBufW) : S_OK;
#ifdef DEBUGOUTPUT_LOG
  DebugPrint("%S\n", lpszBufW);
#endif //DEBUGOUTPUT_LOG
  if (lpszBufW != szTempBufW)
    MX_FREE(lpszBufW);

  //done
  return hRes;
}

} //namespace MX
