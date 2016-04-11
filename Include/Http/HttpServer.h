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

#define MX_HTTP_SERVER_RequestTimeoutMs               "HTTP_SERVER_RequestTimeoutMs"
#define MX_HTTP_SERVER_RequestTimeoutMs_DEFVAL        60000

//-----------------------------------------------------------

namespace MX {

class CHttpServer : public virtual CBaseMemObj, private CCriticalSection
{
  MX_DISABLE_COPY_CONSTRUCTOR(CHttpServer);
public:
  class CRequest;

  //--------

  typedef Callback<HRESULT (__in CHttpServer *lpHttp, __in CRequest *lpRequest,
                            __inout CHttpBodyParserBase *&lpBodyParser)> OnRequestHeadersReceivedCallback;
  typedef Callback<HRESULT (__in CHttpServer *lpHttp, __in CRequest *lpRequest)> OnRequestCompletedCallback;

  typedef Callback<VOID (__in CHttpServer *lpHttp, __in CRequest *lpRequest, __in HRESULT hErrorCode)> OnErrorCallback;

  //--------

public:
  CHttpServer(__in CSockets &cSocketMgr, __in CPropertyBag &cPropBag);
  ~CHttpServer();

  VOID On(__in OnRequestHeadersReceivedCallback cRequestHeadersReceivedCallback);
  VOID On(__in OnRequestCompletedCallback cRequestCompletedCallback);
  VOID On(__in OnErrorCallback cErrorCallback);

  HRESULT StartListening(__in int nPort, __in_opt CIpcSslLayer::eProtocol nProtocol=CIpcSslLayer::ProtocolUnknown,
                         __in_opt CSslCertificate *lpSslCertificate=NULL, __in_opt CCryptoRSA *lpSslKey=NULL);
  HRESULT StartListening(__in_z LPCSTR szBindAddressA, __in int nPort,
                         __in_opt CIpcSslLayer::eProtocol nProtocol=CIpcSslLayer::ProtocolUnknown,
                         __in_opt CSslCertificate *lpSslCertificate=NULL, __in_opt CCryptoRSA *lpSslKey=NULL);
  HRESULT StartListening(__in_z LPCWSTR szBindAddressW, __in int nPort,
                         __in_opt CIpcSslLayer::eProtocol nProtocol=CIpcSslLayer::ProtocolUnknown,
                         __in_opt CSslCertificate *lpSslCertificate=NULL, __in_opt CCryptoRSA *lpSslKey=NULL);
  VOID StopListening();

public:
  class CRequestUserData : public virtual CBaseMemObj, public TRefCounted<CRequestUserData>
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CRequestUserData);
  protected:
    CRequestUserData() : CBaseMemObj(), TRefCounted<CRequestUserData>()
      { };
  public:
    virtual ~CRequestUserData()
      { };
  };

