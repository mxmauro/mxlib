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

//-----------------------------------------------------------

#define MX_HTTP_CLIENT_ResponseTimeoutMs               "HTTP_CLIENT_ResponseTimeoutMs"
#define MX_HTTP_CLIENT_ResponseTimeoutMs_DEFVAL        60000

#define MX_HTTP_CLIENT_MaxRedirectionsCount            "HTTP_CLIENT_MaxRedirectionsCount"
#define MX_HTTP_CLIENT_MaxRedirectionsCount_DEFVAL     10

//-----------------------------------------------------------

namespace MX {

class CHttpClient : public virtual CBaseMemObj
{
  MX_DISABLE_COPY_CONSTRUCTOR(CHttpClient);
public:
  typedef enum {
    OptionKeepConnectionOpen = 1,
    OptionAcceptCompressedContent = 2
  } eOptionFlags;

  typedef enum {
    StateClosed = 0,
    StateSendingRequest,
    StateReceivingResponseHeaders,
    StateReceivingResponseBody,
    StateDocumentCompleted,
    StateWaitingForRedirection
  } eState;

  //--------

  typedef Callback<HRESULT (__in CHttpClient *lpHttp,
                            __inout CHttpBodyParserBase *&lpBodyParser)> OnHeadersReceivedCallback;
  typedef Callback<HRESULT (__in CHttpClient *lpHttp)> OnDocumentCompletedCallback;

  typedef Callback<VOID (__in CHttpClient *lpHttp, __in HRESULT hErrorCode)> OnErrorCallback;

  typedef Callback<HRESULT (__in CHttpClient *lpHttp, __inout CIpcSslLayer::eProtocol &nProtocol,
                            __inout CSslCertificateArray *&lpCheckCertificates, __inout CSslCertificate *&lpSelfCert,
                            __inout CCryptoRSA *&lpPrivKey)> OnQueryCertificatesCallback;

  //--------

public:
  CHttpClient(__in CSockets &cSocketMgr, __in CPropertyBag &cPropBag);
  ~CHttpClient();

  VOID On(__in OnHeadersReceivedCallback cHeadersReceivedCallback);
  VOID On(__in OnDocumentCompletedCallback cDocumentCompletedCallback);
  VOID On(__in OnErrorCallback cErrorCallback);
  VOID On(__in OnQueryCertificatesCallback cQueryCertificatesCallback);

  HRESULT SetRequestMethodAuto();
  HRESULT SetRequestMethod(__in_z LPCSTR szMethodA);
  HRESULT SetRequestMethod(__in_z LPCWSTR szMethodW);

  template<class T>
  HRESULT AddRequestHeader(__out T **lplpHeader=NULL)
    {
    return AddRequestHeader(T::GetNameStatic(), reinterpret_cast<CHttpHeaderBase**>(lplpHeader));
    };
  HRESULT AddRequestHeader(__in_z LPCSTR szNameA, __out CHttpHeaderBase **lplpHeader);
  HRESULT AddRequestHeader(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA, __out_opt CHttpHeaderBase **lplpHeader=NULL);
  HRESULT AddRequestHeader(__in_z LPCSTR szNameA, __in_z LPCWSTR szValueW, __out_opt CHttpHeaderBase **lplpHeader=NULL);
  HRESULT RemoveRequestHeader(__in_z LPCSTR szNameA);
  HRESULT RemoveAllRequestHeaders();

  HRESULT AddRequestCookie(__in CHttpCookie &cSrc);
  HRESULT AddRequestCookie(__in CHttpCookieArray &cSrc);
  HRESULT AddRequestCookie(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA);
  HRESULT AddRequestCookie(__in_z LPCWSTR szNameW, __in_z LPCWSTR szValueW);
  HRESULT RemoveRequestCookie(__in_z LPCSTR szNameA);
  HRESULT RemoveRequestCookie(__in_z LPCWSTR szNameW);
  HRESULT RemoveAllRequestCookies();

  HRESULT AddRequestPostData(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA);
  HRESULT AddRequestPostData(__in_z LPCWSTR szNameW, __in_z LPCWSTR szValueW);

  HRESULT AddRequestPostDataFile(__in_z LPCSTR szNameA, __in_z LPCSTR szFileNameA, __in CStream *lpStream);
  HRESULT AddRequestPostDataFile(__in_z LPCWSTR szNameW, __in_z LPCWSTR szFileNameW, __in CStream *lpStream);

  HRESULT RemoveRequestPostData(__in_z LPCSTR szNameA);
  HRESULT RemoveRequestPostData(__in_z LPCWSTR szNameW);
  HRESULT RemoveAllRequestPostData();

  HRESULT SetOptionFlags(__in int nOptionFlags);
  int GetOptionFlags();

  HRESULT Open(__in CUrl &cUrl);
  HRESULT Open(__in_z LPCSTR szUrlA);
  HRESULT Open(__in_z LPCWSTR szUrlW);
  VOID Close(__in_opt BOOL bReuseConn=TRUE);

  HRESULT GetLastRequestError();

