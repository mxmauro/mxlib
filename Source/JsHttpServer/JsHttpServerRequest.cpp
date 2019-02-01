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
  bDetached = FALSE;
  lpJsHttpServer = NULL;
  return;
}

CJsHttpServer::CClientRequest::~CClientRequest()
{
  return;
}

VOID CJsHttpServer::CClientRequest::Detach()
{
  bDetached = TRUE;
  return;
}

HRESULT CJsHttpServer::CClientRequest::OnSetup()
{
  bDetached = FALSE;
  return __super::OnSetup();;
}

VOID CJsHttpServer::CClientRequest::OnCleanup()
{
  __super::OnCleanup();
  return;
}

HRESULT CJsHttpServer::CClientRequest::CreateJVM(_Outptr_result_maybenull_ CJavascriptVM **lplpJvm)
{
  HRESULT hRes;

  if (lplpJvm == NULL)
    return E_POINTER;

  *lplpJvm = MX_DEBUG_NEW CJavascriptVM();
  if ((*lplpJvm) == NULL)
    return E_OUTOFMEMORY;

  //initialize javascript engine
  hRes = lpJsHttpServer->InitializeJVM(*lplpJvm, this);
  if (FAILED(hRes))
  {
    delete *lplpJvm;
    *lplpJvm = NULL;
  }

  //done
  return hRes;
}

} //namespace MX