public:
  class CRequest : public virtual CBaseMemObj, public TLnkLstNode<CRequest>, public CIpc::CUserData
  {
    MX_DISABLE_COPY_CONSTRUCTOR(CRequest);
  private:
    CRequest(__in CHttpServer *lpHttpServer, __in CPropertyBag &cPropBag);
  public:
    ~CRequest();

    LPCSTR GetMethod() const;
    CUrl* GetUrl() const;

    SIZE_T GetRequestHeadersCount() const;
    CHttpHeaderBase* GetRequestHeader(__in SIZE_T nIndex) const;

    template<class T>
    T* GetRequestHeader() const
      {
      return reinterpret_cast<T*>(GetRequestHeader(T::GetNameStatic()));
      };
    CHttpHeaderBase* GetRequestHeader(__in_z LPCSTR szNameA) const;

    SIZE_T GetRequestCookiesCount() const;
    CHttpCookie* GetRequestCookie(__in SIZE_T nIndex) const;

    CHttpBodyParserBase* GetRequestBodyParser() const;

    VOID ResetResponse();

    HRESULT SetResponseStatus(__in LONG nStatus, __in_opt LPCSTR szReasonA=NULL);

    template<class T>
    HRESULT AddResponseHeader(__out_opt T **lplpHeader=NULL)
      {
      return AddResponseHeader(T::GetNameStatic(), reinterpret_cast<CHttpHeaderBase**>(lplpHeader));
      };
    HRESULT AddResponseHeader(__in_z LPCSTR szNameA, __out_opt CHttpHeaderBase **lplpHeader=NULL);

    template<class T>
    HRESULT AddResponseHeader(__in_z LPCSTR szValueA, __in_opt SIZE_T nValueLen=(SIZE_T)-1,
                              __out_opt T **lplpHeader=NULL)
      {
      return AddResponseHeader(T::GetNameStatic(), szValueA, nValueLen,
                               reinterpret_cast<CHttpHeaderBase**>(lplpHeader));
      };
    HRESULT AddResponseHeader(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA, __in_opt SIZE_T nValueLen=(SIZE_T)-1,
                              __out_opt CHttpHeaderBase **lplpHeader=NULL);

    //NOTE: Unicode values will be UTF-8 & URL encoded
    template<class T>
    HRESULT AddResponseHeader(__in_z LPCWSTR szValueW, __in_opt SIZE_T nValueLen=(SIZE_T)-1,
                              __out_opt T **lplpHeader=NULL)
      {
      return AddResponseHeader(T::GetNameStatic(), szValueW, nValueLen,
                               reinterpret_cast<CHttpHeaderBase**>(lplpHeader));
      };
    HRESULT AddResponseHeader(__in_z LPCSTR szNameA, __in_z LPCWSTR szValueW, __in_opt SIZE_T nValueLen=(SIZE_T)-1,
                              __out_opt CHttpHeaderBase **lplpHeader=NULL);

    HRESULT RemoveResponseHeader(__in_z LPCSTR szNameA);
    HRESULT RemoveAllResponseHeaders();

    SIZE_T GetResponseHeadersCount() const;

    CHttpHeaderBase* GetResponseHeader(__in SIZE_T nIndex) const;
    template<class T>
    T* GetResponseHeader() const
      {
      return reinterpret_cast<T*>(GetResponseHeader(T::GetNameStatic()));
      };
    CHttpHeaderBase* GetResponseHeader(__in_z LPCSTR szNameA) const;

    HRESULT AddResponseCookie(__in CHttpCookie &cSrc);
    HRESULT AddResponseCookie(__in CHttpCookieArray &cSrc);
    HRESULT AddResponseCookie(__in_z LPCSTR szNameA, __in_z LPCSTR szValueA, __in_z_opt LPCSTR szDomainA=NULL,
                              __in_z_opt LPCSTR szPathA=NULL, __in_opt const CDateTime *lpDate=NULL,
                              __in_opt BOOL bIsSecure=FALSE, __in_opt BOOL bIsHttpOnly=FALSE);
    HRESULT AddResponseCookie(__in_z LPCWSTR szNameW, __in_z LPCWSTR szValueW, __in_z_opt LPCWSTR szDomainW=NULL,
                              __in_z_opt LPCWSTR szPathW=NULL, __in_opt const CDateTime *lpDate=NULL,
                              __in_opt BOOL bIsSecure=FALSE, __in_opt BOOL bIsHttpOnly=FALSE);
    HRESULT RemoveResponseCookie(__in_z LPCSTR szNameA);
    HRESULT RemoveResponseCookie(__in_z LPCWSTR szNameW);
    HRESULT RemoveAllResponseCookies();
    CHttpCookie* GetResponseCookie(__in SIZE_T nIndex) const;
    CHttpCookie* GetResponseCookie(__in_z LPCSTR szNameA) const;
    CHttpCookie* GetResponseCookie(__in_z LPCWSTR szNameW) const;
    CHttpCookieArray* GetResponseCookies() const;
    SIZE_T GetResponseCookiesCount() const;

    HRESULT SendResponse(__in LPCVOID lpData, __in SIZE_T nDataLen);
    HRESULT SendFile(__in_z LPCWSTR szFileNameW);
    HRESULT SendStream(__in CStream *lpStream, __in_z_opt LPCWSTR szFileNameW=NULL);

    HRESULT SendErrorPage(__in LONG nStatusCode, __in HRESULT hErrorCode, __in_z_opt LPCSTR szBodyExplanationA=NULL);

    HANDLE GetUnderlyingSocket() const;
    CSockets* GetUnderlyingSocketManager() const;

    VOID SetUserData(__in CRequestUserData *lpUserData);
    CRequestUserData* GetUserData() const;

  private:
    VOID ResetForNewRequest();

    BOOL IsKeepAliveRequest() const;
    BOOL HasErrorBeenSent() const;

    HRESULT BuildAndSendResponse();
    HRESULT BuildAndInsertHeaderStream();

    VOID MarkLinkAsClosed();
    BOOL IsLinkClosed() const;

  private:
    friend class CHttpServer;

    typedef enum {
      StateClosed = 0,
      StateReceivingRequestHeaders,
      StateReceivingRequestBody,
      StateBuildingResponse,
      StateIgnoringRequest400,
      StateIgnoringRequest413,
      StateError
    } eState;

    LONG volatile nMutex;
    CHttpServer *lpHttpServer;
    CSockets *lpSocketMgr;
    HANDLE hConn;
    eState nState;
    LONG volatile nFlags;

    struct tagRequest {
      tagRequest(__in CPropertyBag &cPropBag) : cHttpCmn(TRUE, cPropBag)
        { };

      CUrl cUrl;
      CStringA cStrMethodA;
      CHttpCommon cHttpCmn;
      struct {
        LONG volatile nMutex;
        TPendingListHelperGeneric<CTimedEventQueue::CEvent*> cActiveList;
      } sTimeout;
      ULARGE_INTEGER liLastRead;
    } sRequest;

    struct tagResponse {
      tagResponse(__in CPropertyBag &cPropBag) : cHttpCmn(FALSE, cPropBag)
        { };

      LONG nStatus;
      CStringA cStrReasonA;
      CHttpCommon cHttpCmn;
      TArrayListWithRelease<CStream*> aStreamsList;
      BOOL bLastStreamIsData;
      LPCSTR szMimeTypeHintA;
      CStringA cStrFileNameA;
    } sResponse;

    TAutoRefCounted<CRequestUserData> cUserData;
  };

