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
#ifndef _MX_HTTPSERVER_H
#define _MX_HTTPSERVER_H

#include "..\Defines.h"
#include "..\Callbacks.h"
#include "..\TimedEventQueue.h"
#include "..\Comm\Sockets.h"
#include "..\Comm\IpcSslLayer.h"
#include "HttpCommon.h"
#include "HttpCookie.h"
#include "HttpBodyParserDefault.h"
#include "HttpBodyParserMultipartFormData.h"
#include "HttpBodyParserUrlEncodedForm.h"
#include "HttpBodyParserIgnore.h"

//-----------------------------------------------------------

namespace MX {

class CHttpServer : public virtual CBaseMemObj, public CLoggable, private CCriticalSection
{
  MX_DISABLE_COPY_CONSTRUCTOR(CHttpServer);
public:
  class CClientRequest;

  //--------

  typedef Callback<HRESULT (_Out_ CClientRequest **lplpRequest)> OnNewRequestObjectCallback;
  typedef Callback<VOID (_In_ CHttpServer *lpHttp, _In_ CClientRequest *lpRequest, _In_ HANDLE hShutdownEv,
                         _Outptr_ _Maybenull_ CHttpBodyParserBase **lplpBodyParser)> OnRequestHeadersReceivedCallback;
  typedef Callback<VOID (_In_ CHttpServer *lpHttp, _In_ CClientRequest *lpRequest,
                         _In_ HANDLE hShutdownEv)> OnRequestCompletedCallback;

  typedef Callback<VOID (_In_ CHttpServer *lpHttp, _In_ CClientRequest *lpRequest, _In_ HRESULT hErrorCode)> OnErrorCallback;

  //--------

public:
  CHttpServer(_In_ CSockets &cSocketMgr, _In_ CIoCompletionPortThreadPool &cWorkerPool,
              _In_opt_ CLoggable *lpLogParent = NULL);
  ~CHttpServer();

  VOID SetOption_MaxRequestTimeoutMs(_In_ DWORD dwTimeoutMs);
  VOID SetOption_MaxHeaderSize(_In_ DWORD dwSize);
  VOID SetOption_MaxFieldSize(_In_ DWORD dwSize);
  VOID SetOption_MaxFileSize(_In_ ULONGLONG ullSize);
  VOID SetOption_MaxFilesCount(_In_ DWORD dwCount);
  BOOL SetOption_TemporaryFolder(_In_opt_z_ LPCWSTR szFolderW);
  VOID SetOption_MaxBodySizeInMemory(_In_ DWORD dwSize);
  VOID SetOption_MaxBodySize(_In_ ULONGLONG ullSize);

  VOID On(_In_ OnNewRequestObjectCallback cNewRequestObjectCallback);
  VOID On(_In_ OnRequestHeadersReceivedCallback cRequestHeadersReceivedCallback);
  VOID On(_In_ OnRequestCompletedCallback cRequestCompletedCallback);
  VOID On(_In_ OnErrorCallback cErrorCallback);

  HRESULT StartListening(_In_ int nPort, _In_opt_ CIpcSslLayer::eProtocol nProtocol=CIpcSslLayer::ProtocolUnknown,
                         _In_opt_ CSslCertificate *lpSslCertificate=NULL, _In_opt_ CCryptoRSA *lpSslKey=NULL);
  HRESULT StartListening(_In_opt_z_ LPCSTR szBindAddressA, _In_ int nPort,
                         _In_opt_ CIpcSslLayer::eProtocol nProtocol=CIpcSslLayer::ProtocolUnknown,
                         _In_opt_ CSslCertificate *lpSslCertificate=NULL, _In_opt_ CCryptoRSA *lpSslKey=NULL);
  HRESULT StartListening(_In_opt_z_ LPCWSTR szBindAddressW, _In_ int nPort,
                         _In_opt_ CIpcSslLayer::eProtocol nProtocol=CIpcSslLayer::ProtocolUnknown,
                         _In_opt_ CSslCertificate *lpSslCertificate=NULL, _In_opt_ CCryptoRSA *lpSslKey=NULL);
  VOID StopListening();

public:
  class CClientRequest : public CIpc::CUserData, public TLnkLstNode<CClientRequest>
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CClientRequest);
  protected:
    CClientRequest();
  public:
    ~CClientRequest();

    VOID End(_In_opt_ HRESULT hErrorCode=S_OK);

