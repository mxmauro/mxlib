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
#include "..\TimedEvent.h"
#include "..\Comm\Sockets.h"
#include "HttpCommon.h"
#include "HttpCookie.h"
#include "HttpBodyParserBase.h"
#include "WebSockets.h"

//-----------------------------------------------------------

namespace MX {

class CHttpServer : public virtual CBaseMemObj, public CLoggable, public CNonCopyableObj
{
public:
  class CClientRequest;

public:
  typedef Callback<HRESULT(_In_ CHttpServer *lpHttp, _Outptr_result_maybenull_ CSslCertificate **lplpSslCert,
                           _Outptr_result_maybenull_ CEncryptionKey **lplpSslPrivKey,
                           _Outptr_result_maybenull_ CDhParam **lplpDhParam)> OnQuerySslCertificatesCallback;

  typedef Callback<HRESULT (_In_ CHttpServer *lpHttp, _Out_ CClientRequest **lplpRequest)> OnNewRequestObjectCallback;

  typedef Callback<HRESULT (_In_ CHttpServer *lpHttp, _In_ CClientRequest *lpRequest,
                            _Outptr_result_maybenull_ CHttpBodyParserBase **lplpBodyParser)>
                            OnRequestHeadersReceivedCallback;

  typedef Callback<VOID (_In_ CHttpServer *lpHttp, _In_ CClientRequest *lpRequest)> OnRequestCompletedCallback;

  typedef Callback<HRESULT (_In_ CHttpServer *lpHttp, _In_ CClientRequest *lpRequest, _In_ int nVersion,
                            _In_opt_ LPCSTR *szProtocolsA, _In_ SIZE_T nProtocolsCount, _Out_ int &nSelectedProtocol,
                            _In_ TArrayList<int> &aSupportedVersions,
                            _Outptr_result_maybenull_ CWebSocket **lplpWebSocket)> OnWebSocketRequestReceivedCallback;

  typedef Callback<VOID(_In_ CHttpServer *lpHttp, _In_ CClientRequest *lpRequest)> OnRequestDestroyedCallback;

  typedef Callback<HRESULT (_In_ CHttpServer *lpHttp, _Inout_ CSecureStringA &cStrBodyA, _In_ LONG nStatusCode,
                            _In_ LPCSTR szStatusMessageA,
                            _In_z_ LPCSTR szAdditionalExplanationA)> OnCustomErrorPageCallback;

  //--------

public:
  CHttpServer(_In_ CSockets &cSocketMgr, _In_opt_ CLoggable *lpLogParent = NULL);
  ~CHttpServer();

  VOID SetOption_MaxConnectionsPerIp(_In_ DWORD dwLimit);
  VOID SetOption_RequestHeaderTimeout(_In_ DWORD dwTimeoutMs);
  VOID SetOption_GracefulTerminationTimeout(_In_ DWORD dwTimeoutMs);
  VOID SetOption_KeepAliveTimeout(_In_ DWORD dwTimeoutMs);
  VOID SetOption_RequestBodyLimits(_In_ float nMinimumThroughputInKbps, _In_ DWORD dwSecondsOfLowThroughput);
  VOID SetOption_ResponseLimits(_In_ float nMinimumThroughputInKbps, _In_ DWORD dwSecondsOfLowThroughput);
  VOID SetOption_MaxHeaderSize(_In_ DWORD dwSize);
  VOID SetOption_MaxFieldSize(_In_ DWORD dwSize);
  VOID SetOption_MaxFileSize(_In_ ULONGLONG ullSize);
  VOID SetOption_MaxFilesCount(_In_ DWORD dwCount);
  BOOL SetOption_TemporaryFolder(_In_opt_z_ LPCWSTR szFolderW);
  VOID SetOption_MaxBodySizeInMemory(_In_ DWORD dwSize);
  VOID SetOption_MaxBodySize(_In_ ULONGLONG ullSize);
  VOID SetOption_MaxIncomingBytesWhileSending(_In_ DWORD dwMaxIncomingBytesWhileSending);
  VOID SetOption_MaxRequestsPerSecond(_In_ DWORD dwMaxRequestsPerSecond, _In_ DWORD dwBurstSize);

