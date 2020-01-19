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
#ifndef _MX_HTTPSERVER_H
#define _MX_HTTPSERVER_H

#include "..\Defines.h"
#include "..\Callbacks.h"
#include "..\Comm\Sockets.h"
#include "..\Comm\IpcSslLayer.h"
#include "HttpCommon.h"
#include "HttpCookie.h"
#include "HttpBodyParserDefault.h"
#include "HttpBodyParserMultipartFormData.h"
#include "HttpBodyParserUrlEncodedForm.h"
#include "HttpBodyParserIgnore.h"
#include "WebSockets.h"

//-----------------------------------------------------------

namespace MX {

class CHttpServer : public virtual CThread, public CLoggable, public CNonCopyableObj
{
public:
  class CClientRequest;

public:
  typedef struct {
    LPCSTR *lpszProtocolsA;
    int nSelectedProtocol;
    CWebSocket *lpWebSocket;
  } WEBSOCKET_REQUEST_CALLBACK_DATA;

public:
  typedef Callback<HRESULT (_In_ CHttpServer *lpHttp, _Out_ CClientRequest **lplpRequest)> OnNewRequestObjectCallback;

  typedef Callback<HRESULT (_In_ CHttpServer *lpHttp, _In_ CClientRequest *lpRequest,
                            _Outptr_ _Maybenull_ CHttpBodyParserBase **lplpBodyParser)>
                            OnRequestHeadersReceivedCallback;

  typedef Callback<VOID (_In_ CHttpServer *lpHttp, _In_ CClientRequest *lpRequest)> OnRequestCompletedCallback;

  typedef Callback<HRESULT (_In_ CHttpServer *lpHttp, _In_ CClientRequest *lpRequest,
                            _Inout_ WEBSOCKET_REQUEST_CALLBACK_DATA &sData)> OnWebSocketRequestReceivedCallback;

  typedef Callback<VOID(_In_ CHttpServer *lpHttp, _In_ CClientRequest *lpRequest,
                        _In_ HRESULT hrErrorCode)> OnErrorCallback;

  //--------

public:
  CHttpServer(_In_ CSockets &cSocketMgr, _In_opt_ CLoggable *lpLogParent = NULL);
  ~CHttpServer();

  VOID SetOption_MaxConnectionsPerIp(_In_ DWORD dwLimit);
  VOID SetOption_RequestHeaderTimeout(_In_ DWORD dwTimeoutMs);
  VOID SetOption_RequestBodyLimits(_In_ DWORD dwMinimumThroughputInBps, _In_ DWORD dwSecondsOfLowThroughput);
  VOID SetOption_ResponseLimits(_In_ DWORD dwMinimumThroughputInBps, _In_ DWORD dwSecondsOfLowThroughput);
  VOID SetOption_MaxHeaderSize(_In_ DWORD dwSize);
  VOID SetOption_MaxFieldSize(_In_ DWORD dwSize);
  VOID SetOption_MaxFileSize(_In_ ULONGLONG ullSize);
  VOID SetOption_MaxFilesCount(_In_ DWORD dwCount);
  BOOL SetOption_TemporaryFolder(_In_opt_z_ LPCWSTR szFolderW);
  VOID SetOption_MaxBodySizeInMemory(_In_ DWORD dwSize);
  VOID SetOption_MaxBodySize(_In_ ULONGLONG ullSize);

  VOID SetNewRequestObjectCallback(_In_ OnNewRequestObjectCallback cNewRequestObjectCallback);
  VOID SetRequestHeadersReceivedCallback(_In_ OnRequestHeadersReceivedCallback cRequestHeadersReceivedCallback);
  VOID SetRequestCompletedCallback(_In_ OnRequestCompletedCallback cRequestCompletedCallback);
  VOID SetWebSocketRequestReceivedCallback(_In_ OnWebSocketRequestReceivedCallback cWebSocketRequestReceivedCallback);
  VOID SetErrorCallback(_In_ OnErrorCallback cErrorCallback);

