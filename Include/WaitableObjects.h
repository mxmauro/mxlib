/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2015. All rights reserved.
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
#ifndef _MX_WAITABLEOBJECTS_H
#define _MX_WAITABLEOBJECTS_H

#include "Defines.h"
#include <intrin.h>
#include "AutoHandle.h"

#pragma intrinsic (_InterlockedExchange)
#pragma intrinsic (_InterlockedCompareExchange)
#pragma intrinsic (_InterlockedExchangeAdd)
#if defined(_M_X64)
  #pragma intrinsic (_InterlockedExchangeAdd64)
  #pragma intrinsic (_InterlockedExchange64)
  #pragma intrinsic (_InterlockedCompareExchange64)
#endif //_M_X64

//-----------------------------------------------------------

namespace MX {

BOOL IsMultiProcessor();
VOID _YieldProcessor();

static __inline LONG __InterlockedRead(__inout LONG volatile *lpnValue)
{
  return _InterlockedExchangeAdd(lpnValue, 0L);
}

#if defined(_M_IX86)
static __inline LPVOID __InterlockedReadPointer(__inout LPVOID volatile *lpValue)
{
  return (LPVOID)_InterlockedExchangeAdd((LONG volatile*)lpValue, 0L);
}

static __inline LPVOID __InterlockedExchangePointer(__inout LPVOID volatile *lpValue, __in LPVOID lpPtr)
{
  return (LPVOID)_InterlockedExchange((LONG volatile*)lpValue, (long)lpPtr);
}

static __inline LPVOID __InterlockedCompareExchangePointer(__inout LPVOID volatile *lpValue, __in LPVOID lpExchange,
                                                           __in LPVOID lpComperand)
{
  return (LPVOID)_InterlockedCompareExchange((LONG volatile*)lpValue, (long)lpExchange, (long)lpComperand);
}

#elif defined(_M_X64)

static __inline LPVOID __InterlockedReadPointer(__inout LPVOID volatile *lpValue)
{
  return (LPVOID)_InterlockedExchangeAdd64((__int64 volatile*)lpValue, 0i64);
}

static __inline LPVOID __InterlockedExchangePointer(__inout LPVOID volatile *lpValue, __in LPVOID lpPtr)
{
  return (LPVOID)_InterlockedExchange64((__int64 volatile*)lpValue, (__int64)lpPtr);
}

static __inline LPVOID __InterlockedCompareExchangePointer(__inout LPVOID volatile *lpValue, __in LPVOID lpExchange,
                                                           __in LPVOID lpComperand)
{
  return (LPVOID)_InterlockedCompareExchange64((__int64 volatile*)lpValue, (__int64)lpExchange, (__int64)lpComperand);
}
#endif

static __forceinline SIZE_T __InterlockedReadSizeT(__inout SIZE_T volatile *lpnValue)
{
  return (SIZE_T)__InterlockedReadPointer((LPVOID volatile*)lpnValue);
}

static __forceinline SIZE_T __InterlockedExchangeSizeT(__inout SIZE_T volatile *lpnValue, __in SIZE_T nNewValue)
{
  return (SIZE_T)__InterlockedExchangePointer((LPVOID volatile*)lpnValue, (LPVOID)nNewValue);
}

static __forceinline SIZE_T __InterlockedCompareExchangeSizeT(__inout SIZE_T volatile *lpnValue, __in SIZE_T nExchange,
                                                              __in SIZE_T nComperand)
{
  return (SIZE_T)__InterlockedCompareExchangePointer((LPVOID volatile*)lpnValue, (LPVOID)nExchange, (LPVOID)nComperand);
}

//-----------------------------------------------------------

class CCriticalSection : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CCriticalSection);
public:
  CCriticalSection(__in ULONG nSpinCount=4000) : CBaseMemObj()
    {
    if (::MxRtlInitializeCriticalSectionAndSpinCount(&cs, nSpinCount) == STATUS_NOT_IMPLEMENTED)
      ::MxRtlInitializeCriticalSection(&cs);
    return;
    };

  virtual ~CCriticalSection()
    {
    ::MxRtlDeleteCriticalSection(&cs);
    return;
    };

  _inline VOID Lock()
    {
    ::MxRtlEnterCriticalSection(&cs);
    return;
    };

  _inline BOOL TryLock()
    {
    return ::MxRtlTryEnterCriticalSection(&cs);
    };

  _inline VOID Unlock()
    {
    ::MxRtlLeaveCriticalSection(&cs);
    return;
    };

