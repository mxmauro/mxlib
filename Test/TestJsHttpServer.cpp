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
#include "TestJsHttpServer.h"
#include <JsHttpServer\JsHttpServer.h>
#include <JsHttpServer\Plugins\JsHttpServerSessionPlugin.h>
#include <JsLib\Plugins\JsMySqlPlugin.h>
#include <JsLib\Plugins\JsSQLitePlugin.h>

//-----------------------------------------------------------

class CTestJsRequest : public MX::CJsHttpServer::CJsRequest
{
public:
  CTestJsRequest() : MX::CJsHttpServer::CJsRequest()
    { };

  VOID Reset()
    {
    cSessionJsObj.Release();
    return;
    };

public:
  MX::TAutoRefCounted<MX::CJsHttpServerSessionPlugin> cSessionJsObj;
};

//-----------------------------------------------------------

static VOID OnEngineError(_In_ MX::CIpc *lpIpc, _In_ HRESULT hErrorCode);
static HRESULT OnNewRequestObject(_Out_ MX::CJsHttpServer::CJsRequest **lplpRequest);
static HRESULT OnRequest(_In_ MX::CJsHttpServer *lpHttp, _In_ MX::CJsHttpServer::CJsRequest *lpRequest,
                         _Inout_ MX::CJavascriptVM &cJvm, _Inout_ MX::CStringA &cStrCodeA);
static HRESULT OnRequireJsModule(_In_ MX::CJsHttpServer *lpHttp, _In_ MX::CJsHttpServer::CJsRequest *lpRequest,
                                 _Inout_ MX::CJavascriptVM &cJvm,
                                 _Inout_ MX::CJavascriptVM::CRequireModuleContext *lpReqContext,
                                 _Inout_ MX::CStringA &cStrCodeA);
static VOID OnError(_In_ MX::CJsHttpServer *lpHttp, _In_ MX::CJsHttpServer::CJsRequest *lpRequest,
                    _In_ HRESULT hErrorCode);
static VOID DeleteSessionFiles();
static HRESULT OnSessionLoadSave(_In_ MX::CJsHttpServerSessionPlugin *lpPlugin, _In_ BOOL bLoading);
static HRESULT LoadTxtFile(_Inout_ MX::CStringA &cStrContentsA, _In_z_ LPCWSTR szFileNameW);
static HRESULT BuildWebFileName(_Inout_ MX::CStringW &cStrFullFileNameW, _Out_ LPCWSTR &szExtensionW,
                                _In_z_ LPCWSTR szPathW);
static DukTape::duk_ret_t OnGetExecutablePath(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                              _In_z_ LPCSTR szFunctionNameA);

//-----------------------------------------------------------

