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
#ifndef _MX_HTTPCLIENT_H
#define _MX_HTTPCLIENT_H

#include "..\Defines.h"
#include "..\Callbacks.h"
#include "..\Comm\Sockets.h"
#include "HttpCommon.h"
#include "HttpCookie.h"
#include "HttpBodyParserDefault.h"
#include "HttpBodyParserMultipartFormData.h"
#include "HttpBodyParserUrlEncodedForm.h"
#include "HttpBodyParserIgnore.h"
#include "WebSockets.h"
#include "..\Comm\Proxy.h"

//-----------------------------------------------------------

namespace MX {

class CHttpClient : public virtual TRefCounted<CBaseMemObj>, public CLoggable, public CNonCopyableObj
{
public:
  typedef enum {
    StateClosed = 0,
    StateSendingRequestHeaders,
    StateSendingDynamicRequestBody,
    StateReceivingResponseHeaders,
    StateAfterHeaders,
    StateReceivingResponseBody,
    StateDocumentCompleted,
    StateWaitingForRedirection,
    StateEstablishingProxyTunnelConnection,
    StateWaitingProxyTunnelConnectionResponse,
    StateNoOp
  } eState;

public:
  typedef struct {
    struct {
      CWebSocket *lpSocket;
      int nVersion;
      LPCSTR *lpszProtocolsA;
    } sWebSocket;
    struct {
      LPCSTR szHeaderNameA;
    } sSendLocalIP;
  } OPEN_OPTIONS, *LPOPEN_OPTIONS;

public:
  //NOTE: Leave cStrFullFileNameW empty to download to temp location (with imposed limitations)
  typedef Callback<HRESULT (_In_ CHttpClient *lpHttp, _In_z_ LPCWSTR szFileNameW, _In_opt_ PULONGLONG lpnContentSize,
                            _In_z_ LPCSTR szTypeA, _In_ BOOL bTreatAsAttachment, _Inout_ CStringW &cStrFullFileNameW,
                            _Outptr_result_maybenull_ CHttpBodyParserBase **lpBodyParser)> OnHeadersReceivedCallback;
  typedef Callback<VOID (_In_ CHttpClient *lpHttp)> OnDocumentCompletedCallback;

  typedef Callback<HRESULT (_In_ CHttpClient *lpHttp, _In_ CWebSocket *lpWebSocket,
                            _In_z_ LPCSTR szProtocolA)> OnWebSocketHandshakeCompletedCallback;

  typedef Callback<VOID (_In_ CHttpClient *lpHttp)> OnDymanicRequestBodyStartCallback;

  typedef Callback<HRESULT (_In_ CHttpClient *lpHttp, _Outptr_result_maybenull_ CSslCertificateArray **lplpCheckCerts,
                            _Outptr_result_maybenull_ CSslCertificate **lplpSelfCert,
                            _Outptr_result_maybenull_ CEncryptionKey **lplpPrivKey)> OnQueryCertificatesCallback;

  typedef Callback<HRESULT (_In_ CIpc *lpIpc, _In_ HANDLE h)> OnConnectionCreatedCallback;
  typedef Callback<HRESULT (_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ HRESULT hrErrorCode)> OnConnectionDestroyedCallback;

  //--------

public:
  CHttpClient(_In_ CSockets &cSocketMgr, _In_opt_ CLoggable *lpLogParent = NULL);
  ~CHttpClient();

  VOID SetOption_Timeout(_In_ DWORD dwTimeoutMs);
  VOID SetOption_MaxRedirectionsCount(_In_ DWORD dwCount);
  VOID SetOption_MaxHeaderSize(_In_ DWORD dwSize);
  VOID SetOption_MaxFieldSize(_In_ DWORD dwSize);
  VOID SetOption_MaxFileSize(_In_ ULONGLONG ullSize);
  VOID SetOption_MaxFilesCount(_In_ DWORD dwCount);
  BOOL SetOption_TemporaryFolder(_In_opt_z_ LPCWSTR szFolderW);
  VOID SetOption_MaxBodySizeInMemory(_In_ DWORD dwSize);
  VOID SetOption_MaxBodySize(_In_ ULONGLONG ullSize);
  VOID SetOption_MaxRawRequestBodySizeInMemory(_In_ DWORD dwSize);
  VOID SetOption_KeepConnectionOpen(_In_ BOOL bKeep);
  VOID SetOption_AcceptCompressedContent(_In_ BOOL bAccept);

