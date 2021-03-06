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
#include "TestJsHttpServer.h"
#include <JsHttpServer\JsHttpServer.h>
#include <JsHttpServer\Plugins\JsHttpServerSessionPlugin.h>
#include <JsLib\Plugins\JsMySqlPlugin.h>
#include <JsLib\Plugins\JsSQLitePlugin.h>
#include <JsLib\Plugins\JsonWebTokenPlugin.h>
#include <TaskQueue.h>

//-----------------------------------------------------------

static LONG volatile nLogMutex = 0;

//-----------------------------------------------------------

class CTestJsHttpServerWebSocket : public MX::CWebSocket
{
public:
  CTestJsHttpServerWebSocket();
  ~CTestJsHttpServerWebSocket();

  virtual HRESULT OnConnected();
  virtual HRESULT OnTextMessage(_In_ LPCSTR szMsgA, _In_ SIZE_T nMsgLength);
  virtual HRESULT OnBinaryMessage(_In_ LPVOID lpData, _In_ SIZE_T nDataSize);

  virtual SIZE_T GetMaxMessageSize() const;
  virtual VOID OnPongFrame();
  virtual VOID OnCloseFrame(_In_ USHORT wCode, _In_  HRESULT hrErrorCode);
};

//-----------------------------------------------------------

static VOID OnEngineError(_In_ MX::CIpc *lpIpc, _In_ HRESULT hrErrorCode);
static HRESULT OnNewRequestObject(_In_ MX::CJsHttpServer *lpHttp,
                                  _Out_ MX::CJsHttpServer::CClientRequest **lplpRequest);
static VOID OnRequestCompleted(_In_ MX::CJsHttpServer *lpHttp, _In_ MX::CJsHttpServer::CClientRequest *lpRequest);
static VOID OnProcessJsRequest(_In_ MX::CTaskQueue *lpQueue, _In_ MX::CTaskQueue::CTask *lpTask);
static HRESULT OnRequireJsModule(_In_ MX::CJsHttpServer *lpHttp, _In_ MX::CJsHttpServer::CClientRequest *lpRequest,
                                 _Inout_ MX::CJavascriptVM &cJvm,
                                 _Inout_ MX::CJavascriptVM::CRequireModuleContext *lpReqContext,
                                 _Inout_ MX::CStringA &cStrCodeA);
static HRESULT OnWebSocketRequestReceived(_In_ MX::CJsHttpServer *lpHttp,
                                          _In_ MX::CJsHttpServer::CClientRequest *lpRequest, _In_ int nVersion,
                                          _In_opt_ LPCSTR *szProtocolsA, _In_ SIZE_T nProtocolsCount,
                                          _Out_ int &nSelectedProtocol, _In_ MX::TArrayList<int> &aSupportedVersions,
                                          _Out_ _Maybenull_ MX::CWebSocket **lplpWebSocket);
static VOID DeleteSessionFiles();
static HRESULT OnSessionPersistance(_In_ MX::CJsHttpServerSessionPlugin *lpPlugin,
                                    _In_ MX::CJsHttpServerSessionPlugin::ePersistanceOption nPersistanceOption);
static HRESULT LoadTxtFile(_Inout_ MX::CStringA &cStrContentsA, _In_z_ LPCWSTR szFileNameW);
static HRESULT BuildWebFileName(_Inout_ MX::CStringW &cStrFullFileNameW, _Out_ LPCWSTR &szExtensionW,
                                _In_z_ LPCWSTR szPathW);
static DukTape::duk_ret_t OnGetExecutablePath(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                              _In_z_ LPCSTR szFunctionNameA);
static DukTape::duk_ret_t OnGetUniqueId(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                        _In_z_ LPCSTR szFunctionNameA);
static HRESULT OnLog(_In_z_ LPCWSTR szInfoW);

//-----------------------------------------------------------

class CTestJsRequest : public MX::CJsHttpServer::CClientRequest, public MX::CTaskQueue::CTask
{
public:
  CTestJsRequest() : MX::CJsHttpServer::CClientRequest()
    {
    ::MxMemSet(&sOvr, 0, sizeof(sOvr));
    return;
    };

  HRESULT InitializeSession(_Inout_ MX::CJavascriptVM &cJvm,
                            _Inout_ MX::CJavascriptVM::CRequireModuleContext *lpReqContext)
    {
    HRESULT hRes;

    MX_ASSERT(!cSessionJsObj);
    cSessionJsObj.Attach(MX_DEBUG_NEW MX::CJsHttpServerSessionPlugin());
    if (!cSessionJsObj)
      return E_OUTOFMEMORY;
    hRes = cSessionJsObj->Setup(this, MX_BIND_CALLBACK(&OnSessionPersistance), NULL, NULL, L"/", 2 * 60 * 60, FALSE,
                                TRUE);
    if (FAILED(hRes))
      return hRes;
    return lpReqContext->ReplaceModuleExportsWithObject(cSessionJsObj);
    };

public:
  OVERLAPPED sOvr;
  MX::CStringW cStrFileNameW;

private:
  BOOL OnCleanup()
    {
    if (cSessionJsObj)
    {
      cSessionJsObj->Save();
      cSessionJsObj.Release();
    }
    return MX::CJsHttpServer::CClientRequest::OnCleanup();
    };

private:
  MX::TAutoRefCounted<MX::CJsHttpServerSessionPlugin> cSessionJsObj;
};

