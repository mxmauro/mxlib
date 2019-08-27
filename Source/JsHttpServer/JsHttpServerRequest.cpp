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
      if (FAILED(lpJVM->Reset()))
      {
        goto delete_vm;
      }

      lpJsHttpServer->FreeVM(lpJVM);
    }
    else
    {
delete_vm:
      delete lpJVM;
    }
    lpJVM = NULL;
  }
  return;
}

} //namespace MX
