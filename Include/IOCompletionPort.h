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
#ifndef _MX_IOCOMPLETIONPORT_H
#define _MX_IOCOMPLETIONPORT_H

#include "Defines.h"
#include "LinkedList.h"
#include "Threads.h"
#include "Callbacks.h"

//-----------------------------------------------------------

namespace MX {

class CIoCompletionPort : private virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CIoCompletionPort);
public:
  CIoCompletionPort();
  ~CIoCompletionPort();

  HRESULT Initialize(_In_ DWORD dwMaxConcurrency);
  VOID Finalize();

  HRESULT Attach(_In_ HANDLE h, _In_ ULONG_PTR nKey);

  HRESULT Post(_In_ ULONG_PTR nKey, _In_ DWORD dwBytes, _In_ OVERLAPPED *lpOvr=NULL);
  HRESULT Post(_In_ LPOVERLAPPED_ENTRY lpEntry);

  HRESULT Get(_Out_ LPOVERLAPPED_ENTRY lpEntries, _Inout_ ULONG &nEntriesCount, _In_ DWORD dwTimeoutMs=INFINITE);

  operator HANDLE() const
    {
    return hIOCP;
    };

private:
  HANDLE hIOCP;
};

//-----------------------------------------------------------

class CIoCompletionPortThreadPool : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CIoCompletionPortThreadPool);
public:
  typedef Callback<VOID (_In_ CIoCompletionPortThreadPool *lpPool, _Inout_ LPVOID &lpUserData)> OnThreadStartCallback;
  typedef Callback<VOID (_In_ CIoCompletionPortThreadPool *lpPool, _In_ LPVOID lpUserData)> OnThreadEndCallback;
  typedef Callback<VOID (_In_ CIoCompletionPortThreadPool *lpPool, _In_ HRESULT hRes)> OnThreadStartErrorCallback;
  typedef Callback<VOID (_In_ CIoCompletionPortThreadPool *lpPool, _In_ DWORD dwBytes, _In_ OVERLAPPED *lpOvr,
                         _In_ HRESULT hRes)> OnPacketCallback;

public:
  CIoCompletionPortThreadPool(_In_opt_ DWORD dwMinThreadsCount=0, _In_opt_ DWORD dwMaxThreadsCount=0,
                              _In_opt_ DWORD dwWorkerThreadIdleTimeoutMs=2000,
                              _In_opt_ DWORD dwShutdownThreadThreshold=2);
  ~CIoCompletionPortThreadPool();

  VOID On(_In_opt_ OnThreadStartCallback cThreadStartCallback);
  VOID On(_In_opt_ OnThreadEndCallback cThreadEndCallback);
  VOID On(_In_opt_ OnThreadStartErrorCallback cThreadStartErrorCallback);

  HRESULT Initialize();
  VOID Finalize();

  //NOTE: 'cCallback' SHOULD be available when a packet is dequeued because a pointer is stored in the IOCP queue
  HRESULT Attach(_In_ HANDLE h, _In_ OnPacketCallback &cCallback);
  HRESULT Post(_In_ OnPacketCallback &cCallback, _In_ DWORD dwBytes, _In_ OVERLAPPED *lpOvr);

  DWORD GetActiveThreadsCount() const
    {
    return (DWORD)__InterlockedRead(const_cast<LONG volatile*>(&(sThreads.nActiveCount)));
    };

  DWORD GetBusyThreadsCount() const
    {
    return (DWORD)__InterlockedRead(const_cast<LONG volatile*>(&(sThreads.nBusyCount)));
    };

  BOOL IsDynamicGrowable() const
    {
    return (dwMaxThreadsCount > dwMinThreadsCount) ? TRUE : FALSE;
    };

  static DWORD GetNumberOfProcessors();

private:
  class CThread : public virtual CBaseMemObj, public TClassWorkerThread<CIoCompletionPortThreadPool>,
                  public TLnkLstNode<CThread>
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CThread);
  public:
    CThread() : CBaseMemObj(), TClassWorkerThread<CIoCompletionPortThreadPool>(), TLnkLstNode<CThread>()
      { };
  public:
    LONG nFlags;
  };

  VOID InternalFinalize();

  HRESULT StartThread(_In_ BOOL bInitial);
  VOID ThreadProc(_In_ SIZE_T nParam);

private:
  LONG volatile nSlimMutex;
  OnThreadStartCallback cThreadStartCallback;
  OnThreadEndCallback cThreadEndCallback;
  OnThreadStartErrorCallback cThreadStartErrorCallback;
  DWORD dwMinThreadsCount, dwMaxThreadsCount;
  DWORD dwWorkerThreadIdleTimeoutMs, dwShutdownThreadThreshold;
  CIoCompletionPort cIOCP;
  struct {
    LONG volatile nMutex;
    LONG volatile nShuttingDown;
    LONG volatile nBusyCount;
    LONG volatile nActiveCount;
    TLnkLst<CThread> cList;
  } sThreads;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_IOCOMPLETIONPORT_H