int TestJsHttpServer(_In_ BOOL bUseSSL)
{
  MX::CIoCompletionPortThreadPool cDispatcherPool, cWorkerPool;
  MX::CSockets cSckMgr(cDispatcherPool);
  MX::CJsHttpServer cJsHttpServer(cSckMgr, cWorkerPool);
  MX::CSslCertificate cSslCert;
  MX::CCryptoRSA cSslPrivateKey;
  HRESULT hRes;

  DeleteSessionFiles();

  cWorkerPool.SetOption_MinThreadsCount(4);
  cSckMgr.SetOption_MaxAcceptsToPost(24);
  //cSckMgr.SetOption_PacketSize(16384);
  cJsHttpServer.SetOption_MaxFilesCount(10);

  hRes = cDispatcherPool.Initialize();
  if (SUCCEEDED(hRes))
    hRes = cWorkerPool.Initialize();
  if (SUCCEEDED(hRes))
  {
    cSckMgr.On(MX_BIND_CALLBACK(&OnEngineError));
    hRes = cSckMgr.Initialize();
  }
  if (SUCCEEDED(hRes) && bUseSSL != FALSE)
  {
    MX::CStringA cStrTempA;
    MX::CStringW cStrTempW;

    //load SSL certificate
    hRes = GetAppPath(cStrTempW);
    if (SUCCEEDED(hRes) && cStrTempW.Concat(L"Web\\Certificates\\ssl.crt") == FALSE)
      hRes = E_OUTOFMEMORY;
    if (SUCCEEDED(hRes))
      hRes = LoadTxtFile(cStrTempA, (LPCWSTR)cStrTempW);
    if (SUCCEEDED(hRes))
      hRes = cSslCert.InitializeFromPEM((LPCSTR)cStrTempA);
    //load private key
    if (SUCCEEDED(hRes))
      hRes = GetAppPath(cStrTempW);
    if (SUCCEEDED(hRes) && cStrTempW.Concat(L"Web\\Certificates\\ssl.key") == FALSE)
      hRes = E_OUTOFMEMORY;
    if (SUCCEEDED(hRes))
      hRes = LoadTxtFile(cStrTempA, (LPCWSTR)cStrTempW);
    if (SUCCEEDED(hRes))
      hRes = cSslPrivateKey.SetPrivateKeyFromPEM((LPCSTR)cStrTempA);
  }
  if (SUCCEEDED(hRes))
  {
    cJsHttpServer.On(MX_BIND_CALLBACK(&OnNewRequestObject));
    cJsHttpServer.On(MX_BIND_CALLBACK(&OnRequest));
    cJsHttpServer.On(MX_BIND_CALLBACK(&OnRequireJsModule));
    cJsHttpServer.On(MX_BIND_CALLBACK(&OnError));
  }
  if (SUCCEEDED(hRes))
  {
    if (bUseSSL != FALSE)
      hRes = cJsHttpServer.StartListening(443, MX::CIpcSslLayer::ProtocolTLSv1_2, &cSslCert, &cSslPrivateKey);
    else
      hRes = cJsHttpServer.StartListening(80);
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

static VOID OnEngineError(_In_ MX::CIpc *lpIpc, _In_ HRESULT hErrorCode)
{
  return;
}

static HRESULT OnNewRequestObject(_Out_ MX::CJsHttpServer::CJsRequest **lplpRequest)
{
  *lplpRequest = MX_DEBUG_NEW CTestJsRequest();
  return ((*lplpRequest) != NULL) ? S_OK : E_OUTOFMEMORY;
}

static HRESULT OnRequest(_In_ MX::CJsHttpServer *lpHttp, _In_ MX::CJsHttpServer::CJsRequest *_lpRequest,
                         _Inout_ MX::CJavascriptVM &cJvm, _Inout_ MX::CStringA &cStrCodeA)
{
  CTestJsRequest *lpRequest = static_cast<CTestJsRequest*>(_lpRequest);
  MX::CStringW cStrFileNameW;
  LPCWSTR szExtensionW;
  HRESULT hRes;

  lpRequest->Reset();

  hRes = BuildWebFileName(cStrFileNameW, szExtensionW, lpRequest->GetUrl()->GetPath());
  if (FAILED(hRes))
    return hRes;
  //check extension
  if (MX::StrCompareW(szExtensionW, L".jss", TRUE) == 0)
  {
    hRes = LoadTxtFile(cStrCodeA, (LPCWSTR)cStrFileNameW);
    if (SUCCEEDED(hRes))
      hRes = MX::CJsHttpServerSessionPlugin::Register(cJvm);
    if (SUCCEEDED(hRes))
      hRes = MX::CJsMySqlPlugin::Register(cJvm);
    if (SUCCEEDED(hRes))
      hRes = MX::CJsSQLitePlugin::Register(cJvm);
    if (SUCCEEDED(hRes))
      hRes = cJvm.AddNativeFunction("getExecutablePath", MX_BIND_CALLBACK(&OnGetExecutablePath), 0);
    if (hRes == MX_E_FileNotFound || hRes == MX_E_PathNotFound)
    {
      hRes = lpRequest->SendErrorPage(404, E_INVALIDARG);
    }
  }
  else if (MX::StrCompareW(szExtensionW, L".css", TRUE) == 0 ||
           MX::StrCompareW(szExtensionW, L".js", TRUE) == 0 ||
           MX::StrCompareW(szExtensionW, L".jpg", TRUE) == 0 ||
           MX::StrCompareW(szExtensionW, L".png", TRUE) == 0 ||
           MX::StrCompareW(szExtensionW, L".gif", TRUE) == 0 ||
           MX::StrCompareW(szExtensionW, L".dat", TRUE) == 0)
  {
    hRes = lpRequest->SendFile((LPCWSTR)cStrFileNameW);
    if (hRes == MX_HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
        hRes == MX_HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND))
    {
      hRes = lpRequest->SendErrorPage(404, E_INVALIDARG);
    }
  }
  else
  {
    hRes = lpRequest->SendErrorPage(404, E_INVALIDARG);
  }
  return hRes;
}

static HRESULT OnRequireJsModule(_In_ MX::CJsHttpServer *lpHttp, _In_ MX::CJsHttpServer::CJsRequest *_lpRequest,
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
    if (!(lpRequest->cSessionJsObj))
    {
      lpRequest->cSessionJsObj.Attach(MX_DEBUG_NEW MX::CJsHttpServerSessionPlugin(cJvm));
      if (!(lpRequest->cSessionJsObj))
        return E_OUTOFMEMORY;
      hRes = lpRequest->cSessionJsObj->Setup(lpRequest, MX_BIND_CALLBACK(&OnSessionLoadSave), NULL, NULL, L"/",
                                             2*60*60, FALSE, TRUE);
      if (FAILED(hRes))
        return hRes;
    }
    hRes = lpReqContext->ReplaceModuleExportsWithObject(lpRequest->cSessionJsObj);
    //done
    return hRes;
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

static VOID OnError(_In_ MX::CJsHttpServer *lpHttp, _In_ MX::CJsHttpServer::CJsRequest *lpRequest,
                    _In_ HRESULT hErrorCode)
{
  return;
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
static HRESULT OnSessionLoadSave(_In_ MX::CJsHttpServerSessionPlugin *lpPlugin, _In_ BOOL bLoading)
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
  if (cStrFileNameW.Concat(L"\\") == FALSE ||
      cStrFileNameW.Concat(lpPlugin->GetSessionId()) == FALSE)
    return E_OUTOFMEMORY;
  //get current time
  hRes = cDtNow.SetFromNow(FALSE);
  if (FAILED(hRes))
    return hRes;
  //open file
  for (dwPass=10; dwPass>0; dwPass--)
  {
    if (bLoading != FALSE)
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
  {
    return MX_HRESULT_FROM_WIN32(dwOsErr);
  }
  //read/write
  bDelete = FALSE;
  if (bLoading != FALSE)
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
