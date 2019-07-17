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

CJsHttpServer::CClientRequest::CClientRequest() : CHttpServer::CClientRequest()
{
  lpJsHttpServer = NULL;
  lpJVM = NULL;
  sFlags.nIsNew = sFlags.nDetached = sFlags.nDiscardedVM = 0;
  return;
}

CJsHttpServer::CClientRequest::~CClientRequest()
{
  if (lpJVM != NULL)
    delete lpJVM;
  return;
}

VOID CJsHttpServer::CClientRequest::Detach()
{
  sFlags.nDetached = 1;
  return;
}

HRESULT CJsHttpServer::CClientRequest::OnSetup()
{
  sFlags.nDetached = 0;
  return __super::OnSetup();
}

VOID CJsHttpServer::CClientRequest::OnCleanup()
{
  __super::OnCleanup();
  return;
}

HRESULT CJsHttpServer::CClientRequest::AttachJVM()
{
  BOOL bIsNew;
  HRESULT hRes;

  //initialize javascript engine
  hRes = lpJsHttpServer->AllocAndInitVM(&lpJVM, bIsNew, this);
  if (SUCCEEDED(hRes))
  {
    sFlags.nIsNew = (bIsNew != FALSE) ? 1 : 0;
  }

  //done
  return hRes;
}

VOID CJsHttpServer::CClientRequest::DiscardVM()
{
  sFlags.nDiscardedVM = 1;
  return;
}

VOID CJsHttpServer::CClientRequest::FreeJVM()
{
  if (lpJVM != NULL)
  {
    if (lpJsHttpServer != NULL && sFlags.nDiscardedVM == 0)
    {
      lpJVM->RemoveCachedModules();

      lpJVM->RunNativeProtectedAndGetError(0, 0, [](_In_ DukTape::duk_context *lpCtx) -> VOID
      {
        //called twice. see: https://duktape.org/api.html#duk_gc
        DukTape::duk_gc(lpCtx, 0);
        DukTape::duk_gc(lpCtx, 0);
      });

      lpJsHttpServer->FreeVM(lpJVM);
    }
    else
    {
      delete lpJVM;
    }
    lpJVM = NULL;
  }
  return;
}

} //namespace MX
