/*
 * Original code by Mauro H. Leggieri (http://www.mauroleggieri.com.ar)
 *
 * Copyright (C) 2002-2019. All rights reserved.
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
#include "JavascriptVMCommon.h"

//-----------------------------------------------------------

namespace MX {

void CJavascriptVM::CProxyCallbacks::serialize(_In_ void *p)
{
  cProxyHasNamedPropertyCallback.serialize(p);
  p = (char*)p + OnProxyHasNamedPropertyCallback::serialization_buffer_size();
  cProxyHasIndexedPropertyCallback.serialize(p);
  p = (char*)p + OnProxyHasIndexedPropertyCallback::serialization_buffer_size();
  cProxyGetNamedPropertyCallback.serialize(p);
  p = (char*)p + OnProxyGetNamedPropertyCallback::serialization_buffer_size();
  cProxyGetIndexedPropertyCallback.serialize(p);
  p = (char*)p + OnProxyGetIndexedPropertyCallback::serialization_buffer_size();
  cProxySetNamedPropertyCallback.serialize(p);
  p = (char*)p + OnProxySetNamedPropertyCallback::serialization_buffer_size();
  cProxySetIndexedPropertyCallback.serialize(p);
  p = (char*)p + OnProxySetIndexedPropertyCallback::serialization_buffer_size();
  cProxyDeleteNamedPropertyCallback.serialize(p);
  p = (char*)p + OnProxyDeleteNamedPropertyCallback::serialization_buffer_size();
  cProxyDeleteIndexedPropertyCallback.serialize(p);
  p = (char*)p + OnProxyDeleteIndexedPropertyCallback::serialization_buffer_size();
  cProxyGetPropertyNameCallback.serialize(p);
  return;
}

void CJavascriptVM::CProxyCallbacks::deserialize(_In_ void *p)
{
  cProxyHasNamedPropertyCallback.deserialize(p);
  p = (char*)p + OnProxyHasNamedPropertyCallback::serialization_buffer_size();
  cProxyHasIndexedPropertyCallback.deserialize(p);
  p = (char*)p + OnProxyHasIndexedPropertyCallback::serialization_buffer_size();
  cProxyGetNamedPropertyCallback.deserialize(p);
  p = (char*)p + OnProxyGetNamedPropertyCallback::serialization_buffer_size();
  cProxyGetIndexedPropertyCallback.deserialize(p);
  p = (char*)p + OnProxyGetIndexedPropertyCallback::serialization_buffer_size();
  cProxySetNamedPropertyCallback.deserialize(p);
  p = (char*)p + OnProxySetNamedPropertyCallback::serialization_buffer_size();
  cProxySetIndexedPropertyCallback.deserialize(p);
  p = (char*)p + OnProxySetIndexedPropertyCallback::serialization_buffer_size();
  cProxyDeleteNamedPropertyCallback.deserialize(p);
  p = (char*)p + OnProxyDeleteNamedPropertyCallback::serialization_buffer_size();
  cProxyDeleteIndexedPropertyCallback.deserialize(p);
  p = (char*)p + OnProxyDeleteIndexedPropertyCallback::serialization_buffer_size();
  cProxyGetPropertyNameCallback.deserialize(p);
  return;
}

size_t CJavascriptVM::CProxyCallbacks::serialization_buffer_size()
{
  return OnProxyHasNamedPropertyCallback::serialization_buffer_size() +
         OnProxyHasIndexedPropertyCallback::serialization_buffer_size() +
         OnProxyGetNamedPropertyCallback::serialization_buffer_size() +
         OnProxyGetIndexedPropertyCallback::serialization_buffer_size() +
         OnProxySetNamedPropertyCallback::serialization_buffer_size() +
         OnProxySetIndexedPropertyCallback::serialization_buffer_size() +
         OnProxyDeleteNamedPropertyCallback::serialization_buffer_size() +
         OnProxyDeleteIndexedPropertyCallback::serialization_buffer_size() +
         OnProxyGetPropertyNameCallback::serialization_buffer_size();
}

} //namespace MX