  VOID SetQuerySslCertificatesCallback(_In_ OnQuerySslCertificatesCallback cQuerySslCertificatesCallback);
  VOID SetNewRequestObjectCallback(_In_ OnNewRequestObjectCallback cNewRequestObjectCallback);
  VOID SetRequestHeadersReceivedCallback(_In_ OnRequestHeadersReceivedCallback cRequestHeadersReceivedCallback);
  VOID SetRequestCompletedCallback(_In_ OnRequestCompletedCallback cRequestCompletedCallback);
  VOID SetWebSocketRequestReceivedCallback(_In_ OnWebSocketRequestReceivedCallback cWebSocketRequestReceivedCallback);
  VOID SetRequestDestroyedCallback(_In_ OnRequestDestroyedCallback cRequestDestroyedCallback);
  VOID SetCustomErrorPageCallback(_In_ OnCustomErrorPageCallback cCustomErrorPageCallback);

  HRESULT StartListening(_In_ CSockets::eFamily nFamily, _In_ int nPort,
                         _In_opt_ CSockets::CListenerOptions *lpOptions = NULL);
  HRESULT StartListening(_In_opt_z_ LPCSTR szBindAddressA, _In_ CSockets::eFamily nFamily, _In_ int nPort,
                         _In_opt_ CSockets::CListenerOptions *lpOptions = NULL);
  HRESULT StartListening(_In_opt_z_ LPCWSTR szBindAddressW, _In_ CSockets::eFamily nFamily, _In_ int nPort,
                         _In_opt_ CSockets::CListenerOptions *lpOptions = NULL);
  VOID StopListening();

public:
  class CClientRequest : public CIpc::CUserData
  {
  protected:
    CClientRequest();
  public:
    ~CClientRequest();

    VOID End(_In_opt_ HRESULT hrErrorCode = S_OK);

    BOOL IsAlive() const;

    BOOL MustAbort() const;
    HANDLE GetAbortEvent() const;

    HRESULT EnableDirectResponse();

    LPCSTR GetMethod() const;
    CUrl* GetUrl() const;

    CHttpServer* GetHttpServer() const
      {
      return lpHttpServer;
      };

    CHttpBodyParserBase* GetRequestBodyParser() const;

    SIZE_T GetRequestHeadersCount() const;
    CHttpHeaderBase* GetRequestHeader(_In_ SIZE_T nIndex) const;
    CHttpHeaderBase* GetRequestHeaderByName(_In_z_ LPCSTR szNameA) const;
    template<class T>
    T* GetRequestHeader() const
      {
      return reinterpret_cast<T*>(GetRequestHeaderByName(T::GetHeaderNameStatic()));
      };

    SIZE_T GetRequestCookiesCount() const;
    CHttpCookie* GetRequestCookie(_In_ SIZE_T nIndex) const;
    CHttpCookie* GetRequestCookieByName(_In_z_ LPCSTR szNameA) const;
    CHttpCookie* GetRequestCookieByName(_In_z_ LPCWSTR szNameW) const;

    HRESULT ResetResponse();

    HRESULT SetResponseStatus(_In_ LONG nStatusCode, _In_opt_ LPCSTR szReasonA = NULL);

