/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2019. All rights reserved.
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

class CWindowsRemoteHandle
{
  MX_DISABLE_COPY_CONSTRUCTOR(CWindowsRemoteHandle);
public:
  CWindowsRemoteHandle()
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