private:
  friend class CRequest;

  typedef TArrayList<CTimedEventQueue::CEvent*> __TEventArray;

  HRESULT OnSocketCreate(__in CIpc *lpIpc, __in HANDLE h, __deref_inout CIpc::CUserData **lplpUserData,
                         __inout CIpc::CREATE_CALLBACK_DATA &sData);
  VOID OnListenerSocketDestroy(__in CIpc *lpIpc, __in HANDLE h, __in CIpc::CUserData *lpUserData,
                               __in HRESULT hErrorCode);

  VOID OnSocketDestroy(__in CIpc *lpIpc, __in HANDLE h, __in CIpc::CUserData *lpUserData,
                       __in HRESULT hErrorCode);
  HRESULT OnSocketConnect(__in CIpc *lpIpc, __in HANDLE h, __in CIpc::CUserData *lpUserData,
                          __inout CIpc::CLayerList &cLayersList, __in HRESULT hErrorCode);
  HRESULT OnSocketDataReceived(__in CIpc *lpIpc, __in HANDLE h, __in CIpc::CUserData *lpUserData);

  VOID OnRequestTimeout(__in CTimedEventQueue::CEvent *lpEvent);

  HRESULT SendStreams(__in CRequest *lpRequest, __in BOOL bForceClose);

  HRESULT QuickSendErrorResponseAndReset(__in CRequest *lpRequest, __in LONG nErrorCode, __in HRESULT hErrorCode,
                                         __in_opt BOOL bForceClose=TRUE);

  HRESULT SendGenericErrorPage(__in CRequest *lpRequest, __in LONG nErrorCode, __in HRESULT hErrorCode,
                               __in_z_opt LPCSTR szBodyExplanationA=NULL);
  static LPCSTR LocateError(__in LONG nErrorCode);

  VOID OnAfterSendResponse(__in CIpc *lpIpc, __in HANDLE h, __in LPVOID lpCookie, __in CIpc::CUserData *lpUserData);

  HRESULT SetupTimeoutEvent(__in CRequest *lpRequest, __in DWORD dwTimeoutMs);
  VOID CancelAllTimeoutEvents(__in CRequest *lpRequest);

  VOID DoCancelEventsCallback(__in __TEventArray &cEventsList);

  VOID OnRequestDestroyed(__in CRequest *lpRequest);

private:
  CSockets &cSocketMgr;
  CPropertyBag &cPropBag;
  DWORD dwRequestTimeoutMs;
  struct {
    LONG volatile nRwMutex;
    CIpcSslLayer::eProtocol nProtocol;
    TAutoDeletePtr<CSslCertificate> cSslCertificate;
    TAutoDeletePtr<CCryptoRSA> cSslPrivateKey;
  } sSsl;
  CSystemTimedEventQueue *lpTimedEventQueue;
  LONG volatile nRundownLock;
  HANDLE hAcceptConn;
  OnRequestHeadersReceivedCallback cRequestHeadersReceivedCallback;
  OnRequestCompletedCallback cRequestCompletedCallback;
  OnErrorCallback cErrorCallback;
  LONG volatile nRequestsMutex;
  TLnkLst<CRequest> cRequestsList;
};

} //namespace MX

//-----------------------------------------------------------

#endif //_MX_HTTPSERVER_H