    HRESULT AddResponseHeader(_In_ CHttpHeaderBase *lpHeader);
    template<class T>
    HRESULT AddResponseHeader(_Out_ T **lplpHeader = NULL)
      {
      return AddResponseHeader(T::GetHeaderNameStatic(), reinterpret_cast<CHttpHeaderBase**>(lplpHeader));
      };
    HRESULT AddResponseHeader(_In_z_ LPCSTR szNameA, _Deref_opt_out_ CHttpHeaderBase **lplpHeader,
                             _In_opt_ BOOL bReplaceExisting = TRUE);
    HRESULT AddResponseHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1,
                              _Out_opt_ CHttpHeaderBase **lplpHeader = NULL, _In_opt_ BOOL bReplaceExisting = TRUE);
    HRESULT AddResponseHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1,
                              _Out_opt_ CHttpHeaderBase **lplpHeader = NULL, _In_opt_ BOOL bReplaceExisting = TRUE);
    HRESULT RemoveResponseHeader(_In_z_ LPCSTR szNameA);
    HRESULT RemoveAllResponseHeaders();

    SIZE_T GetResponseHeadersCount() const;
    CHttpHeaderBase* GetResponseHeader(_In_ SIZE_T nIndex) const;
    CHttpHeaderBase* GetResponseHeaderByName(_In_z_ LPCSTR szNameA) const;
    template<class T>
    T* GetResponseHeader() const
      {
      return reinterpret_cast<T*>(GetResponseHeaderByName(T::GetHeaderNameStatic()));
      };

    HRESULT AddResponseCookie(_In_ CHttpCookie *lpCookie);
    HRESULT AddResponseCookie(_In_ CHttpCookieArray &cSrc, _In_opt_ BOOL bReplaceExisting = FALSE);
    HRESULT AddResponseCookie(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA, _In_opt_z_ LPCSTR szDomainA = NULL,
                              _In_opt_z_ LPCSTR szPathA = NULL, _In_opt_ const CDateTime *lpDate = NULL,
                              _In_opt_ BOOL bIsSecure = FALSE, _In_opt_ BOOL bIsHttpOnly = FALSE,
                              _In_opt_ CHttpCookie::eSameSite nSameSite = CHttpCookie::eSameSite::None);
    HRESULT AddResponseCookie(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szValueW, _In_opt_z_ LPCWSTR szDomainW = NULL,
                              _In_opt_z_ LPCWSTR szPathW = NULL, _In_opt_ const CDateTime *lpDate = NULL,
                              _In_opt_ BOOL bIsSecure = FALSE, _In_opt_ BOOL bIsHttpOnly = FALSE,
                              _In_opt_ CHttpCookie::eSameSite nSameSite = CHttpCookie::eSameSite::None);
    HRESULT RemoveResponseCookie(_In_z_ LPCSTR szNameA);
    HRESULT RemoveResponseCookie(_In_z_ LPCWSTR szNameW);
    HRESULT RemoveAllResponseCookies();

    SIZE_T GetResponseCookiesCount() const;
    CHttpCookie* GetResponseCookie(_In_ SIZE_T nIndex) const;
    CHttpCookie* GetResponseCookieByName(_In_z_ LPCSTR szNameA) const;
    CHttpCookie* GetResponseCookieByName(_In_z_ LPCWSTR szNameW) const;

    Http::eBrowser GetBrowser() const
      {
      return cRequestParser.GetRequestBrowser();
      };

    HRESULT SendResponse(_In_ LPCVOID lpData, _In_ SIZE_T nDataLen);
    HRESULT SendFile(_In_z_ LPCWSTR szFileNameW);
    HRESULT SendStream(_In_ CStream *lpStream);

    HRESULT SetMimeTypeFromFileName(_In_opt_z_ LPCWSTR szFileNameW = NULL);
    HRESULT SetFileName(_In_opt_z_ LPCWSTR szFileNameW = NULL, _In_opt_ BOOL bInline = FALSE);

    HRESULT SendErrorPage(_In_ LONG nStatusCode, _In_ HRESULT hrErrorCode,
                          _In_opt_z_ LPCSTR szAdditionalExplanationA = NULL);

    HRESULT DisableClientCache();

    VOID IgnoreKeepAlive();

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