public:
  class CAutoLock : public virtual CBaseMemObj
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CAutoLock);
  public:
    CAutoLock(__in CCriticalSection &_cCS) : CBaseMemObj(), cCS(_cCS)
      {
      cCS.Lock();
      return;
      };

    ~CAutoLock()
      {
      cCS.Unlock();
      return;
      };

    BOOL IsLockHeld() const
      {
      return bLockHeld;
      };

  private:
    CCriticalSection &cCS;
    BOOL bLockHeld;
  };

public:
  class CTryAutoLock : public virtual CBaseMemObj
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CTryAutoLock);
  public:
    CTryAutoLock(__in CCriticalSection &cCS) : CBaseMemObj()
      {
      lpCS = (cCS.TryLock() != FALSE) ? &cCS : NULL;
      return;
      };

    ~CTryAutoLock()
      {
      if (lpCS != NULL)
        lpCS->Unlock();
      return;
      };

    BOOL IsLocked() const
      {
      return (lpCS != NULL) ? TRUE : FALSE;
      };

  private:
    CCriticalSection *lpCS;
  };

private:
  RTL_CRITICAL_SECTION cs;
};

//-----------------------------------------------------------

class CWindowsEvent : public CWindowsHandle
{
  MX_DISABLE_COPY_CONSTRUCTOR(CWindowsEvent);
public:
  CWindowsEvent() : CWindowsHandle()
    { };

  BOOL Create(__in BOOL bManualReset, __in BOOL bInitialState, __in_z_opt LPCWSTR szNameW=NULL,
              __in_opt LPSECURITY_ATTRIBUTES lpSecAttr=NULL, __out_opt LPBOOL lpbAlreadyExists=NULL)
    {
    MX_OBJECT_ATTRIBUTES sObjAttr;
    MX_UNICODE_STRING usName;
    HANDLE _h;
    NTSTATUS nNtStatus;

    if (lpbAlreadyExists != NULL)
      *lpbAlreadyExists = FALSE;
    MemSet(&sObjAttr, 0, sizeof(sObjAttr));
    sObjAttr.Length = (ULONG)sizeof(sObjAttr);
    sObjAttr.Attributes = 0x00000080; //OBJ_OPENIF
    if (szNameW != NULL)
    {
      //get root directory handle
      sObjAttr.RootDirectory = GetRootDirHandle();
      if (sObjAttr.RootDirectory == NULL)
        return FALSE;
      //setup object name
      usName.Buffer = (PWSTR)szNameW;
      for (usName.Length=0; szNameW[usName.Length]!=0; usName.Length++);
      usName.Length *= (USHORT)sizeof(WCHAR);
      usName.MaximumLength = usName.Length;
      sObjAttr.ObjectName = &usName;
    }
    //setup security
    if (lpSecAttr != NULL)
    {
      sObjAttr.SecurityDescriptor = lpSecAttr->lpSecurityDescriptor;
      if (lpSecAttr->bInheritHandle != FALSE)
        sObjAttr.Attributes |= 0x00000002; //OBJ_INHERIT
    }
    //create event
    nNtStatus = ::MxNtCreateEvent(&_h, EVENT_ALL_ACCESS, &sObjAttr,
                                  (bManualReset == FALSE) ? MxSynchronizationEvent : MxNotificationEvent,
                                  bInitialState);
    //close root directory
    if (sObjAttr.RootDirectory != NULL)
      ::MxNtClose(sObjAttr.RootDirectory);
    //process result
    if (!NT_SUCCESS(nNtStatus))
      return FALSE;
    if (nNtStatus == STATUS_OBJECT_NAME_EXISTS && lpbAlreadyExists != NULL)
      *lpbAlreadyExists = TRUE;
    Attach(_h);
    return TRUE;
    };