//-----------------------------------------------------------

class CJsTest : public virtual MX::CBaseMemObj
{
public:
  CJsTest() : MX::CBaseMemObj(), cSckMgr(cDispatcherPool), cJsHttpServer(cSckMgr)
    {
    return;
    };

  HRESULT OnQuerySslCertificates(_In_ MX::CJsHttpServer *lpHttp,
                                 _Outptr_result_maybenull_ MX::CSslCertificate **lplpSslCert,
                                 _Outptr_result_maybenull_ MX::CEncryptionKey **lplpSslPrivKey,
                                 _Outptr_result_maybenull_ MX::CDhParam **lplpDhParam)
    {
    UNREFERENCED_PARAMETER(lpHttp);

    *lplpSslCert = cSslCert.Get();
    (*lplpSslCert)->AddRef();
    *lplpSslPrivKey = cSslPrivateKey.Get();
    (*lplpSslPrivKey)->AddRef();
    *lplpDhParam = cSslDhParam.Get();
    (*lplpDhParam)->AddRef();
    return S_OK;
    };

public:
  MX::CIoCompletionPortThreadPool cDispatcherPool;
  MX::CSockets cSckMgr;
  MX::TAutoRefCounted<MX::CSslCertificate> cSslCert;
  MX::TAutoRefCounted<MX::CEncryptionKey> cSslPrivateKey;
  MX::TAutoRefCounted<MX::CDhParam> cSslDhParam;
  MX::CJsHttpServer cJsHttpServer; //NOTE: At the end else certificates can be destroyed before
  MX::CTaskQueue cTaskQueue;
};

//-----------------------------------------------------------

int TestJsHttpServer()
{
  CJsTest cTest;
  DWORD dwPort;
  BOOL bUseSSL;
  HRESULT hRes;

  DeleteSessionFiles();

  bUseSSL = DoesCmdLineParamExist(L"ssl");

  hRes = GetCmdLineParamUInt(L"port", &dwPort);
  if (hRes == MX_E_NotFound)
  {
    dwPort = (bUseSSL != FALSE) ? 443 : 80;
  }
  else if (FAILED(hRes) || dwPort < 1 || dwPort > 65535)
  {
    wprintf_s(L"Error: Invalid server port specified.\n");
    return (int)(SUCCEEDED(hRes) ? E_INVALIDARG : hRes);
  }

  cTest.cJsHttpServer.SetOption_MaxFilesCount(10);
  cTest.cJsHttpServer.SetLogCallback(MX_BIND_CALLBACK(&OnLog));
  cTest.cJsHttpServer.SetLogLevel(dwLogLevel);

  //cTest.cSckMgr.SetOption_PacketSize(16384);
  cTest.cSckMgr.SetLogCallback(MX_BIND_CALLBACK(&OnLog));
  cTest.cSckMgr.SetLogLevel(dwLogLevel);

  cTest.cDispatcherPool.SetOption_ThreadStackSize(256 * 1024);
  cTest.cTaskQueue.SetOption_ThreadStackSize(256 * 1024);
  cTest.cTaskQueue.SetOption_MinThreadsCount(4);
  cTest.cTaskQueue.SetOption_ThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);

  hRes = cTest.cDispatcherPool.Initialize();
  if (SUCCEEDED(hRes))
    hRes = cTest.cTaskQueue.Initialize();
  if (SUCCEEDED(hRes))
  {
    cTest.cSckMgr.SetEngineErrorCallback(MX_BIND_CALLBACK(&OnEngineError));
    hRes = cTest.cSckMgr.Initialize();
  }
  if (SUCCEEDED(hRes) && bUseSSL != FALSE)
  {
    MX::CStringA cStrTempA;
    MX::CStringW cStrTempW;

    //load SSL certificate
    hRes = GetAppPath(cStrTempW);
    if (SUCCEEDED(hRes) && cStrTempW.Concat(L"Web\\Certificates\\webserver_ssl_cert.pem") == FALSE)
      hRes = E_OUTOFMEMORY;
    if (SUCCEEDED(hRes))
      hRes = LoadTxtFile(cStrTempA, (LPCWSTR)cStrTempW);
    if (SUCCEEDED(hRes))
    {
      cTest.cSslCert.Attach(MX_DEBUG_NEW MX::CSslCertificate());
      if (cTest.cSslCert)
        hRes = cTest.cSslCert->InitializeFromPEM((LPCSTR)cStrTempA);
      else
        hRes = E_OUTOFMEMORY;
    }
    //load private key
    if (SUCCEEDED(hRes))
      hRes = GetAppPath(cStrTempW);
    if (SUCCEEDED(hRes) && cStrTempW.Concat(L"Web\\Certificates\\webserver_ssl_priv_key.pem") == FALSE)
      hRes = E_OUTOFMEMORY;
    if (SUCCEEDED(hRes))
      hRes = LoadTxtFile(cStrTempA, (LPCWSTR)cStrTempW);
    if (SUCCEEDED(hRes))
    {
      cTest.cSslPrivateKey.Attach(MX_DEBUG_NEW MX::CEncryptionKey());
      if (cTest.cSslPrivateKey)
        hRes = cTest.cSslPrivateKey->SetPrivateKeyFromPEM((LPCSTR)cStrTempA);
      else
        hRes = E_OUTOFMEMORY;
    }
    //load DH param
    if (SUCCEEDED(hRes))
      hRes = GetAppPath(cStrTempW);
    if (SUCCEEDED(hRes) && cStrTempW.Concat(L"Web\\Certificates\\webserver_ssl_dhparam.pem") == FALSE)
      hRes = E_OUTOFMEMORY;
    if (SUCCEEDED(hRes))
      hRes = LoadTxtFile(cStrTempA, (LPCWSTR)cStrTempW);
    if (SUCCEEDED(hRes))
    {
      cTest.cSslDhParam.Attach(MX_DEBUG_NEW MX::CDhParam());
      if (cTest.cSslDhParam)
        hRes = cTest.cSslDhParam->SetFromPEM((LPCSTR)cStrTempA);
      else
        hRes = E_OUTOFMEMORY;
    }
  }
  if (SUCCEEDED(hRes))
  {
    cTest.cJsHttpServer.SetNewRequestObjectCallback(MX_BIND_CALLBACK(&OnNewRequestObject));
    cTest.cJsHttpServer.SetRequestCompletedCallback(MX_BIND_CALLBACK(&OnRequestCompleted));
    cTest.cJsHttpServer.SetRequireJsModuleCallback(MX_BIND_CALLBACK(&OnRequireJsModule));
    cTest.cJsHttpServer.SetWebSocketRequestReceivedCallback(MX_BIND_CALLBACK(&OnWebSocketRequestReceived));
  }
  if (SUCCEEDED(hRes))
  {
    MX::CSockets::CListenerOptions cOptions;

    if (bUseSSL != FALSE)
    {
      cTest.cJsHttpServer.SetQuerySslCertificatesCallback(MX_BIND_MEMBER_CALLBACK(&CJsTest::OnQuerySslCertificates,
                                                                                  &cTest));
    }

    //cOptions.dwBackLogSize = 0;
    cOptions.dwMaxAcceptsToPost = 16;
    //cOptions.dwMaxRequestsPerSecond = 0;
    //cOptions.dwBurstSize = 0;
    hRes = cTest.cJsHttpServer.StartListening(MX::CSockets::FamilyIPv4, (int)dwPort, &cOptions);
  }
  //----
  if (SUCCEEDED(hRes))
  {
    while (ShouldAbort() == FALSE)
      ::Sleep(100);
  }
  //done
  return (int)hRes;
}

