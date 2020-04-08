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
#include "JsHttpServerCommon.h"
#include "..\..\include\Http\HtmlEntities.h"
#include "..\..\Include\AtomicOps.h"

//-----------------------------------------------------------

static HRESULT TransformJavascriptCode(_Inout_ MX::CStringA &cStrCodeA);
static BOOL TransformJavascriptCode_ConvertToPrint(_Inout_ MX::CStringA &cStrCodeA, _Inout_ SIZE_T nNonCodeBlockStart,
                                                   _Inout_ SIZE_T &nCurrPos);

//-----------------------------------------------------------

namespace MX {

CJsHttpServer::CClientRequest::CClientRequest() : CHttpServer::CClientRequest()
{
  lpJsHttpServer = NULL;
  lpJVM = NULL;
  _InterlockedExchange(&nFlags, 0);
  return;
}

CJsHttpServer::CClientRequest::~CClientRequest()
{
  MX_DELETE(lpJVM);
  return;
}

HRESULT CJsHttpServer::CClientRequest::OnSetup()
{
  return CHttpServer::CClientRequest::OnSetup();
}

BOOL CJsHttpServer::CClientRequest::OnCleanup()
{
  FreeJVM();
  return CHttpServer::CClientRequest::OnCleanup();
}

HRESULT CJsHttpServer::CClientRequest::AttachJVM()
{
  BOOL bIsNew;
  HRESULT hRes;

  //initialize javascript engine
  hRes = cJvmManager->AllocAndInitVM(&lpJVM, bIsNew, cRequireJsModuleCallback, this);
  if (SUCCEEDED(hRes))
  {
    if (bIsNew != FALSE)
      _InterlockedOr(&nFlags, __REQUEST_FLAGS_IsNew);
    else
      _InterlockedAnd(&nFlags, ~__REQUEST_FLAGS_IsNew);
  }

  //done
  return hRes;
}

VOID CJsHttpServer::CClientRequest::DiscardVM()
{
  _InterlockedOr(&nFlags, __REQUEST_FLAGS_DiscardVM);
  return;
}

CJavascriptVM* CJsHttpServer::CClientRequest::GetVM(_Out_opt_ LPBOOL lpbIsNew) const
{
  if (lpbIsNew != NULL)
  {
    *lpbIsNew = ((__InterlockedRead(const_cast<LONG volatile*>(&nFlags)) & __REQUEST_FLAGS_IsNew) != 0) ? TRUE : FALSE;
  }
  return lpJVM;
}

HRESULT CJsHttpServer::CClientRequest::RunScript(_In_ LPCSTR szCodeA)
{
  MX::CStringA cStrTransformedCodeA;
  HRESULT hRes;

  if (szCodeA == NULL)
    return E_POINTER;
  if (lpJVM == NULL)
    return MX_E_NotReady;

  //quick exit if nothing to run
  if (*szCodeA == 0)
    return S_OK;

  //transform code
  if (cStrTransformedCodeA.Copy(szCodeA) == FALSE)
    return E_OUTOFMEMORY;
  hRes = TransformJavascriptCode(cStrTransformedCodeA);
  if (FAILED(hRes))
    return hRes;

  //quick exit if nothing to run
  if (*szCodeA == 0)
    return S_OK;

  if (cStrTransformedCodeA.InsertN("var _code = (function() {  'use strict';  ", 0, 42) == FALSE ||
      cStrTransformedCodeA.ConcatN("\r\n});\r\n_code();\r\ndelete _code;\r\n", 32) == FALSE)
  {
    return E_OUTOFMEMORY;
  }

  //execute JS code
  try
  {
    lpJVM->Run(cStrTransformedCodeA, GetUrl()->GetPath());
  }
  catch (CJsHttpServerSystemExit &e)
  {
    if (*(e.GetDescription()) != 0)
    {
      CStringA cStrBodyA;

      hRes = ResetResponse();
      if (SUCCEEDED(hRes))
        hRes = DisableClientCache();
      if (SUCCEEDED(hRes))
      {
        if (cStrBodyA.Format("Error: %s", e.GetDescription()) != FALSE)
        {
          hRes = HtmlEntities::ConvertTo(cStrBodyA);
          if (SUCCEEDED(hRes))
          {
            if (cStrBodyA.InsertN("<html><body><pre>", 0, 6 + 6 + 5) != FALSE &&
                cStrBodyA.ConcatN("</pre></body></html>", 6 + 7 + 7) != FALSE)
            {
              hRes = SendResponse((LPCSTR)cStrBodyA, cStrBodyA.GetLength());
            }
            else
            {
              hRes = E_OUTOFMEMORY;
            }
          }
        }
        else
        {
          hRes = E_OUTOFMEMORY;
        }
      }
    }
  }
  catch (CJsWindowsError &e)
  {
    hRes = ResetResponse();
    if (SUCCEEDED(hRes))
      hRes = DisableClientCache();
    if (SUCCEEDED(hRes))
    {
      BOOL bShowFileNameAndLine = ((__InterlockedRead(&nFlags) & __REQUEST_FLAGS_DebugShowFileNameAndLine) != 0);
      BOOL bShowStack = ((__InterlockedRead(&nFlags) & __REQUEST_FLAGS_DebugShowStack) != 0);

      hRes = BuildErrorPage(e.GetHResult(), e.GetDescription(),
                            ((bShowFileNameAndLine != FALSE) ? e.GetFileName() : NULL),
                            ((bShowFileNameAndLine != FALSE) ? e.GetLineNumber() : 0),
                            ((bShowStack != FALSE) ? e.GetStackTrace() : NULL));
    }
  }
  catch (CJsError &e)
  {
    hRes = ResetResponse();
    if (SUCCEEDED(hRes))
      hRes = DisableClientCache();
    if (SUCCEEDED(hRes))
    {
      BOOL bShowFileNameAndLine = ((__InterlockedRead(&nFlags) & __REQUEST_FLAGS_DebugShowFileNameAndLine) != 0);
      BOOL bShowStack = ((__InterlockedRead(&nFlags) & __REQUEST_FLAGS_DebugShowStack) != 0);

      hRes = BuildErrorPage(E_FAIL, e.GetDescription(),
                            ((bShowFileNameAndLine != FALSE) ? e.GetFileName() : NULL),
                            ((bShowFileNameAndLine != FALSE) ? e.GetLineNumber() : 0),
                            ((bShowStack != FALSE) ? e.GetStackTrace() : NULL));
    }
  }
  catch (...)
  {
    hRes = MX_E_UnhandledException;
  }

  //done
  return hRes;
}

VOID CJsHttpServer::CClientRequest::FreeJVM()
{
  if (lpJVM != NULL)
  {
    if ((__InterlockedRead(&nFlags) & __REQUEST_FLAGS_DiscardVM) == 0)
    {
      if (SUCCEEDED(lpJVM->Reset()))
      {
        cJvmManager->FreeVM(lpJVM);
      }
      else
      {
        delete lpJVM;
      }
    }
    else
    {
      delete lpJVM;
    }
    lpJVM = NULL;
  }
  return;
}

HRESULT CJsHttpServer::CClientRequest::BuildErrorPage(_In_ HRESULT hr, _In_opt_z_ LPCSTR szDescriptionA,
                                                      _In_z_ LPCSTR szFileNameA, _In_ int nLine,
                                                      _In_z_ LPCSTR szStackTraceA)
{
  CStringA cStrBodyA;
  HRESULT hRes;

  if (szDescriptionA != NULL && *szDescriptionA != 0)
  {
    if (cStrBodyA.Format("Error 0x%08X: %s", hr, szDescriptionA) == FALSE)
      return E_OUTOFMEMORY;
  }
  else
  {
    if (cStrBodyA.Format("Error 0x%08X", hr) == FALSE)
      return E_OUTOFMEMORY;
  }
  if (szFileNameA != NULL && *szFileNameA != 0)
  {
    if (nLine > 0)
    {
      if (cStrBodyA.AppendFormat(" @ %s(%lu)", szFileNameA, nLine) == FALSE)
        return E_OUTOFMEMORY;
    }
    else
    {
      if (cStrBodyA.ConcatN(" @ ", 3) == FALSE || cStrBodyA.Concat(szFileNameA) == FALSE)
        return E_OUTOFMEMORY;
    }
  }
  if (szStackTraceA != NULL && *szStackTraceA != 0)
  {
    if (cStrBodyA.ConcatN("\r\nStack trace:\r\n", 16) == FALSE || cStrBodyA.Concat(szStackTraceA) == FALSE)
      return E_OUTOFMEMORY;
  }
  hRes = HtmlEntities::ConvertTo(cStrBodyA);
  if (FAILED(hRes))
    return hRes;
  if (cStrBodyA.InsertN("<pre>", 0, 5) == FALSE || cStrBodyA.ConcatN("</pre>", 6) == FALSE)
    return E_OUTOFMEMORY;
  return SendErrorPage(500, hr, (LPCSTR)cStrBodyA);
}

HRESULT CJsHttpServer::CClientRequest::OnRequireJsModule(_In_ DukTape::duk_context *lpCtx,
                                                         _In_ CJavascriptVM::CRequireModuleContext *lpReqContext,
                                                         _Inout_ CStringA &cStrCodeA)
{
  HRESULT hRes;

  if (!cRequireJsModuleCallback)
    return E_NOTIMPL;
  MX_ASSERT(GetServerRequestFromContext(lpCtx) == this);
  hRes = cRequireJsModuleCallback(lpJsHttpServer, this, *CJavascriptVM::FromContext(lpCtx), lpReqContext, cStrCodeA);
  if (SUCCEEDED(hRes))
    hRes = TransformJavascriptCode(cStrCodeA);
  //done
  return hRes;
}

} //namespace MX

