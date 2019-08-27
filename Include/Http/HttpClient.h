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
#include "..\Comm\IpcSslLayer.h"
#include "HttpCommon.h"
#include "HttpCookie.h"
#include "HttpBodyParserDefault.h"
#include "HttpBodyParserMultipartFormData.h"
#include "HttpBodyParserUrlEncodedForm.h"
#include "HttpBodyParserIgnore.h"
#include "..\Comm\Proxy.h"

//-----------------------------------------------------------

namespace MX {

class CHttpClient : public virtual CBaseMemObj, public CLoggable
{
  MX_DISABLE_COPY_CONSTRUCTOR(CHttpClient);
public:
  typedef enum {
    StateClosed = 0,
    StateSendingRequestHeaders,
    StateSendingDynamicRequestBody,
    StateReceivingResponseHeaders,
    StateReceivingResponseBody,
    StateDocumentCompleted,
    StateWaitingForRedirection,
    StateRetryingAuthentication,
    StateEstablishingProxyTunnelConnection,
    StateWaitingProxyTunnelConnectionResponse,
    StateNoOp
  } eState;

  //--------

  //NOTE: Leave cStrFullFileNameW empty to download to temp location (with imposed limitations)
  typedef Callback<HRESULT (_In_ CHttpClient *lpHttp, _In_z_ LPCWSTR szFileNameW, _In_opt_ PULONGLONG lpnContentSize,
                            _In_z_ LPCSTR szTypeA, _In_ BOOL bTreatAsAttachment, _Inout_ CStringW &cStrFullFileNameW,
                            _Inout_ CHttpBodyParserBase **lpBodyParser)> OnHeadersReceivedCallback;
  typedef Callback<HRESULT (_In_ CHttpClient *lpHttp)> OnDocumentCompletedCallback;

  typedef Callback<VOID (_In_ CHttpClient *lpHttp)> OnDymanicRequestBodyStartCallback;

  typedef Callback<VOID (_In_ CHttpClient *lpHttp, _In_ HRESULT hrErrorCode)> OnErrorCallback;

  typedef Callback<HRESULT (_In_ CHttpClient *lpHttp, _Inout_ CIpcSslLayer::eProtocol &nProtocol,
                            _Inout_ CSslCertificateArray **lplpCheckCertificates, _Inout_ CSslCertificate **lplpSelfCert,
                            _Inout_ CCryptoRSA **lplpPrivKey)> OnQueryCertificatesCallback;

  //--------

public:
  CHttpClient(_In_ CSockets &cSocketMgr, _In_opt_ CLoggable *lpLogParent = NULL);
  ~CHttpClient();

  VOID SetOption_ResponseHeaderTimeout(_In_ DWORD dwTimeoutMs);
  VOID SetOption_ResponseBodyLimits(_In_ DWORD dwMinimumThroughputInBps, _In_ DWORD dwSecondsOfLowThroughput);
  VOID SetOption_MaxRedirectionsCount(_In_ DWORD dwCount);
  VOID SetOption_MaxHeaderSize(_In_ DWORD dwSize);
  VOID SetOption_MaxFieldSize(_In_ DWORD dwSize);
  VOID SetOption_MaxFileSize(_In_ ULONGLONG ullSize);
  VOID SetOption_MaxFilesCount(_In_ DWORD dwCount);
  BOOL SetOption_TemporaryFolder(_In_opt_z_ LPCWSTR szFolderW);
  VOID SetOption_MaxBodySizeInMemory(_In_ DWORD dwSize);
  VOID SetOption_MaxBodySize(_In_ ULONGLONG ullSize);
  VOID SetOption_KeepConnectionOpen(_In_ BOOL bKeep);
  VOID SetOption_AcceptCompressedContent(_In_ BOOL bAccept);

  VOID SetHeadersReceivedCallback(_In_ OnHeadersReceivedCallback cHeadersReceivedCallback);
  VOID SetDocumentCompletedCallback(_In_ OnDocumentCompletedCallback cDocumentCompletedCallback);
  VOID SetDymanicRequestBodyStartCallback(_In_ OnDymanicRequestBodyStartCallback cDymanicRequestBodyStartCallback);
  VOID SetErrorCallback(_In_ OnErrorCallback cErrorCallback);
  VOID SetQueryCertificatesCallback(_In_ OnQueryCertificatesCallback cQueryCertificatesCallback);

  HRESULT SetRequestMethodAuto();
  HRESULT SetRequestMethod(_In_z_ LPCSTR szMethodA);
  HRESULT SetRequestMethod(_In_z_ LPCWSTR szMethodW);