    VOID EnableDirectResponse();

    LPCSTR GetMethod() const;
    CUrl* GetUrl() const;

    CHttpServer* GetHttpServer() const
      {
      return lpHttpServer;
      };

    SIZE_T GetRequestHeadersCount() const;
    CHttpHeaderBase* GetRequestHeader(_In_ SIZE_T nIndex) const;

    template<class T>
    T* GetRequestHeader() const
      {
      return reinterpret_cast<T*>(GetRequestHeader(T::GetNameStatic()));
      };
    CHttpHeaderBase* GetRequestHeader(_In_z_ LPCSTR szNameA) const;

    SIZE_T GetRequestCookiesCount() const;
    CHttpCookie* GetRequestCookie(_In_ SIZE_T nIndex) const;

    CHttpBodyParserBase* GetRequestBodyParser() const;

    VOID ResetResponse();

    HRESULT SetResponseStatus(_In_ LONG nStatus, _In_opt_ LPCSTR szReasonA=NULL);

    template<class T>
    HRESULT AddResponseHeader(_Out_opt_ T **lplpHeader=NULL, _In_ BOOL bReplaceExisting=TRUE)
      {
      return AddResponseHeader(T::GetNameStatic(), reinterpret_cast<CHttpHeaderBase**>(lplpHeader), bReplaceExisting);
      };
    HRESULT AddResponseHeader(_In_z_ LPCSTR szNameA, _Out_opt_ CHttpHeaderBase **lplpHeader=NULL,
                              _In_ BOOL bReplaceExisting=TRUE);