  HRESULT StartListening(_In_ CSockets::eFamily nFamily, _In_ int nPort,
                         _In_opt_ CIpcSslLayer::eProtocol nProtocol = CIpcSslLayer::ProtocolUnknown,
                         _In_opt_ CSslCertificate *lpSslCertificate = NULL, _In_opt_ CCryptoRSA *lpSslKey = NULL);
  HRESULT StartListening(_In_opt_z_ LPCSTR szBindAddressA, _In_ CSockets::eFamily nFamily, _In_ int nPort,
                         _In_opt_ CIpcSslLayer::eProtocol nProtocol = CIpcSslLayer::ProtocolUnknown,
                         _In_opt_ CSslCertificate *lpSslCertificate = NULL, _In_opt_ CCryptoRSA *lpSslKey = NULL);
  HRESULT StartListening(_In_opt_z_ LPCWSTR szBindAddressW, _In_ CSockets::eFamily nFamily, _In_ int nPort,
                         _In_opt_ CIpcSslLayer::eProtocol nProtocol = CIpcSslLayer::ProtocolUnknown,
                         _In_opt_ CSslCertificate *lpSslCertificate = NULL, _In_opt_ CCryptoRSA *lpSslKey = NULL);
  VOID StopListening();

public:
  class CClientRequest : public CIpc::CUserData, public TLnkLstNode<CClientRequest>
  {
  protected:
    CClientRequest();
  public:
    ~CClientRequest();

    virtual VOID End(_In_opt_ HRESULT hrErrorCode = S_OK);

    BOOL MustAbort() const;
    HANDLE GetAbortEvent() const;

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
      return reinterpret_cast<T*>(GetRequestHeader(T::GetHeaderNameStatic()));
      };
    CHttpHeaderBase* GetRequestHeader(_In_z_ LPCSTR szNameA) const;

    SIZE_T GetRequestCookiesCount() const;
    CHttpCookie* GetRequestCookie(_In_ SIZE_T nIndex) const;

    CHttpBodyParserBase* GetRequestBodyParser() const;

    VOID ResetResponse();

    HRESULT SetResponseStatus(_In_ LONG nStatus, _In_opt_ LPCSTR szReasonA = NULL);

    template<class T>
    HRESULT AddResponseHeader(_Out_opt_ T **lplpHeader = NULL, _In_ BOOL bReplaceExisting = TRUE)
      {
      return AddResponseHeader(T::GetHeaderNameStatic(), reinterpret_cast<CHttpHeaderBase**>(lplpHeader),
                               bReplaceExisting);
      };
    HRESULT AddResponseHeader(_In_z_ LPCSTR szNameA, _Out_opt_ CHttpHeaderBase **lplpHeader = NULL,
                              _In_ BOOL bReplaceExisting = TRUE);

    template<class T>
    HRESULT AddResponseHeader(_In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1,
                              _Out_opt_ T **lplpHeader = NULL, _In_ BOOL bReplaceExisting = TRUE)
      {
      return AddResponseHeader(T::GetHeaderNameStatic(), szValueA, nValueLen,
                               reinterpret_cast<CHttpHeaderBase**>(lplpHeader), bReplaceExisting);
      };
    HRESULT AddResponseHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1,
                              _Out_opt_ CHttpHeaderBase **lplpHeader = NULL, _In_ BOOL bReplaceExisting = TRUE);