  template<class T>
  HRESULT AddRequestHeader(_Out_ T **lplpHeader=NULL)
    {
    return AddRequestHeader(T::GetHeaderNameStatic(), reinterpret_cast<CHttpHeaderBase**>(lplpHeader));
    };
  HRESULT AddRequestHeader(_In_z_ LPCSTR szNameA, _Out_ CHttpHeaderBase **lplpHeader, _In_ BOOL bReplaceExisting=TRUE);
  HRESULT AddRequestHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA, _Out_opt_ CHttpHeaderBase **lplpHeader=NULL,
                           _In_ BOOL bReplaceExisting=TRUE);
  HRESULT AddRequestHeader(_In_z_ LPCSTR szNameA, _In_z_ LPCWSTR szValueW, _Out_opt_ CHttpHeaderBase **lplpHeader=NULL,
                           _In_ BOOL bReplaceExisting=TRUE);
  HRESULT RemoveRequestHeader(_In_z_ LPCSTR szNameA);
  HRESULT RemoveRequestHeader(_In_ CHttpHeaderBase *lpHeader);
  HRESULT RemoveAllRequestHeaders();

  HRESULT AddRequestCookie(_In_ CHttpCookie &cSrc);
  HRESULT AddRequestCookie(_In_ CHttpCookieArray &cSrc);
  HRESULT AddRequestCookie(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA);
  HRESULT AddRequestCookie(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szValueW);
  HRESULT RemoveRequestCookie(_In_z_ LPCSTR szNameA);
  HRESULT RemoveRequestCookie(_In_z_ LPCWSTR szNameW);
  HRESULT RemoveAllRequestCookies();

  HRESULT AddRequestPostData(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA);
  HRESULT AddRequestPostData(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szValueW);
  HRESULT AddRequestPostDataFile(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szFileNameA, _In_ CStream *lpStream);
  HRESULT AddRequestPostDataFile(_In_z_ LPCWSTR szNameW, _In_z_ LPCWSTR szFileNameW, _In_ CStream *lpStream);
  HRESULT AddRequestRawPostData(_In_ LPCVOID lpData, _In_ SIZE_T nLength);
  HRESULT AddRequestRawPostData(_In_ CStream *lpStream);
  HRESULT EnableDynamicPostData();
  HRESULT SignalEndOfPostData();

  HRESULT RemoveRequestPostData(_In_opt_z_ LPCSTR szNameA);
  HRESULT RemoveRequestPostData(_In_opt_z_ LPCWSTR szNameW);
  HRESULT RemoveAllRequestPostData();

  HRESULT SetProxy(_In_ CProxy &cProxy);

  HRESULT SetAuthCredentials(_In_opt_z_ LPCWSTR szUserNameW, _In_opt_z_ LPCWSTR szPasswordW);

  HRESULT Open(_In_ CUrl &cUrl);
  HRESULT Open(_In_z_ LPCSTR szUrlA);
  HRESULT Open(_In_z_ LPCWSTR szUrlW);
  VOID Close(_In_opt_ BOOL bReuseConn=TRUE);

  HRESULT GetLastRequestError();

  HRESULT IsResponseHeaderComplete();
  BOOL IsDocumentComplete();
  BOOL IsClosed();

  LONG GetResponseStatus();
  HRESULT GetResponseReason(_Inout_ CStringA &cStrDestA);

  CHttpBodyParserBase* GetResponseBodyParser();

  SIZE_T GetResponseHeadersCount() const;
  CHttpHeaderBase* GetResponseHeader(_In_ SIZE_T nIndex) const;

  template<class T>
  T* GetResponseHeader() const
    {
    return reinterpret_cast<T*>(GetResponseHeader(T::GetHeaderNameStatic()));
    };

  CHttpHeaderBase* GetResponseHeader(_In_z_ LPCSTR szNameA) const;

  SIZE_T GetResponseCookiesCount() const;

  HRESULT GetResponseCookie(_In_ SIZE_T nIndex, _Out_ CHttpCookie &cCookie);
  HRESULT GetResponseCookies(_Out_ CHttpCookieArray &cCookieArray);

  HANDLE GetUnderlyingSocket() const;
  CSockets* GetUnderlyingSocketManager() const;

  LPCSTR GetRequestBoundary() const;

private:
  HRESULT InternalOpen(_In_ CUrl &cUrl);

