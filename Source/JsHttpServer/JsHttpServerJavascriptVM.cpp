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
#include "JsHttpServerCommon.h"

//-----------------------------------------------------------

namespace MX {

CHttpServer::CRequest* CJsHttpServer::GetServerRequestFromContext(__in DukTape::duk_context *lpCtx)
{
  MX::CHttpServer::CRequest *lpRequest;

  try
  {
    DukTape::duk_push_global_object(lpCtx);
    DukTape::duk_get_prop_string(lpCtx, -1, INTERNAL_REQUEST_PROPERTY);
    if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
      lpRequest = reinterpret_cast<MX::CHttpServer::CRequest*>(DukTape::duk_to_pointer(lpCtx, -1));
    else
      lpRequest = NULL;
    DukTape::duk_pop_2(lpCtx);
  }
  catch (CJavascriptVM::CException&)
  {
    lpRequest = NULL;
  }
  return lpRequest;
}

HRESULT CJsHttpServer::InitializeJVM(__in CJavascriptVM &cJvm, __in CHttpServer::CRequest *lpRequest)
{
  CStringA cStrTempA, cStrTempA_2;
  CUrl *lpUrl;
  CHAR szBufA[32];
  TAutoRefCounted<CHttpBodyParserBase> cBodyParser;
  CSockets *lpSckMgr;
  HANDLE hConn;
  SOCKADDR_INET sAddr;
  SIZE_T i, nCount;
  HRESULT hRes;

  //init JVM
  cJvm.On(MX_BIND_MEMBER_CALLBACK(&CJsHttpServer::OnRequireJsModule, this));
  hRes = cJvm.Initialize();
  __EXIT_ON_ERROR(hRes);

  //store request pointer
  try
  {
    DukTape::duk_push_global_object(cJvm);
    DukTape::duk_push_pointer(cJvm, lpRequest);
    DukTape::duk_put_prop_string(cJvm, -2, INTERNAL_REQUEST_PROPERTY);
    DukTape::duk_pop(cJvm);
  }
  catch (CJavascriptVM::CException& ex)
  {
    return ex.GetErrorCode();
  }

  //register C++ objects
  hRes = Internals::CFileFieldJsObject::Register(cJvm, FALSE, FALSE);
  __EXIT_ON_ERROR(hRes);
  hRes = Internals::CRawBodyJsObject::Register(cJvm, FALSE, FALSE);
  __EXIT_ON_ERROR(hRes);

  //create main objects
  hRes = cJvm.CreateObject("request");
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddObjectStringProperty("request", "method", lpRequest->GetMethod(),
                                      CJavascriptVM::PropertyFlagEnumerable);
  __EXIT_ON_ERROR(hRes);
  lpUrl = lpRequest->GetUrl();
  //host
  hRes = Utf8_Encode(cStrTempA, lpUrl->GetHost());
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddObjectStringProperty("request", "host", (LPCSTR)cStrTempA, CJavascriptVM::PropertyFlagEnumerable);
  __EXIT_ON_ERROR(hRes);
  //port
  hRes = cJvm.AddObjectNumericProperty("request", "port", (double)(lpUrl->GetPort()),
                                       CJavascriptVM::PropertyFlagEnumerable);
  __EXIT_ON_ERROR(hRes);
  //path
  hRes = Utf8_Encode(cStrTempA, lpUrl->GetPath());
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddObjectStringProperty("request", "path", (LPCSTR)cStrTempA, CJavascriptVM::PropertyFlagEnumerable);
  __EXIT_ON_ERROR(hRes);

  //query strings
  hRes = cJvm.CreateObject("request.query");
  __EXIT_ON_ERROR(hRes);
  nCount = lpUrl->GetQueryStringCount();
  for (i=0; i<nCount; i++)
  {
    if (StrCompareW(lpUrl->GetQueryStringName(i), L"session_id") == 0)
      continue; //skip session id query
    hRes = Utf8_Encode(cStrTempA, lpUrl->GetQueryStringName(i));
    __EXIT_ON_ERROR(hRes);
    hRes = Utf8_Encode(cStrTempA_2, lpUrl->GetQueryStringValue(i));
    __EXIT_ON_ERROR(hRes);
    hRes = cJvm.AddObjectStringProperty("request.query", (LPCSTR)cStrTempA, (LPCSTR)cStrTempA_2,
                                        CJavascriptVM::PropertyFlagEnumerable);
    __EXIT_ON_ERROR(hRes);
  }

  //remote ip and port
  hRes = cJvm.CreateObject("AddressFamily");
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddObjectNumericProperty("AddressFamily", "IPV4", 1.0, CJavascriptVM::PropertyFlagEnumerable);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.AddObjectNumericProperty("AddressFamily", "IPV6", 2.0, CJavascriptVM::PropertyFlagEnumerable);
  __EXIT_ON_ERROR(hRes);

  lpSckMgr = lpRequest->GetUnderlyingSocketManager();
  hConn = lpRequest->GetUnderlyingSocket();
  if (hConn == NULL || lpSckMgr == NULL)
    return E_UNEXPECTED;
  hRes = lpSckMgr->GetPeerAddress(hConn, &sAddr);
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.CreateObject("request.remote");
  __EXIT_ON_ERROR(hRes);
  switch (sAddr.si_family)
  {
    case AF_INET:
      if (cStrTempA.Format("%lu.%lu.%lu.%lu", sAddr.Ipv4.sin_addr.S_un.S_un_b.s_b1,
                            sAddr.Ipv4.sin_addr.S_un.S_un_b.s_b2, sAddr.Ipv4.sin_addr.S_un.S_un_b.s_b3,
                            sAddr.Ipv4.sin_addr.S_un.S_un_b.s_b4) == FALSE)
        return E_OUTOFMEMORY;
      hRes = cJvm.AddObjectNumericProperty("request.remote", "family", 1.0, CJavascriptVM::PropertyFlagEnumerable);
      __EXIT_ON_ERROR(hRes);
      hRes = cJvm.AddObjectStringProperty("request.remote", "address", (LPCSTR)cStrTempA,
                                          CJavascriptVM::PropertyFlagEnumerable);
      __EXIT_ON_ERROR(hRes);
      hRes = cJvm.AddObjectNumericProperty("request.remote", "port", (double)htons(sAddr.Ipv4.sin_port),
                                            CJavascriptVM::PropertyFlagEnumerable);
      __EXIT_ON_ERROR(hRes);
      break;

    case AF_INET6:
      if (cStrTempA.Format("%4x:%4x:%4x:%4x:%4x:%4x:%4x:%4x", sAddr.Ipv6.sin6_addr.u.Word[0],
                           sAddr.Ipv6.sin6_addr.u.Word[1], sAddr.Ipv6.sin6_addr.u.Word[2],
                           sAddr.Ipv6.sin6_addr.u.Word[3], sAddr.Ipv6.sin6_addr.u.Word[4],
                           sAddr.Ipv6.sin6_addr.u.Word[5], sAddr.Ipv6.sin6_addr.u.Word[6],
                           sAddr.Ipv6.sin6_addr.u.Word[7]) == FALSE)
        return E_OUTOFMEMORY;
      for (i=0; i<8; i++)
      {
        szBufA[(i << 1)] = '0';
        szBufA[(i << 1) + 1] = ':';
      }
      szBufA[15] = 0; //--> "0:0:0:0:0:0:0:0"
      for (i=8; i>=2; i--)
      {
        LPCSTR sA = StrFindA((LPCSTR)cStrTempA, szBufA);
        if (sA != NULL)
        {
          if (i == 8) //special case for all values equal to zero
          {
            if (cStrTempA.Copy("::") == FALSE)
              return E_OUTOFMEMORY;
          }
          else if (sA == (LPCSTR)cStrTempA)
          {
            //the group of zeros are at the beginning
            cStrTempA.Delete(0, (i << 1) - 1);
            if (cStrTempA.InsertN(":", 0, 1) == FALSE)
              return E_OUTOFMEMORY;
          }

          else if (sA[(i << 1) - 1] == 0)
          {
            //the group of zeros are at the end
            cStrTempA.Delete((SIZE_T)(sA - (LPCSTR)cStrTempA), (i << 1) - 1);
            if (cStrTempA.ConcatN(":", 1) == FALSE)
              return E_OUTOFMEMORY;
          }
          else
          {
            //they are in the middle
            cStrTempA.Delete((SIZE_T)(sA - (LPCSTR)cStrTempA), (i << 1) - 1);
          }
          break;
        }
        szBufA[(i << 1) + 1 - 2] = 0;
      }
      hRes = cJvm.AddObjectNumericProperty("request.remote", "family", 2.0, CJavascriptVM::PropertyFlagEnumerable);
      __EXIT_ON_ERROR(hRes);
      hRes = cJvm.AddObjectStringProperty("request.remote", "address", (LPCSTR)cStrTempA,
                                          CJavascriptVM::PropertyFlagEnumerable);
      __EXIT_ON_ERROR(hRes);
      hRes = cJvm.AddObjectNumericProperty("request.remote", "port", (double)htons(sAddr.Ipv6.sin6_port),
                                            CJavascriptVM::PropertyFlagEnumerable);
      __EXIT_ON_ERROR(hRes);
      break;

    default:
      hRes = E_NOTIMPL;
      break;
  }

  //add request headers
  hRes = cJvm.CreateObject("request.headers");
  __EXIT_ON_ERROR(hRes);

  nCount = lpRequest->GetRequestHeadersCount();
  for (i=0; i<nCount; i++)
  {
    CHttpHeaderBase *lpHdr;

    lpHdr = lpRequest->GetRequestHeader(i);
    hRes = lpHdr->Build(cStrTempA);
    __EXIT_ON_ERROR(hRes);
    hRes = cJvm.AddObjectStringProperty("request.headers", lpHdr->GetName(), (LPCSTR)cStrTempA,
                                        CJavascriptVM::PropertyFlagEnumerable);
    __EXIT_ON_ERROR(hRes);
  }

  //add cookies
  hRes = cJvm.CreateObject("request.cookies");
  __EXIT_ON_ERROR(hRes);
  nCount = lpRequest->GetRequestCookiesCount();
  for (i=0; i<nCount; i++)
  {
    CHttpCookie *lpCookie;

    lpCookie = lpRequest->GetRequestCookie(i);
    if (cStrTempA.Copy(lpCookie->GetName()) == FALSE)
      return E_OUTOFMEMORY;
    for (LPSTR sA=(LPSTR)cStrTempA; *sA!=0; *sA++)
    {
      if (*sA == '.')
        *sA = '_';
    }
    hRes = cJvm.HasObjectProperty("request.cookies", (LPCSTR)cStrTempA);
    if (hRes == S_FALSE)
    {
      //don't add duplicates
      hRes = cJvm.AddObjectStringProperty("request.cookies", (LPCSTR)cStrTempA, lpCookie->GetValue(),
                                          CJavascriptVM::PropertyFlagEnumerable);
    }
    __EXIT_ON_ERROR(hRes);
  }

  //add body
  hRes = cJvm.CreateObject("request.post");
  __EXIT_ON_ERROR(hRes);
  hRes = cJvm.CreateObject("request.files");
  __EXIT_ON_ERROR(hRes);
  cBodyParser.Attach(lpRequest->GetRequestBodyParser());
  if (cBodyParser)
  {
    if (StrCompareA(cBodyParser->GetType(), "default") == 0)
    {
      CHttpBodyParserDefault *lpParser = (CHttpBodyParserDefault*)(cBodyParser.Get());
      Internals::CRawBodyJsObject *lpJsObj;

      hRes = lpParser->ToString(cStrTempA);
      __EXIT_ON_ERROR(hRes);
      lpJsObj = MX_DEBUG_NEW Internals::CRawBodyJsObject(NULL);
      if (!lpJsObj)
        return E_OUTOFMEMORY;
      lpJsObj->Initialize(lpParser);
      hRes = cJvm.AddObjectJsObjectProperty("request", "rawbody", lpJsObj, CJavascriptVM::PropertyFlagEnumerable);
      lpJsObj->Release();
      __EXIT_ON_ERROR(hRes);
    }
    else if (StrCompareA(cBodyParser->GetType(), "form") == 0)
    {
      CHttpBodyParserFormBase *lpParser = (CHttpBodyParserFormBase*)(cBodyParser.Get());

      //add post fields
      nCount = lpParser->GetFieldsCount();
      for (i=0; i<nCount; i++)
      {
        hRes = InsertPostField(cJvm, lpParser->GetField(i), "request.post");
        __EXIT_ON_ERROR(hRes);
      }
      //add files fields
      nCount = lpParser->GetFileFieldsCount();
      for (i=0; i<nCount; i++)
      {
        hRes = InsertPostFileField(cJvm, lpParser->GetFileField(i), "request.files");
        __EXIT_ON_ERROR(hRes);
      }
    }
  }

  //response functions
  hRes = Internals::JsHttpServer::AddResponseMethods(cJvm, lpRequest);
  __EXIT_ON_ERROR(hRes);

  //helper functions
  hRes = Internals::JsHttpServer::AddHelpersMethods(cJvm, lpRequest);
  __EXIT_ON_ERROR(hRes);

  //done
  return S_OK;
}

HRESULT CJsHttpServer::TransformJavascriptCode(__inout MX::CStringA &cStrCodeA)
{
  typedef enum {
    CodeModeNone=0, CodeModeInCode, CodeModeInCodePrint
  } eCodeMode;
  LPCSTR sA;
  CHAR chQuoteA;
  SIZE_T k, nCurrPos, nNonCodeBlockStart;
  eCodeMode nCodeMode, nOrigCodeMode;
  HRESULT hRes;

  //remove UTF-8 BOM if any
  if (cStrCodeA.GetLength() >= 3)
  {
    sA = (LPCSTR)cStrCodeA;
    if (sA[0] == 0xEF && sA[1] == 0xBB && sA[2] == 0xBF)
      cStrCodeA.Delete(0, 3);
  }
  //locate for '<%... %>' blocks outside of quotes
  nNonCodeBlockStart = 0;
  chQuoteA = 0;
  nCodeMode = CodeModeNone;
  sA = (LPCSTR)cStrCodeA;
  while (*sA != 0)
  {
    if (chQuoteA == 0)
    {
      if (*sA == '\\' && (sA[1] == '"' || sA[1] == '\'' || sA[1] == '\\'))
      {
        sA += 2;
      }
      else if (nCodeMode != CodeModeNone && (*sA == '"' || *sA == '\''))
      {
        //only process strings inside JS code
        chQuoteA = *sA++;
      }
      else if (nCodeMode == CodeModeNone && *sA == '<' && sA[1] == '%')
      {
        nCodeMode = (sA[2] != '=') ? CodeModeInCode : CodeModeInCodePrint;
        nCurrPos = (SIZE_T)(sA - (LPCSTR)cStrCodeA);
        //remove tag
        if (nCodeMode == CodeModeInCode)
        {
          //if after tag there are only spaces/tabs and the new line then remove them too
          for (k=2; sA[k]==' ' || sA[k]=='\t'; k++);
          if (sA[k] == 0)
            cStrCodeA.Delete(nCurrPos, k);
          else if (sA[k] == '\n')
            cStrCodeA.Delete(nCurrPos, k+1);
          else if (sA[k] == '\r')
            cStrCodeA.Delete(nCurrPos, (sA[k] == '\n') ? (k+2) : (k+1));
          else
            cStrCodeA.Delete(nCurrPos, 2);
        }
        else
        {
          cStrCodeA.Delete(nCurrPos, 3);
        }
        //convert code from 'nNonCodeBlockStart' to 'nCurrPos' to a print function
        hRes = TransformJavascriptCode_ConvertToPrint(cStrCodeA, nNonCodeBlockStart, nCurrPos);
        if (FAILED(hRes))
          return hRes;
        //insert "echo("
        if (nCodeMode == CodeModeInCodePrint)
        {
          if (cStrCodeA.InsertN("echo(htmlentities(", nCurrPos, 18) == FALSE)
            return E_OUTOFMEMORY;
          nCurrPos += 18;
        }
        //reset code pointer
        sA = (LPCSTR)cStrCodeA + nCurrPos;
      }
      else if (nCodeMode != CodeModeNone && *sA == '%' && sA[1] == '>')
      {
        nOrigCodeMode = nCodeMode;
        nCodeMode = CodeModeNone;
        nNonCodeBlockStart = (SIZE_T)(sA - (LPCSTR)cStrCodeA);
        //remove the tag and, if after it, there are only spaces/tabs and the new line then remove them too
        for (k=2; sA[k]==' ' || sA[k]=='\t'; k++);
        if (sA[k] == 0)
          cStrCodeA.Delete(nNonCodeBlockStart, k);
        else if (sA[k] == '\n')
          cStrCodeA.Delete(nNonCodeBlockStart, k+1);
        else if (sA[k] == '\r')
          cStrCodeA.Delete(nNonCodeBlockStart, (sA[k] == '\n') ? (k+2) : (k+1));
        else
          cStrCodeA.Delete(nNonCodeBlockStart, 2);
        //close echo if print mode
        if (nOrigCodeMode == CodeModeInCodePrint)
        {
          if (cStrCodeA.InsertN("));", nNonCodeBlockStart, 3) == FALSE)
            return E_OUTOFMEMORY;
          nNonCodeBlockStart += 3;
        }
        //reset code pointer
        sA = (LPCSTR)cStrCodeA + nNonCodeBlockStart;
      }
      else if (nCodeMode == CodeModeInCode && *sA == '/' && sA[1] == '/')
      {
        sA += 2;
        //skip line comment
        while (*sA != 0 && *sA != '\r' && *sA != '\n')
          sA++;
        while (*sA == '\r' || *sA == '\n')
          sA++;
      }
      else if (nCodeMode == CodeModeInCode && *sA == '/' && sA[1] == '*')
      {
        sA += 2;
        //skip multi-line comment
        while (*sA != 0 && (*sA != '*' || sA[1] != '/'))
          sA++;
        if (*sA != 0)
          sA += 2;
      }
      else
      {
        sA++;
      }
    }
    else
    {
      if (*sA == '\\' && (sA[1] == chQuoteA || sA[1] == '\\'))
      {
        sA += 2;
      }
      else
      {
        if (*sA == chQuoteA)
          chQuoteA = 0;
        *sA++;
      }
    }
  }
  //must end in non-code mode
  if (nCodeMode != CodeModeNone)
    return E_UNEXPECTED;

  //convert final block
  nCurrPos = (SIZE_T)(sA - (LPCSTR)cStrCodeA);
  //convert code from 'nNonCodeBlockStart' to 'nCurrPos' to a print function
  hRes = TransformJavascriptCode_ConvertToPrint(cStrCodeA, nNonCodeBlockStart, nCurrPos);
  //done
  return hRes;
}

HRESULT CJsHttpServer::TransformJavascriptCode_ConvertToPrint(__inout MX::CStringA &cStrCodeA,
                                                              __inout SIZE_T &nNonCodeBlockStart,
                                                              __inout SIZE_T &nCurrPos)
{
  LPCSTR sA;

  if (nNonCodeBlockStart < nCurrPos)
  {
    //insert a call to 'echo' method
    if (cStrCodeA.Insert("echo(\"", nNonCodeBlockStart) == FALSE)
      return E_OUTOFMEMORY;
    nNonCodeBlockStart += 4+2;
    nCurrPos += 4+2;
    //escape quotes and convert control chars
    sA = (LPCSTR)cStrCodeA + nNonCodeBlockStart;
    while (nNonCodeBlockStart < nCurrPos)
    {
      switch (*sA)
      {
        case '"':
        case '\r':
        case '\n':
        case '\t':
          switch (*sA)
          {
            case '\r':
              *((LPSTR)sA) = 'r';
              break;
            case '\n':
              *((LPSTR)sA) = 'n';
              break;
            case '\t':
              *((LPSTR)sA) = 't';
              break;
          }
          if (cStrCodeA.InsertN("\\", nNonCodeBlockStart, 1) == FALSE)
            return E_OUTOFMEMORY;
          nNonCodeBlockStart += 2;
          nCurrPos++;
          sA = (LPCSTR)cStrCodeA + nNonCodeBlockStart;
          break;

        default:
          if (*((LPBYTE)sA) < 32)
          {
            cStrCodeA.Delete(nNonCodeBlockStart, 1);
            nCurrPos--;
          }
          else
          {
            sA++;
            nNonCodeBlockStart++;
          }
          break;
      }
    }
    //close call to 'print'
    if (cStrCodeA.Insert("\");", nCurrPos) == FALSE)
      return E_OUTOFMEMORY;
    nCurrPos += 3;
  }
  return S_OK;
}

HRESULT CJsHttpServer::InsertPostField(__in CJavascriptVM &cJvm, __in CHttpBodyParserFormBase::CField *lpField,
                                       __in LPCSTR szBaseObjectNameA)
{
  CStringA cStrNameA;
  SIZE_T i, nCount;
  HRESULT hRes;

  hRes = Utf8_Encode(cStrNameA, lpField->GetName());
  __EXIT_ON_ERROR(hRes);
  nCount = lpField->GetSubIndexesCount();
  if (nCount > 0)
  {
    if (cStrNameA.InsertN(".", 0, 1) == FALSE || cStrNameA.Insert(szBaseObjectNameA, 0) == FALSE)
      return E_OUTOFMEMORY;
    hRes = cJvm.CreateObject((LPCSTR)cStrNameA);
    if (hRes == MX_E_AlreadyExists)
      hRes = S_OK;
    for (i=0; SUCCEEDED(hRes) && i<nCount; i++)
      hRes = InsertPostField(cJvm, lpField->GetSubIndexAt(i), (LPCSTR)cStrNameA);
  }
  else
  {
    CStringA cStrValueA;

    hRes = Utf8_Encode(cStrValueA, lpField->GetValue());
    __EXIT_ON_ERROR(hRes);

    hRes = cJvm.AddObjectStringProperty(szBaseObjectNameA, (LPCSTR)cStrNameA, (LPCSTR)cStrValueA,
                                        CJavascriptVM::PropertyFlagEnumerable);
  }
  return hRes;
}

HRESULT CJsHttpServer::InsertPostFileField(__in CJavascriptVM &cJvm,
                                           __in CHttpBodyParserFormBase::CFileField *lpFileField,
                                           __in LPCSTR szBaseObjectNameA)
{
  CStringA cStrNameA;
  SIZE_T i, nCount;
  HRESULT hRes;

  hRes = Utf8_Encode(cStrNameA, lpFileField->GetName());
  __EXIT_ON_ERROR(hRes);
  nCount = lpFileField->GetSubIndexesCount();
  if (nCount > 0)
  {
    if (cStrNameA.InsertN(".", 0, 1) == FALSE || cStrNameA.Insert(szBaseObjectNameA, 0) == FALSE)
      return E_OUTOFMEMORY;
    hRes = cJvm.CreateObject((LPCSTR)cStrNameA);
    if (hRes == MX_E_AlreadyExists)
      hRes = S_OK;
    for (i=0; SUCCEEDED(hRes) && i<nCount; i++)
      hRes = InsertPostFileField(cJvm, lpFileField->GetSubIndexAt(i), (LPCSTR)cStrNameA);
  }
  else
  {
    Internals::CFileFieldJsObject *lpJsObj;

    lpJsObj = MX_DEBUG_NEW Internals::CFileFieldJsObject(cJvm);
    if (!lpJsObj)
      return E_OUTOFMEMORY;
    lpJsObj->Initialize(lpFileField);
    hRes = cJvm.AddObjectJsObjectProperty(szBaseObjectNameA, (LPCSTR)cStrNameA, lpJsObj,
                                          CJavascriptVM::PropertyFlagEnumerable);
    lpJsObj->Release();
  }
  return hRes;
}

} //namespace MX