  protected:
    enum class eState
    {
      Closed = 0,
      Inactive,
      ReceivingRequestHeaders,
      AfterHeaders,
      ReceivingRequestBody,
      BuildingResponse,
      SendingResponse,
      NegotiatingWebSocket,
      WebSocket,
      GracefulTermination,
      Terminated,
      KeepingAlive,
      LingerClose
    };

  protected:
    eState GetState() const
      {
      return nState;
      };

  private:
    friend class CHttpServer;

    enum class eTimeoutTimer
    {
      Headers = 0x01,
      Throughput = 0x02,
      GracefulTermination = 0x04,
      KeepAlive = 0x08,

      All = Headers | Throughput | GracefulTermination | KeepAlive
    };

    friend inline eTimeoutTimer operator|(eTimeoutTimer lhs, eTimeoutTimer rhs);
    friend inline eTimeoutTimer operator&(eTimeoutTimer lhs, eTimeoutTimer rhs);
    friend inline eTimeoutTimer operator~(eTimeoutTimer v);

  private:
    HRESULT Initialize(_In_ CHttpServer *lpHttpServer, _In_ CSockets *lpSocketMgr, _In_ HANDLE hConn,
                       _In_ DWORD dwMaxHeaderSize);

    HRESULT ResetForNewRequest();

    HRESULT SetState(_In_ eState nNewState);

    BOOL IsKeepAliveRequest() const;
    BOOL HasErrorBeenSent() const;
    BOOL HasHeadersBeenSent() const;

    HRESULT SendHeaders();
    HRESULT SendQueuedStreams();

    VOID MarkLinkAsClosed();
    BOOL IsLinkClosed() const;

    VOID ResetResponseForNewRequest(_In_ BOOL bPreserveWebSocket);

    HRESULT StartTimeoutTimers(_In_ eTimeoutTimer nTimers);
    VOID StopTimeoutTimers(_In_ eTimeoutTimer nTimers);

    static LPCWSTR GetNamedState(_In_ eState nState);

  private:
    CLnkLstNode cListNode;
    LONG volatile nTimerCallbackRundownLock{ MX_RUNDOWNPROT_INIT };
    CCriticalSection cMutex;
    CHttpServer *lpHttpServer{ NULL };
    CSockets *lpSocketMgr{ NULL };
    HANDLE hConn{ NULL };
    SOCKADDR_INET sPeerAddr{ 0 };
    eState nState{ eState::Inactive };
    LONG volatile nFlags{ 0 };
    HRESULT hrErrorCode = { S_OK };
    LONG volatile nHeadersTimeoutTimerId{ 0 };
    LONG volatile nThroughputTimerId{ 0 };
    DWORD dwLowThroughputCounter{ 0 };
    LONG volatile nGracefulTerminationTimerId{ 0 };
    LONG volatile nKeepAliveTimeoutTimerId{ 0 };
    Internals::CHttpParser cRequestParser{ TRUE, NULL };
    struct {
      LONG nStatus{ 0 };
      CStringA cStrReasonA;
      CHttpHeaderArray cHeaders;
      CHttpCookieArray cCookies;
      TArrayListWithRelease<CStream*> aStreamsList;
      BOOL bLastStreamIsData{ FALSE };
      LPCSTR szMimeTypeHintA{ NULL };
      CStringW cStrFileNameW;
      BOOL bIsInline{ FALSE };
      BOOL bDirect{ FALSE }, bPreserveWebSocketHeaders{ FALSE };
    } sResponse;
  };

private:
  friend class CClientRequest;

  typedef struct _WEBSOCKET_REQUEST_DATA {
    int nVersion{ 0 };
    TArrayListWithFree<LPCSTR> aProtocols;
    TAutoRefCounted<CHttpHeaderRespSecWebSocketAccept> cHeaderRespSecWebSocketAccept;
  } WEBSOCKET_REQUEST_DATA;

