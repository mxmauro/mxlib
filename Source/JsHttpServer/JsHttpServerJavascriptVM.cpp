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

HRESULT CJsHttpServer::OnNewRequestObject(_Out_ CHttpServer::CClientRequest **lplpRequest)
{
  CClientRequest *lpJsRequest;
  HRESULT hRes;

  if (lplpRequest == NULL)
    return E_POINTER;
  *lplpRequest = NULL;

  if (cNewRequestObjectCallback)
  {
    lpJsRequest = NULL;
    hRes = cNewRequestObjectCallback(&lpJsRequest);
  }
  else
  {
    lpJsRequest = MX_DEBUG_NEW CClientRequest();
    hRes = (lpJsRequest != NULL) ? S_OK : E_OUTOFMEMORY;
  }
  if (SUCCEEDED(hRes) && lpJsRequest != NULL)
  {
    lpJsRequest->lpJsHttpServer = this;
    *lplpRequest = lpJsRequest;
  }
  return hRes;
}

CJsHttpServer::CClientRequest* CJsHttpServer::GetServerRequestFromContext(_In_ DukTape::duk_context *lpCtx)
{
  CJavascriptVM *lpJVM = CJavascriptVM::FromContext(lpCtx);
  CClientRequest *lpRequest = NULL;
  HRESULT hRes;

  hRes = lpJVM->RunNativeProtectedAndGetError(0, 0, [&lpRequest](_In_ DukTape::duk_context *lpCtx) -> VOID
  {
    DukTape::duk_push_global_object(lpCtx);
    DukTape::duk_get_prop_string(lpCtx, -1, INTERNAL_REQUEST_PROPERTY);
    if (DukTape::duk_is_undefined(lpCtx, -1) == 0)
      lpRequest = reinterpret_cast<CClientRequest*>(DukTape::duk_to_pointer(lpCtx, -1));
    DukTape::duk_pop_2(lpCtx);
    return;
  });
  return (SUCCEEDED(hRes)) ? lpRequest : NULL;
}

HRESULT CJsHttpServer::TransformJavascriptCode(_Inout_ MX::CStringA &cStrCodeA)
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

BOOL CJsHttpServer::TransformJavascriptCode_ConvertToPrint(_Inout_ MX::CStringA &cStrCodeA,
                                            _Inout_ SIZE_T nNonCodeBlockStart, _Inout_ SIZE_T &nCurrPos)
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

HRESULT CJsHttpServer::InsertPostField(_In_ CJavascriptVM &cJvm, _In_ CHttpBodyParserFormBase::CField *lpField,
                                       _In_ LPCSTR szBaseObjectNameA)
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

HRESULT CJsHttpServer::InsertPostFileField(_In_ CJavascriptVM &cJvm,
                                           _In_ CHttpBodyParserFormBase::CFileField *lpFileField,
                                           _In_ LPCSTR szBaseObjectNameA)
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

DukTape::duk_ret_t CJsHttpServer::OnRequestDetach(_In_ DukTape::duk_context *lpCtx, _In_z_ LPCSTR szObjectNameA,
                                                  _In_z_ LPCSTR szFunctionNameA)
{
  CClientRequest *lpRequest = GetServerRequestFromContext(lpCtx);
  lpRequest->sFlags.nDetached = 1;
  return 0;
}

} //namespace MX