static VOID OnEngineError(_In_ MX::CIpc *lpIpc, _In_ HRESULT hrErrorCode)
{
  return;
}

static HRESULT OnNewRequestObject(_In_ MX::CJsHttpServer *lpHttp, _Out_ MX::CJsHttpServer::CClientRequest **lplpRequest)
{
  *lplpRequest = MX_DEBUG_NEW CTestJsRequest();
  return ((*lplpRequest) != NULL) ? S_OK : E_OUTOFMEMORY;
}

static VOID OnRequestCompleted(_In_ MX::CJsHttpServer *lpHttp, _In_ MX::CJsHttpServer::CClientRequest *_lpRequest)
{
  CJsTest *lpJsTest = CONTAINING_RECORD(lpHttp, CJsTest, cJsHttpServer);
  CTestJsRequest *lpRequest = static_cast<CTestJsRequest*>(_lpRequest);
  LPCWSTR szExtensionW = NULL;
  HRESULT hRes;

  hRes = BuildWebFileName(lpRequest->cStrFileNameW, szExtensionW, lpRequest->GetUrl()->GetPath());
  if (FAILED(hRes))
  {
    hRes = lpRequest->SendErrorPage(500, hRes);
    lpRequest->End(hRes);
    return;
  }

  //check extension
  if (MX::StrCompareW(szExtensionW, L".jss", TRUE) == 0)
  {
    hRes = lpRequest->SetMimeTypeFromFileName(L".html");
    if (SUCCEEDED(hRes))
    {
      lpRequest->AddRef();
      hRes = lpJsTest->cTaskQueue.QueueTask(lpRequest, MX_BIND_CALLBACK(&OnProcessJsRequest));
      if (FAILED(hRes))
      {
        lpRequest->Release();
        hRes = lpRequest->SendErrorPage(500, hRes);
        lpRequest->End(hRes);
      }
    }

    return;
  }

  if (MX::StrCompareW(szExtensionW, L".css", TRUE) == 0 || MX::StrCompareW(szExtensionW, L".js", TRUE) == 0 ||
      MX::StrCompareW(szExtensionW, L".jpg", TRUE) == 0 || MX::StrCompareW(szExtensionW, L".png", TRUE) == 0 ||
      MX::StrCompareW(szExtensionW, L".gif", TRUE) == 0 || MX::StrCompareW(szExtensionW, L".dat", TRUE) == 0 ||
      MX::StrCompareW(szExtensionW, L".htm", TRUE) == 0 || MX::StrCompareW(szExtensionW, L".html", TRUE) == 0 ||
      MX::StrCompareW(szExtensionW, L".txt", TRUE) == 0)
  {
    hRes = lpRequest->SendFile((LPCWSTR)(lpRequest->cStrFileNameW));
    if (SUCCEEDED(hRes))
      hRes = lpRequest->SetMimeTypeFromFileName((LPCWSTR)(lpRequest->cStrFileNameW));
    if (hRes == MX_E_FileNotFound || hRes == MX_E_PathNotFound)
    {
      hRes = lpRequest->SendErrorPage(404, MX_E_FileNotFound);
    }

    lpRequest->End(hRes);
    return;
  }

  hRes = lpRequest->SendErrorPage(404, MX_E_FileNotFound);
  lpRequest->End(hRes);
  return;
}