  VOID SetHeadersReceivedCallback(_In_ OnHeadersReceivedCallback cHeadersReceivedCallback);
  VOID SetDocumentCompletedCallback(_In_ OnDocumentCompletedCallback cDocumentCompletedCallback);
  VOID SetWebSocketHandshakeCompletedCallback(_In_ OnWebSocketHandshakeCompletedCallback
                                              cWebSocketHandshakeCompletedCallback);
  VOID SetDymanicRequestBodyStartCallback(_In_ OnDymanicRequestBodyStartCallback cDymanicRequestBodyStartCallback);
  VOID SetQueryCertificatesCallback(_In_ OnQueryCertificatesCallback cQueryCertificatesCallback);
  VOID SetConnectionCreatedCallback(_In_ OnConnectionCreatedCallback cConnectionCreatedCallback);
  VOID SetConnectionDestroyedCallback(_In_ OnConnectionDestroyedCallback cConnectionDestroyedCallback);

  HRESULT SetRequestMethodAuto();
  HRESULT SetRequestMethod(_In_z_ LPCSTR szMethodA);
  HRESULT SetRequestMethod(_In_z_ LPCWSTR szMethodW);

  HRESULT AddRequestHeader(_In_ CHttpHeaderBase *lpHeader);
  template<class T>
  HRESULT AddRequestHeader(_Out_ T **lplpHeader = NULL)
    {
    return AddRequestHeader(T::GetHeaderNameStatic(), reinterpret_cast<CHttpHeaderBase**>(lplpHeader));
    };
  HRESULT AddRequestHeader(_In_z_ LPCSTR szNameA, _Out_opt_ CHttpHeaderBase **lplpHeader,
                           _In_opt_ BOOL bReplaceExisting = TRUE);
  HRESULT AddRequestHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1,
                           _Out_opt_ CHttpHeaderBase **lplpHeader = NULL, _In_opt_ BOOL bReplaceExisting = TRUE);
  HRESULT AddRequestHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW, _In_opt_ SIZE_T nValueLen = (SIZE_T)-1,
                           _Out_opt_ CHttpHeaderBase **lplpHeader = NULL, _In_opt_ BOOL bReplaceExisting = TRUE);
  HRESULT RemoveRequestHeader(_In_z_ LPCSTR szNameA);
  HRESULT RemoveAllRequestHeaders();

  SIZE_T GetRequestHeadersCount() const;
  CHttpHeaderBase *GetRequestHeader(_In_ SIZE_T nIndex) const;
  CHttpHeaderBase *GetRequestHeaderByName(_In_z_ LPCSTR szNameA) const;
  template<class T>
  T* GetRequestHeader() const
    {
    return reinterpret_cast<T *>(GetRequestHeaderByName(T::GetHeaderNameStatic()));
    };

  HRESULT AddRequestCookie(_In_ CHttpCookie *lpCookie);
  HRESULT AddRequestCookie(_In_ CHttpCookieArray &cSrc, _In_opt_ BOOL bReplaceExisting = FALSE);
  HRESULT AddRequestCookie(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA);
  HRESULT AddRequestCookie(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szValueW);
  HRESULT RemoveRequestCookie(_In_z_ LPCSTR szNameA);
  HRESULT RemoveRequestCookie(_In_z_ LPCWSTR szNameW);
  HRESULT RemoveAllRequestCookies();

  SIZE_T GetRequestCookiesCount() const;
  CHttpCookie* GetRequestCookie(_In_ SIZE_T nIndex) const;
  CHttpCookie* GetRequestCookieByName(_In_z_ LPCSTR szNameA) const;
  CHttpCookie* GetRequestCookieByName(_In_z_ LPCWSTR szNameW) const;

  HRESULT AddRequestPostData(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA);
  HRESULT AddRequestPostData(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szValueW);
  HRESULT AddRequestPostDataFile(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szFileNameA, _In_ CStream *lpStream);
  HRESULT AddRequestPostDataFile(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szFileNameW, _In_ CStream *lpStream);
  HRESULT AddRequestRawPostData(_In_ LPCVOID lpData, _In_ SIZE_T nLength);
  HRESULT AddRequestRawPostData(_In_ CStream *lpStream);
  HRESULT RemoveRequestPostData(_In_opt_z_ LPCSTR szNameA);
  HRESULT RemoveRequestPostData(_In_opt_z_ LPCWSTR szNameW);
  HRESULT RemoveAllRequestPostData();
  HRESULT EnableDynamicPostData();
  HRESULT SignalEndOfPostData();

  HRESULT SetProxy(_In_ CProxy &cProxy);

  HRESULT Open(_In_ CUrl &cUrl, _In_opt_ LPOPEN_OPTIONS lpOptions = NULL);
  HRESULT Open(_In_z_ LPCSTR szUrlA, _In_opt_ LPOPEN_OPTIONS lpOptions = NULL);
  HRESULT Open(_In_z_ LPCWSTR szUrlW, _In_opt_ LPOPEN_OPTIONS lpOptions = NULL);
  VOID Close(_In_opt_ BOOL bReuseConn = TRUE);

  HRESULT GetLastRequestError() const;

  HRESULT IsResponseHeaderComplete() const;
  BOOL IsDocumentComplete() const;
  BOOL IsClosed() const;

  LONG GetResponseStatus() const;
  HRESULT GetResponseReason(_Inout_ CStringA &cStrDestA) const;

  CHttpBodyParserBase* GetResponseBodyParser() const;

  SIZE_T GetResponseHeadersCount() const;
  CHttpHeaderBase* GetResponseHeader(_In_ SIZE_T nIndex) const;
  CHttpHeaderBase* GetResponseHeaderByName(_In_z_ LPCSTR szNameA) const;
  template<class T>
  T* GetResponseHeader() const
    {
    return reinterpret_cast<T*>(GetResponseHeaderByName(T::GetHeaderNameStatic()));
    };

  SIZE_T GetResponseCookiesCount() const;
  CHttpCookie* GetResponseCookie(_In_ SIZE_T nIndex) const;
  CHttpCookie* GetResponseCookieByName(_In_z_ LPCSTR szNameA) const;
  CHttpCookie* GetResponseCookieByName(_In_z_ LPCWSTR szNameW) const;

  HANDLE GetUnderlyingSocket() const;
  CSockets* GetUnderlyingSocketManager() const;

  LPCSTR GetRequestBoundary() const;

