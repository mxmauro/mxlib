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
#ifndef _MX_WAITABLEOBJECTS_H
#define _MX_WAITABLEOBJECTS_H

#include "Defines.h"
#include "AtomicOps.h"
#include "AutoHandle.h"

//-----------------------------------------------------------

namespace MX {

BOOL IsMultiProcessor();
VOID _YieldProcessor();

//-----------------------------------------------------------

class CCriticalSection : public virtual CBaseMemObj, public CNonCopyableObj
{
public:
  CCriticalSection(_In_ ULONG nSpinCount = 4000) : CBaseMemObj(), CNonCopyableObj()
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
  class CAutoLock : public virtual CBaseMemObj, public CNonCopyableObj
  {
  public:
    CAutoLock(_In_ CCriticalSection &_cCS) : CBaseMemObj(), CNonCopyableObj(), cCS(_cCS)
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
  class CTryAutoLock : public virtual CBaseMemObj, public CNonCopyableObj
  {
  public:
    CTryAutoLock(_In_ CCriticalSection &cCS) : CBaseMemObj(), CNonCopyableObj()
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
public:
  CWindowsEvent();

  HRESULT Create(_In_ BOOL bManualReset, _In_ BOOL bInitialState, _In_opt_z_ LPCWSTR szNameW = NULL,
                 _In_opt_ LPSECURITY_ATTRIBUTES lpSecAttr=NULL, _Out_opt_ LPBOOL lpbAlreadyExists = NULL);

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
public:
  CWindowsMutex();

  HRESULT Create(_In_opt_z_ LPCWSTR szNameW = NULL, _In_ BOOL bInitialOwner = TRUE,
                 _In_opt_ LPSECURITY_ATTRIBUTES lpSecAttr = NULL, _Out_opt_ LPBOOL lpbAlreadyExists = NULL);

  HRESULT Open(_In_opt_z_ LPCWSTR szNameW = NULL, _In_ BOOL bQueryOnly = FALSE, _In_opt_ BOOL bInherit = FALSE);

  BOOL Lock(_In_ DWORD dwTimeout = INFINITE)
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

class CFastLock : public virtual CBaseMemObj, public CNonCopyableObj
{
public:
  CFastLock(_Inout_ LONG volatile *_lpnLock) : CBaseMemObj(), CNonCopyableObj()
    {
    LONG nTid = (LONG)::MxGetCurrentThreadId();

    nFlagMask = 0;
    lpnLock = _lpnLock;
    while (_InterlockedCompareExchange(lpnLock, nTid, 0) != 0)
      _YieldProcessor();
    return;
    };

  CFastLock(_Inout_ LONG volatile *_lpnLock, _In_ LONG _nFlagMask) : CBaseMemObj(), CNonCopyableObj()
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

#define MX_RWLOCK_INIT { 0 }

typedef union {
  LONG volatile nValue;
  SRWLOCK sOsLock;
} RWLOCK, *LPRWLOCK;

VOID SlimRWL_Initialize(_In_ LPRWLOCK lpLock);
BOOL SlimRWL_TryAcquireShared(_In_ LPRWLOCK lpLock);
VOID SlimRWL_AcquireShared(_In_ LPRWLOCK lpLock);
VOID SlimRWL_ReleaseShared(_In_ LPRWLOCK lpLock);
BOOL SlimRWL_TryAcquireExclusive(_In_ LPRWLOCK lpLock);
VOID SlimRWL_AcquireExclusive(_In_ LPRWLOCK lpLock);
VOID SlimRWL_ReleaseExclusive(_In_ LPRWLOCK lpLock);

class CAutoSlimRWLBase : public virtual CBaseMemObj
{
protected:
  CAutoSlimRWLBase(_In_ LPRWLOCK _lpLock, _In_ BOOL b) : CBaseMemObj()
    {
    lpLock = _lpLock;
    bShared = b;
    return;
    };

public:
  ~CAutoSlimRWLBase()
    {
    if (bShared == FALSE)
      SlimRWL_ReleaseExclusive(lpLock);
    else
      SlimRWL_ReleaseShared(lpLock);
    return;
    };

  VOID UpgradeToExclusive()
    {
    if (bShared != FALSE)
    {
      SlimRWL_ReleaseShared(lpLock);
      bShared = FALSE;
      SlimRWL_AcquireExclusive(lpLock);
    }
    return;
    };

  VOID DowngradeToShared()
    {
    if (bShared == FALSE)
    {
      SlimRWL_ReleaseExclusive(lpLock);
      bShared = TRUE;
      SlimRWL_AcquireShared(lpLock);
    }
    return;
    };

private:
  LPRWLOCK lpLock;
  BOOL bShared;
};

class CAutoSlimRWLShared : public CAutoSlimRWLBase, public CNonCopyableObj
{
public:
  CAutoSlimRWLShared(_In_ LPRWLOCK lpLock) : CAutoSlimRWLBase(lpLock, TRUE), CNonCopyableObj()
    {
    SlimRWL_AcquireShared(lpLock);
    return;
    };
};

class CAutoSlimRWLExclusive : public CAutoSlimRWLBase, public CNonCopyableObj
{
public:
  CAutoSlimRWLExclusive(_In_ LPRWLOCK lpLock) : CAutoSlimRWLBase(lpLock, FALSE), CNonCopyableObj()
    {
    SlimRWL_AcquireExclusive(lpLock);
    return;
    };
};

//-----------------------------------------------------------

VOID RundownProt_Initialize(_In_ LONG volatile *lpnValue);
BOOL RundownProt_Acquire(_In_ LONG volatile *lpnValue);
VOID RundownProt_Release(_In_ LONG volatile *lpnValue);
VOID RundownProt_WaitForRelease(_In_ LONG volatile *lpnValue);

class CAutoRundownProtection : public virtual CBaseMemObj, public CNonCopyableObj
{
public:
  CAutoRundownProtection(_In_ LONG volatile *_lpnValue) : CBaseMemObj(), CNonCopyableObj()
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