private:
  VOID OnHeadersTimeoutTimerCallback(_In_ LONG nTimerId, _In_ LPVOID lpUserData, _In_opt_ LPBOOL lpbCancel);
  VOID OnThroughputTimerCallback(_In_ LONG nTimerId, _In_ LPVOID lpUserData, _In_opt_ LPBOOL lpbCancel);
  VOID OnGracefulTerminationTimerCallback(_In_ LONG nTimerId, _In_ LPVOID lpUserData, _In_opt_ LPBOOL lpbCancel);
  VOID OnKeepAliveTimerCallback(_In_ LONG nTimerId, _In_ LPVOID lpUserData, _In_opt_ LPBOOL lpbCancel);

  HRESULT OnSocketCreate(_In_ CIpc *lpIpc, _In_ HANDLE h, _Inout_ CIpc::CREATE_CALLBACK_DATA &sData);
  VOID OnListenerSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                               _In_ HRESULT hrErrorCode);

  VOID OnSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                       _In_ HRESULT hrErrorCode);
  HRESULT OnSocketConnect(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData);
  HRESULT OnSocketDataReceived(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData);

  BOOL CheckRateLimit();

  VOID TerminateRequest(_In_ CClientRequest *lpRequest, _In_ HRESULT hrErrorCode);
  VOID OnRequestError(_In_ CClientRequest *lpRequest, _In_ HRESULT hrErrorCode,
                      _Inout_ CClientRequest::eTimeoutTimer &nTimersToStart);

  HRESULT FillResponseWithError(_In_ CClientRequest *lpRequest, _In_ LONG nStatusCode, _In_ HRESULT hrErrorCode,
                                _In_opt_z_ LPCSTR szAdditionalExplanationA);

  static LPCSTR GetStatusCodeMessage(_In_ LONG nStatusCode);

  VOID OnAfterSendResponse(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ LPVOID lpCookie, _In_ CIpc::CUserData *lpUserData);

  BOOL IsWebSocket(_In_ CClientRequest *lpRequest);
  HRESULT ValidateWebSocket(_In_ CClientRequest *lpRequest, _Inout_ WEBSOCKET_REQUEST_DATA &sData);
  HRESULT ProcessWebSocket(_In_ CClientRequest *lpRequest, _In_ CWebSocket *lpWebSocket, _In_ int nSelectedProtocol,
                           _In_ TArrayList<int> &aSupportedVersions, _In_ WEBSOCKET_REQUEST_DATA &sData,
                           _Out_ BOOL &bFatal);

  VOID OnRequestEnding(_In_ CClientRequest *lpRequest, _In_ HRESULT hrErrorCode);

  HRESULT OnDownloadStarted(_Out_ LPHANDLE lphFile, _In_z_ LPCWSTR szFileNameW, _In_ LPVOID lpUserParam);

private:
  class CRequestLimiter : public virtual CBaseMemObj
  {
  public:
    CRequestLimiter(_In_ PSOCKADDR_INET lpAddr) : CBaseMemObj()
      {
      return;
      };

    static int InsertCompareFunc(_In_ LPVOID lpContext, _In_ CRedBlackTreeNode *lpNode1,
                                 _In_ CRedBlackTreeNode *lpNode2)
      {
      CRequestLimiter *lpLimiter1 = CONTAINING_RECORD(lpNode1, CRequestLimiter, cTreeNode);
      CRequestLimiter *lpLimiter2 = CONTAINING_RECORD(lpNode2, CRequestLimiter, cTreeNode);
      return ::MxMemCompare(&(lpLimiter1->sAddr), &(lpLimiter2->sAddr), sizeof(SOCKADDR_INET));
      };

    static int SearchCompareFunc(_In_ LPVOID lpContext, _In_ PSOCKADDR_INET key, _In_ CRedBlackTreeNode *lpNode)
      {
      CRequestLimiter *lpLimiter = CONTAINING_RECORD(lpNode, CRequestLimiter, cTreeNode);

      return ::MxMemCompare(key, &(lpLimiter->sAddr), sizeof(SOCKADDR_INET));
      };

  private:
    friend class CHttpServer;

    SOCKADDR_INET sAddr{};
    LONG volatile nCount{ 1 };
    CRedBlackTreeNode cTreeNode;
  };