    //NOTE: Unicode values will be UTF-8 & URL encoded
    template<class T>
    HRESULT AddResponseHeader(_In_z_ LPCWSTR szValueW, _In_opt_ SIZE_T nValueLen=(SIZE_T)-1,
                              _Out_opt_ T **lplpHeader = NULL, _In_ BOOL bReplaceExisting = TRUE)
      {
      return AddResponseHeader(T::GetHeaderNameStatic(), szValueW, nValueLen,
                               reinterpret_cast<CHttpHeaderBase**>(lplpHeader), bReplaceExisting);
      };
    HRESULT AddResponseHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW, _In_opt_ SIZE_T nValueLen=(SIZE_T)-1,
                              _Out_opt_ CHttpHeaderBase **lplpHeader = NULL, _In_ BOOL bReplaceExisting = TRUE);

    HRESULT RemoveResponseHeader(_In_z_ LPCSTR szNameA);
    HRESULT RemoveResponseHeader(_In_ CHttpHeaderBase *lpHeader);
    HRESULT RemoveAllResponseHeaders();

    SIZE_T GetResponseHeadersCount() const;

    CHttpHeaderBase* GetResponseHeader(_In_ SIZE_T nIndex) const;
    template<class T>
    T* GetResponseHeader() const
      {
      return reinterpret_cast<T*>(GetResponseHeader(T::GetHeaderNameStatic()));
      };
    CHttpHeaderBase* GetResponseHeader(_In_z_ LPCSTR szNameA) const;

    HRESULT AddResponseCookie(_In_ CHttpCookie &cSrc);
    HRESULT AddResponseCookie(_In_ CHttpCookieArray &cSrc);
    HRESULT AddResponseCookie(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA, _In_opt_z_ LPCSTR szDomainA = NULL,
                              _In_opt_z_ LPCSTR szPathA = NULL, _In_opt_ const CDateTime *lpDate = NULL,
                              _In_opt_ BOOL bIsSecure = FALSE, _In_opt_ BOOL bIsHttpOnly = FALSE);
    HRESULT AddResponseCookie(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szValueW, _In_opt_z_ LPCWSTR szDomainW = NULL,
                              _In_opt_z_ LPCWSTR szPathW = NULL, _In_opt_ const CDateTime *lpDate = NULL,
                              _In_opt_ BOOL bIsSecure = FALSE, _In_opt_ BOOL bIsHttpOnly = FALSE);
    HRESULT RemoveResponseCookie(_In_z_ LPCSTR szNameA);
    HRESULT RemoveResponseCookie(_In_z_ LPCWSTR szNameW);
    HRESULT RemoveAllResponseCookies();
    CHttpCookie* GetResponseCookie(_In_ SIZE_T nIndex) const;
    CHttpCookie* GetResponseCookie(_In_z_ LPCSTR szNameA) const;
    CHttpCookie* GetResponseCookie(_In_z_ LPCWSTR szNameW) const;
    CHttpCookieArray* GetResponseCookies() const;
    SIZE_T GetResponseCookiesCount() const;

    CHttpHeaderBase::eBrowser GetBrowser() const
      {
      return nBrowser;
      };

    HRESULT SendHeaders();
    HRESULT SendResponse(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen);
    HRESULT SendFile(_In_z_ LPCWSTR szFileNameW);
    HRESULT SendStream(_In_ CStream *lpStream, _In_opt_z_ LPCWSTR szFileNameW=NULL);

    HRESULT SendErrorPage(_In_ LONG nStatusCode, _In_ HRESULT hrErrorCode, _In_opt_z_ LPCSTR szBodyExplanationA = NULL);

    HRESULT ResetAndDisableClientCache();

    HANDLE GetUnderlyingSocketHandle() const;
    CSockets* GetUnderlyingSocketManager() const;

    virtual HRESULT OnSetup()
      {
      return S_OK;
      };

    //Return FALSE if you encounter an error during cleanup and the request object cannot be reused
    virtual BOOL OnCleanup()
      {
      return TRUE;
      };

  private:
    friend class CHttpServer;

    typedef enum {
      StateClosed = 0,
      StateInitializing,
      StateReceivingRequestHeaders,
      StateReceivingRequestBody,
      StateBuildingResponse,
      StateSendingResponse,
      StateNegotiatingWebSocket,
      StateWebSocket,
      StateError
    } eState;

  private:
    HRESULT Initialize(_In_ CHttpServer *lpHttpServer, _In_ CSockets *lpSocketMgr, _In_ HANDLE hConn,
                       _In_ DWORD dwMaxHeaderSize);

    HRESULT ResetForNewRequest();

    HRESULT SetState(_In_ eState nNewState);

    BOOL IsKeepAliveRequest() const;
    BOOL HasErrorBeenSent() const;
    BOOL HasHeadersBeenSent() const;

    HRESULT BuildAndInsertOrSendHeaderStream();
    HRESULT BuildAndSendWebSocketHeaderStream(_In_z_ LPCSTR szProtocolA);

    HRESULT SendQueuedStreams();

    VOID MarkLinkAsClosed();
    BOOL IsLinkClosed() const;

    VOID QueryBrowser();

  private:
    class CRequest : public CBaseMemObj
    {
    public:
      CRequest(_In_ CHttpServer *lpHttpServer) : CBaseMemObj(), cHttpCmn(TRUE, lpHttpServer)
        {
        dwHeadersTimeoutCounterSecs = DWORD_MAX;
        dwBodyLowThroughputCcounter = 0;
        return;
        };

      VOID ResetForNewRequest()
        {
        cHttpCmn.ResetParser();
        dwHeadersTimeoutCounterSecs = DWORD_MAX;
        dwBodyLowThroughputCcounter = 0;
        return;
        };

    public:
      CHttpCommon cHttpCmn;
      DWORD dwHeadersTimeoutCounterSecs;
      DWORD dwBodyLowThroughputCcounter;
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
        dwLowThroughputCcounter = 0;
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
        dwLowThroughputCcounter = 0;
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
      DWORD dwLowThroughputCcounter;
    };

  private:
    typedef struct tagWEBSOCKET_INFO {
      CHttpHeaderReqSecWebSocketKey *lpReqSecWebSocketKeyHeader;
      LPCSTR szProtocolsA[1];
    } WEBSOCKET_INFO, *LPWEBSOCKET_INFO;

  private:
    CHttpHeaderBase::eBrowser nBrowser;
    CCriticalSection cMutex;
    CHttpServer *lpHttpServer;
    CSockets *lpSocketMgr;
    HANDLE hConn;
    SOCKADDR_INET sPeerAddr;
    eState nState;
    LONG volatile nFlags;
    MX::TAutoDeletePtr<CRequest> cRequest;
    MX::TAutoDeletePtr<CResponse> cResponse;
    MX::TAutoFreePtr<WEBSOCKET_INFO> cWebSocketInfo;
  };