private:
  class CConnection : public CIpc::CUserData
  {
  public:
    CConnection(_In_ CHttpClient *lpHttpClient);
    ~CConnection();

    HANDLE GetConn() const
      {
      return hConn;
      };

    BOOL IsClosed() const;

    HRESULT Connect(_In_ CSockets::eFamily nFamily, _In_z_ LPCWSTR szAddressW, _In_ int nPort, _In_ BOOL bUseSSL);
    VOID Close(_In_ HRESULT hrErrorCode);

    VOID Detach();

    HRESULT SendMsg(_In_reads_bytes_(nMsgSize) LPCVOID lpMsg, _In_ SIZE_T nMsgSize);
    HRESULT SendStream(_In_ CStream *lpStream);
    HRESULT SendAfterSendRequestHeaders();

    HRESULT GetBufferedMessage(_Out_ LPVOID lpMsg, _Inout_ SIZE_T *lpnMsgSize);
    HRESULT ConsumeBufferedMessage(_In_ SIZE_T nConsumedBytes);

  private:
    friend class CHttpClient;

    CHttpClient* GetHttpClient();

    HRESULT OnSocketCreate(_In_ CIpc *lpIpc, _In_ HANDLE h, _Inout_ CIpc::CREATE_CALLBACK_DATA &sData);
    VOID OnSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                         _In_ HRESULT hrErrorCode);
    HRESULT OnSocketConnect(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_opt_ CIpc::CUserData *lpUserData);
    HRESULT OnSocketDataReceived(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData);

    VOID OnAfterSendRequestHeaders(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ LPVOID lpCookie,
                                   _In_ CIpc::CUserData *lpUserData);

  private:
    CIpc *lpIpc;
    HANDLE hConn;
    BOOL bUseSSL;
    RWLOCK sRwMutex;
    CHttpClient *lpHttpClient;
  };