  BOOL Open(__in_z_opt LPCWSTR szNameW=NULL, __in_opt BOOL bInherit=FALSE)
    {
    MX_OBJECT_ATTRIBUTES sObjAttr;
    MX_UNICODE_STRING usName;
    NTSTATUS nNtStatus;
    HANDLE _h;

    MemSet(&sObjAttr, 0, sizeof(sObjAttr));
    sObjAttr.Length = (ULONG)sizeof(sObjAttr);
    sObjAttr.Attributes = 0x00000080; //OBJ_OPENIF
    if (bInherit != FALSE)
      sObjAttr.Attributes |= 0x00000002; //OBJ_INHERIT
    if (szNameW != NULL)
    {
      //get root directory handle
      sObjAttr.RootDirectory = GetRootDirHandle();
      if (sObjAttr.RootDirectory == NULL)
        return FALSE;
      //setup object name
      usName.Buffer = (PWSTR)szNameW;
      for (usName.Length=0; szNameW[usName.Length]!=0; usName.Length++);
      usName.Length *= (USHORT)sizeof(WCHAR);
      usName.MaximumLength = usName.Length;
      sObjAttr.ObjectName = &usName;
    }
    //open event
    nNtStatus = ::MxNtOpenEvent(&_h, EVENT_ALL_ACCESS, &sObjAttr);
    //close root directory
    if (sObjAttr.RootDirectory != NULL)
      ::MxNtClose(sObjAttr.RootDirectory);
    //process result
    if (!NT_SUCCESS(nNtStatus))
      return FALSE;
    Attach(_h);
    return TRUE;
    };

  BOOL Wait(__in DWORD dwTimeout=0)
    {
    LARGE_INTEGER liTimeout, *lpliTimeout;

    if (h == NULL)
      return TRUE;
    lpliTimeout = NULL;
    if (dwTimeout != INFINITE)
    {
      liTimeout.QuadPart = (LONGLONG)MX_MILLISECONDS_TO_100NS(dwTimeout);
      lpliTimeout = &liTimeout;
    }
    return (::MxNtWaitForSingleObject(h, FALSE, lpliTimeout) == WAIT_OBJECT_0) ? TRUE : FALSE;
    };

  BOOL Reset()
    {
    return (h != NULL && NT_SUCCESS(::MxNtResetEvent(h, NULL))) ? TRUE : FALSE;
    };

  BOOL Set()
    {
    return (h != NULL && NT_SUCCESS(::MxNtSetEvent(h, NULL))) ? TRUE : FALSE;
    };

private:
  HANDLE GetRootDirHandle()
    {
    MX_OBJECT_ATTRIBUTES sObjAttr;
    MX_UNICODE_STRING usName;
    HANDLE _h;
    NTSTATUS nNtStatus;

    //open root directory
    MemSet(&sObjAttr, 0, sizeof(sObjAttr));
    sObjAttr.Length = (ULONG)sizeof(sObjAttr);
    usName.Buffer = L"\\BaseNamedObjects";
    usName.Length = usName.MaximumLength = 34;
    sObjAttr.ObjectName = &usName;
    nNtStatus = ::MxNtOpenDirectoryObject(&_h, 6, &sObjAttr); //DIRECTORY_CREATE_OBJECT|DIRECTORY_TRAVERSE
    return (NT_SUCCESS(nNtStatus)) ? _h : NULL;
    };
};

//-----------------------------------------------------------

class CWindowsMutex : public CWindowsHandle
{
  MX_DISABLE_COPY_CONSTRUCTOR(CWindowsMutex);
public:
  CWindowsMutex() : CWindowsHandle()
    { };

  BOOL Create(__in_z_opt LPCWSTR szNameW=NULL, __in BOOL bInitialOwner=TRUE,
              __in_opt LPSECURITY_ATTRIBUTES lpSecAttr=NULL, __out_opt LPBOOL lpbAlreadyExists=NULL)
    {
    MX_OBJECT_ATTRIBUTES sObjAttr;
    MX_UNICODE_STRING usName;
    HANDLE _h;
    NTSTATUS nNtStatus;

    if (lpbAlreadyExists != NULL)
      *lpbAlreadyExists = FALSE;
    MemSet(&sObjAttr, 0, sizeof(sObjAttr));
    sObjAttr.Length = (ULONG)sizeof(sObjAttr);
    sObjAttr.Attributes = 0x00000080; //OBJ_OPENIF
    if (szNameW != NULL)
    {
      //get root directory handle
      sObjAttr.RootDirectory = GetRootDirHandle();
      if (sObjAttr.RootDirectory == NULL)
        return FALSE;
      //setup object name
      usName.Buffer = (PWSTR)szNameW;
      for (usName.Length=0; szNameW[usName.Length]!=0; usName.Length++);
      usName.Length *= (USHORT)sizeof(WCHAR);
      usName.MaximumLength = usName.Length;
      sObjAttr.ObjectName = &usName;
    }
    //setup security
    if (lpSecAttr != NULL)
    {
      sObjAttr.SecurityDescriptor = lpSecAttr->lpSecurityDescriptor;
      if (lpSecAttr->bInheritHandle != FALSE)
        sObjAttr.Attributes |= 0x00000002; //OBJ_INHERIT
    }
    //create mutant
    nNtStatus = ::MxNtCreateMutant(&_h, MUTANT_ALL_ACCESS, &sObjAttr, (BOOLEAN)bInitialOwner);
    //close root directory
    if (sObjAttr.RootDirectory != NULL)
      ::MxNtClose(sObjAttr.RootDirectory);
    //process result
    if (!NT_SUCCESS(nNtStatus))
      return FALSE;
    if (nNtStatus == STATUS_OBJECT_NAME_EXISTS && lpbAlreadyExists != NULL)
      *lpbAlreadyExists = TRUE;
    Attach(_h);
    return TRUE;
    };