static VOID OnProcessJsRequest(_In_ MX::CTaskQueue *lpQueue, _In_ MX::CTaskQueue::CTask *lpTask)
{
  CTestJsRequest *lpRequest = (CTestJsRequest*)lpTask;
  HRESULT hRes;

  hRes = (lpRequest->IsAlive() != FALSE) ? S_OK : MX_E_Cancelled;
  if (SUCCEEDED(hRes))
  {
    hRes = lpRequest->AttachJVM();
    if (SUCCEEDED(hRes))
    {
      MX::CJavascriptVM *lpJVM;
      BOOL bIsNew;

      lpJVM = lpRequest->GetVM(&bIsNew);

      if (bIsNew != FALSE)
      {
        hRes = MX::CJsHttpServerSessionPlugin::Register(*lpJVM);
        if (SUCCEEDED(hRes))
          hRes = MX::CJsMySqlPlugin::Register(*lpJVM);
        if (SUCCEEDED(hRes))
          hRes = MX::CJsSQLitePlugin::Register(*lpJVM);
        if (SUCCEEDED(hRes))
          hRes = MX::CJsonWebTokenPlugin::Register(*lpJVM);
        if (SUCCEEDED(hRes))
          hRes = lpJVM->AddNativeFunction("getExecutablePath", MX_BIND_CALLBACK(&OnGetExecutablePath), 0);
        if (SUCCEEDED(hRes))
          hRes = lpJVM->AddNativeFunction("getUniqueId", MX_BIND_CALLBACK(&OnGetUniqueId), 0);
      }

      if (SUCCEEDED(hRes))
      {
        CHAR szBufA[64];

        _snprintf_s(szBufA, MX_ARRAYLEN(szBufA), _TRUNCATE, "0x%p", lpRequest);
        hRes = lpJVM->AddStringProperty("requestAddress", szBufA,
                                        MX::CJavascriptVM::PropertyFlagEnumerable |
                                        MX::CJavascriptVM::PropertyFlagConfigurable);
      }

      if (SUCCEEDED(hRes))
      {
        MX::CStringA cStrCodeA;

        hRes = LoadTxtFile(cStrCodeA, (LPCWSTR)(lpRequest->cStrFileNameW));
        if (SUCCEEDED(hRes))
        {
          lpRequest->DisplayDebugInfoOnError(TRUE, TRUE);

          hRes = lpRequest->RunScript(cStrCodeA);
        }
        else if (hRes == MX_E_FileNotFound || hRes == MX_E_PathNotFound)
        {
          hRes = lpRequest->SendErrorPage(404, E_INVALIDARG);
        }
      }
    }
    else
    {
      hRes = lpRequest->SendErrorPage(500, hRes);
    }
  }
  else
  {
    hRes = lpRequest->SendErrorPage(500, hRes);
  }
  lpRequest->End(hRes);
  lpRequest->Release();
  return;
}

static HRESULT OnRequireJsModule(_In_ MX::CJsHttpServer *lpHttp, _In_ MX::CJsHttpServer::CClientRequest *_lpRequest,
                                 _Inout_ MX::CJavascriptVM &cJvm,
                                 _Inout_ MX::CJavascriptVM::CRequireModuleContext *lpReqContext,
                                 _Inout_ MX::CStringA &cStrCodeA)
{
  CTestJsRequest *lpRequest = static_cast<CTestJsRequest*>(_lpRequest);
  MX::CStringW cStrFileNameW;
  LPCWSTR szExtensionW;
  HRESULT hRes;

  if (MX::StrCompareW(lpReqContext->GetId(), L"session") == 0)
  {
    return lpRequest->InitializeSession(cJvm, lpReqContext);
  }
  //----
  hRes = BuildWebFileName(cStrFileNameW, szExtensionW, lpReqContext->GetId());
  if (FAILED(hRes))
    return hRes;

  //check extension
  if (MX::StrCompareW(szExtensionW, L".jss", TRUE) == 0)
  {
    hRes = LoadTxtFile(cStrCodeA, (LPCWSTR)cStrFileNameW);
    if (hRes == MX_HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
        hRes == MX_HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND))
    {
      hRes = lpRequest->SendErrorPage(404, E_INVALIDARG);
    }
    return hRes;
  }

  if (MX::StrCompareW(szExtensionW, L".js", TRUE) == 0)
  {
    hRes = LoadTxtFile(cStrCodeA, (LPCWSTR)cStrFileNameW);
    if (SUCCEEDED(hRes))
    {
      if (cStrCodeA.InsertN("<%", 0, 2) == FALSE || cStrCodeA.ConcatN("%>", 2) == FALSE)
        hRes = E_OUTOFMEMORY;
    }
    if (hRes == MX_HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
        hRes == MX_HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND))
    {
      hRes = lpRequest->SendErrorPage(404, E_INVALIDARG);
    }
    return hRes;
  }

  //not found
  return MX_E_NotFound;
}