private:
  friend class CConnection;

  HRESULT InternalOpen(_In_ CUrl &cUrl, _In_opt_ LPOPEN_OPTIONS lpOptions, _In_ BOOL bIsRedirecting,
                       _Outptr_ _Maybenull_ CConnection **lplpConnectionToRelease);

  VOID OnConnectionClosed(_In_ CConnection *lpConn, _In_ HRESULT hrErrorCode);
  HRESULT OnConnectionEstablished(_In_ CConnection *lpConn);
  HRESULT OnDataReceived(_In_ CConnection *lpConn);
  HRESULT OnAddSslLayer(_In_ CIpc *lpIpc, _In_ HANDLE h);

  VOID OnRedirection(_In_ LONG nTimerId, _In_ LPVOID lpUserData, _In_opt_ LPBOOL lpbCancel);
  VOID OnAfterSendRequestHeaders(_In_ CConnection *lpConn);

  VOID SetErrorOnRequestAndClose(_In_ HRESULT hrErrorCode);

  HRESULT BuildRequestHeaders(_Inout_ CStringA &cStrReqHdrsA);
  HRESULT BuildRequestHeaderAdd(_Inout_ CStringA &cStrReqHdrsA, _In_z_ LPCSTR szNameA, _In_z_ LPCSTR szDefaultValueA,
                                _In_ MX::Http::eBrowser nBrowser);
  HRESULT AddRequestHeadersForBody(_Inout_ CStringA &cStrReqHdrsA);

  HRESULT SendTunnelConnect(_In_ CConnection *lpConnection);
  HRESULT SendRequestHeaders(_In_ CConnection *lpConnection);
  HRESULT SendRequestBody(_In_ CConnection *lpConnection);

  VOID GenerateRequestBoundary();

  HRESULT OnDownloadStarted(_Out_ LPHANDLE lphFile, _In_z_ LPCWSTR szFileNameW, _In_ LPVOID lpUserParam);

  VOID ResetRequestForNewRequest();
  VOID ResetResponseForNewRequest();

  HRESULT SetupIgnoreBody();

  CConnection* GetConnection();

  BOOL LockThreadCall(_In_ BOOL bAllowRecursive);

  VOID OnRequestTimeout(_In_ LONG nTimerId, _In_ LPVOID lpUserData, _In_opt_ LPBOOL lpbCancel);

private:
  class CPostDataItem : public virtual CBaseMemObj, public CNonCopyableObj
  {
  public:
    CPostDataItem(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA);
    CPostDataItem(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szFileNameA, _In_ CStream *lpStream);
    CPostDataItem(_In_ CStream *lpStream);
    ~CPostDataItem();

  private:
    friend class CHttpClient;

    CLnkLstNode cListNode;
    LPSTR szNameA, szValueA;
    SIZE_T nValueLen;
    TAutoRefCounted<CStream> cStream;
  };

private:
  CCriticalSection cMutex;
  CSockets &cSocketMgr;
  eState nState;
  struct {
    RWLOCK sRwMutex;
    TAutoRefCounted<CConnection> cLink;
  } sConnection;
  CProxy cProxy;
  HRESULT hLastErrorCode;
  DWORD dwTimeoutMs;
  DWORD dwMaxRedirCount;
  DWORD dwMaxFieldSize;
  ULONGLONG ullMaxFileSize;
  DWORD dwMaxFilesCount;
  CStringW cStrTemporaryFolderW;
  DWORD dwMaxBodySizeInMemory;
  ULONGLONG ullMaxBodySize;
  DWORD dwMaxRawRequestBodySizeInMemory;
  BOOL bKeepConnectionOpen;
  BOOL bAcceptCompressedContent;

  LONG volatile nCallInProgressThread;
  OnHeadersReceivedCallback cHeadersReceivedCallback;
  OnDymanicRequestBodyStartCallback cDymanicRequestBodyStartCallback;
  OnDocumentCompletedCallback cDocumentCompletedCallback;
  OnWebSocketHandshakeCompletedCallback cWebSocketHandshakeCompletedCallback;
  OnQueryCertificatesCallback cQueryCertificatesCallback;
  OnConnectionCreatedCallback cConnectionCreatedCallback;
  OnConnectionDestroyedCallback cConnectionDestroyedCallback;

  struct {
    CUrl cUrl;
    CStringA cStrMethodA;
    CHttpHeaderArray cHeaders;
    CHttpCookieArray cCookies;
    struct {
      CLnkLst cList;
      BOOL bHasRaw;
      LONG nDynamicFlags;
    } sPostData;
    CHAR szBoundaryA[32];
    BOOL bUsingMultiPartFormData;
    BOOL bUsingProxy;
    TAutoRefCounted<CWebSocket> cWebSocket;
    TAutoRefCounted<CHttpHeaderGeneric> cLocalIpHeader;
    LONG volatile nTimeoutTimerId;
  } sRequest;

  struct {
    Internals::CHttpParser cParser{ FALSE, NULL };
    CStringW cStrDownloadFileNameW;
    TAutoFreePtr<BYTE> aMsgBuf;
  } sResponse;

  struct {
    CUrl cUrl;
    DWORD dwCounter;
    LONG volatile nTimerId;
  } sRedirection;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPCLIENT_H
