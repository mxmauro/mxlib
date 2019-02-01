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

#define __InterlockedRead(lpnValue) _InterlockedExchangeAdd(lpnValue, 0L)

#if defined(_M_IX86)

#define __InterlockedReadPointer(lpValue)                                                          \
           ((LPVOID)_InterlockedExchangeAdd((LONG volatile*)lpValue, 0L))
#define __InterlockedExchangePointer(lpValue, lpPtr)                                               \
           ((LPVOID)_InterlockedExchange((LONG volatile*)lpValue, PtrToLong(lpPtr)))
#define __InterlockedCompareExchangePointer(lpValue, lpExchange, lpComperand)                      \
           ((LPVOID)_InterlockedCompareExchange((LONG volatile*)lpValue, (LONG)lpExchange,         \
                                                PtrToLong(lpComperand)))

#elif defined(_M_X64)

#define __InterlockedReadPointer(lpValue)                                                          \
           ((LPVOID)_InterlockedExchangeAdd64((__int64 volatile*)lpValue, 0i64))
#define __InterlockedExchangePointer(lpValue, lpPtr)                                               \
           ((LPVOID)_InterlockedExchange64((__int64 volatile*)lpValue, (__int64)lpPtr))
#define __InterlockedCompareExchangePointer(lpValue, lpExchange, lpComperand)                      \
           ((LPVOID)_InterlockedCompareExchange64((__int64 volatile*)lpValue, (__int64)lpExchange, \
                                                  (__int64)lpComperand))

#endif

#define __InterlockedReadSizeT(lpnValue)                                                           \
           ((SIZE_T)__InterlockedReadPointer((LPVOID volatile*)lpnValue))
#define __InterlockedExchangeSizeT(lpnValue, nNewValue)                                            \
           ((SIZE_T)__InterlockedExchangePointer((LPVOID volatile*)lpnValue, (LPVOID)nNewValue))
#define __InterlockedCompareExchangeSizeT(lpnValue, nExchange, nComperand)                         \
           ((SIZE_T)__InterlockedCompareExchangePointer((LPVOID volatile*)lpnValue,                \
                                                        (LPVOID)nExchange, (LPVOID)nComperand))

//-----------------------------------------------------------

class CCriticalSection : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CCriticalSection);
public:
  CCriticalSection(_In_ ULONG nSpinCount=4000) : CBaseMemObj()
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
    CAutoLock(_In_ CCriticalSection &_cCS) : CBaseMemObj(), cCS(_cCS)
      {
      cCS.Lock();
      return;
      };

    ~CAutoLock()
      {
      cCS.Unlock();
      return;
      };

  private:
    CCriticalSection &cCS;
  };

public:
  class CTryAutoLock : public virtual CBaseMemObj
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CTryAutoLock);
  public:
    CTryAutoLock(_In_ CCriticalSection &cCS) : CBaseMemObj()
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
  CWindowsEvent();

  HRESULT Create(_In_ BOOL bManualReset, _In_ BOOL bInitialState, _In_opt_z_ LPCWSTR szNameW=NULL,
                 _In_opt_ LPSECURITY_ATTRIBUTES lpSecAttr=NULL, _Out_opt_ LPBOOL lpbAlreadyExists=NULL);

  HRESULT Open(_In_z_ LPCWSTR szNameW, _In_opt_ BOOL bInherit=FALSE);

  BOOL Wait(_In_ DWORD dwTimeoutMs);

  BOOL Reset()
    {
    return (h != NULL && NT_SUCCESS(::MxNtResetEvent(h, NULL))) ? TRUE : FALSE;
    };

  BOOL Set()
    {
    return (h != NULL && NT_SUCCESS(::MxNtSetEvent(h, NULL))) ? TRUE : FALSE;
    };
};

//-----------------------------------------------------------

class CWindowsMutex : public CWindowsHandle
{
  MX_DISABLE_COPY_CONSTRUCTOR(CWindowsMutex);
public:
  CWindowsMutex();

  HRESULT Create(_In_opt_z_ LPCWSTR szNameW=NULL, _In_ BOOL bInitialOwner=TRUE,
                 _In_opt_ LPSECURITY_ATTRIBUTES lpSecAttr=NULL, _Out_opt_ LPBOOL lpbAlreadyExists=NULL);