private:
  CCriticalSection cs;
  CSockets &cSocketMgr;
  DWORD dwMaxConnectionsPerIp{ 0xFFFFFFFFUL };
  DWORD dwRequestHeaderTimeoutMs{ 30000 }, dwGracefulTerminationTimeoutMs{ 30000 }, dwKeepAliveTimeoutMs{ 60000 };
  float nRequestBodyMinimumThroughputInKbps{ 0.25f }, nResponseMinimumThroughputInKbps{ 0.25f };
  DWORD dwRequestBodySecondsOfLowThroughput{ 10 }, dwResponseSecondsOfLowThroughput{ 10 };
  DWORD dwMaxHeaderSize{ 16384 };
  DWORD dwMaxFieldSize{ 256000 };
  ULONGLONG ullMaxFileSize{ 2097152ui64 };
  DWORD dwMaxFilesCount{ 4 };
  CStringW cStrTemporaryFolderW;
  DWORD dwMaxBodySizeInMemory{ 32768 };
  ULONGLONG ullMaxBodySize{ 10485760ui64 };
  DWORD dwMaxIncomingBytesWhileSending{ 52428800 };
  DWORD dwMaxRequestsPerSecond{ 0 };
  DWORD dwMaxRequestsBurstSize{ 0 };
  struct {
    LONG volatile nMutex{ MX_RUNDOWNPROT_INIT };
    CTimer cTimer;
    union {
      DWORD dwRequestCounter{ 0 };
      DWORD dwCurrentExcess;
    };
  } sLimiter;

  LONG volatile nDownloadNameGeneratorCounter{ 0 };
  LONG volatile nRundownLock{ MX_RUNDOWNPROT_INIT };
  HANDLE hAcceptConn{ NULL };
  LONG volatile nActiveRequestsCount{ 0 };

  OnQuerySslCertificatesCallback cQuerySslCertificatesCallback;
  OnNewRequestObjectCallback cNewRequestObjectCallback;
  OnRequestHeadersReceivedCallback cRequestHeadersReceivedCallback;
  OnRequestCompletedCallback cRequestCompletedCallback;
  OnWebSocketRequestReceivedCallback cWebSocketRequestReceivedCallback;
  OnRequestDestroyedCallback cRequestDestroyedCallback;
  OnCustomErrorPageCallback cCustomErrorPageCallback;
  RWLOCK sRequestsListRwMutex{};
  CLnkLst cRequestsList;
  CWindowsEvent cShutdownEv;

  struct {
    RWLOCK sRwMutex{};
    CRedBlackTree cTree;
  } sRequestLimiter;
};


inline CHttpServer::CClientRequest::eTimeoutTimer operator|(CHttpServer::CClientRequest::eTimeoutTimer lhs, CHttpServer::CClientRequest::eTimeoutTimer rhs)
{
  return static_cast<CHttpServer::CClientRequest::eTimeoutTimer>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

inline CHttpServer::CClientRequest::eTimeoutTimer operator&(CHttpServer::CClientRequest::eTimeoutTimer lhs, CHttpServer::CClientRequest::eTimeoutTimer rhs)
{
  return static_cast<CHttpServer::CClientRequest::eTimeoutTimer>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

inline CHttpServer::CClientRequest::eTimeoutTimer operator~(CHttpServer::CClientRequest::eTimeoutTimer v)
{
  return static_cast<CHttpServer::CClientRequest::eTimeoutTimer>(~static_cast<int>(v));
}

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPSERVER_H