  HRESULT OnSocketCreate(_In_ CIpc *lpIpc, _In_ HANDLE h, _Inout_ CIpc::CREATE_CALLBACK_DATA &sData);
  VOID OnSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                       _In_ HRESULT hrErrorCode);
  HRESULT OnSocketConnect(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_opt_ CIpc::CUserData *lpUserData,
                          _In_ HRESULT hrErrorCode);
  HRESULT OnSocketDataReceived(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData);

  VOID OnRedirectOrRetryAuth(_In_ LONG nTimerId, _In_ LPVOID lpUserData);
  VOID OnResponseHeadersTimeout(_In_ LONG nTimerId, _In_ LPVOID lpUserData);
  VOID OnResponseBodyTimeout(_In_ LONG nTimerId, _In_ LPVOID lpUserData);
  VOID OnAfterSendRequestHeaders(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ LPVOID lpCookie,
                                 _In_ CIpc::CUserData *lpUserData);
  VOID OnAfterSendRequestBody(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ LPVOID lpCookie,
                              _In_ CIpc::CUserData *lpUserData);

  VOID SetErrorOnRequestAndClose(_In_ HRESULT hrErrorCode);

  HRESULT SetupRedirectOrRetry(_In_ DWORD dwDelayMs);
  HRESULT SetupResponseHeadersTimeout();

  HRESULT AddSslLayer(_In_ CIpc *lpIpc, _In_ HANDLE h);

  HRESULT BuildRequestHeaders(_Inout_ CStringA &cStrReqHdrsA);
  HRESULT BuildRequestHeaderAdd(_Inout_ CStringA &cStrReqHdrsA, _In_z_ LPCSTR szNameA, _In_z_ LPCSTR szDefaultValueA,
                                _In_ CHttpHeaderBase::eBrowser nBrowser);
  HRESULT AddRequestHeadersForBody(_Inout_ CStringA &cStrReqHdrsA);

  HRESULT SendTunnelConnect();
  HRESULT SendRequestHeaders();
  HRESULT SendRequestBody();

  VOID GenerateRequestBoundary();

  HRESULT OnDownloadStarted(_Out_ LPHANDLE lphFile, _In_z_ LPCWSTR szFileNameW, _In_ LPVOID lpUserParam);

  VOID ResetRequestForNewRequest();
  VOID ResetResponseForNewRequest();

private:
  class CPostDataItem : public virtual CBaseMemObj, public TLnkLstNode<CPostDataItem>
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CPostDataItem);
  public:
    CPostDataItem(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szValueA);
    CPostDataItem(_In_z_ LPCSTR szNameA, _In_z_ LPCSTR szFileNameA, _In_ CStream *lpStream);
    CPostDataItem(_In_ CStream *lpStream);
    ~CPostDataItem();

  public:
    LPSTR szNameA, szValueA;
    SIZE_T nValueLen;
    TAutoRefCounted<CStream> cStream;
  };

private:
  CCriticalSection cMutex;
  CSockets &cSocketMgr;
  LONG volatile nRundownLock;
  LONG volatile nPendingHandlesCounter;
  eState nState;
  HANDLE hConn;
  CProxy cProxy;
  CSecureStringW cStrAuthUserNameW, cStrAuthUserPasswordW;
  HRESULT hLastErrorCode;
  DWORD dwResponseHeaderTimeoutMs;
  DWORD dwResponseBodyMinimumThroughputInBps;
  DWORD dwResponseBodySecondsOfLowThroughput;
  DWORD dwMaxRedirCount;
  DWORD dwMaxFieldSize;
  ULONGLONG ullMaxFileSize;
  DWORD dwMaxFilesCount;
  CStringW cStrTemporaryFolderW;
  DWORD dwMaxBodySizeInMemory;
  ULONGLONG ullMaxBodySize;
  BOOL bKeepConnectionOpen;
  BOOL bAcceptCompressedContent;

  OnHeadersReceivedCallback cHeadersReceivedCallback;
  OnDymanicRequestBodyStartCallback cDymanicRequestBodyStartCallback;
  OnDocumentCompletedCallback cDocumentCompletedCallback;
  OnErrorCallback cErrorCallback;
  OnQueryCertificatesCallback cQueryCertificatesCallback;

  struct {
    CHttpCommon cHttpCmn;
    CUrl cUrl;
    CStringA cStrMethodA;
    struct {
      TLnkLst<CPostDataItem> cList;
      BOOL bHasRaw;
      BOOL bIsDynamic;
    } sPostData;
    CHAR szBoundaryA[32];
    BOOL bUsingMultiPartFormData;
    BOOL bUsingProxy;
  } sRequest;

  struct {
    CHttpCommon cHttpCmn;
    CStringW cStrDownloadFileNameW;
    LONG volatile nTimerId;
    DWORD dwBodyLowThroughputCcounter;
  } sResponse;

  struct {
    CUrl cUrl;
    DWORD dwRedirectCounter;
    LONG volatile nTimerId;
  } sRedirectOrRetryAuth;

  struct {
    BOOL bSeen401, bSeen407;
    int nSeen401Or407Counter;
  } sAuthorization;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPCLIENT_H
