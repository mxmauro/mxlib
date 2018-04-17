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
#ifndef _MX_HTTPCLIENT_H
#define _MX_HTTPCLIENT_H

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
#include "..\Comm\Proxy.h"

//-----------------------------------------------------------

namespace MX {

class CHttpClient : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CHttpClient);
public:
  typedef enum {
    StateClosed = 0,
    StateSendingRequest,
    StateReceivingResponseHeaders,
    StateReceivingResponseBody,
    StateDocumentCompleted,
    StateWaitingForRedirection,
    StateEstablishingProxyTunnelConnection,
    StateWaitingProxyTunnelConnectionResponse
  } eState;

  //--------

  //NOTE: Leave cStrFullFileNameW empty to download to temp location (with imposed limitations)
  typedef Callback<HRESULT (_In_ CHttpClient *lpHttp, _In_z_ LPCWSTR szFileNameW, _In_opt_ PULONGLONG lpnContentSize,
                            _In_z_ LPCSTR szTypeA, _In_ BOOL bTreatAsAttachment, _Out_ CStringW &cStrFullFileNameW,
                            _Outptr_result_maybenull_ CHttpBodyParserBase **lpBodyParser)> OnHeadersReceivedCallback;
  typedef Callback<HRESULT (_In_ CHttpClient *lpHttp)> OnDocumentCompletedCallback;

  typedef Callback<VOID (_In_ CHttpClient *lpHttp, _In_ HRESULT hErrorCode)> OnErrorCallback;

  typedef Callback<HRESULT (_In_ CHttpClient *lpHttp, _Inout_ CIpcSslLayer::eProtocol &nProtocol,
                            _Inout_ CSslCertificateArray *&lpCheckCertificates, _Inout_ CSslCertificate *&lpSelfCert,
                            _Inout_ CCryptoRSA *&lpPrivKey)> OnQueryCertificatesCallback;

  //--------

public:
  CHttpClient(_In_ CSockets &cSocketMgr);
  ~CHttpClient();

  VOID SetOption_MaxResponseTimeoutMs(_In_ DWORD dwTimeoutMs);
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

  VOID On(_In_ OnHeadersReceivedCallback cHeadersReceivedCallback);
  VOID On(_In_ OnDocumentCompletedCallback cDocumentCompletedCallback);
  VOID On(_In_ OnErrorCallback cErrorCallback);
  VOID On(_In_ OnQueryCertificatesCallback cQueryCertificatesCallback);

  HRESULT SetRequestMethodAuto();
  HRESULT SetRequestMethod(_In_z_ LPCSTR szMethodA);
  HRESULT SetRequestMethod(_In_z_ LPCWSTR szMethodW);

  template<class T>
  HRESULT AddRequestHeader(_Out_ T **lplpHeader=NULL)
    {
    return AddRequestHeader(T::GetNameStatic(), reinterpret_cast<CHttpHeaderBase**>(lplpHeader));
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

  HRESULT RemoveRequestPostData(_In_opt_z_ LPCSTR szNameA);
  HRESULT RemoveRequestPostData(_In_opt_z_ LPCWSTR szNameW);
  HRESULT RemoveAllRequestPostData();

  HRESULT SetProxy(_In_ CProxy &cProxy);

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
    return reinterpret_cast<T*>(GetResponseHeader(T::GetNameStatic()));
    };

  CHttpHeaderBase* GetResponseHeader(_In_z_ LPCSTR szNameA) const;

  SIZE_T GetResponseCookiesCount() const;

  HRESULT GetResponseCookie(_In_ SIZE_T nIndex, _Out_ CHttpCookie &cCookie);
  HRESULT GetResponseCookies(_Out_ CHttpCookieArray &cCookieArray);

  HANDLE GetUnderlyingSocket() const;
  CSockets* GetUnderlyingSocketManager() const;

private:
  HRESULT InternalOpen(_In_ CUrl &cUrl);

  HRESULT OnSocketCreate(_In_ CIpc *lpIpc, _In_ HANDLE h, _Inout_ CIpc::CREATE_CALLBACK_DATA &sData);
  VOID OnSocketDestroy(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData,
                       _In_ HRESULT hErrorCode);
  HRESULT OnSocketConnect(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_opt_ CIpc::CUserData *lpUserData,
                          _Inout_ CIpc::CLayerList &cLayersList, _In_ HRESULT hErrorCode);
  HRESULT OnSocketDataReceived(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ CIpc::CUserData *lpUserData);

  VOID OnRedirect(_In_ CTimedEventQueue::CEvent *lpEvent);
  VOID OnResponseTimeout(_In_ CTimedEventQueue::CEvent *lpEvent);
  VOID OnAfterSendRequest(_In_ CIpc *lpIpc, _In_ HANDLE h, _In_ LPVOID lpCookie, _In_ CIpc::CUserData *lpUserData);

  VOID SetErrorOnRequestAndClose(_In_ HRESULT hErrorCode);

  HRESULT UpdateResponseTimeoutEvent();

  HRESULT AddSslLayer(_In_ CIpc *lpIpc, _In_ HANDLE h, _Inout_opt_ CIpc::CREATE_CALLBACK_DATA *lpData);

  HRESULT BuildRequestHeaders(_Inout_ CStringA &cStrReqHdrsA);
  HRESULT BuildRequestHeaderAdd(_Inout_ CStringA &cStrReqHdrsA, _In_z_ LPCSTR szNameA, _In_z_ LPCSTR szDefaultValueA);
  HRESULT AddRequestHeadersForBody(_Inout_ CStringA &cStrReqHdrsA);

  HRESULT SendTunnelConnect();
  HRESULT SendRequestHeader();
  HRESULT SendRequestBody();

  HRESULT OnDownloadStarted(_Out_ LPHANDLE lphFile, _In_z_ LPCWSTR szFileNameW, _In_ LPVOID lpUserParam);

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
  CSystemTimedEventQueue *lpTimedEventQueue;
  TPendingListHelperGeneric<CTimedEventQueue::CEvent*> cPendingEvents;
  TPendingListHelperGeneric<HANDLE> cPendingHandles;
  LONG volatile nRundownLock;
  eState nState;
  HANDLE hConn;
  CProxy cProxy;
  HRESULT hLastErrorCode;

  DWORD dwResponseTimeoutMs;
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
  OnDocumentCompletedCallback cDocumentCompletedCallback;
  OnErrorCallback cErrorCallback;
  OnQueryCertificatesCallback cQueryCertificatesCallback;

  class CRequest : public CBaseMemObj
  {
  public:
    CRequest();
    ~CRequest();

  public:
    CUrl cUrl;
    CStringA cStrMethodA;
    CHttpCommon cHttpCmn;
    struct {
      TLnkLst<CPostDataItem> cList;
      BOOL bHasRaw;
    } sPostData;
    CHAR szBoundary[32];
    BOOL bUsingMultiPartFormData;
    BOOL bUsingProxy;
  } cRequest;

  class CResponse : public CBaseMemObj
  {
  public:
    CResponse();

  public:
    CHttpCommon cHttpCmn;
    CTimedEventQueue::CEvent *lpTimeoutEvent;
    CStringW cStrDownloadFileNameW;
  } cResponse;

  struct {
    CTimedEventQueue::CEvent *lpEvent;
    CUrl cUrl;
    SIZE_T nWaitTimeSecs;
    DWORD dwRedirectCounter;
  } sRedirect;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPCLIENT_H