    template<class T>
    HRESULT AddResponseHeader(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen=(SIZE_T)-1,
                              _Out_opt_ T **lplpHeader=NULL, _In_ BOOL bReplaceExisting=TRUE)
      {
      return AddResponseHeader(T::GetNameStatic(), szValueA, nValueLen, reinterpret_cast<CHttpHeaderBase**>(lplpHeader),
                               bReplaceExisting);
      };
    HRESULT AddResponseHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen=(SIZE_T)-1,
                              _Out_opt_ CHttpHeaderBase **lplpHeader=NULL, _In_ BOOL bReplaceExisting=TRUE);

    //NOTE: Unicode values will be UTF-8 & URL encoded
    template<class T>
    HRESULT AddResponseHeader(_In_z_ LPCWSTR szValueW, _In_opt_ SIZE_T nValueLen=(SIZE_T)-1,
                              _Out_opt_ T **lplpHeader=NULL, _In_ BOOL bReplaceExisting=TRUE)
      {
      return AddResponseHeader(T::GetNameStatic(), szValueW, nValueLen, reinterpret_cast<CHttpHeaderBase**>(lplpHeader),
                               bReplaceExisting);
      };
    HRESULT AddResponseHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW, _In_opt_ SIZE_T nValueLen=(SIZE_T)-1,
                              _Out_opt_ CHttpHeaderBase **lplpHeader=NULL, _In_ BOOL bReplaceExisting=TRUE);

    HRESULT RemoveResponseHeader(_In_z_ LPCSTR szNameA);
    HRESULT RemoveResponseHeader(_In_ CHttpHeaderBase *lpHeader);
    HRESULT RemoveAllResponseHeaders();

    SIZE_T GetResponseHeadersCount() const;

    CHttpHeaderBase* GetResponseHeader(_In_ SIZE_T nIndex) const;
    template<class T>
    T* GetResponseHeader() const
      {
      return reinterpret_cast<T*>(GetResponseHeader(T::GetNameStatic()));
      };
    CHttpHeaderBase* GetResponseHeader(_In_z_ LPCSTR szNameA) const;

    HRESULT AddResponseCookie(_In_ CHttpCookie &cSrc);
    HRESULT AddResponseCookie(_In_ CHttpCookieArray &cSrc);
    HRESULT AddResponseCookie(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA, _In_opt_z_ LPCSTR szDomainA=NULL,
                              _In_opt_z_ LPCSTR szPathA=NULL, _In_opt_ const CDateTime *lpDate=NULL,
                              _In_opt_ BOOL bIsSecure=FALSE, _In_opt_ BOOL bIsHttpOnly=FALSE);
    HRESULT AddResponseCookie(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szValueW, _In_opt_z_ LPCWSTR szDomainW=NULL,
                              _In_opt_z_ LPCWSTR szPathW=NULL, _In_opt_ const CDateTime *lpDate=NULL,
                              _In_opt_ BOOL bIsSecure=FALSE, _In_opt_ BOOL bIsHttpOnly=FALSE);
    HRESULT RemoveResponseCookie(_In_z_ LPCSTR szNameA);
    HRESULT RemoveResponseCookie(_In_z_ LPCWSTR szNameW);
    HRESULT RemoveAllResponseCookies();
    CHttpCookie* GetResponseCookie(_In_ SIZE_T nIndex) const;
    CHttpCookie* GetResponseCookie(_In_z_ LPCSTR szNameA) const;
    CHttpCookie* GetResponseCookie(_In_z_ LPCWSTR szNameW) const;
    CHttpCookieArray* GetResponseCookies() const;
    SIZE_T GetResponseCookiesCount() const;

    HRESULT SendHeaders();
    HRESULT SendResponse(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen);
    HRESULT SendFile(_In_z_ LPCWSTR szFileNameW);
    HRESULT SendStream(_In_ CStream *lpStream, _In_opt_z_ LPCWSTR szFileNameW=NULL);

    HRESULT SendErrorPage(_In_ LONG nStatusCode, _In_ HRESULT hErrorCode, _In_opt_z_ LPCSTR szBodyExplanationA=NULL);

    HANDLE GetUnderlyingSocket() const;
    CSockets* GetUnderlyingSocketManager() const;

    virtual HRESULT OnSetup()
      {
      return S_OK;
      };

    virtual VOID OnCleanup()
      {
      return;
      };

  private:
    HRESULT Initialize(_In_ CHttpServer *lpHttpServer, _In_ CSockets *lpSocketMgr, _In_ HANDLE hConn,
                       _In_ DWORD dwMaxHeaderSize);

    VOID ResetForNewRequest();

    BOOL IsKeepAliveRequest() const;
    BOOL HasErrorBeenSent() const;
    BOOL HasHeadersBeenSent() const;

    HRESULT BuildAndInsertOrSendHeaderStream();

    HRESULT SendQueuedStreams();

    VOID MarkLinkAsClosed();
    BOOL IsLinkClosed() const;

  private:
    friend class CHttpServer;

    typedef enum {
      StateClosed = 0,
      StateInitializing,
      StateReceivingRequestHeaders,
      StateReceivingRequestBody,
      StateBuildingResponse,
      StateError
    } eState;

  private:
    class CRequest : public CBaseMemObj
    {
    public:
      CRequest(_In_ CHttpServer *lpHttpServer) : CBaseMemObj(), cHttpCmn(TRUE, lpHttpServer)
        {
        _InterlockedExchange(&(sTimeout.nMutex), 0);
        liLastRead.QuadPart = 0ui64;
        return;
        };

      VOID ResetForNewRequest()
        {
        cUrl.Reset();
        cStrMethodA.Empty();
        cHttpCmn.ResetParser();
        liLastRead.QuadPart = 0ui64;
        return;
        };

    public:
      CUrl cUrl;
      CStringA cStrMethodA;
      CHttpCommon cHttpCmn;
      struct {
        LONG volatile nMutex;
        TPendingListHelperGeneric<CTimedEventQueue::CEvent*> cActiveList;
      } sTimeout;
      ULARGE_INTEGER liLastRead;
    };

  private:
    class CResponse : public CBaseMemObj
    {
    public:
      CResponse(_In_ CHttpServer *lpHttpServer) : CBaseMemObj(), cHttpCmn(FALSE, lpHttpServer)
        {
        nStatus = 0;
        bLastStreamIsData = FALSE;
        szMimeTypeHintA = NULL;
        bDirect = FALSE;
        return;
        };

      VOID ResetForNewRequest()
        {
        nStatus = 0;
        cStrReasonA.Empty();
        cHttpCmn.ResetParser();
        aStreamsList.RemoveAllElements();
        bLastStreamIsData = FALSE;
        szMimeTypeHintA = NULL;
        cStrFileNameA.Empty();
        bDirect = FALSE;
        return;
        };

    public:
      LONG nStatus;
      CStringA cStrReasonA;
      CHttpCommon cHttpCmn;
      TArrayListWithRelease<CStream*> aStreamsList;
      BOOL bLastStreamIsData;
      LPCSTR szMimeTypeHintA;
      CStringA cStrFileNameA;
      BOOL bDirect;
    };

  private:
    OVERLAPPED sOvr;
    CCriticalSection cMutex;
    CHttpServer *lpHttpServer;
    CSockets *lpSocketMgr;
    HANDLE hConn;
    eState nState;
    LONG volatile nFlags;
    MX::TAutoDeletePtr<CRequest> cRequest;
    MX::TAutoDeletePtr<CResponse> cResponse;
  };

