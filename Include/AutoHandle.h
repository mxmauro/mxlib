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
#ifndef _MX_AUTOHANDLE_H
#define _MX_AUTOHANDLE_H

#include "Defines.h"

//-----------------------------------------------------------

namespace MX {

class CWindowsHandle : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CWindowsHandle);
public:
  CWindowsHandle() : CBaseMemObj()
    {
    h = NULL;
    return;
    };

  ~CWindowsHandle()
    {
    Close();
    return;
    };

  VOID Close()
    {
    CloseHandleSEH(h);
    h = NULL;
    return;
    };

  HANDLE* operator&()
    {
    MX_ASSERT(h == NULL || h == INVALID_HANDLE_VALUE);
    return &h;
    };

  operator HANDLE() const
    {
    return Get();
    };

  HANDLE Get() const
    {
    return (h != NULL && h != INVALID_HANDLE_VALUE) ? h : NULL;
    };

  bool operator!() const
    {
    return (h == NULL || h == INVALID_HANDLE_VALUE) ? true : false;
    };

  operator bool() const
    {
    return (h != NULL && h != INVALID_HANDLE_VALUE) ? true : false;
    };

  VOID Attach(_In_ HANDLE _h)
    {
    Close();
    if (_h != NULL && _h != INVALID_HANDLE_VALUE)
      h = _h;
    return;
    };

  HANDLE Detach()
    {
    HANDLE hTemp;

    hTemp = (h != INVALID_HANDLE_VALUE) ? h : NULL;
    h = NULL;
    return hTemp;
    };

  static VOID CloseHandleSEH(_In_ HANDLE h)
    {
    if (h != NULL && h != INVALID_HANDLE_VALUE)
    {
#if defined(_M_IX86)
      ::MxCallStdCallWithSEH1(&::MxNtClose, NULL, (SIZE_T)h);
#elif defined(_M_X64)
      ::MxCallWithSEH(&::MxNtClose, NULL, (SIZE_T)h, 0, 0);
#endif
    }
    return;
    };

protected:
  HANDLE h;
};

//-----------------------------------------------------------

class CWindowsRemoteHandle : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CWindowsRemoteHandle);
public:
  CWindowsRemoteHandle() : CBaseMemObj()
    {
    hProc = h = NULL;
    return;
    };

  ~CWindowsRemoteHandle()
    {
    Close();
    return;
    };

  VOID Close()
    {
    CloseRemoteHandleSEH(hProc, h);
    CWindowsHandle::CloseHandleSEH(hProc);
    hProc = h = NULL;
    return;
    };

  HRESULT Init(_In_ DWORD dwPid, _In_ HANDLE _h)
    {
    NTSTATUS nNtStatus;

    if (dwPid == 0 || _h == NULL || _h == INVALID_HANDLE_VALUE)
      return E_INVALIDARG;
    Close();
    hProc = MxOpenProcess(SYNCHRONIZE|PROCESS_DUP_HANDLE, FALSE, dwPid);
    if (hProc == NULL)
      return E_ACCESSDENIED;
    nNtStatus = MxNtDuplicateObject(MX_CURRENTPROCESS, _h, hProc, &h, 0, 0, DUPLICATE_SAME_ACCESS);
    if (!NT_SUCCESS(nNtStatus))
    {
      Close();
      return HRESULT_FROM_NT(nNtStatus);
    }
    return S_OK;
    };

  HRESULT Init(_In_ HANDLE _hProc, _In_ HANDLE _h)
    {
    NTSTATUS nNtStatus;

    if (_hProc == NULL || _hProc == INVALID_HANDLE_VALUE ||
        _h == NULL || _h == INVALID_HANDLE_VALUE)
      return E_INVALIDARG;
    
    nNtStatus = MxNtDuplicateObject(MX_CURRENTPROCESS, _hProc, MX_CURRENTPROCESS, &hProc, 0, 0, DUPLICATE_SAME_ACCESS);
    if (NT_SUCCESS(nNtStatus))
      nNtStatus = MxNtDuplicateObject(MX_CURRENTPROCESS, _h, hProc, &h, 0, 0, DUPLICATE_SAME_ACCESS);
    if (!NT_SUCCESS(nNtStatus))
    {
      Close();
      return HRESULT_FROM_NT(nNtStatus);
    }
    return S_OK;
    };

  operator HANDLE() const
    {
    return Get();
    };

  HANDLE Get() const
    {
    return (h != NULL && h != INVALID_HANDLE_VALUE) ? h : NULL;
    };

  bool operator!() const
    {
    return (h == NULL || h == INVALID_HANDLE_VALUE) ? true : false;
    };

  operator bool() const
    {
    return (h != NULL && h != INVALID_HANDLE_VALUE) ? true : false;
    };

  VOID Attach(_In_ HANDLE _hProc, _In_ HANDLE _h)
    {
    Close();
    if (_hProc != NULL && _hProc != INVALID_HANDLE_VALUE)
      hProc = _hProc;
    if (_h != NULL && _h != INVALID_HANDLE_VALUE)
      h = _h;
    return;
    };

  HANDLE Detach()
    {
    HANDLE hTemp;

    hTemp = h;
    h = NULL;
    Close();
    return hTemp;
    };

  static VOID CloseRemoteHandleSEH(_In_ HANDLE hProc, _In_ HANDLE h)
    {
    if (h != NULL && h != INVALID_HANDLE_VALUE &&
        hProc != NULL && hProc != INVALID_HANDLE_VALUE)
    {
#if defined(_M_IX86)
      ::MxCallStdCallWithSEH2(&CWindowsRemoteHandle::InternalCloseRH, NULL, (SIZE_T)hProc, (SIZE_T)h);
#elif defined(_M_X64)
      ::MxCallWithSEH(&CWindowsRemoteHandle::InternalCloseRH, NULL, (SIZE_T)hProc, (SIZE_T)h, 0);
#endif
    }
    return;
    };

protected:
  HANDLE hProc, h;

private:
  static VOID InternalCloseRH(_In_ HANDLE hProc, _In_ HANDLE h)
    {
    MxNtDuplicateObject(hProc, h, hProc, NULL, 0, FALSE, DUPLICATE_CLOSE_SOURCE);
    return;
    };
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_AUTOHANDLE_H