//-----------------------------------------------------------

static HRESULT TransformJavascriptCode(_Inout_ MX::CStringA &cStrCodeA)
{
  typedef enum {
    CodeModeNone=0, CodeModeInCode, CodeModeInCodePrint
  } eCodeMode;
  LPSTR sA;
  CHAR chQuoteA;
  SIZE_T k, nCurrPos, nNonCodeBlockStart;
  eCodeMode nCodeMode, nOrigCodeMode;

  //remove UTF-8 BOM if any
  if (cStrCodeA.GetLength() >= 3)
  {
    sA = (LPSTR)cStrCodeA;
    if (sA[0] == 0xEF && sA[1] == 0xBB && sA[2] == 0xBF)
      cStrCodeA.Delete(0, 3);
  }

  //locate for '<%... %>' blocks outside of quotes
  nNonCodeBlockStart = 0;
  chQuoteA = 0;
  nCodeMode = CodeModeNone;
  sA = (LPSTR)cStrCodeA;
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
      else if (nCodeMode != CodeModeNone && *sA == '/' && sA[1] != '/' && sA[1] != '*')
      {
        //check for a possible regular expression and avoid dying in the intent
        LPSTR szCurrA = sA;

        //look for the previous non-white character
        while (szCurrA > (LPSTR)cStrCodeA && *((LPBYTE)(szCurrA - 1)) <= 32)
          szCurrA--;
        if (szCurrA > (LPSTR)cStrCodeA && MX::StrChrA("(,=:[!&|?{};~+-*%/^<>", *(szCurrA - 1)) != NULL)
        {
          //assume we are dealing with a regex so advance until end
          sA++;
          while (*sA != 0 && *sA != '/')
          {
            if (sA[0] == '\\' && sA[1] != 0)
              sA++;
            sA++;
          }
          //parse flags
          if (*sA == '/')
          {
            sA++;
            while ((*sA >= 'A' && *sA <= 'Z') || (*sA >= 'a' && *sA <= 'z'))
              sA++;
          }
        }
        else
        {
          //if we reach here, then it is NOT a RegEx
          sA++;
        }
      }
      else if (nCodeMode == CodeModeNone && *sA == '<' && sA[1] == '%')
      {
        nCodeMode = (sA[2] != '=') ? CodeModeInCode : CodeModeInCodePrint;
        nCurrPos = (SIZE_T)(sA - (LPSTR)cStrCodeA);

        //convert code from 'nNonCodeBlockStart' to 'nCurrPos' to a print function
        if (TransformJavascriptCode_ConvertToPrint(cStrCodeA, nNonCodeBlockStart, nCurrPos) == FALSE)
          return E_OUTOFMEMORY;
        //reset code pointer and convert the tag to spaces
        sA = (LPSTR)cStrCodeA + nCurrPos;
        for (k = (nCodeMode == CodeModeInCode) ? 2 : 3; k>0; k--,nCurrPos++)
          *sA++ = ' ';
        //insert "echo("
        if (nCodeMode == CodeModeInCodePrint)
        {
          if (cStrCodeA.InsertN("echo(htmlentities(", nCurrPos, 18) == FALSE)
            return E_OUTOFMEMORY;
          nCurrPos += 18;
        }
        //reset code pointer
        sA = (LPSTR)cStrCodeA + nCurrPos;
      }
      else if (nCodeMode != CodeModeNone && *sA == '%' && sA[1] == '>')
      {
        nOrigCodeMode = nCodeMode;
        nCodeMode = CodeModeNone;
        nNonCodeBlockStart = (SIZE_T)(sA - (LPCSTR)cStrCodeA);
        //close echo if print mode
        if (nOrigCodeMode == CodeModeInCodePrint)
        {
          if (cStrCodeA.InsertN("));", nNonCodeBlockStart, 3) == FALSE)
            return E_OUTOFMEMORY;
          nNonCodeBlockStart += 3;
        }
        //reset code pointer
        sA = (LPSTR)cStrCodeA + nNonCodeBlockStart;
        //convert the tag to spaces
        for (k=2; k>0; k--,nNonCodeBlockStart++)
          *sA++ = ' ';
        //if after tag, there are only spaces/tabs and the new line, then skip them
        k = 0;
        while (sA[k] == ' ' || sA[k] == '\t')
          k++;
        if (sA[k] == 0)
          nNonCodeBlockStart += k;
        else if (sA[k] == '\n')
          nNonCodeBlockStart += k+1;
        else if (sA[k] == '\r')
          nNonCodeBlockStart += k + ((sA[k+1] == '\n') ? 2 : 1);
        //reset code pointer
        sA = (LPSTR)cStrCodeA + nNonCodeBlockStart;
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
        sA++;
      }
    }
  }
  //must end in non-code mode
  if (nCodeMode != CodeModeNone)
    return E_UNEXPECTED;

  //convert final block
  nCurrPos = (SIZE_T)(sA - (LPSTR)cStrCodeA);
  //convert code from 'nNonCodeBlockStart' to 'nCurrPos' to a print function
  if (TransformJavascriptCode_ConvertToPrint(cStrCodeA, nNonCodeBlockStart, nCurrPos) == FALSE)
    return E_OUTOFMEMORY;
  //done
  return S_OK;
}

