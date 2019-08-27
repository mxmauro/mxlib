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
#include "JavascriptVMCommon.h"

//-----------------------------------------------------------

namespace MX {

CJsError::CJsError()
{
  lpStrMessageA = NULL;
  lpStrFileNameA = NULL;
  nLine = 0;
  lpStrStackTraceA = NULL;
  return;
}

CJsError::CJsError(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nStackIndex)
{
  lpStrMessageA = NULL;
  lpStrFileNameA = NULL;
  nLine = 0;
  lpStrStackTraceA = NULL;
  //-----
  if (lpCtx != NULL)
  {
    LPCSTR sA;
    HRESULT hRes = S_OK;

    try
    {
      //message
      DukTape::duk_get_prop_string(lpCtx, nStackIndex, "message");
      sA = DukTape::duk_get_string(lpCtx, -1);
      lpStrMessageA = MX_DEBUG_NEW CRefCountedStringA();
      if (lpStrMessageA != NULL)
      {
        if (lpStrMessageA->Copy((sA != NULL) ? sA : "") == FALSE)
          hRes = E_OUTOFMEMORY;
      }
      else
      {
        hRes = E_OUTOFMEMORY;
      }
      DukTape::duk_pop(lpCtx);

      //filename
      if (SUCCEEDED(hRes))
      {
        DukTape::duk_get_prop_string(lpCtx, nStackIndex, "fileName");
        sA = (DukTape::duk_is_undefined(lpCtx, -1) == false) ? DukTape::duk_safe_to_string(lpCtx, -1) : "";
        lpStrFileNameA = MX_DEBUG_NEW CRefCountedStringA();
        if (lpStrFileNameA != NULL)
        {
          if (lpStrFileNameA->Copy(sA) == FALSE)
            hRes = E_OUTOFMEMORY;
        }
        else
        {
          hRes = E_OUTOFMEMORY;
        }
        DukTape::duk_pop(lpCtx);
      }

      //line number
      if (SUCCEEDED(hRes))
      {
        DukTape::duk_get_prop_string(lpCtx, nStackIndex, "lineNumber");
        sA = (DukTape::duk_is_undefined(lpCtx, -1) == false) ? DukTape::duk_safe_to_string(lpCtx, -1) : "";
        while (*sA >= '0' && *sA <= '9')
        {
          nLine = nLine * 10 + ((ULONG)*sA++ - '0');
        }
        DukTape::duk_pop(lpCtx);
      }

      //stack trace
      if (SUCCEEDED(hRes))
      {
        DukTape::duk_get_prop_string(lpCtx, nStackIndex, "stack");
        sA = (DukTape::duk_is_undefined(lpCtx, -1) == false) ? DukTape::duk_safe_to_string(lpCtx, -1) : "N/A";
        lpStrStackTraceA = MX_DEBUG_NEW CRefCountedStringA();
        if (lpStrStackTraceA != NULL)
        {
          if (lpStrStackTraceA->Copy(sA) == FALSE)
            hRes = E_OUTOFMEMORY;
        }
        else
        {
          hRes = E_OUTOFMEMORY;
        }
        DukTape::duk_pop(lpCtx);
      }
    }
    catch (...)
    {
      Cleanup();
      throw;
    }
    if (FAILED(hRes))
    {
      Cleanup();
      throw CJsWindowsError(hRes);
    }
  }
  return;
}

CJsError::CJsError(_In_z_ LPCSTR szMessageA, _In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szFileNameA,
                   _In_ ULONG _nLine)
{
  HRESULT hRes = S_OK;

  lpStrMessageA = NULL;
  lpStrFileNameA = NULL;
  nLine = 0;
  lpStrStackTraceA = NULL;
  //----
  try
  {
    //message
    if (szMessageA != NULL && *szMessageA != 0)
    {
      lpStrMessageA = MX_DEBUG_NEW CRefCountedStringA();
      if (lpStrMessageA != NULL)
      {
        if (lpStrMessageA->Copy(szMessageA) == FALSE)
          hRes = E_OUTOFMEMORY;
      }
      else
      {
        hRes = E_OUTOFMEMORY;
      }
    }

    //filename
    if (SUCCEEDED(hRes) && szFileNameA != NULL && *szFileNameA != 0)
    {
      lpStrFileNameA = MX_DEBUG_NEW CRefCountedStringA();
      if (lpStrFileNameA != NULL)
      {
        if (lpStrFileNameA->Copy(szFileNameA) == FALSE)
          hRes = E_OUTOFMEMORY;
      }
      else
      {
        hRes = E_OUTOFMEMORY;
      }
    }

    //line number
    if (SUCCEEDED(hRes))
    {
      nLine = _nLine;
    }

    /* MEANINGLESS: Does not work but code left for future reference
    //stack trace
    if (SUCCEEDED(hRes))
    {
      //we haven't an error object so create a dummy one
      DukTape::duk_push_error_object_raw(lpCtx, DUK_ERR_ERROR, __FILE__, __LINE__, "");
      DukTape::duk_get_prop_string(lpCtx, -1, "stack");
      sA = (DukTape::duk_is_undefined(lpCtx, -1) == false) ? DukTape::duk_safe_to_string(lpCtx, -1) : "N/A";
      lpStrStackTraceA = MX_DEBUG_NEW CRefCountedStringA();
      if (lpStrStackTraceA != NULL)
      {
        if (lpStrStackTraceA->Copy(sA) == FALSE)
          hRes = E_OUTOFMEMORY;
      }
      else
      {
        hRes = E_OUTOFMEMORY;
      }
      DukTape::duk_pop_2(lpCtx);
    }
    */
  }
  catch (...)
  {
    Cleanup();
    throw;
  }
  if (FAILED(hRes))
  {
    Cleanup();
    throw CJsWindowsError(hRes);
  }
  //done
  return;
}

CJsError::CJsError(_In_ const CJsError &obj)
{
  lpStrMessageA = NULL;
  lpStrFileNameA = NULL;
  nLine = 0;
  lpStrStackTraceA = NULL;
  //----
  *this = obj;
  return;
}

CJsError::~CJsError()
{
  Cleanup();
  return;
}

CJsError& CJsError::operator=(_In_ const CJsError &obj)
{
  if (this != &obj)
  {
    Cleanup();
    //----
    lpStrMessageA = obj.lpStrMessageA;
    if (obj.lpStrMessageA != NULL)
      obj.lpStrMessageA->AddRef();
    //----
    lpStrFileNameA = obj.lpStrFileNameA;
    if (obj.lpStrFileNameA != NULL)
      obj.lpStrFileNameA->AddRef();
    //----
    nLine = obj.nLine;
    //----
    lpStrStackTraceA = obj.lpStrStackTraceA;
    if (obj.lpStrStackTraceA != NULL)
      obj.lpStrStackTraceA->AddRef();
  }
  return *this;
}

VOID CJsError::Cleanup()
{
  if (lpStrMessageA != NULL)
  {
    lpStrMessageA->Release();
    lpStrMessageA = NULL;
  }
  if (lpStrFileNameA != NULL)
  {
    lpStrFileNameA->Release();
    lpStrFileNameA = NULL;
  }
  if (lpStrStackTraceA != NULL)
  {
    lpStrStackTraceA->Release();
    lpStrStackTraceA = NULL;
  }
  return;
}

//-----------------------------------------------------------

CJsWindowsError::CJsWindowsError(_In_ DukTape::duk_context *lpCtx, _In_ DukTape::duk_idx_t nStackIndex) :
                 CJsError(lpCtx, nStackIndex)
{
  DukTape::duk_get_prop_string(lpCtx, nStackIndex, "hr");
  hRes = (HRESULT)DukTape::duk_get_int(lpCtx, -1);
  DukTape::duk_pop(lpCtx);
  return;
}

CJsWindowsError::CJsWindowsError(_In_ HRESULT _hRes, _In_ DukTape::duk_context *lpCtx,
                                 _In_ DukTape::duk_idx_t nStackIndex) : CJsError(lpCtx, nStackIndex)
{
  hRes = _hRes;
  return;
}

CJsWindowsError::CJsWindowsError(_In_ HRESULT _hRes, _In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szFileNameA,
                                 _In_ ULONG nLine) : CJsError("", lpCtx, szFileNameA, nLine)
{
  hRes = _hRes;
  QueryMessageString();
  return;
}

CJsWindowsError::CJsWindowsError(_In_ HRESULT _hRes) : CJsError()
{
  hRes = _hRes;
  QueryMessageString();
  return;
}

CJsWindowsError::CJsWindowsError(_In_ const CJsWindowsError &obj) : CJsError()
{
  *this = obj;
  return;
}

CJsWindowsError& CJsWindowsError::operator=(_In_ const CJsWindowsError &obj)
{
  if (this != &obj)
  {
    CJsError::operator=(obj);
    hRes = obj.hRes;
  }
  return *this;
}

VOID CJsWindowsError::QueryMessageString()
{
  CRefCountedStringA *lpStrA;
  LPWSTR szBufW;

  lpStrA = MX_DEBUG_NEW CRefCountedStringA();
  if (lpStrA == NULL)
    return;
  if (::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, (DWORD)hRes, 0,
                       (LPWSTR)&szBufW, 1024, NULL) > 0)
  {
    if (SUCCEEDED(Utf8_Encode(*lpStrA, szBufW)))
    {
      if (lpStrMessageA != NULL)
        lpStrMessageA->Release();
      lpStrMessageA = lpStrA;
      ::LocalFree((HLOCAL)szBufW);
      return;
    }
    ::LocalFree((HLOCAL)szBufW);
  }
  if (lpStrA->Format(L"0x%08X", hRes) != FALSE)
  {
    if (lpStrMessageA != NULL)
      lpStrMessageA->Release();
    lpStrMessageA = lpStrA;
    return;
  }
  lpStrA->Release();
  return;
}

} //namespace MX