  HRESULT IsResponseHeaderComplete();
  BOOL IsDocumentComplete();
  BOOL IsClosed();

  LONG GetResponseStatus();
  HRESULT GetResponseReason(__inout CStringA &cStrDestA);

  CHttpBodyParserBase* GetResponseBodyParser();

  SIZE_T GetResponseHeadersCount() const;
  CHttpHeaderBase* GetResponseHeader(__in SIZE_T nIndex) const;

  template<class T>
  T* GetResponseHeader() const
    {
    return reinterpret_cast<T*>(GetResponseHeader(T::GetNameStatic()));
    };

  CHttpHeaderBase* GetResponseHeader(__in_z LPCSTR szNameA) const;

  SIZE_T GetResponseCookiesCount() const;

  HRESULT GetResponseCookie(__in SIZE_T nIndex, __out CHttpCookie &cCookie);
  HRESULT GetResponseCookies(__out CHttpCookieArray &cCookieArray);

  HANDLE GetUnderlyingSocket() const;
  CSockets* GetUnderlyingSocketManager() const;

private:
  HRESULT InternalOpen(__in CUrl &cUrl);

  HRESULT OnSocketCreate(__in CIpc *lpIpc, __in HANDLE h, __inout CIpc::CREATE_CALLBACK_DATA &sData);
  VOID OnSocketDestroy(__in CIpc *lpIpc, __in HANDLE h, __in CIpc::CUserData *lpUserData,
                       __in HRESULT hErrorCode);
  HRESULT OnSocketConnect(__in CIpc *lpIpc, __in HANDLE h, __in CIpc::CUserData *lpUserData,
                          __inout CIpc::CLayerList &cLayersList, __in HRESULT hErrorCode);
  HRESULT OnSocketDataReceived(__in CIpc *lpIpc, __in HANDLE h, __in CIpc::CUserData *lpUserData);

  VOID OnRedirect(__in CTimedEventQueue::CEvent *lpEvent);
  VOID OnResponseTimeout(__in CTimedEventQueue::CEvent *lpEvent);
  VOID OnAfterSendRequest(__in CIpc *lpIpc, __in HANDLE h, __in LPVOID lpCookie, __in CIpc::CUserData *lpUserData);

  VOID SetErrorOnRequestAndClose(__in HRESULT hErrorCode);

  HRESULT BuildRequestHeaders(__inout CStringA &cStrReqHdrsA);
  HRESULT BuildRequestHeaderAdd(__inout CStringA &cStrReqHdrsA, __in_z LPCSTR szNameA, __in_z LPCSTR szDefaultValueA);
  HRESULT AddRequestHeadersForBody(__inout CStringA &cStrReqHdrsA);

  HRESULT SendRequestBody();

private:
  class CPostDataItem : public virtual CBaseMemObj, public TLnkLstNode<CPostDataItem>
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CPostDataItem);
  public:
    CPostDataItem(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA, __in_opt CStream *lpStream=NULL);
    ~CPostDataItem();

    LPCSTR GetName() const
      {
      return szNameA;
      };

    LPCSTR GetValue() const
      {
      return szValueA;
      };

    CStream* GetStream() const
      {
      return cStream.Get();
      };

  private:
    LPSTR szNameA, szValueA;
    TAutoRefCounted<CStream> cStream;
  };

private:
  CCriticalSection cMutex;
  CSockets &cSocketMgr;
  CPropertyBag &cPropBag;
  DWORD dwResponseTimeoutMs, dwMaxRedirCount;
  CSystemTimedEventQueue *lpTimedEventQueue;
  TPendingListHelperGeneric<CTimedEventQueue::CEvent*> cPendingEvents;
  TPendingListHelperGeneric<HANDLE> cPendingHandles;
  LONG volatile nRundownLock;
  eState nState;
  int nOptionFlags;
  HANDLE hConn;
  HRESULT hLastErrorCode;
  OnHeadersReceivedCallback cHeadersReceivedCallback;
  OnDocumentCompletedCallback cDocumentCompletedCallback;
  OnErrorCallback cErrorCallback;
  OnQueryCertificatesCallback cQueryCertificatesCallback;

  struct tagRequest {
    tagRequest(__in CPropertyBag &cPropBag) : cHttpCmn(FALSE, cPropBag)
      { };

    CUrl cUrl;
    CStringA cStrMethodA;
    CHttpCommon cHttpCmn;
    TLnkLst<CPostDataItem> cPostData;
    CHAR szBoundary[32];
    BOOL bUsingMultiPartFormData;
  } sRequest;

  struct tagResponse {
    tagResponse(__in CPropertyBag &cPropBag) : cHttpCmn(FALSE, cPropBag)
      { };

    CHttpCommon cHttpCmn;
    CTimedEventQueue::CEvent *lpTimeoutEvent;
  } sResponse;

  struct tagRedirect {
    CTimedEventQueue::CEvent *lpEvent;
    CUrl cUrl;
    SIZE_T nWaitTimeSecs;
    DWORD dwRedirectCounter;
  } sRedirect;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPCLIENT_H