  BOOL Open(__in_z_opt LPCWSTR szNameW=NULL, __in BOOL bQueryOnly=FALSE, __in_opt BOOL bInherit=FALSE)
    {
    MX_OBJECT_ATTRIBUTES sObjAttr;
    MX_UNICODE_STRING usName;
    NTSTATUS nNtStatus;
    HANDLE _h;

    MemSet(&sObjAttr, 0, sizeof(sObjAttr));
    sObjAttr.Length = (ULONG)sizeof(sObjAttr);
    sObjAttr.Attributes = 0x00000080; //OBJ_OPENIF
    if (bInherit != FALSE)
      sObjAttr.Attributes |= 0x00000002; //OBJ_INHERIT
    if (szNameW != NULL)
    {
      //get root directory handle
      sObjAttr.RootDirectory = GetRootDirHandle();
      if (sObjAttr.RootDirectory == NULL)
        return FALSE;
      //setup object name
      usName.Buffer = (PWSTR)szNameW;
      for (usName.Length=0; szNameW[usName.Length]!=0; usName.Length++);
      usName.Length *= (USHORT)sizeof(WCHAR);
      usName.MaximumLength = usName.Length;
      sObjAttr.ObjectName = &usName;
    }
    //open mutant
    nNtStatus = ::MxNtOpenMutant(&_h, (bQueryOnly == FALSE) ? MUTANT_ALL_ACCESS : (STANDARD_RIGHTS_READ|SYNCHRONIZE),
                                 &sObjAttr);
    //close root directory
    if (sObjAttr.RootDirectory != NULL)
      ::MxNtClose(sObjAttr.RootDirectory);
    //process result
    if (!NT_SUCCESS(nNtStatus))
      return FALSE;
    Attach(_h);
    return TRUE;
    };

  BOOL Lock(__in DWORD dwTimeout=INFINITE)
    {
    LARGE_INTEGER liTimeout, *lpliTimeout;

    if (h == NULL)
      return FALSE;
    lpliTimeout = NULL;
    if (dwTimeout != INFINITE)
    {
      liTimeout.QuadPart = (LONGLONG)MX_MILLISECONDS_TO_100NS(dwTimeout);
      lpliTimeout = &liTimeout;
    }
    return (::MxNtWaitForSingleObject(h, FALSE, lpliTimeout) == WAIT_OBJECT_0) ? TRUE : FALSE;
    };

  BOOL Unlock()
    {
    return (h != NULL && NT_SUCCESS(::MxNtReleaseMutant(h, NULL))) ? TRUE : FALSE;
    };

private:
  HANDLE GetRootDirHandle()
    {
    MX_OBJECT_ATTRIBUTES sObjAttr;
    MX_UNICODE_STRING usName;
    HANDLE _h;
    NTSTATUS nNtStatus;

    //open root directory
    MemSet(&sObjAttr, 0, sizeof(sObjAttr));
    sObjAttr.Length = (ULONG)sizeof(sObjAttr);
    usName.Buffer = L"\\BaseNamedObjects";
    usName.Length = usName.MaximumLength = 34;
    sObjAttr.ObjectName = &usName;
    nNtStatus = ::MxNtOpenDirectoryObject(&_h, 6, &sObjAttr); //DIRECTORY_CREATE_OBJECT|DIRECTORY_TRAVERSE
    return (NT_SUCCESS(nNtStatus)) ? _h : NULL;
    };
};

//-----------------------------------------------------------