static HRESULT OnWebSocketRequestReceived(_In_ MX::CJsHttpServer *lpHttp,
                                          _In_ MX::CJsHttpServer::CClientRequest *lpRequest, _In_ int nVersion,
                                          _In_opt_ LPCSTR *szProtocolsA, _In_ SIZE_T nProtocolsCount,
                                          _Out_ int &nSelectedProtocol, _In_ MX::TArrayList<int> &aSupportedVersions,
                                          _Out_ _Maybenull_ MX::CWebSocket **lplpWebSocket)
{
  nSelectedProtocol = 0;
  *lplpWebSocket = MX_DEBUG_NEW CTestJsHttpServerWebSocket();
  return ((*lplpWebSocket) != NULL) ? S_OK : E_OUTOFMEMORY;
}

static VOID DeleteSessionFiles()
{
  MX::CStringW cStrFileNameW;
  WIN32_FIND_DATAW sFdW;
  HANDLE hFindFile;
  SIZE_T nLen;
  HRESULT hRes;

  hRes = GetAppPath(cStrFileNameW);
  if (FAILED(hRes) || cStrFileNameW.Concat(L"session\\*") == FALSE)
    return;
  nLen = cStrFileNameW.GetLength() - 1;
  hFindFile = ::FindFirstFileW((LPCWSTR)cStrFileNameW, &sFdW);
  if (hFindFile != INVALID_HANDLE_VALUE)
  {
    do
    {
      if ((sFdW.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
      {
        cStrFileNameW.Delete(nLen, (SIZE_T)-1);
        if (cStrFileNameW.Concat(sFdW.cFileName) != FALSE)
        {
          if (::DeleteFileW((LPCWSTR)cStrFileNameW) == FALSE)
          {
            ::Sleep(10);
            ::SetFileAttributesW((LPCWSTR)cStrFileNameW, FILE_ATTRIBUTE_NORMAL);
            ::DeleteFileW((LPCWSTR)cStrFileNameW);
          }
        }
      }
    }
    while (::FindNextFileW(hFindFile, &sFdW) != FALSE);
    ::FindClose(hFindFile);
  }
  return;
}

#define delete_and_exit(_cmd) { _cmd; bDelete = TRUE; goto done; }
static HRESULT OnSessionPersistance(_In_ MX::CJsHttpServerSessionPlugin *lpPlugin,
                                    _In_ MX::CJsHttpServerSessionPlugin::ePersistanceOption nPersistanceOption)
{
  MX::CWindowsHandle cFileH;
  MX::CStringA cStrNameA, cStrValueA;
  MX::CStringW cStrFileNameW;
  DWORD dw, dwWritten, dwReaded, dwOsErr, dwPass;
  MX::CDateTime cDt, cDtNow;
  SIZE_T nLen, nIndex;
  LPCSTR szNameA, szValueA;
  union {
    BYTE aTempBuf[128];
    CHAR szBufA[128];
  };
  double nDbl;
  MX::CPropertyBag::eType nPropType;
  BOOL bDelete;
  HRESULT hRes;

  hRes = GetAppPath(cStrFileNameW);
  if (FAILED(hRes))
    return hRes;
  if (cStrFileNameW.Concat(L"session") == FALSE)
    return E_OUTOFMEMORY;
  ::CreateDirectoryW((LPCWSTR)cStrFileNameW, NULL);
  if (cStrFileNameW.Concat(L"\\") == FALSE || cStrFileNameW.Concat(lpPlugin->GetSessionId()) == FALSE)
    return E_OUTOFMEMORY;

  if (nPersistanceOption == MX::CJsHttpServerSessionPlugin::PersistanceOptionDelete)
  {
    ::DeleteFileW((LPCWSTR)cStrFileNameW);
    return S_OK;
  }

  //get current time
  hRes = cDtNow.SetFromNow(FALSE);
  if (FAILED(hRes))
    return hRes;

  //open file
  for (dwPass = 10; dwPass > 0; dwPass--)
  {
    if (nPersistanceOption == MX::CJsHttpServerSessionPlugin::PersistanceOptionLoad)
    {
      cFileH.Attach(::CreateFileW((LPCWSTR)cStrFileNameW, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, NULL));
    }
    else
    {
      cFileH.Attach(::CreateFileW((LPCWSTR)cStrFileNameW, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                  NULL));
    }
    if (cFileH)
    {
      dwOsErr = ERROR_SUCCESS;
      break;
    }
    dwOsErr = ::GetLastError();
    if (dwPass == 1 || (dwOsErr != ERROR_SHARING_VIOLATION && dwOsErr != ERROR_ACCESS_DENIED))
      break;
  }
  if (!cFileH)
    return MX_HRESULT_FROM_WIN32(dwOsErr);

  //read/write
  bDelete = FALSE;
  if (nPersistanceOption == MX::CJsHttpServerSessionPlugin::PersistanceOptionLoad)
  {
    //read date
    if (::ReadFile(cFileH, szBufA, 10+1+8, &dwReaded, NULL) == FALSE)
      delete_and_exit( hRes = MX_HRESULT_FROM_LASTERROR(); );
    if (dwReaded != 10+1+8)
      delete_and_exit( hRes = MX_E_ReadFault; );
    szBufA[10+1+8] = 0;

    //parse
    hRes = cDt.SetFromString(szBufA, "%Y/%m/%d %H:%M:%S");
    if (FAILED(hRes))
      delete_and_exit( ; );

    //compare with current time
    if (cDtNow < cDt)
      delete_and_exit( hRes = S_OK; );
    if (cDtNow.GetDiff(cDt, MX::CDateTime::UnitsHours) >= 1)
      delete_and_exit( hRes = S_OK; );

    //read fields
    while (1)
    {
      if (::ReadFile(cFileH, szBufA, 1, &dwReaded, NULL) == FALSE)
        return MX_HRESULT_FROM_LASTERROR();
      if (dwReaded != 1)
        delete_and_exit( hRes = MX_E_ReadFault; );
      if (szBufA[0] == '*')
        break;

      //check type
      if (szBufA[0] != 'N' && szBufA[0] != 'D' && szBufA[0] != 'S')
        delete_and_exit( hRes = MX_E_InvalidData; );

      //read name length
      if (::ReadFile(cFileH, &dw, 4, &dwReaded, NULL) == FALSE)
        delete_and_exit( hRes = MX_HRESULT_FROM_LASTERROR(); );
      if (dwReaded != 4)
        delete_and_exit( hRes = MX_E_ReadFault; );

      //check length
      if (dw > 512)
        delete_and_exit( hRes = MX_E_InvalidData; );

      //read name
      if (cStrNameA.EnsureBuffer((SIZE_T)(dw+1)) == FALSE)
        delete_and_exit( hRes = E_OUTOFMEMORY; );
      if (::ReadFile(cFileH, (LPSTR)cStrNameA, dw, &dwReaded, NULL) == FALSE)
        delete_and_exit( hRes = MX_HRESULT_FROM_LASTERROR() );
      if (dwReaded != dw)
        delete_and_exit( hRes = MX_E_ReadFault; );
      ((LPSTR)cStrNameA)[dw] = 0;
      cStrNameA.Refresh();

      //process value
      switch (szBufA[0])
      {
        case 'N': //a null value
          //add property
          hRes = lpPlugin->GetBag()->SetNull((LPCSTR)cStrNameA);
          if (FAILED(hRes))
            delete_and_exit( ; );
          break;

        case 'D': //a double value
          if (::ReadFile(cFileH, &nDbl, (DWORD)sizeof(double), &dwReaded, NULL) == FALSE)
            delete_and_exit( hRes = MX_HRESULT_FROM_LASTERROR() );
          if (dwReaded != (DWORD)sizeof(double))
            delete_and_exit( hRes = MX_E_ReadFault; );

          //add property
          hRes = lpPlugin->GetBag()->SetDouble((LPCSTR)cStrNameA, nDbl);
          if (FAILED(hRes))
            delete_and_exit( ; );
          break;

        case 'S': //a string value
          //read value length
          if (::ReadFile(cFileH, &dw, 4, &dwReaded, NULL) == FALSE)
            delete_and_exit( hRes = MX_HRESULT_FROM_LASTERROR(); );
          if (dwReaded != 4)
            delete_and_exit( hRes = MX_E_ReadFault; );

          //check length
          if (dw > 0x7FFFFFFFUL)
            delete_and_exit( hRes = MX_E_InvalidData; );

          //read value
          if (cStrValueA.EnsureBuffer((SIZE_T)(dw+1)) == FALSE)
            delete_and_exit( hRes = E_OUTOFMEMORY; );
          if (::ReadFile(cFileH, (LPSTR)cStrValueA, dw, &dwReaded, NULL) == FALSE)
            delete_and_exit( hRes = MX_HRESULT_FROM_LASTERROR());
          if (dwReaded != dw)
            delete_and_exit( hRes = MX_E_ReadFault; );
          ((LPSTR)cStrValueA)[dw] = 0;
          cStrValueA.Refresh();

          //add property
          hRes = lpPlugin->GetBag()->SetString((LPCSTR)cStrNameA, (LPCSTR)cStrValueA);
          if (FAILED(hRes))
            delete_and_exit(;);
          break;
      }
    }
  }
  else
  {
    //write date
    _snprintf_s(szBufA, _countof(szBufA), _TRUNCATE, "%04lu/%02lu/%02lu %02lu:%02lu:%02lu", cDtNow.GetYear(),
                cDtNow.GetMonth(), cDtNow.GetDay(), cDtNow.GetHours(), cDtNow.GetMinutes(), cDtNow.GetSeconds());
    if (::WriteFile(cFileH, szBufA, 10+1+8, &dwWritten, NULL) == FALSE)
      delete_and_exit( hRes = MX_HRESULT_FROM_LASTERROR(); );
    if (dwWritten != 10+1+8)
      delete_and_exit( hRes = MX_E_WriteFault; );

    //iterate each bag property
    for (nIndex=0; (szNameA=lpPlugin->GetBag()->GetAt(nIndex)) != NULL; nIndex++)
    {
      //get name length
      nLen = MX::StrLenA(szNameA);
      if (nLen >= 512)
        continue; //ignore long names

      //get property type
      nPropType = lpPlugin->GetBag()->GetType(nIndex);
      if (nPropType == MX::CPropertyBag::PropertyTypeNull)
        szBufA[0] = 'N';
      else if (nPropType == MX::CPropertyBag::PropertyTypeDouble)
        szBufA[0] = 'D';
      else if (nPropType == MX::CPropertyBag::PropertyTypeAnsiString)
        szBufA[0] = 'S';
      else
        continue;

      //write type
      if (::WriteFile(cFileH, szBufA, 1, &dwWritten, NULL) == FALSE)
        delete_and_exit( hRes = MX_HRESULT_FROM_LASTERROR(); );
      if (dwWritten != 1)
        delete_and_exit( hRes = MX_E_WriteFault; );

      //write name length
      dw = (DWORD)nLen;
      if (::WriteFile(cFileH, &dw, 4, &dwWritten, NULL) == FALSE)
        delete_and_exit( hRes = MX_HRESULT_FROM_LASTERROR(); );
      if (dwWritten != 4)
        delete_and_exit( hRes = MX_E_WriteFault; );

      //write name
      if (::WriteFile(cFileH, szNameA, dw, &dwWritten, NULL) == FALSE)
        delete_and_exit(MX_HRESULT_FROM_LASTERROR());
      if (dwWritten != dw)
        delete_and_exit( hRes = MX_E_WriteFault; );

      //write data
      switch (nPropType)
      {
        case MX::CPropertyBag::PropertyTypeDouble:
          //get value
          hRes = lpPlugin->GetBag()->GetDouble(nIndex, nDbl);
          if (FAILED(hRes))
            delete_and_exit( ; );

          //write value
          if (::WriteFile(cFileH, &nDbl, (DWORD)sizeof(nDbl), &dwWritten, NULL) == FALSE)
            delete_and_exit( hRes = MX_HRESULT_FROM_LASTERROR(); );
          if (dwWritten != (DWORD)sizeof(nDbl))
            delete_and_exit( hRes = MX_E_WriteFault; );
          break;

        case MX::CPropertyBag::PropertyTypeAnsiString:
          //get value
          hRes = lpPlugin->GetBag()->GetString(nIndex, szValueA);
          if (FAILED(hRes))
            delete_and_exit( ; );

          //write value length
          nLen = MX::StrLenA(szValueA);
          dw = (nLen < 0x7FFFFFFFUL) ? (DWORD)nLen : 0x7FFFFFFFUL; //truncate long values
          if (::WriteFile(cFileH, &dw, 4, &dwWritten, NULL) == FALSE)
            delete_and_exit( hRes = MX_HRESULT_FROM_LASTERROR(); );
          if (dwWritten != 4)
            delete_and_exit( hRes = MX_E_WriteFault; );

          //write value
          if (::WriteFile(cFileH, szValueA, dw, &dwWritten, NULL) == FALSE)
            delete_and_exit( hRes = MX_HRESULT_FROM_LASTERROR(); );
          if (dwWritten != dw)
            delete_and_exit( hRes = MX_E_WriteFault; );
          break;
      }
    }

    //write end of file mark
    if (::WriteFile(cFileH, "*", 1, &dwWritten, NULL) == FALSE)
      delete_and_exit( hRes = MX_HRESULT_FROM_LASTERROR(); );
    if (dwWritten != 1)
      delete_and_exit( hRes = MX_E_WriteFault; );
  }
  hRes = S_OK;

done:
  if (bDelete != FALSE)
  {
    cFileH.Close();
    ::DeleteFileW((LPCWSTR)cStrFileNameW);
  }
  //done
  return hRes;
}

static HRESULT LoadTxtFile(_Inout_ MX::CStringA &cStrContentsA, _In_z_ LPCWSTR szFileNameW)
{
  MX::CWindowsHandle cFileH;
  DWORD dw, dw2;

  cStrContentsA.Empty();
  if (szFileNameW == NULL)
    return E_POINTER;
  cFileH.Attach(::CreateFileW(szFileNameW, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              NULL));
  if (!cFileH)
    return MX_HRESULT_FROM_LASTERROR();
  dw = ::GetFileSize(cFileH, &dw2);
  if (dw == 0 || dw == DWORD_MAX || dw2 != 0)
    return MX_E_InvalidData;
  if (cStrContentsA.EnsureBuffer((SIZE_T)(dw+1)) == FALSE)
    return E_OUTOFMEMORY;
  if (::ReadFile(cFileH, (LPSTR)cStrContentsA, dw, &dw2, NULL) == FALSE)
    return MX_HRESULT_FROM_LASTERROR();
  if (dw != dw2)
    return MX_E_ReadFault;
  ((LPSTR)cStrContentsA)[dw] = 0;
  cStrContentsA.Refresh();
  return S_OK;
}

static HRESULT BuildWebFileName(_Inout_ MX::CStringW &cStrFullFileNameW, _Out_ LPCWSTR &szExtensionW,
                                _In_z_ LPCWSTR szPathW)
{
  LPWSTR sW;
  HRESULT hRes;

  szExtensionW = NULL;
  hRes = GetAppPath(cStrFullFileNameW);
  if (FAILED(hRes))
    return hRes;
  if (cStrFullFileNameW.Concat(L"Web\\") == FALSE)
    return E_OUTOFMEMORY;
  while (*szPathW == L'/')
    szPathW++;
  if (cStrFullFileNameW.Concat(szPathW) == FALSE)
    return FALSE;
  for (sW=(LPWSTR)cStrFullFileNameW; *sW!=0; sW++)
  {
    if (*sW == L'/')
      *sW = L'\\';
  }
  if (*(sW-1) == L'\\')
  {
    if (cStrFullFileNameW.Concat(L"index.jss") == FALSE)
      return E_OUTOFMEMORY;
  }
  //----
  sW = (LPWSTR)cStrFullFileNameW + (cStrFullFileNameW.GetLength() - 1);
  while (sW >= (LPWSTR)cStrFullFileNameW && *sW != L'.' && *sW != L'/')
    sW--;
  szExtensionW = (sW >= (LPWSTR)cStrFullFileNameW && *sW == L'.') ? sW : L"";
  return S_OK;
}

static DukTape::duk_ret_t OnGetExecutablePath(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                              _In_z_ LPCSTR szFunctionNameA)
{
  MX::CStringW cStrPathW;
  HRESULT hRes;

  hRes = GetAppPath(cStrPathW);
  if (FAILED(hRes))
    MX_JS_THROW_WINDOWS_ERROR(lpCtx, hRes);

  MX::CJavascriptVM::PushString(lpCtx, (LPCWSTR)cStrPathW, cStrPathW.GetLength());
  return 1;
}

static DukTape::duk_ret_t OnGetUniqueId(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                        _In_z_ LPCSTR szFunctionNameA)
{
  static LONG volatile nUniqueId = 0;

  DukTape::duk_push_int(lpCtx, (DukTape::duk_int_t)_InterlockedIncrement(&nUniqueId));
  return 1;
}

static HRESULT OnLog(_In_z_ LPCWSTR szInfoW)
{
  MX::CFastLock cLock(&nLogMutex);

  wprintf_s(L"%s\n", szInfoW);
  return S_OK;
}

//-----------------------------------------------------------

CTestJsHttpServerWebSocket::CTestJsHttpServerWebSocket() : MX::CWebSocket()
{
  return;
}

CTestJsHttpServerWebSocket::~CTestJsHttpServerWebSocket()
{
  return;
}

HRESULT CTestJsHttpServerWebSocket::OnConnected()
{
  static const LPCSTR szWelcomeMsgA = "Welcome to the MX JsHttpServer WebSocket demo!!!";
  HRESULT hRes;

  hRes = BeginTextMessage();
  if (SUCCEEDED(hRes))
    hRes = SendTextMessage(szWelcomeMsgA);
  if (SUCCEEDED(hRes))
    hRes = EndMessage();
  //done
  return hRes;
}

HRESULT CTestJsHttpServerWebSocket::OnTextMessage(_In_ LPCSTR szMsgA, _In_ SIZE_T nMsgLength)
{
  HRESULT hRes;

  hRes = BeginTextMessage();
  if (SUCCEEDED(hRes))
    hRes = SendTextMessage(szMsgA, nMsgLength);
  if (SUCCEEDED(hRes))
    hRes = EndMessage();
  //done
  return hRes;
}

HRESULT CTestJsHttpServerWebSocket::OnBinaryMessage(_In_ LPVOID lpData, _In_ SIZE_T nDataSize)
{
  HRESULT hRes;

  hRes = BeginBinaryMessage();
  if (SUCCEEDED(hRes))
    hRes = SendBinaryMessage(lpData, nDataSize);
  if (SUCCEEDED(hRes))
    hRes = EndMessage();
  //done
  return hRes;
}

SIZE_T CTestJsHttpServerWebSocket::GetMaxMessageSize() const
{
  return 1048596;
}

VOID CTestJsHttpServerWebSocket::OnPongFrame()
{
  return;
}

VOID CTestJsHttpServerWebSocket::OnCloseFrame(_In_ USHORT wCode, _In_  HRESULT hrErrorCode)
{
  return;
}