private:
  friend class CClientRequest;

  typedef TArrayList<CTimedEventQueue::CEvent*> __TEventArray;

  HRESULT OnSocketCreate(_In_ CIpc *lpIpc, _In_ HANDLE h, _Inout_ CIpc::CREATE_CALLBACK_DATA &sData);
  VOID OnListenerSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                               _In_ HRESULT hErrorCode);

  VOID OnSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                       _In_ HRESULT hErrorCode);
  HRESULT OnSocketConnect(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                          _Inout_ CIpc::CLayerList &cLayersList, _In_ HRESULT hErrorCode);
  HRESULT OnSocketDataReceived(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData);

  VOID OnRequestTimeout(_In_ CTimedEventQueue::CEvent *lpEvent);

  VOID OnRequestCompleted(_In_ CIoCompletionPortThreadPool *lpPool, _In_ DWORD dwBytes, _In_ OVERLAPPED *lpOvr,
                          _In_ HRESULT hRes);

  HRESULT QuickSendErrorResponseAndReset(_In_ CClientRequest *lpRequest, _In_ LONG nErrorCode, _In_ HRESULT hErrorCode,
                                         _In_ BOOL bForceClose);

  HRESULT GenerateErrorPage(_Out_ CStringA &cStrResponseA, _In_ CClientRequest *lpRequest, _In_ LONG nErrorCode,
                            _In_ HRESULT hErrorCode, _In_opt_z_ LPCSTR szBodyExplanationA=NULL);
  static LPCSTR LocateError(_In_ LONG nErrorCode);

  VOID OnAfterSendResponse(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ LPVOID lpCookie, _In_ CIpc::CUserData *lpUserData);

  HRESULT SetupTimeoutEvent(_In_ CClientRequest *lpRequest, _In_ DWORD dwTimeoutMs);
  VOID CancelAllTimeoutEvents(_In_ CClientRequest *lpRequest);

  VOID DoCancelEventsCallback(_In_ __TEventArray &cEventsList);

  VOID OnRequestDestroyed(_In_ CClientRequest *lpRequest);
  VOID OnRequestEnding(_In_ CClientRequest *lpRequest, _In_ HRESULT hErrorCode);

  HRESULT OnDownloadStarted(_Out_ LPHANDLE lphFile, _In_z_ LPCWSTR szFileNameW, _In_ LPVOID lpUserParam);

private:
  CSockets &cSocketMgr;
  CIoCompletionPortThreadPool &cWorkerPool;
  //NOTE: CIoCompletionPortThreadPool::Post needs a non-dynamic variable
  CIoCompletionPortThreadPool::OnPacketCallback cRequestCompletedWP;

  DWORD dwMaxRequestTimeoutMs;
  DWORD dwMaxHeaderSize;
  DWORD dwMaxFieldSize;
  ULONGLONG ullMaxFileSize;
  DWORD dwMaxFilesCount;
  CStringW cStrTemporaryFolderW;
  DWORD dwMaxBodySizeInMemory;
  ULONGLONG ullMaxBodySize;

  LONG volatile nDownloadNameGeneratorCounter;
  struct {
    LONG volatile nRwMutex;
    CIpcSslLayer::eProtocol nProtocol;
    TAutoDeletePtr<CSslCertificate> cSslCertificate;
    TAutoDeletePtr<CCryptoRSA> cSslPrivateKey;
  } sSsl;
  CSystemTimedEventQueue *lpTimedEventQueue;
  LONG volatile nRundownLock;
  HANDLE hAcceptConn;
  OnNewRequestObjectCallback cNewRequestObjectCallback;
  OnRequestHeadersReceivedCallback cRequestHeadersReceivedCallback;
  OnRequestCompletedCallback cRequestCompletedCallback;
  OnErrorCallback cErrorCallback;
  LONG volatile nRequestsMutex;
  TLnkLst<CClientRequest> cRequestsList;
  CWindowsEvent cShutdownEv;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPSERVER_H