class CFastLock : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CFastLock);
public:
  CFastLock(__inout LONG volatile *_lpnLock) : CBaseMemObj()
    {
    nFlagMask = 0;
    lpnLock = _lpnLock;
    while (_InterlockedCompareExchange(lpnLock, 1, 0) != 0)
      _YieldProcessor();
    return;
    };

  CFastLock(__inout LONG volatile *_lpnLock, __in LONG _nFlagMask) : CBaseMemObj()
    {
    nFlagMask = _nFlagMask;
    lpnLock = _lpnLock;
    while ((_InterlockedOr(lpnLock, nFlagMask) & nFlagMask) != 0)
      MX::_YieldProcessor();
    return;
    };

  ~CFastLock()
    {
    if (nFlagMask == 0)
      _InterlockedExchange(lpnLock, 0);
    else
      _InterlockedAnd(lpnLock, ~nFlagMask);
    return;
    };

private:
  LONG volatile *lpnLock;
  LONG nFlagMask;
};

//-----------------------------------------------------------

VOID SlimRWL_Initialize(__in LONG volatile *lpnValue);
BOOL SlimRWL_TryAcquireShared(__in LONG volatile *lpnValue);
VOID SlimRWL_AcquireShared(__in LONG volatile *lpnValue);
VOID SlimRWL_ReleaseShared(__in LONG volatile *lpnValue);
BOOL SlimRWL_TryAcquireExclusive(__in LONG volatile *lpnValue);
VOID SlimRWL_AcquireExclusive(__in LONG volatile *lpnValue);
VOID SlimRWL_ReleaseExclusive(__in LONG volatile *lpnValue);

class CAutoSlimRWLBase : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CAutoSlimRWLBase);
protected:
  CAutoSlimRWLBase(__in LONG volatile *_lpnValue, __in BOOL b) : CBaseMemObj()
    {
    lpnValue = _lpnValue;
    bShared = b;
    return;
    };

public:
  ~CAutoSlimRWLBase()
    {
    if (bShared == FALSE)
      SlimRWL_ReleaseExclusive(lpnValue);
    else
      SlimRWL_ReleaseShared(lpnValue);
    return;
    };

  VOID UpgradeToExclusive()
    {
    if (bShared != FALSE)
    {
      SlimRWL_ReleaseShared(lpnValue);
      bShared = FALSE;
      SlimRWL_AcquireExclusive(lpnValue);
    }
    return;
    };

  VOID DowngradeToShared()
    {
    if (bShared == FALSE)
    {
      SlimRWL_ReleaseExclusive(lpnValue);
      bShared = TRUE;
      SlimRWL_AcquireShared(lpnValue);
    }
    return;
    };

private:
  LONG volatile *lpnValue;
  BOOL bShared;
};

class CAutoSlimRWLShared : public CAutoSlimRWLBase
{
  MX_DISABLE_COPY_CONSTRUCTOR(CAutoSlimRWLShared);
public:
  CAutoSlimRWLShared(__in LONG volatile *lpnValue) : CAutoSlimRWLBase(lpnValue, TRUE)
    {
    SlimRWL_AcquireShared(lpnValue);
    return;
    };
};

class CAutoSlimRWLExclusive : public CAutoSlimRWLBase
{
  MX_DISABLE_COPY_CONSTRUCTOR(CAutoSlimRWLExclusive);
public:
  CAutoSlimRWLExclusive(__in LONG volatile *lpnValue) : CAutoSlimRWLBase(lpnValue, FALSE)
    {
    SlimRWL_AcquireExclusive(lpnValue);
    return;
    };
};

//-----------------------------------------------------------

VOID RundownProt_Initialize(__in LONG volatile *lpnValue);
BOOL RundownProt_Acquire(__in LONG volatile *lpnValue);
VOID RundownProt_Release(__in LONG volatile *lpnValue);
VOID RundownProt_WaitForRelease(__in LONG volatile *lpnValue);

class CAutoRundownProtection : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CAutoRundownProtection);
public:
  CAutoRundownProtection(__in LONG volatile *_lpnValue) : CBaseMemObj()
    {
    bAcquired = RundownProt_Acquire(lpnValue = _lpnValue);
    return;
    };

  ~CAutoRundownProtection()
    {
    if (bAcquired != FALSE)
      RundownProt_Release(lpnValue);
    return;
    };

  BOOL IsAcquired()
    {
    return bAcquired;
    };

private:
  LONG volatile *lpnValue;
  BOOL bAcquired;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_WAITABLEOBJECTS_H