  HRESULT Open(_In_opt_z_ LPCWSTR szNameW=NULL, _In_ BOOL bQueryOnly=FALSE, _In_opt_ BOOL bInherit=FALSE);

  BOOL Lock(_In_ DWORD dwTimeout=INFINITE)
    {
    LARGE_INTEGER liTimeout, *lpliTimeout;

    if (h == NULL)
      return FALSE;
    lpliTimeout = NULL;
    if (dwTimeout != INFINITE)
    {
      liTimeout.QuadPart = -(LONGLONG)MX_MILLISECONDS_TO_100NS(dwTimeout);
      lpliTimeout = &liTimeout;
    }
    return (::MxNtWaitForSingleObject(h, FALSE, lpliTimeout) == WAIT_OBJECT_0) ? TRUE : FALSE;
    };

  BOOL Unlock()
    {
    return (h != NULL && NT_SUCCESS(::MxNtReleaseMutant(h, NULL))) ? TRUE : FALSE;
    };
};

//-----------------------------------------------------------

class CFastLock : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CFastLock);
public:
  CFastLock(_Inout_ LONG volatile *_lpnLock) : CBaseMemObj()
    {
    nFlagMask = 0;
    lpnLock = _lpnLock;
    while (_InterlockedCompareExchange(lpnLock, 1, 0) != 0)
      _YieldProcessor();
    return;
    };

  CFastLock(_Inout_ LONG volatile *_lpnLock, _In_ LONG _nFlagMask) : CBaseMemObj()
    {
    nFlagMask = _nFlagMask;
    lpnLock = _lpnLock;
    while ((_InterlockedOr(lpnLock, nFlagMask) & nFlagMask) != 0)
      _YieldProcessor();
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

VOID SlimRWL_Initialize(_In_ LONG volatile *lpnValue);
BOOL SlimRWL_TryAcquireShared(_In_ LONG volatile *lpnValue);
VOID SlimRWL_AcquireShared(_In_ LONG volatile *lpnValue);
VOID SlimRWL_ReleaseShared(_In_ LONG volatile *lpnValue);
BOOL SlimRWL_TryAcquireExclusive(_In_ LONG volatile *lpnValue);
VOID SlimRWL_AcquireExclusive(_In_ LONG volatile *lpnValue);
VOID SlimRWL_ReleaseExclusive(_In_ LONG volatile *lpnValue);

class CAutoSlimRWLBase : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CAutoSlimRWLBase);
protected:
  CAutoSlimRWLBase(_In_ LONG volatile *_lpnValue, _In_ BOOL b) : CBaseMemObj()
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
  CAutoSlimRWLShared(_In_ LONG volatile *lpnValue) : CAutoSlimRWLBase(lpnValue, TRUE)
    {
    SlimRWL_AcquireShared(lpnValue);
    return;
    };
};

class CAutoSlimRWLExclusive : public CAutoSlimRWLBase
{
  MX_DISABLE_COPY_CONSTRUCTOR(CAutoSlimRWLExclusive);
public:
  CAutoSlimRWLExclusive(_In_ LONG volatile *lpnValue) : CAutoSlimRWLBase(lpnValue, FALSE)
    {
    SlimRWL_AcquireExclusive(lpnValue);
    return;
    };
};

//-----------------------------------------------------------

VOID RundownProt_Initialize(_In_ LONG volatile *lpnValue);
BOOL RundownProt_Acquire(_In_ LONG volatile *lpnValue);
VOID RundownProt_Release(_In_ LONG volatile *lpnValue);
VOID RundownProt_WaitForRelease(_In_ LONG volatile *lpnValue);

class CAutoRundownProtection : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CAutoRundownProtection);
public:
  CAutoRundownProtection(_In_ LONG volatile *_lpnValue) : CBaseMemObj()
    {
    lpnValue = (RundownProt_Acquire(_lpnValue) != FALSE) ? _lpnValue : NULL;
    return;
    };

  ~CAutoRundownProtection()
    {
    if (lpnValue != NULL)
      RundownProt_Release(lpnValue);
    return;
    };

  BOOL IsAcquired() const
    {
    return (lpnValue != NULL) ? TRUE : FALSE;
    };

private:
  LONG volatile *lpnValue;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_WAITABLEOBJECTS_H