private:
  friend class CClientRequest;

  VOID ThreadProc();

  HRESULT OnSocketCreate(_In_ CIpc *lpIpc, _In_ HANDLE h, _Inout_ CIpc::CREATE_CALLBACK_DATA &sData);
  VOID OnListenerSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                               _In_ HRESULT hrErrorCode);

  VOID OnSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                       _In_ HRESULT hrErrorCode);
  HRESULT OnSocketConnect(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData, _In_ HRESULT hrErrorCode);
  HRESULT OnSocketDataReceived(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData);

  HRESULT QuickSendErrorResponseAndReset(_In_ CClientRequest *lpRequest, _In_ LONG nErrorCode, _In_ HRESULT hrErrorCode,
                                         _In_ BOOL bForceClose);

  HRESULT GenerateErrorPage(_Out_ CStringA &cStrResponseA, _In_ CClientRequest *lpRequest, _In_ LONG nErrorCode,
                            _In_ HRESULT hrErrorCode, _In_opt_z_ LPCSTR szBodyExplanationA=NULL);
  static LPCSTR LocateError(_In_ LONG nErrorCode);

  VOID OnAfterSendResponse(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ LPVOID lpCookie, _In_ CIpc::CUserData *lpUserData);

  HRESULT CheckForWebSocket(_In_ CClientRequest *lpRequest);
  HRESULT InitiateWebSocket(_In_ CClientRequest *lpRequest, _In_ WEBSOCKET_REQUEST_CALLBACK_DATA &sData,
                            _Out_ BOOL &bFatal);

  VOID OnRequestDestroyed(_In_ CClientRequest *lpRequest);
  VOID OnRequestEnding(_In_ CClientRequest *lpRequest, _In_ HRESULT hrErrorCode);

  HRESULT OnDownloadStarted(_Out_ LPHANDLE lphFile, _In_z_ LPCWSTR szFileNameW, _In_ LPVOID lpUserParam);

private:
  class CRequestLimiter : public virtual CBaseMemObj, public TRedBlackTreeNode<CRequestLimiter>
  {
  public:
    CRequestLimiter(_In_ PSOCKADDR_INET lpAddr) : CBaseMemObj(), TRedBlackTreeNode<CRequestLimiter>()
      {
      MxMemCopy(&sAddr, lpAddr, sizeof(SOCKADDR_INET));
      _InterlockedExchange(&nCount, 1);
      return;
      };

    static int InsertCompareFunc(_In_ LPVOID lpContext, _In_ CRequestLimiter *lpLim1, _In_ CRequestLimiter *lpLim2)
      {
      return ::MxMemCompare(&(lpLim1->sAddr), &(lpLim2->sAddr), sizeof(SOCKADDR_INET));
      };

    static int SearchCompareFunc(_In_ LPVOID lpContext, _In_ PSOCKADDR_INET key, _In_ CRequestLimiter *lpLim)
      {
      return ::MxMemCompare(key, &(lpLim->sAddr), sizeof(SOCKADDR_INET));
      };

  public:
    SOCKADDR_INET sAddr;
    LONG volatile nCount;
  };

private:
  CCriticalSection cs;
  CSockets &cSocketMgr;
  DWORD dwMaxConnectionsPerIp;
  DWORD dwRequestHeaderTimeoutMs;
  DWORD dwRequestBodyMinimumThroughputInBps, dwResponseMinimumThroughputInBps;
  DWORD dwRequestBodySecondsOfLowThroughput, dwResponseSecondsOfLowThroughput;
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
  LONG volatile nRundownLock;
  HANDLE hAcceptConn;

  OnNewRequestObjectCallback cNewRequestObjectCallback;
  OnRequestHeadersReceivedCallback cRequestHeadersReceivedCallback;
  OnRequestCompletedCallback cRequestCompletedCallback;
  OnWebSocketRequestReceivedCallback cWebSocketRequestReceivedCallback;
  OnErrorCallback cErrorCallback;
  LONG volatile nRequestsListRwMutex;
  TLnkLst<CClientRequest> cRequestsList;
  CWindowsEvent cShutdownEv;

  struct {
    LONG volatile nRwMutex;
    TRedBlackTree<CRequestLimiter> cTree;
  } sRequestLimiter;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPSERVER_H