static BOOL TransformJavascriptCode_ConvertToPrint(_Inout_ MX::CStringA &cStrCodeA, _Inout_ SIZE_T nNonCodeBlockStart,
                                                   _Inout_ SIZE_T &nCurrPos)
{
  CHAR chA;
  LPSTR sA;

  while (nNonCodeBlockStart < nCurrPos)
  {
    //insert a call to 'echo' method
    if (cStrCodeA.Insert("echo(\"", nNonCodeBlockStart) == FALSE)
      return FALSE;
    nNonCodeBlockStart += 6;
    nCurrPos += 6;
    //escape quotes and convert control chars
    sA = (LPSTR)cStrCodeA + nNonCodeBlockStart;
    while (nNonCodeBlockStart < nCurrPos)
    {
      if (*sA == '\r' || *sA == '\n')
        break;
      if (*sA == '"' || *sA == '\t' || *sA == '\\')
      {
        chA = (*sA == '\t') ? 't' : *sA;
        if (cStrCodeA.InsertN("\\", nNonCodeBlockStart, 1) == FALSE)
          return FALSE;
        nNonCodeBlockStart += 2;
        nCurrPos++;
        sA = (LPSTR)cStrCodeA + nNonCodeBlockStart;
        *(sA-1) = chA;
      }
      else if (*((LPBYTE)sA) < 32)
      {
        cStrCodeA.Delete(nNonCodeBlockStart, 1);
        nCurrPos--;
      }
      else
      {
        sA++;
        nNonCodeBlockStart++;
      }
    }
    if (nNonCodeBlockStart < nCurrPos)
    {
      //the code ended with a \r, \n or \r\n pair.
      //we leave them in order to maintain line numbering with original code but we close the echo sentence
      //adding the correct chars
      if (*sA == '\r')
      {
        if (sA[1] == '\n')
        {
          if (cStrCodeA.Insert("\\r\\n\");", nNonCodeBlockStart) == FALSE)
            return FALSE;
          nNonCodeBlockStart += 7 + 2; //plus 2 because the \r\n
          nCurrPos += 7;
        }
        else
        {
          if (cStrCodeA.Insert("\\r\");", nNonCodeBlockStart) == FALSE)
            return FALSE;
          nNonCodeBlockStart += 5 + 1; //plus 1 because the \r
          nCurrPos += 5;
        }
      }
      else
      {
        if (cStrCodeA.Insert("\\n\");", nNonCodeBlockStart) == FALSE)
          return FALSE;
        nNonCodeBlockStart += 5 + 1; //plus 1 because the \n
        nCurrPos += 5;
      }
    }
    else
    {
      //the code ended so close the ECHO sentence
      if (cStrCodeA.Insert("\");", nNonCodeBlockStart) == FALSE)
        return FALSE;
      nNonCodeBlockStart += 3;
      nCurrPos += 3;
    }
  }
  return TRUE;
}
