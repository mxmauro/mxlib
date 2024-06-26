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
